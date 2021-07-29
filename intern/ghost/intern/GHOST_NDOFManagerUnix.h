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
 
#ifndef __GHOST_NDOFMANAGERUNIX_H__
#define __GHOST_NDOFMANAGERUNIX_H__

#include "GHOST_NDOFManager.h"

/* Event capture is handled within the NDOF manager on Linux,
 * so there's no need for SystemX11 to look for them. */

class GHOST_NDOFManagerUnix : public GHOST_NDOFManager
{
public:
	GHOST_NDOFManagerUnix(GHOST_System&);
	~GHOST_NDOFManagerUnix();
	bool available();
	bool processEvents();

private:
	bool m_available;
};

#endif  /* __GHOST_NDOFMANAGERUNIX_H__ */
