#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


void main(){
	static const int cantProc=50;
	int estado;
	int numeroProceso;
	pid_t pid;
	pid_t wpid;
	
	
	
	for(numeroProceso=0;numeroProceso<cantProc;numeroProceso++){  		// creacion de N procesos hijos
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
		for(int i=0;i<4;i++){
			printf("Soy el proceso hijo numero %d, mi id es %d\n", numeroProceso, getpid());
		}
		exit(0);
	}else{
		for(int i=0; i<cantProc;i++){
			if((wpid=wait(NULL))>=0){
				printf("Proceso %d terminado\n",wpid);
			}
		}
	
		printf("Soy el padre, mi id es %d\n",getpid());
	}

	
}	
