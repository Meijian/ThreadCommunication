#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <stdint.h>
#define IPC_MAX  20
#define BUFFER_SIZE 500
#define ADD_ME "hello world\n"
#define MAX_NODES 100
#define NODE_FILE ".nodes"
#define SEM_COUNTING         0
#define SEM_CNT              0
#define SEM_BINARY           1
#define SEM_BIN              1


const char PRINTER = 'M';
const char REQUEST = 'E';
const char REPLY = 'L';
const long int SERVER = 1L;

int semid;
int IPC_KEY;
typedef struct {
  long int msgTo;
  long int msgFrom;
  char buffer[BUFFER_SIZE];
} Message;

int EZIPC_SEM_TRANS(int sid);
int EZIPC_SHM_TRANS(int mid);
int EZIPC_ERROR(char *string);
int SHOW(int sid);
int EZIPC_SEM_CALL(int sid,int op);
void *EZIPC_SHM_ADDR(int mid);
void P(int sid);
void V(int sid);
int EZIPC_SEM_MAKE(int sid,int numsems);
void EZIPC_SHM_MAKE(int mid,int size);
void EZIPC_SEM_REMOVE();
void EZIPC_SHM_REMOVE();
int EZIPC_SHM_DET( char *addr);
void SETUP();
void SETUP_KEY(int key);
int SEMAPHORE(int type, int value);
void *SHARED_MEMORY(int size);
int COBEGIN(int X);
void COEND(int X);
void initializedSharedVariables(int nodeCount);

