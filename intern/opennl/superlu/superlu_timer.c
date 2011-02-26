/** \file opennl/superlu/superlu_timer.c
 *  \ingroup opennl
 */
/* 
 * Purpose
 * ======= 
 *	Returns the time in seconds used by the process.
 *
 * Note: the timer function call is machine dependent. Use conditional
 *       compilation to choose the appropriate function.
 *
 */

/* We want this flag, safer than putting in build system */
#define NO_TIMER

double  SuperLU_timer_ ();

#ifdef SUN 
/*
 * 	It uses the system call gethrtime(3C), which is accurate to 
 *	nanoseconds. 
*/
#include <sys/time.h>
 
double SuperLU_timer_() {
    return ( (double)gethrtime() / 1e9 );
}

#else

#ifndef NO_TIMER
#include <sys/types.h>
#include <sys/times.h>
#include <time.h>
#include <sys/time.h>
#endif

#ifndef CLK_TCK
#define CLK_TCK 60
#endif
double SuperLU_timer_(void);

double SuperLU_timer_(void)
{
#ifdef NO_TIMER
    /* no sys/times.h on WIN32 */
    double tmp;
    tmp = 0.0;
#else
    struct tms use;
    double tmp;
    times(&use);
    tmp = use.tms_utime;
    tmp += use.tms_stime;
#endif
    return (double)(tmp) / CLK_TCK;
}

#endif

