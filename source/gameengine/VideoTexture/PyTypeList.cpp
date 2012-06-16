/*
-----------------------------------------------------------------------------
This source file is part of blendTex library

Copyright (c) 2007 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

/** \file gameengine/VideoTexture/PyTypeList.cpp
 *  \ingroup bgevideotex
 */

#include "PyTypeList.h"

#include <memory>
#include <vector>

#include "PyObjectPlus.h"

/// destructor
PyTypeList::~PyTypeList()
{
	// if list exists
	if (m_list.get() != NULL)
		for (PyTypeListType::iterator it = m_list->begin(); it != m_list->end(); ++it)
			delete *it;
}

/// check, if type is in list
bool PyTypeList::in (PyTypeObject * type)
{
	// if list exists
	if (m_list.get() != NULL)
		// iterate items in list
		for (PyTypeListType::iterator it = m_list->begin(); it != m_list->end(); ++it)
			// if item is found, return with success
			if ((*it)->getType() == type) return true;
	// otherwise return not found
	return false;
}

/// add type to list
void PyTypeList::add (PyTypeObject * type, const char * name)
{
	// if list doesn't exist, create it
	if (m_list.get() == NULL) 
		m_list.reset(new PyTypeListType());
	if (!in(type))
		// add new item to list
		m_list->push_back(new PyTypeListItem(type, name));
}

/// prepare types
bool PyTypeList::ready (void)
{
	// if list exists
	if (m_list.get() != NULL)
		// iterate items in list
		for (PyTypeListType::iterator it = m_list->begin(); it != m_list->end(); ++it)
			// if preparation failed, report it
			if (PyType_Ready((*it)->getType()) < 0) return false;
	// success
	return true;
}

/// register types to module
void PyTypeList::reg (PyObject * module)
{
	// if list exists
	if (m_list.get() != NULL)
		// iterate items in list
		for (PyTypeListType::iterator it = m_list->begin(); it != m_list->end(); ++it)
		{
			// increase ref count
			Py_INCREF((*it)->getType());
			// add type to module
			PyModule_AddObject(module, (char*)(*it)->getName(), (PyObject*)(*it)->getType());
		}
}
