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
void *thread_maneja_buffer (void *parametro);
void *thread_espera_vista (void *parametro);
void my_handler(int sig);
void my_handler_2(int sig);


//Cantidad de procesos esclavo
int CANT_PROC;	

//Pipe esclavos->padre, global para ser visto desde los threads
int pipeEP[2];


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

//Ruta para creacion de archivo local con los hashes
char *hashDataPath;

// Archivo de salida
FILE *file_pointer; 

//MAIN:
void main(int argc, char const *argv[]){
	//Variables
	DIR * dirp;
	struct  dirent * direntp;
	int estado;
	int numeroProceso;
	pid_t pid;
	pid_t wpid;	
	pthread_t idThreadEsclavos;
	pthread_t idThreadVista;



	//Se valida la entrada
	if(argv[1]==NULL){
        perror("ERROR: Ingrese una path por linea de comando para comenzar a procesar");
        exit(EXIT_FAILURE);
    }

	if(argv[2]==NULL){
        perror("ERROR: Ingrese path para crear archivo con resultado de proceso");
        exit(EXIT_FAILURE);
    }


	if(argv[3]==NULL){
        perror("ERROR: Ingrese la cantidad de procesos esclavo");
        exit(EXIT_FAILURE);
    }


    //Seteo CANT_PROC
    sscanf(argv[3], "%d", &CANT_PROC);


    //Array de pipes (file descriptors)
	int pipeFds[CANT_PROC*2];


	//Asigna output y abre archivo de salida para escritura
	hashDataPath = (char *)argv[2];
	file_pointer = fopen(hashDataPath, "w"); 
	printf("Path archivo output: %s ", hashDataPath );	

	//Se abre el directorio de entrada
	printf("Path input:%s\n",argv[1]);
	dirp=opendir(argv[1]);


	//Se inicializan los semaforos 
	sem_init(&sem_new_vista,0,0);
	sem_init(&sem_fin_vista,0,0);


	//Se inicializan los pipes PADRE->HIJOS
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

	

	//Se lanzan los threads que controlan la vista y el resultado de los esclavos

	/*if (pthread_create(&idThreadEsclavos, NULL, thread_maneja_buffer, NULL) != 0){
		perror ("No puedo crear thread");
		exit (-1);
	}


	if (pthread_create(&idThreadVista, NULL, thread_espera_vista, NULL) != 0){
		perror ("No puedo crear thread");
		exit (-1);
	}*/

    

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
				//write(pipeEP[1],"Bye",NAME_MAX);
				//close(pipeEP[1]);
				exit(0);
			}	

			//Se genera la ruta del archivo a procesar
			strcpy(path,ruta);			
			strcpy(buf2,buf);			
			strcat(path,buf2);
			printf("HIJO %d: ruta completa %s\n",numeroProceso,path);
			
			//Se calcula el hash md5 del archivo
			h=calcularmd5(path);
			printf("HIJO %d: hash---> %s\n",numeroProceso,h);

			//Se envia el resultado obtenido al padre mediante el pipe ESCLAVO->PADRE
			//close(pipeEP[0]);
			//printf("HIJO %d: envio resultado\n",numeroProceso);			
			//write(pipeEP[1],h,NAME_MAX);

		}	
		
		

	}else{		// Lògica del proceso padre
		
		
		//Se cierran los extremos de lectura de todos los PIPES
		for (int i = 0; i < CANT_PROC; i++){
			close(pipeFds[2*i]);
		}
		
		
		//Se distribuyen los archivos en los PIPES
		int count=0;
		int actual;
		while((direntp=readdir(dirp))!=NULL){
			
			actual=count%CANT_PROC;

			if((strcmp(direntp->d_name,".")!=0) && (strcmp(direntp->d_name,"..")!=0)){	
				
				printf("PADRE: escribiendo %s\n",direntp->d_name );
				write(pipeFds[(actual*2)+1],direntp->d_name,NAME_MAX);

			}
			count++;
		}

		
		//El proceso padre avisa a todos los esclavos que no hay mas archivos a procesar
		for (int i = 0; i < CANT_PROC; i++){
			write(pipeFds[(2*i)+1],"Bye",NAME_MAX);
		}


		//Se cierran los extremos de escritura de todos los PIPES
		for (int i = 0; i < CANT_PROC; i++){
			close(pipeFds[(2*i)+1]);
		}

		
		//El proceso padre espera la finalizacion de todos los esclavos
		for(int i=0; i<CANT_PROC;i++){
			if((wpid=wait(NULL))>=0){
				printf("Proceso %d terminado\n",wpid);
			}
		}
		
		//El proceso padre espera la finalizacion de los HILOS 1 y 2
		//pthread_join(idThreadEsclavos,NULL);
		//pthread_join(idThreadVista,NULL);
	

		//Detach de los segmentos de memoria compartida 
	    shmdt(sem_p); 
	    shmdt(mutex);
		shmdt(sem_c); 
	    shmdt(data); 
		//Se destruyen los segmentos de memoria compartida
	    shmctl(shmid_sem_p,IPC_RMID,NULL);	 
	    shmctl(shmid_mutex,IPC_RMID,NULL);
	    shmctl(shmid_sem_c,IPC_RMID,NULL);
		shmctl(shmid,IPC_RMID,NULL);

		//Se cierra el archivo de salida
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
	
	//limpiamos variable
	memset(hash,0,sizeof(hash));

	//leo todo lo devuelto por el comando md5sum
	while (fgets(pipedata, PIPE_BUF, fp) != NULL){
		strcat(hash, pipedata);
	}

	pclose(fp);
	return hash;
}



void * thread_maneja_buffer (void *parametro){
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
		//sleep(1);
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

	printf ("HILO 1: SIAMO AFORI\n");
}


void * thread_espera_vista (void *parametro){
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
	printf ("HILO 2: se ha detectado nueva vista\n");
	

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

	printf("HILO 2: Esperamos que termine vista\n");
	//Esperamos que termine la vista
	sem_wait(&sem_fin_vista);

	close(fileDescriptorFifoView);
}


void my_handler(int sig){
	printf("HANDLER: He sido interrumpido por la señal: %d\n",sig);
	sem_post(&sem_new_vista);
}



void my_handler_2(int sig){
	printf("HANDLER 2: Vista finalizada\n");
	sem_post(&sem_fin_vista);
}
