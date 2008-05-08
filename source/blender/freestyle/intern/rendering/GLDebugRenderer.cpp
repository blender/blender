
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

#include <stdio.h>
#include "../scene_graph/VertexRep.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/IndexedFaceSet.h"
#include "../scene_graph/LineRep.h"
#include "../geometry/Grid.h"

#include "GLDebugRenderer.h"

#ifdef __MACH__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

void GLDebugRenderer::visitIndexedFaceSet(IndexedFaceSet& iFaceSet)  
{
  unsigned int fIndex = 0;
  
  const real * vertices = iFaceSet.vertices();
  const real * normals = iFaceSet.normals();
  const Material *const* materials = (const Material**)iFaceSet.materials();
  const unsigned *vindices = iFaceSet.vindices();
  const unsigned *nindices = iFaceSet.nindices();
  const unsigned *mindices = iFaceSet.mindices();
  const unsigned numfaces = iFaceSet.numFaces();
  const unsigned *numVertexPerFace = iFaceSet.numVertexPerFaces();
  const IndexedFaceSet::TRIANGLES_STYLE * faceStyle = iFaceSet.trianglesStyle();  

  const unsigned *pvi = vindices;
  const unsigned *pni = nindices;
  const unsigned* pmi = mindices;

  for(fIndex=0; fIndex<numfaces; fIndex++)
  {
    switch(faceStyle[fIndex])
    {
    case IndexedFaceSet::TRIANGLE_STRIP:
      RenderTriangleStrip(vertices, normals, materials, pvi, pni, pmi, numVertexPerFace[fIndex]);
      break;
    case IndexedFaceSet::TRIANGLE_FAN:
      RenderTriangleFan(vertices, normals, materials, pvi, pni, pmi, numVertexPerFace[fIndex]);
      break;
    case IndexedFaceSet::TRIANGLES:
      RenderTriangles(vertices, normals, materials, pvi, pni, pmi, numVertexPerFace[fIndex]);
      break;
    }
    pvi += numVertexPerFace[fIndex];
    pni += numVertexPerFace[fIndex];
    pmi += numVertexPerFace[fIndex];
  }
}

void GLDebugRenderer::visitNodeShape(NodeShape& iShapeNode) 
{
  // Gets the bbox size:
  real minY = iShapeNode.bbox().getMin()[1];
  real maxY = iShapeNode.bbox().getMax()[1];
  
  // sets the text size:
  _bboxSize = fabs((maxY-minY)); ///1000.f;
}

void GLDebugRenderer::visitLineRep(LineRep& iLine) 
{ 

  glColor3f(0,0,0);

  GLRenderer::visitLineRep(iLine);
}

void GLDebugRenderer::visitOrientedLineRep(OrientedLineRep& iLine) 
{
  GLRenderer::visitOrientedLineRep(iLine);
}

void GLDebugRenderer::visitVertexRep(VertexRep& iVertex) 
{
  glPointSize(3.0);

  GLRenderer::visitVertexRep(iVertex);
}


void GLDebugRenderer::renderBitmapString(real x, 
					 real y, 
					 real z, 
					 void *font, 
					 char *string,
					 float size) 
{  
  char *c;
  
  glPushMatrix();
  glTranslater(x, y,z);
  real textSize = min(_bboxSize/10.0, _minEdgeSize/2.0);
  // adjust the text size so as it 
  // is acceptable giving the bbox size:
  while(_bboxSize/textSize>1000)
    textSize *= 10.0;

  glScalef(size, size, size);
  glScalef(textSize/200.0, textSize/200.0, textSize/200.0);
  for (c=string; *c != '\0'; c++) 
  {
    glutStrokeCharacter(font, *c);
  }
  glPopMatrix();
}

void GLDebugRenderer::RenderTriangleStrip(const real *iVertices, 
                                          const real *iNormals,
                                          const Material *const* iMaterials, 
                                          const unsigned* iVIndices, 
                                          const unsigned* iNIndices,
                                          const unsigned* iMIndices,
                                          const unsigned iNVertices) 
{
  //  glBegin(GL_TRIANGLE_STRIP);
  //  for(unsigned int i=0; i<iNVertices; i++)
  //  {
  //    glNormal3r(iNormals[iNIndices[i]], 
  //               iNormals[iNIndices[i]+1],
  //               iNormals[iNIndices[i]+2]);
  //
  //    glVertex3r( iVertices[iVIndices[i]], 
  //                iVertices[iVIndices[i]+1],
  //                iVertices[iVIndices[i]+2]);
  //  }
  //  glEnd();
}

void GLDebugRenderer::RenderTriangleFan(const real *iVertices, 
                                        const real *iNormals,
                                        const Material *const* iMaterials, 
                                        const unsigned* iVIndices, 
                                        const unsigned* iNIndices,
                                        const unsigned* iMIndices,
                                        const unsigned iNVertices)  
{
  //  glBegin(GL_TRIANGLE_FAN);
  //  for(unsigned int i=0; i<iNVertices; i++)
  //  {
  //    glNormal3r(iNormals[iNIndices[i]], 
  //               iNormals[iNIndices[i]+1],
  //               iNormals[iNIndices[i]+2]);
  //
  //    glVertex3r( iVertices[iVIndices[i]], 
  //                iVertices[iVIndices[i]+1],
  //                iVertices[iVIndices[i]+2]);
  //  }
  //  glEnd();
}

void GLDebugRenderer::RenderTriangles(const real *iVertices, 
                                      const real *iNormals,
                                      const Material *const* iMaterials, 
                                      const unsigned* iVIndices, 
                                      const unsigned* iNIndices,
                                      const unsigned* iMIndices,
                                      const unsigned iNVertices)  
{
  //  // Renders the normals:
  //  glBegin(GL_LINES);
  //  for(unsigned int i=0; i<iNVertices; i++)
  //  { 
  //    glVertex3r( iVertices[iVIndices[i]], 
  //      iVertices[iVIndices[i]+1],
  //      iVertices[iVIndices[i]+2]);
  //    
  //    glVertex3r(iVertices[iVIndices[i]] + iNormals[iNIndices[i]]/10.f, 
  //      iVertices[iVIndices[i]+1] + iNormals[iNIndices[i]+1]/10.f,
  //      iVertices[iVIndices[i]+2] + iNormals[iNIndices[i]+2]/10.f);
  //  }
  //  glEnd();
}
