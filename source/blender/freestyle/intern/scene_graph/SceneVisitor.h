/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to visit (without doing anything) a scene graph structure
 */

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

#define VISIT_COMPLETE_DEF(type) \
  virtual void visit##type(type &) {} \
  virtual void visit##type##Before(type &) {} \
  virtual void visit##type##After(type &) {}

#define VISIT_DECL(type) virtual void visit##type(type &)

#define VISIT_COMPLETE_DECL(type) \
  virtual void visit##type##Before(type &); \
  virtual void visit##type(type &); \
  virtual void visit##type##After(type &)

class Node;
class NodeShape;
class NodeGroup;
class NodeLight;
class NodeCamera;
class NodeDrawingStyle;
class NodeTransform;
class NodeViewLayer;

class Rep;
class LineRep;
class OrientedLineRep;
class TriangleRep;
class VertexRep;
class IndexedFaceSet;
class DrawingStyle;
class FrsMaterial;

class SceneVisitor {
 public:
  SceneVisitor() {}
  virtual ~SceneVisitor() {}

  virtual void beginScene() {}
  virtual void endScene() {}

  //
  // visitClass methods
  //
  //////////////////////////////////////////////

  VISIT_COMPLETE_DEF(Node)
  VISIT_COMPLETE_DEF(NodeShape)
  VISIT_COMPLETE_DEF(NodeGroup)
  VISIT_COMPLETE_DEF(NodeLight)
  VISIT_COMPLETE_DEF(NodeCamera)
  VISIT_COMPLETE_DEF(NodeDrawingStyle)
  VISIT_COMPLETE_DEF(NodeTransform)
  VISIT_COMPLETE_DEF(NodeViewLayer)

  VISIT_COMPLETE_DEF(Rep)
  VISIT_COMPLETE_DEF(LineRep)
  VISIT_COMPLETE_DEF(OrientedLineRep)
  VISIT_COMPLETE_DEF(TriangleRep)
  VISIT_COMPLETE_DEF(VertexRep)
  VISIT_COMPLETE_DEF(IndexedFaceSet)
  VISIT_COMPLETE_DEF(DrawingStyle)
  VISIT_COMPLETE_DEF(FrsMaterial)

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SceneVisitor")
#endif
};

} /* namespace Freestyle */
