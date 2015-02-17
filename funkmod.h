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

STATIC void init_move_order(board brd); /* initialize move order with initial table */

STATIC result_t endgame_search(board brd,color_t tomove,long targettime,evaluation_t);

STATIC result_t iterative_deepening(board brd,color_t tomove,long targettime,long extratime,search_t,evaluation_t,bool randomsort);

#endif
