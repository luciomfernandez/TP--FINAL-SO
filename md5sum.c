#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/limits.h>

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

void main(int argc, char *argv[]){
	char *hash;
	if (argv[1] == NULL){
		printf("No se ha informado nombre de archivo para calcular hash \n");
		return;
	}
	printf("Calculando hash para archivo %s \n", argv[1]);
	hash = calcularmd5(argv[1]);
	printf("Hash calculado %s \n", hash);
}
