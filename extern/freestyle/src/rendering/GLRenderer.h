//
//  Filename         : GLRenderer.h
//  Author(s)        : Stephane Grabli, Emmanuel Turquin
//  Purpose          : Class to render a 3D scene thanks to OpenGL
//  Date of creation : 07/02/2002
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

#ifndef  GLRENDERER_H
# define GLRENDERER_H

# ifdef WIN32
#  include <windows.h>
# endif
# ifdef __MACH__
#  include <OpenGL/gl.h>
# else
#  include <GL/gl.h>
# endif

# include "../system/FreestyleConfig.h"
# include "../system/Precision.h"
# include "../scene_graph/SceneVisitor.h"
# include "../geometry/Geom.h"
using namespace Geometry; 

class LIB_RENDERING_EXPORT GLRenderer : public SceneVisitor
{
 public:

  inline GLRenderer() : SceneVisitor() {}
  virtual ~GLRenderer() {}

  //
  // visitClass methods
  //
  //////////////////////////////////////////////

  VISIT_DECL(NodeLight)
  VISIT_DECL(NodeCamera)
  VISIT_DECL(NodeTransform)

  VISIT_DECL(LineRep)
  VISIT_DECL(OrientedLineRep)
  VISIT_DECL(TriangleRep)
  VISIT_DECL(VertexRep)
  VISIT_DECL(IndexedFaceSet)
  VISIT_DECL(DrawingStyle)
  VISIT_DECL(Material)

  virtual void visitMaterial(const Material&);
  virtual void visitNodeTransformBefore(NodeTransform&);
  virtual void visitNodeTransformAfter(NodeTransform&);
  virtual void visitNodeDrawingStyleBefore(NodeDrawingStyle&);
  virtual void visitNodeDrawingStyleAfter(NodeDrawingStyle&);

 protected:

  /*! Renders a face made of a triangles strip
   *    iVertices
   *      Array of float containing the face vertices. 3 floats per 
   *      x, y, z vertex coordinates
   *    iNormals 
   *      Array of float containing the face normals. 3 floats per 
   *      x, y, z vertex normal coordinates
   *    iTexCoords
   *      Array of float containing the face uv coords. 2 floats per 
   *      u,v vertex texture coordinates
   *    iVIndices
   *      Array of the indices (to use with the iVertices array) 
   *      describing the vertices parsing order
   *    iNIndices
   *      Array of normals indices (to use with iNormals array)
   *      describing the normals parsing order
   *    iTIndices
   *      Array of texture coordinates indices (to use with iTexCoords array)
   *      describing the texture coordinates parsing order
   *    iNVertices
   *      The number of vertices in the face
   */
  virtual void RenderTriangleStrip(const real *iVertices, 
                                   const real *iNormals,
                                   const Material *const*iMaterials, 
                                   const real *iTexCoords,
                                   const unsigned* iVIndices, 
                                   const unsigned* iNIndices,
                                   const unsigned* iMIndices,
                                   const unsigned* iTIndices,
                                   const unsigned iNVertices) ;

  /*! Renders a face made of a triangles fan
   *    iVertices
   *      Array of float containing the face vertices. 3 floats per 
   *      x, y, z vertex coordinates
   *    iNormals 
   *      Array of float containing the face normals. 3 floats per 
   *      x, y, z vertex normal coordinates
   *    iTexCoords
   *      Array of float containing the face uv coords. 2 floats per 
   *      u,v vertex texture coordinates
   *    iVIndices
   *      Array of the indices (to use with the iVertices array) 
   *      describing the vertices parsing order
   *    iNIndices
   *      Array of normals indices (to use with iNormals array)
   *      describing the normals parsing order
   *    iTIndices
   *      Array of texture coordinates indices (to use with iTexCoords array)
   *      describing the texture coordinates parsing order
   *    iNVertices
   *      The number of vertices in the face
   */
  virtual void RenderTriangleFan(const real *iVertices, 
                                 const real *iNormals,
                                 const Material *const* iMaterials,
                                 const real *iTexCoords,
                                 const unsigned* iVIndices, 
                                 const unsigned* iNIndices,
                                 const unsigned* iMIndices,
                                 const unsigned* iTIndices,
                                 const unsigned iNVertices) ;

  /*! Renders a face made of single triangles
   *    iVertices
   *      Array of float containing the face vertices. 3 floats per 
   *      x, y, z vertex coordinates
   *    iNormals 
   *      Array of float containing the face normals. 3 floats per 
   *      x, y, z vertex normal coordinates
   *    iTexCoords
   *      Array of float containing the face uv coords. 2 floats per 
   *      u,v vertex texture coordinates
   *    iVIndices
   *      Array of the indices (to use with the iVertices array) 
   *      describing the vertices parsing order
   *    iNIndices
   *      Array of normals indices (to use with iNormals array)
   *      describing the normals parsing order
   *    iTIndices
   *      Array of texture coordinates indices (to use with iTexCoords array)
   *      describing the texture coordinates parsing order
   *    iNVertices
   *      The number of vertices in the face
   */
  virtual void RenderTriangles(const real *iVertices, 
                               const real *iNormals,
                               const Material *const* iMaterials, 
                               const real *iTexCoords,
                               const unsigned* iVIndices, 
                               const unsigned* iNIndices,
                               const unsigned* iMIndices,
                               const unsigned* iTIndices,
                               const unsigned iNVertices)  ;

  /*! Apply a transform matrix by multiplying 
   *  the current OpenGL ModelView Matrix by 
   *  iMatrix
   */
  virtual void applyTransform( const Matrix44r &iMatrix)  ;

  /*! Sets the current drawing color.
   *  Active only when light is disabled
   *  (simple call to glColor4fv)
   *    rgba
   *      array of 4 floats (r, g, b and alpha)
   */
  virtual void RenderColor( const float *rgba);

  /*! glVertex3f or glVertex3d */
  inline void glVertex3r(float x, float y, float z) {glVertex3f(x,y,z);}
  inline void glVertex3r(real x, real y, real z) {glVertex3d(x,y,z);}

  /*! glVertex3f or glNormal3d */
  inline void glNormal3r(float x, float y, float z) {glNormal3f(x,y,z);}
  inline void glNormal3r(real x, real y, real z) {glNormal3d(x,y,z);}

  /*! glMultMatrixf or glMultMatrixd */
  inline void glMultMatrixr(float *m) {glMultMatrixf(m);}
  inline void glMultMatrixr(real *m) {glMultMatrixd(m);}

};

#endif // GLRENDERER_H
