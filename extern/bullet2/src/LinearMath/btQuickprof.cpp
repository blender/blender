/************************************************************************
* QuickProf                                                             *
* Copyright (C) 2006                                                    *
* Tyler Streeter  tylerstreeter@gmail.com                               *
* All rights reserved.                                                  *
* Web: http://quickprof.sourceforge.net                                 *
*                                                                       *
* This library is free software; you can redistribute it and/or         *
* modify it under the terms of EITHER:                                  *
*   (1) The GNU Lesser bteral Public License as published by the Free  *
*       Software Foundation; either version 2.1 of the License, or (at  *
*       your option) any later version. The text of the GNU Lesser      *
*       bteral Public License is included with this library in the     *
*       file license-LGPL.txt.                                          *
*   (2) The BSD-style license that is included with this library in     *
*       the file license-BSD.txt.                                       *
*                                                                       *
* This library is distributed in the hope that it will be useful,       *
* but WITHOUT ANY WARRANTY; without even the implied warranty of        *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the files    *
* license-LGPL.txt and license-BSD.txt for more details.                *
************************************************************************/

// Please visit the project website (http://quickprof.sourceforge.net) 
// for usage instructions.

// Credits: The Clock class was inspired by the Timer classes in 
// Ogre (www.ogre3d.org).

#include "LinearMath/btQuickprof.h"

#ifdef USE_QUICKPROF

// Note: We must declare these private static variables again here to 
// avoid link errors.
bool btProfiler::mEnabled = false;
hidden::Clock btProfiler::mClock;
unsigned long int btProfiler::mCurrentCycleStartMicroseconds = 0;
unsigned long int btProfiler::mLastCycleDurationMicroseconds = 0;
std::map<std::string, hidden::ProfileBlock*> btProfiler::mProfileBlocks;
std::ofstream btProfiler::mOutputFile;
bool btProfiler::mFirstFileOutput = true;
btProfiler::BlockTimingMethod btProfiler::mFileOutputMethod;
unsigned long int btProfiler::mCycleNumber = 0;
#endif //USE_QUICKPROF
