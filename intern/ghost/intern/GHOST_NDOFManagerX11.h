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
 
#ifndef _GHOST_NDOFMANAGERX11_H_
#define _GHOST_NDOFMANAGERX11_H_

#include "GHOST_NDOFManager.h"
#include "GHOST_Types.h"
#include "GHOST_WindowX11.h"
#include "GHOST_EventNDOF.h"
#include <X11/Xlib.h>
#include <stdio.h>

class GHOST_NDOFManagerX11 : public GHOST_NDOFManager
{
GHOST_WindowX11 * m_ghost_window_x11;

public:
	GHOST_NDOFManagerX11(GHOST_System& sys)
		: GHOST_NDOFManager(sys)
		{}

	void setGHOSTWindowX11(GHOST_WindowX11 * w){
	    if (m_ghost_window_x11 == NULL)
		m_ghost_window_x11 = w;
	}

	GHOST_WindowX11 * getGHOSTWindowX11(){
	    return m_ghost_window_x11;
	}	

	// whether multi-axis functionality is available (via the OS or driver)
	// does not imply that a device is plugged in or being used
	bool available()
		{
		// never available since I've not yet written it!
		return true;
		}

	virtual bool sendMotionEvent()
		{
		if (m_atRest)
			return false;		

		GHOST_EventNDOFMotion* event = new GHOST_EventNDOFMotion(m_motionTime, getGHOSTWindowX11());
		GHOST_TEventNDOFMotionData* data = (GHOST_TEventNDOFMotionData*) event->getData();

		const float scale = 1.f/350.f; // SpaceNavigator sends +/- 350 usually
		// 350 according to their developer's guide; others recommend 500 as comfortable

		// possible future enhancement
		// scale *= m_sensitivity;

		data->tx = -scale * m_translation[0];
		data->ty = scale * m_translation[1];
		data->tz = scale * m_translation[2];

		data->rx = scale * m_rotation[0];
		data->ry = scale * m_rotation[1];
		data->rz = scale * m_rotation[2];

		data->dt = 0.001f * (m_motionTime - m_prevMotionTime); // in seconds

		m_prevMotionTime = m_motionTime;

		printf("sending T=(%.2f,%.2f,%.2f) R=(%.2f,%.2f,%.2f) dt=%.3f\n",
			data->tx, data->ty, data->tz, data->rx, data->ry, data->rz, data->dt);

		if (!m_system.pushEvent(event))
		    return false;

		// 'at rest' test goes at the end so that the first 'rest' event gets sent
		m_atRest = m_rotation[0] == 0 && m_rotation[1] == 0 && m_rotation[2] == 0 &&
			m_translation[0] == 0 && m_translation[1] == 0 && m_translation[2] == 0;

		return true;
		}
};


#endif
