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

/** \file
 * \ingroup freestyle
 * \brief A Set of indexed faces to represent a surface object
 */

#include "IndexedFaceSet.h"

namespace Freestyle {

IndexedFaceSet::IndexedFaceSet()
{
  _Vertices = nullptr;
  _Normals = nullptr;
  _FrsMaterials = nullptr;
  _TexCoords = nullptr;
  _FaceEdgeMarks = nullptr;
  _VSize = 0;
  _NSize = 0;
  _MSize = 0;
  _TSize = 0;
  _NumFaces = 0;
  _NumVertexPerFace = nullptr;
  _FaceStyle = nullptr;
  _VIndices = nullptr;
  _VISize = 0;
  _NIndices = nullptr;
  _NISize = 0;
  _MIndices = nullptr;
  _MISize = 0;
  _TIndices = nullptr;
  _TISize = 0;
}

IndexedFaceSet::IndexedFaceSet(float *iVertices,
                               unsigned iVSize,
                               float *iNormals,
                               unsigned iNSize,
                               FrsMaterial **iMaterials,
                               unsigned iMSize,
                               float *iTexCoords,
                               unsigned iTSize,
                               unsigned iNumFaces,
                               unsigned *iNumVertexPerFace,
                               TRIANGLES_STYLE *iFaceStyle,
                               FaceEdgeMark *iFaceEdgeMarks,
                               unsigned *iVIndices,
                               unsigned iVISize,
                               unsigned *iNIndices,
                               unsigned iNISize,
                               unsigned *iMIndices,
                               unsigned iMISize,
                               unsigned *iTIndices,
                               unsigned iTISize,
                               unsigned iCopy)
{
  if (1 == iCopy) {
    _VSize = iVSize;
    _Vertices = new float[_VSize];
    memcpy(_Vertices, iVertices, iVSize * sizeof(float));

    _NSize = iNSize;
    _Normals = new float[_NSize];
    memcpy(_Normals, iNormals, iNSize * sizeof(float));

    _MSize = iMSize;
    _FrsMaterials = nullptr;
    if (iMaterials) {
      _FrsMaterials = new FrsMaterial *[_MSize];
      for (unsigned int i = 0; i < _MSize; ++i) {
        _FrsMaterials[i] = new FrsMaterial(*(iMaterials[i]));
      }
    }
    _TSize = iTSize;
    _TexCoords = nullptr;
    if (_TSize) {
      _TexCoords = new float[_TSize];
      memcpy(_TexCoords, iTexCoords, iTSize * sizeof(float));
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
    _MIndices = nullptr;
    if (iMIndices) {
      _MIndices = new unsigned[_MISize];
      memcpy(_MIndices, iMIndices, _MISize * sizeof(unsigned));
    }
    _TISize = iTISize;
    _TIndices = nullptr;
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
    _FrsMaterials = nullptr;
    if (iMaterials) {
      _FrsMaterials = iMaterials;
    }

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
    _MIndices = nullptr;
    if (iMISize) {
      _MIndices = iMIndices;
    }

    _TISize = iTISize;
    _TIndices = iTIndices;
  }
}

IndexedFaceSet::IndexedFaceSet(const IndexedFaceSet &iBrother) : Rep(iBrother)
{
  _VSize = iBrother.vsize();
  _Vertices = new float[_VSize];
  memcpy(_Vertices, iBrother.vertices(), _VSize * sizeof(float));

  _NSize = iBrother.nsize();
  _Normals = new float[_NSize];
  memcpy(_Normals, iBrother.normals(), _NSize * sizeof(float));

  _MSize = iBrother.msize();
  if (_MSize) {
    _FrsMaterials = new FrsMaterial *[_MSize];
    for (unsigned int i = 0; i < _MSize; ++i) {
      _FrsMaterials[i] = new FrsMaterial(*(iBrother._FrsMaterials[i]));
    }
  }
  else {
    _FrsMaterials = nullptr;
  }

  _TSize = iBrother.tsize();
  _TexCoords = nullptr;
  if (_TSize) {
    _TexCoords = new float[_TSize];
    memcpy(_TexCoords, iBrother.texCoords(), _TSize * sizeof(float));
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
    _MIndices = nullptr;
  }

  _TISize = iBrother.tisize();
  _TIndices = nullptr;
  if (_TISize) {
    _TIndices = new unsigned[_TISize];
    memcpy(_TIndices, iBrother.tindices(), _TISize * sizeof(unsigned));
  }
}

IndexedFaceSet::~IndexedFaceSet()
{
  if (nullptr != _Vertices) {
    delete[] _Vertices;
    _Vertices = nullptr;
  }

  if (nullptr != _Normals) {
    delete[] _Normals;
    _Normals = nullptr;
  }

  if (nullptr != _FrsMaterials) {
    for (unsigned int i = 0; i < _MSize; ++i) {
      delete _FrsMaterials[i];
    }
    delete[] _FrsMaterials;
    _FrsMaterials = nullptr;
  }

  if (nullptr != _TexCoords) {
    delete[] _TexCoords;
    _TexCoords = nullptr;
  }

  if (nullptr != _NumVertexPerFace) {
    delete[] _NumVertexPerFace;
    _NumVertexPerFace = nullptr;
  }

  if (nullptr != _FaceStyle) {
    delete[] _FaceStyle;
    _FaceStyle = nullptr;
  }

  if (nullptr != _FaceEdgeMarks) {
    delete[] _FaceEdgeMarks;
    _FaceEdgeMarks = nullptr;
  }

  if (nullptr != _VIndices) {
    delete[] _VIndices;
    _VIndices = nullptr;
  }

  if (nullptr != _NIndices) {
    delete[] _NIndices;
    _NIndices = nullptr;
  }

  if (nullptr != _MIndices) {
    delete[] _MIndices;
    _MIndices = nullptr;
  }
  if (nullptr != _TIndices) {
    delete[] _TIndices;
    _TIndices = nullptr;
  }
}

void IndexedFaceSet::accept(SceneVisitor &v)
{
  Rep::accept(v);
  v.visitIndexedFaceSet(*this);
}

void IndexedFaceSet::ComputeBBox()
{
  float XMax = _Vertices[0];
  float YMax = _Vertices[1];
  float ZMax = _Vertices[2];

  float XMin = _Vertices[0];
  float YMin = _Vertices[1];
  float ZMin = _Vertices[2];

  // parse all the coordinates to find the Xmax, YMax, ZMax
  float *v = _Vertices;

  for (unsigned int i = 0; i < (_VSize / 3); ++i) {
    if (*v > XMax) {
      XMax = *v;
    }
    if (*v < XMin) {
      XMin = *v;
    }
    ++v;

    if (*v > YMax) {
      YMax = *v;
    }
    if (*v < YMin) {
      YMin = *v;
    }
    ++v;

    if (*v > ZMax) {
      ZMax = *v;
    }
    if (*v < ZMin) {
      ZMin = *v;
    }
    ++v;
  }

  setBBox(BBox<Vec3f>(Vec3f(XMin, YMin, ZMin), Vec3f(XMax, YMax, ZMax)));
}

} /* namespace Freestyle */
