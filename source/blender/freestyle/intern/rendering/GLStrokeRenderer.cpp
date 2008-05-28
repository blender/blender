
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

#include "GLStrokeRenderer.h"

//soc
// #include <qimage.h>
// #include <qfileinfo.h>
// #include <qgl.h>
// #include <qfile.h>

extern "C" {
#include "BLI_blenlib.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}

#include "../system/StringUtils.h"

#ifdef WIN32
# include "extgl.h"
#endif // WIN32

//#define glBlendEquation(x)

GLStrokeRenderer::GLStrokeRenderer()
:StrokeRenderer()
{
  _textureManager = new GLTextureManager;
}

GLStrokeRenderer::~GLStrokeRenderer()
{
  if(0 != _textureManager)
  {
    delete _textureManager;
    _textureManager = 0;
  }
}

float initialColor(float x, float avTex=0.5)
{
  float y=(1-x)/avTex;
  return (y>1 ? 1 : y);
}
//float complementColor(float x, float avTex=0.5)
//{
//  float y=(1-x)/avTex-1;
//  return (y<0 ? 0 : y);
//}

float complementColor(float x, float avTex=0.5)
{
  float y=(1-x);///avTex-1;
  return (y<0 ? 0 : y);
}

void GLStrokeRenderer::RenderStrokeRep(StrokeRep *iStrokeRep) const
{
  glPushAttrib(GL_COLOR_BUFFER_BIT);
  Stroke::MediumType strokeType = iStrokeRep->getMediumType();
  // int averageTextureAlpha=0.5; //default value
  // if (strokeType==OIL_STROKE)
    // averageTextureAlpha=0.75; 
  // if (strokeType>=NO_BLEND_STROKE)
    // averageTextureAlpha=1.0; 
  // if (strokeType<0)
    // {
      // renderNoTexture(iStrokeRep);
      // return;
    // }
  //soc unused - int i;
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glShadeModel(GL_SMOOTH);
  glDisable(GL_DEPTH_TEST);
    
  glEnable(GL_BLEND);

  if(strokeType==Stroke::DRY_MEDIUM)
    {
      glBlendEquation(GL_MAX);
    }
  else if(strokeType==Stroke::OPAQUE_MEDIUM)
    {
      glBlendEquation(GL_ADD);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
  else
    {
      glBlendEquation(GL_ADD);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
  glEnable(GL_TEXTURE_2D);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  //first texture, basically the only one for lighter strokes
  glBindTexture(GL_TEXTURE_2D, iStrokeRep->getTextureId()); 
  //glBindTexture(GL_TEXTURE_2D, _textureManager.getPaperTextureIndex()); 
  
  vector<Strip*>& strips = iStrokeRep->getStrips();
  for(vector<Strip*>::iterator s=strips.begin(), send=strips.end();
  s!=send;
  ++s){
    Strip::vertex_container& vertices = (*s)->vertices();
    glBegin(GL_TRIANGLE_STRIP);
    for(Strip::vertex_container::iterator v=vertices.begin(), vend=vertices.end();
    v!=vend;
    ++v){
      StrokeVertexRep * svRep = (*v);
      Vec3r color = svRep->color();
      real alpha = svRep->alpha();
      glColor4f(complementColor(color[0]), 
      complementColor(color[1]), 
      complementColor(color[2]), alpha);
      glTexCoord2f(svRep->texCoord()[0],svRep->texCoord()[1] );
      glVertex2f(svRep->point2d()[0], svRep->point2d()[1]);
    }  
    glEnd();
  }
//  if (strokeType>=NO_BLEND_STROKE) return;
  //  //return;
  //
  //  //second texture, the complement, for stronger strokes
  //  glBindTexture(GL_TEXTURE_2D, _textureManager.getTextureIndex(2*strokeType+1)); 
  //  glBegin(GL_TRIANGLE_STRIP);
  //  for(i=0; i<_sizeStrip; i++)
  //  {
  //    glColor4f(complementColor(_color[i][0]), 
  //      complementColor(_color[i][1]), 
  //      complementColor(_color[i][2]), _alpha[i]);
  //    glTexCoord2f(_texCoord[i][0],_texCoord[i][1] );
  //    glVertex2f(_vertex[i][0], _vertex[i][1]);
  //  }
  //  glEnd();

  glPopAttrib();
}

void GLStrokeRenderer::RenderStrokeRepBasic(StrokeRep *iStrokeRep) const
{
  glPushAttrib(GL_COLOR_BUFFER_BIT);
  //soc unused - Stroke::MediumType strokeType = iStrokeRep->getMediumType();
  //soc unused - int i;
  glDisable(GL_CULL_FACE);
  glDisable(GL_LIGHTING);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glShadeModel(GL_SMOOTH);
  glDisable(GL_DEPTH_TEST);
    
  glEnable(GL_BLEND);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_TEXTURE_2D);
  glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  //first texture, basically the only one for lighter strokes
  glBindTexture(GL_TEXTURE_2D, iStrokeRep->getTextureId()); 
  //glBindTexture(GL_TEXTURE_2D, _textureManager.getPaperTextureIndex()); 
  
  vector<Strip*>& strips = iStrokeRep->getStrips();
  for(vector<Strip*>::iterator s=strips.begin(), send=strips.end();
  s!=send;
  ++s){
    Strip::vertex_container& vertices = (*s)->vertices();
    glBegin(GL_TRIANGLE_STRIP);
    for(Strip::vertex_container::iterator v=vertices.begin(), vend=vertices.end();
    v!=vend;
    ++v){
      StrokeVertexRep * svRep = (*v);
      Vec3r color = svRep->color();
      real alpha = svRep->alpha();
      glColor4f(color[0], 
              color[1], 
              color[2], alpha);
      glTexCoord2f(svRep->texCoord()[0],svRep->texCoord()[1] );
      glVertex2f(svRep->point2d()[0], svRep->point2d()[1]);
    }  
    glEnd();
  }
  glPopAttrib();
}

//No Texture
//void GLStrokeRenderer::renderNoTexture(StrokeRep *iStrokeRep) const
//{
//  Stroke::MediumType strokeType = iStrokeRep->getMediumType();
//  int sizeStrip = iStrokeRep->sizeStrip();
//  const Vec3r *color = iStrokeRep->colors();
//  const Vec2r *vertex = iStrokeRep->vertices();
//  const float *alpha = iStrokeRep->alpha();
//  
//  glDisable(GL_LIGHTING);
//  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
//  glShadeModel(GL_SMOOTH);
//  glDisable(GL_DEPTH_TEST);
//
//  //if (strokeType==NO_TEXTURE_STROKE)
//  if(strokeType < 0)
//    {
//      glDisable(GL_BLEND);
//      glDisable(GL_TEXTURE_2D);
//      glBegin(GL_TRIANGLE_STRIP);
//      for(int i=0; i<sizeStrip; i++)
//	{ 
//	  glColor4f(complementColor(color[i][0]), 
//		    complementColor(color[i][1]), 
//		    complementColor(color[i][2]), alpha[i]);
//	  glVertex2f(vertex[i][0], vertex[i][1]);
//	} 
//      glEnd();
//    } 
//  else
//    {
//      //#ifdef WIN32
//      //glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
//      glBlendEquation(GL_ADD);
//      //#endif
//      glEnable(GL_BLEND);
//      glBlendFunc(GL_SRC_ALPHA, GL_ONE);
//      glDisable(GL_TEXTURE_2D);
//      glBegin(GL_TRIANGLE_STRIP);
//      for(int i=0; i<sizeStrip; i++)
//	{ 
//	  glColor4f(complementColor(color[i][0]), 
//		    complementColor(color[i][1]), 
//		    complementColor(color[i][2]), alpha[i]);
//	  glVertex2f(vertex[i][0], vertex[i][1]);
//	} 
//      glEnd();
//    }
//  //	cerr<<"color="<<_color[1][0]<<", "<<_color[1][1]<<", "<<_color[1][2]<<") "<<endl;
//	
//	
//}


/**********************************/
/*                                */
/*                                */
/*         GLTextureManager         */
/*                                */
/*                                */
/**********************************/

//#define TEXTURES_DIR ROOT_DIR "/data/textures"

GLTextureManager::GLTextureManager ()
: TextureManager()
{
  //_brushes_path = Config::getInstance()...
}

GLTextureManager::~GLTextureManager ()
{
}

void
GLTextureManager::loadPapers ()
{
  unsigned size = _papertextures.size();
  _papertexname = new unsigned[size];
  GLuint *tmp = new GLuint[size];
  glGenTextures(size, tmp);
  for(unsigned i=0;i<size;++i){
    _papertexname[i] = tmp[i];
  }
  delete [] tmp;

  // Papers textures
  cout << "Loading papers textures..." << endl;

  for (unsigned i = 0; i < size; i++){
	cout << i << ": " << _papertextures[i] << endl;
	preparePaper(_papertextures[i].c_str(), _papertexname[i]);
  }

  cout << "Done." << endl << endl;
}

void GLTextureManager::loadStandardBrushes()
{
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/charcoalAlpha.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/washbrushAlpha.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/oil.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/oilnoblend.bmp", Stroke::HUMID_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/charcoalAlpha.bmp", Stroke::DRY_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/washbrushAlpha.bmp", Stroke::DRY_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/opaqueDryBrushAlpha.bmp", Stroke::OPAQUE_MEDIUM);
  //  getBrushTextureIndex(TEXTURES_DIR "/brushes/opaqueBrushAlpha.bmp", Stroke::OPAQUE_MEDIUM);
  _defaultTextureId = getBrushTextureIndex("smoothAlpha.bmp", Stroke::OPAQUE_MEDIUM);
}


unsigned
GLTextureManager::loadBrush(string sname, Stroke::MediumType mediumType)
{
  GLuint texId;
  glGenTextures(1, &texId);
  bool found = false;
  vector<string> pathnames;
  string path; //soc
  StringUtils::getPathName(TextureManager::Options::getBrushesPath(),
  sname,
  pathnames);
  for (vector<string>::const_iterator j = pathnames.begin(); j != pathnames.end(); j++) {
    path = j->c_str();
    //soc if(QFile::exists(path)){
	if( BLI_exists( const_cast<char *>(path.c_str()) ) ) {
      found = true;
      break;
    }
  }
  if(!found)
    return 0;
  // Brush texture
  cout << "Loading brush texture..." << endl;
  switch(mediumType){
  case Stroke::DRY_MEDIUM:
    //soc prepareTextureLuminance((const char*)path.toAscii(), texId);
	prepareTextureLuminance(StringUtils::toAscii(path), texId);
    break;
  case Stroke::HUMID_MEDIUM:
  case Stroke::OPAQUE_MEDIUM:
  default:
    //soc prepareTextureAlpha((const char*)path.toAscii(), texId);
	prepareTextureAlpha(StringUtils::toAscii(path), texId);
    break;
  }
  cout << "Done." << endl << endl;

  return texId;
}

bool 
GLTextureManager::prepareTextureAlpha (string sname, GLuint itexname)
{
  //soc const char * name = sname.c_str();
	char * name = (char *) sname.c_str();
	
  //soc 
  // QImage qim(name);
  // QFileInfo fi(name);
  // QString filename = fi.fileName();
	ImBuf *qim = IMB_loadiffname(name, 0);
	char filename[FILE_MAXFILE];
	BLI_splitdirstring(name, filename);

//soc  if (qim.isNull()) 
  if (!qim) //soc 
    {
      cerr << "  Error: unable to read \"" << filename << "\"" << endl;
	IMB_freeImBuf(qim);
      return false;
    }

  if( qim->depth > 8) //soc
    {
      cerr<<"  Error: \""<< filename <<"\" has "<< qim->depth <<" bits/pixel"<<endl; //soc
      return false;
    }
  //		qim=QGLWidget::convertToGLFormat( qimOri );

  glBindTexture(GL_TEXTURE_2D, itexname);
  //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		  GL_LINEAR);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
  //	      GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		  GL_LINEAR);     

  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, qim->x, qim->y, 0, 
	       GL_ALPHA, GL_UNSIGNED_BYTE, qim->rect);	//soc

  //soc cout << "  \"" << filename.toAscii().data() << "\" loaded with "<< qim.depth() << " bits per pixel" << endl;
	cout << "  \"" << StringUtils::toAscii(filename) << "\" loaded with "<< qim->depth << " bits per pixel" << endl;
	
  return true;

}

