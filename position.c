/*
 * Copyright 1993 Ola Liljedahl
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "stdio.h"
#include "errno.h"
#include "position.h"
#include "times.h"

#define COMPUTERNAME "funkylady"

static void write_board(FILE *file,board brade)
{
	int row,col;
	for(row=0;row<8;row++)
	{
		for(col=0;col<8;col++)
		{
			char c='.';
			if(GETBIT(brade.black,8*row+col)) c='x';
			else if(GETBIT(brade.white,8*row+col)) c='o';
			fputc(c,file);
		}
		fputc('\n',file);
	}
}

STATIC bool save_position(char *filename,position_t *pos)
{
	for(;;)
	{
		char fname[128];
		FILE *file;
		if(!filename)
		{
			int len;
			printf("Enter filename: "); fflush(stdout);
			if(!fgets(fname,sizeof fname,stdin)) return false; /* end-of-file or error */
			/* remove stupid newline from end of string */
			len=strlen(fname);
			if(len>0 && fname[len-1]=='\n') fname[--len]=0;
			if(len==0) return false;
			filename=fname;
		}
		file=fopen(filename,"w");
		if(file!=NULL)
		{
			int i;
			fprintf(file,"board\n");
			write_board(file,pos->brade);
			fprintf(file,"timestamp %lu\n",pos->timestamp);
			fprintf(file,"computercolor %s\n",pos->computer==black?"black":"white");
			fprintf(file,"humantime %d\n",pos->humantime);
			fprintf(file,"computertime %d\n",pos->computertime);
			fprintf(file,"targettime %d\n",pos->targettime);
			fprintf(file,"turntomove %s\n",pos->turntomove==black?"black":"white");
			fprintf(file,"passes %d\n",pos->passes);
			fprintf(file,"cachesize %lu\n",pos->cachesize);
			fprintf(file,"deterministic %s\n",pos->deterministic?"true":"false");
			if(strlen(pos->computername)) fprintf(file,"computername %s\n",pos->computername);
			if(strlen(pos->humanname)) fprintf(file,"humanname %s\n",pos->humanname);
			fprintf(file,"moves ");
			for(i=0;i<pos->nummoves;i++)
			{
				if(pos->moves[i]<64) fprintf(file,"%c%c ",pos->moves[i]%8+'a',pos->moves[i]/8+'1');
				else fprintf(file,"ps ");
			}
			fprintf(file,"\n");
			fprintf(file,"times ");
			for(i=0;i<pos->nummoves;i++)
			{
				fprintf(file,"%d ",pos->times[i]);
			}
			fprintf(file,"\n");
			{
				evaluation_t eval=dummyeval;
				int i=0;
				for(;;)
				{
					const char *str = "";
					while(pos->eval[i]==eval && i<60) i++;
					if(i==60) break;
					eval=pos->eval[i];
					switch(eval)
					{
						case discdiff : str="discdiff";
										break;
						case winloss : str="winloss";
										break;
						case mobility : str="mobility";
										break;
						case potmobility : str="potmobility";
										break;
					        case dummyeval: 
                                                                                break;
					}
					fprintf(file,"%s %d\n",str,i+1);
				}
			}
			if(fclose(file)==EOF)
			{
				fprintf(stderr,"errno %d writing file %s\n",errno,filename);
				return false;
			}
			return true;
		}
		else fprintf(stderr,"errno %d opening file %s\n",errno,filename);
		filename=0;
	}
}

static bool read_board(FILE *file,board *brade)
{
	int row,col;
	brade->black=0;
	brade->white=0;
	for(row=0;row<8;row++)
	{
		for(col=0;col<8;col++)
		{
			int c=fgetc(file);
			switch(c)
			{
				case 'x' :
				case 'X' :
					SETBIT(brade->black,8*row+col)
					break;
				case 'o' :
				case 'O' :
					SETBIT(brade->white,8*row+col)
					break;
				case '.' :
				case ' ' :
					break;
				case EOF :
					fprintf(stderr,"errno %d reading board\n",errno);
					return false;
				default :
					fprintf(stderr,"error: invalid character %c in board\n",c);
					return false;
			}
		}
		fgetc(file); /* consume eol */
	}
	return true;
}

