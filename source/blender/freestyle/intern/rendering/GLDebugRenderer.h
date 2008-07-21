//
//  Filename         : GLDebugRenderer.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to render the debug informations related to
//                     a scene
//  Date of creation : 03/04/2002
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  GLDEBUGRENDERER_H
# define GLDEBUGRENDERER_H

# include <float.h>
# include "../system/FreestyleConfig.h"
# include "GLRenderer.h"
# include "../view_map/Silhouette.h"
# include "../winged_edge/Curvature.h"

class WSMeshShape;
class WSExactShape;

class LIB_RENDERING_EXPORT GLDebugRenderer : public GLRenderer
{
public:

  inline GLDebugRenderer() : GLRenderer() {
    _bboxSize = 2.0;
    setMaxValue(&_minEdgeSize);
    _SelectedFEdge = 0;
  }

  inline ~GLDebugRenderer() {}

  VISIT_DECL(NodeShape)

  VISIT_DECL(IndexedFaceSet)
  VISIT_DECL(LineRep)
  VISIT_DECL(OrientedLineRep)
  VISIT_DECL(VertexRep)

  /*! Renders a bitmap string in world coordinates 
   *  x, y, z
   *    The world coordinates of the sentence's starting point
   *  font
   *    The font used to display the text. 
   *    Must be one of :
   *    - GLUT_STROKE_ROMAN 
   *    - GLUT_STROKE_MONO_ROMAN 
   *  string
   *    The text to display
   *  size
   *    The relative size of the text to display
   */
  void renderBitmapString(real x, 
			  real y, 
			  real z, 
			  void *font, 
			  char *string,
			  float size = 1.f)  ; 

  /*! Reinitialize the Renderer so as the previous 
   *  text size does not affect the current one.
   *  iBBoxSize
   *    The size of the scene bounding box.
   */
  inline void ReInit(real iBBoxSize) {_bboxSize = iBBoxSize; setMaxValue(&_minEdgeSize);}

  inline void setSelectedFEdge(FEdge *iFEdge) {_SelectedFEdge = iFEdge;}
  inline FEdge * selectedFEdge() {return _SelectedFEdge;}

protected:

  /*! Renders a face made of a triangles strip
   *    iVertices
   *      Array of float containing the face vertices. 3 floats per 
   *      x, y, z vertex coordinates
   *    iNormals 
   *      Array of float containing the face normals. 3 floats per 
   *      x, y, z vertex normal coordinates
   *    iVIndices
   *      Array of the indices (to use with the iVertices array) 
   *      describing the vertices parsing order
   *    iNIndices
   *      Array of normals indices (to use with iNormals array)
   *      describing the normals parsing order
   *    iNVertices
   *      The number of vertices in the face
   */
  virtual void RenderTriangleStrip(const real *iVertices, 
                                   const real *iNormals,
                                   const Material *const* iMaterials, 
                                   const unsigned* iVIndices, 
                                   const unsigned* iNIndices,
                                   const unsigned* iMIndices,
                                   const unsigned iNVertices);

  /*! Renders a face made of a triangles fan
   *    iVertices
   *      Array of float containing the face vertices. 3 floats per 
   *      x, y, z vertex coordinates
   *    iNormals 
   *      Array of float containing the face normals. 3 floats per 
   *      x, y, z vertex normal coordinates
   *    iVIndices
   *      Array of the indices (to use with the iVertices array) 
   *      describing the vertices parsing order
   *    iNIndices
   *      Array of normals indices (to use with iNormals array)
   *      describing the normals parsing order
   *    iNVertices
   *      The number of vertices in the face
   */
  virtual void RenderTriangleFan(const real *iVertices, 
                                   const real *iNormals,
                                   const Material *const*iMaterials, 
                                   const unsigned* iVIndices, 
                                   const unsigned* iNIndices,
                                   const unsigned* iMIndices,
                                   const unsigned iNVertices);

  /*! Renders a face made of single triangles
   *    iVertices
   *      Array of float containing the face vertices. 3 floats per 
   *      x, y, z vertex coordinates
   *    iNormals 
   *      Array of float containing the face normals. 3 floats per 
   *      x, y, z vertex normal coordinates
   *    iVIndices
   *      Array of the indices (to use with the iVertices array) 
   *      describing the vertices parsing order
   *    iNIndices
   *      Array of normals indices (to use with iNormals array)
   *      describing the normals parsing order
   *    iNVertices
   *      The number of vertices in the face
   */
  virtual void RenderTriangles(const real *iVertices, 
                                   const real *iNormals,
                                   const Material *const* iMaterials, 
                                   const unsigned* iVIndices, 
                                   const unsigned* iNIndices,
                                   const unsigned* iMIndices,
                                   const unsigned iNVertices);

  /*! glTranslatef or glTranslated */
  inline void glTranslater(float x, float y, float z) {glTranslatef(x,y,z);}
  inline void glTranslater(real x, real y, real z) {glTranslated(x,y,z);}


private:

  inline void setMaxValue(float *oValue) {*oValue = FLT_MAX;}
  inline void setMaxValue(real *oValue) {*oValue = DBL_MAX;}

  mutable real _bboxSize; 
  mutable real _minEdgeSize;

  FEdge *_SelectedFEdge;
};

#endif // GLDEBUGRENDERER_H
