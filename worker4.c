// Jeremy Zahrndt
// Project 4 - worker.c
// CS-4760 - Operating Systems

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <time.h>


// Parent and child agree on common key
#define SHMKEY  97805246


//------------------------------------------------------------------------------------------------------------------------------------
typedef struct msgbuffer {
        long mtype; //Important: this store the type of message, and that is used to direct a message to a particular process (address)
        int intData;
} msgbuffer;


//-------------------------------------------------------------------------------------------------------------------------------------
//Main Function
int main(int argc, char ** argv) {
        //declare variables for message queue
        msgbuffer buf;
        int msqid = 0; // messageQueueID
        key_t key;
        buf.mtype = 1;
        buf.intData;

        // Seed the random number generator with the current time and the PID
        srand(time(NULL) ^ (getpid()<<16));

        // get a key for our message queue
        if ((key = ftok("msgq.txt", 1)) == -1){
                printf("-> WORKER %d: ftok error\n", getpid());
                exit(1);
        }

        // create our message queue
        if ((msqid = msgget(key, 0666)) == -1) {
                printf("-> WORKER %d: error in msgget\n", getpid());
                exit(1);
        }


        //Check the number of commands
        if(argc !=  1) {
                printf("-> WORKER %d: Usage: ./worker \n", getpid());
                return EXIT_FAILURE;
        }


        //while loop to select 1 of the 3 options and send message back, until it sends a negative message meaning it terminated.
        while(1) {
                //message receive
                if ( msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
                        printf("-> WORKER %d: failed to receive message from oss\n", getpid());
                        exit(1);
                }

                int quantum = buf.intData;
                //printf("-> WORKER %d: IntData recieved by OSS message: %d\n", getpid(), quantum);

                // Generates a random number between 0 and 99
                int randPercent = rand() % 100;

                // Generate Partial time (might be needed) - A random percentage (between 1% and 99%)
                int partialQuantum = (int)(quantum * (1 + (rand() % 99)) / 100);


                // determine what worker is going to do
                if (randPercent < 95) {
                        // 95% probability: Process will use its full time quantum
                        //printf("-> WORKER %d: Process will use its full time quantum.\n", getpid());
                        buf.intData = quantum;

                } else if (randPercent < 98) {
                        // 3% probability: Process will use part of its time quantum before an I/O interrupt
                        //printf("-> WORKER %d: Process will use %d nanoseconds of its quantum before an I/O interrupt.\n", getpid(), partialQuantum);
                        buf.intData = partialQuantum;
                        //printf("-> WORKER %d: buf.intData = %d\n", getpid(), buf.intData);

                } else {
                        // 2% probability: Process will terminate after using a percentage of the quantum
                        //printf("-> WORKER %d: Process will use %d nanoseconds of its quantum before TERMINATING.\n", getpid(), partialQuantum);
                        buf.intData = (-1 * partialQuantum);
                        //printf("-> WORKER %d: buf.intData = %d\n", getpid(), buf.intData);

                }


                // change buf type to parent process id (ppid)
                buf.mtype = getppid();

                // message send of the quantum time used (negative if it terminated)
                if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                        printf("-> WORKER %d: msgsnd to oss failed\n", getpid());
                        exit(1);
                }

                // Check if buf.intData is negative after sending the message
                if(buf.intData < 0) {
                        break;
                }

        } //end of while loop

        //printf("-> WORKER: End of worker %d\n", getpid());

        return EXIT_SUCCESS;
}