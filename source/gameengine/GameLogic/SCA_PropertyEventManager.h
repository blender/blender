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

/** \file SCA_PropertyEventManager.h
 *  \ingroup gamelogic
 */

#ifndef __SCA_PROPERTYEVENTMANAGER_H__
#define __SCA_PROPERTYEVENTMANAGER_H__

#include "SCA_EventManager.h"

#include <vector>

using namespace std;

class SCA_PropertyEventManager : public SCA_EventManager
{
public:
	SCA_PropertyEventManager(class SCA_LogicManager* logicmgr);
	virtual ~SCA_PropertyEventManager();
	virtual void NextFrame();
	//SCA_LogicManager* GetLogicManager() { return m_logicmgr;}


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:SCA_PropertyEventManager")
#endif
};

#endif //__SCA_PROPERTYEVENTMANAGER_H__

