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
 * Unique instance of system class for system specific information / access
 * Used by SYS_System
 */
#ifndef __SINGLETONSYSTEM_H
#define __SINGLETONSYSTEM_H

#include "GEN_Map.h"
#include "STR_HashedString.h"
#include "GEN_DataCache.h"

class SYS_SingletonSystem
{
public:
	static		SYS_SingletonSystem*	Instance();
	static		void	Destruct();

	int		SYS_GetCommandLineInt(const char* paramname,int defaultvalue);
	float		SYS_GetCommandLineFloat(const char* paramname,float defaultvalue);
	const char*	SYS_GetCommandLineString(const char* paramname,const char* defaultvalue);

	void		SYS_WriteCommandLineInt(const char* paramname,int value);
	void		SYS_WriteCommandLineFloat(const char* paramname,float value);
	void		SYS_WriteCommandLineString(const char* paramname,const char* value);

	SYS_SingletonSystem();

private:
	static SYS_SingletonSystem*	_instance;
	GEN_Map<STR_HashedString,int>	m_int_commandlineparms;
	GEN_Map<STR_HashedString,float>	m_float_commandlineparms;
	GEN_Map<STR_HashedString,STR_String>	m_string_commandlineparms;
};

#endif //__SINGLETONSYSTEM_H

