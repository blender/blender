/**
 * $Id$
 */

/*

*
* Template Numerical Toolkit (TNT): Linear Algebra Module
*
* Mathematical and Computational Sciences Division
* National Institute of Technology,
* Gaithersburg, MD USA
*
*
* This software was developed at the National Institute of Standards and
* Technology (NIST) by employees of the Federal Government in the course
* of their official duties. Pursuant to title 17 Section 105 of the
* United States Code, this software is not subject to copyright protection
* and is in the public domain.  The Template Numerical Toolkit (TNT) is
* an experimental system.  NIST assumes no responsibility whatsoever for
* its use by other parties, and makes no guarantees, expressed or implied,
* about its quality, reliability, or any other characteristic.
*
* BETA VERSION INCOMPLETE AND SUBJECT TO CHANGE
* see http://math.nist.gov/tnt for latest updates.
*
*/

#ifndef STPWATCH_H
#define STPWATCH_H

// for clock() and CLOCKS_PER_SEC
#include <ctime>

namespace TNT
{

/*  Simple stopwatch object:

        void    start()     : start timing
        double  stop()      : stop timing
        void    reset()     : set elapsed time to 0.0
        double  read()      : read elapsed time (in seconds)

*/

inline double seconds(void)
{
    static const double secs_per_tick = 1.0 / CLOCKS_PER_SEC;
    return ( (double) clock() ) * secs_per_tick;
}


class stopwatch {
    private:
        int running;
        double last_time;
        double total;

    public:
        stopwatch() : running(0), last_time(0.0), total(0.0) {}
        void reset() { running = 0; last_time = 0.0; total=0.0; }
        void start() { if (!running) { last_time = seconds(); running = 1;}}
        double stop()  { if (running) 
                            {
                                total += seconds() - last_time; 
                                running = 0;
                             }
                          return total; 
                        }
        double read()   {  if (running) 
                            {
                                total+= seconds() - last_time;
                                last_time = seconds();
                            }
                           return total;
                        }       
                            
};

} // namespace TNT

#endif
 
