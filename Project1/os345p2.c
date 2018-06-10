// os345p2.c - 5 state scheduling	08/08/2013
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the CS345 projects.          **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <assert.h>
#include <time.h>
#include "os345.h"
#include "os345signals.h"

#define my_printf	printf

// ***********************************************************************
// project 2 variables
static Semaphore* s1Sem;					// task 1 semaphore
static Semaphore* s2Sem;					// task 2 semaphore
extern Semaphore* tic_10_sec;


extern TCB tcb[];								// task control block
extern int curTask;							// current task #
extern Semaphore* semaphoreList;			// linked list of active semaphores
extern jmp_buf reset_context;				// context of kernel stack
extern PQ * rq;

// ***********************************************************************
// project 2 functions and tasks

int signalTask(int, char**);
int ImAliveTask(int, char**);
int task10sec(int, char**);

// ***********************************************************************
// ***********************************************************************
// project2 command
int P2_project2(int argc, char* argv[])
{
	static char* s1Argv[] = {"signal1", "s1Sem"};
	static char* s2Argv[] = {"signal2", "s2Sem"};
	static char* aliveArgv[] = {"I'm Alive", "3"};
	static char* tickerArgv[] = { "ticker!", "7" };

	printf("\nStarting Project 2");
	SWAP;

	for (int i = 0; i < 10; i++) {
		createTask("TenSeconds",				// task name
			task10sec,				// task
			HIGH_PRIORITY,	// task priority
			2,							// task argc
			tickerArgv);
	}
	// start tasks looking for sTask semaphores
	createTask("signal1",				// task name
					signalTask,				// task
					VERY_HIGH_PRIORITY,	// task priority
					2,							// task argc
					s1Argv);					// task argument pointers

	createTask("signal2",				// task name
					signalTask,				// task
					VERY_HIGH_PRIORITY,	// task priority
					2,							// task argc
					s2Argv);					// task argument pointers

	createTask("I'm Alive",				// task name
					ImAliveTask,			// task
					LOW_PRIORITY,			// task priority
					2,							// task argc
					aliveArgv);				// task argument pointers

	createTask("I'm Alive",				// task name
					ImAliveTask,			// task
					LOW_PRIORITY,			// task priority
					2,							// task argc
					aliveArgv);				// task argument pointers
	return 0;
} // end P2_project2



// ***********************************************************************
// ***********************************************************************
// list tasks command
int P2_listTasks(int argc, char* argv[])
{
	//int i;
	printf("\nReady Queue:\n");
	printq(rq);
//	?? 1) List all tasks in all queues
// ?? 2) Show the task stake (new, running, blocked, ready)
// ?? 3) If blocked, indicate which semaphore
	/*
	for (i=0; i<MAX_TASKS; i++)
	{
		if (tcb[i].name)
		{
			printf("\n%4d/%-4d%20s%4d  ", i, tcb[i].parent,
		  				tcb[i].name, tcb[i].priority);
			if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
			else if (tcb[i].state == S_NEW) my_printf("New");
			else if (tcb[i].state == S_READY) my_printf("Ready");
			else if (tcb[i].state == S_RUNNING) my_printf("Running");
			else if (tcb[i].state == S_BLOCKED) {
				my_printf("Blocked    %s",tcb[i].event->name);
				//printq(tcb[i].event->q);
			}
			else if (tcb[i].state == S_EXIT) my_printf("Exiting");
			swapTask();
		}
	}*/
	Semaphore* sem = semaphoreList;
	Semaphore** semLink = &semaphoreList;

	// assert semaphore is binary or counting
	assert("createSemaphore Error" && ((sem->type == 0) || (sem->type == 1)));	// assert type is validate
	while (sem)
	{
		if (sem->q->size != 0) {
			printf("\nblocked Queue for sem: %s\n", sem->name);
			printq(sem->q);
		}
		// move to next semaphore
		semLink = (Semaphore**)&sem->semLink;
		sem = (Semaphore*)sem->semLink;
		swapTask();
	}

	return 0;
} // end P2_listTasks



// ***********************************************************************
// ***********************************************************************
// list semaphores command
//
int match(char* mask, char* name)
{
   int i,j;

   // look thru name
	i = j = 0;
	if (!mask[0]) return 1;
	while (mask[i] && name[j])
   {
		if (mask[i] == '*') return 1;
		if (mask[i] == '?') ;
		else if ((mask[i] != toupper(name[j])) && (mask[i] != tolower(name[j]))) return 0;
		i++;
		j++;
   }
	if (mask[i] == name[j]) return 1;
   return 0;
} // end match

int P2_listSems(int argc, char* argv[])				// listSemaphores
{
	Semaphore* sem = semaphoreList;
	while(sem)
	{
		if ((argc == 1) || match(argv[1], sem->name))
		{
			printf("\n%20s  %c  %d  %s", sem->name, (sem->type?'C':'B'), sem->state,
	  					tcb[sem->taskNum].name);
		}
		sem = (Semaphore*)sem->semLink;
	}
	return 0;
} // end P2_listSems



