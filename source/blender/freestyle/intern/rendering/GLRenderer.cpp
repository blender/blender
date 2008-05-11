
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
#include "../scene_graph/NodeDrawingStyle.h"
#include "../scene_graph/NodeLight.h"
#include "../scene_graph/NodeCamera.h"
#include "../scene_graph/NodeTransform.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/OrientedLineRep.h"
#include "../scene_graph/VertexRep.h"
#include "../stroke/Stroke.h"

#include "../scene_graph/TriangleRep.h"

#include "GLRenderer.h"

static GLenum lights[8] = {GL_LIGHT0, 
                    GL_LIGHT1, 
                    GL_LIGHT2, 
                    GL_LIGHT3, 
                    GL_LIGHT4,
                    GL_LIGHT5,
                    GL_LIGHT6,
                    GL_LIGHT7};

void GLRenderer::visitIndexedFaceSet(IndexedFaceSet& ifs)
{
  /*GLuint dl = ifs.displayList();
  if(dl != 0){
    glCallList(dl);
    return;
  }*/
  unsigned int fIndex = 0;
  
  const real * vertices = ifs.vertices();
  const real * normals = ifs.normals();
  const real * texCoords = ifs.texCoords();
  const Material *const* materials = ifs.materials();
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

  //dl = glGenLists(1);
  //glNewList(dl, GL_COMPILE_AND_EXECUTE);
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
		if(pmi)
			pmi += numVertexPerFace[fIndex];
    if(pti)
        pti += numVertexPerFace[fIndex];
  }
  //glEndList();
  //ifs.SetDisplayList(dl);
}

void GLRenderer::visitNodeTransform(NodeTransform& tn) {
  if(tn.scaled())
    glEnable(GL_NORMALIZE);
}

void GLRenderer::visitNodeTransformBefore(NodeTransform& tn) {
  glPushMatrix();

  // Now apply transform
  applyTransform(tn.matrix());
}

void GLRenderer::visitNodeTransformAfter(NodeTransform& tn) {
  glPopMatrix();
}

void GLRenderer::visitNodeLight(NodeLight& ln)  
{
  if(true != ln.isOn())
    return;

  int number = ln.number();
  
  glLightfv(lights[number], GL_AMBIENT, ln.ambient());
  glLightfv(lights[number], GL_DIFFUSE, ln.diffuse());
  glLightfv(lights[number], GL_SPECULAR, ln.specular());
  glLightfv(lights[number], GL_POSITION, ln.position());

  glEnable(lights[number]);
}

void GLRenderer::visitNodeCamera(NodeCamera& cn)  
{
    const double * mvm = cn.modelViewMatrix();
    const double * pm = cn.projectionMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMultMatrixd(pm);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMultMatrixd(mvm);


}

void GLRenderer::visitNodeDrawingStyleBefore(NodeDrawingStyle& ds) {
  glPushAttrib(GL_ALL_ATTRIB_BITS);
}

void GLRenderer::visitNodeDrawingStyleAfter(NodeDrawingStyle&) {
  glPopAttrib();
}

void GLRenderer::RenderTriangleStrip( const real *iVertices, 
                                     const real *iNormals,
                                     const Material *const* iMaterials,
                                     const real *iTexCoords,
                                     const unsigned* iVIndices, 
                                     const unsigned* iNIndices,
                                     const unsigned* iMIndices,
                                     const unsigned* iTIndices,
                                     const unsigned iNVertices) 
{
  unsigned index = -1;
  glBegin(GL_TRIANGLE_STRIP);
  for(unsigned int i=0; i<iNVertices; i++)
  {
		if(iMIndices){
			if(iMIndices[i] != index){
				visitMaterial(*(iMaterials[iMIndices[i]]));
				index = iMIndices[i];
			}
		}

        if(iTIndices){
            glTexCoord2f(   iTexCoords[iTIndices[i]],
                            iTexCoords[iTIndices[i]+1]);
		}
    
    glNormal3r(iNormals[iNIndices[i]], 
               iNormals[iNIndices[i]+1],
               iNormals[iNIndices[i]+2]);

    glVertex3r( iVertices[iVIndices[i]], 
                iVertices[iVIndices[i]+1],
                iVertices[iVIndices[i]+2]);
  }
  glEnd();
}

