/*
 * Copyright 1993 Ola Liljedahl
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "times.h"
#include "alfabeta.h"

#ifdef __GNUC__
#define likely(x)   __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#define EMPTY 0

/* variables that are readonly after initialization */
static matrix RAY[64];	/* position -> matrix */
static matrix NBS[64];	/* position -> matrix */
static matrix FLIP[64][64]; /* position -> position -> matrix */

/* structure and variables that are used by the evaluation hashcache */
struct element
{
	board brade;
	short value;
};
static struct element *CACHE;
static long NUMELEMS,HITS,ACCESSES;

/* variables used by the move ordering */
uint8_t MOVES[2][64];	/* index -> move, the moves in priority ordering */
int SUCCESS[2][64];	/* move -> priority */
int NUMMOVES;		/* the number of valid moves in MOVES array, decreases as moves are done */

/* global alfabeta variables - input, constant during an alfabeta search */
static int DISCS;		/* total number of discs on board when search starts */
static int MAXPLY;		/* how many moves to make before evaluation == search depth */
static int SSDEPTH;		/* MAXPLY+how many plies extra to search when searching in a corner region */
static int BADVALUE;	/* how bad is an X-square */
static bool WINLOSS;	/* early endgame search => value=sign(discdifference) */
static evaluation_t EVAL=potmobility;	/* what evaluation function to use */

/* global alfabeta variables - output, updated during an alfabeta search */
static move_t BESTMOVE;	/* contains the best alfabeta move after an alfabeta search */
static bool EARLYEXIT;	/* someone tries to tell us search was terminated early */
static long ABCALLS;	/* number of alfabeta calls = total number of nodes */

static __inline__ int sign(int v)
{
	return v<0 ? -1 : v>0 ? 1 : 0 ;
}

static __inline__ int absval(int v)
{
	return v<0 ? -v : v ;
}

#define AND(a,b) ((a)&(b))
#define OR(a,b) ((a)|(b))
#define NOT(a) (~(a))
#define LIT(a,b) 0x##a##b##ULL

/*
	initialize RAY,NBS,FLIP
*/

STATIC bool init_tables(long cachesize)
{
	int i;
	(void)cachesize;
	if(cachesize>0)
	{
		NUMELEMS=cachesize;
		CACHE=(struct element *)calloc(cachesize,sizeof(struct element));
		if(!CACHE)
		{
			fprintf(stderr,"error: could not allocate memory (%lu bytes) for cache\n",cachesize*sizeof(struct element));
			return false;
		}
	}
	else
	{
		NUMELEMS=0;
		CACHE=0;
	}
	for(i=0;i<64;i++)
	{
		int x,y;
		matrix m=EMPTY;
		for(x=i%8,y=i/8;--x>=0;) SETBIT(m,8*y+x);
		for(x=i%8,y=i/8;++x<=7;) SETBIT(m,8*y+x);
		for(x=i%8,y=i/8;--y>=0;) SETBIT(m,8*y+x);
		for(x=i%8,y=i/8;++y<=7;) SETBIT(m,8*y+x);
		for(x=i%8,y=i/8;--x>=0&&--y>=0;) SETBIT(m,8*y+x);
		for(x=i%8,y=i/8;++x<=7&&--y>=0;) SETBIT(m,8*y+x);
		for(x=i%8,y=i/8;--x>=0&&++y<=7;) SETBIT(m,8*y+x);
		for(x=i%8,y=i/8;++x<=7&&++y<=7;) SETBIT(m,8*y+x);
		RAY[i]=m;
	}
	for(i=0;i<64;i++)
	{
		bool l=i%8!=0;
		bool r=i%8!=7;
		bool u=i>=8;
		bool d=i<56;
		matrix m=EMPTY;
		if(l) SETBIT(m,i-1);
		if(r) SETBIT(m,i+1);
		if(u) SETBIT(m,i-8);
		if(d) SETBIT(m,i+8);
		if(l&&u) SETBIT(m,i-1-8);
		if(r&&u) SETBIT(m,i+1-8);
		if(l&&d) SETBIT(m,i-1+8);
		if(r&&d) SETBIT(m,i+1+8);
		NBS[i]=m;
	}
	for(i=0;i<64;i++)
	{
		int ri=i/8,ki=i%8,j;
		for(j=0;j<64;j++)
		{
			int rj=j/8,kj=j%8;
			matrix m=EMPTY;
			if(ri==rj) /* same row */
			{
				int k;
				if(ki<kj) for(k=ki+1;k<kj;k++) SETBIT(m,8*ri+k);
				else if(ki>kj) for(k=ki-1;k>kj;k--) SETBIT(m,8*ri+k);
			}
			else if(ki==kj) /* same column */
			{
				int r;
				if(ri<rj) for(r=ri+1;r<rj;r++) SETBIT(m,8*r+ki);
				else if(ri>rj) for(r=ri-1;r>rj;r--) SETBIT(m,8*r+ki);
			}
			else
			{
				int dr=ri-rj;
				int dk=ki-kj;
				if(absval(dk)==absval(dr) && dr!=0) /* same diagonale */
				{
					int sr=sign(dr);
					int sk=sign(dk);
					int r,k;
					for(r=rj+sr,k=kj+sk;r!=ri && k!=ki;r+=sr,k+=sk) SETBIT(m,8*r+k);
				}
			}
			FLIP[i][j]=m;
		}
	}
	return true;
}

