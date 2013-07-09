/** \file elbeem/intern/utilities.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Global C style utility funcions
 *
 *****************************************************************************/
#ifndef UTILITIES_H
#include "ntl_vector3dim.h"


/* debugging outputs , debug level 0 (off) to 10 (max) */
#ifdef ELBEEM_PLUGIN
#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 0
#else // ELBEEM_PLUGIN
#define DEBUG 10
#endif // ELBEEM_PLUGIN
extern "C" int gDebugLevel;


// time measurements
typedef unsigned long myTime_t;


// state of the simulation world
// default
#define SIMWORLD_INVALID       0
// performing init
#define SIMWORLD_INITIALIZING  1
// after init, before starting simulation
#define SIMWORLD_INITED        2
// stop of the simulation run, can be continued later
#define SIMWORLD_STOP          3
// error during init
#define SIMWORLD_INITERROR    -1
// error during simulation
#define SIMWORLD_PANIC        -2
// general error 
#define SIMWORLD_GENERICERROR -3

// access global state of elbeem simulator
void setElbeemState(int set);
int  getElbeemState(void);
int  isSimworldOk(void);

// access elbeem simulator error string
void setElbeemErrorString(const char* set);
char* getElbeemErrorString(void);


/* debug output function */
#define DM_MSG        1
#define DM_NOTIFY     2
#define DM_IMPORTANT  3
#define DM_WARNING    4
#define DM_ERROR      5
#define DM_DIRECT     6
#define DM_FATAL      7
void messageOutputFunc(string from, int id, string msg, myTime_t interval);

/* debugging messages defines */
#ifdef DEBUG 
#if LBM_PRECISION==2
#define MSGSTREAM std::ostringstream msg; msg.precision(15); msg.width(17);
#else
#define MSGSTREAM std::ostringstream msg; msg.precision(7); msg.width(9);
#endif

#	define debMsgDirect(mStr)                         if(gDebugLevel>0)      { std::ostringstream msg; msg << mStr; messageOutputFunc(string(""), DM_DIRECT, msg.str(), 0); }
#	define debMsgStd(from,id,mStr,level)              if(gDebugLevel>=level) { MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, id, msg.str(), 0); }
#	define debMsgNnl(from,id,mStr,level)              if(gDebugLevel>=level) { MSGSTREAM; msg << mStr       ; messageOutputFunc(from, id, msg.str(), 0); }
#	define debMsgInter(from,id,mStr,level, interval)  if(gDebugLevel>=level) { MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, id, msg.str(), interval); }
#	define debugOut(mStr,level)                       if(gDebugLevel>=level) { debMsgStd("D",DM_MSG,mStr,level); }
#	define debugOutNnl(mStr,level)                    if(gDebugLevel>=level) { debMsgNnl("D",DM_MSG,mStr,level); }
#	define debugOutInter(mStr,level, interval)        debMsgInter("D",DM_MSG ,mStr,level, interval); 
/* Error output function */
#define errMsg(from,mStr)                           if(gDebugLevel>0){ MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, DM_ERROR,   msg.str(), 0); }
#define warnMsg(from,mStr)                          if(gDebugLevel>0){ MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, DM_WARNING, msg.str(), 0); }

#else
// no messages at all...
#	define debMsgDirect(mStr)
#	define debMsgStd(from,id,mStr,level)
#	define debMsgNnl(from,id,mStr,level)
#	define debMsgInter(from,id,mStr,level, interval)
#	define debugOut(mStr,level)  
#	define debugOutNnl(mStr,level)  
#	define debugOutInter(mStr,level, interval) 
#	define errMsg(from,mStr)
#	define warnMsg(from,mStr)
#endif

#define errorOut(mStr) { errMsg("D",mStr); }

// fatal errors - have to be handled 
#define errFatal(from,mStr,errCode) { \
	setElbeemState(errCode); \
	MSGSTREAM; msg << mStr; \
	messageOutputFunc(from, DM_FATAL, msg.str(), 0); \
}


//! helper function that converts a string to integer
int convertString2Int(const char *str, int alt);

//! helper function that converts a flag field to a readable integer
string convertFlags2String(int flags);

//! get the current system time
myTime_t getTime();
//! convert time to readable string
string getTimeString(myTime_t usecs);

//! helper to check if a bounding box was specified in the right way
bool checkBoundingBox(ntlVec3Gfx s, ntlVec3Gfx e, string checker);

//! reset color output for elbeem init
void resetGlobalColorSetting();


/*! print some vector from 3 values e.g. for ux,uy,uz */
#define PRINT_VEC(x,y,z) " ["<<(x)<<","<<(y)<<","<<(z)<<"] "

/*! print some vector from 3 values e.g. for ux,uy,uz */
#define PRINT_VEC2D(x,y) " ["<<(x)<<","<<(y)<<"] "

/*! print l'th neighbor of i,j,k as a vector, as we need ijk all the time */
#define PRINT_IJK_NBL PRINT_VEC(i+D::dfVecX[l],j+D::dfVecY[l],k+D::dfVecZ[l])

/*! print i,j,k as a vector, as we need ijk all the time */
#define PRINT_IJK PRINT_VEC(i,j,k)

/*! print i,j,k as a vector, as we need ijk all the time */
#define PRINT_IJ PRINT_VEC2D(i,j)

/*! print some vector from 3 values e.g. for ux,uy,uz */
#define PRINT_NTLVEC(v) " ["<<(v)[0]<<","<<(v)[1]<<","<<(v)[2]<<"] "

/*! print some vector from 3 values e.g. for ux,uy,uz */
#define PRINT_NTLVEC2D(v) " ["<<(v)[0]<<","<<(v)[1]<<"] "

/*! print a triangle */
#define PRINT_TRIANGLE(t,mpV)  " { "<<PRINT_VEC( (mpV[(t).getPoints()[0]][0]),(mpV[(t).getPoints()[0]][1]),(mpV[(t).getPoints()[0]][2]) )<<\
	PRINT_VEC( (mpV[(t).getPoints()[1]][0]),(mpV[(t).getPoints()[1]][1]),(mpV[(t).getPoints()[1]][2]) )<<" | "<<\
	PRINT_VEC( (mpV[(t).getPoints()[2]][0]),(mpV[(t).getPoints()[2]][1]),(mpV[(t).getPoints()[2]][2]) )<<" } "


// write png image
int writePng(const char *fileName, unsigned char **rowsp, int w, int h);

/* some useful templated functions 
 * may require some operators for the classes
 */

/* minimum */
#ifdef MIN
#undef MIN
#endif
template < class T >
inline T
MIN( T a, T b )
{ return (a < b) ? a : b ; }

/* maximum */
#ifdef MAX
#undef MAX
#endif
template < class T >
inline T
MAX( T a, T b )
{ return (a < b) ? b : a ; }

/* sign of the value */
template < class T >
inline T
SIGNUM( T a )
{ return (0 < a) ? 1 : -1 ; }

/* sign, returns -1,0,1 depending on sign/value=0 */
template < class T >
inline T
SIGNUM0( T a )
{ return (0 < a) ? 1 : ( a < 0 ? -1 : 0 ) ; }

/* round to nearest integer */
inline int
ROUND(double d)
{ return int(d + 0.5); }

/* square function */
template < class T >
inline T
SQUARE( T a )
{ return a*a; }


#define UTILITIES_H
#endif
