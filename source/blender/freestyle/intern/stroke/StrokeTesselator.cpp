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

/** \file blender/freestyle/intern/stroke/StrokeTesselator.cpp
 *  \ingroup freestyle
 *  \brief Class to build a Node Tree designed to be displayed from a set of strokes structure.
 *  \author Stephane Grabli
 *  \date 26/03/2002
 */

#include "StrokeAdvancedIterators.h"
#include "StrokeTesselator.h"

#include "../scene_graph/OrientedLineRep.h"
#include "../scene_graph/NodeGroup.h"
#include "../scene_graph/NodeShape.h"

namespace Freestyle {

LineRep *StrokeTesselator::Tesselate(Stroke *iStroke)
{
	if (0 == iStroke)
		return 0;

	LineRep *line;
	line = new OrientedLineRep();

	Stroke::vertex_iterator v, vend;
	if (2 == iStroke->vertices_size()) {
		line->setStyle(LineRep::LINES);
		v = iStroke->vertices_begin();
		StrokeVertex *svA = (*v);
		v++;
		StrokeVertex *svB = (*v);
		Vec3r A((*svA)[0], (*svA)[1], 0);
		Vec3r B((*svB)[0], (*svB)[1], 0);
		line->AddVertex(A);
		line->AddVertex(B);
	}
	else {
		if (_overloadFrsMaterial)
			line->setFrsMaterial(_FrsMaterial);

		line->setStyle(LineRep::LINE_STRIP);

		for (v = iStroke->vertices_begin(), vend = iStroke->vertices_end(); v != vend; v++) {
			StrokeVertex *sv = (*v);
			Vec3r V((*sv)[0], (*sv)[1], 0);
			line->AddVertex(V);
		}
	}
	line->setId(iStroke->getId());
	line->ComputeBBox();

	return line;
}

template<class StrokeVertexIterator>
NodeGroup *StrokeTesselator::Tesselate(StrokeVertexIterator begin, StrokeVertexIterator end)
{
	NodeGroup *group = new NodeGroup;
	NodeShape *tshape = new NodeShape;
	group->AddChild(tshape);
	//tshape->material().setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
	tshape->setFrsMaterial(_FrsMaterial);

	for (StrokeVertexIterator c = begin, cend = end; c != cend; c++) {
		tshape->AddRep(Tesselate((*c)));
	}

	return group;
}

} /* namespace Freestyle */
