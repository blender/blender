/*
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
#ifndef _GHOST_NDOFMANAGER_H_
#define _GHOST_NDOFMANAGER_H_

#include "GHOST_System.h"


class GHOST_NDOFManager
{
public:
	GHOST_NDOFManager(GHOST_System&);

	virtual ~GHOST_NDOFManager() {};

	// whether multi-axis functionality is available (via the OS or driver)
	// does not imply that a device is plugged in or being used
	virtual bool available() = 0;

	// the latest raw data from the device
	void updateTranslation(short t[3], GHOST_TUns64 time);
	void updateRotation(short r[3], GHOST_TUns64 time);
	// this one sends events immediately for changed buttons
	void updateButtons(unsigned short b, GHOST_TUns64 time);

	// processes most recent raw data into an NDOFMotion event and sends it
	// returns whether an event was sent
	virtual bool sendMotionEvent();

protected:
	GHOST_System& m_system;

	short m_translation[3];
	short m_rotation[3];
	unsigned short m_buttons;

	GHOST_TUns64 m_motionTime;
	GHOST_TUns64 m_prevMotionTime; // time of most recent Motion event sent
	bool m_atRest;

	void updateMotionTime(GHOST_TUns64 t);
	void resetMotion();
};


#endif
