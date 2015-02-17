/*
 * Copyright 1993 Ola Liljedahl
 */

#ifndef _TIMES_H
#define _TIMES_H

/*
	return real time, unit is 0.01 s (actual resolution is OS dependent)
*/

STATIC unsigned long real_time(void);

/*
	return used CPU time, unit is 0.01 s (actual resolution is OS dependent)
*/

STATIC unsigned long cpu_time(void);

#endif
