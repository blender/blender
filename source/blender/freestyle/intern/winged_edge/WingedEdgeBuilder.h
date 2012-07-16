//
//  Filename         : WingedEdgeBuilder.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to render a WingedEdge data structure
//                     from a polyhedral data structure organized in
//                     nodes of a scene graph
//  Date of creation : 28/05/03
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

#ifndef  WINGED_EDGE_BUILDER_H
# define WINGED_EDGE_BUILDER_H

# include "../system/FreestyleConfig.h"
# include "../system/RenderMonitor.h"
# include "../scene_graph/SceneVisitor.h"
# include "WEdge.h"
# include "../scene_graph/IndexedFaceSet.h"
# include "../scene_graph/NodeTransform.h"

class LIB_WINGED_EDGE_EXPORT WingedEdgeBuilder : public SceneVisitor
{
 public:

  inline WingedEdgeBuilder() : SceneVisitor() {
    _current_wshape = NULL;
    _current_frs_material = NULL;
    _current_matrix = NULL;
    _winged_edge = new WingedEdge; // Not deleted by the destructor
    _pRenderMonitor = NULL;
  }

  virtual ~WingedEdgeBuilder() {
    for (vector<Matrix44r*>::iterator it = _matrices_stack.begin();
	 it != _matrices_stack.end();
	 it++)
      delete *it;
    _matrices_stack.clear();
  }

  VISIT_DECL(IndexedFaceSet)
  VISIT_DECL(NodeShape)
  VISIT_DECL(NodeTransform)

  virtual void visitNodeTransformAfter(NodeTransform&);

  //
  // Accessors
  //
  /////////////////////////////////////////////////////////////////////////////

  inline WingedEdge*	getWingedEdge() {
    return _winged_edge;
  }

  inline WShape*	getCurrentWShape() {
    return _current_wshape;
  }
 
  inline FrsMaterial*	getCurrentFrsMaterial() {
    return _current_frs_material;
  }

  inline Matrix44r*	getCurrentMatrix() {
    return _current_matrix;
  }

  //
  // Modifiers
  //
  /////////////////////////////////////////////////////////////////////////////

  inline void setCurrentWShape(WShape* wshape) {
    _current_wshape = wshape;
  }

  inline void setCurrentFrsMaterial(FrsMaterial* mat) {
    _current_frs_material = mat;
  }

  //  inline void setCurrentMatrix(Matrix44r* matrix) {
  //    _current_matrix = matrix;
  //  }

  inline void setRenderMonitor(RenderMonitor *iRenderMonitor) {
    _pRenderMonitor = iRenderMonitor;
  }

 protected:

  virtual void buildWShape(WShape& shape, IndexedFaceSet& ifs);
  virtual void buildWVertices(WShape& shape,
			      const real *vertices,
			      unsigned vsize);

  RenderMonitor *_pRenderMonitor;

 private:

  void buildTriangleStrip(const real *vertices, 
			  const real *normals, 
        vector<FrsMaterial>&  iMaterials, 
        const real *texCoords,
		const IndexedFaceSet::FaceEdgeMark *iFaceEdgeMarks,
			  const unsigned *vindices, 
			  const unsigned *nindices,
        const unsigned *mindices,
        const unsigned *tindices,
			  const unsigned nvertices);

  void buildTriangleFan(const real *vertices, 
			const real *normals, 
      vector<FrsMaterial>&  iMaterials,
      const real *texCoords,
	  const IndexedFaceSet::FaceEdgeMark *iFaceEdgeMarks,
			const unsigned *vindices, 
			const unsigned *nindices,
      const unsigned *mindices,
      const unsigned *tindices,
			const unsigned nvertices);

  void buildTriangles(const real *vertices, 
		      const real *normals, 
          vector<FrsMaterial>&  iMaterials,
          const real *texCoords,
		  const IndexedFaceSet::FaceEdgeMark *iFaceEdgeMarks,
		      const unsigned *vindices, 
		      const unsigned *nindices,
          const unsigned *mindices,
          const unsigned *tindices,
		      const unsigned nvertices);

  void transformVertices(const real *vertices,
			 unsigned vsize, 
			 const Matrix44r& transform,
			 real *res);

  void transformNormals(const real *normals,
			unsigned nsize,
			const Matrix44r& transform,
			real *res);
 
  WShape*		_current_wshape;
  FrsMaterial*		_current_frs_material;
  WingedEdge*		_winged_edge;
  Matrix44r*		_current_matrix;
  vector<Matrix44r*>	_matrices_stack;
};

#endif // WINGED_EDGE_BUILDER_H