int main(int argc, char *argv[]){
  if (argc != 2){
    printf("Please provide node number \n");
    return 1;
  }

  int node = atoi(argv[1]);
  printf("node %d :)\n", node);

  FILE *nodeFile = fopen(NODE_FILE, "a+");
  rewind(nodeFile);

  int nodeCount = 0;
  int nodes[MAX_NODES];
  while(fscanf (nodeFile, "%d *", &nodes[nodeCount]) == 1 && nodeCount < MAX_NODES){
    nodeCount++;
  }

  fprintf(nodeFile, "%d\n", node);
  fclose(nodeFile);

  //Initializing shared variables
  SETUP_KEY(node);
  int *n = SHARED_MEMORY(sizeof(int));
  *n = nodeCount;
  int *reqNumber = SHARED_MEMORY(sizeof(int));
  *reqNumber = 0;
  int *highestReqNumber = SHARED_MEMORY(sizeof(int));
  *highestReqNumber = 0;
  int *outstandingReplies = SHARED_MEMORY(sizeof(int));
  *outstandingReplies = 0;
  int *requestCS = SHARED_MEMORY(sizeof(int));
  *requestCS = 0;
  int *nodeNumbers = SHARED_MEMORY(MAX_NODES*sizeof(int));
  int *temp = nodeNumbers;
  int i;
  for (i = 0; i < MAX_NODES; i++){
    nodeNumbers[i] = nodes[i];
  }
  int *deferredReplies = SHARED_MEMORY(MAX_NODES*sizeof(int));
  temp = deferredReplies;
  int j;
  for (j = 0; j < MAX_NODES; j++){
    deferredReplies[j] = 0;
  }
  int mutex = SEMAPHORE(SEM_BIN, 1);
  int waitSemaphore = SEMAPHORE(SEM_BIN, 1);

  //Initializing message queues
  int printerQueue, replyQueue, requestQueue;

  key_t printerTok = ftok(".", PRINTER);
  key_t requestTok = ftok(".", REQUEST);
  key_t replyTok = ftok(".", REPLY);

  if ((printerQueue = msgget(printerTok, IPC_CREAT | 0660)) == -1){
    perror("Queue creation failed");
    return 1;
  }
  if ((requestQueue = msgget(requestTok, IPC_CREAT | 0660)) == -1){
    perror("Queue creation failed");
    return 1;
  }
  if ((replyQueue = msgget(replyTok, IPC_CREAT | 0660)) == -1){
    perror("Queue creation failed");
    return 1;
  }

  //Add me protocol
  Message msgAddMe;
  int msgSize = sizeof(Message) - sizeof(long int);

  msgAddMe.msgFrom = node;
  strcpy(msgAddMe.buffer , ADD_ME);

  printf("sending message: ");
  int x;
  for (x = 0; x < nodeCount; x++){
    msgAddMe.msgTo = (long int)nodes[x];
    if (msgsnd(requestQueue, &msgAddMe, msgSize, 0) == -1){
      perror("Failed to add to protocol");
      return 1;
    }
    printf("%d ", nodes[x]);
  }
  printf("\n");

  //Forking to create three processes
  int pid = fork();

  if (pid == -1){
    perror("Fork failed D:");
    return 2;
  }

  else if (pid == 0) {
    //child process: reply handler
    pid = fork();

    if (pid == -1){
      perror("Fork failed D:");
      return 2;
    }

    else if (pid == 0) {
      //child/parent process: cs mutex
      printf("--mutex process up\n");
      Message msgMutex;
      int randomWait;

      while (1){
        //randomWait = (rand() % 15) + 1;
        //sleep(randomWait);
        sleep(2);
        //printf("I WANT THE CRITICAL SECTION!\n");

        printf("Highest request number is: %d\n", *highestReqNumber);
        P(mutex);
        *requestCS = 1;
        *highestReqNumber += 1;
        *reqNumber = *highestReqNumber;
        V(mutex);
        printf("Request number is: %d\n", *reqNumber);

        *outstandingReplies = *n;

        msgMutex.msgFrom = node;
        int send;
        for (send = 0; send < *n; send++){
          msgMutex.msgTo = nodeNumbers[send];
          sprintf (msgMutex.buffer, "%d", *reqNumber);
          if (msgsnd(requestQueue, &msgMutex, msgSize, 0) == -1){
            perror("Mutex failed to send request");
          }
          printf("Asking %d\n", nodeNumbers[send]);
        }

        while (*outstandingReplies > 0){
          printf("mutex unblocked %d\n", *outstandingReplies);
          P(waitSemaphore);
        }

        // BEGINNING OF CRITICAL SECTION //
        msgMutex.msgTo = SERVER;
        msgMutex.msgFrom = node;
        sprintf (msgMutex.buffer, "############## START OUTPUT FOR NODE %d ##############\n", node);
        if (msgsnd(printerQueue, &msgMutex, msgSize, 0) == -1){
          perror("Mutex failed to send");
        }

        strcpy(msgMutex.buffer , "Entered Critical Section!!\n");
        int randomTimes = (rand() % 10) + 1;

        while (randomTimes){
          if (msgsnd(printerQueue, &msgMutex, msgSize, 0) == -1){
            perror("Mutex failed to send");
          }
          randomTimes--;
        }

        sprintf (msgMutex.buffer, "-------------- END OUTPUT FOR NODE %d --------------\n", node);
        if (msgsnd(printerQueue, &msgMutex, msgSize, 0) == -1){
          perror("Mutex failed to send");
        }

        // END OF CRITICAL SECTION //

        *requestCS = 0;

        int blocked;
        for (blocked = 0; blocked <= *n; blocked++) {
          if (deferredReplies[nodeNumbers[blocked]]) {
            deferredReplies[nodeNumbers[blocked]] = 0;
            msgMutex.msgTo = nodeNumbers[blocked];
            if (msgsnd(replyQueue, &msgMutex, msgSize, 0) == -1){
              perror("Mutex failed to send reply");
            }
            printf("Replied to %d\n", nodeNumbers[blocked]);
          }
        }
      }
    }

    else {
      //parent process: reply handler
      printf("--reply process up\n");
      Message msgReply;
      while(1){
        //TODO: check for errors
        msgrcv(replyQueue, &msgReply, msgSize, node, 0);
        printf("Received reply from: %d\n", msgReply.msgFrom);
        *outstandingReplies -= 1;
        V(waitSemaphore);
      }
    }
  }

  else {
    //parent process: request handler
    printf("--request process up\n");
    Message msgRequest;
    while(1){
      //TODO: check for errors
      msgrcv(requestQueue, &msgRequest, msgSize, node, 0);

      if(strcmp(msgRequest.buffer, ADD_ME) == 0){
        //Add me request
        nodeNumbers[*n] = (int)msgRequest.msgFrom;
        deferredReplies[*n] = 0;
        *n += 1;
        int aux;
        printf("--Nodes are now (%d): ", *n);
        for (aux = 0; aux < *n; aux++) {
          printf("%d ",nodeNumbers[aux]);
        }
        printf("\n");
      }

      else {
        //Actual request message: determine priorities
        int k;
        if (sscanf(msgRequest.buffer, "%d *", &k)){
          printf("Received request with number %d\n", k);
        } else {
          continue;
        }

        int deferIt;
        if (k > *highestReqNumber){
          *highestReqNumber = k;
        }
        P(mutex);
         deferIt = (*requestCS) &&
         ( ( k > *reqNumber) ||
         ( k == *reqNumber && msgRequest.msgFrom > node ) );
        V(mutex);

        if (deferIt){
          //make it wait
          printf("Make %d wait\n", msgRequest.msgFrom);
          deferredReplies[msgRequest.msgFrom] = 1;
        }
        else{
          //send reply
          printf("Execute %d\n", msgRequest.msgFrom);
          msgRequest.msgTo = msgRequest.msgFrom;
          msgRequest.msgFrom = node;
          if (msgsnd(replyQueue, &msgRequest, msgSize, 0) == -1){
            perror("Mutex failed to send");
          }
        }
      }
    }

  }
}

