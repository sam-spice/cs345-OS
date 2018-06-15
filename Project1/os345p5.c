// os345p5.c - Scheduler
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
#include "os345.h"

#define NUM_PARENTS			5
#define NUM_REPORT_SECONDS	5

// ***********************************************************************
// project 5 variables




int parentTask(int argc, char* argv[]);
int groupReportTask(int argc, char* argv[]);
int childTask(int argc, char* argv[]);
void print_scheduler(fair_scheduler*);
fair_scheduler* get_fair_scheduler();
void calc_refill_times(fair_scheduler*);
int get_time_for_task(int);


Semaphore* childALive;				// childALive semaphore
Semaphore* parentDead;				// parent dead
extern Semaphore* tics1sec;			// 1 second semaphore
extern int curTask;					// current task #
extern int scheduler_mode;			// scheduler mode
long int group_count[NUM_PARENTS];	// parent group counters
int num_siblings[NUM_PARENTS];		// number in each group
Semaphore* scheduler_mutex;
extern fair_scheduler* p5_scheduler;

// ***********************************************************************
// ***********************************************************************
// project5 command
//
//
int P5_project5(int argc, char* argv[])		// project 5
{
	int i;
	char* new_argv[4];						// child arguments
	char arg1[16];
	char arg2[16];
	char arg3[16];

	static char* groupReportArgv[] = {"groupReport", "4"};
	

	// check if just changing scheduler mode
	if (argc > 1)
	{
		scheduler_mode = atoi(argv[1]);
		printf("\nScheduler Mode = %d (%s)", scheduler_mode, scheduler_mode ? "FSS" : "RR");
		return 0;
	}

	printf("\nStarting Project 5");
	scheduler_mutex = createSemaphore("scheduler mutex", 0, 1);

	p5_scheduler = get_fair_scheduler();
	for (i = 0; i < NUM_PARENTS; ++i)
	{
		num_siblings[i] = (rand() % 25) + 1;
		printf("\nGroup[%d] = %d", i, num_siblings[i]);
	}

	childALive = createSemaphore("childALive", BINARY, 0);
	parentDead = createSemaphore("parentDead", BINARY, 0);

	// create parents
	for (i = 0; i < NUM_PARENTS; i++)
	{
		group_count[i] = 0;					// zero group counter

		sprintf(arg1, "parent%d", i + 1);
		sprintf(arg2, "%d", i + 1);
//		sprintf(arg3, "%d", 1 + 4 * i);
		sprintf(arg3, "%d", num_siblings[i]);
		new_argv[0] = arg1;
		new_argv[1] = arg2;
		new_argv[2] = arg3;

		printf("\nCreate %s with %d child%s", arg1, atoi(arg3), (atoi(arg3) == 1 ? "" : "ren"));
		createTask(new_argv[0]				// task name
				 , parentTask,				// parent task
				   MED_PRIORITY,			// priority
				   3,						// argc
				   new_argv);				// argv
		SEM_WAIT(parentDead);				// wait for parent to die
	}
	
	
	calc_refill_times(p5_scheduler);
	print_scheduler(p5_scheduler);
	// create reporting task
	createTask("Group Report"	,			// task name
				groupReportTask,			// task
				MED_PRIORITY,				// task priority
				2,							// task argc
				groupReportArgv);			// task argument pointers
	return 0;
} // end P5_project5



// ***********************************************************************
// ***********************************************************************
//	group parent - create children
//		argv[0] = group base name
//		argv[1] = parent #
//		argv[2] = # of children
//
void add_to_scheduler(int group, fair_scheduler* scheduler) {
	semWait(scheduler_mutex);
	scheduler->times[curTask].group = group;
	semSignal(scheduler_mutex);
}

int parentTask(int argc, char* argv[])		// group 1
{
	int i, num_children;
	char buffer[32];
	int parent = atoi(argv[1]);
	num_children = atoi(argv[2]);
	p5_scheduler->parent_ids[parent - 1] = curTask;
	p5_scheduler->group_size[parent - 1]++;

	add_to_scheduler(parent - 1, p5_scheduler);
	for (i = 'a'; i < 'a' + num_children; i++)
	{
		sprintf(buffer, "%s%d%c", "child_", atoi(argv[1]), i);
		createTask(buffer,					// task name
				childTask,					// task
				MED_PRIORITY,				// priority
				3,							// task argc
				argv);						// task argument pointers
		SEM_WAIT(childALive);				// wait until child is going
		p5_scheduler->group_size[parent - 1]++;
	}
	SEM_SIGNAL(parentDead);					// parent dies

//	while (1) SWAP;							// keep parent alive
	while (1)
	{
		++group_count[parent - 1];
		SWAP;
	}

	return 0;
} // end parentTask