void GLRenderer::RenderTriangleFan( const real *iVertices, 
                                    const real *iNormals,
                                    const Material *const* iMaterials,
                                    const real *iTexCoords,
                                    const unsigned* iVIndices, 
                                    const unsigned* iNIndices,
                                    const unsigned* iMIndices,
                                    const unsigned* iTIndices,
                                    const unsigned iNVertices)  
{
  unsigned index = -1;
  glBegin(GL_TRIANGLE_FAN);
  for(unsigned int i=0; i<iNVertices; i++)
  {
		if(iMIndices){
			if(iMIndices[i] != index){
				visitMaterial(*(iMaterials[iMIndices[i]]));
				index = iMIndices[i];
			}
		}
        if(iTIndices){
            glTexCoord2f(   iTexCoords[iTIndices[i]],
                            iTexCoords[iTIndices[i]+1]);
		}

    glNormal3r(iNormals[iNIndices[i]], 
               iNormals[iNIndices[i]+1],
               iNormals[iNIndices[i]+2]);

    glVertex3r( iVertices[iVIndices[i]], 
                iVertices[iVIndices[i]+1],
                iVertices[iVIndices[i]+2]);
  }
  glEnd();
}

void GLRenderer::RenderTriangles( const real *iVertices, 
                                 const real *iNormals,
                                 const Material *const* iMaterials,
                                 const real *iTexCoords,
                                 const unsigned* iVIndices, 
                                 const unsigned* iNIndices,
                                 const unsigned* iMIndices,
                                 const unsigned* iTIndices,
                                 const unsigned iNVertices)  
{
  unsigned index = -1;
  glBegin(GL_TRIANGLES);
  for(unsigned int i=0; i<iNVertices; i++)
  {
		if(iMIndices){
			if(iMIndices[i] != index){
				visitMaterial(*(iMaterials[iMIndices[i]]));
				index = iMIndices[i];
			}
		}
        if(iTIndices){
            glTexCoord2f(   iTexCoords[iTIndices[i]],
                            iTexCoords[iTIndices[i]+1]);
		}

    glNormal3r(iNormals[iNIndices[i]], 
               iNormals[iNIndices[i]+1],
               iNormals[iNIndices[i]+2]);

    glVertex3r( iVertices[iVIndices[i]], 
                iVertices[iVIndices[i]+1],
                iVertices[iVIndices[i]+2]);
  }
  glEnd();
}

void GLRenderer::visitLineRep( LineRep& iLine) 
{
  if(iLine.width() != 0)
    glLineWidth(iLine.width());

  switch(iLine.style())
  {
  case LineRep::LINES:
    glBegin(GL_LINES);
    break;
  case LineRep::LINE_STRIP:
    glBegin(GL_LINE_STRIP);
    break;
  case LineRep::LINE_LOOP:
    glBegin(GL_LINE_LOOP);
    break;
  default:
    return;
  }

 const vector<Vec3r>& vertices = iLine.vertices();
  //soc unused float step=1.f/vertices.size();
  vector<Vec3r>::const_iterator v;

  for(v=vertices.begin(); v!=vertices.end(); v++)
    glVertex3r((*v)[0], (*v)[1], (*v)[2]);

  glEnd();
}


void GLRenderer::visitTriangleRep( TriangleRep& iTriangle) 
{
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  switch(iTriangle.style())
  {
  case TriangleRep::FILL:
    glPolygonMode(GL_FRONT, GL_FILL);
    break;
  case TriangleRep::LINES:
    glPolygonMode(GL_FRONT, GL_LINES);
    break;
  default:
    return;
  }

  glBegin(GL_TRIANGLES);
  for(int i=0; i<3; ++i)
  {
    glColor3f(iTriangle.color(i)[0], iTriangle.color(i)[1], iTriangle.color(i)[2]);
    glVertex3r(iTriangle.vertex(i)[0], iTriangle.vertex(i)[1], iTriangle.vertex(i)[2]);
  }

  glEnd();



  glPopAttrib();

}

