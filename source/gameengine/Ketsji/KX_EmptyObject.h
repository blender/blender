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

/** \file KX_EmptyObject.h
 *  \ingroup ketsji
 */

#ifndef __KX_EMPTYOBJECT_H__
#define  __KX_EMPTYOBJECT_H__
#include "KX_GameObject.h"

class KX_EmptyObject : public KX_GameObject
{
public:
	KX_EmptyObject(void* sgReplicationInfo,SG_Callbacks callbacks) :
	KX_GameObject(sgReplicationInfo,callbacks) 
	{};
	virtual ~KX_EmptyObject();


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:KX_EmptyObject")
#endif
};

#endif  /* __KX_EMPTYOBJECT_H__ */
