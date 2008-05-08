
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

#include "GLMonoColorRenderer.h"

void GLMonoColorRenderer::visitMaterial(Material&) {
  glColor3f(_r, _g, _b);
}

void GLMonoColorRenderer::visitDrawingStyle(DrawingStyle&) {
  glDisable(GL_LIGHTING);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);
  glLineWidth(3.0);
  glPolygonMode(GL_BACK, GL_LINE);
  //glPolygonMode(GL_BACK, GL_FILL);
}

void GLMonoColorRenderer::setColor(float r, float g, float b, float alpha) {
  _r = r;
  _g = g;
  _b = b;
  _alpha = alpha;
}