bool 
GLTextureManager::prepareTextureLuminance (string sname, GLuint itexname)
{
  //soc const char * name = sname.c_str();
	char * name = (char *) sname.c_str();
	
  //soc
  // QImage qim(name);
  // QFileInfo fi(name);
  // QString filename = fi.fileName();
	ImBuf *qim = IMB_loadiffname(name, 0);
	char filename[FILE_MAXFILE];
	BLI_splitdirstring(name, filename);
	
  if (!qim) //soc 
    {
      cerr << "  Error: unable to read \"" << filename << "\"" << endl;
	IMB_freeImBuf(qim);
      return false;
    }
  if (qim->depth > 8) //soc
    {
      cerr<<"  Error: \""<<filename<<"\" has "<<qim->depth <<" bits/pixel"<<endl;//soc
      return false;
    }

  glBindTexture(GL_TEXTURE_2D, itexname);
  //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		  GL_LINEAR);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
  //	      GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		  GL_LINEAR);     

  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, qim->x, qim->y, 0, 
	       GL_LUMINANCE, GL_UNSIGNED_BYTE, qim->rect);	//soc

  //soc cout << "  \"" << filename.toAscii().data() << "\" loaded with "<< qim.depth() << " bits per pixel" << endl;
	cout << "  \"" << StringUtils::toAscii(filename) << "\" loaded with "<< qim->depth << " bits per pixel" << endl;
	
  return true;

}

