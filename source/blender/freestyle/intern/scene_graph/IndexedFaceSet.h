/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief A Set of indexed faces to represent a surface object
 */

#include <memory.h>
#include <stdio.h>

//! inherits from class Rep
#include "Rep.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

class IndexedFaceSet : public Rep {
 public:
  /** Triangles description style: */
  enum TRIANGLES_STYLE {
    TRIANGLE_STRIP,
    TRIANGLE_FAN,
    TRIANGLES,
  };

  /** User-specified face and edge marks for feature edge detection */
  /* XXX Why in hell not use an enum here too? */
  typedef uchar FaceEdgeMark;
  static const FaceEdgeMark FACE_MARK = 1 << 0;
  static const FaceEdgeMark EDGE_MARK_V1V2 = 1 << 1;
  static const FaceEdgeMark EDGE_MARK_V2V3 = 1 << 2;
  static const FaceEdgeMark EDGE_MARK_V3V1 = 1 << 3;

  /** Builds an empty indexed face set. */
  IndexedFaceSet();

  /** Builds an indexed face set
   *    iVertices
   *      The array of object vertices 3D coordinates (for all faces).
   *      If iCopy != 0, the array is copied; you must deallocate iVertices. Else you must not.
   *    iVSize
   *      The size of iVertices (must be a multiple of 3)
   *    iNormals
   *      The array of object normals 3D coordinates.
   *      If iCopy != 0, the array is copied; you must deallocate iNormals. Else you must not.
   *    iNSize
   *      The size of iNormals
   *    iMaterials
   *      The array of materials
   *    iMSize
   *      The size of iMaterials
   *    iTexCoords
   *      The array of texture coordinates.
   *    iTSize
   *      The size of iTexCoords (must be multiple of 2)
   *    iNumFaces
   *      The number of faces
   *    iNumVertexPerFace
   *      Array containing the number of vertices per face.
   *    iFaceStyle
   *      Array containing the description style of each faces.
   *      The style belongs to:
   *        - TRIANGLE_STRIP: the face indices describe a triangle strip
   *        - TRIANGLE_FAN  : the face indices describe a triangle fan
   *        - TRIANGLES     : the face indices describe single triangles
   *      If iCopy != 0, the array is copied; you must deallocate iFaceStyle. Else you must not.
   *    iVIndices,
   *      Array of vertices indices.
   *      The integers contained in this array must be multiple of 3.
   *      If iCopy != 0, the array is copied; you must deallocate iVIndices. Else you must not.
   *    iVISize
   *      The size of iVIndices.
   *    iNIndices
   *      Array of normals indices.
   *      The integers contained in this array must be multiple of 3.
   *      If iCopy != 0, the array is copied; you must deallocate iNIndices. Else you must not.
   *    iNISize
   *      The size of iNIndices
   *    iMIndices
   *      The Material indices (per vertex)
   *    iMISize
   *      The size of iMIndices
   *    iTIndices
   *      The Texture coordinates indices (per vertex). The integers contained in this array must
   * be multiple of 2. iTISize The size of iMIndices iCopy 0 : the arrays are not copied. The
   * pointers passed as arguments are used. IndexedFaceSet takes these arrays desallocation in
   * charge. 1 : the arrays are copied. The caller is in charge of the arrays, passed as arguments
   * desallocation.
   */
  IndexedFaceSet(float *iVertices,
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
                 uint iCopy = 1);

  /** Builds an indexed face set from an other indexed face set */
  IndexedFaceSet(const IndexedFaceSet &iBrother);

  void swap(IndexedFaceSet &ioOther)
  {
    std::swap(_Vertices, ioOther._Vertices);
    std::swap(_Normals, ioOther._Normals);
    std::swap(_FrsMaterials, ioOther._FrsMaterials);
    std::swap(_TexCoords, ioOther._TexCoords);
    std::swap(_FaceEdgeMarks, ioOther._FaceEdgeMarks);

    std::swap(_VSize, ioOther._VSize);
    std::swap(_NSize, ioOther._NSize);
    std::swap(_MSize, ioOther._MSize);
    std::swap(_TSize, ioOther._TSize);

    std::swap(_NumFaces, ioOther._NumFaces);
    std::swap(_NumVertexPerFace, ioOther._NumVertexPerFace);
    std::swap(_FaceStyle, ioOther._FaceStyle);

    std::swap(_VIndices, ioOther._VIndices);
    std::swap(_NIndices, ioOther._NIndices);
    std::swap(_MIndices, ioOther._MIndices);  // Material Indices
    std::swap(_TIndices, ioOther._TIndices);

    std::swap(_VISize, ioOther._VISize);
    std::swap(_NISize, ioOther._NISize);
    std::swap(_MISize, ioOther._MISize);
    std::swap(_TISize, ioOther._TISize);

    Rep::swap(ioOther);
  }

  IndexedFaceSet &operator=(const IndexedFaceSet &iBrother)
  {
    IndexedFaceSet tmp(iBrother);
    swap(tmp);
    return *this;
  }

  /** Destructor
   *  deallocates all the resources
   */
  virtual ~IndexedFaceSet();

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);

  /** Compute the Bounding Box */
  virtual void ComputeBBox();

  /** Accessors */
  virtual const float *vertices() const
  {
    return _Vertices;
  }

  virtual const float *normals() const
  {
    return _Normals;
  }

  virtual const FrsMaterial *const *frs_materials() const
  {
    return _FrsMaterials;
  }

  virtual const float *texCoords() const
  {
    return _TexCoords;
  }

  virtual const uint vsize() const
  {
    return _VSize;
  }

  virtual const uint nsize() const
  {
    return _NSize;
  }

  virtual const uint msize() const
  {
    return _MSize;
  }

  virtual const uint tsize() const
  {
    return _TSize;
  }

  virtual const uint numFaces() const
  {
    return _NumFaces;
  }

  virtual const uint *numVertexPerFaces() const
  {
    return _NumVertexPerFace;
  }

  virtual const TRIANGLES_STYLE *trianglesStyle() const
  {
    return _FaceStyle;
  }

  virtual const uchar *faceEdgeMarks() const
  {
    return _FaceEdgeMarks;
  }

  virtual const uint *vindices() const
  {
    return _VIndices;
  }

  virtual const uint *nindices() const
  {
    return _NIndices;
  }

  virtual const uint *mindices() const
  {
    return _MIndices;
  }

  virtual const uint *tindices() const
  {
    return _TIndices;
  }

  virtual const uint visize() const
  {
    return _VISize;
  }

  virtual const uint nisize() const
  {
    return _NISize;
  }

  virtual const uint misize() const
  {
    return _MISize;
  }

  virtual const uint tisize() const
  {
    return _TISize;
  }

 protected:
  float *_Vertices;
  float *_Normals;
  FrsMaterial **_FrsMaterials;
  float *_TexCoords;

  uint _VSize;
  uint _NSize;
  uint _MSize;
  uint _TSize;

  uint _NumFaces;
  uint *_NumVertexPerFace;
  TRIANGLES_STYLE *_FaceStyle;
  FaceEdgeMark *_FaceEdgeMarks;

  uint *_VIndices;
  uint *_NIndices;
  uint *_MIndices;  // Material Indices
  uint *_TIndices;  // Texture coordinates Indices

  uint _VISize;
  uint _NISize;
  uint _MISize;
  uint _TISize;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:IndexedFaceSet")
#endif
};

} /* namespace Freestyle */
