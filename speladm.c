/*
 * Copyright 1993 Ola Liljedahl
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "speladm.h"
#include "position.h"
#include "funkmod.h"
#include "times.h"

static int max(int a,int b)
{
	return a>b ? a : b ;
}

static int min(int a,int b)
{
	return a<b ? a : b ;
}

#define BOLD "[1m"
#define NORMAL "[0m"

static void print_board_and_moves(board brade,matrix moves,color_t col)
{
	int x,y;
	printf("    a b c d e f g h\n");
	printf("   -----------------\n");
	for(y=0;y<8;y++)
	{
		printf("%d | ",y+1);
		for(x=0;x<8;x++)
		{
			int bit=8*y+x;
			if(GETBIT(brade.black,bit))
			{
				putchar('X');
			}
			else if(GETBIT(brade.white,bit))
			{
				putchar('O');
			}
			else if(GETBIT(moves,bit))
			{
				printf(BOLD"%c"NORMAL,col==black?'x':'o');
			}
			else
			{
				putchar('.');
			}
			putchar(' ');
		}
		printf("| %d\n",y+1);
	}
	printf("   -----------------\n");
	printf("    a b c d e f g h\n");
}

static void print_board(board brade,move_t move)
{
	int x,y;
	printf("    a b c d e f g h\n");
	printf("   -----------------\n");
	for(y=0;y<8;y++)
	{
		printf("%d | ",y+1);
		for(x=0;x<8;x++)
		{
			unsigned bit=8*y+x;
			if(bit==move)
			{
				fputs(BOLD,stdout);
			}
			if(GETBIT(brade.black,bit))
			{
				putchar('X');
			}
			else if(GETBIT(brade.white,bit))
			{
				putchar('O');
			}
			else
			{
				putchar('.');
			}
			if(bit==move)
			{
				fputs(NORMAL,stdout);
			}
			putchar(' ');
		}
		printf("| %d\n",y+1);
	}
	printf("   -----------------\n");
	printf("    a b c d e f g h\n");
}

static int pipein=-1;
static int pipeout=-1;
char *PIPEIN;
char *PIPEOUT;

static void writemove(move_t move)
{
	char buf[4];
	if(move==pass)
	{
		buf[0]='p';
		buf[1]='s';
		buf[2]='\n';
		buf[3]='\0';
	}
	else
	{
		buf[0]=(char)(move%8+'a');
		buf[1]=(char)(move/8+'1');
		buf[2]='\n';
		buf[3]='\0';
	}
	if (pipeout == -1)
	{
		pipeout=open(PIPEOUT,O_WRONLY | O_APPEND | O_CREAT | O_TRUNC | O_EXCL, S_IRUSR | S_IWUSR);
		if(pipeout == -1)
		{
			fprintf(stderr,"error opening pipe \"%s\" errno %u %s\n",PIPEOUT,errno,strerror(errno));
			exit(20); /* ok */
		}
	}
	if(write(pipeout,buf,strlen(buf))<0)
	{
		fprintf(stderr,"error writing pipe \"%s\" errno %u %s\n",PIPEOUT,errno,strerror(errno));
		exit(20); /* ok */
	}
}

static move_t readmove(matrix lm)
{
	while(pipein==-1)
	{
		pipein=open(PIPEIN,O_RDONLY,0);
		if(pipein==-1 && errno!=ENOENT)
		{
			fprintf(stderr,"error opening pipe \"%s\" errno %u %s\n",PIPEIN,errno,strerror(errno));
			exit(20); /* ok */
		}
		sleep(1);
	}
	{
		char buf[4];
		int row,col,move,len;
		for(;;)
		{
			len=read(pipein,buf,3);
			if(len>0) break;
			if(len<0)
			{
				fprintf(stderr,"error reading pipe \"%s\" errno %u %s\n",PIPEIN,errno,strerror(errno));
				exit(20); /* ok */
			}
			sleep(1);
		}
		buf[len]=0;
		if(strcmp(buf,"ps")==0)
		{
			if(count_matrix(lm)==0) return pass;
			fprintf(stderr,"error: invalid pass\n");
			exit(20); /* ok */
		}
		col=buf[0]-'a';
		row=buf[1]-'1';
		if(col<0 || col>7 || row<0 || row>7)
		{
			fprintf(stderr,"garbage %s\n",buf);
			return readmove(lm);
		}
		move=8*row+col;
		if(GETBIT(lm,move)==0)
		{
			fprintf(stderr,"error: invalid move %s\n",buf);
			exit(20); /* ok */
		}
		return move;
	}
}

