/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Global C style utility funcions
 *
 *****************************************************************************/
#ifndef UTILITIES_H
#include "ntl_vector3dim.h"

typedef unsigned long myTime_t;

//! helper function that converts a string to integer
int convertString2Int(const char *string, int alt);

//! helper function that converts a flag field to a readable integer
std::string convertFlags2String(int flags);

//! write png image
#ifndef NOPNG
//int writePng(const char *fileName, unsigned char **rows, int w, int h, int colortype, int bitdepth);
int writePng(const char *fileName, unsigned char **rows, int w, int h);
//! write opengl buffer to png
void writeOpenglToPng(const char *fileName);
#endif// NOPNG

// output streams
#ifdef ELBEEM_BLENDER
extern "C" FILE* GEN_errorstream;
extern "C" FILE* GEN_userstream;
#endif // ELBEEM_BLENDER

//! get the current system time
myTime_t getTime();
//! convert time to readable string
std::string getTimeString(myTime_t usecs);

//! helper to check if a bounding box was specified in the right way
bool checkBoundingBox(ntlVec3Gfx s, ntlVec3Gfx e, std::string checker);

// optionally include OpenGL utility functions
#ifdef USE_GLUTILITIES

void drawCubeWire(ntlVec3Gfx s, ntlVec3Gfx e);
void drawCubeSolid(ntlVec3Gfx s, ntlVec3Gfx e);

#endif // USE_GLUTILITIES


/* debugging outputs */
//#define DEBUG 10

/* debug output function */
#define DM_MSG        1
#define DM_NOTIFY     2
#define DM_IMPORTANT  3
#define DM_WARNING    4
#define DM_ERROR      5
#define DM_DIRECT     6
void messageOutputFunc(std::string from, int id, std::string msg, myTime_t interval);

/* debugging messages defines */
#if LBM_PRECISION==2
#define MSGSTREAM std::ostringstream msg; msg.precision(15); msg.width(17);
#else
#define MSGSTREAM std::ostringstream msg; msg.precision(7); msg.width(9);
#endif
#ifdef DEBUG 
#	define debMsgDirect(mStr)                        { std::ostringstream msg; msg << mStr; messageOutputFunc(string(""), DM_DIRECT, msg.str(), 0); }
#	define debMsgStd(from,id,mStr,level)             if(DEBUG>=level){ MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, id, msg.str(), 0); }
#	define debMsgNnl(from,id,mStr,level)             if(DEBUG>=level){ MSGSTREAM; msg << mStr       ; messageOutputFunc(from, id, msg.str(), 0); }
#	define debMsgInter(from,id,mStr,level, interval) if(DEBUG>=level){ MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, id, msg.str(), interval); }
#	define debugOut(mStr,level)    if(DEBUG>=level){ debMsgStd("D",DM_MSG,mStr,level); }
#	define debugOutNnl(mStr,level) if(DEBUG>=level){ debMsgNnl("D",DM_MSG,mStr,level); }
#	define debugOutInter(mStr,level, interval) debMsgInter("D",DM_MSG,mStr,level, interval); 

#else

#	define debMsgDirect(mStr)
#	define debMsgStd(from,id,mStr,level)
#	define debMsgNnl(from,id,mStr,level)
#	define debMsgInter(from,id,mStr,level, interval)
#	define debugOut(mStr,level)  
#	define debugOutNnl(mStr,level)  
#	define debugOutInter(mStr,level, interval) 
#endif

/* Error output function */
#define errMsg(from,mStr) { MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, DM_ERROR,   msg.str(), 0); }
#define warnMsg(from,mStr){ MSGSTREAM; msg << mStr <<"\n"; messageOutputFunc(from, DM_WARNING, msg.str(), 0); }
#define errorOut(mStr) { errMsg("D",mStr); }
// old...  #define ...(mStr) { std::cout << mStr << "\n"; fflush(stdout); }

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



/* some useful templated functions 
 * may require some operators for the classes
 */

/* minimum */
template < class T >
inline T
MIN( T a, T b )
{ return (a < b) ? a : b ; }

/* maximum */
template < class T >
inline T
MAX( T a, T b )
{ return (a < b) ? b : a ; }

/* absolute value */
template < class T >
inline T
ABS( T a )
{ return (0 < a) ? a : -a ; }

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