STATIC void get_output(move_t *bestmove,bool *earlyexit,long *abcalls,long *hits,long *accesses)
{
	*bestmove=BESTMOVE;
	*earlyexit=EARLYEXIT;
	*abcalls=ABCALLS;
	*hits=HITS;
	*accesses=ACCESSES;
}

/*
	return position of first occupied square of a matrix
	LSB is 0.
*/

STATIC __inline__ int first_bit_m(matrix m)
{
#ifdef __GNUC__
	return __builtin_ffsll(m) - 1;
#else
	return ffsll(m) - 1;
#endif
}

/*
	count number of set bits in a 32-bit number
*/

static __inline__ int count_bits32(uint32_t lw)
{
#if defined __GNUC__ && (!defined __x86_64__ || defined __POPCNT__)
	/* Only use GCC's builtin_popcount() on x86 if it can generate the POPCNT
	 * instruction, GCC's SW emulation is slower than our code below */
	return __builtin_popcount(lw);
#else
    // count bits of each 2-bit chunk
    lw  = lw - ((lw >> 1) & 0x55555555);
    // count bits of each 4-bit chunk
    lw  = (lw & 0x33333333) + ((lw >> 2) & 0x33333333);
    // count bits of each 8-bit chunk
    lw  = lw + (lw >> 4);
    // mask out junk
    lw &= 0xF0F0F0F;
    // add all four 8-bit chunks
    return (lw * 0x01010101) >> 24;
#endif
}

/*
	count number of set bits in a 64-bit number
*/

static __inline__ int count_bits64(bits64 ll)
{
    return count_bits32(LOWER(ll)) + count_bits32(UPPER(ll));
}

/*
	count number of set bits in a matrix
*/

STATIC __inline__ int count_matrix(matrix m)
{
	return count_bits64(m);
}

STATIC __inline__ board swap_sides(board brd)
{
	board revb;
	revb.black=brd.white;
	revb.white=brd.black;
	return revb;
}

/*
	return a matrix with discs that should be turned/become black
	if move is not a legal move return an empty matrix
	(all legal moves turn at least one disc...)
*/

STATIC __inline__ matrix new_black_discs(board brd,move_t move)
{
	matrix newdiscs=EMPTY;
	if(GETBIT(brd.black,move)==0 && GETBIT(brd.white,move)==0 && NBS[move]&brd.white)
	{
		bits64 brackets;
		brackets=RAY[move] & brd.black;
		while(brackets)
		{
			int bracket=first_bit_m(brackets);
			matrix flipdiscs=FLIP[move][bracket];
			if(flipdiscs && flipdiscs==(flipdiscs&brd.white))
			{
				newdiscs|=flipdiscs;
			}
			brackets^=1ULL<<bracket;
		}
		if(newdiscs)
		{
			SETBIT(newdiscs,move);
		}
	}
	return newdiscs;
}

/*
	turn the specified discs on a board
*/

STATIC __inline__ board flip_discs(board brd,matrix newdiscs)
{
	brd.black|=newdiscs;
	brd.white&=~newdiscs;
	return brd;
}

/*
	return true if legal move
*/

STATIC __inline__ bool legal_move(board brd,move_t move)
{
	if(GETBIT(brd.black,move)==0 && GETBIT(brd.white,move)==0 && (NBS[move]&brd.white))
	{
		bits64 brackets;
		brackets=RAY[move] & brd.black;
		while(brackets)
		{
			int bracket=first_bit_m(brackets);
			matrix flipdiscs=FLIP[move][bracket];
			if(flipdiscs && flipdiscs==(flipdiscs&brd.white)) return true;
			brackets^=1ULL<<bracket;
		}
	}
	return false;
}

