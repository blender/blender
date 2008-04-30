
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

#include "../scene_graph/IndexedFaceSet.h"
#include "../scene_graph/NodeShape.h"
#include "GLSelectRenderer.h"

static const float	INACTIVE_COLOR_MIN = 0.2;
static const float	INACTIVE_COLOR_MAX = 0.8;
static const float	INACTIVE_COLOR_OFFSET = 0.2;

static const float	ACTIVE_COLOR[4] = {0.8,
					   0.2,
					   0.2,
					   1};

static float		selection_color[4] = {INACTIVE_COLOR_MIN,
					      INACTIVE_COLOR_MIN,
					      INACTIVE_COLOR_MIN,
					      1};


void GLSelectRenderer::resetColor() {
  for (unsigned i = 0; i < 3; ++i)
    selection_color[i] = INACTIVE_COLOR_MIN;
}

void GLSelectRenderer::visitNodeShape(NodeShape& sn) {
  if (_gl_select_rendering)
    return;
  for (unsigned i = 0; i < 3; ++i) {
    selection_color[i] += INACTIVE_COLOR_OFFSET;
    if (selection_color[i] > INACTIVE_COLOR_MAX)
      selection_color[i] = INACTIVE_COLOR_MIN;
  }
  if (sn.shapes()[0]->getId() == _selected_shape) {
    _current_shape_active = true;
    return;
  }
  _current_shape_active = false;
}

void GLSelectRenderer::visitMaterial(Material& m) {
  if (_gl_select_rendering)
    return;

  const float* amb = m.ambient();
  const float* spec = m.specular();
  const float* em = m.emission();

  if (_current_shape_active) {
    RenderColor(ACTIVE_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, ACTIVE_COLOR);
  } else {
    RenderColor(selection_color);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, selection_color);
  }
  glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT, GL_EMISSION, em);
  glMaterialf(GL_FRONT, GL_SHININESS, m.shininess());
}

void GLSelectRenderer::visitMaterial(const Material& m) {
  if (_gl_select_rendering)
    return;

  const float* amb = m.ambient();
  const float* spec = m.specular();
  const float* em = m.emission();

  if (_current_shape_active) {
    RenderColor(ACTIVE_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, ACTIVE_COLOR);
  } else {
    RenderColor(selection_color);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, selection_color);
  }
  glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT, GL_EMISSION, em);
  glMaterialf(GL_FRONT, GL_SHININESS, m.shininess());
}

void GLSelectRenderer::visitIndexedFaceSet(IndexedFaceSet& ifs)
{
  unsigned int fIndex = 0;
  
  const real * vertices = ifs.vertices();
  const real * normals = ifs.normals();
  const Material *const* materials = ifs.materials();
  const real * texCoords= ifs.texCoords();
  const unsigned *vindices = ifs.vindices();
  const unsigned *nindices = ifs.nindices();
  const unsigned *mindices = ifs.mindices();
  const unsigned *tindices = ifs.tindices();
  const unsigned numfaces = ifs.numFaces();
  const IndexedFaceSet::TRIANGLES_STYLE * faceStyle = ifs.trianglesStyle();
  const unsigned *numVertexPerFace = ifs.numVertexPerFaces();
  

  const unsigned* pvi = vindices;
  const unsigned* pni = nindices;
  const unsigned* pmi = mindices;
  const unsigned* pti = tindices;

  for(fIndex=0; fIndex<numfaces; fIndex++)
  {
    switch(faceStyle[fIndex])
    {
    case IndexedFaceSet::TRIANGLE_STRIP:
      RenderTriangleStrip(vertices, normals, materials, texCoords, pvi, pni, pmi, pti, numVertexPerFace[fIndex]);
      break;
    case IndexedFaceSet::TRIANGLE_FAN:
      RenderTriangleFan(vertices, normals, materials, texCoords, pvi, pni, pmi, pti, numVertexPerFace[fIndex]);
      break;
    case IndexedFaceSet::TRIANGLES:
      RenderTriangles(vertices, normals, materials, texCoords, pvi, pni, pmi, pti, numVertexPerFace[fIndex]);
      break;
    }
    pvi += numVertexPerFace[fIndex];
    pni += numVertexPerFace[fIndex];
    pmi += numVertexPerFace[fIndex];
    if(pti)
        pti += numVertexPerFace[fIndex];
  }
}

void GLSelectRenderer::visitNodeShapeBefore(NodeShape& sn) {
  if (!_gl_select_rendering)
    return;
  
  glPushName(sn.shapes()[0]->getId().getFirst());
}

void GLSelectRenderer::visitNodeShapeAfter(NodeShape& sn) {
  if (!_gl_select_rendering)
    return;

  glPopName();
}
