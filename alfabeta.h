/*
 * Copyright 1993 Ola Liljedahl
 */

#ifndef _ALFABETA_H
#define _ALFABETA_H

#include <stdint.h>

#define INFINITY 30000	/* a value larger than ALL values from the evaluation */

typedef enum { false=0, true=1 } bool;

typedef uint64_t bits64;
typedef bits64 matrix;

typedef struct
{
	matrix black,white;
} board;

/* some matrix operations */
#define SETBIT(m,b) do { (m)|=1ULL<<(b); } while (0)
#define INVBIT(m,b) do { (m)^=1ULL<<(b); } while (0)
#define GETBIT(m,b) ( ((m)>>(b))&1 )
#define LOWER(m) (uint32_t)((m) >> 32)
#define UPPER(m) (uint32_t)(m)

typedef enum { black=0, white=1 } color_t;
typedef enum { dummyeval=-1,discdiff,winloss,mobility,potmobility } evaluation_t;
typedef enum
{
	a1= 0,b1= 1,c1= 2,d1= 3,e1= 4,f1= 5,g1= 6,h1= 7,
	a2= 8,b2= 9,c2=10,d2=11,e2=12,f2=13,g2=14,h2=15,
	a3=16,b3=17,c3=18,d3=19,e3=20,f3=21,g3=22,h3=23,
	a4=24,b4=25,c4=26,d4=27,e4=28,f4=29,g4=30,h4=31,
	a5=32,b5=33,c5=34,d5=35,e5=36,f5=37,g5=38,h5=39,
	a6=40,b6=41,c6=42,d6=43,e6=44,f6=45,g6=46,h6=47,
	a7=48,b7=49,c7=50,d7=51,e7=52,f7=53,g7=54,h7=55,
	a8=56,b8=57,c8=58,d8=59,e8=60,f8=61,g8=62,h8=63,
	pass=64,none=65,quit=66,load=67,undef=255 /* plus some extra values that are handy */
} move_t;

bool init_tables(long cachesize);

void set_input(evaluation_t eval,int discs,int maxply,int ssdepth,bool winloss);

void get_output(move_t *bestmove,bool *earlyexit,long *abcalls,long *hits,long *accesses);

int first_bit_m(matrix);

matrix new_black_discs(board brd,move_t move);

board flip_discs(board brd,matrix newdiscs);

board swap_sides(board brd);

int count_matrix(matrix);

bool legal_move(board brd,move_t move);

int alfabeta(board brd,int ply,int alfa,int beta,bool otherpassed,color_t tomove,int cornermove);

int endgame_alfabeta(board brd,int ply,int alfa,int beta,bool otherpassed,color_t tomove);

int earlyeg_alfabeta(board brd,color_t tomove,unsigned target);

/* move order variables */
extern uint8_t MOVES[2][64];	/* index -> move, the moves in priority order */
extern int SUCCESS[2][64];	/* move -> success value */
extern int NUMMOVES;		/* number of valid moves in MOVES array, decreases as moves are done */

#endif
