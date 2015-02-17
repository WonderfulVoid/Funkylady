/*
 * Copyright 1993 Ola Liljedahl
 */

#include "normalize.h"

#define MOVE(r,c) (8*(r)+(c))
#define SWAP(a,b) { int _t_=a; a=b; b=_t_; }

STATIC move_t transform_move(move_t move,int tf)
{
	int row=move/8;
	int col=move%8;
	if(tf&4) SWAP(row,col);
	switch(tf&3)
	{
		case 0 : return MOVE(row,col);
		case 1 : return MOVE(col,7-row);
		case 2 : return MOVE(7-row,7-col);
		case 3 : return MOVE(7-col,row);
	}
	return 0; /* NOTREACHED */
}

STATIC move_t inverse_transform_move(move_t move,int tf)
{
	int y=move/8;
	int x=move%8;
	int row,col;
	switch(tf&3)
	{
		case 0 :
			row=y;
			col=x;
			break;
		case 1 :
			row=7-x;
			col=y;
			break;
		case 2 :
			row=7-y;
			col=7-x;
			break;
		case 3 :
			row=x;
			col=7-y;
			break;
	}
	if(tf&4) SWAP(row,col);
	return MOVE(row,col);
}

STATIC int normalize_board(board *pos)
{
	board b[8];				/* all 8 transformed bitboards */
	int mintf,tf,row,col;

	b[0].black=b[0].white=0;
	b[4].black=b[4].white=0;
	for(row=0;row<8;row++)
	{
		for(col=0;col<8;col++)
		{
			int pos0=8*row+col;
			int pos4=8*col+row;
			if(GETBIT(pos->black,pos0))
			{
				SETBIT(b[0].black,pos0);
				SETBIT(b[4].black,pos4);
			}
			else if(GETBIT(pos->white,pos0))
			{
				SETBIT(b[0].white,pos0);
				SETBIT(b[4].white,pos4);
			}
		}
	}

	/* rotate 90—, 180— och 270— */
	for(tf=0;tf<8;tf+=4)
	{
		b[tf+1].black=b[tf+1].white=0;
		b[tf+2].black=b[tf+2].white=0;
		b[tf+3].black=b[tf+3].white=0;
		for(row=0;row<8;row++)
		{
			for(col=0;col<8;col++)
			{
				int pos=8*row+col;
				if(GETBIT(b[tf].black,pos))
				{
					SETBIT(b[tf+1].black,MOVE(  col,7-row));	/* rotate 90— */
					SETBIT(b[tf+2].black,MOVE(7-row,7-col));	/* rotate 180— */
					SETBIT(b[tf+3].black,MOVE(7-col,  row));	/* rotate 270— */
				}
				else if(GETBIT(b[tf].white,pos))
				{
					SETBIT(b[tf+1].white,MOVE(  col,7-row));	/* rotate 90— */
					SETBIT(b[tf+2].white,MOVE(7-row,7-col));	/* rotate 180— */
					SETBIT(b[tf+3].white,MOVE(7-col,  row));	/* rotate 270— */
				}
			}
		}
	}

	/* find smallest transformed bitboard */
	mintf=0;
	for(tf=1;tf<8;tf++)
	{
		int diff=LOWER(b[tf].black)-LOWER(b[mintf].black);
		if(diff<0) mintf=tf;
		else if(diff==0)
		{
			int diff=UPPER(b[tf].black)-UPPER(b[mintf].black);
			if(diff<0) mintf=tf;
			else if(diff==0)
			{
				int diff=LOWER(b[tf].white)-LOWER(b[mintf].white);
				if(diff<0) mintf=tf;
				else if(diff==0)
				{
					int diff=UPPER(b[tf].white)-UPPER(b[mintf].white);
					if(diff<0) mintf=tf;
				}
			}
		}
	}

	/* return transformed board and transform used */
	*pos=b[mintf];
	return mintf;
}
