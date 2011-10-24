/*
 * 
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

#ifdef WITH_INPUT_NDOF // use contents of this file

#include "GHOST_NDOFManagerWin32.h"


GHOST_NDOFManagerWin32::GHOST_NDOFManagerWin32(GHOST_System& sys)
    : GHOST_NDOFManager(sys)
{
	setDeadZone(0.1f);
}

// whether multi-axis functionality is available (via the OS or driver)
// does not imply that a device is plugged in or being used
bool GHOST_NDOFManagerWin32::available()
{
	// always available since RawInput is built into Windows
	return true;
}

#endif // WITH_INPUT_NDOF