// ***********************************************************************
// ***********************************************************************
//	child task
//		argv[0] = group base name
//		argv[1] = parent #
//		argv[2] = # of siblings
//
int childTask(int argc, char* argv[])		// child Task
{
	int parent = atoi(argv[1]);
	if ((parent<1) || (parent>NUM_PARENTS))
	{
		printf("\n**Parent Error!  Task %d, Parent %d", curTask, parent);
		return 0;							// die!!
	}
	add_to_scheduler(parent - 1, p5_scheduler);
	SEM_SIGNAL(childALive);					// child is alive!!
	// count # of times scheduled
	while (1)
	{
		group_count[parent-1]++;
		SWAP;
	}
	return 0;
} // end childTask



// ***********************************************************************
// ***********************************************************************
// Group Report task
//
int groupReportTask(int argc, char* argv[])
{
	int i;
	int count = NUM_REPORT_SECONDS;
	long int sum;

	while (1)
	{
		while (count-- > 0)
		{
		   	// update every second
			SEM_WAIT(tics1sec);

		}

		sum = 0;
		for (i = 0; i < NUM_PARENTS; ++i) sum += group_count[i];

		printf("\nGroups:");
		for (i=0; i<NUM_PARENTS; i++)
		{
//			printf("%10ld", group_count[i]);
			if (sum == 0) sum = 1;
			printf("%10ld (%d%%)", group_count[i], (group_count[i] * 100) / sum);
			group_count[i] = 0;
		}

		count = NUM_REPORT_SECONDS;
	}
	return 0;
} // end groupReportTask

fair_scheduler * get_fair_scheduler() {
	fair_scheduler * to_return = (fair_scheduler*)malloc(sizeof(fair_scheduler));
	for (int i = 0; i < NUM_PARENTS; i++) {
		to_return->group_size[i] = 0;
		to_return->parent_ids[i] = 0;
	}
	for (int i = 0; i < MAX_TASKS; i++) {
		to_return->times[i].group = -1;
		to_return->times[i].cur_time = 0;
		to_return->times[i].max_time = 0;
	}
	return to_return;
}

void calc_refill_times(fair_scheduler* scheduler) {
	semWait(scheduler_mutex);
	int max = 0;
	// find max group size
	for (int i = 0; i < NUM_PARENTS; i++) {
		if (scheduler->group_size[i] > max) max = scheduler->group_size[i];
	}
	//proportion max clocks
	int max_clocks = max * 5;
	scheduler->max_total = max_clocks * NUM_PARENTS;
	scheduler->total_tasks = scheduler->max_total;
	int sizes[NUM_PARENTS];
	for (int i = 0; i < NUM_PARENTS; i++) {
		sizes[i] = max_clocks / scheduler->group_size[i];
	}
	//give every task their fair slice
	for (int i = 0; i < MAX_TASKS; i++) {
		if (scheduler->times[i].group >= 0) {
			scheduler->times[i].max_time = sizes[scheduler->times[i].group];
			scheduler->times[i].cur_time = sizes[scheduler->times[i].group];
		}
		else {
			scheduler->times[i].max_time = 1;
			scheduler->times[i].cur_time = 1;
		}
	}

	// distribute extra to the parent
	for (int i = 0; i < NUM_PARENTS; i++) {
		scheduler->times[scheduler->parent_ids[i]].max_time += max_clocks % scheduler->group_size[i];
		scheduler->times[scheduler->parent_ids[i]].cur_time = scheduler->times[scheduler->parent_ids[i]].max_time;
	}
	semSignal(scheduler_mutex);
}

void print_scheduler(fair_scheduler* scheduler) {
	//semWait(scheduler_mutex);
	printf("\nParents:");
	for (int i = 0; i < NUM_PARENTS; i++) {
		printf("\n\tGroup #:%d size: %d", i+1, scheduler->group_size[i]);
		printf("\n\tGroup #:%d Parent taskid: %d", i + 1, scheduler->parent_ids[i]);
	}
	printf("\nRemaining Times:");
	for (int i = 0; i < MAX_TASKS; i++) {
		if (1/*scheduler->times[i].group >= 0*/) {
			printf("\n\tTask #%d has %d clocks of %d\t in group: %d", i, scheduler->times[i].cur_time, 
				scheduler->times[i].max_time, scheduler->times[i].group);
		}
	}
	//semSignal(scheduler_mutex);
}

void check_vals(fair_scheduler * scheduler) {
	int left = 0;
	for (int i = 0; i < MAX_TASKS; i++) {
		if (scheduler->times[i].group >= 0) {
			left += scheduler->times[i].cur_time;
		}
	}
	if (!left) {
		for (int i = 0; i < MAX_TASKS; i++) {
			scheduler->times[i].cur_time = scheduler->times[i].max_time;
		}
	}
}

int get_time_for_task(int task_id, fair_scheduler* scheduler) {
	//printf("cur id: %d", task_id);
	//print_scheduler(scheduler);
	if (scheduler->times[task_id].group >= 0) {
		int to_return = scheduler->times[task_id].cur_time--;
		if (scheduler->times[task_id].cur_time < 0) scheduler->times[task_id].cur_time = 0;
		check_vals(scheduler);
		return to_return;
	}
	else {
		return 1;
	}
}