/*
	a faster version of legal_move
	no check for potential mobility
*/

static __inline__ bool fast_legal_move(board brd,move_t move)
{
	bits64 brackets;
	brackets=RAY[move] & brd.black;
	while(brackets)
	{
		int bracket=first_bit_m(brackets);
		matrix flipdiscs=FLIP[move][bracket];
		if(flipdiscs && flipdiscs==(flipdiscs&brd.white)) return true;
		brackets^=1ULL<<bracket;
	}
	return false;
}

/*
	evaluate a corner region, inclusive corner mobility, for both black and white
*/

static __inline__ int corner_value(board brd,int h,int ca,int cb,int x)
{
	int value;
	bool bca=GETBIT(brd.black,ca);
	bool bcb=GETBIT(brd.black,cb);
	bool bx =GETBIT(brd.black,x );
	bool wca=GETBIT(brd.white,ca);
	bool wcb=GETBIT(brd.white,cb);
	bool wx =GETBIT(brd.white,x );
	if(GETBIT(brd.black,h))
	{
		value=15;
		     if(bca) value+=10;
		else if(wca) value-=5;
		     if(bcb) value+=10;
		else if(wcb) value-=5;
		     if(bx ) value+=10;
		else if(wx ) value-=5;
	}
	else if(GETBIT(brd.white,h))
	{
		value=-15;
		     if(bca) value+=5;
		else if(wca) value-=10;
		     if(bcb) value+=5;
		else if(wcb) value-=10;
		     if(bx ) value+=5;
		else if(wx ) value-=10;
	}
	else /* corner is empty */
	{
		value=0;
		if(bca) value-=1;
		if(bcb) value-=1;
		if(bx ) value-=BADVALUE;
		if((bca || bcb || bx) && fast_legal_move(swap_sides(brd),h)) value-=15; /* white has corner mobility - bad */
		else if((wca || wcb || wx) && fast_legal_move(brd,h)) value+=15; /* black has corner mobility - good */
		if(wca) value+=1;
		if(wcb) value+=1;
		if(wx ) value+=BADVALUE;
	}
	return value;
}

/*
	value all corner regions
*/

static __inline__ int corner_values(board brd)
{
	return
		corner_value(brd,a1,b1,a2,b2)+
		corner_value(brd,h1,g1,h2,g2)+
		corner_value(brd,a8,a7,b8,b7)+
		corner_value(brd,h8,g8,h7,g7);
}

/*
	disc counter
*/

static __inline__ int disc_difference(board brd)
{
	return count_matrix(brd.black)-count_matrix(brd.white);
}

static __inline__ int disc_difference2(board brd,int ply)
{
	return 2*count_matrix(brd.black)-DISCS-ply;
}

static __inline__ bits64 shiftr(bits64 b,int n)
{
	return b >> n;
}

static __inline__ bits64 shiftl(bits64 b,int n)
{
	return b << n;
}

#if 0
static __inline__ int mobility(board brd)
{
	bits64 black,white,wwb,wbw,blacks,whites,blm=0,wlm=0,empty;
	black=brd.black;
	white=brd.white;
	empty=~(black|white);

#define MOBS(MASK,SHIFT,AMOUNT)\
	wbw=wwb=empty;\
	blacks=SHIFT(black&MASK,AMOUNT);\
	whites=SHIFT(white&MASK,AMOUNT);\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks&MASK,AMOUNT);\
	whites=SHIFT(whites&MASK,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks&MASK,AMOUNT);\
	whites=SHIFT(whites&MASK,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks&MASK,AMOUNT);\
	whites=SHIFT(whites&MASK,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks&MASK,AMOUNT);\
	whites=SHIFT(whites&MASK,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks&MASK,AMOUNT);\
	whites=SHIFT(whites&MASK,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks&MASK,AMOUNT);\
	whites=SHIFT(whites&MASK,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;

#define MOBSNOMASK(SHIFT,AMOUNT)\
	wbw=wwb=empty;\
	blacks=SHIFT(black,AMOUNT);\
	whites=SHIFT(white,AMOUNT);\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks,AMOUNT);\
	whites=SHIFT(whites,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks,AMOUNT);\
	whites=SHIFT(whites,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks,AMOUNT);\
	whites=SHIFT(whites,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks,AMOUNT);\
	whites=SHIFT(whites,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks,AMOUNT);\
	whites=SHIFT(whites,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;\
	wwb=wwb&whites;\
	wbw=wbw&blacks;\
	blacks=SHIFT(blacks,AMOUNT);\
	whites=SHIFT(whites,AMOUNT);\
	blm|=wwb&blacks;\
	wlm|=wbw&whites;

	MOBS(0x7f7f7f7f7f7f7f7fLL,shiftl,1)
	MOBS(0xfefefefefefefefeLL,shiftr,1)
	MOBS(0xfefefefefefefefeLL,shiftl,7)
	MOBS(0x7f7f7f7f7f7f7f7fLL,shiftr,7)
	MOBS(0x7f7f7f7f7f7f7f7fLL,shiftl,9)
	MOBS(0xfefefefefefefefeLL,shiftr,9)
	MOBSNOMASK(shiftl,8)
	MOBSNOMASK(shiftr,8)
	return 4*( count_bits64(blm)-count_bits64(wlm) ) ;
#undef MOBS
#undef MOBSNOMASK
}

