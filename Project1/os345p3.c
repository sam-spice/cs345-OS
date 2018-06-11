// os345p3.c - Jurassic Park
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
#include <time.h>
#include <assert.h>
#include "os345.h"
#include "os345park.h"

// ***********************************************************************
//globals
extern TCB tcb[];								// task control block
extern int curTask;
extern Semaphore* taskSems[MAX_TASKS];		// task semaphore

// Project 3 structs
typedef struct
{
	unsigned short size;
	struct
	{
		Semaphore* sem;
		int delta;
	} times[MAX_TASKS];
} DC;
// project 3 variables
Semaphore* DCMutex;
Semaphore* getPassenger;
Semaphore* seatTaken;
Semaphore* passengerSeated;
Semaphore* needDriverMutex;
Semaphore* wakeupDriver;
Semaphore* haveDriver;
Semaphore* outPassenger;
Semaphore* outSeat;
Semaphore* passengerOut;
Semaphore* releaseDriver;
Semaphore* waitMalbox;
Semaphore* mailBoxAcquired;
Semaphore* mailbox;

DC * parkClock;


// Jurassic Park
extern JPARK myPark;
extern Semaphore* parkMutex;						// protect park access
extern Semaphore* tics10thsec;				// 1/10 second semaphore
extern Semaphore* fillSeat[NUM_CARS];			// (signal) seat ready to fill
extern Semaphore* seatFilled[NUM_CARS];		// (wait) passenger seated
extern Semaphore* rideOver[NUM_CARS];			// (signal) ride over


// ***********************************************************************
// project 3 functions and tasks
void CL3_project3(int, char**);
void CL3_dc(int, char**);
DC * getDClock();
int addDC(DC*, int, Semaphore *);
void printDC(DC*);
int decDC(DC*);
int P3_Clock(int, char*);

//car
int Car_Task(int, char**);

//visitor
int VisitorTask(int, char**);

// ***********************************************************************
// ***********************************************************************
// project3 command
int P3_project3(int argc, char* argv[])
{
	char buf[32];
	char* newArgv[2];
	
	// intitialize delta clock!
	parkClock = getDClock();
	DCMutex = createSemaphore("DCMutex", 0, 1);
	//seed rand
	srand(time(NULL));

	// initialize semaphores
	getPassenger = createSemaphore("Get Passenger", 0, 0);
	seatTaken = createSemaphore("Seat Taken", 0, 0);
	passengerSeated = createSemaphore("Passenger Seated", 0, 0);
	needDriverMutex = createSemaphore("NeedDriver", 0, 1);
	wakeupDriver = createSemaphore("Wakeup Driver", 0, 0);
	haveDriver = createSemaphore("Have Driver", 0, 0);
	outPassenger = createSemaphore("Out Passenger", 0, 0);
	outSeat = createSemaphore("Out of Seat", 0, 0);
	passengerOut = createSemaphore("Passenger out", 0, 0);
	releaseDriver = createSemaphore("Driver out", 0, 0);
	waitMalbox = createSemaphore("Driver Mail", 0, 1);
	mailBoxAcquired = createSemaphore("Mail Delivered", 0, 0);

	char* clock_argv[2];
	
	clock_argv[0] = "temp";
	createTask("DC Task",				// task name
		P3_Clock,				// task
		HIGH_PRIORITY,				// task priority
		1,								// task count
		clock_argv);

	// start park
	sprintf(buf, "jurassicPark");
	newArgv[0] = buf;
	createTask( buf,				// task name
		jurassicTask,				// task
		MED_PRIORITY,				// task priority
		1,								// task count
		newArgv);					// task argument

	// wait for park to get initialized...
	while (!parkMutex) SWAP;
	printf("\nStart Jurassic Park...");
	
	semWait(parkMutex);
	for (int i = 0; i < 4; i++) {
		char* carArgv[2];
		sprintf(buf, "%d", i); 
		carArgv[0] = buf;
		createTask("Car", Car_Task, MED_PRIORITY, 1, carArgv);
		SWAP;
	}
	semSignal(parkMutex);

	//create visitors
	semWait(parkMutex);
	for (int i = 0; i < 45; i++) {
		char* visArgv[2];
		sprintf(buf, "Vis ID: %d", i);
		visArgv[0] = buf;
		createTask("Visitor", VisitorTask, MED_PRIORITY, 1, visArgv);
		SWAP;
	}
	semSignal(parkMutex);
	//?? create car, driver, and visitor tasks here

	return 0;
} // end project3



// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printDC(parkClock); 
	return 1;
} // end CL3_dc

int P3_Clock(int argc, char* argv[]) {
	
	while (1) {
		semWait(tics10thsec); SWAP;
		//printf("HERE! Behold!\n");
		decDC(parkClock); SWAP;
	}
}

int Car_Task(int argc, char* argv[]) {
	//myPark.numInCarLine = myPark.numInPark = 4;
	int car_id = atoi(argv[0]); SWAP;
	Semaphore* my_sem = taskSems[curTask]; SWAP;
	printf("\nI am car: %d", car_id); SWAP;
	while (1) {
		for (int i = 0; i < 3; i++) {
			semWait(fillSeat[car_id]); SWAP;
			
			// signal ready for passenger
			semSignal(getPassenger); SWAP;
			
			//printf("here\n\n\n");

			//wait for passenger to respond
			semWait(seatTaken); SWAP;

			//remove passenger from line
			semSignal(passengerSeated); SWAP;
			if (i == 2) {
				//only i can get driver
				semWait(needDriverMutex); SWAP;

				//wake up a driver
				semSignal(wakeupDriver); SWAP;

				//wait till I have a driver
				semWait(haveDriver); SWAP;
				
				//pass car id to driver
				semWait(waitMalbox); SWAP;
				mailbox = my_sem;
				semWait(mailBoxAcquired);
				semSignal(waitMalbox);


				//have a driver someone else can get one
				semSignal(needDriverMutex); SWAP;
			}
			semSignal(seatFilled[car_id]); SWAP;
		}

		semWait(rideOver[car_id]); SWAP;
		for (int i = 0; i < 3; i++) {
			// tell passenger they can get out
			semSignal(outPassenger); SWAP;

			// wait for passenger to get out
			semWait(outSeat); SWAP;

			//passenger is out
			semSignal(passengerOut); SWAP;
		}
		// release driver to the wild;
		semSignal(releaseDriver); SWAP;
		
	}
}

int VisitorTask(int argc, char* argv[]) {
	Semaphore * mySem = taskSems[curTask]; SWAP;
	//wait to show up
	int wait_time = (rand() % 100); SWAP;
	addDC(parkClock, wait_time, mySem); SWAP;
	
	semWait(mySem); SWAP;

	semWait(parkMutex); SWAP;
	myPark.numOutsidePark++; SWAP;
	semSignal(parkMutex); SWAP;

	while (1) SWAP;

}

