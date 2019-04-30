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

#ifndef __FREESTYLE_WINGED_EDGE_BUILDER_H__
#define __FREESTYLE_WINGED_EDGE_BUILDER_H__

/** \file
 * \ingroup freestyle
 * \brief Class to render a WingedEdge data structure
 * from a polyhedral data structure organized in nodes of a scene graph.
 */

#include "WEdge.h"

#include "../scene_graph/IndexedFaceSet.h"
#include "../scene_graph/NodeTransform.h"
#include "../scene_graph/SceneVisitor.h"

#include "../system/FreestyleConfig.h"
#include "../system/RenderMonitor.h"

namespace Freestyle {

class WingedEdgeBuilder : public SceneVisitor {
 public:
  inline WingedEdgeBuilder() : SceneVisitor()
  {
    _current_wshape = NULL;
    _current_frs_material = NULL;
    _current_matrix = NULL;
    _winged_edge = new WingedEdge;  // Not deleted by the destructor
    _pRenderMonitor = NULL;
  }

  virtual ~WingedEdgeBuilder()
  {
    for (vector<Matrix44r *>::iterator it = _matrices_stack.begin(); it != _matrices_stack.end();
         ++it)
      delete *it;
    _matrices_stack.clear();
  }

  VISIT_DECL(IndexedFaceSet)
  VISIT_DECL(NodeShape)
  VISIT_DECL(NodeTransform)

  virtual void visitNodeTransformAfter(NodeTransform &);

  //
  // Accessors
  //
  /////////////////////////////////////////////////////////////////////////////

  inline WingedEdge *getWingedEdge()
  {
    return _winged_edge;
  }

  inline WShape *getCurrentWShape()
  {
    return _current_wshape;
  }

  inline FrsMaterial *getCurrentFrsMaterial()
  {
    return _current_frs_material;
  }

  inline Matrix44r *getCurrentMatrix()
  {
    return _current_matrix;
  }

  //
  // Modifiers
  //
  /////////////////////////////////////////////////////////////////////////////

  inline void setCurrentWShape(WShape *wshape)
  {
    _current_wshape = wshape;
  }

  inline void setCurrentFrsMaterial(FrsMaterial *mat)
  {
    _current_frs_material = mat;
  }

#if 0
  inline void setCurrentMatrix(Matrix44r *matrix)
  {
    _current_matrix = matrix;
  }
#endif

  inline void setRenderMonitor(RenderMonitor *iRenderMonitor)
  {
    _pRenderMonitor = iRenderMonitor;
  }

 protected:
  virtual bool buildWShape(WShape &shape, IndexedFaceSet &ifs);
  virtual void buildWVertices(WShape &shape, const float *vertices, unsigned vsize);

  RenderMonitor *_pRenderMonitor;

 private:
  void buildTriangleStrip(const float *vertices,
                          const float *normals,
                          vector<FrsMaterial> &iMaterials,
                          const float *texCoords,
                          const IndexedFaceSet::FaceEdgeMark *iFaceEdgeMarks,
                          const unsigned *vindices,
                          const unsigned *nindices,
                          const unsigned *mindices,
                          const unsigned *tindices,
                          const unsigned nvertices);

  void buildTriangleFan(const float *vertices,
                        const float *normals,
                        vector<FrsMaterial> &iMaterials,
                        const float *texCoords,
                        const IndexedFaceSet::FaceEdgeMark *iFaceEdgeMarks,
                        const unsigned *vindices,
                        const unsigned *nindices,
                        const unsigned *mindices,
                        const unsigned *tindices,
                        const unsigned nvertices);

  void buildTriangles(const float *vertices,
                      const float *normals,
                      vector<FrsMaterial> &iMaterials,
                      const float *texCoords,
                      const IndexedFaceSet::FaceEdgeMark *iFaceEdgeMarks,
                      const unsigned *vindices,
                      const unsigned *nindices,
                      const unsigned *mindices,
                      const unsigned *tindices,
                      const unsigned nvertices);

  void transformVertices(const float *vertices,
                         unsigned vsize,
                         const Matrix44r &transform,
                         float *res);

  void transformNormals(const float *normals,
                        unsigned nsize,
                        const Matrix44r &transform,
                        float *res);

  WShape *_current_wshape;
  FrsMaterial *_current_frs_material;
  WingedEdge *_winged_edge;
  Matrix44r *_current_matrix;
  vector<Matrix44r *> _matrices_stack;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WingedEdgeBuilder")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_WINGED_EDGE_BUILDER_H__
