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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_SCENE_HASH_H__
#define __FREESTYLE_SCENE_HASH_H__

/** \file blender/freestyle/intern/scene_graph/SceneHash.h
 *  \ingroup freestyle
 */

#include "IndexedFaceSet.h"
#include "SceneVisitor.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class SceneHash : public SceneVisitor
{
public:
	inline SceneHash() : SceneVisitor()
	{
		_hashcode = 0.0;
	}

	virtual ~SceneHash() {}

	VISIT_DECL(IndexedFaceSet)

	inline real getValue() {
		return _hashcode;
	}

	inline void reset() {
		_hashcode = 0.0;
	}

private:
	real _hashcode;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SceneHash")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_SCENE_HASH_H__
