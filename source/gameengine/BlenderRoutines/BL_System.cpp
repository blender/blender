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
 * Interface to the commandline arguments
 */

/** \file gameengine/BlenderRoutines/BL_System.cpp
 *  \ingroup blroutines
 */

#include "CTR_Map.h"
#include "STR_HashedString.h"
#include "BL_System.h"

struct SingletonSystem {
	CTR_Map<STR_HashedString,int> int_params;
	CTR_Map<STR_HashedString,float> float_params;
	CTR_Map<STR_HashedString,STR_String> string_params;
};

static SingletonSystem *_system_instance = NULL;

SYS_SystemHandle SYS_GetSystem()
{
	if (!_system_instance)
		_system_instance = new SingletonSystem();

	return (SYS_SystemHandle)_system_instance;
}

void SYS_DeleteSystem(SYS_SystemHandle sys)
{
	if (_system_instance) {
		delete _system_instance;
		_system_instance = NULL;
	}
}

int SYS_GetCommandLineInt(SYS_SystemHandle sys, const char *paramname, int defaultvalue)
{
	int *result = ((SingletonSystem *)sys)->int_params[paramname];
	if (result)
		return *result;

	return defaultvalue;
}

float SYS_GetCommandLineFloat(SYS_SystemHandle sys, const char *paramname, float defaultvalue)
{
	float *result = ((SingletonSystem *)sys)->float_params[paramname];
	if (result)
		return *result;

	return defaultvalue;
}

const char *SYS_GetCommandLineString(SYS_SystemHandle sys, const char *paramname, const char *defaultvalue)
{
	STR_String *result = ((SingletonSystem *)sys)->string_params[paramname];
	if (result)
		return *result;

	return defaultvalue;
}

void SYS_WriteCommandLineInt(SYS_SystemHandle sys, const char *paramname, int value)
{
	((SingletonSystem *)sys)->int_params.insert(paramname, value);
}

void SYS_WriteCommandLineFloat(SYS_SystemHandle sys, const char *paramname, float value)
{
	((SingletonSystem *)sys)->float_params.insert(paramname, value);
}

void SYS_WriteCommandLineString(SYS_SystemHandle sys, const char *paramname, const char *value)
{
	((SingletonSystem *)sys)->string_params.insert(paramname, value);
}

