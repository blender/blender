/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Global C style utility funcions
 *
 *****************************************************************************/


#include <iostream>
#include <sstream>
#ifdef WIN32
// for timing
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#endif

#include "utilities.h"

#ifndef NOPNG
#ifdef WIN32
#include "png.h"
#else
#include <png.h>
#endif
#endif // NOPNG
#include <zlib.h>

// global debug level
#ifdef DEBUG 
int gDebugLevel = DEBUG;
#else // DEBUG 
int gDebugLevel = 0;
#endif // DEBUG 

// global world state, acces with get/setElbeemState
int gElbeemState = SIMWORLD_INVALID;

// access global state of elbeem simulator
void setElbeemState(int set) {
	gElbeemState = set;
}
int  getElbeemState(void) { 
	return gElbeemState;
}
int  isSimworldOk(void) {
	return (getElbeemState>=0);
}

// last error as string, acces with get/setElbeemErrorString
char gElbeemErrorString[256] = {'-','\0' };

// access elbeem simulator error string
void setElbeemErrorString(const char* set) {
	strncpy(gElbeemErrorString, set, 256);
}
char* getElbeemErrorString(void) { return gElbeemErrorString; }


//! for interval debugging output
myTime_t globalIntervalTime = 0;
//! color output setting for messages (0==off, else on)
#ifdef WIN32
// switch off first call
#define DEF_globalColorSetting -1 
#else // WIN32
// linux etc., on by default
#define DEF_globalColorSetting 1 
#endif // WIN32
int globalColorSetting = DEF_globalColorSetting; // linux etc., on by default
int globalFirstEnvCheck = 0;
void resetGlobalColorSetting() { globalColorSetting = DEF_globalColorSetting; }

// global string for formatting vector output, TODO test!?
const char *globVecFormatStr = "V[%f,%f,%f]";


// global mp on/off switch
bool glob_mpactive = false; 
// global access to mpi index, for debugging (e.g. in utilities.cpp)
int glob_mpnum = -1;
int glob_mpindex = -1;
int glob_mppn = -1;


//-----------------------------------------------------------------------------
// helper function that converts a string to integer, 
// and returns an alternative value if the conversion fails
int convertString2Int(const char *str, int alt)
{
	int val;
	char *endptr;
	bool success=true;

	val = strtol(str, &endptr, 10);
	if( (str==endptr) ||
			((str!=endptr) && (*endptr != '\0')) ) success = false;

	if(!success) {
		return alt;
	}
	return val;
}

//-----------------------------------------------------------------------------
//! helper function that converts a flag field to a readable integer
string convertFlags2String(int flags) {
	std::ostringstream ret;
	ret <<"(";
	int max = sizeof(int)*8;
	for(int i=0; i<max; i++) {
		if(flags & (1<<31)) ret <<"1";
		else ret<<"0";
		if(i<max-1) {
			//ret << ",";
			if((i%8)==7) ret << " ";
		}
		flags = flags << 1;
	}	
	ret <<")";
	return ret.str();
}