void GLRenderer::visitOrientedLineRep(OrientedLineRep& iLine) 
{
  switch(iLine.style())
  {
  case LineRep::LINES:
    glBegin(GL_LINES);
    break;
  case LineRep::LINE_STRIP:
    glBegin(GL_LINE_STRIP);
    break;
  case LineRep::LINE_LOOP:
    glBegin(GL_LINE_LOOP);
    break;
  default:
    return;
  }

  int i=0;
  int ncolor = iLine.getId().getFirst()%3;
  
  const vector<Vec3r>& vertices = iLine.vertices();
  float step=1.f/vertices.size();
  vector<Vec3r>::const_iterator v;
  for(v=vertices.begin(); v!=vertices.end(); v++)
  {
    switch(ncolor)
    {
    case 0:
      glColor3f(i*step,0.f,0.f);
      break;
    case 1:
      glColor3f(0.f, i*step, 0.f);
      break;
    case 2:
      glColor3f(0.f, 0.f, i*step);
      break;
    default:
      glColor3f(i*step, i*step,i*step);
      break;
    }
    i++;
    glVertex3r((*v)[0], (*v)[1], (*v)[2]);
  }

  glEnd();
}

void GLRenderer::visitVertexRep( VertexRep& iVertex) 
{
  if(iVertex.pointSize() != 0.f)
    glPointSize(iVertex.pointSize());
  
  glBegin(GL_POINTS);
  glVertex3r(iVertex.x(), iVertex.y(), iVertex.z());
  glEnd();
}

void GLRenderer::visitDrawingStyle(DrawingStyle& iDrawingStyle) 
{
  
  // Drawing Style management
  switch(iDrawingStyle.style())
  {
  case DrawingStyle::FILLED:
    glPolygonMode(GL_FRONT, GL_FILL);
    glShadeModel(GL_SMOOTH);
    break;

  case DrawingStyle::LINES:
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_LINE_SMOOTH);
    glPolygonMode(GL_FRONT, GL_LINE);
    glLineWidth(iDrawingStyle.lineWidth());
    break;

  case DrawingStyle::POINTS:
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_POINT_SMOOTH);
    glPolygonMode(GL_FRONT, GL_POINT);
    glPointSize(iDrawingStyle.pointSize());
    break;

  case DrawingStyle::INVISIBLE:
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(0);
    break;

  default:
    break;
  }

  glLineWidth(iDrawingStyle.lineWidth());
  glPointSize(iDrawingStyle.pointSize());

  // FIXME
  if(true == iDrawingStyle.lightingEnabled())
    glEnable(GL_LIGHTING);
  else
    glDisable(GL_LIGHTING);
}

void GLRenderer::visitMaterial(Material& m) {
  const float* diff = m.diffuse();
  const float* amb = m.ambient();
  const float* spec = m.specular();
  const float* em = m.emission();

  RenderColor(diff);
  glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT, GL_EMISSION, em);
  glMaterialf(GL_FRONT, GL_SHININESS, m.shininess());
}

void GLRenderer::visitMaterial(const Material& m) {
  const float* diff = m.diffuse();
  const float* amb = m.ambient();
  const float* spec = m.specular();
  const float* em = m.emission();

  RenderColor(diff);
  glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
  glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
  glMaterialfv(GL_FRONT, GL_EMISSION, em);
  glMaterialf(GL_FRONT, GL_SHININESS, m.shininess());
}
void GLRenderer::applyTransform( const Matrix44r &iMatrix)  
{
  real m[16];
  for(int lign=0; lign<4; lign++)
    for(int column=0; column<4; column++)
      m[column*4+lign] = iMatrix(lign, column);

  glMultMatrixr(m);
}

void GLRenderer::RenderColor( const float *rgba)  
{
  glColor4fv(rgba);
} 