int EZIPC_SEM_TRANS(int sid)
{
int ipcid;
        ipcid=semget(((IPC_KEY*IPC_MAX)+sid),1,0666|IPC_CREAT);
        return(ipcid);
}

/*
SHM_TRANS converts the memory id to the form used by the ipc library.
*/

int EZIPC_SHM_TRANS(int mid)

{
int ipcid;
        ipcid=shmget(((IPC_KEY*IPC_MAX)+mid),1,0666|IPC_CREAT);
        return(ipcid);
}

/*
ERROR reports errors and exit
*/

int EZIPC_ERROR(char *string)

{
	printf("\nError Encountered:\n");
	perror(string);
	exit(0);
}

/*
SHOW returns the current value of a semaphore, this is really just for debugging
purposes.
*/

int SHOW(int sid)

{
int value;
	value=semctl(EZIPC_SEM_TRANS(semid),sid,GETVAL,0);
	return (value);
}

/*
SEM_CALL performs a designated operation on a semaphore in the
block choosen by SEM_TRANS.
	sid is the id of the semaphore to be operated on.
 	op is a number to be added to the current value of
         the semaphore.
*/

int EZIPC_SEM_CALL(int sid,int op)


{
int x;
        struct sembuf sb;
        sb.sem_num = sid;
        sb.sem_op = op;
        sb.sem_flg = 0;
/* 	printf("sem_call: sid:%d val:%d op:%d\n",sid,SHOW(sid),op);  */
        if ( (x=(semop(EZIPC_SEM_TRANS(semid),&sb,1))) ==-1  )
             { printf("TRACE SEM_CALL ERROR\n");
                EZIPC_ERROR("SEM_CALL: Semaphore ID Error");
             }
        return(x);
}

/*
SHM_ADDR attachs and returns a pointer to the shared memory block mid.
*/

void *EZIPC_SHM_ADDR(int mid)

{
char *addr;
// extern char *shmat();
	addr=shmat(EZIPC_SHM_TRANS(mid),0,0);
if ( (uintptr_t)addr == -1 )
    {
       EZIPC_ERROR("Error:");
   }
	return (addr);
}


/*
P and V  follow the text-book standard for the P and V operations.
sid is the number of the semaphore indicated by SEM_TRANS.
*/

void P(int sid)

{
	EZIPC_SEM_CALL(sid,-1);
}


void V(int sid)

{
char *addr;
	EZIPC_SEM_CALL(0,-1);
        addr = EZIPC_SHM_ADDR(0);
 	if ( ((*(addr+1+sid))==SEM_BIN) && (SHOW(sid)==1))
		{
		/* do not increment binary semaphore past 1 */
		}
	else
		{
		EZIPC_SEM_CALL(sid,1);
		}
	EZIPC_SEM_CALL(0,1);
	EZIPC_SHM_DET(addr);
}

/*
SEM_MAKE creates a semaphore and releases it for use.
*/

int EZIPC_SEM_MAKE(int sid,int numsems)


{
int i;
i=0;
      if((semid=semget(((IPC_KEY*IPC_MAX)+i),IPC_MAX,0666|IPC_CREAT)==-1 ))
                EZIPC_ERROR("SEM_MAKE: Semaphore Creation Error");
/*   printf("SEM_MAKE semid %d\n",semid); */
      return(semid);
}

/*
SHM_MAKE creates a shared memory block and releases it for use.
*/

void EZIPC_SHM_MAKE(int mid,int size)


{
	if(shmget(((IPC_KEY*IPC_MAX)+mid),size,0777|IPC_CREAT)==-1)
		EZIPC_ERROR("Shared Memory Creation Error");
}


/*
EZIPC_SEM_REMOVE removes all the semaphores.
*/

void EZIPC_SEM_REMOVE()
{
int x;
	for(x=0; x<=IPC_MAX; x++)
		semctl(EZIPC_SEM_TRANS(semid),x,IPC_RMID);
}

/*
SHM_REMOVE removes all the blocks of shared memory.
*/