#ifndef NOPNG
//-----------------------------------------------------------------------------
//! write png image
int writePng(const char *fileName, unsigned char **rowsp, int w, int h)
{
	// defaults for elbeem
	const int colortype = PNG_COLOR_TYPE_RGBA;
	const int bitdepth = 8;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_bytep *rows = rowsp;

	//FILE *fp = fopen(fileName, "wb");
	FILE *fp = NULL;
	string doing = "open for writing";
	if (!(fp = fopen(fileName, "wb"))) goto fail;

	if(!png_ptr) {
		doing = "create png write struct";
		if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) goto fail;
	}
	if(!info_ptr) {
		doing = "create png info struct";
		if (!(info_ptr = png_create_info_struct(png_ptr))) goto fail;
	}

	if (setjmp(png_jmpbuf(png_ptr))) goto fail;
	doing = "init IO";
	png_init_io(png_ptr, fp);
	doing = "write header";
	png_set_IHDR(png_ptr, info_ptr, w, h, bitdepth, colortype, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	doing = "write info";
	png_write_info(png_ptr, info_ptr);
	doing = "write image";
	png_write_image(png_ptr, rows);
	doing = "write end";
	png_write_end(png_ptr, NULL);
	doing = "write destroy structs";
	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose( fp );
	return 0;

fail:	
	errMsg("writePng","Write_png: could not "<<doing<<" !");
	if(fp) fclose( fp );
	if(png_ptr || info_ptr) png_destroy_write_struct(&png_ptr, &info_ptr);
	return -1;
}
#else // NOPNG
// fallback - write ppm
int writePng(const char *fileName, unsigned char **rowsp, int w, int h)
{
	gzFile gzf;
	string filentemp(fileName);
	// remove suffix
	if((filentemp.length()>4) && (filentemp[filentemp.length()-4]=='.')) {
		filentemp[filentemp.length()-4] = '\0';
	}
	std::ostringstream filennew;
	filennew << filentemp.c_str();
	filennew << ".ppm.gz";

	gzf = gzopen(filennew.str().c_str(), "wb9");
	if(!gzf) goto fail;

	gzprintf(gzf,"P6\n%d %d\n255\n",w,h);
	// output binary pixels
	for(int j=0;j<h;j++) {
		for(int i=0;i<h;i++) {
			// remove alpha values
			gzwrite(gzf,&rowsp[j][i*4],3);
		}
	}

	gzclose( gzf );
	errMsg("writePng/ppm","Write_png/ppm: wrote to "<<filennew.str()<<".");
	return 0;

fail:	
	errMsg("writePng/ppm","Write_png/ppm: could not write to "<<filennew.str()<<" !");
	return -1;
}
#endif // NOPNG


//-----------------------------------------------------------------------------
// helper function to determine current time
myTime_t getTime()
{
	myTime_t ret = 0;
#ifdef WIN32
	LARGE_INTEGER liTimerFrequency;
	QueryPerformanceFrequency(&liTimerFrequency);
	LARGE_INTEGER liLastTime;
	QueryPerformanceCounter(&liLastTime);
	ret = (INT)( ((double)liLastTime.QuadPart / liTimerFrequency.QuadPart)*1000 ); // - mFirstTime;
#else
	struct timeval tv;
	struct timezone tz;
	tz.tz_minuteswest = 0;
	tz.tz_dsttime = 0;
	gettimeofday(&tv,&tz);
 	ret = (tv.tv_sec*1000)+(tv.tv_usec/1000); //-mFirstTime;
#endif
	return (myTime_t)ret;
}
//-----------------------------------------------------------------------------
// convert time to readable string
string getTimeString(myTime_t usecs) {
	std::ostringstream ret;
	//myTime_t us = usecs % 1000;
	myTime_t ms = (myTime_t)(   (double)usecs / (60.0*1000.0)  );
	myTime_t ss = (myTime_t)(  ((double)usecs / 1000.0) - ((double)ms*60.0)  );
	int      ps = (int)(       ((double)usecs - (double)ss*1000.0)/10.0 );

 	//ret.setf(ios::showpoint|ios::fixed);
 	//ret.precision(5); ret.width(7);

	if(ms>0) {
		ret << ms<<"m"<< ss<<"s" ;
	} else {
		if(ps>0) {
			ret << ss<<".";
			if(ps<10) { ret <<"0"; }
			ret <<ps<<"s" ;
		} else {
			ret << ss<<"s" ;
		}
	}
	return ret.str();
}

//! helper to check if a bounding box was specified in the right way
bool checkBoundingBox(ntlVec3Gfx s, ntlVec3Gfx e, string checker) {
	if( (s[0]>e[0]) ||
			(s[1]>e[1]) ||
			(s[2]>e[2]) ) {
		errFatal("checkBoundingBox","Check by '"<<checker<<"' for BB "<<s<<":"<<e<<" failed! Aborting...",SIMWORLD_INITERROR);
		return 1;
	}
	return 0;
}



