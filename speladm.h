/*
 * Copyright 1993 Ola Liljedahl
 */

#ifndef _SPELADM_H
#define _SPELADM_H

#include "position.h"

extern char *PIPEIN;
extern char *PIPEOUT;

void replay_moves(position_t *pos,int frommove /* 0..59 */ );

bool play_game(position_t *pos);

#endif