// ***********************************************************************
// ***********************************************************************
// reset system
int P2_reset(int argc, char* argv[])						// reset
{
	longjmp(reset_context, POWER_DOWN_RESTART);
	// not necessary as longjmp doesn't return
	return 0;

} // end P2_reset



// ***********************************************************************
// ***********************************************************************
// kill task

int P2_killTask(int argc, char* argv[])			// kill task
{
	int taskId = INTEGER(argv[1]);				// convert argument 1

	if (taskId > 0) printf("\nKill Task %d", taskId);
	else printf("\nKill All Tasks");

	// kill task
	if (killTask(taskId)) printf("\nkillTask Error!");

	return 0;
} // end P2_killTask



// ***********************************************************************
// ***********************************************************************
// signal command
void sem_signal(Semaphore* sem)		// signal
{
	if (sem)
	{
		printf("\nSignal %s", sem->name);
		SEM_SIGNAL(sem);
	}
	else my_printf("\nSemaphore not defined!");
	return;
} // end sem_signal



// ***********************************************************************
int P2_signal1(int argc, char* argv[])		// signal1
{
	SEM_SIGNAL(s1Sem);
	return 0;
} // end signal

int P2_signal2(int argc, char* argv[])		// signal2
{
	SEM_SIGNAL(s2Sem);
	return 0;
} // end signal

int task10sec(int argc, char* argv[]) 
{
	swapTask();
	time_t curr_time;
	struct tm * timeinfo;
	while(1){
		swapTask();
		SEM_WAIT(tic_10_sec);
		time(&curr_time);
		timeinfo = localtime(&curr_time);
		printf("\nTaskid: %d\n", curTask);
		printf("Current local time and date: %s\n", asctime(timeinfo));
		swapTask();

	}
}

// ***********************************************************************
// ***********************************************************************
// signal task
//
#define COUNT_MAX	5
//
int signalTask(int argc, char* argv[])
{
	int count = 0;					// task variable

	// create a semaphore
	Semaphore** mySem = (!strcmp(argv[1], "s1Sem")) ? &s1Sem : &s2Sem;
	*mySem = createSemaphore(argv[1], 0, 0);

	// loop waiting for semaphore to be signaled
	while(count < COUNT_MAX)
	{
		SEM_WAIT(*mySem);			// wait for signal
		printf("\n%s  Task[%d], count=%d", tcb[curTask].name, curTask, ++count);
	}
	return 0;						// terminate task
} // end signalTask



// ***********************************************************************
// ***********************************************************************
// I'm alive task
int ImAliveTask(int argc, char* argv[])
{
	int i;							// local task variable
	while (1)
	{
		printf("\n(%d) I'm Alive!", curTask);
		for (i=0; i<100000; i++) swapTask();
	}
	return 0;						// terminate task
} // end ImAliveTask



// **********************************************************************
// **********************************************************************
// read current time
//
char* myTime(char* svtime)
{
	time_t cTime;						// current time

	time(&cTime);						// read current time
	strcpy(svtime, asctime(localtime(&cTime)));
	svtime[strlen(svtime)-1] = 0;		// eliminate nl at end
	return svtime;
} // end myTime

int deQ(PQ * q, TID tid) {
	if (tid == -1) {
		if (q->size == 0) {
			return -1;
		}
		else {
			int to_return = q->pq[q->size - 1].tid;
			q->size--;
			return to_return;
		}
	}
	else {
		for (int i = 0; i < q->size; i++) {
			if (q->pq[i].tid == tid) {
				for (int j = i; j < q->size; j++) {
					q->pq[j] = q->pq[j + 1];
				}
				q->size--;
				return tid;
			}
		}
		return -1;
	}
}

int enQ(PQ * q, TID tid, int priority) {
	if (q->size == 0) {
		q->pq[0].tid = tid;
		q->pq[0].priority = priority;
		q->size++;
	}
	else {
		//q->size++;
		int i = 0;
		while (q->pq[i].priority < priority && i < q->size) i++;
		//i++;
		for (int j = q->size; j > i; j--) {
			q->pq[j] = q->pq[j - 1];
		}
		q->pq[i].tid = tid;
		q->pq[i].priority = priority;
		q->size++;
	}
	//printq(q);
	return tid;
}

void printq(PQ * q) {
	if (q->size == 0) return;
	printf("Queue Size: %d\n", q->size);
	for (int j = q->size - 1; j >= 0; j--) {
		int i = q->pq[j].tid;
		if (tcb[i].name)
		{
			printf("\n\t%4d/%-4d%20s%4d  ", i, tcb[i].parent,
				tcb[i].name, tcb[i].priority);
			if (tcb[i].signal & mySIGSTOP) my_printf("Paused");
			else if (tcb[i].state == S_NEW) my_printf("New");
			else if (tcb[i].state == S_READY) my_printf("Ready");
			else if (tcb[i].state == S_RUNNING) my_printf("Running");
			else if (tcb[i].state == S_BLOCKED) {
				my_printf("Blocked    %s", tcb[i].event->name);
				//printq(tcb[i].event->q);
			}
			else if (tcb[i].state == S_EXIT) my_printf("Exiting");
			//printf("\n");
			swapTask();
		}
	}
}