void EZIPC_SHM_REMOVE()
{
int x;
	for(x=0;x<=IPC_MAX; x++)
		shmctl(EZIPC_SHM_TRANS(x),IPC_RMID,0);
}

/*
EZIPC_SHM_DET detach a shared memory segment
SV has a default limit of 6 attached segments
*/

 int EZIPC_SHM_DET( char *addr)
{
	shmdt( addr);
        return(0);
}


void SETUP()
{
char *Maint_Block;
int Child;
IPC_KEY = getuid();

	EZIPC_SHM_MAKE(0,2+IPC_MAX);
	Maint_Block=EZIPC_SHM_ADDR(0);
	*Maint_Block=1;
	*(Maint_Block+1)=1;
	semid = EZIPC_SEM_MAKE(0,1);
	EZIPC_SEM_CALL(semid,1);
	if((Child=fork())!=0)
		{
		wait(&Child);
		EZIPC_SEM_REMOVE();
		EZIPC_SHM_REMOVE();
		exit(0);
		}
	EZIPC_SHM_DET(Maint_Block);
 		/* else continue running the program */
}

void SETUP_KEY(int key)
{
char *Maint_Block;
int Child;
IPC_KEY = key;

	EZIPC_SHM_MAKE(0,2+IPC_MAX);
	Maint_Block=EZIPC_SHM_ADDR(0);
	*Maint_Block=1;
	*(Maint_Block+1)=1;
	semid = EZIPC_SEM_MAKE(0,1);
	EZIPC_SEM_CALL(semid,1);
	if((Child=fork())!=0)
		{
		wait(&Child);
		EZIPC_SEM_REMOVE();
		EZIPC_SHM_REMOVE();
		exit(0);
		}
	EZIPC_SHM_DET(Maint_Block);
 		/* else continue running the program */
}

/*
SEMAPHORE creates a new semaphore and returns it's sid , it also sets the initial
value and the type of semaphore to be used, the types can be SEM_BIN, SEM_CNT,
SEM_BINARY, SEM_COUNTING. the value of a Binary Semaphore will not get above 1.
*/

int SEMAPHORE(int type, int value)


{
int sid, x;
union sem_num {
 int	val;
 struct  semid_ds *buf;
 ushort	*array;
 } semctl_arg;

char *Maint_Block;

	EZIPC_SEM_CALL(0,-1);
 	Maint_Block=EZIPC_SHM_ADDR(0);
	if(*Maint_Block < IPC_MAX)
		{
		sid=(*Maint_Block)++;
		EZIPC_SEM_CALL(0,1);
		*(Maint_Block+1+sid)=type;
		if ( (type == SEM_BIN) && ( value > 1 || value < 0 ) )
		  EZIPC_ERROR("SEMAPHORE: Binary semaphore not initialized to 1 or 0");
		if (value < 0 )
		  EZIPC_ERROR("SEMAPHORE:Semaphore initialized to negative value");
		  semctl_arg.val = value;
	          semctl(EZIPC_SEM_TRANS(semid),sid,SETVAL,semctl_arg);

/*		EZIPC_SEM_CALL(sid,value);   */
		}
	else
		{
		EZIPC_SEM_CALL(0,1);
		EZIPC_ERROR("SEMAPHORE: Too Many Semaphores Requested");
		}
	EZIPC_SHM_DET(Maint_Block);
	return(sid);
}

/*
SHARED_MEMORY creates a block of shared memory, and returns a pointer to the
block of memory.
*/

void *SHARED_MEMORY(int size)

{
int mid;
void *addr;
char *Maint_Block;

	EZIPC_SEM_CALL(0,-1);
 	Maint_Block=EZIPC_SHM_ADDR(0);
	if(*(Maint_Block+1) < IPC_MAX)
		{
		mid=(*(Maint_Block+1))++;
		EZIPC_SEM_CALL(0,1);
		EZIPC_SHM_MAKE(mid,size);
		}
	else
		{
		EZIPC_SEM_CALL(0,1);
		EZIPC_ERROR("Too Many Shared Memory Blocks Requested");
		}

	EZIPC_SHM_DET(Maint_Block);
	addr=EZIPC_SHM_ADDR(mid);
	return(addr);
}


int COBEGIN(int X)

{
int i;
	for (i=1;i<=X;i++)
		if(fork()==0)		/* creates a new process */
			break;
	if (i>X)
		i=0;			/* assigns parent 0 */
	return(i);			/* returns relative number */
}


void COEND(int X)

{
int signal;
	if(X==0)
		while (wait(&signal) != -1); /*parent waits for all children*/
	else
		exit(0);		/* children are killed */
}


