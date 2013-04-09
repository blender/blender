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

/** \file blender/freestyle/intern/scene_graph/IndexedFaceSet.cpp
 *  \ingroup freestyle
 *  \brief A Set of indexed faces to represent a surfacic object
 *  \author Stephane Grabli
 *  \date 22/01/2002
 */

#include "IndexedFaceSet.h"

namespace Freestyle {

IndexedFaceSet::IndexedFaceSet() : Rep()
{
	_Vertices = NULL;
	_Normals = NULL;
	_FrsMaterials = 0;
	_TexCoords = 0;
	_FaceEdgeMarks = 0;
	_VSize = 0;
	_NSize = 0;
	_MSize = 0;
	_TSize = 0;
	_NumFaces = 0;
	_NumVertexPerFace = NULL;
	_FaceStyle = NULL;
	_VIndices = NULL;
	_VISize = 0;
	_NIndices = NULL;
	_NISize = 0;
	_MIndices = NULL;
	_MISize = 0;
	_TIndices = NULL;
	_TISize = 0;
	_displayList = 0;
}

IndexedFaceSet::IndexedFaceSet(real *iVertices, unsigned iVSize, real *iNormals, unsigned iNSize,
                               FrsMaterial **iMaterials, unsigned iMSize, real *iTexCoords, unsigned iTSize,
                               unsigned iNumFaces, unsigned *iNumVertexPerFace, TRIANGLES_STYLE *iFaceStyle,
                               FaceEdgeMark *iFaceEdgeMarks, unsigned *iVIndices, unsigned iVISize,
                               unsigned *iNIndices, unsigned iNISize, unsigned *iMIndices, unsigned iMISize,
                               unsigned *iTIndices, unsigned iTISize, unsigned iCopy)
: Rep()
{
	if (1 == iCopy) {
		_VSize = iVSize;
		_Vertices = new real[_VSize];
		memcpy(_Vertices, iVertices, iVSize * sizeof(real));

		_NSize = iNSize;
		_Normals = new real[_NSize];
		memcpy(_Normals, iNormals, iNSize * sizeof(real));

		_MSize = iMSize;
		_FrsMaterials = 0;
		if (iMaterials) {
			_FrsMaterials = new FrsMaterial * [_MSize];
			for (unsigned int i = 0; i < _MSize; ++i)
				_FrsMaterials[i] = new FrsMaterial(*(iMaterials[i]));
		}
		_TSize = iTSize;
		_TexCoords = 0;
		if (_TSize) {
			_TexCoords = new real[_TSize];
			memcpy(_TexCoords, iTexCoords, iTSize * sizeof(real));
		}

		_NumFaces = iNumFaces;
		_NumVertexPerFace = new unsigned[_NumFaces];
		memcpy(_NumVertexPerFace, iNumVertexPerFace, _NumFaces * sizeof(unsigned));

		_FaceStyle = new TRIANGLES_STYLE[_NumFaces];
		memcpy(_FaceStyle, iFaceStyle, _NumFaces * sizeof(TRIANGLES_STYLE));

		_FaceEdgeMarks = new FaceEdgeMark[_NumFaces];
		memcpy(_FaceEdgeMarks, iFaceEdgeMarks, _NumFaces * sizeof(FaceEdgeMark));

		_VISize = iVISize;
		_VIndices = new unsigned[_VISize];
		memcpy(_VIndices, iVIndices, _VISize * sizeof(unsigned));

		_NISize = iNISize;
		_NIndices = new unsigned[_NISize];
		memcpy(_NIndices, iNIndices, _NISize * sizeof(unsigned));

		_MISize = iMISize;
		_MIndices = 0;
		if (iMIndices) {
			_MIndices = new unsigned[_MISize];
			memcpy(_MIndices, iMIndices, _MISize * sizeof(unsigned));
		}
		_TISize = iTISize;
		_TIndices = 0;
		if (_TISize) {
			_TIndices = new unsigned[_TISize];
			memcpy(_TIndices, iTIndices, _TISize * sizeof(unsigned));
		}
	}
	else {
		_VSize = iVSize;
		_Vertices = iVertices;

		_NSize = iNSize;
		_Normals = iNormals;

		_MSize = iMSize;
		_FrsMaterials = 0;
		if (iMaterials)
			_FrsMaterials = iMaterials;

		_TSize = iTSize;
		_TexCoords = iTexCoords;

		_NumFaces = iNumFaces;
		_NumVertexPerFace = iNumVertexPerFace;
		_FaceStyle = iFaceStyle;
		_FaceEdgeMarks = iFaceEdgeMarks;

		_VISize = iVISize;
		_VIndices = iVIndices;

		_NISize = iNISize;
		_NIndices = iNIndices;

		_MISize = iMISize;
		_MIndices = 0;
		if (iMISize)
			_MIndices = iMIndices;

		_TISize = iTISize;
		_TIndices = iTIndices;
	}

	_displayList = 0;
}

IndexedFaceSet::IndexedFaceSet(const IndexedFaceSet& iBrother) : Rep(iBrother)
{
	_VSize = iBrother.vsize();
	_Vertices = new real[_VSize];
	memcpy(_Vertices, iBrother.vertices(), _VSize * sizeof(real));

	_NSize = iBrother.nsize();
	_Normals = new real[_NSize];
	memcpy(_Normals, iBrother.normals(), _NSize * sizeof(real));

	_MSize = iBrother.msize();
	if (_MSize) {
		_FrsMaterials = new FrsMaterial * [_MSize];
		for (unsigned int i = 0; i < _MSize; ++i) {
			_FrsMaterials[i] = new FrsMaterial(*(iBrother._FrsMaterials[i]));
		}
	}
	else {
		_FrsMaterials = 0;
	}

	_TSize = iBrother.tsize();
	_TexCoords = 0;
	if (_TSize) {
		_TexCoords = new real[_TSize];
		memcpy(_TexCoords, iBrother.texCoords(), _TSize * sizeof(real));
	}

	_NumFaces = iBrother.numFaces();
	_NumVertexPerFace = new unsigned[_NumFaces];
	memcpy(_NumVertexPerFace, iBrother.numVertexPerFaces(), _NumFaces * sizeof(unsigned));

	_FaceStyle = new TRIANGLES_STYLE[_NumFaces];
	memcpy(_FaceStyle, iBrother.trianglesStyle(), _NumFaces * sizeof(TRIANGLES_STYLE));

	_FaceEdgeMarks = new FaceEdgeMark[_NumFaces];
	memcpy(_FaceEdgeMarks, iBrother.faceEdgeMarks(), _NumFaces * sizeof(FaceEdgeMark));

	_VISize = iBrother.visize();
	_VIndices = new unsigned[_VISize];
	memcpy(_VIndices, iBrother.vindices(), _VISize * sizeof(unsigned));

	_NISize = iBrother.nisize();
	_NIndices = new unsigned[_NISize];
	memcpy(_NIndices, iBrother.nindices(), _NISize * sizeof(unsigned));

	_MISize = iBrother.misize();
	if (_MISize) {
		_MIndices = new unsigned[_MISize];
		memcpy(_MIndices, iBrother.mindices(), _MISize * sizeof(unsigned));
	}
	else {
		_MIndices = 0;
	}

	_TISize = iBrother.tisize();
	_TIndices = 0;
	if (_TISize) {
		_TIndices = new unsigned[_TISize];
		memcpy(_TIndices, iBrother.tindices(), _TISize * sizeof(unsigned));
	}

	_displayList = 0;
}

IndexedFaceSet::~IndexedFaceSet()
{
	if (NULL != _Vertices) {
		delete[] _Vertices;
		_Vertices = NULL;
	}

	if (NULL != _Normals) {
		delete[] _Normals;
		_Normals = NULL;
	}

	if (NULL != _FrsMaterials) {
		for (unsigned int i = 0; i < _MSize; ++i)
			delete _FrsMaterials[i];
		delete[] _FrsMaterials;
		_FrsMaterials = NULL;
	}

	if (NULL != _TexCoords) {
		delete[] _TexCoords;
		_TexCoords = NULL;
	}

	if (NULL != _NumVertexPerFace) {
		delete[] _NumVertexPerFace;
		_NumVertexPerFace = NULL;
	}

	if (NULL != _FaceStyle) {
		delete[] _FaceStyle;
		_FaceStyle = NULL;
	}

	if (NULL != _FaceEdgeMarks) {
		delete[] _FaceEdgeMarks;
		_FaceEdgeMarks = NULL;
	}

	if (NULL != _VIndices) {
		delete[] _VIndices;
		_VIndices = NULL;
	}

	if (NULL != _NIndices) {
		delete[] _NIndices;
		_NIndices = NULL;
	}

	if (NULL != _MIndices) {
		delete[] _MIndices;
		_MIndices = NULL;
	}
	if (NULL != _TIndices) {
		delete[] _TIndices;
		_TIndices = NULL;
	}

	// should find a way to deallocates the displayList
	// glDeleteLists(GLuint list, GLSizei range)
	_displayList = 0;
}

void IndexedFaceSet::accept(SceneVisitor& v)
{
	Rep::accept(v);
	v.visitIndexedFaceSet(*this);
}

void IndexedFaceSet::ComputeBBox()
{
	real XMax = _Vertices[0];
	real YMax = _Vertices[1];
	real ZMax = _Vertices[2];

	real XMin = _Vertices[0];
	real YMin = _Vertices[1];
	real ZMin = _Vertices[2];

	// parse all the coordinates to find the Xmax, YMax, ZMax
	real *v = _Vertices;

	for (unsigned int i = 0; i < (_VSize / 3); ++i) {
		if (*v > XMax)
			XMax = *v;
		if (*v < XMin)
			XMin = *v;
		++v;

		if (*v > YMax)
			YMax = *v;
		if (*v < YMin)
			YMin = *v;
		++v;

		if (*v > ZMax)
			ZMax = *v;
		if (*v < ZMin)
			ZMin = *v;
		++v;
	}

	setBBox(BBox<Vec3r>(Vec3r(XMin, YMin, ZMin), Vec3r(XMax, YMax, ZMax)));
}

} /* namespace Freestyle */
