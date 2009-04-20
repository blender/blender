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
 * Timing routine taken and modified from KX_BlenderSystem.cpp
 */

#ifndef _GPW_SYSTEM_H_
#define _GPW_SYSTEM_H_

#pragma warning (disable:4786) // suppress stl-MSVC debug info warning

#include "GPC_System.h"

#if defined(__CYGWIN32__)
#	define __int64 long long
#endif

class GPW_System : public GPC_System
{
public:
	GPW_System();

	virtual double GetTimeInSeconds();
protected:

  __int64 m_freq;
  __int64 m_lastCount;
  __int64 m_lastRest;
  long    m_lastTime;

};

#endif //_GPW_SYSTEM_H_

