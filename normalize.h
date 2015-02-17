/*
 * Copyright 1993 Ola Liljedahl
 */

#ifndef _NORMALIZE_H
#define _NORMALIZE_H

#include "alfabeta.h"

STATIC move_t transform_move(move_t move,int tf);

STATIC move_t inverse_transform_move(move_t move,int tf);

STATIC int normalize_board(board *pos);

#endif