//-----------------------------------------------------------------------------
// debug message output

static string col_black ( "\033[0;30m");
static string col_dark_gray ( "\033[1;30m");
static string col_bright_gray ( "\033[0;37m");
static string col_red ( "\033[0;31m");
static string col_bright_red ( "\033[1;31m");
static string col_green ( "\033[0;32m");
static string col_bright_green ( "\033[1;32m");
static string col_bright_yellow ( "\033[1;33m");
static string col_yellow ( "\033[0;33m");
static string col_cyan ( "\033[0;36m");
static string col_bright_cyan ( "\033[1;36m");
static string col_purple ( "\033[0;35m");
static string col_bright_purple ( "\033[1;35m");
static string col_neutral ( "\033[0m");
static string col_std = col_bright_gray;

std::ostringstream globOutstr;
bool               globOutstrForce=false;
#define DM_NONE      100
void messageOutputForce(string from) {
	bool org = globOutstrForce;
	globOutstrForce = true;
	messageOutputFunc(from, DM_NONE, "\n", 0);
	globOutstrForce = org;
}

void messageOutputFunc(string from, int id, string msg, myTime_t interval) {
	// fast skip
	if((id!=DM_FATAL)&&(gDebugLevel<=0)) return;

	if(interval>0) {
		myTime_t currTime = getTime();
		if((currTime - globalIntervalTime)>interval) {
			globalIntervalTime = getTime();
		} else {
			return;
		}
	}

	// colors off?
	if( (globalColorSetting == -1) || // off for e.g. win32 
		  ((globalColorSetting==1) && ((id==DM_FATAL)||( getenv("ELBEEM_NOCOLOROUT") )) )
		) {
		// only reset once
		col_std = col_black = col_dark_gray = col_bright_gray =  
		col_red =  col_bright_red =  col_green =  
		col_bright_green =  col_bright_yellow =  
		col_yellow =  col_cyan =  col_bright_cyan =  
		col_purple =  col_bright_purple =  col_neutral =  "";
		globalColorSetting = 0;
	}

	std::ostringstream sout;
	if(id==DM_DIRECT) {
		sout << msg;
	} else {
		sout << col_cyan<< from;
		switch(id) {
			case DM_MSG:
				sout << col_std << " message:";
				break;
			case DM_NOTIFY:
				sout << col_bright_cyan << " note:" << col_std;
				break;
			case DM_IMPORTANT:
				sout << col_yellow << " important:" << col_std;
				break;
			case DM_WARNING:
				sout << col_bright_red << " warning:" << col_std;
				break;
			case DM_ERROR:
				sout << col_red << " error:" << col_red;
				break;
			case DM_FATAL:
				sout << col_red << " fatal("<<gElbeemState<<"):" << col_red;
				break;
			case DM_NONE:
				// only internal debugging msgs
				break;
			default:
				// this shouldnt happen...
				sout << col_red << " --- messageOutputFunc error: invalid id ("<<id<<") --- aborting... \n\n" << col_std;
				break;
		}
		sout <<" "<< msg << col_std;
	}

	if(id==DM_FATAL) {
		strncpy(gElbeemErrorString,sout.str().c_str(), 256);
		// dont print?
		if(gDebugLevel==0) return;
		sout << "\n"; // add newline for output
	}

	// determine output - file==1/stdout==0 / globstr==2
	char filen[256];
	strcpy(filen,"debug_unini.txt");
	int fileout = false;
#if ELBEEM_MPI==1
	std::ostringstream mpin;
	if(glob_mpindex>=0) {
		mpin << "elbeem_log_"<< glob_mpindex <<".txt";
	} else {
		mpin << "elbeem_log_ini.txt";
	}
	fileout = 1;
	strncpy(filen, mpin.str().c_str(),255); filen[255]='\0';
#else
	strncpy(filen, "elbeem_debug_log.txt",255);
#endif

#ifdef WIN32
	// windows causes trouble with direct output
	fileout = 1;
#endif // WIN32

#if PARALLEL==1
	fileout = 2;// buffer out, switch off again...
	if(globOutstrForce) fileout=1;
#endif
	if(getenv("ELBEEM_FORCESTDOUT")) {
		fileout = 0;// always direct out
	}
	//fprintf(stdout,"out deb %d, %d, '%s',l%d \n",globOutstrForce,fileout, filen, globOutstr.str().size() );

#if PARALLEL==1
#pragma omp critical 
#endif // PARALLEL==1
	{
	if(fileout==1) {
		// debug level is >0 anyway, so write to file...
		FILE *logf = fopen(filen,"a+");
		// dont complain anymore here...
		if(logf) {
			if(globOutstrForce) {
				fprintf(logf, "%s",globOutstr.str().c_str() );
				globOutstr.str(""); // reset
			}
			fprintf(logf, "%s",sout.str().c_str() );
			fclose(logf);
		}
	} else if(fileout==2) {
			globOutstr << sout.str();
	} else {
		// normal stdout output
		fprintf(stdout, "%s",sout.str().c_str() );
		if(id!=DM_DIRECT) fflush(stdout); 
	}
	} // omp crit
}

