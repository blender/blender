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

/** \file SCA_IScene.h
 *  \ingroup gamelogic
 */

#ifndef __SCA_ISCENE_H__
#define __SCA_ISCENE_H__

#include <vector>

#include "STR_String.h"
#include "RAS_2DFilterManager.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

struct SCA_DebugProp
{
	class CValue*	m_obj;
	STR_String 		m_name;
	SCA_DebugProp();
	~SCA_DebugProp();
};

class SCA_IScene 
{
	std::vector<SCA_DebugProp*> m_debugList;
public:
	SCA_IScene();
	virtual ~SCA_IScene();
	virtual class SCA_IObject* AddReplicaObject(class CValue* gameobj,
												class CValue* locationobj,
												int lifespan=0)=0;
	virtual void	RemoveObject(class CValue* gameobj)=0;
	virtual void	DelayedRemoveObject(class CValue* gameobj)=0;
	//virtual void	DelayedReleaseObject(class CValue* gameobj)=0;
	
	virtual void	ReplaceMesh(class CValue* gameobj,
								void* meshobj, bool use_gfx, bool use_phys)=0;
	std::vector<SCA_DebugProp*>& GetDebugProperties();
	void			AddDebugProperty(class CValue* debugprop,
									 const STR_String &name);
	void			RemoveAllDebugProperties();
	virtual void	Update2DFilter(std::vector<STR_String>& propNames, void* gameObj, 
									RAS_2DFilterManager::RAS_2DFILTER_MODE filtermode, 
									int pass, STR_String& text) {}


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:SCA_IScene"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__SCA_ISCENE_H__