STATIC bool load_position(char *filename,position_t *pos,bool parameters)
{
	int i=0;
	pos->timestamp=real_time();
	pos->brade.black = 1<<28|1ULL<<35;
	pos->brade.white = 1<<27|1ULL<<36;
	pos->humantime=0;
	pos->computertime=0;
	pos->turntomove=black;
	pos->passes=0;
	pos->nummoves=0;
	pos->targettime=20*60; /* 20 minutes, 60 secs/min */
	pos->computer=white;
	while(i<44) pos->eval[i++]=potmobility;
	while(i<48) pos->eval[i++]=winloss;
	while(i<60) pos->eval[i++]=discdiff;
	pos->cachesize=0;
	pos->deterministic=false;
	strncpy(pos->computername,COMPUTERNAME,sizeof pos->computername);
	pos->computername[sizeof pos->computername-1]=0;
	memset(pos->humanname,0,sizeof pos->humanname);
	for(;;)
	{
		bool success=false;
		char fname[128];
		FILE *file;
		if(!filename)
		{
			int len;
			printf("Enter filename: "); fflush(stdout);
			if(!fgets(fname,sizeof fname,stdin)) return false; /* end-of-file or error */
			/* remove stupid newline from end of string */
			len=strlen(fname);
			if(len>0 && fname[len-1]=='\n') fname[--len]=0;
			if(len==0) return false;
			filename=fname;
		}
		file=fopen(filename,"r");
		if(file!=NULL)
		{
			for(;;)
			{
				int len;
				char buf[256];
				/* Not enough to cast return value to (void), must actually
				 * assign it in order to avoid warning */
				char *rc = fgets(buf,sizeof buf,file);
				(void)rc;
				if(ferror(file))
				{
					fprintf(stderr,"error: could not read %sfile %s errno %d\n",parameters?"parameter ":"",filename,errno);
					break;
				}
				else if(feof(file))
				{
					success=true;
					break;
				}
				len=strlen(buf);
				if(len>0 && buf[len-1]=='\n') buf[--len]=0;
				if(!parameters && strncmp(buf,"timestamp ",10)==0)
				{
					pos->timestamp=atol(buf+10);
				}
				else if(!parameters && strncmp(buf,"board",5)==0)
				{
					if(!read_board(file,&pos->brade)) break;
				}
				else if(strncmp(buf,"computercolor ",14)==0)
				{
					if(strncmp(buf+14,"black",5)==0) pos->computer=black;
					else if(strncmp(buf+14,"white",5)==0) pos->computer=white;
					else
					{
						fprintf(stderr,"error: invalid computer color %s\n",buf+14);
						break;
					}
				}
				else if(!parameters && strncmp(buf,"humantime ",10)==0)
				{
					pos->humantime=atoi(buf+10);
				}
				else if(!parameters && strncmp(buf,"computertime ",13)==0)
				{
					pos->computertime=atoi(buf+13);
				}
				else if(strncmp(buf,"targettime ",11)==0)
				{
					pos->targettime=atoi(buf+11);
				}
				else if(!parameters && strncmp(buf,"turntomove ",11)==0)
				{
					if(strncmp(buf+11,"black",5)==0) pos->turntomove=black;
					else if(strncmp(buf+11,"white",5)==0) pos->turntomove=white;
					else
					{
						fprintf(stderr,"error: invalid turntomove color %s\n",buf+11);
						break;
					}
				}
				else if(!parameters && strncmp(buf,"passes ",7)==0)
				{
					pos->passes=atoi(buf+7);
				}
				else if(strncmp(buf,"cachesize ",10)==0)
				{
					pos->cachesize=atol(buf+10);
				}
				else if(strncmp(buf,"deterministic ",14)==0)
				{
					if(strcmp(buf+14,"true")==0) pos->deterministic=true;
					else if(strcmp(buf+14,"false")==0) pos->deterministic=false;
					else
					{
						fprintf(stderr,"error: invalid deterministic truth value %s\n",buf+9);
						break;
					}
				}
				else if(strncmp(buf,"computername ",13)==0)
				{
					strncpy(pos->computername,buf+13,sizeof pos->computername);
					pos->computername[sizeof pos->computername-1]=0;
				}
				else if(strncmp(buf,"humanname ",10)==0)
				{
					strncpy(pos->humanname,buf+10,sizeof pos->humanname);
					pos->humanname[sizeof pos->humanname-1]=0;
				}
				else if(!parameters && strncmp(buf,"moves ",6)==0)
				{
					int i=0;
					char *p;
					for(p=buf+6;*p;p+=3)
					{
						if(p[0]=='p' && p[1]=='s' && p[2]==' ') pos->moves[i++]=64;
						else if(p[0]>='a' && p[0]<='h' && p[1]>='1' && p[1]<='8' && p[2]==' ')
						{
							pos->moves[i++]=p[0]-'a'+8*(p[1]-'1');
						}
						else
						{
							fprintf(stderr,"error: unrecognized move %s\n",p);
							break;
						}
					}
					pos->nummoves=i;
				}
				else if(!parameters && strncmp(buf,"times ",6)==0)
				{
					int i=0;
					char *p=buf+6;
					while(*p)
					{
						pos->times[i++]=(short)atoi(p);
						while(*p && !isspace(*p)) p++;
						while(*p && isspace(*p)) p++;
					}
				}
				else if(strncmp(buf,"winloss ",8)==0)
				{	
					int i=atoi(buf+8)-1;
					while(i<60) pos->eval[i++]=winloss;
				}
				else if(strncmp(buf,"discdiff ",9)==0)
				{
					int i=atoi(buf+9)-1;
					while(i<60) pos->eval[i++]=discdiff;
				}
				else if(strncmp(buf,"mobility ",9)==0)
				{
					int i=atoi(buf+9)-1;
					while(i<60) pos->eval[i++]=mobility;
				}
				else if(strncmp(buf,"potmobility ",12)==0)
				{
					int i=atoi(buf+12)-1;
					while(i<60) pos->eval[i++]=potmobility;
				}
				else if(buf[0]!='#')
				{
					fprintf(stderr,"error: unrecognized identifier '%s' in %sfile '%s'\n",buf,parameters?"parameter ":"",filename);
					break;
				}
			}
			fclose(file);
			return success;
		}
		else if(parameters)
		{
			fprintf(stderr,"warning: could not open parameterfile '%s' errno %d\n",filename,errno);
			return false;
		}
		fprintf(stderr,"error: could not open savefile '%s' errno %d\n",filename,errno);
		filename=0; /* ask user for alternative filename */
	}
}