static int evaluate_mob(board brd)
{
	return mobility(brd)+corner_values(brd);
}

static int evaluate_mob_cache(board brd)
{
#define EQUAL(b1,b2) (b1.black.lower==b2.black.lower && b1.black.upper==b2.black.upper && b1.white.lower==b2.white.lower && b1.white.upper==b2.white.upper)
	struct element *elem=&CACHE[(brd.black.lower+brd.black.upper+brd.white.lower+brd.white.upper)%NUMELEMS];
	ACCESSES++;
	if(EQUAL(elem->brade,brd)) HITS++;
	else
	{
		elem->brade=brd;
		elem->value=evaluate_mob(brd);
	}
	return elem->value;
}
#endif

static __inline__ int potential_mobility(board brd)
{
	bits64 black,white,empty,bpm=EMPTY,wpm=EMPTY,enbs=EMPTY;
	black=brd.black;
	white=brd.white;
	empty=NOT(OR(black,white));

#define POTMOBS(MASK,SHIFT,AMOUNT)\
	bpm=OR(bpm,SHIFT(AND(white,MASK),AMOUNT));\
	wpm=OR(wpm,SHIFT(AND(black,MASK),AMOUNT));\
	enbs=OR(enbs,SHIFT(AND(empty,MASK),AMOUNT));

#define POTMOBSNOMASK(SHIFT,AMOUNT)\
	bpm=OR(bpm,SHIFT(white,AMOUNT));\
	wpm=OR(wpm,SHIFT(black,AMOUNT));\
	enbs=OR(enbs,SHIFT(empty,AMOUNT));

	POTMOBS(LIT(7f7f7f7f,7f7f7f7f),shiftl,1)
	POTMOBS(LIT(fefefefe,fefefefe),shiftr,1)
	POTMOBS(LIT(fefefefe,fefefefe),shiftl,7)
	POTMOBS(LIT(7f7f7f7f,7f7f7f7f),shiftr,7)
	POTMOBS(LIT(7f7f7f7f,7f7f7f7f),shiftl,9)
	POTMOBS(LIT(fefefefe,fefefefe),shiftr,9)
	POTMOBSNOMASK(shiftl,8)
	POTMOBSNOMASK(shiftr,8)
	return 2*(count_bits64(AND(enbs,white))-count_bits64(AND(enbs,black))+count_bits64(AND(bpm,empty))-count_bits64(AND(wpm,empty)));
#undef POTMOBS
#undef POTMOBSNOMASK
}

STATIC void set_input(evaluation_t eval,int discs,int maxply,int ssdepth,bool winloss)
{
	if(eval!=EVAL)
	{
		EVAL=eval;
		if(CACHE)
		{
			memset(CACHE,0,NUMELEMS*sizeof(struct element)); /* invalidate all the cache elements */
		}
	}
	DISCS=discs;
	MAXPLY=maxply;
	SSDEPTH=MAXPLY+ssdepth;
	BADVALUE=(80-DISCS-MAXPLY)/4;
	WINLOSS=winloss;
	BESTMOVE=none;
	EARLYEXIT=false;
	ABCALLS=0;
	HITS=0;
	ACCESSES=0;
}

static int evaluate(board brd)
{
	return potential_mobility(brd)+corner_values(brd);
}

