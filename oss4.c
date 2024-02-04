// Jeremy Zahrndt
// Project 4 - oss.c
// CS-4760 - Operating Systems

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <stdbool.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <float.h>
#include <limits.h>


// Parent and child agree on common key
#define SHMKEY  97805246
// global variables
#define oneSecond 1000000000
int shmid, msqid;

//----------------------------------------------------------------------------------------------
struct Clock {
        int seconds;
        int nanoSeconds;
};

struct Clock *clockPointer;


//----------------------------------------------------------------------------------------------
struct PCB {
        int occupied;           // either true or false
        pid_t pid;              // process id of this child
        int startSeconds;       // time when it was forked
        int startNano;          // time when it was forked

        // new elements
        int serviceTimeSeconds; // total seconds it has been "scheduled"
        int serviceTimeNano; // total nanoseconds it has been "scheduled"
        int eventWaitSec; // when does its event happen?
        int eventWaitNano; // when does its event happen?
        int blocked; // is this process waiting on event?
};

//----------------------------------------------------------------------------------------------
typedef struct msgbuffer {
        long mtype; //Important: this store the type of message, and that is used to direct a message to a particular process (address)
        int intData;
} msgbuffer;


// help function -------------------------------------------------------------------------------
void help(){
        printf("Usage: ./oss [-h] [-n proc] [-s simul] [-t timeToLaunchNewChild] [-f logfile]\n");
        printf("\t-h: Help Information\n");
        printf("\t-n proc: Number of total children to launch\n");
        printf("\t-s simul: How many children to allow to run simultaneously\n");
        printf("\t-t timeToLaunchNewChild: The given time to Launch new child every so many nanoseconds\n");
        printf("\t-f logfile: The name of Logfile you want to write to\n");
}


// Function to get random seconds and nanoseconds ----------------------------------------------
void generateRandomTime(int *seconds, int *nanoseconds) {
        srand(time(NULL));

        //Generate random seconds [0, 5]
        *seconds = rand() % 6;

        //Generate random nanoseconds [0, 1000]
        *nanoseconds = rand() % 1001;
}


// increment clock function --------------------------------------------------------------------
void incrementClock(struct Clock* clockPointer, int incrementTime) {
        clockPointer->nanoSeconds += incrementTime;

        // Check if nanoseconds have reached 1 second
        if (clockPointer->nanoSeconds >= oneSecond) {
                clockPointer->seconds++;
                clockPointer->nanoSeconds -= oneSecond;
        }
}


// increment serviceTime function --------------------------------------------------------------------
void incrementServiceTime(struct PCB* procTable, int incrementTime, int workerNum) {
        procTable[workerNum].serviceTimeNano += incrementTime;

        // Check if nanoseconds have reached 1 second
        if (procTable[workerNum].serviceTimeNano >= oneSecond) {
                procTable[workerNum].serviceTimeSeconds++;
                procTable[workerNum].serviceTimeNano -= oneSecond;
        }
}


int lineCount = 0;
// function for displaying the process table---------------------------------------------------
void procTableDisplayToLog(const char* logFile, struct Clock* clockPointer, struct PCB* procTable, int proc){
        char mess1[256], mess2[256], mess3[256], mess4[246];

        if (lineCount < 10000) {
                FILE* filePointer = fopen(logFile, "a");

                if (filePointer != NULL) {
                        //create messages
                        sprintf(mess1, "OSS PID: %d  SysClockS: %d  SysClockNano: %d\n", getpid(), clockPointer->seconds, clockPointer->nanoSeconds);
                        sprintf(mess2, "Process Table: \n");
                        sprintf(mess3, "%-10s%-10s%-10s%-15s%-15s%-15s%-15s%-15s%-15s%-10s\n", "Entry", "Occupied", "PID", "StartS", "StartN", "ServiceT(s)", "ServiceT(n)", "EventW(s)", "EventW(n)", "Blocked");

                        //send message to log
                        fprintf(filePointer, "%s", mess1);
                        fprintf(filePointer, "%s", mess2);
                        fprintf(filePointer, "%s", mess3);

                        for(int i=0; i < proc; i++){
                                sprintf(mess4, "%-10d%-10d%-10d%-15d%-15d%-15d%-15d%-15d%-15d%-10d\n", i, procTable[i].occupied, procTable[i].pid, procTable[i].startSeconds, procTable[i].startNano, procTable[i].serviceTimeSeconds, procTable[i].serviceTimeNano, procTable[i].eventWaitSec, procTable[i].eventWaitNano, procTable[i].blocked);

                                fprintf(filePointer, "%s", mess4);
                        }

                        fclose(filePointer);
                } else {
                        perror("OSS: Error opening logFile\n");
                        exit(1);
                }
        }
                lineCount += 3;
                lineCount += proc;

}


