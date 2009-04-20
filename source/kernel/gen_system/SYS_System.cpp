/*
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
 * System specific information / access.
 * Interface to the commandline arguments
 */
#include "SYS_System.h"
#include "SYS_SingletonSystem.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SYS_SystemHandle SYS_GetSystem()
{
	return (SYS_SystemHandle) SYS_SingletonSystem::Instance();
}

void SYS_DeleteSystem(SYS_SystemHandle sys)
{
	if (sys) {
		((SYS_SingletonSystem *) sys)->Destruct();
	}
}

int SYS_GetCommandLineInt(SYS_SystemHandle sys, const char *paramname, int defaultvalue)
{
	return ((SYS_SingletonSystem *) sys)->SYS_GetCommandLineInt(paramname, defaultvalue);
}

float SYS_GetCommandLineFloat(SYS_SystemHandle sys, const char *paramname, float defaultvalue)
{
	return ((SYS_SingletonSystem *) sys)->SYS_GetCommandLineFloat(paramname, defaultvalue);
}

const char *SYS_GetCommandLineString(SYS_SystemHandle sys, const char *paramname, const char *defaultvalue)
{
	return ((SYS_SingletonSystem *) sys)->SYS_GetCommandLineString(paramname, defaultvalue);
}

void SYS_WriteCommandLineInt(SYS_SystemHandle sys, const char *paramname, int value)
{
	((SYS_SingletonSystem *) sys)->SYS_WriteCommandLineInt(paramname, value);
}

void SYS_WriteCommandLineFloat(SYS_SystemHandle sys, const char *paramname, float value)
{
	((SYS_SingletonSystem *) sys)->SYS_WriteCommandLineFloat(paramname, value);
}

void SYS_WriteCommandLineString(SYS_SystemHandle sys, const char *paramname, const char *value)
{
	((SYS_SingletonSystem *) sys)->SYS_WriteCommandLineString(paramname, value);
}
