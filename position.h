/*
 * Copyright 1993 Ola Liljedahl
 */

#ifndef _POSITION_H
#define _POSITION_H

#include "alfabeta.h"

#define MAXMOVE 80

typedef struct
{
	unsigned long timestamp;
	board brade;
	color_t turntomove,computer;
	int targettime,humantime,computertime;
	int passes;
	int nummoves; /* number of used positions in moves & times */
	move_t moves[MAXMOVE]; /* includes passes as moves */
	short times[MAXMOVE]; /* the time that each move took */
	evaluation_t eval[60];
	long cachesize;
	bool deterministic;
	char computername[20],humanname[20];
} position_t;

STATIC bool save_position(char *filename,position_t *pos);

STATIC bool load_position(char *filename,position_t *pos,bool parameters);

#endif
