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

#ifndef __FREESTYLE_STROKE_TESSELATOR_H__
#define __FREESTYLE_STROKE_TESSELATOR_H__

/** \file blender/freestyle/intern/stroke/StrokeTesselator.h
 *  \ingroup freestyle
 *  \brief Class to build a Node Tree designed to be displayed from a set of strokes structure.
 *  \author Stephane Grabli
 *  \date 26/03/2002
 */

#include "Stroke.h"

#include "../scene_graph/LineRep.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class StrokeTesselator
{
public:
	inline StrokeTesselator()
	{
		_FrsMaterial.setDiffuse(0, 0, 0, 1);
		_overloadFrsMaterial = false;
	}

	virtual ~StrokeTesselator() {}

	/*! Builds a line rep contained from a Stroke */
	LineRep *Tesselate(Stroke *iStroke);

	/*! Builds a set of lines rep contained under a a NodeShape, itself contained under a NodeGroup
	 *  from a set of strokes.
	 */
	template<class StrokeIterator>
	NodeGroup *Tesselate(StrokeIterator begin, StrokeIterator end);

	inline void setFrsMaterial(const FrsMaterial& iMaterial)
	{
		_FrsMaterial = iMaterial;
		_overloadFrsMaterial = true;
	}

	inline const FrsMaterial& frs_material() const
	{
		return _FrsMaterial;
	}

private:
	FrsMaterial _FrsMaterial;
	bool _overloadFrsMaterial;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:StrokeTesselator")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_STROKE_TESSELATOR_H__
