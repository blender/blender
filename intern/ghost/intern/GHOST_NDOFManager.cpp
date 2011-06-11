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
 * Contributor(s):
 *   Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "GHOST_NDOFManager.h"
#include "GHOST_EventNDOF.h"
#include "GHOST_WindowManager.h"
#include <string.h> // for memory functions
#include <stdio.h> // for debug tracing

GHOST_NDOFManager::GHOST_NDOFManager(GHOST_System& sys)
	: m_system(sys)
	, m_buttons(0)
	, m_motionTime(1000) // one full second (operators should filter out such large time deltas)
	, m_prevMotionTime(0)
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

void GHOST_NDOFManager::updateButton(int button_number, bool press, GHOST_TUns64 time)
	{
	GHOST_IWindow* window = m_system.getWindowManager()->getActiveWindow();

	GHOST_EventNDOFButton* event = new GHOST_EventNDOFButton(time, window);
	GHOST_TEventNDOFButtonData* data = (GHOST_TEventNDOFButtonData*) event->getData();

	data->action = press ? GHOST_kPress : GHOST_kRelease;
	data->button = button_number + 1;

	printf("sending button %d %s\n", data->button, (data->action == GHOST_kPress) ? "pressed" : "released");

	m_system.pushEvent(event);

	unsigned short mask = 1 << button_number;
	if (press)
		m_buttons |= mask; // set this button's bit
	else
		m_buttons &= ~mask; // clear this button's bit
	}

void GHOST_NDOFManager::updateButtons(unsigned short button_bits, GHOST_TUns64 time)
	{
	GHOST_IWindow* window = m_system.getWindowManager()->getActiveWindow();

	unsigned short diff = m_buttons ^ button_bits;

	for (int i = 0; i < 16; ++i)
		{
		unsigned short mask = 1 << i;

		if (diff & mask)
			{
			GHOST_EventNDOFButton* event = new GHOST_EventNDOFButton(time, window);
			GHOST_TEventNDOFButtonData* data = (GHOST_TEventNDOFButtonData*) event->getData();
			
			data->action = (button_bits & mask) ? GHOST_kPress : GHOST_kRelease;
			data->button = i + 1;

			printf("sending button %d %s\n", data->button, (data->action == GHOST_kPress) ? "pressed" : "released");

			m_system.pushEvent(event);
			}
		}

	m_buttons = button_bits;
	}

bool GHOST_NDOFManager::sendMotionEvent()
	{
	if (m_atRest)
		return false;

	GHOST_IWindow* window = m_system.getWindowManager()->getActiveWindow();

	GHOST_EventNDOFMotion* event = new GHOST_EventNDOFMotion(m_motionTime, window);
	GHOST_TEventNDOFMotionData* data = (GHOST_TEventNDOFMotionData*) event->getData();

	const float scale = 1.f / 350.f; // SpaceNavigator sends +/- 350 usually
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

	m_system.pushEvent(event);

	// 'at rest' test goes at the end so that the first 'rest' event gets sent
	m_atRest = m_rotation[0] == 0 && m_rotation[1] == 0 && m_rotation[2] == 0 &&
		m_translation[0] == 0 && m_translation[1] == 0 && m_translation[2] == 0;
	// this needs to be aware of calibration -- 0.01 0.01 0.03 might be 'rest'

	return true;
	}