/*
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// ***********************************************************************
// delta clock command
int P3_dc(int argc, char* argv[])
{
	printf("\nDelta Clock");
	// ?? Implement a routine to display the current delta clock contents
	//printf("\nTo Be Implemented!");
	int i;
	for (i=0; i<numDeltaClock; i++)
	{
		printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
	}
	return 0;
} // end CL3_dc


// ***********************************************************************
// display all pending events in the delta clock list
void printDeltaClock(void)
{
	int i;
	for (i=0; i<numDeltaClock; i++)
	{
		printf("\n%4d%4d  %-20s", i, deltaClock[i].time, deltaClock[i].sem->name);
	}
	return;
}


// ***********************************************************************
// test delta clock
int P3_tdc(int argc, char* argv[])
{
	createTask( "DC Test",			// task name
		dcMonitorTask,		// task
		10,					// task priority
		argc,					// task arguments
		argv);

	timeTaskID = createTask( "Time",		// task name
		timeTask,	// task
		10,			// task priority
		argc,			// task arguments
		argv);
	return 0;
} // end P3_tdc



// ***********************************************************************
// monitor the delta clock task
int dcMonitorTask(int argc, char* argv[])
{
	int i, flg;
	char buf[32];
	// create some test times for event[0-9]
	int ttime[10] = {
		90, 300, 50, 170, 340, 300, 50, 300, 40, 110	};

	for (i=0; i<10; i++)
	{
		sprintf(buf, "event[%d]", i);
		event[i] = createSemaphore(buf, BINARY, 0);
		insertDeltaClock(ttime[i], event[i]);
	}
	printDeltaClock();

	while (numDeltaClock > 0)
	{
		SEM_WAIT(dcChange)
		flg = 0;
		for (i=0; i<10; i++)
		{
			if (event[i]->state ==1)			{
					printf("\n  event[%d] signaled", i);
					event[i]->state = 0;
					flg = 1;
				}
		}
		if (flg) printDeltaClock();
	}
	printf("\nNo more events in Delta Clock");

	// kill dcMonitorTask
	tcb[timeTaskID].state = S_EXIT;
	return 0;
} // end dcMonitorTask


extern Semaphore* tics1sec;

// ********************************************************************************************
// display time every tics1sec
int timeTask(int argc, char* argv[])
{
	char svtime[64];						// ascii current time
	while (1)
	{
		SEM_WAIT(tics1sec)
		printf("\nTime = %s", myTime(svtime));
	}
	return 0;
} // end timeTask
*/

DC * getDClock() {
	DC * to_return = (DC*)malloc(sizeof(DC)); SWAP;
	to_return->size = 0;	SWAP;
	return to_return;
}

int addDC(DC* dc, int time, Semaphore * sem) {
	// mutex for delta clock
	semWait(DCMutex); SWAP;
	//init val to return 
	int to_return; SWAP;

	// find position in delta clock for new entry
	for (int i = 0; i < dc->size; i++) {
		SWAP;
		// found new position in list
		if (time < dc->times[i].delta) {
			SWAP;
			// move all element up one to accomodate new entry
			for (int j = (dc->size); j > i; j--) {
				dc->times[j] = dc->times[j - 1]; SWAP;
			}
			// place ne entry
			dc->times[i].delta = time; SWAP;
			dc->times[i].sem = sem; SWAP;
			
			// adjust next time appropriately
			dc->times[i + 1].delta -= time;
			if (dc->times[i + 1].delta < 0) {
				dc->times[i + 1].delta = 0; SWAP;
			}
			
			// get val to return and unblock
			to_return = ++dc->size; SWAP;
			semSignal(DCMutex); SWAP;
			return to_return;
		}
		else {
			time -= dc->times[i].delta; SWAP;
			if (time < 0) {
				time = 0; SWAP;
			}
		}
	}
	// largest time in list, place at end of list and adjust accordingly
	dc->times[dc->size].delta = time; SWAP;
	dc->times[dc->size].sem = sem; SWAP;
	to_return = ++dc->size; SWAP;
	semSignal(DCMutex); SWAP;
	return to_return;
}

int decDC(DC* dc) {
	semWait(DCMutex); SWAP;
	dc->times[0].delta--; SWAP;
	int to_return = 0; SWAP;
	while (dc->times[0].delta == 0 && dc->size > 0) {
		to_return++;  SWAP;
		semSignal(dc->times[0].sem); SWAP;
		//printf("\nsem triggered: %s", dc->times[0].sem->name); SWAP;
		for (int i = 0; i < dc->size; i++) {
			dc->times[i] = dc->times[i + 1]; SWAP;
		}
		// end of list adjustment
		dc->size--; SWAP;
	}
	semSignal(DCMutex); SWAP;
	return to_return;
}

void printDC(DC* dc) {
	semWait(DCMutex); SWAP;
	printf("\nDC size: %d", dc->size); SWAP;
	for (int i = 0; i < dc->size; i++) {
		SWAP;
		printf("\n\t%d/%d %s", i, dc->times[i].delta, dc->times[i].sem->name); SWAP;
	}
	semSignal(DCMutex); SWAP;
	return;
}
