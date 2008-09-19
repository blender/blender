/*
Copyright (c) 2006 Tyler Streeter

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the use of this software.
Permission is granted to anyone to use this software for any purpose, 
including commercial applications, and to alter it and redistribute it freely, 
subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software. If you use this software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*/


// Please visit the project website (http://quickprof.sourceforge.net) 
// for usage instructions.

// Credits: The Clock class was inspired by the Timer classes in 
// Ogre (www.ogre3d.org).

#include "LinearMath/btQuickprof.h"

#ifdef USE_QUICKPROF

// Note: We must declare these private static variables again here to 
// avoid link errors.
bool btProfiler::mEnabled = false;
btClock btProfiler::mClock;
unsigned long int btProfiler::mCurrentCycleStartMicroseconds = 0;
unsigned long int btProfiler::mLastCycleDurationMicroseconds = 0;
std::map<std::string, hidden::ProfileBlock*> btProfiler::mProfileBlocks;
std::ofstream btProfiler::mOutputFile;
bool btProfiler::mFirstFileOutput = true;
btProfiler::BlockTimingMethod btProfiler::mFileOutputMethod;
unsigned long int btProfiler::mCycleNumber = 0;
#endif //USE_QUICKPROF
