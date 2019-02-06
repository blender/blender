/*
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
 */

/** \file \ingroup freestyle
 *  \brief Class to display textual information about a scene graph.
 */

#include <iomanip>

#include "IndexedFaceSet.h"
#include "ScenePrettyPrinter.h"

namespace Freestyle {

#define VISIT(CLASS)                              \
	void ScenePrettyPrinter::visit##CLASS(CLASS&) \
	{                                             \
		_ofs << _space << #CLASS << endl;         \
	}

VISIT(Node)
VISIT(NodeShape)
VISIT(NodeGroup)
VISIT(NodeLight)
VISIT(NodeDrawingStyle)
VISIT(NodeTransform)

void ScenePrettyPrinter::visitNodeShapeBefore(NodeShape&)
{
	increaseSpace();
}

void ScenePrettyPrinter::visitNodeShapeAfter(NodeShape&)
{
	decreaseSpace();
}

void ScenePrettyPrinter::visitNodeGroupBefore(NodeGroup&)
{
	increaseSpace();
}

void ScenePrettyPrinter::visitNodeGroupAfter(NodeGroup&)
{
	decreaseSpace();
}

void ScenePrettyPrinter::visitNodeDrawingStyleBefore(NodeDrawingStyle&)
{
	increaseSpace();
}

void ScenePrettyPrinter::visitNodeDrawingStyleAfter(NodeDrawingStyle&)
{
	decreaseSpace();
}

void ScenePrettyPrinter::visitNodeTransformBefore(NodeTransform&)
{
	increaseSpace();
}

void ScenePrettyPrinter::visitNodeTransformAfter(NodeTransform&)
{
	decreaseSpace();
}

VISIT(LineRep)
VISIT(OrientedLineRep)
VISIT(TriangleRep)
VISIT(VertexRep)

void ScenePrettyPrinter::visitIndexedFaceSet(IndexedFaceSet& ifs)
{
	const float *vertices = ifs.vertices();
	unsigned vsize = ifs.vsize();

	_ofs << _space << "IndexedFaceSet" << endl;
	const float *p = vertices;
	for (unsigned int i = 0; i < vsize / 3; i++) {
		_ofs << _space << "  " << setw(3) << setfill('0') << i << ": "  << p[0] << ", " << p[1] << ", " << p[2] << endl;
		p += 3;
	}
}

} /* namespace Freestyle */
