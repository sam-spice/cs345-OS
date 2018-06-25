// os345mmu.c - LC-3 Memory Management Unit	03/05/2017
//
//		03/12/2015	added PAGE_GET_SIZE to accessPage()
//
// **************************************************************************
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
#include "os345lc3.h"

// ***********************************************************************
// mmu variables

// LC-3 memory
unsigned short int memory[LC3_MAX_MEMORY];

// statistics
int memAccess;						// memory accesses
int memHits;						// memory hits
int memPageFaults;					// memory faults
int clockRPT;						// RPT clock
int clockUPT;						// UPT clock
//int clockOuter;
//int clockInner; 
int nextPage;
int clockOuter;
int clockInner;

int getFrame(int);
int getAvailableFrame(void);
extern TCB tcb[];					// task control block
extern int curTask;					// current task #

int getFrame(int notme)
{
	int frame;
	frame = getAvailableFrame();
	if (frame >=0) return frame;

	// run clock
	frame = Clock(notme);
	if (frame == -1) printf("\nWe're toast!!!!!!!!!!!!");

	return frame;
}

void swapFrameToSwap(int swap_idx)
{
	
	int pte1;
	int pte2;

	// entry to swap, and entry to swap into its positin
	pte1 = memory[swap_idx];
	pte2 = memory[swap_idx + 1];

	// do the swap
	if (DIRTY(pte1) && PAGED(pte2)) accessPage(SWAPPAGE(pte2), FRAME(pte1), PAGE_OLD_WRITE);
	else if (!PAGED(pte2)) {
		memory[swap_idx + 1] = SET_PAGED(nextPage);
		accessPage(NULL, FRAME(pte1), PAGE_NEW_WRITE);
	}
	// clear the memory if I don't do this i get weird trap errors
	memory[swap_idx] = 0;
	return;
}

int Clock(int curr_frame) {
	int frame_to_return;

	int loop_delimiter = 50;
	while(loop_delimiter){
		// variables copied from the getmemaddr func
		int rpta, rpte1, rpte2;
		int upta, upte1, upte2;
		int rptFrame, uptFrame;
		// loop back around if you reach the end
		if ((clockOuter += 2) >= LC3_RPT_END) {
			clockOuter = LC3_RPT;
			loop_delimiter--;
		}
		// grab the root page table 
		rpte1 = memory[clockOuter];
		if (DEFINED(rpte1) && REFERENCED(rpte1)) {
			
			memory[clockOuter] = rpte1 = CLEAR_REF(rpte1);
		}
		else if (DEFINED(rpte1)) {
			// get the user page table addres
			upta = (FRAME(rpte1) << 6);

			// loop over the inner portion
			for (int i = 0; i < 64; i += 2) { 
				upte1 = memory[upta + i];
				if (PINNED(upte1) || FRAME(upte1) == curr_frame)continue;

				else if (DEFINED(upte1) && REFERENCED(upte1)) { 
					// pin the root page table entry, without this I fail memtest
					// also need to keep it pinned here without pinning it not just in memory array
					rpte1 = SET_PINNED(rpte1);
					memory[clockOuter] = rpte1;

					//clear the reference to the upt entry
					upte1 = CLEAR_REF(upte1);
					memory[upta + i] = upte1;
				}
				//found and not referenced
				else if (DEFINED(upte1) && !REFERENCED(upte1)) { 
					// similar to previous block of code
					rpte1 = SET_DIRTY(rpte1);
					memory[clockOuter] = rpte1;

					frame_to_return = FRAME(upte1);
					swapFrameToSwap(upta + i);
					// found the guy to get rid of so return it
					return frame_to_return;
				}
				else continue;
			}
			// if I got here there was no frame to remove in the current upt so see if I can remove the upt
			if (!REFERENCED(rpte1) && !PINNED(rpte1) && DEFINED(rpte1) &&FRAME(rpte1) != curr_frame) { 
				frame_to_return = FRAME(rpte1);

				swapFrameToSwap(clockOuter);

				return frame_to_return;
			}
			// nothing could be returns so take another spin
			else { 
				rpte1 = CLEAR_PINNED(rpte1);
				memory[clockOuter] = rpte1;
			}

		}
	} 
	return -1;
}
// **************************************************************************
// **************************************************************************
// LC3 Memory Management Unit
// Virtual Memory Process
// **************************************************************************
//           ___________________________________Frame defined
//          / __________________________________Dirty frame
//         / / _________________________________Referenced frame
//        / / / ________________________________Pinned in memory
//       / / / /     ___________________________
//      / / / /     /                 __________frame # (0-1023) (2^10)
//     / / / /     /                 / _________page defined
//    / / / /     /                 / /       __page # (0-4096) (2^12)
//   / / / /     /                 / /       /
//  / / / /     / 	             / /       /
// F D R P - - f f|f f f f f f f f|S - - - p p p p|p p p p p p p p

#define MMU_ENABLE	1

