#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 500


const char PRINTER = 'M';
const char REQUEST = 'E';
const char REPLY = 'L';
const long int SERVER = 1L;

typedef struct {
  long int msgTo;
  long int msgFrom;
  char buffer[BUFFER_SIZE];
} Message;

int main (){
  char buffer[BUFFER_SIZE];
  int printerQueue, replyQueue, requestQueue, n;
  Message msg;

  key_t printerTok = ftok(".", PRINTER);
  key_t requestTok = ftok(".", REQUEST);
  key_t replyTok = ftok(".", REPLY);

  if ((printerQueue = msgget(printerTok, IPC_CREAT | 0660)) == -1){
    perror("Queue failed");
    return 1;
  }
  if ((requestQueue = msgget(requestTok, IPC_CREAT | 0660)) == -1){
    perror("Queue failed");
    return 1;
  }
  if ((replyQueue = msgget(replyTok, IPC_CREAT | 0660)) == -1){
    perror("Queue failed");
    return 1;
  }

  printf("Printer is on, Queue \n");

  int msgSize = sizeof(Message) - sizeof(long int);

  while(1){
    n = msgrcv(printerQueue, &msg, msgSize, SERVER, 0);
    printf("%s", msg.buffer);
  }

  return 1;
}
