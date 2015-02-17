/*
 * Copyright 1993 Ola Liljedahl
 */

#include <time.h>
#include <sys/time.h>
#include "times.h"

#undef get_time

/*
	return real time, unit is 0.01 s (actual resolution is OS dependent)
*/

STATIC unsigned long real_time(void)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec * 100 + tv.tv_usec / 10000;
}

/*
	return used CPU time, unit is 0.01 s (actual resolution is OS dependent)
*/

STATIC unsigned long cpu_time(void)
{
	return clock() / (CLOCKS_PER_SEC / 100);
}
