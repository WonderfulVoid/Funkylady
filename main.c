/*
 * Copyright 1993 Ola Liljedahl
 */

#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "position.h"
#include "speladm.h"
#include "alfabeta.h"
#include "funkmod.h"
#include "times.h"

static const char *PIPEIN;
static const char *PIPEOUT;

static void dump_position(position_t *pos)
{
	char filename[40];
	sprintf(filename,"dump.%lu",real_time());
	save_position(filename,pos);
}

static void create_pipe(const char *name)
{
	int rc = mkfifo(name,S_IRUSR|S_IWUSR);
	if (rc != 0 && errno != EEXIST)
	{
		fprintf(stderr,"error creating fifo \"%s\" errno %u %s\n",
				name,errno,strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static int open_pipe(const char *name, int flags)
{
	int fd=open(name,flags,0);
	if(fd<0)
	{
		fprintf(stderr,"error opening fifo \"%s\" errno %u %s\n",
				name,errno,strerror(errno));
		exit(EXIT_FAILURE);
	}
	return fd;
}

static void open_pipes(const char *input, const char *output)
{
	create_pipe(input);
	create_pipe(output);
	/* Ensure peers open pipes in matching order to avoid deadlock */
	if (strcmp(input,output) < 0)
	{
		pipein=open_pipe(input,O_RDONLY);
		pipeout=open_pipe(output,O_WRONLY);
	}
	else
	{
		pipeout=open_pipe(output,O_WRONLY);
		pipein=open_pipe(input,O_RDONLY);
	}
}

static bool parse_cmdline(int argc,char *argv[],position_t *pos,int *frommove,bool *dump)
{
	int i;
	for(i=1;i<argc;)
	{
		if(strcmp(argv[i],"?")==0)
		{
			fprintf(stderr,"usage: %s [-ccolor black/white] [-ttime minutes] [-load filename] [-from movenr] [-cachesize numelems] [-deterministic true/false] [-opponent name] [-nodump] [-pipes pipein pipeout]\n",argv[0]);
			return false;
		}
		else if(strcmp(argv[i],"-ccolor")==0)
		{
			if(i+1<argc)
			{
				if(strcmp(argv[i+1],"black")==0) pos->computer=black;
				else if(strcmp(argv[i+1],"white")==0) pos->computer=white;
				else
				{
					fprintf(stderr,"error: invalid computer color %s\n",argv[i+1]);
					return false;
				}
			}
			else
			{
				fprintf(stderr,"error: missing parameter to %s\n",argv[i]);
				return false;
			}
			i+=2;
		}
		else if(strcmp(argv[i],"-ttime")==0)
		{
			if(i+1<argc)
			{
				char *endptr = NULL;
				pos->targettime=60*strtol(argv[i+1],&endptr,0);
				if(*endptr)
				{
					fprintf(stderr,"error: invalid target time %s\n",argv[i+1]);
					return false;
				}
			}
			else
			{
				fprintf(stderr,"error: missing parameter to %s\n",argv[i]);
				return false;
			}
			i+=2;
		}
		else if(strcmp(argv[i],"-load")==0)
		{
			if(i+1<argc)
			{
				if(!load_position(argv[i+1],pos,false)) return false;
			}
			else
			{
				fprintf(stderr,"error: missing parameter to %s\n",argv[i]);
				return false;
			}
			i+=2;
		}
		else if(strcmp(argv[i],"-from")==0)
		{
			if(i+1<argc)
			{
				char *endptr = NULL;
				*frommove=strtol(argv[i+1],&endptr,0)-1;
				if(*endptr || *frommove<0)
				{
					fprintf(stderr,"error: invalid from movenr %s\n",argv[i+1]);
					return false;
				}
			}
			else
			{
				fprintf(stderr,"error: missing parameter to %s\n",argv[i]);
				return false;
			}
			i+=2;
		}
		else if(strcmp(argv[i],"-cachesize")==0)
		{
			if(i+1<argc)
			{
				char *endptr = NULL;
				pos->cachesize=strtol(argv[i+1],&endptr,0);
				if(*endptr)
				{
					fprintf(stderr,"error: invalid cache size %s\n",argv[i+1]);
					return false;
				}
			}
			else
			{
				fprintf(stderr,"error: missing parameter to %s\n",argv[i]);
				return false;
			}
			i+=2;
		}
		else if(strcmp(argv[i],"-deterministic")==0)
		{
			if(i+1<argc)
			{
				if(strcmp(argv[i+1],"true")==0) pos->deterministic=true;
				else if(strcmp(argv[i+1],"false")==0) pos->deterministic=false;
				else
				{
					fprintf(stderr,"error: invalid truth value %s\n",argv[i+1]);
					return false;
				}
			}
			else
			{	
				fprintf(stderr,"error: missing parameter to %s\n",argv[i]);
				return false;
			}
			i+=2;
		}
		else if(strcmp(argv[i],"-opponent")==0)
		{
			if(i+1<argc)
			{
				strncpy(pos->humanname,argv[i+1],sizeof pos->humanname);
				pos->humanname[sizeof pos->humanname-1]=0;
				if(strlen(argv[i+1])>sizeof pos->humanname-1)
				{
					fprintf(stderr,"warning: opponent name '%s' too long, truncated to %zu chars\n",argv[i+1],sizeof pos->humanname-1);
				}
			}
			else
			{
				fprintf(stderr,"error: missing parameter to %s\n",argv[i]);
				return false;
			}
			i+=2;
		}
		else if(strcmp(argv[i],"-nodump")==0)
		{
			*dump=false;
			i++;
		}
		else if(strcmp(argv[i],"-pipes")==0)
		{
			if(i+2<argc)
			{
				PIPEIN=argv[i+1];
				PIPEOUT=argv[i+2];
				open_pipes(PIPEIN, PIPEOUT);
			}
			else
			{
				fprintf(stderr,"error: missing parameters to %s\n",argv[i]);
				return false;
			}
			i+=3;
		}
		else
		{
			fprintf(stderr,"error: unrecognized option %s\n",argv[i]);
			return false;
		}
	}
	return true;
}

int main(int argc,char *argv[])
{
	int ret=20;
	position_t pos;
	int frommove=-1;
	bool dump=true;
	printf("Welcome to Funkylady, the Othello contestor\n"
		   "Copyright 1993 Ola Liljedahl, all rights reserved\n");
	load_position("funky.param",&pos,true); /* initialize and load default evaluation parameters */
	if(parse_cmdline(argc,argv,&pos,&frommove,&dump) && init_tables(pos.cachesize))
	{
		srand(real_time());
		if(!pos.deterministic) srand(real_time());
		if(frommove>=0) replay_moves(&pos,frommove);
		if(play_game(&pos) && dump) dump_position(&pos);
		ret = 0;
	}
	/* We are lax with error checks when cleaning up before exit */
	if (pipein != -1)
	{
		(void)close(pipein);
	}
	if (pipeout != -1)
	{
		(void)close(pipeout);
	}
	if (PIPEIN != NULL)
	{
		(void)remove(PIPEIN);
	}
	if (PIPEOUT != NULL)
	{
		(void)remove(PIPEIN);
	}
	return ret;
}
