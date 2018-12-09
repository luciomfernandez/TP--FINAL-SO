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
#include <semaphore.h>
#define FIFO_NAME "fifoForView"
#define SHRM_PATH "/tmp"
#define BUFFER_SIZE 2048

//FUNCIONES AUX:

char *calcularmd5(char *filename);
void *funThreadEsclavos (void *parametro);
void *funThreadVistas (void *parametro);
void my_handler(int sig);
void my_handler_2(int sig);


//Pipe esclavos->padre, global para ser visto desde los threads
int pipeEP[2];
//Constantes globales
static const int CANT_PROC=3;



//Semaforos
sem_t* mutex;
sem_t* sem_p;
sem_t* sem_c;
sem_t sem_new_vista;
sem_t sem_fin_vista;

//Id memoria compartida
int shmid;
int shmid_mutex;
int shmid_sem_p;
int shmid_sem_c;

//Segmento de memoria compartida
char * data;

//ruta para creacion de archivo local con los hashes
char *hashDataPath;

// create a FILE typed pointer
FILE *file_pointer; 

//MAIN:
void main(int argc, char const *argv[]){
	if(argv[1]==NULL){
        perror("ERROR: Ingrese una path por linea de comando para comenzar a procesar");
        exit(EXIT_FAILURE);
    }

		if(argv[2]==NULL){
        perror("ERROR: Ingrese path para crear archivo con resultado de proceso");
        exit(EXIT_FAILURE);
    }

	//Asigna
	hashDataPath = (char *)argv[2];
	
	printf("path archivo %s ", hashDataPath );
	// open the file "name_of_file.txt" for writing
	file_pointer = fopen(hashDataPath, "w"); 
	
	//Array de pipes (file descriptors)
	int pipeFds[CANT_PROC*2];

	
	//Variables
	DIR * dirp;
	struct  dirent * direntp;
	int estado;
	int numeroProceso;
	pid_t pid;
	pid_t wpid;

	//Init semaforo
	sem_init(&sem_new_vista,0,0);
	sem_init(&sem_fin_vista,0,0);

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
		
		//Padre espera la finalizacion del HILO 1 E HILO 2
		pthread_join(idThreadEsclavos,NULL);
		pthread_join(idThreadVista,NULL);
	

		//detach from shared memory  
	    shmdt(sem_p); 
	    shmdt(mutex);
		shmdt(sem_c); 
	    shmdt(data); 
		//destroy the shared memory
	    shmctl(shmid_sem_p,IPC_RMID,NULL);	 
	    shmctl(shmid_mutex,IPC_RMID,NULL);
	    shmctl(shmid_sem_c,IPC_RMID,NULL);
		shmctl(shmid,IPC_RMID,NULL);

	// Close the file
	fclose(file_pointer); 
	
		printf("Fin\n");
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

	key_t key_sem_mutex;
    key_t key_sem_p;
    key_t key_sem_c;
    key_t key_shrm;
    
    //Creamos segmento de memoria compartida entre procesos para el buffer
    key_shrm = ftok(SHRM_PATH,'Z');
    shmid = shmget(key_shrm,BUFFER_SIZE,0666|IPC_CREAT);
    printf("Cree el seg: %d\n", shmid);
	//Creamos memoria compartida para el mutex 
    key_sem_mutex = ftok(SHRM_PATH,'S');
    shmid_mutex = shmget(key_sem_mutex,sizeof(sem_t),0666|IPC_CREAT);

    //Creamos memoria compartida para el semaforo produtor 
    key_sem_p = ftok(SHRM_PATH,'A');	
    shmid_sem_p = shmget(key_sem_p,sizeof(sem_t),0666|IPC_CREAT);

    //Creamos memoria compartida para el semaforo consumidor 
    key_sem_c = ftok(SHRM_PATH,'B');	
    shmid_sem_c = shmget(key_sem_c,sizeof(sem_t),0666|IPC_CREAT);


    //Attach to shared memory 
	data  = (char*) shmat(shmid,(void*)0,0);    
    mutex = (sem_t*) shmat(shmid_mutex,(void*)0,0);
    sem_p =	(sem_t*) shmat(shmid_sem_p,(void*)0,0);
    sem_c = (sem_t*) shmat(shmid_sem_c,(void*)0,0);


	//Init semaforos   	
	sem_init(mutex,1,1);
	sem_init(sem_p,1,BUFFER_SIZE);
	sem_init(sem_c,1,0);

 	
 	int i=0;
 	
	while (1){
		sleep(1);
		read(pipeEP[0],bufH,NAME_MAX);
		if(strcmp(bufH,"Bye")==0){
			contador++;
			if(contador==CANT_PROC){ //Si todos los procesos terminaron de esribir, cierro el pipe
				close(pipeEP[0]);
				break;	
			}				
		}else{ 
			// Write to the file
			fprintf(file_pointer, bufH);
			
			//Agregamos resultado al buffer
			for(int j=0;j<strlen(bufH);j++){	
				sem_wait(sem_p);
				sem_wait(mutex);

				data[i%BUFFER_SIZE]=bufH[j];
				i++;

				sem_post(mutex);
				sem_post(sem_c);
			}
			
			sem_wait(sem_p);
			sem_wait(mutex);	

			data[i%BUFFER_SIZE]='\n';
			i++;

			sem_post(mutex);
			sem_post(sem_c);
		} 	
		printf ("HILO 1: Leo resultado%s\n",bufH);
	}
	printf ("HILO 1: Listo se recolectaron todos los resultados\n");
	
	//Escribimos caracter especial para finalizar lectura del buffer compartido
	sem_wait(sem_p);
	sem_wait(mutex);

	data[i%BUFFER_SIZE]='\a';

	sem_post(mutex);
	sem_post(sem_c);

	//detach from shared memory 
	shmdt(data); 
}


void * funThreadVistas (void *parametro){
	int fileDescriptorFifoView;
	char id[50];

	//Preparamos el handler la señal
	signal(SIGUSR1, my_handler);
	signal(SIGUSR2, my_handler_2);
	
	//Creamos el named pipe
	mknod(FIFO_NAME, S_IFIFO | 0666, 0);


	sleep(1);
	printf ("HILO 2: espera vista\n");
	//Sem espera nueva vista
	sem_wait(&sem_new_vista);
	printf ("HILO 2: se ah detectado nueva vista\n");
	

	//Recuperamos el id del segmento de memoria compartida
	key_t key = ftok(SHRM_PATH,'Z');  
	int shmid = shmget(key,1024,0666|IPC_CREAT); 
	
	//Limpiamos el string id y convertimos a string
	memset(id,0,sizeof(id));
	sprintf(id,"%d",shmid);
	
	//Lo imprimimos a modo informativo
	printf("HILO 2: Id (string): %s \n", id);		
	
	
	//Para escritura sin bloqueo	
	fileDescriptorFifoView = open(FIFO_NAME, O_WRONLY);
	//Lo escribimos en el named pipe
	write(fileDescriptorFifoView, &shmid, sizeof(shmid));			

	printf("Esperamos que termine vista\n");
	//Esperamos que termine la vista
	sem_wait(&sem_fin_vista);

	//close(fileDescriptorFifoView);
}


void my_handler(int sig){
	printf("HANDLER: He sido interrumpido por la señal: %d\n",sig);
	sem_post(&sem_new_vista);
}



void my_handler_2(int sig){
	printf("HANDLER 2: Vista finalizada\n");
	sem_post(&sem_fin_vista);
}
