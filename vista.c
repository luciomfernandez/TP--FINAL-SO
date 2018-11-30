#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#define FIFO_NAME "fifoForViewProcess"


int main(int argc, char const *argv[]) {
 	int pid,fd;
 	char buf[1024];


 	printf("Envio señal\n");
 	
 	//Casteo la entrada	
 	sscanf(argv[1],"%d",&pid);
 	
 	//Envio señal a la app utilizando el pid
 	kill(pid,SIGUSR1);
 	

 	//Abro el fifo
 	fd=open(FIFO_NAME, O_RDONLY);
 	
 	//Leo identificador del buffer de memoria compartida cuando la app lo mande
 	read(fd,buf,1024);
 	printf("He leido: %s\n", buf);


 	//INICIO SECCION CRITICA!!

 		//LECTURA DEL BUFFER

 	//FIN SECCION CRITICA!!
 	

 	return 0;
 } 
