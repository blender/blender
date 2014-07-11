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

/** \file gameengine/GameLogic/SCA_IScene.cpp
 *  \ingroup gamelogic
 */


#include "SCA_IScene.h"
#include "Value.h"

SCA_DebugProp::SCA_DebugProp(): m_obj(NULL)
{
}

SCA_DebugProp::~SCA_DebugProp()
{
	if (m_obj) 
		m_obj->Release(); 
}

SCA_IScene::SCA_IScene()
{
}

void SCA_IScene::RemoveAllDebugProperties()
{
	for (std::vector<SCA_DebugProp*>::iterator it = m_debugList.begin();
		!(it==m_debugList.end());++it)
	{
		delete (*it);
	}
	m_debugList.clear();
}


SCA_IScene::~SCA_IScene()
{
	RemoveAllDebugProperties();
}


std::vector<SCA_DebugProp*>& SCA_IScene::GetDebugProperties() 
{
	return m_debugList;
}


bool SCA_IScene::PropertyInDebugList( class CValue *gameobj, const STR_String &name )
{
	for (std::vector<SCA_DebugProp*>::iterator it = m_debugList.begin();
		!(it==m_debugList.end());++it) {
		STR_String debugname = (*it)->m_name;
		CValue *debugobj = (*it)->m_obj;

		if (debugobj == gameobj && debugname == name)
			return true;
	}
	return false;
}


bool SCA_IScene::ObjectInDebugList( class CValue *gameobj )
{
	for (std::vector<SCA_DebugProp*>::iterator it = m_debugList.begin();
		!(it==m_debugList.end());++it) {
		CValue* debugobj = (*it)->m_obj;

		if (debugobj == gameobj)
			return true;
	}
	return false;
}


void SCA_IScene::AddDebugProperty(class CValue* debugprop,
								  const STR_String &name)
{
	if (m_debugList.size() < DEBUG_MAX_DISPLAY) {
		SCA_DebugProp* dprop = new SCA_DebugProp();
		dprop->m_obj = debugprop;
		debugprop->AddRef();
		dprop->m_name = name;
		m_debugList.push_back(dprop);
	}
}


void SCA_IScene::RemoveDebugProperty(class CValue *gameobj,
								  const STR_String &name)
{
	vector<SCA_DebugProp*>::iterator it = m_debugList.begin();
	while(it != m_debugList.end()) {
		STR_String debugname = (*it)->m_name;
		CValue *debugobj = (*it)->m_obj;

		if (debugobj == gameobj && debugname == name) {
			delete (*it);
			m_debugList.erase(it);
			break;
		}
		++it;
	}
}


void SCA_IScene::RemoveObjectDebugProperties(class CValue* gameobj)
{	
	vector<SCA_DebugProp*>::iterator it = m_debugList.begin();
	while (it != m_debugList.end()) {
		CValue* debugobj = (*it)->m_obj;

		if (debugobj == gameobj) {
			delete (*it);
			m_debugList.erase(it);
			continue;
		}
		++it;
	}
}