// function for displaying the process table---------------------------------------------------
void procTableDisplay(struct Clock* clockPointer, struct PCB* procTable, int proc){

        printf("OSS PID: %d  SysClockS: %d  SysClockNano: %d\n", getpid(), clockPointer->seconds, clockPointer->nanoSeconds);
        printf("Process Table: \n");
        printf("%-10s%-10s%-10s%-15s%-15s%-15s%-15s%-15s%-15s%-10s\n", "Entry", "Occupied", "PID", "StartS", "StartN", "ServiceT(s)", "ServiceT(n)", "EventW(s)", "EventW(n)", "Blocked");

        for(int i=0; i < proc; i++){
                printf("%-10d%-10d%-10d%-15d%-15d%-15d%-15d%-15d%-15d%-10d\n", i, procTable[i].occupied, procTable[i].pid, procTable[i].startSeconds, procTable[i].startNano, procTable[i].serviceTimeSeconds, procTable[i].serviceTimeNano, procTable[i].eventWaitSec, procTable[i].eventWaitNano, procTable[i].blocked);
        }
}


bool timeout = false;
// function for signal handle to change timeout---------------------------------------------
void alarmSignalHandler(int signum) {
        printf("\n\n\n\nALERT -> OSS: Been 3 seconds: No more Generating NEW Processes!\n\n\n\n");
        timeout = true;
}

bool timeout2 = false;
// function for ctrl-c signal handler--------------------------------------------------------
void controlHandler(int signum) {
        printf("\n\n\n\nOSS: You hit Ctrl-C. Time to Terminate\n\n\n\n");
        timeout2 = true;
}


// fucntion to handle logging when message is recieved and message is sent----------------------
void logMessage(const char* logFile, const char* message) {

        if (lineCount < 10000) {
                FILE* filePointer = fopen(logFile, "a"); //open logFile in append mode
                if (filePointer != NULL) {
                        fprintf(filePointer, "%s", message);
                        fclose(filePointer);
                } else {
                        perror("OSS: Error opening logFile\n");
                        exit(1);
                }
        }
        lineCount++;
}