static char *find_argument(char *str,char **ptr)
{
	while(*str && isspace(*str)) str++;
	if(*str)
	{
		*ptr=str;
		while(*str && !isspace(*str)) str++;
		if(*str)
		{
			*str=0; /* mark end of argument */
			return str+1; /* may be more arguments following */
		}
		/* else found null character */
		return str; /* no more arguments */
	}
	*ptr=0; /* no argument found */
	return str; /* no more arguments */
}

static board play_move(board brd,move_t move,color_t tomove)
{
	if(move<64)
	{
		if(tomove!=black)
		{
			brd=swap_sides(brd);
		}
		brd=flip_discs(brd,new_black_discs(brd,move));
		if(tomove!=black)
		{
			brd=swap_sides(brd);
		}
	}
	return brd;
}

/*
	user wants to play from move frommove+1 (in the range [1..60])
	we must replay all moves in the range [1..(frommove+1)-1],
	[0..frommove-1] in internal coodinates
*/

void replay_moves(position_t *pos,int frommove)
{
	int i,validmoves=0;
	for(i=0;i<pos->nummoves;i++)
	{
		if(pos->moves[i]<64) validmoves++;
		else if(pos->moves[i]!=pass) break;
	}
	if(count_matrix(pos->brade.black)+count_matrix(pos->brade.white)-4==validmoves)
	{
		int movenr;
		pos->brade.black = 1<<28|1ULL<<35;
		pos->brade.white = 1<<27|1ULL<<36;
		pos->humantime=0;
		pos->computertime=0;
		pos->turntomove=black;
		pos->passes=0;
		if(frommove>pos->nummoves)
		{
			printf("Warning: position only has %d move%s\n",pos->nummoves,pos->nummoves==1?"":"s");
			frommove=pos->nummoves;
		}
		for(movenr=0;movenr<frommove;movenr++)
		{
			move_t move = pos->moves[movenr];
			if(move==pass)
			{
				pos->passes++;
			}
			else /* valid move 0..63 */
			{
				pos->brade=play_move(pos->brade,move,pos->turntomove);
				pos->passes=0;
			}
			if(pos->turntomove==pos->computer)
			{
				pos->computertime += pos->times[movenr];
			}
			else
			{
				pos->humantime += pos->times[movenr];
			}
			pos->turntomove=!pos->turntomove;
		}
		pos->nummoves=movenr;
	}
	else
	{
		printf("Warning: inconsistent position, will not replay moves\n");
	}
}

static void print_info(position_t *pos,move_t lastmove)
{
	print_board(pos->brade,lastmove);
	printf("             discs   time\n");
	printf("   human (%c):  %2d   %2d:%02d\n",pos->computer==white?'X':'O',count_matrix(pos->computer==white?pos->brade.black:pos->brade.white),pos->humantime/60,pos->humantime%60);
	printf("computer (%c):  %2d   %2d:%02d\n",pos->computer==black?'X':'O',count_matrix(pos->computer==black?pos->brade.black:pos->brade.white),pos->computertime/60,pos->computertime%60);
}

/*
	return a matrix with legal moves
*/

static matrix legal_moves(board brd,color_t tomove)
{
	int i;
	matrix m=0;
	if(tomove!=black) brd=swap_sides(brd);
	for(i=0;i<64;i++)
	{
		if(legal_move(brd,i)) SETBIT(m,i);
	}
	return m;
}

/*
	read a valid move or a command from the user
	if he types only a newline execute default action
	(ie show legal moves, return forced more or return pass)
*/

