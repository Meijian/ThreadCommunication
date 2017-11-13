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

int main(){
  int printerQueue;

  key_t printerTok = ftok(".", PRINTER);

  if ((printerQueue = msgget(printerTok, IPC_CREAT | 0660)) == -1){
    perror("Queue failed");
    return 1;
  }

  Message msg;
  msg.msgTo = SERVER;
  msg.msgFrom = getpid();
  strcpy(msg.buffer , ":):)Hacked in:):)\n");

  printf("Hacker started!! %d\n", printerQueue);

  int msgSize = sizeof(Message) - sizeof(long int);
  int randomTime;

  while (1){
    sleep(2);
    printf("Hacker attacking!!\n");
    if (msgsnd(printerQueue, &msg, msgSize, 0) == -1){
      perror("Failed to send");
    }
  }
}