// helper functions from external program using elbeem lib (e.g. Blender)
/* set gDebugLevel according to env. var */
extern "C" 
void elbeemCheckDebugEnv(void) {
	const char *strEnvName = "BLENDER_ELBEEMDEBUG";
	const char *strEnvName2 = "ELBEEM_DEBUGLEVEL";
	if(globalFirstEnvCheck) return;

	if(getenv(strEnvName)) {
		gDebugLevel = atoi(getenv(strEnvName));
		if(gDebugLevel>0) debMsgStd("performElbeemSimulation",DM_NOTIFY,"Using envvar '"<<strEnvName<<"'='"<<getenv(strEnvName)<<"', debugLevel set to: "<<gDebugLevel<<"\n", 1);
	}
	if(getenv(strEnvName2)) {
		gDebugLevel = atoi(getenv(strEnvName2));
		if(gDebugLevel>0) debMsgStd("performElbeemSimulation",DM_NOTIFY,"Using envvar '"<<strEnvName2<<"'='"<<getenv(strEnvName2)<<"', debugLevel set to: "<<gDebugLevel<<"\n", 1);
	}
	if(gDebugLevel< 0) gDebugLevel =  0;
	if(gDebugLevel>10) gDebugLevel =  0; // only use valid values
	globalFirstEnvCheck = 1;
}

/* elbeem debug output function */
extern "C" 
void elbeemDebugOut(char *msg) {
	elbeemCheckDebugEnv();
	// external messages default to debug level 5...
	if(gDebugLevel<5) return;
	// delegate to messageOutputFunc
	messageOutputFunc("[External]",DM_MSG,msg,0);
}

/* set elbeem debug output level (0=off to 10=full on) */
extern "C" 
void elbeemSetDebugLevel(int level) {
	if(level<0)  level=0;
	if(level>10) level=10;
	gDebugLevel=level;
}


/* estimate how much memory a given setup will require */
#include "solver_interface.h"

extern "C" 
double elbeemEstimateMemreq(int res, 
		float sx, float sy, float sz,
		int refine, char *retstr) {
	int resx = res, resy = res, resz = res;
	// dont use real coords, just place from 0.0 to sizeXYZ
	ntlVec3Gfx vgs(0.0), vge(sx,sy,sz);
	initGridSizes( resx,resy,resz, vgs,vge, refine, 0);

	double memreq = -1.0;
	string memreqStr("");	
	// ignore farfield for now...
	calculateMemreqEstimate(resx,resy,resz, refine, 0., &memreq, NULL, &memreqStr );

	if(retstr) { 
		// copy at max. 32 characters
		strncpy(retstr, memreqStr.c_str(), 32 );
		retstr[31] = '\0';
	}
	return memreq;
}



