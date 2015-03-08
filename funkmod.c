/*
 * Copyright 1993 Ola Liljedahl
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "funkmod.h"
#include "times.h"

static __inline__ bool bool_random(void)
{
	return rand()&1;
}

/*
	make sanity checks on move order
*/

static void sanity_checks(board brd,color_t tomove)
{
	int i;
	matrix occupied;
	uint8_t *moves=MOVES[tomove];
	occupied=brd.black&brd.white;
	while(occupied)
	{
		int pos=first_bit_m(occupied);
		printf("position %d set in both black and white\n",pos);
		INVBIT(occupied,pos);
	}
	occupied=brd.black|brd.white;
	for(i=0;i<NUMMOVES;i++)
	{
		if(GETBIT(brd.black,moves[i]) || GETBIT(brd.white,moves[i])) printf("moves[%d]=%d occupied\n",i,moves[i]);
		SETBIT(occupied,moves[i]);
	}
	if(occupied!=0xffffffffffffffffULL)
	{
		occupied=~occupied;
		while(occupied)
		{
			int pos=first_bit_m(occupied);
			printf("position %d missing from both MOVES and board\n",pos);
			INVBIT(occupied,pos);
		}
	}
	if(NUMMOVES!=64-count_matrix(brd.black)-count_matrix(brd.white)) printf("NUMMOVES %d blacks %d whites %d\n",NUMMOVES,count_matrix(brd.black),count_matrix(brd.white));
}

/*
	remove made moves from the move order
	initialize order from the static table
*/

void init_move_order(board brd)
{
	int i,nummoves=0;
	matrix occupied;
	static const uint8_t moves[64]=	/* the moves in initial priority ordering */
	{
		 0,  7, 56, 63,  2,  5, 16, 23, 40, 47, 58, 61,  3,  4, 24, 31,
		32, 39, 59, 60, 19, 20, 26, 29, 34, 37, 43, 44, 18, 21, 42, 45,
		11, 12, 25, 30, 33, 38, 51, 52, 10, 13, 17, 22, 41, 46, 50, 53,
		 1,  6,  8, 15, 48, 55, 57, 62,  9, 14, 49, 54, 27, 28, 35, 36
	};
	occupied=brd.black|brd.white;

	for(i=0;i<64;i++)
	{
		if(!GETBIT(occupied,moves[i]))
		{
			MOVES[0][nummoves]=moves[i];
			MOVES[1][nummoves]=moves[i];
			nummoves++;
		}
	}
	NUMMOVES=nummoves;

	for(i=0;i<64;i++)
	{
		SUCCESS[0][i]=0;
		SUCCESS[1][i]=0;
	}

	sanity_checks(brd,black);
	sanity_checks(brd,white);
}

/*
	remove made moves from the move order
	do not change the order
*/

static void remove_played_moves(board brd)
{
	int i,nummoves0=0,nummoves1=0;
	uint8_t moves[2][64];
	matrix occupied;
	occupied=brd.black|brd.white;

	for(i=0;i<NUMMOVES;i++)
	{
		moves[0][i]=MOVES[0][i];
		moves[1][i]=MOVES[1][i];
	}

	for(i=0;i<NUMMOVES;i++)
	{
		if(!GETBIT(occupied,moves[0][i]))
		{
			MOVES[0][nummoves0++]=moves[0][i];
		}
		if(!GETBIT(occupied,moves[1][i]))
		{
			MOVES[1][nummoves1++]=moves[1][i];
		}
	}
	assert(nummoves0==nummoves1);
	NUMMOVES=nummoves0;

	for(i=0;i<64;i++)
	{
		SUCCESS[0][i]=0;
		SUCCESS[1][i]=0;
	}

	sanity_checks(brd,black);
	sanity_checks(brd,white);
}

/*
	sort the move order partially according to the new success values
*/

static void sort_move_array(board brd,uint8_t moves[],int success[],bool random)
{
	int i;
	matrix occupied;
	occupied=brd.black|brd.white;

	for(i=0;i<NUMMOVES;i++)
	{
		int m,bestm=undef,bests=-1;
		for(m=0;m<64;m++)
		{
			if(!GETBIT(occupied,m) && (success[m]>bests || (random && success[m]==bests && bool_random())))
			{
				bests=success[bestm=m];
			}
		}
		moves[i]=(uint8_t)bestm;
		success[bestm]=-2;
	}
	for(i=0;i<64;i++) success[i]=0;
}

static void sort_move_arrays(board brd,bool random)
{
	sort_move_array(brd,MOVES[0],SUCCESS[0],random);
	sort_move_array(brd,MOVES[1],SUCCESS[1],random);
	sanity_checks(brd,black);
	sanity_checks(brd,white);
}

