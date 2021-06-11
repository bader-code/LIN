
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>	
#include <fcntl.h>
#include <pthread.h>       
#include <signal.h>
#include <time.h>	



#define NR_LEDS 8
#define NR_BYTES_BLINK_MSG 6

const char structureCommand[] = "0:0x000000";

#define BUFFER_LENGTH NR_LEDS*sizeof(structureCommand) + 1

#define MAX_LENGHT 512

#define NR_SAMPLE_COLORS 4

typedef struct buffer_in{
  char char_in[MAX_LENGHT];
  int bytes;
}tBuffer_in;



unsigned int table[] = {
    0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000
};




volatile tBuffer_in buffer_in;

void delay(int number_of_seconds) { 
    // Converting time into milli_seconds 
    int milli_seconds = 1000 * number_of_seconds; 
  
    // Storing start time 
    clock_t start_time = clock(); 
  
    // looping till required time is not achieved 
    while (clock() < start_time + milli_seconds); 
} 


void tableString(char *str_leds, int patron){
  char *ptr_table = str_leds;
  int i = 0;

  if(patron == 0){
    for(; i < NR_LEDS; i++){
      ptr_table += sprintf(ptr_table,"%d:0x0000%x",i,table[i]);
      if(i < NR_LEDS - 1){
         ptr_table += sprintf(ptr_table,"%c",',');
      }
    }
 }
 else if(patron == 1){
   for(; i < NR_LEDS; i++){
     ptr_table += sprintf(ptr_table,"%d:0x00%x00",i,table[i]);
     if(i < NR_LEDS - 1){
         ptr_table += sprintf(ptr_table,"%c",',');
     }
    }
 }
  else if(patron == 2){
    for(; i < NR_LEDS; i++){
     ptr_table += sprintf(ptr_table,"%d:0x%x0000",i,table[i]);
     if(i < NR_LEDS - 1){
         ptr_table += sprintf(ptr_table,"%c",',');
     }
    }
   
 }
  else if(patron == 3){
    for(; i < NR_LEDS; i++){
     ptr_table += sprintf(ptr_table,"%d:0x%x%x%x",i,table[i],table[i],table[i]);
     if(i < NR_LEDS - 1){
         ptr_table += sprintf(ptr_table,"%c",',');
     }
    }
 }
}
  


void *threadProcessing_A(){
  
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);  

  
  int i = 0, j = 0, patron = 0, vueltas = 50;
	int fd;
	fd = open("/dev/usb/blinkstick0", O_RDWR | O_EXCL);
  char str_leds[BUFFER_LENGTH];
  while(1){
    pthread_testcancel(); 
    table[7] = (unsigned int)buffer_in.char_in[i];
    tableString(str_leds,patron);
    j = 0;
    for(; j < NR_LEDS - 1; j++){table[j] = table[j + 1];}
    i = (i + 1) % buffer_in.bytes;
    if(write(fd,str_leds, strlen(str_leds)) < strlen(str_leds)){printf("Error en el write\n");return 0;}
    if(i == 0){
      if(vueltas > 0){
           vueltas--;
      }
      j = 0;
      for(;j < NR_LEDS; j++){
        table[j] = 0x000000;
      }
      patron = (patron + 1) % 4;
    } 
    delay(10 * vueltas);
  }

}

void *threadProcessing_R(){
  
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);  

  
  int i = 0, vueltas = 50;
	int fd;
	fd = open("/dev/usb/blinkstick0", O_RDWR | O_EXCL);
  char str_leds[BUFFER_LENGTH];
  while(1){
    pthread_testcancel();
    table[(rand() % 8)] = (unsigned int)buffer_in.char_in[i];
    i = (i + 1) % buffer_in.bytes;
    tableString(str_leds,(rand() % 4));
    if(write(fd,str_leds, strlen(str_leds)) < strlen(str_leds)){printf("Error en el write\n");return 0;} 
    table[(rand() % 8)] = 0x000000; 
    delay(10 * vueltas);
  }

}

void *threadProcessing_M(){
  
  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);  

  
  int i = 0, j = 0, patron = 0, vueltas = 50;
	int fd;
	fd = open("/dev/usb/blinkstick0", O_RDWR | O_EXCL);
  char str_leds[BUFFER_LENGTH];
  while(1){
    pthread_testcancel();
    for(; j < NR_LEDS; j++){
      table[j] = (unsigned int)buffer_in.char_in[i];
      i = (i + 1) % buffer_in.bytes;
    }
    tableString(str_leds,(rand() % 4));
    if(write(fd,str_leds, strlen(str_leds)) < strlen(str_leds)){printf("Error en el write\n");return 0;} 
    delay(10 * 20);
  }

}




int main(){

  char option, clean;
  pthread_t threadID;	



	srand(time(0));


  while (1){
    int str_in;


    printf("ELIGE UNA OPCION: ");
    scanf("%c",&option);

    switch (option){
    case 'A':   printf("MODELO A -> ESTILO MATRIZ DE PUNTOS\n"); 
                pthread_cancel(threadID);
                while ((clean = getchar()) != '\n' && clean != EOF);
	              if(pthread_create(&threadID,NULL,threadProcessing_A,NULL) < 0){printf("ERROR -> pthread_create operation\n");}break;

    case 'T':   printf("INTRODUCE TEXTO A MOSTRAR -> MAX = [%d] bytes: ",MAX_LENGHT);  
                pthread_cancel(threadID);
                buffer_in.bytes = 0;
                while ((clean = getchar()) != '\n' && clean != EOF);
                while((str_in = getc(stdin)) !=  10 &&  buffer_in.bytes < MAX_LENGHT){
                              buffer_in.char_in[buffer_in.bytes] = (char)str_in;
                              buffer_in.bytes++;
                }break;

    case 'R':   printf("MODELO R -> ESTILO RANDOM\n");  
                pthread_cancel(threadID);
                while ((clean = getchar()) != '\n' && clean != EOF);
                if(pthread_create(&threadID,NULL,threadProcessing_R,NULL) < 0){printf("ERROR -> pthread_create operation\n");}break;

    case 'H':  printf("MODELO A -> ESTILO MATRIZ DE PUNTOS\n");  
               printf("MODELO R -> ESTILO RANDOM\n"); 
               printf("MODELO T -> INTRODUCE TEXTO A MOSTRAR -> MAX = [%d] bytes: ",MAX_LENGHT); 
               printf("MODELO H -> MUESTRA AYUDA DE LAS OPCIONES EXISTENTES\n");break;
               printf("MODELO E -> SALIENDO\n");

    case 'E':  printf("MODELO E -> SALIENDO\n"); return 0;break;

    case 'M':  printf("MODELO M -> MODO CODIGO MORSE\n");
               pthread_cancel(threadID);
               while ((clean = getchar()) != '\n' && clean != EOF);
               if(pthread_create(&threadID,NULL,threadProcessing_M,NULL) < 0){printf("ERROR -> pthread_create operation\n");}break;

    default:break;
    }

  }
  return 0;
}
