/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to display textual information about a scene graph.
 */

#include <fstream>
#include <iostream>
#include <string>

#include "SceneVisitor.h"

using namespace std;

namespace Freestyle {

class ScenePrettyPrinter : public SceneVisitor {
 public:
  ScenePrettyPrinter(const string filename = "SceneLog.txt") : SceneVisitor()
  {
    if (!filename.empty()) {
      _ofs.open(filename.c_str());
    }
    if (!_ofs.is_open()) {
      cerr << "Warning, unable to open file \"" << filename << "\"" << endl;
    }
    _space = "";
  }

  virtual ~ScenePrettyPrinter()
  {
    if (_ofs.is_open()) {
      _ofs.close();
    }
  }

  //
  // visitClass methods
  //
  //////////////////////////////////////////////

  VISIT_DECL(Node);
  VISIT_DECL(NodeShape);
  VISIT_DECL(NodeGroup);
  VISIT_DECL(NodeLight);
  VISIT_DECL(NodeDrawingStyle);
  VISIT_DECL(NodeTransform);

  VISIT_DECL(LineRep);
  VISIT_DECL(OrientedLineRep);
  VISIT_DECL(TriangleRep);
  VISIT_DECL(VertexRep);
  VISIT_DECL(IndexedFaceSet);

  virtual void visitNodeShapeBefore(NodeShape &);
  virtual void visitNodeShapeAfter(NodeShape &);
  virtual void visitNodeGroupBefore(NodeGroup &);
  virtual void visitNodeGroupAfter(NodeGroup &);
  virtual void visitNodeDrawingStyleBefore(NodeDrawingStyle &);
  virtual void visitNodeDrawingStyleAfter(NodeDrawingStyle &);
  virtual void visitNodeTransformBefore(NodeTransform &);
  virtual void visitNodeTransformAfter(NodeTransform &);

 protected:
  void increaseSpace()
  {
    _space += "  ";
  }

  void decreaseSpace()
  {
    _space.erase(0, 2);
  }

 private:
  ofstream _ofs;
  string _space;
};

} /* namespace Freestyle */
