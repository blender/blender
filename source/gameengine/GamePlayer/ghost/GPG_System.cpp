/**
 * $Id$
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
 * Blender Player system on GHOST.
 */

#include "GPG_System.h"
#include <assert.h>
#include "GHOST_ISystem.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

GPG_System::GPG_System(GHOST_ISystem* system)
: m_system(system)
{
	assert(m_system);
}


double GPG_System::GetTimeInSeconds()
{
	GHOST_TInt64 millis = (GHOST_TInt64)m_system->getMilliSeconds();
	double time = (double)millis;
	time /= 1000.;
	return time;
}


