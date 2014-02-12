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

#ifdef WITH_INPUT_NDOF

#include "GHOST_NDOFManagerX11.h"
#include "GHOST_SystemX11.h"
#include <spnav.h>
#include <stdio.h>


GHOST_NDOFManagerX11::GHOST_NDOFManagerX11(GHOST_System& sys)
	:
	GHOST_NDOFManager(sys),
	m_available(false)
{
	setDeadZone(0.1f); /* how to calibrate on Linux? throw away slight motion! */

	if (spnav_open() != -1) {
		m_available = true;

		/* determine exactly which device (if any) is plugged in */

#define MAX_LINE_LENGTH 100

		/* look for USB devices with Logitech's vendor ID */
		FILE *command_output = popen("lsusb -d 046d:", "r");
		if (command_output) {
			char line[MAX_LINE_LENGTH] = {0};
			while (fgets(line, MAX_LINE_LENGTH, command_output)) {
				unsigned short vendor_id = 0, product_id = 0;
				if (sscanf(line, "Bus %*d Device %*d: ID %hx:%hx", &vendor_id, &product_id) == 2)
					if (setDevice(vendor_id, product_id)) {
						break; /* stop looking once the first 3D mouse is found */
					}
			}
			pclose(command_output);
		}
	}
	else {
#ifdef DEBUG
		/* annoying for official builds, just adds noise and most prople don't own these */
		puts("ndof: spacenavd not found");
		/* This isn't a hard error, just means the user doesn't have a 3D mouse. */
#endif
	}
}

GHOST_NDOFManagerX11::~GHOST_NDOFManagerX11()
{
	if (m_available)
		spnav_close();
}

bool GHOST_NDOFManagerX11::available()
{
	return m_available;
}

bool GHOST_NDOFManagerX11::processEvents()
{
	bool anyProcessed = false;

	if (m_available) {
		spnav_event e;
		while (spnav_poll_event(&e)) {
			switch (e.type) {
				case SPNAV_EVENT_MOTION:
				{
					/* convert to blender view coords */
					GHOST_TUns64 now = m_system.getMilliSeconds();
					short t[3] = {(short)e.motion.x, (short)e.motion.y, (short)-e.motion.z};
					short r[3] = {(short)-e.motion.rx, (short)-e.motion.ry, (short)e.motion.rz};

					updateTranslation(t, now);
					updateRotation(r, now);
					break;
				}
				case SPNAV_EVENT_BUTTON:
					GHOST_TUns64 now = m_system.getMilliSeconds();
					updateButton(e.button.bnum, e.button.press, now);
					break;
			}
			anyProcessed = true;
		}
	}

	return anyProcessed;
}

#endif /* WITH_INPUT_NDOF */
