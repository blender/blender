/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	September 21, 2001
 */

#include "GHOST_DisplayManager.h"
#include "GHOST_Debug.h"


GHOST_DisplayManager::GHOST_DisplayManager(
	void)
: m_settingsInitialized(false)
{
}


GHOST_DisplayManager::~GHOST_DisplayManager(void)
{
}


GHOST_TSuccess
GHOST_DisplayManager::initialize(
	void)
{
	GHOST_TSuccess success;
	if (!m_settingsInitialized) {
		success = initializeSettings();
		m_settingsInitialized = true;
	}
	else {
		success = GHOST_kSuccess;
	}
	return success;
}


GHOST_TSuccess
GHOST_DisplayManager::getNumDisplays(
	GHOST_TUns8& /*numDisplays*/) const
{
	// Don't know if we have a display...
	return GHOST_kFailure;
}


GHOST_TSuccess
GHOST_DisplayManager::getNumDisplaySettings(
	GHOST_TUns8 display, 
	GHOST_TInt32& numSettings) const
{
	GHOST_TSuccess success;

	GHOST_ASSERT(m_settingsInitialized, "GHOST_DisplayManager::getNumDisplaySettings(): m_settingsInitialized=false");
	GHOST_TUns8 numDisplays;
	success = getNumDisplays(numDisplays);
	if (success == GHOST_kSuccess) {
		if (display < numDisplays) {
			numSettings = m_settings[display].size();
		}
		else {
			success = GHOST_kFailure;
		}
	}
	return success;
}


GHOST_TSuccess
GHOST_DisplayManager::getDisplaySetting(
	GHOST_TUns8 display, 
	GHOST_TInt32 index, 
	GHOST_DisplaySetting& setting) const
{
	GHOST_TSuccess success;

	GHOST_ASSERT(m_settingsInitialized, "GHOST_DisplayManager::getNumDisplaySettings(): m_settingsInitialized=false");
	GHOST_TUns8 numDisplays;
	success = getNumDisplays(numDisplays);
	if (success == GHOST_kSuccess) {
		if (display < numDisplays && ((GHOST_TUns8)index < m_settings[display].size())) {
			setting = m_settings[display][index];
		}
		else {
			success = GHOST_kFailure;
		}
	}
	return success;
}


GHOST_TSuccess
GHOST_DisplayManager::getCurrentDisplaySetting(
	GHOST_TUns8 /*display*/,
	GHOST_DisplaySetting& /*setting*/) const
{
	return GHOST_kFailure;
}


GHOST_TSuccess
GHOST_DisplayManager::setCurrentDisplaySetting(
	GHOST_TUns8 /*display*/,
	const GHOST_DisplaySetting& /*setting*/)
{
	return GHOST_kFailure;
}


GHOST_TSuccess
GHOST_DisplayManager::findMatch(
	GHOST_TUns8 display, 
	const GHOST_DisplaySetting& setting, 
	GHOST_DisplaySetting& match) const
{
	GHOST_TSuccess success = GHOST_kSuccess;
	GHOST_ASSERT(m_settingsInitialized, "GHOST_DisplayManager::findMatch(): m_settingsInitialized=false");

	int criteria[4] = { setting.xPixels, setting.yPixels, setting.bpp, setting.frequency };
	int capabilities[4];
	double field, score;
	double best = 1e12; // A big number
	int found = 0;

	// Look at all the display modes
	for (int i = 0; (i < (int)m_settings[display].size()); i++) {
		// Store the capabilities of the display device
		capabilities[0] = m_settings[display][i].xPixels;
		capabilities[1] = m_settings[display][i].yPixels;
		capabilities[2] = m_settings[display][i].bpp;
		capabilities[3] = m_settings[display][i].frequency;

		// Match against all the fields of the display settings
		score = 0;
		for (int j = 0; j < 4; j++) {
			field = capabilities[j] - criteria[j];
			score += field * field;
		}

		if (score < best) {
			found = i;
			best = score;
		}
	}
	
	match = m_settings[display][found];
	
	GHOST_PRINT("GHOST_DisplayManager::findMatch(): settings of match:\n");
	GHOST_PRINT("  setting.xPixels=" << match.xPixels << "\n");
	GHOST_PRINT("  setting.yPixels=" << match.yPixels << "\n");
	GHOST_PRINT("  setting.bpp=" << match.bpp << "\n");
	GHOST_PRINT("  setting.frequency=" << match.frequency << "\n");

	return success;
}


GHOST_TSuccess
GHOST_DisplayManager::initializeSettings(
	void)
{
	GHOST_TUns8 numDisplays;
	GHOST_TSuccess success = getNumDisplays(numDisplays);
	if (success == GHOST_kSuccess) {
		for (GHOST_TUns8 display = 0; (display < numDisplays) && (success == GHOST_kSuccess); display++) {
			GHOST_DisplaySettings displaySettings;
			m_settings.push_back(displaySettings);
			GHOST_TInt32 numSettings;
			success = getNumDisplaySettings(display, numSettings);
			if (success == GHOST_kSuccess) {
				GHOST_TInt32 index;
				GHOST_DisplaySetting setting;
				for (index = 0; (index < numSettings) && (success == GHOST_kSuccess); index++) {
					success = getDisplaySetting(display, index, setting);
					m_settings[display].push_back(setting);
				}
			}
			else {
				break;
			}
		}
	}
	return success;
}