result_t endgame_search(board brd,color_t tomove,long target,evaluation_t eval)
{
	result_t result;
	unsigned long start=cpu_time(),elapsed;
	int bestvalue,maxply=64-count_matrix(brd.black)-count_matrix(brd.white);
	move_t BESTMOVE;
	bool EARLYEXIT;
	long ABCALLS,HITS,ACCESSES;

	if(target<100) target=100; /* always at least one second */
	memset(&result,0,sizeof result);
	result.target=target;
	remove_played_moves(brd);

	if(eval==discdiff) printf("Endgame calculation\n");
	else if(eval==winloss) printf("Early endgame (win/loss) calculation\n");
	printf("Search time target %lu.%02lu secs\n",target/100,target%100);

	set_input(eval,count_matrix(brd.black)+count_matrix(brd.white),maxply,0,eval==winloss);
	if(eval==discdiff)
		bestvalue=endgame_alfabeta(tomove==black?brd:swap_sides(brd),0,-INFINITY,INFINITY,false,tomove);
	else /* eval==winloss */
		bestvalue=earlyeg_alfabeta(tomove==black?brd:swap_sides(brd),tomove,target);
	elapsed=cpu_time()-start;
	get_output(&BESTMOVE,&EARLYEXIT,&ABCALLS,&HITS,&ACCESSES);

	result.bestmove=BESTMOVE;
	result.bestvalue=bestvalue;
	result.elapsed=elapsed;
	result.nodes=ABCALLS;
	result.depth=maxply;
	result.earlyexit=EARLYEXIT;

	{
		char move[10];
		if((unsigned int)BESTMOVE<64)
		{
			move[0]=(char)(BESTMOVE%8+'a');
			move[1]=(char)(BESTMOVE/8+'1');
		}
		else
		{
			move[0]='-';
			move[1]='-';
		}
		move[2]=0;
		printf("ply ss  bm  value   nodes  time   nodes/s\n");
		if(elapsed) printf("%2d  %2d  %s %5d %8lu %3lu.%02lu  %7lu\n",maxply,0,move,bestvalue,ABCALLS,elapsed/100,elapsed%100,100*ABCALLS/elapsed);
		else		printf("%2d  %2d  %s %5d %6lu  ~0\n",maxply,0,move,bestvalue,ABCALLS);
	}

	if(EARLYEXIT) printf("Search was terminated early\n");

	return result;
}

result_t iterative_deepening(board brd,color_t tomove,unsigned target,unsigned extra,search_t mode,evaluation_t eval,bool randomsort)
{
#define SSDEPTH 2
	result_t result;
	unsigned long start=cpu_time(),elapsed;
	int maxply,first=3,last=64-count_matrix(brd.black)-count_matrix(brd.white);
	int prevbv=-INFINITY;
	move_t BESTMOVE;
	bool EARLYEXIT;
	long ABCALLS,HITS,ACCESSES;

	if(target<100) target=100; /* always at least one second */
	memset(&result,0,sizeof result);
	result.target=target;
	remove_played_moves(brd);

	printf("Search time target %u.%02u secs\n",target/100,target%100);
	printf("ply ss  bm  value   nodes hits  time   nodes/s\n");

	for(maxply=first;maxply<=last && (cpu_time()-start)<target*2/3;maxply++)
	{
		int bestvalue = 0;
		prevbv=bestvalue;
		set_input(eval,count_matrix(brd.black)+count_matrix(brd.white),maxply,mode==secondary?SSDEPTH:0,false);
		{
			unsigned long start,elapsed;
			start=cpu_time();
			bestvalue=alfabeta(tomove==black?brd:swap_sides(brd),0,-INFINITY,INFINITY,false,tomove,0);
			elapsed=cpu_time()-start;
			get_output(&BESTMOVE,&EARLYEXIT,&ABCALLS,&HITS,&ACCESSES);

			result.bestmove=BESTMOVE;
			result.bestvalue=bestvalue;
			result.elapsed+=elapsed;
			result.nodes+=ABCALLS;
			result.depth=maxply;

			if(elapsed) printf("%2d  %2d  %c%c %5d %8lu %3lu%% %3lu.%02lu  %7lu\n",maxply,mode==secondary?SSDEPTH:0,BESTMOVE%8+'a',BESTMOVE/8+'1',bestvalue,ABCALLS,ACCESSES?100*HITS/ACCESSES:0,elapsed/100,elapsed%100,100*ABCALLS/elapsed);
			else		printf("%2d  %2d  %c%c %5d %8lu %3lu%%  ~0\n",maxply,mode==secondary?SSDEPTH:0,BESTMOVE%8+'a',BESTMOVE/8+'1',bestvalue,ABCALLS,ACCESSES?100*HITS/ACCESSES:0);
		}
		if(BESTMOVE==undef)
		{
			fprintf(stderr, "BESTMOVE==undef\n");
			abort();
			break;
		}

		if(extra && maxply<last && cpu_time()-start>=target*2/3 && bestvalue<prevbv && bestvalue<prevbv-abs(prevbv)/2 && cpu_time()-start<(target+extra)*2/3)
		{
			printf("Increasing target time with %u.%02u secs\n",extra/100,extra%100);
			target+=extra;
			result.target+=extra;
			extra=0;
		}
		SUCCESS[tomove][BESTMOVE]=1<<30; /* a small fix (?) */
		sort_move_arrays(brd,randomsort);
	}
	elapsed=cpu_time()-start;
	printf("Total elapsed time %lu.%02lu secs\n",elapsed/100,elapsed%100);
	return result;
#undef SSDEPTH
}