bool 
GLTextureManager::prepareTextureLuminanceAndAlpha (string sname, GLuint itexname)
{
  //soc const char * name = sname.c_str();
	char * name = (char *) sname.c_str();
	
  //soc
  // QImage qim(name);
  // QFileInfo fi(name);
  // QString filename = fi.fileName();
	ImBuf *qim = IMB_loadiffname(name, 0);
	char filename[FILE_MAXFILE];
	BLI_splitdirstring(name, filename);

  if (!qim) //soc 
    {
      cerr << "  Error: unable to read \"" << filename << "\"" << endl;
	IMB_freeImBuf(qim);
      return false;
    }

  if (qim->depth > 8) //soc
    {
      cerr<<"  Error: \""<<filename<<"\" has "<< qim->depth <<" bits/pixel"<<endl; //soc
      return false;
    }
					   
  glBindTexture(GL_TEXTURE_2D, itexname);
  //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					     
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		  GL_LINEAR);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
  //	      GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		  GL_LINEAR);     
						     
  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, qim->x, qim->y, 0, 
	       GL_LUMINANCE, GL_UNSIGNED_BYTE, qim->rect);	//soc
  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, qim->x, qim->y, 0, 
	       GL_ALPHA, GL_UNSIGNED_BYTE, qim->rect);	//soc
							 
  //soc cout << "  \"" << filename.toAscii().data() << "\" loaded with "<< qim.depth() << " bits per pixel" << endl;						   
	cout << "  \"" << StringUtils::toAscii(filename) << "\" loaded with "<< qim->depth << " bits per pixel" << endl;

  return true;
							     
}

