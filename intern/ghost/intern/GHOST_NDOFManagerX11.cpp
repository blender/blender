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
 
#include "GHOST_NDOFManagerX11.h"
#include "GHOST_SystemX11.h"
#include <spnav.h>
#include <stdio.h>


GHOST_NDOFManagerX11::GHOST_NDOFManagerX11(GHOST_System& sys)
	: GHOST_NDOFManager(sys)
	{
	if (spnav_open() != -1)
		{
		m_available = true;
		}
	else
		{
		printf("<!> SpaceNav driver not found\n");
		// This isn't a hard error, just means the user doesn't have a SpaceNavigator.
		m_available = false;
		}
	}

GHOST_NDOFManagerX11::~GHOST_NDOFManagerX11()
	{
	if (m_available)
		{
		spnav_remove_events(SPNAV_EVENT_ANY); // ask nuclear if this is needed
		spnav_close();
		}
	}

bool GHOST_NDOFManagerX11::available()
	{
	return m_available;
	}

bool GHOST_NDOFManagerX11::processEvents()
	{
	GHOST_TUns64 now = m_system.getMilliSeconds();

	bool anyProcessed = false;
	spnav_event e;
	while (spnav_poll_event(&e))
		{
		switch (e.type)
			{
			case SPNAV_EVENT_MOTION:
				{
				short t[3] = {e.motion.x, e.motion.y, e.motion.z};
				short r[3] = {e.motion.rx, e.motion.ry, e.motion.rz};

				updateTranslation(t, now);
				updateRotation(r, now);
				break;
				}
			case SPNAV_EVENT_BUTTON:
				updateButton(e.button.bnum, e.button.press, now);
				break;
			}
		anyProcessed = true;
		}
	return anyProcessed;
	}