// main function--------------------------------------------------------------------------------
int main(int argc, char** argv) {
     // Declare variables
        signal(SIGALRM, alarmSignalHandler);
        signal(SIGINT, controlHandler);

        alarm(3); // dispatch new workers for only 3 seconds

        int proc, simul, option;
        int randomSeconds, randomNanoSeconds;
        int timeLimit;
        char* logFile;

        // variables for stats
        int totalWSec = 0; // total of all process wait till launch (sec)
        long int totalWNano = 0; // ^ (nano)

        int runTimeSec = 0; // total of all occasions of workers running (sec)
        long int runTimeNano = 0; // ^(nano)

        int totalEventWaitSec = 0; // total of all event seconds added up
        long int totalEventWaitNano = 0; // total of all event nanoseconds added up

        long int totalIdleTime = 0; // total idle time


      // get opt to get command line arguments
        while((option = getopt(argc, argv, "hn:s:t:f:")) != -1) {
                switch(option) {
                        case 'h':
                                help();
                                return EXIT_SUCCESS;
                        case 'n':
                                proc = atoi(optarg);
                                break;
                        case 's':
                                simul = atoi(optarg);
                                break;
                        case 't':
                                timeLimit = atoi(optarg);
                                break;
                        case 'f':
                                logFile = optarg;
                                break;
                        case '?':
                                help();
                                return EXIT_FAILURE;
                        default:
                                break;
                }
        }

      //check the -s the number of simultanious processes
        if(simul <= 0 || simul >= 20) {
                printf("Usage: The number of simultaneous processes must be greater than 0 or less than 20\n");
                return EXIT_FAILURE;
        }

      //check the -n (make sure its not negative
        if(proc <= 0) {
                printf("Usage: The number of child processes being runned must be greater than 0\n");
                return EXIT_FAILURE;
        }

      // create array of structs for process table with size = number of children
        struct PCB processTable[proc];

      // Initalize the process table information for each process to 0
        for(int i = 0; i < proc; i++) {
                processTable[i].occupied = 0;
                processTable[i].pid = 0;
                processTable[i].startSeconds = 0;
                processTable[i].startNano = 0;
                processTable[i].serviceTimeSeconds = 0;
                processTable[i].serviceTimeNano = 0;
                processTable[i].eventWaitSec = 0;
                processTable[i].eventWaitNano = 0;
                processTable[i].blocked = 0;
        }

      // Allocate memory for the simulated clock
        shmid = shmget(SHMKEY, sizeof(struct Clock), 0666 | IPC_CREAT);
        if (shmid == -1) {
                perror("OSS: Error in shmget");
                exit(1);
        }

      // Attach to the shared memory segment
        clockPointer = (struct Clock *)shmat(shmid, 0, 0);
        if (clockPointer == (struct Clock *)-1) {
                perror("OSS: Error in shmat");
                exit(1);
        }

      // Initialize the simulated clock to zero
        clockPointer->seconds = 0;
        clockPointer->nanoSeconds = 0;

      // check all given info
        printf("OSS: Get Opt Information & PCB Initialized to 0 for Given # of workers:\n");
        printf("---------------------------------------------------------------------------------------\n");
        printf("\tClock pointer: %d  :%d\n", clockPointer->seconds, clockPointer->nanoSeconds);
        printf("\tproc: %d\n", proc);
        printf("\tsimul: %d\n", simul);
        printf("\ttimeToLaunchNewChild: %d\n", timeLimit);
        printf("\tlogFile: %s\n\n", logFile);
        procTableDisplay(clockPointer, processTable, proc);
        printf("---------------------------------------------------------------------------------------\n");

      // set up message queue and logFile
        msgbuffer buf;
        key_t key;
        system("touch msgq.txt");

      // get a key for our message queues
        if ((key = ftok("msgq.txt", 1)) == -1) {
                perror("OSS: ftok error\n");
                exit(1);
        }

      // create our message queue
        if ((msqid = msgget(key, 0666 | IPC_CREAT)) == -1) {
                perror("OSS: error in msgget\n");
                exit(1);
        }
        printf("OSS: message queue is set up\n");


      // Declare variable used in loop
        int workers = 0;  // makes sure workers dont pass proc -n
        int activeWorkers = 0; // makes sure workers dont pass simul -s
        int workerNum = 0; // holds entry number
        bool childrenInSystem = false; // makes sure loop doesnt exit with workers running
        bool errorInSystem = false;
        int quantum = 10000000; // constant quantum time
        int schedulerTime = 900000; // constant sheduling time
        int unblockedTime = 2000000; // constant unblocking time
        int termWorker = 0; // number of worker that have terminated
        int copyNano = clockPointer->nanoSeconds; // makes sure children can launch every timeToLaunchNewChild -t
        int sec = 0; // used to help dislay every half second
        bool halfSecondFlag = true;

      // Create and initialize priority array - display for testing purposes
        long double priority[proc];
                printf("Initialize Priority array to all zeros: [  ");
                for (int i = 0; i < proc; i++) {
                        priority[i] = 0.0;
                        printf("%LF  ", priority[i]);
                }
                printf("]\n");

    //ctrl-c handler
     while(!timeout) {
        // stay in loop until timeout hits from signal or childInSystem = False or the number of workers is greater than or equal to the maximum number of process
        while (workers < proc || childrenInSystem == true) {

                bool blocked = false;
                if (errorInSystem == false) {

                        //check if there are any blocked and if you cant launch another worker or not
                        for (int i = 0; i < proc; i++) {
                                if (processTable[i].blocked == 1) {
                                        blocked = true;
                                        break;
                                }
                        }


                        // if oss cant reach blocked worker and no other worker to be launched increment by a 1 second
                        if (blocked == true) {
                                printf("!-> OSS: Incrementing clock by %d nanoseconds to get to BLOCKED worker\n", oneSecond);

                                incrementClock(clockPointer, oneSecond);
                                //procTableDisplay(clockPointer, processTable, proc);
                                //procTableDisplayToLog(logFile, clockPointer, processTable, proc);
                                totalIdleTime += oneSecond;
                        }


                        // if oss cant reach -t to launch new worker increment the clock (+ timeToLaunchNewChild)
                        if (blocked == false) {
                                printf("!-> OSS: Incrementing clock by %d nanoseconds to get to -t parameter\n", timeLimit);

                                incrementClock(clockPointer, timeLimit);
                                totalIdleTime += timeLimit;
                        }

                } // end of if (errorInSystem == false)

                //troubleshoot checker
                        //char mess[256];
                        //sprintf(mess, "OSS: copyNano = %d  (in loop)\n", copyNano);
                        //logMessage(logFile, mess);


                if (activeWorkers < simul && workers < proc && !timeout) {
                        // troubleshoot checker
                                //printf("OSS: Made it here\n");
                                //char mess[256];
                                //sprintf(mess, "OSS: copyNano = %d  (in loop)\n", copyNano);
                                //logMessage(logFile, mess);

                        //check if -t nanoseconds have passed
                        if (clockPointer->nanoSeconds >= (int)(copyNano + timeLimit)) {
                                copyNano += timeLimit;

                                // Check if nanoseconds have reached 1 second
                                if (copyNano >= oneSecond) {
                                        copyNano = 0;
                                }

                                //fork a worker
                                pid_t childPid = fork();

                                if (childPid == 0) {
                                       // Char array to hold information for exec call
                                        char* args[] = {"./worker", 0};

                                       // Execute the worker file with given arguments
                                        execvp(args[0], args);
                                } else {
                                        activeWorkers++;
                                        childrenInSystem = true;
                                      // New child was launched, update process table
                                        for(int i = 0; i < proc; i++) {
                                                if (processTable[i].pid == 0) {
                                                        processTable[i].occupied = 1;
                                                        processTable[i].pid = childPid;
                                                        processTable[i].startSeconds = clockPointer->seconds;
                                                        processTable[i].startNano = clockPointer->nanoSeconds;
                                                        break;
                                                }
                                        }

                                        char message3[256];
                                        sprintf(message3, "-> OSS: Generating Process with PID %d and putting it in ready queue at time %d:%d\n", processTable[workers].pid, clockPointer->seconds, clockPointer->nanoSeconds);
                                        //printf("%s\n", message3);
                                        logMessage(logFile, message3);
                                        workers++;
                                }
                        } //end of if-else statement for -t parameter
                } //end of simul if statement


             // Check if any workers have become unblocked (blocked = 0;)
                for(int i = 0; i < proc; i++) {
                        if (processTable[i].blocked == 1) {
                                if (clockPointer->seconds >= processTable[i].eventWaitSec) {
                                        //unblock worker
                                                char mess[256];
                                                sprintf(mess, "OSS: Worker %d is getting UNBLOCKED\n", processTable[i].pid);
                                                logMessage(logFile, mess);
                                                //printf("%s\n", mess);

                                                processTable[i].blocked = 0;

                                        // increment workers and activeWorkers
                                                //activeWorkers++;

                                        //increment clock for work unblocking worker
                                                incrementClock(clockPointer, unblockedTime);

                                } else if (clockPointer->seconds == processTable[i].eventWaitSec && clockPointer->nanoSeconds >= processTable[i].eventWaitNano) {
                                        //unblock worker
                                                char mess[256];
                                                sprintf(mess, "OSS: Worker %d is getting UNBLOCKED\n", processTable[i].pid);
                                                logMessage(logFile, mess);
                                                //printf("%s\n", mess);

                                                processTable[i].blocked = 0;

                                        //increment clock for work unblocking worker
                                                incrementClock(clockPointer, unblockedTime);

                                }
                        }
                }



             // schedule a process by sending it a message according to priorities
               // determine priorities
                long double minPriority = LDBL_MAX;
                int schedWorker = 0;

                for(int i = 0; i < proc; i++) {
                        if (processTable[i].occupied == 1 && processTable[i].blocked == 0) {
                                // calculate priorities
                                if ((clockPointer->seconds - processTable[i].startSeconds) + (clockPointer->nanoSeconds - processTable[i].startNano) == 0) {
                                        priority[i] = 0.0;
                                } else {
                                        priority[i] = ((long double) processTable[i].serviceTimeSeconds * oneSecond + processTable[i].serviceTimeNano) / ((long double)(clockPointer->seconds - processTable[i].startSeconds) * oneSecond + (clockPointer->nanoSeconds - processTable[i].startNano));
                                }

                                // determine minimum priority from queue
                                if (priority[i] <  minPriority) {
                                        minPriority = priority[i];
                                        schedWorker = i;
                                        //printf("OSS: (in loop) minPriority = %lf\n", minPriority);
                                        //printf("OSS: (in loop) schedWorker = %d\n", schedWorker);

                                }

                        }
                }
                // display priority queue
                        char mess1[256], mess2[256], mess3[256];
                        sprintf(mess1, "OSS: Priority Queue before scheduler: [  ");
                        //printf("%s", mess1);
                        logMessage(logFile, mess1);

                        for (int i = 0; i < proc; i++) {
                                if (processTable[i].occupied == 1  && processTable[i].blocked == 0) {
                                        sprintf(mess2, "Worker %d: %Lf  ", i, priority[i]);
                                        //printf("%s", mess2);
                                        logMessage(logFile, mess2);
                                }
                        }

                        sprintf(mess3, "]\n");
                        //printf("%s", mess3);
                        logMessage(logFile, mess3);

                //printf("OSS: (after loop) minPriority = %lf\n", minPriority);
                //printf("OSS: (after loop) schedWorker = %d\n", schedWorker);


                workerNum = schedWorker;
                struct PCB childP = processTable[workerNum];
                int cPid = childP.pid;


                if(cPid != 0 && childP.occupied == 1 && childP.blocked == 0) {
                        buf.mtype = cPid;
                        buf.intData = quantum;

                     //message send to each launched worker
                        if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                perror("OSS: msgsnd to worker failed");
                                exit(1);
                        } else {
                             //increment clock for cpu time used for scheduling and print and log requried info
                                incrementClock(clockPointer, schedulerTime);

                                char message[256];
                                char message4[256];
                                sprintf(message, "OSS: Dispatching process with PID %d priority %LF from ready queue at time %d:%d\n", cPid, minPriority, clockPointer->seconds, clockPointer->nanoSeconds);
                                sprintf(message4, "OSS: Total Time for this Dispatch was %d nanoseconds\n", buf.intData);
                                //printf("%s\n", message);
                                //printf("%s\n", message4);

                                logMessage(logFile, message);
                                logMessage(logFile, message4);
                        }

                     //message recieve
                        if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
                                perror("OSS: Failed to recieve message\n");
                                exit(1);

                        } else {
                             // Check which option worker picked via message
                                //printf("OSS: Returned message from worker %d:  %d\n", cPid, buf.intData);

                                if (quantum == buf.intData) {
                                        // Full Quantum Time was used - passed message with same time as quantum
                                                //print and log that message was recieved from worker
                                                        char message2[256];
                                                        sprintf(message2, "OSS: Recieving that process with PID %d ran for %d nanoseconds\n", cPid, buf.intData);
                                                        //printf("%s\n", message2);
                                                        logMessage(logFile, message2);

                                                        char mess[256];
                                                        sprintf(mess, "OSS: Process %d used its full time quantum.\n", cPid);
                                                        //printf("%s\n", mess);
                                                        logMessage(logFile, mess);

                                                //increment clock with quantum time used
                                                        incrementClock(clockPointer, buf.intData);

                                                //update service time
                                                        incrementServiceTime(processTable, buf.intData, workerNum);

                                                // update run time variables for cpu utilization
                                                        //runTimeNano += buf.intData;

                                                        //if (runTimeNano >= oneSecond) {
                                                                //runTimeNano -= oneSecond;
                                                                //runTimeSec++;
                                                        //}

                                } else if (buf.intData < 0) {
                                      // Worker Terminated - passed message with negative partial quantum
                                                //change int to positive
                                                        buf.intData = -1 * buf.intData;

                                                //print and log that message was recieved from worker
                                                        char message2[256];
                                                        sprintf(message2, "OSS: Recieving that process with PID %d ran for %d nanoseconds\n", cPid, buf.intData);
                                                        //printf("%s\n", message2);
                                                        logMessage(logFile, message2);

                                                        char mess[256];
                                                        sprintf(mess, "-> OSS: Process %d used Part of Quantum and TERMINATED.\n", cPid);
                                                        //printf("%s\n", mess);
                                                        logMessage(logFile, mess);

                                                //increment clock with quantum time used
                                                        incrementClock(clockPointer, buf.intData);

                                                //update service time
                                                        incrementServiceTime(processTable, buf.intData, workerNum);

                                                // update run time variables for cpu utilization
                                                        //runTimeNano += buf.intData;

                                                        //if (runTimeNano >= oneSecond) {
                                                                //runTimeNano -= oneSecond;
                                                                //runTimeSec++;
                                                        //}

                                                //increment termWorker
                                                        termWorker++;

                                                //change occupied to 0 in PCB
                                                        processTable[workerNum].occupied = 0;

                                                //decrement activeWorkers
                                                        activeWorkers--;

                                } else {
                                      // I/O interrupt - passed message with postive partial quantum
                                                // print and log that message was recieved from worker
                                                        char message2[256];
                                                        sprintf(message2, "OSS: Recieving that process with PID %d ran for %d nanoseconds\n", cPid, buf.intData);
                                                        //printf("%s\n", message2);
                                                        logMessage(logFile, message2);

                                                        char mess[256];
                                                        sprintf(mess, "-> OSS: Process %d used Part of Quantum and got BLOCKED .\n", cPid);
                                                        //printf("%s\n", mess);
                                                        logMessage(logFile, mess);

                                                // increment clock with quantum time used
                                                        incrementClock(clockPointer, buf.intData);

                                                // update service time
                                                        incrementServiceTime(processTable, buf.intData, workerNum);

                                                // update run time variables for cpu utilization
                                                        //runTimeNano += buf.intData;

                                                        //if (runTimeNano >= oneSecond) {
                                                                //runTimeNano -= oneSecond;
                                                                //runTimeSec++;
                                                        //}

                                                // change blocked to 1 in PCB
                                                        processTable[workerNum].blocked = 1;

                                                // change occupied to 0 in PCB
                                                        //processTable[workerNum].occupied = 0;

                                                // decrement activeWorkers
                                                        //activeWorkers--;

                                                // get random numbers for eventWaitSec and eventWaitNano
                                                        generateRandomTime(&randomSeconds, &randomNanoSeconds);

                                                // update waitTime sec and nano
                                                        processTable[workerNum].eventWaitSec = (int)(clockPointer->seconds + randomSeconds);
                                                        processTable[workerNum].eventWaitNano = (int)(clockPointer->nanoSeconds + randomNanoSeconds);

                                                        if (processTable[workerNum].eventWaitNano >= oneSecond) {
                                                                processTable[workerNum].eventWaitNano -= oneSecond;
                                                                processTable[workerNum].eventWaitSec++;
                                                        }

                                                // update event wait variables for Average time a process waited in a blocked queue
                                                        totalEventWaitSec += randomSeconds;
                                                        totalEventWaitNano += randomNanoSeconds;

                                                        if (totalEventWaitNano >= oneSecond) {
                                                                totalEventWaitNano -= oneSecond;
                                                                totalEventWaitSec++;
                                                        }

                                                // Display proc table to see wait time
                                                        //procTableDisplay(clockPointer, processTable, proc);

                                }

                        } // end of else for message recieve

                } // end of if cPid != 0 statement


                // display process table every half second
                        if (clockPointer->nanoSeconds >= (oneSecond / 2) && halfSecondFlag) {
                                procTableDisplay(clockPointer, processTable, proc);
                                procTableDisplayToLog(logFile, clockPointer, processTable, proc);
                                halfSecondFlag = false;
                                sec++;
                        }

                        if (clockPointer->seconds >= sec && !halfSecondFlag) {
                                procTableDisplay(clockPointer, processTable, proc);
                                procTableDisplayToLog(logFile, clockPointer, processTable, proc);
                                halfSecondFlag = true;

                                if (clockPointer->nanoSeconds >= (oneSecond / 2)) {
                                        halfSecondFlag = false;
                                        sec++;
                                }

                        }





                // NOT needed in this project
                        // increment worker number
                        // workerNum++;
                        // if(workerNum >= proc) {
                                // workerNum = 0;
                        //}
                        //printf("OSS: workerNum = %d\n", workerNum);


                // check if no worker in system
                        for (int i = 0; i < proc; i++) {
                                if (processTable[i].occupied == 1 && processTable[i].blocked == 0) {
                                        errorInSystem = true;
                                        break;
                                } else {
                                        errorInSystem = false;
                                }
                        }

                        //for (int i = 0; i < proc; i++) {
                        //      if (processTable[i].occupied == 0

                        if(termWorker == proc) {
                                childrenInSystem = false;
                        }
                        int newWorkerVar = workers;
                        if (errorInSystem == false && timeout == true) {
                                //printf("MADE IT HERE\n");
                                for (int i = 0; i < proc; i++) {
                                        if (processTable[i].occupied == 1 || processTable[i].blocked == 1) {
                                                childrenInSystem = true;
                                                workers = newWorkerVar;
                                                break;
                                        } else {
                                                workers = proc;
                                                childrenInSystem = false;
                                        }
                                }
                        }



                // Used for troubleshooting:
                        //printf("OSS: childrenInSystem = %s\n", childrenInSystem ? "true" : "false");
                        //printf("OSS: workers = %d\n", workers);

        } //end of main while loop


        //print final PCB and display this is the end of oss
                char mess9[256];
                sprintf(mess9, "\nFinal PCB Table:\n");
                printf("%s", mess9);
                logMessage(logFile, mess9);
                procTableDisplay(clockPointer, processTable, proc);
                procTableDisplayToLog(logFile, clockPointer, processTable, proc);

        //Calculate Statistics
                int newProc;
                for (int i = 0; i < proc; i++) {
                        if (processTable[i].pid != 0) {
                                totalWNano += processTable[i].startNano;

                                if (totalWNano >= oneSecond) {
                                        totalWNano -= oneSecond;
                                        totalWSec++;
                                }

                                totalWSec += processTable[i].startSeconds;

                                runTimeNano += processTable[i].serviceTimeNano;

                                if (runTimeNano >= oneSecond) {
                                        runTimeNano = 0;
                                        runTimeSec++;
                                }

                                runTimeSec += processTable[i].serviceTimeSeconds;

                                newProc++;
                        }
                }
                //printf("Help:\n\trunTimeSec=%d\trunTimeNano=%ld", runTimeSec, runTimeNano);

                // (Time CPU is busy / Total time of System) * 100
                long double averageCpuUtil =(((long double) runTimeSec * oneSecond + runTimeNano) / ((long double) clockPointer->seconds * oneSecond + clockPointer->nanoSeconds) * 100);

                long double averageBlocked = ((long double) totalEventWaitSec * oneSecond + totalEventWaitNano) / newProc;

                long double averageWaitTime = ((long double) totalWSec * oneSecond + totalWNano) / newProc;

                //Display Statistics
                printf("\nStatistics:\n---------------------------------------------\n");
                printf("\tAverage Wait Time for Worker: %LF nanoseconds\n", averageWaitTime);
                printf("\tAverage CPU Utilization: %LF%%\n", averageCpuUtil);
                printf("\tAverage Blocked Wait Time: %LF nanoseconds\n", averageBlocked);
                printf("\tTotal CPU Idle Time: %ld nanoseconds\n", totalIdleTime);

    } //end of ctrl-c timeout loop

        // do clean up
                for(int i=0; i < proc; i++) {
                        if(processTable[i].occupied == 1) {
                                kill(processTable[i].pid, SIGKILL);
                        }
                }


                // get rid of message queue
                if (msgctl(msqid, IPC_RMID, NULL) == -1) {
                        perror("oss.c: msgctl to get rid of queue, failed\n");
                        exit(1);
                }


                //detach from shared memory
                shmdt(clockPointer);

                if (shmctl(shmid, IPC_RMID, NULL) == -1) {
                        perror("oss.c: shmctl to get rid or shared memory, failed\n");
                        exit(1);
                }

                system("rm msgq.txt");

                printf("\n\nOSS: End of Parent (System is clean)\n");

        //return that oss ended successfully
                return EXIT_SUCCESS;

}
