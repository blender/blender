/**
 * $Id: GHOST_DisplayManagerCocoa.mm 13161 2008-01-07 19:13:47Z hos $
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**

 * $Id: GHOST_DisplayManagerCocoa.mm 13161 2008-01-07 19:13:47Z hos $
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	September 21, 2001
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <Carbon/Carbon.h>

#include "GHOST_DisplayManagerCocoa.h"
#include "GHOST_Debug.h"

// We do not support multiple monitors at the moment


GHOST_DisplayManagerCocoa::GHOST_DisplayManagerCocoa(void)
{
	if (::CGGetActiveDisplayList(0, NULL, &m_numDisplays) != CGDisplayNoErr)
	{
		m_numDisplays = 0;
		m_displayIDs = NULL;
	}
	if (m_numDisplays > 0)
	{
		m_displayIDs = new CGDirectDisplayID [m_numDisplays];
		GHOST_ASSERT((m_displayIDs!=NULL), "GHOST_DisplayManagerCocoa::GHOST_DisplayManagerCocoa(): memory allocation failed");
		::CGGetActiveDisplayList(m_numDisplays, m_displayIDs, &m_numDisplays);
	}
}


GHOST_TSuccess GHOST_DisplayManagerCocoa::getNumDisplays(GHOST_TUns8& numDisplays) const
{
	numDisplays = (GHOST_TUns8) m_numDisplays;
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerCocoa::getNumDisplaySettings(GHOST_TUns8 display, GHOST_TInt32& numSettings) const
{
	GHOST_ASSERT((display==kMainDisplay), "GHOST_DisplayManagerCocoa::getNumDisplaySettings(): only main display is supported");
	
	CFArrayRef displayModes;
	displayModes = ::CGDisplayAvailableModes(m_displayIDs[display]);
	CFIndex numModes = ::CFArrayGetCount(displayModes);
	numSettings = (GHOST_TInt32)numModes;
	
	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerCocoa::getDisplaySetting(GHOST_TUns8 display, GHOST_TInt32 index, GHOST_DisplaySetting& setting) const
{
	GHOST_ASSERT((display==kMainDisplay), "GHOST_DisplayManagerCocoa::getDisplaySetting(): only main display is supported");
	
	CFArrayRef displayModes;
	CGDirectDisplayID d = m_displayIDs[display];
	displayModes = ::CGDisplayAvailableModes(d);
	//CFIndex numModes = ::CFArrayGetCount(displayModes);/*unused*/
	//GHOST_TInt32 numSettings = (GHOST_TInt32)numModes; /*unused*/
	 CFDictionaryRef displayModeValues = (CFDictionaryRef)::CFArrayGetValueAtIndex(displayModes, index);
			
	setting.xPixels		= getValue(displayModeValues, kCGDisplayWidth);
	setting.yPixels		= getValue(displayModeValues, kCGDisplayHeight);
	setting.bpp			= getValue(displayModeValues, kCGDisplayBitsPerPixel);
	setting.frequency	= getValue(displayModeValues, kCGDisplayRefreshRate);
			
#ifdef GHOST_DEBUG
	printf("display mode: width=%d, height=%d, bpp=%d, frequency=%d\n", setting.xPixels, setting.yPixels, setting.bpp, setting.frequency);
#endif // GHOST_DEBUG

	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerCocoa::getCurrentDisplaySetting(GHOST_TUns8 display, GHOST_DisplaySetting& setting) const
{
	GHOST_ASSERT((display==kMainDisplay), "GHOST_DisplayManagerCocoa::getCurrentDisplaySetting(): only main display is supported");
        
	CFDictionaryRef displayModeValues = ::CGDisplayCurrentMode(m_displayIDs[display]);
	
	setting.xPixels		= getValue(displayModeValues, kCGDisplayWidth);
	setting.yPixels		= getValue(displayModeValues, kCGDisplayHeight);
	setting.bpp			= getValue(displayModeValues, kCGDisplayBitsPerPixel);
	setting.frequency	= getValue(displayModeValues, kCGDisplayRefreshRate);

#ifdef GHOST_DEBUG
	printf("current display mode: width=%d, height=%d, bpp=%d, frequency=%d\n", setting.xPixels, setting.yPixels, setting.bpp, setting.frequency);
#endif // GHOST_DEBUG

	return GHOST_kSuccess;
}


GHOST_TSuccess GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(GHOST_TUns8 display, const GHOST_DisplaySetting& setting)
{
	GHOST_ASSERT((display==kMainDisplay), "GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(): only main display is supported");

#ifdef GHOST_DEBUG
	printf("GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(): requested settings:\n");
	printf("  setting.xPixels=%d\n", setting.xPixels);
	printf("  setting.yPixels=%d\n", setting.yPixels);
	printf("  setting.bpp=%d\n", setting.bpp);
	printf("  setting.frequency=%d\n", setting.frequency);
#endif // GHOST_DEBUG

	CFDictionaryRef displayModeValues = ::CGDisplayBestModeForParametersAndRefreshRate(
		m_displayIDs[display],
		(size_t)setting.bpp,
		(size_t)setting.xPixels,
		(size_t)setting.yPixels,
		(CGRefreshRate)setting.frequency,
		NULL);

#ifdef GHOST_DEBUG
	printf("GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(): switching to:\n");
	printf("  setting.xPixels=%d\n", getValue(displayModeValues, kCGDisplayWidth));
	printf("  setting.yPixels=%d\n", getValue(displayModeValues, kCGDisplayHeight));
	printf("  setting.bpp=%d\n", getValue(displayModeValues, kCGDisplayBitsPerPixel));
	printf("  setting.frequency=%d\n", getValue(displayModeValues, kCGDisplayRefreshRate));
#endif // GHOST_DEBUG

	CGDisplayErr err = ::CGDisplaySwitchToMode(m_displayIDs[display], displayModeValues);
        
	return err == CGDisplayNoErr ? GHOST_kSuccess : GHOST_kFailure;
}


long GHOST_DisplayManagerCocoa::getValue(CFDictionaryRef values, CFStringRef key) const
{
    CFNumberRef numberValue = (CFNumberRef) CFDictionaryGetValue(values, key);
    
    if (!numberValue)
    {
        return -1;
    }
    
    long intValue;
    
    if (!CFNumberGetValue(numberValue, kCFNumberLongType, &intValue))
    {
        return -1;
    }
    
    return intValue;
}

