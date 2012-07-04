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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ghost/intern/GHOST_ISystemPaths.cpp
 *  \ingroup GHOST
 */


/**
 * Copyright (C) 2001 NaN Technologies B.V.
 * @author	Maarten Gribnau
 * @date	May 7, 2001
 */

#include "GHOST_ISystemPaths.h"

#ifdef WIN32
#  include "GHOST_SystemPathsWin32.h"
#else
#  ifdef __APPLE__
#    ifdef GHOST_COCOA
#      include "GHOST_SystemPathsCocoa.h"
#    else
#      include "GHOST_SystemPathsCarbon.h"
#    endif
#  else
#    include "GHOST_SystemPathsX11.h"
#  endif
#endif


GHOST_ISystemPaths *GHOST_ISystemPaths::m_systemPaths = 0;


GHOST_TSuccess GHOST_ISystemPaths::create()
{
	GHOST_TSuccess success;
	if (!m_systemPaths) {
#ifdef WIN32
		m_systemPaths = new GHOST_SystemPathsWin32();
#else
#  ifdef __APPLE__
#    ifdef GHOST_COCOA
		m_systemPaths = new GHOST_SystemPathsCocoa();
#    else
		m_systemPaths = new GHOST_SystemPathsCarbon();
#    endif
#  else
		m_systemPaths = new GHOST_SystemPathsX11();
#  endif
#endif 
		success = m_systemPaths != 0 ? GHOST_kSuccess : GHOST_kFailure;
	}
	else {
		success = GHOST_kFailure;
	}
	return success;
}

GHOST_TSuccess GHOST_ISystemPaths::dispose()
{
	GHOST_TSuccess success = GHOST_kSuccess;
	if (m_systemPaths) {
		delete m_systemPaths;
		m_systemPaths = 0;
	}
	else {
		success = GHOST_kFailure;
	}
	return success;
}

GHOST_ISystemPaths *GHOST_ISystemPaths::get()
{
	if (!m_systemPaths) {
		create();
	}
	return m_systemPaths;
}



