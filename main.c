#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


void main(){
	static const int cantProc=3;
	int cantPipes=3;
	int pipeFds[cantPipes*2];
	
	
	int estado;
	int numeroProceso;
	pid_t pid;
	pid_t wpid;
	
	
	
	
	
	
    for(int i = 0; i < (cantPipes); i++){	//Se crean los n PIPES
        if(pipe(pipeFds + i*2) < 0) {
            perror("error en pipe");
            exit(EXIT_FAILURE);
        }
    }
    
    
    for(int i = 0; i < (cantPipes); i++){	//Se escribe en los n PIPES
        printf("PADRE: escribiendo en el pipe %d \n",i+1);
		write(pipeFds[(i*2)+1],"test",5);
    }
    
	
	for(numeroProceso=0;numeroProceso<cantProc;numeroProceso++){  		// Creacion de N procesos hijos
		pid = fork();
		if(pid==0){														
			break;
		}else if(pid==-1){
			perror("Error con fork()");
			exit(1);
			break;
		}
	}

	
	
	if(pid==0){ // lógica de procesos escavo
		/*for(int i=0;i<4;i++){
			printf("Soy el proceso hijo numero %d, mi id es %d\n", numeroProceso, getpid());
		}*/
		char buf[30];
		printf("HIJO %d: leyendo del pipe \n",numeroProceso);
		read(pipeFds[2*numeroProceso],buf,5);
		printf("HIJO %d: lei del bufer \"%s\"\n",numeroProceso,buf);
		exit(0);
	}else{
		for(int i=0; i<cantProc;i++){
			if((wpid=wait(NULL))>=0){
				//printf("Proceso %d terminado\n",wpid);
			}
		}
	
		printf("Soy el padre, mi id es %d\n",getpid());
	}

	
}	