bool 
GLTextureManager::preparePaper (const char *name, GLuint itexname)
{
  //soc
  // QImage qim(name);
  // QFileInfo fi(name);
  // QString filename = fi.fileName();
	ImBuf *qim = IMB_loadiffname(name, 0);
	char filename[FILE_MAXFILE];
	BLI_splitdirstring((char *)name, filename);
	qim->depth = 32;

  if (!qim) //soc 
    {
      cerr << "  Error: unable to read \"" << filename << "\"" << endl;
	IMB_freeImBuf(qim);
      return false;
    }

	//soc: no test because IMB_loadiffname creates 32 bit image directly
	//
	//   if (qim->depth != 32) 
	//     {
	//       cerr<<"  Error: \""<<filename<<"\" has "<< qim->depth <<" bits/pixel"<<endl; //soc
	// IMB_freeImBuf(qim);
	//       return false;
	//     }
	// QImage qim2=QGLWidget::convertToGLFormat( qim );

  glBindTexture(GL_TEXTURE_2D, itexname);
	
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		  GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		  GL_LINEAR);     

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, qim->x, qim->y, 0, 
	       GL_RGBA, GL_UNSIGNED_BYTE, qim->rect); // soc: was qim2

  //cout << "  \"" << filename.toAscii().data() << "\" loaded with "<< qim.depth() << " bits per pixel" << endl;
	cout << "  \"" << StringUtils::toAscii(filename) << "\" loaded with 32 bits per pixel" << endl;
  return true;
}