STATIC int alfabeta(board brd,int ply,int alfa,int beta,bool otherpassed,color_t tomove,int cornermove)
{
#if 0
	static const uint8_t corner[64]=
	{
		1, 1, 0, 0, 0, 0, 1, 1,
		1, 1, 0, 0, 0, 0, 1, 1,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 0, 0, 0, 0, 1, 1,
		1, 1, 0, 0, 0, 0, 1, 1
	};
#else
	bits64 corner = 0xC3C300000000C3C3ULL;
#endif
	int i,bestvalue=-INFINITY,Beta=beta,Alfa=alfa;
	move_t bestmove = none;
	uint8_t *moves=MOVES[tomove];
	ABCALLS++;
	for(i=0;i<NUMMOVES;i++)
	{
		move_t move=moves[i];
		matrix nbd=new_black_discs(brd,move);
		if(nbd)
		{
			board newbrd=swap_sides(flip_discs(brd,nbd));
#if 0
			int newcornermove=cornermove<<1|corner[move];
#else
			int newcornermove=cornermove<<1|((corner>>move)&1);
#endif
			int value;
			if(ply+1>=MAXPLY && !((newcornermove&3) && ply+1<SSDEPTH))
			{
				value=-evaluate(newbrd);
				ABCALLS++;
			}
			else
			{
				value=-alfabeta(newbrd,ply+1,-Beta,-Alfa,false,!tomove,newcornermove);
				if(value>Alfa && beta!=Beta)
				{
					value=-alfabeta(newbrd,ply+1,-beta,-Alfa,false,!tomove,newcornermove);
				}
			}
			if(value>bestvalue)
			{
				bestvalue=value;
				bestmove=move;
				if(bestvalue>Alfa)
				{
					Alfa=bestvalue;
					if(Alfa>=beta) break; /* alfabeta cutoff */
				}
				Beta=Alfa+1; /* new scout presearch window */
			}
		}
	} /* endfor */
	if(unlikely(bestvalue==-INFINITY)) /* we found no move, pass */
	{
		if(unlikely(otherpassed))
		{
			/* both colors passed */
			int dd=disc_difference(brd);
			if(dd<0) return -INFINITY+65+dd;
			else if(dd>0) return  INFINITY-65+dd;
			else return 0;
		}
		return -alfabeta(swap_sides(brd),ply,-beta,-alfa,true,!tomove,cornermove);
	}
	else
	{
		if(unlikely(ply==0 && !otherpassed)) /* only uppdate BESTMOVE if it really is our move */
		{
			BESTMOVE=bestmove; /* the real return value of alfabeta */
		}
		if(bestvalue>alfa) /* we've had an alfabeta cutoff */
		{
			SUCCESS[tomove][bestmove]++;
		}
	}
	return bestvalue;
}

STATIC int endgame_alfabeta(board brd,int ply,int alfa,int beta,bool otherpassed,color_t tomove)
{
	int i,bestvalue=-INFINITY,Beta=beta,Alfa=alfa;
	uint8_t *moves=MOVES[tomove];
	ABCALLS++;
	for(i=0;i<NUMMOVES;i++)
	{
		move_t move=moves[i];
		matrix nbd=new_black_discs(brd,move);
		if(nbd)
		{
			board newbrd=swap_sides(flip_discs(brd,nbd));
			int value;
			if(ply+1==MAXPLY)
			{
				value=-disc_difference2(newbrd,ply+1);
				ABCALLS++;
			}
			else
			{
				value=-endgame_alfabeta(newbrd,ply+1,-Beta,-Alfa,false,!tomove);
				if(value>Alfa && beta!=Beta)
				{
					value=-endgame_alfabeta(newbrd,ply+1,-beta,-Alfa,false,!tomove);
				}
			}
			if(value>bestvalue)
			{
				bestvalue=value;
				if(ply==0 && !otherpassed) /* only update BESTMOVE if it really is our move */
				{
					BESTMOVE=move; /* the real return value of alfabeta */
				}
				if(bestvalue>Alfa)
				{
					Alfa=bestvalue;
					if(Alfa>=beta) return bestvalue; /* alfabeta cutoff */
				}
				Beta=Alfa+1; /* new scout presearch window */
			}
		}
	} /* endfor */
	if(bestvalue==-INFINITY) /* we found no move, pass */
	{
		if(otherpassed)
		{
			/* both passed */
			return disc_difference2(brd,ply);
		}
		return -endgame_alfabeta(swap_sides(brd),ply,-beta,-alfa,true,!tomove);
	}
	return bestvalue;
}

