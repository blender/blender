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

/** \file blender/freestyle/intern/winged_edge/WXEdge.cpp
 *  \ingroup freestyle
 *  \brief Classes to define an Extended Winged Edge data structure.
 *  \author Stephane Grabli
 *  \date 26/10/2003
 */

#include "WXEdge.h"
#include "BKE_global.h"

namespace Freestyle {

/**********************************
 *                                *
 *                                *
 *             WXFace             *
 *                                *
 *                                *
 **********************************/

unsigned int WXFaceLayer::Get0VertexIndex() const
{
	int i = 0;
	int nEdges = _pWXFace->numberOfEdges();
	for (i = 0; i < nEdges; ++i) {
		if (_DotP[i] == 0) {
			return i;
		}
	}
	return -1;
}
unsigned int WXFaceLayer::GetSmoothEdgeIndex() const
{
	int i = 0;
	int nEdges = _pWXFace->numberOfEdges();
	for (i = 0; i < nEdges; ++i) {
		if ((_DotP[i] == 0) && (_DotP[(i + 1) % nEdges] == 0)) {
			return i;
		}
	}
	return -1;
}

void WXFaceLayer::RetrieveCuspEdgesIndices(vector<int>& oCuspEdges)
{
	int i = 0;
	int nEdges = _pWXFace->numberOfEdges();
	for (i = 0; i < nEdges; ++i) {
		if (_DotP[i] * _DotP[(i + 1) % nEdges] < 0) {
			// we got one
			oCuspEdges.push_back(i);
		}
	}
}

WXSmoothEdge *WXFaceLayer::BuildSmoothEdge()
{
	// if the smooth edge has already been built: exit
	if (_pSmoothEdge)
		return _pSmoothEdge;
	real ta, tb;
	WOEdge *woea(0), *woeb(0);
	bool ok = false;
	vector<int> cuspEdgesIndices;
	int indexStart, indexEnd;
	unsigned nedges = _pWXFace->numberOfEdges();
	if (_nNullDotP == nedges) {
		_pSmoothEdge = NULL;
		return _pSmoothEdge;
	}
	if ((_nPosDotP != 0) && (_nPosDotP != _DotP.size()) && (_nNullDotP == 0)) {
		// that means that we have a smooth edge that starts from an edge and ends at an edge
		//-----------------------------
		// We retrieve the 2 edges for which we have opposite signs for each extremity
		RetrieveCuspEdgesIndices(cuspEdgesIndices);
		if (cuspEdgesIndices.size() != 2) // we necessarly have 2 cusp edges
			return 0;

		// let us determine which cusp edge corresponds to the starting:
		// We can do that because we defined that a silhouette edge had the back facing part on its right.
		// So if the WOEdge woea is such that woea[0].dotp > 0 and woea[1].dotp < 0, it is the starting edge.
		//-------------------------------------------

		if (_DotP[cuspEdgesIndices[0]] > 0) {
			woea = _pWXFace->GetOEdge(cuspEdgesIndices[0]);
			woeb = _pWXFace->GetOEdge(cuspEdgesIndices[1]);
			indexStart = cuspEdgesIndices[0];
			indexEnd = cuspEdgesIndices[1];
		}
		else {
			woea = _pWXFace->GetOEdge(cuspEdgesIndices[1]);
			woeb = _pWXFace->GetOEdge(cuspEdgesIndices[0]);
			indexStart = cuspEdgesIndices[1];
			indexEnd = cuspEdgesIndices[0];
		}

		// Compute the interpolation:
		ta = _DotP[indexStart] / (_DotP[indexStart] - _DotP[(indexStart + 1) % nedges]);
		tb = _DotP[indexEnd] / (_DotP[indexEnd] - _DotP[(indexEnd + 1) % nedges]);
		ok = true;
	}
	else if (_nNullDotP == 1) {
		// that means that we have exactly one of the 2 extremities of our silhouette edge is a vertex of the mesh
		if ((_nPosDotP == 2) || (_nPosDotP == 0)) {
			_pSmoothEdge = NULL;
			return _pSmoothEdge;
		}
		RetrieveCuspEdgesIndices(cuspEdgesIndices);
		// We should have only one EdgeCusp:
		if (cuspEdgesIndices.size() != 1) {
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << "Warning in BuildSmoothEdge: weird WXFace configuration" << endl;
			}
			_pSmoothEdge = NULL;
			return NULL;
		}
		unsigned index0 = Get0VertexIndex(); // retrieve the 0 vertex index
		unsigned nedges = _pWXFace->numberOfEdges();
		if (_DotP[cuspEdgesIndices[0]] > 0) {
			woea = _pWXFace->GetOEdge(cuspEdgesIndices[0]);
			woeb = _pWXFace->GetOEdge(index0);
			indexStart = cuspEdgesIndices[0];
			ta = _DotP[indexStart] / (_DotP[indexStart] - _DotP[(indexStart + 1) % nedges]);
			tb = 0.0;
		}
		else {
			woea = _pWXFace->GetOEdge(index0);
			woeb = _pWXFace->GetOEdge(cuspEdgesIndices[0]);
			indexEnd = cuspEdgesIndices[0];
			ta = 0.0;
			tb = _DotP[indexEnd] / (_DotP[indexEnd] - _DotP[(indexEnd + 1) % nedges]);
		}
		ok = true;
	}
	else if (_nNullDotP == 2) {
		// that means that the silhouette edge is an edge of the mesh
		int index = GetSmoothEdgeIndex();
		if (!_pWXFace->front()) {  // is it in the right order ?
			// the order of the WOEdge index is wrong
			woea = _pWXFace->GetOEdge((index + 1) % nedges);
			woeb = _pWXFace->GetOEdge((index - 1) % nedges);
			ta = 0;
			tb = 1;
			ok = true;
		}
		else {
			// here it's not good, our edge is a single point -> skip that face
			ok = false;
#if 0
			// the order of the WOEdge index is good
			woea = _pWXFace->GetOEdge((index - 1) % nedges);
			woeb = _pWXFace->GetOEdge((index + 1) % nedges);
			ta = 1;
			tb = 0;
#endif
		}
	}
	if (ok) {
		_pSmoothEdge = new WXSmoothEdge;
		_pSmoothEdge->setWOeA(woea);
		_pSmoothEdge->setWOeB(woeb);
		_pSmoothEdge->setTa(ta);
		_pSmoothEdge->setTb(tb);
		if (_Nature & Nature::SILHOUETTE) {
			if (_nNullDotP != 2) {
				if (_DotP[_ClosestPointIndex] + 0.01 > 0)
					_pSmoothEdge->setFront(true);
				else
					_pSmoothEdge->setFront(false);
			}
		}
	}

#if 0
	// check bording edges to see if they have different dotp values in bording faces.
	for (int i = 0; i < numberOfEdges(); i++) {
		WSFace *bface = (WSFace *)GetBordingFace(i);
		if (bface) {
			if ((front()) ^ (bface->front())) { // fA->front XOR fB->front (true if one is 0 and the other is 1)
			// that means that the edge i of the face is a silhouette edge
			// CHECK FIRST WHETHER THE EXACTSILHOUETTEEDGE HAS NOT YET BEEN BUILT ON THE OTHER FACE (1 is enough).
			if (((WSExactFace *)bface)->exactSilhouetteEdge()) {
				// that means that this silhouette edge has already been built
				return ((WSExactFace *)bface)->exactSilhouetteEdge();
			}
			// Else we must build it
			WOEdge *woea, *woeb;
			real ta, tb;
			if (!front()) { // is it in the right order ?
				// the order of the WOEdge index is wrong
				woea = _OEdgeList[(i + 1) % numberOfEdges()];
				if (0 == i)
					woeb = _OEdgeList[numberOfEdges() - 1];
				else
					woeb = _OEdgeList[(i - 1)];
				ta = 0;
				tb = 1;
			}
			else {
				// the order of the WOEdge index is good
				if (0 == i)
					woea = _OEdgeList[numberOfEdges() - 1];
				else
					woea = _OEdgeList[(i - 1)];
				woeb = _OEdgeList[(i + 1) % numberOfEdges()];
				ta = 1;
				tb = 0;
			}

			_pSmoothEdge = new ExactSilhouetteEdge(ExactSilhouetteEdge::VERTEX_VERTEX);
			_pSmoothEdge->setWOeA(woea);
			_pSmoothEdge->setWOeA(woeb);
			_pSmoothEdge->setTa(ta);
			_pSmoothEdge->setTb(tb);

			return _pSmoothEdge;
			}
		}
	}
#endif
	return _pSmoothEdge;
}