unsigned short int *getMemAdr(int va, int rwFlg)
{
	memAccess += 2;
	unsigned short int pa;
	int rpta, rpte1, rpte2;
	int upta, upte1, upte2;
	int rptFrame, uptFrame;

	// turn off virtual addressing for system RAM
	if (va < 0x3000) return &memory[va];
#if MMU_ENABLE
	rpta = tcb[curTask].RPT + RPTI(va);		// root page table address
	rpte1 = memory[rpta];					// FDRP__ffffffffff
	rpte2 = memory[rpta+1];					// S___pppppppppppp
	if (DEFINED(rpte1))	memHits++;					// rpte defined
	else{									// rpte undefined
		memPageFaults++;
			rptFrame = getFrame(-1);
			rpte1 = SET_DEFINED(rptFrame);
			
			//swap in the memory we are looking for
			if (PAGED(rpte2)) accessPage(SWAPPAGE(rpte2), rptFrame, PAGE_READ);
			// initialize upt memory
			else memset(&memory[(rptFrame << 6)], 0, 128);
	}					

	memory[rpta] = SET_REF(rpte1);			// set rpt frame access bit
	memory[rpta + 1] = rpte2;


	upta = (FRAME(rpte1)<<6) + UPTI(va);	// user page table address
	upte1 = memory[upta]; 					// FDRP__ffffffffff
	upte2 = memory[upta+1]; 				// S___pppppppppppp
	if (DEFINED(upte1))	memHits++;			// upte defined
	// upte undefined
	else{
		memPageFaults++;
		uptFrame = getFrame(FRAME(memory[rpta]));
		memory[rpta] = SET_REF(SET_DIRTY(rpte1));
		upte1 = SET_DEFINED(uptFrame);
		if (PAGED(upte2)) accessPage(SWAPPAGE(upte2), uptFrame, PAGE_READ);
		// initialize upt memory
		// turns out there is nothing to initilialize but hey

	}
	if (rwFlg) upte1 = SET_DIRTY(upte1);
	
	// save upte1 
	memory[upta] = SET_REF(upte1); 			// set upt frame access bit
	// save changes made to upte2
	memory[upta + 1] = upte2;
	return &memory[(FRAME(upte1)<<6) + FRAMEOFFSET(va)];
#else
	return &memory[va];
#endif
} // end getMemAdr


// **************************************************************************
// **************************************************************************
// set frames available from sf to ef
//    flg = 0 -> clear all others
//        = 1 -> just add bits
//
void setFrameTableBits(int flg, int sf, int ef)
{	int i, data;
	int adr = LC3_FBT-1;             // index to frame bit table
	int fmask = 0x0001;              // bit mask

	// 1024 frames in LC-3 memory
	for (i=0; i<LC3_FRAMES; i++)
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;
			adr++;
			data = (flg)?MEMWORD(adr):0;
		}
		else fmask = fmask >> 1;
		// allocate frame if in range
		if ( (i >= sf) && (i < ef)) data = data | fmask;
		MEMWORD(adr) = data;
	}
	return;
} // end setFrameTableBits


// **************************************************************************
// get frame from frame bit table (else return -1)
int getAvailableFrame()
{
	int i, data;
	int adr = LC3_FBT - 1;				// index to frame bit table
	int fmask = 0x0001;					// bit mask

	for (i=0; i<LC3_FRAMES; i++)		// look thru all frames
	{	if (fmask & 0x0001)
		{  fmask = 0x8000;				// move to next work
			adr++;
			data = MEMWORD(adr);
		}
		else fmask = fmask >> 1;		// next frame
		// deallocate frame and return frame #
		if (data & fmask)
		{  MEMWORD(adr) = data & ~fmask;
			return i;
		}
	}
	return -1;
} // end getAvailableFrame



// **************************************************************************
// read/write to swap space
int accessPage(int pnum, int frame, int rwnFlg)
{
							// swap page size
	static int pageReads;						// page reads
	static int pageWrites;						// page writes
	static unsigned short int swapMemory[LC3_MAX_SWAP_MEMORY];

	if ((nextPage >= LC3_MAX_PAGE) || (pnum >= LC3_MAX_PAGE))
	{
		printf("\nVirtual Memory Space Exceeded!  (%d)", LC3_MAX_PAGE);
		exit(-4);
	}
	switch(rwnFlg)
	{
		case PAGE_INIT:                    		// init paging
			clockRPT = 0;						// clear RPT clock
			clockUPT = 0;						// clear UPT clock
			memAccess = 0;						// memory accesses
			memHits = 0;						// memory hits
			memPageFaults = 0;					// memory faults
			nextPage = 0;						// disk swap space size
			pageReads = 0;						// disk page reads
			pageWrites = 0;						// disk page writes
			return 0;

		case PAGE_GET_SIZE:                    	// return swap size
			return nextPage;

		case PAGE_GET_READS:                   	// return swap reads
			return pageReads;

		case PAGE_GET_WRITES:                    // return swap writes
			return pageWrites;

		case PAGE_GET_ADR:                    	// return page address
			return (int)(&swapMemory[pnum<<6]);

		case PAGE_NEW_WRITE:                   // new write (Drops thru to write old)
			pnum = nextPage++;

		case PAGE_OLD_WRITE:                   // write
			//printf("\n    (%d) Write frame %d (memory[%04x]) to page %d", p.PID, frame, frame<<6, pnum);
			memcpy(&swapMemory[pnum<<6], &memory[frame<<6], 1<<7);
			pageWrites++;
			return pnum;

		case PAGE_READ:                    	// read
			//printf("\n    (%d) Read page %d into frame %d (memory[%04x])", p.PID, pnum, frame, frame<<6);
			memcpy(&memory[frame<<6], &swapMemory[pnum<<6], 1<<7);
			pageReads++;
			return pnum;

		case PAGE_FREE:                   // free page
			printf("\nPAGE_FREE not implemented");
			break;
   }
   return pnum;
} // end accessPage