static move_t enter_move(position_t *pos,matrix legalmoves)
{
	for(;;)
	{
		int len;
		char buf[80];
		printf("Enter your move: "); fflush(stdout);
		if (fgets(buf,sizeof buf,stdin)==NULL)
		{
			return quit;
		}
		len=strlen(buf);
		while(len>0&&buf[len-1]<=' ') len--;
		buf[len]='\0';
		if(strcmp(buf,"quit")==0) return quit;
		else if(strcmp(buf,"?")==0)
		{
			printf("Available commands: quit, undo [numofmoves], save [filename],\nload [filename [frommove]], help [targettime]\n");
		}
		else if(strncmp(buf,"undo",4)==0)
		{
			int undo=strlen(buf)>5?atoi(buf+5):2;
			if(undo>0 && pos->nummoves-undo>=0)
			{
				replay_moves(pos,pos->nummoves-undo);
				return load;
			}
		}
		else if(strncmp(buf,"save",4)==0)
		{
			save_position(strlen(buf)>5?buf+5:0,pos);
		}
		else if(strncmp(buf,"load",4)==0)
		{
			position_t save=*pos;
			char *filename,*frommove;
			find_argument(find_argument(buf+5,&filename),&frommove);
			if(load_position(filename,pos,false))
			{
				if(frommove) replay_moves(pos,atoi(frommove)-1);
				return load;
			}
			*pos=save;
		}
		else if(strncmp(buf,"help",4)==0)
		{
			int target=strlen(buf)>5?atoi(buf+5):10;
			evaluation_t eval=pos->eval[count_matrix(pos->brade.black)+count_matrix(pos->brade.white)-4];
			result_t r;
			if(eval==winloss || eval==discdiff) r=endgame_search(pos->brade,!pos->computer,100*target,eval);
			else r=iterative_deepening(pos->brade,!pos->computer,100*target,0,normal,eval,!pos->deterministic);
			printf("I suggest %c%c value %d\n",r.bestmove%8+'a',r.bestmove/8+'1',r.bestvalue);
		}
		else if(strlen(buf)==0)
		{
			switch(count_matrix(legalmoves))
			{
				case 0 :
					printf("You pass\n");
					return pass;
				case 1 :
				{
					move_t move=first_bit_m(legalmoves);
					printf("You play "BOLD"%c%c"NORMAL"\n",move%8+'a',move/8+'1');
					return move;
				}
				default :
					printf("These are your legal moves\n");
					print_board_and_moves(pos->brade,legalmoves,!pos->computer);
			}
		}
		else if(strcmp(buf,"pass")==0 || strcmp(buf,"ps")==0)
		{
			if(count_matrix(legalmoves)==0) return pass;
			printf("You have legal moves, you cannot pass\n");
		}
		else if(strlen(buf)==2)
		{
			int letter=buf[0];
			int digit=buf[1];
			if(letter>='A' && letter<='H') letter-='A';
			if(letter>='a' && letter<='h') letter-='a';
			if(digit>='1' && digit<='8') digit-='1';
			if(letter>=0 && letter<8 && digit>=0 && digit<8)
			{
				move_t move=8*digit+letter;
				if(GETBIT(legalmoves,move)) return move;
				printf("%s is not a legal move\n",buf);
			}
		}
	}
}

static move_t human_move(position_t *pos)
{
	matrix lm=legal_moves(pos->brade,!pos->computer);
	switch(count_matrix(lm))
	{
		case 0 :
			printf("You have no legal moves, enter "BOLD"ps"NORMAL" to pass\n");
			break;
		case 1 :
		{
			move_t move=first_bit_m(lm);
			printf("You have a forced move to "BOLD"%c%c"NORMAL"\n",move%8+'a',move/8+'1');
			break;
		}
	}
	return PIPEIN ? readmove(lm) : enter_move(pos,lm) ;
}

