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

#ifndef __FREESTYLE_SCENE_PRETTY_PRINTER_H__
#define __FREESTYLE_SCENE_PRETTY_PRINTER_H__

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

#endif  // __FREESTYLE_SCENE_PRETTY_PRINTER_H__
