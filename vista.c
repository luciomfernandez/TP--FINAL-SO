#include <signal.h>
#include <sys/ipc.h> 
#include <errno.h> 
#include <sys/shm.h> 
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#define FIFO_NAME "fifoForView"
#define SHRM_PATH "/tmp"
#define BUFFER_SIZE 2048




sem_t* mutex;
sem_t* sem_p;
sem_t* sem_c;

int main(int argc, char const *argv[]) {
 	int pid;
 	int fd;
 	char buf[1024];
 	

 	key_t key_sem_mutex;
    key_t key_sem_p;
    key_t key_sem_c;

    int shmid;
    int shmid_mutex;
    int shmid_sem_p;
    int shmid_sem_c;

    char * data;


 	printf("Envio señal a la app\n");
 	
 	//Casteo la entrada	
 	sscanf(argv[1],"%d",&pid);
	
	//Envio señal a la app utilizando el pid
 	kill(pid,SIGUSR1);
 	
 	
 	//Abro el fifo
 	fd=open(FIFO_NAME,O_RDONLY);
 	printf("%s\n",FIFO_NAME);

 	//Leo identificador del buffer de memoria compartida cuando la app lo mande
 	printf("Recibiendo id del buffer en memoria compartida\n");
 	read(fd,&shmid,sizeof(shmid));
 	printf("Id recibido: %d\n", shmid);

	
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


    
    int i=0;
    int j=0;
    char buff[256];
	while(1){
        
        if(data[i%BUFFER_SIZE]=='\a'){
        	break;
        }


        sem_wait(sem_c);
        sem_wait(mutex);
        if(data[i%BUFFER_SIZE]=='\n'){
            buff[j]='\0';
            printf("RECIBIDO: %s\n", buff);
            strcpy(buff,"");
            j=0;
        }else{
            buff[j]=data[(i%BUFFER_SIZE)];    
            j++;
        }
        i++;
            
        sem_post(mutex);
        sem_post(sem_p);
    }



	//Envio señal a la app avisando que finalizo la vista
 	kill(pid,SIGUSR2);

 	return 0;
 } 
