/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief A Set of indexed faces to represent a surface object
 */

#include "IndexedFaceSet.h"

#include "BLI_sys_types.h"

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
                               uint iVSize,
                               float *iNormals,
                               uint iNSize,
                               FrsMaterial **iMaterials,
                               uint iMSize,
                               float *iTexCoords,
                               uint iTSize,
                               uint iNumFaces,
                               uint *iNumVertexPerFace,
                               TRIANGLES_STYLE *iFaceStyle,
                               FaceEdgeMark *iFaceEdgeMarks,
                               uint *iVIndices,
                               uint iVISize,
                               uint *iNIndices,
                               uint iNISize,
                               uint *iMIndices,
                               uint iMISize,
                               uint *iTIndices,
                               uint iTISize,
                               uint iCopy)
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
      for (uint i = 0; i < _MSize; ++i) {
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
    _NumVertexPerFace = new uint[_NumFaces];
    memcpy(_NumVertexPerFace, iNumVertexPerFace, _NumFaces * sizeof(uint));

    _FaceStyle = new TRIANGLES_STYLE[_NumFaces];
    memcpy(_FaceStyle, iFaceStyle, _NumFaces * sizeof(TRIANGLES_STYLE));

    _FaceEdgeMarks = new FaceEdgeMark[_NumFaces];
    memcpy(_FaceEdgeMarks, iFaceEdgeMarks, _NumFaces * sizeof(FaceEdgeMark));

    _VISize = iVISize;
    _VIndices = new uint[_VISize];
    memcpy(_VIndices, iVIndices, _VISize * sizeof(uint));

    _NISize = iNISize;
    _NIndices = new uint[_NISize];
    memcpy(_NIndices, iNIndices, _NISize * sizeof(uint));

    _MISize = iMISize;
    _MIndices = nullptr;
    if (iMIndices) {
      _MIndices = new uint[_MISize];
      memcpy(_MIndices, iMIndices, _MISize * sizeof(uint));
    }
    _TISize = iTISize;
    _TIndices = nullptr;
    if (_TISize) {
      _TIndices = new uint[_TISize];
      memcpy(_TIndices, iTIndices, _TISize * sizeof(uint));
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
    for (uint i = 0; i < _MSize; ++i) {
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
  _NumVertexPerFace = new uint[_NumFaces];
  memcpy(_NumVertexPerFace, iBrother.numVertexPerFaces(), _NumFaces * sizeof(uint));

  _FaceStyle = new TRIANGLES_STYLE[_NumFaces];
  memcpy(_FaceStyle, iBrother.trianglesStyle(), _NumFaces * sizeof(TRIANGLES_STYLE));

  _FaceEdgeMarks = new FaceEdgeMark[_NumFaces];
  memcpy(_FaceEdgeMarks, iBrother.faceEdgeMarks(), _NumFaces * sizeof(FaceEdgeMark));

  _VISize = iBrother.visize();
  _VIndices = new uint[_VISize];
  memcpy(_VIndices, iBrother.vindices(), _VISize * sizeof(uint));

  _NISize = iBrother.nisize();
  _NIndices = new uint[_NISize];
  memcpy(_NIndices, iBrother.nindices(), _NISize * sizeof(uint));

  _MISize = iBrother.misize();
  if (_MISize) {
    _MIndices = new uint[_MISize];
    memcpy(_MIndices, iBrother.mindices(), _MISize * sizeof(uint));
  }
  else {
    _MIndices = nullptr;
  }

  _TISize = iBrother.tisize();
  _TIndices = nullptr;
  if (_TISize) {
    _TIndices = new uint[_TISize];
    memcpy(_TIndices, iBrother.tindices(), _TISize * sizeof(uint));
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
    for (uint i = 0; i < _MSize; ++i) {
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

  for (uint i = 0; i < (_VSize / 3); ++i) {
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
