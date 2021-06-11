
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	
#include <fcntl.h>
#include <pthread.h>       
#include <signal.h>
#include <time.h>	

const  int max_entries = 5;    /***/
const  int max_size = 10;      /***/

void *threadProcessing(void *threadArgs){

    int i = *(int*)threadArgs, fd;
    char entrada[4096] = "";
    char salida[4096]  = "";

    printf("Soy el hilo de la entrada %d : pthread_self()%lx\n", i, pthread_self());

    fd = open("/proc/multilist/admin", O_RDWR );

    sprintf(entrada,"new test_string%d s",i);
    if(write(fd,entrada, strlen(entrada)) < strlen(entrada)){printf("Error en el write\n");}


    sprintf(entrada,"/proc/multilist/test_string%d",i);
    fd = open(entrada, O_RDWR );

    write(fd,"add Hola", strlen("add Hola"));
    
    fd = open(entrada, O_RDWR );
    if(read(fd,salida,4096)< 0){printf("Error en el write\n");}

    printf("Salida: %s -> %s\n", entrada,salida);

    sprintf(entrada,"delete test_string%d ",i);
    fd = open("/proc/multilist/admin", O_RDWR );
    if(write(fd,entrada, strlen(entrada)) < strlen(entrada)){printf("Error en el write\n");}




    pthread_exit(0);

}


int main(int argc, char *argv[]){
		                              
	pthread_t threadID[max_entries + 1];	
    int identif[max_entries + 1], i = 0;

        for(i = 0; i < (max_entries + 1); i++){
            identif[i] = i;
            if (pthread_create(&threadID[i],NULL,threadProcessing, (int*)&identif[i]) < 0){}
        }

        for(i = 0; i < (max_entries + 1); i++){
            pthread_join(threadID[i],NULL);
        }

}
