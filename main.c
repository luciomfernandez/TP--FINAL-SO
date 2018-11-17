#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <linux/limits.h>

//FUNCIONES AUX:

//Calcula md5
char *calcularmd5(char *filename);




//MAIN:
void main(int argc, char const *argv[]){
	//Constantes
	static const int CANT_PROC=3;
	

	//Array de pipes (file descriptors)
	int pipeFds[CANT_PROC*2];
	
	//Variables
	DIR * dirp;
	struct  dirent * direntp;
	int estado;
	int numeroProceso;
	pid_t pid;
	pid_t wpid;


	//Se inicializan los pipes
    for(int i = 0; i < (CANT_PROC); i++){	
        if(pipe(pipeFds + i*2) < 0) {
            perror("error en pipe");
            exit(EXIT_FAILURE);
        }
    }



    //Abro el directorio sobre la ruta recibida por linea de comando
	printf("Ruta seleccionada:%s\n",argv[1]);
	dirp=opendir(argv[1]);
	
    

    //Se crean CANT_PROC procesos esclavos
	
	for(numeroProceso=0;numeroProceso<CANT_PROC;numeroProceso++){  		
		pid = fork();
		if(pid==0){														
			break;
		}else if(pid==-1){
			perror("Error con fork()");
			exit(1);
			break;
		}
	}

	
	
	if(pid==0){ 							// Lógica de los procesos esclavo
		char *ruta=(char *)argv[1];	
		char path[PATH_MAX];
		char buf2[NAME_MAX];
		char * h;
		
		
		close(pipeFds[(2*numeroProceso)+1]);
		while(1){
			char buf[NAME_MAX];
			printf("HIJO %d: leyendo del pipe \n",numeroProceso);
			read(pipeFds[2*numeroProceso],buf,NAME_MAX);
			printf("HIJO %d: leyendo el archivo de nombre %s y mi id es %d\n",numeroProceso,buf,getpid());	
			if(strcmp(buf,"Bye")==0){
				exit(0);
			}	

			//Genero la ruta del archivo a procesar
			strcpy(path,ruta);			
			strcpy(buf2,buf);			
			strcat(path,buf2);
			printf("HIJO %d: ruta completa %s\n",numeroProceso,path);
			
			//Calculo md5
			h=calcularmd5(path);
			printf("HIJO %d: hash---> %s\n",numeroProceso,h);

		}	
		
		

	}else{		// Lògica del proceso padre
		
		
		//Cierro todos los extremos de lectura
		for (int i = 0; i < CANT_PROC; i++){
			close(pipeFds[2*i]);
		}
		
		
	

		//Distribuyo los archivos en todos los pipes
		int count=0;
		int actual;
		while((direntp=readdir(dirp))!=NULL){
			//Calculo a que pipe le corresponde
			actual=count%CANT_PROC;

			if((strcmp(direntp->d_name,".")!=0) && (strcmp(direntp->d_name,"..")!=0)){	
				
				printf("PADRE: escribiendo %s\n",direntp->d_name );
				write(pipeFds[(actual*2)+1],direntp->d_name,NAME_MAX);

			}
			count++;
		}

		
		//Bye hijos
		for (int i = 0; i < CANT_PROC; i++){
			write(pipeFds[(2*i)+1],"Bye",NAME_MAX);
		}

		//Cierro todos los extremos de escritura
		for (int i = 0; i < CANT_PROC; i++){
			close(pipeFds[(2*i)+1]);
		}

		
		//Padre espera la finalizacion de todos los esclavos
		for(int i=0; i<CANT_PROC;i++){
			if((wpid=wait(NULL))>=0){
				printf("Proceso %d terminado\n",wpid);
			}
		}
	
	}

	
}	


//FUNCIONES AUXILIARES:


char *calcularmd5(char *filename){

	FILE *fp;
	char *command;
	char *hash;
	char pipedata[PIPE_BUF];
	int size;
	int status;

    size = strlen(filename) + 5;
    command = malloc(size);
	hash 	= malloc(132);
	
	//Armo comando shell
	strcpy(command, "md5sum ");
	strcat(command, filename);

	//Abro pipe que queda conectado con el comando md5sum
	fp = popen(command, "r");
	if (fp == NULL);
	
	//leo todo lo devuelto por el comando md5sum
	while (fgets(pipedata, PIPE_BUF, fp) != NULL){
		strcat(hash, pipedata);
	}

	pclose(fp);
	return hash;
}

