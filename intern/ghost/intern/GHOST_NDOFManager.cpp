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
 * Contributor(s): Mike Erwin, July 2010.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GHOST_NDOFManager.h"
#include "GHOST_EventNDOF.h"
#include <string.h> // for memory functions
#include <stdio.h> // for debug tracing

GHOST_NDOFManager::GHOST_NDOFManager(GHOST_System& sys)
	: m_system(sys)
	, m_buttons(0)
	, m_atRest(true)
	{
	// to avoid the rare situation where one triple is updated and
	// the other is not, initialize them both here:
	memset(m_translation, 0, sizeof(m_translation));
	memset(m_rotation, 0, sizeof(m_rotation));
	}

void GHOST_NDOFManager::updateTranslation(short t[3], GHOST_TUns64 time)
	{
	memcpy(m_translation, t, sizeof(m_translation));
	m_motionTime = time;
	m_atRest = false;
	}

void GHOST_NDOFManager::updateRotation(short r[3], GHOST_TUns64 time)
	{
	memcpy(m_rotation, r, sizeof(m_rotation));
	m_motionTime = time;
	m_atRest = false;
	}

void GHOST_NDOFManager::updateButtons(unsigned short buttons, GHOST_TUns64 time)
	{
	unsigned short diff = m_buttons ^ buttons;
	for (int i = 0; i < 16; ++i)
		{
		unsigned short mask = 1 << i;
		if (diff & mask)
			m_system.pushEvent(new GHOST_EventNDOFButton(time, i + 1,
				(buttons & mask) ? GHOST_kEventNDOFButtonDown : GHOST_kEventNDOFButtonUp));
		}

	m_buttons = buttons;
	}

bool GHOST_NDOFManager::sendMotionEvent()
	{
	if (m_atRest)
		return false;

	GHOST_EventNDOFMotion* event = new GHOST_EventNDOFMotion(m_motionTime);
	GHOST_TEventNDOFData* data = (GHOST_TEventNDOFData*) event->getData();

	const float scale = 1.f / 350.f; // SpaceNavigator sends +/- 350 usually
	// 350 according to their developer's guide; others recommend 500 as comfortable

	// possible future enhancement
	// scale *= m_sensitivity;

	data->tx = scale * m_translation[0];
	data->ty = scale * m_translation[1];
	data->tz = scale * m_translation[2];

	data->rx = scale * m_rotation[0];
	data->ry = scale * m_rotation[1];
	data->rz = scale * m_rotation[2];

	printf("sending T=(%.2f,%.2f,%.2f) R=(%.2f,%.2f,%.2f)\n", data->tx, data->ty, data->tz, data->rx, data->ry, data->rz);

	m_system.pushEvent(event);

	// 'at rest' test goes at the end so that the first 'rest' event gets sent
	m_atRest = m_rotation[0] == 0 && m_rotation[1] == 0 && m_rotation[2] == 0 &&
		m_translation[0] == 0 && m_translation[1] == 0 && m_translation[2] == 0;
	
	return true;
	}
