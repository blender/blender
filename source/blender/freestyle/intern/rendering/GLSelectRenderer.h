//
//  Filename         : GLSelectRenderer.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Class to highlight selected shapes
//  Date of creation : 09/01/2004
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

#ifndef  GL_SELECT_RENDERER_H
# define GL_SELECT_RENDERER_H

# include "GLRenderer.h"

class LIB_RENDERING_EXPORT GLSelectRenderer : public GLRenderer
{
 public:

  inline GLSelectRenderer() : GLRenderer() {
    _selected_shape = -1; // -1 means no selection
    _current_shape_active = false;
    _gl_select_rendering = false;
  }
  virtual ~GLSelectRenderer() {}

  //
  // visitClass methods
  //
  //////////////////////////////////////////////

  VISIT_DECL(NodeShape)

  VISIT_DECL(IndexedFaceSet)
  VISIT_DECL(Material)

  virtual void visitMaterial(const Material&);
  virtual void visitNodeShapeBefore(NodeShape&);
  virtual void visitNodeShapeAfter(NodeShape&);

  void resetColor();

  void setSelectedId(const int id) {
    _selected_shape = id;
  }

  void setSelectRendering(bool b) {
    _gl_select_rendering = b;
  }

  int getSelectedId() const {
    return _selected_shape;
  }

  bool getSelectRendering() const {
    return _gl_select_rendering;
  }

 private:

  int		_selected_shape;
  bool		_current_shape_active;
  bool		_gl_select_rendering;
};

#endif // GL_SELECT_RENDERER_H
