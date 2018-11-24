#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <linux/limits.h>
#include <pthread.h>
#include <sys/ipc.h> 
#include <sys/shm.h> 
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>


//FUNCIONES AUX:

char *calcularmd5(char *filename);
void *funThreadEsclavos (void *parametro);
void *funThreadVistas (void *parametro);


//Pipe esclavos->padre, global para ser visto desde los threads
int pipeEP[2];
//Constantes globales
static const int CANT_PROC=3;

#define FIFO_NAME "fifoForViewProcess"
#define SHRM_NAME "sharedMemForViewProcess"

//MAIN:
void main(int argc, char const *argv[]){
	if(argv[1]==NULL){
        perror("ERROR: Ingrese una path por linea de comando para comenzar a procesar");
        exit(EXIT_FAILURE);
    }


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

    if(pipe(pipeEP) < 0) {
        perror("error en pipe");
        exit(EXIT_FAILURE);
    }

    //Abro el directorio sobre la ruta recibida por linea de comando
	printf("Ruta seleccionada:%s\n",argv[1]);
	dirp=opendir(argv[1]);
	

	//Se lanzan los threads que controlan la vista y el resultado de los esclavos
	pthread_t idThreadEsclavos;
	pthread_t idThreadVista;

	if (pthread_create(&idThreadEsclavos, NULL, funThreadEsclavos, NULL) != 0){
		perror ("No puedo crear thread");
		exit (-1);
	}


	if (pthread_create(&idThreadVista, NULL, funThreadVistas, NULL) != 0){
		perror ("No puedo crear thread");
		exit (-1);
	}

    

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
				close(pipeFds[2*numeroProceso]);
				//Bye pipeEP
				write(pipeEP[1],"Bye",NAME_MAX);
				close(pipeEP[1]);
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

			//Envio resultado al padre
			close(pipeEP[0]);
			printf("HIJO %d: envio resultado\n",numeroProceso);			
			write(pipeEP[1],h,NAME_MAX);

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
		
		//Padre espera la finalizacion del hilo1
		pthread_join(idThreadEsclavos,NULL);
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



void * funThreadEsclavos (void *parametro){
	char bufH[NAME_MAX];
	int contador=0;

	//generamos clave unica 
	key_t key = ftok(SHRM_NAME,65); 

	//creamos un id a partir de la clave 
	int shmid = shmget(key,1024,0666|IPC_CREAT); 

	//attach to shared memory 
	char *str = (char*) shmat(shmid,(void*)0,0); 	
 	
	while (1){
		sleep(1);
		read(pipeEP[0],bufH,NAME_MAX);
		if(strcmp(bufH,"Bye")==0){
				contador++;
				if(contador==CANT_PROC){ //Si todos los procesos terminaron de esribir, cierro el pipe
					close(pipeEP[0]);
					break;	
				}				
			}	else { 
					strcat(str, bufH); // si no viene el string "Bye" escribo en memoria compartida
		} 	
		printf ("HILO 1: Leo resultado%s\n",bufH);
	}
	printf ("HILO 1: Listo se recolectaron todos los resultados\n");
	
	//detach from shared memory 
	shmdt(str); 
}


void * funThreadVistas (void *parametro){
	int fileDescriptorFifoView;
	char id[50];


	while (1){
		sleep(1);
		printf ("Hilo espera vistas\n");
		printf(">>>>>>>>>>>>>>>>>Hilo Vista<<<<<<<<<<<<<<<<\n");

		//Creamos el named pipe
	    mknod(FIFO_NAME, S_IFIFO | 0666, 0);

		//Para escritura sin bloqueo	
		fileDescriptorFifoView = open(FIFO_NAME, O_WRONLY|O_NONBLOCK);
	
		//recuperamos el id del segmento de memoria compartida
		//generamos clave unica 
		key_t key = ftok(SHRM_NAME,65); 

		//creamos un id a partir de la clave 
		int shmid = shmget(key,1024,0666|IPC_CREAT); 
		
		//convertimos el id de formato entero a string
		memset(id,0,sizeof(id));
		sprintf(id,"%d",shmid);
		printf("Id (string): %s \n", id);		

		write(fileDescriptorFifoView, id, strlen(id));			
	}

}