static __inline__ char *wltext(int value)
{
	if(value<0) return "loss";
	else if(value>0) return "win";
	else return "drawn";
}

static int _earlyeg_alfabeta(board brd,int ply,int alfa,int beta,bool otherpassed,color_t tomove)
{
	int i,bestvalue=-INFINITY,Beta=beta,Alfa=alfa;
	uint8_t *moves=MOVES[tomove];
	ABCALLS++;
	for(i=0;i<NUMMOVES;i++)
	{
		move_t move=moves[i];
		matrix nbd=new_black_discs(brd,move);
		if(nbd)
		{
			board newbrd=swap_sides(flip_discs(brd,nbd));
			int value;
			if(ply+1==MAXPLY)
			{
				int dd=disc_difference2(newbrd,ply+1);
				if(dd<0) value=1;
				else if(dd>0) value=-1;
				else value=0;
				ABCALLS++;
			}
			else
			{
				value=-_earlyeg_alfabeta(newbrd,ply+1,-Beta,-Alfa,false,!tomove);
				if(value>Alfa && beta!=Beta && value<1) /* if value==1 then it can't get any better */
				{
					value=-_earlyeg_alfabeta(newbrd,ply+1,-beta,-Alfa,false,!tomove);
				}
			}
			if(value>bestvalue)
			{
				bestvalue=value;
				if(ply==0 && !otherpassed) /* only update BESTMOVE if it really is our move */
				{
					BESTMOVE=move; /* the real return value of alfabeta */
				}
				if(bestvalue>Alfa)
				{
					Alfa=bestvalue;
					if(Alfa>=beta) return bestvalue; /* immediate cutoff */
				}
				Beta=Alfa+1;
			}
		}
	} /* endfor */
	if(bestvalue==-INFINITY) /* we found no move, pass */
	{
		if(otherpassed)
		{
			/* both passed */
			int dd=disc_difference2(brd,ply);
			if(dd<0) return -1;
			else if(dd>0) return 1;
			else return 0;
		}
		return -_earlyeg_alfabeta(swap_sides(brd),ply,-beta,-alfa,true,!tomove);
	}
	return bestvalue;
}

STATIC int earlyeg_alfabeta(board brd,color_t tomove,unsigned target)
{
	unsigned long start=cpu_time();
	const int alfa=-INFINITY,beta=INFINITY;
#if 0
	int i,bestvalue=-INFINITY,Beta=beta;
#else
	int i,bestvalue=-1,Beta=0; /* as if one losing move was already found */
#endif
	EARLYEXIT=false;
	ABCALLS++;
	printf("move  value  nodes  time  nodes/s\n");
	for(i=0;i<NUMMOVES;i++)
	{
		move_t move=MOVES[tomove][i];
		matrix nbd=new_black_discs(brd,move);
		if(nbd)
		{
			if(cpu_time()-start>target) /* we have a legal move to search, but no time */
			{
				EARLYEXIT=true;
				return bestvalue;
			}
			else
			{
				board newbrd=swap_sides(flip_discs(brd,nbd));
				unsigned long elapsed,abcalls=ABCALLS;
				int value;
				printf(" %c%c ",move%8+'a',move/8+'1'); fflush(stdout);
				elapsed=cpu_time();
				value=-_earlyeg_alfabeta(newbrd,1,-Beta,-bestvalue,false,!tomove); /* send out scout with minimal window */
				if(value>bestvalue && beta!=Beta) value=-_earlyeg_alfabeta(newbrd,1,-beta,-bestvalue,false,!tomove); /* scout failed, big search */
				elapsed=cpu_time()-elapsed;
				abcalls=ABCALLS-abcalls;
				if(elapsed) printf(" %5.5s %7lu %3lu.%02lu %5lu\n",wltext(value),abcalls,elapsed/100,elapsed%100,100*abcalls/elapsed);
				else        printf(" %5.5s %7lu  ~0\n",wltext(value),abcalls);
				if(value>bestvalue)
				{
					BESTMOVE=move;
					if(value>0) return value; /* one winning move is all we need */
					bestvalue=value;
					Beta=bestvalue+1; /* update minimal window */
				}
			}
		}
	} /* endfor */
	if(bestvalue==-INFINITY) /* we have no legal moves, pass */
	{
		return -_earlyeg_alfabeta(swap_sides(brd),0,-beta,-alfa,true,!tomove);
	}
	return bestvalue;
}