static move_t computer_move(position_t *pos)
{
	matrix lm=legal_moves(pos->brade,pos->computer);
	int nummoves=count_matrix(lm);
	move_t move;
	if(nummoves)
	{
		if(nummoves>1)
		{
			int index=count_matrix(pos->brade.black)+count_matrix(pos->brade.white)-4;
			result_t r;
			if(pos->eval[index]==discdiff)
			{
				int target=2*(pos->targettime-pos->computertime)/3;
				r=endgame_search(pos->brade,pos->computer,100*target,discdiff);
			}
			else if(pos->eval[index]==winloss)
			{
				result_t mgr;
				int target=pos->targettime/120,elapsed1,elapsed2;
				unsigned long start=cpu_time();
				mgr=iterative_deepening(pos->brade,pos->computer,100*target,0,secondary,potmobility,!pos->deterministic);
				elapsed1=(cpu_time()-start)/100;
				target=2*(pos->targettime-pos->computertime-elapsed1)/3;
				start=cpu_time();
				r=endgame_search(pos->brade,pos->computer,100*target,winloss);
				elapsed2=(cpu_time()-start)/100;
				if(r.earlyexit) /* invalid result */
				{
					r=mgr; /* choose middle game move */
				}
				else if(10*elapsed2<target-elapsed1-elapsed2 /* what is left of our target time */) /* valid result, plenty of time to search for best move */
				{
					target=2*(pos->targettime-pos->computertime-elapsed1-elapsed2 /* what is left of our total time */)/3;
					r=endgame_search(pos->brade,pos->computer,100*target,discdiff);
				}
				else if(r.bestvalue<0) /* all losses, no time to search for best */
				{
					r=mgr; /* choose middle game move */
				}
				/* else choose winning (or drawn) move */
			}
#if 0
			else if(index>=5)
#else
			else
#endif
			{
				int target=pos->targettime/60;
				int extra=0;
				evaluation_t prev=pos->eval[index-1];
				evaluation_t thys=pos->eval[index];
				if(prev!=thys || target*(index-2)>pos->computertime)
				{
					if(prev!=thys || (rand()&3)==0) target+=target;
					else extra=target;
				}
				r=iterative_deepening(pos->brade,pos->computer,100*target,100*extra,secondary,thys,!pos->deterministic);
			}
			move=r.bestmove;
			if(move>63)
			{
				fprintf(stderr,"error: search returned no valid move\n");
				return quit;
			}
			printf("I play "BOLD"%c%c"NORMAL"\n",move%8+'a',move/8+'1');
		}
		else
		{
			move=first_bit_m(lm);
			printf("I have a forced move to "BOLD"%c%c"NORMAL"\n",move%8+'a',move/8+'1');
		}
	}
	else
	{
		move=pass;
		printf("I have no legal moves, I "BOLD"pass"NORMAL"\n");
	}
	if(PIPEOUT) writemove(move);
	return move;
}

bool play_game(position_t *pos)
{
	bool success;
	print_info(pos,pos->nummoves ? pos->moves[pos->nummoves-1] : pass);
	init_move_order(pos->brade);

	for(;;)
	{
		int elapsed;
		move_t move;

		int your_discs=count_matrix(pos->computer==black?pos->brade.white:pos->brade.black);
		int my_discs=count_matrix(pos->computer==white?pos->brade.white:pos->brade.black);
		if(your_discs+my_discs==64 || your_discs==0 || my_discs==0 || pos->passes==2)
		{
			if(pos->passes==2) printf("Double pass, ");
			else if(your_discs==0) printf("You suffered a wipeout, ");
			else if(my_discs==0) printf("I suffered a wipeout, ");
			else printf("Game over, ");
			if(your_discs>my_discs) printf("you win");
			else if(your_discs<my_discs) printf("I win");
			else printf("equal");
			printf(" %d-%d\n",max(your_discs,my_discs),min(your_discs,my_discs));
			success=true;
			break;
		}

		if (pos->turntomove==pos->computer)
		{
			unsigned long start=cpu_time();
			move=computer_move(pos);
			elapsed=(cpu_time()-start)/100;
		}
		else
		{
			unsigned long start=real_time();
			move=human_move(pos) ;
			elapsed=(real_time()-start)/100;
		}

		if(move==quit)
		{
			success=false;
			break;
		}
		else if(move==load)
		{
			move = pos->nummoves ? pos->moves[pos->nummoves-1] : pass ;
			print_info(pos,move);
			init_move_order(pos->brade);
		}
		else /* valid move 0..63 or pass */
		{
			pos->moves[pos->nummoves]=move;
			pos->times[pos->nummoves]=(short)elapsed;
			pos->nummoves++;
			if(pos->turntomove==pos->computer)
			{
				pos->computertime += elapsed;
			}
			else
			{
				pos->humantime += elapsed;
			}
			if(move==pass)
			{
				pos->passes++;
			}
			else
			{
				pos->passes=0;
				pos->brade=play_move(pos->brade,move,pos->turntomove);
				print_info(pos,move);
			}
			pos->turntomove=!pos->turntomove;
		}
	}
	if(pipein!=-1) close(pipein);
	if(PIPEIN!=NULL) remove(PIPEIN);
	if(pipeout!=-1) close(pipeout);
	if(PIPEOUT!=NULL) remove(PIPEOUT);
	return success;
}