void WXFace::ComputeCenter()
{
	vector<WVertex *> iVertexList;
	RetrieveVertexList(iVertexList);
	Vec3r center;
	for (vector<WVertex *>::iterator wv = iVertexList.begin(), wvend = iVertexList.end(); wv != wvend; ++wv) {
		center += (*wv)->GetVertex();
	}
	center /= (real)iVertexList.size();
	setCenter(center);
}

/**********************************
 *                                *
 *                                *
 *             WXShape            *
 *                                *
 *                                *
 **********************************/

WFace *WXShape::MakeFace(vector<WVertex *>& iVertexList, vector<bool>& iFaceEdgeMarksList, unsigned iMaterialIndex)
{
	WFace *face = WShape::MakeFace(iVertexList, iFaceEdgeMarksList, iMaterialIndex);
	if (!face)
		return NULL;

	Vec3r center;
	for (vector<WVertex *>::iterator wv = iVertexList.begin(), wvend = iVertexList.end(); wv != wvend; ++wv) {
		center += (*wv)->GetVertex();
	}
	center /= (real)iVertexList.size();
	((WXFace *)face)->setCenter(center);

	return face;
}

WFace *WXShape::MakeFace(vector<WVertex *>& iVertexList, vector<Vec3r>& iNormalsList, vector<Vec2r>& iTexCoordsList,
                         vector<bool>& iFaceEdgeMarksList, unsigned iMaterialIndex)
{
	WFace *face = WShape::MakeFace(iVertexList, iNormalsList, iTexCoordsList, iFaceEdgeMarksList, iMaterialIndex);

#if 0
	Vec3r center;
	for (vector<WVertex *>::iterator wv = iVertexList.begin(), wvend = iVertexList.end(); wv != wvend; ++wv) {
		center += (*wv)->GetVertex();
	}
	center /= (real)iVertexList.size();
	((WSFace *)face)->setCenter(center);
#endif

	return face;
}

} /* namespace Freestyle */
