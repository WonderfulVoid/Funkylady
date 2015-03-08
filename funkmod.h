/*
 * Copyright 1993 Ola Liljedahl
 */

#ifndef _FUNKMOD_H
#define _FUNKMOD_H

#include "alfabeta.h"

typedef struct
{
	int bestmove;
	int bestvalue;
	long target;
	long elapsed;
	int nodes;
	int depth;
	bool earlyexit;
} result_t;

typedef enum { normal,secondary } search_t;

void init_move_order(board brd); /* initialize move order with initial table */

result_t endgame_search(board brd,color_t tomove,long targettime,evaluation_t);

result_t iterative_deepening(board brd,color_t tomove,unsigned targettime,unsigned extratime,search_t,evaluation_t,bool randomsort);

#endif
