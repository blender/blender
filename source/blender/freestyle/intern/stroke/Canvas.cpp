
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

#include "StrokeRenderer.h"
#include <vector>
#include "../system/FreestyleConfig.h"
#include "../system/TimeStamp.h"
#include "../system/PseudoNoise.h"
#include "Canvas.h"
#include "../image/Image.h"
#include "../image/GaussianFilter.h"
#include "../image/ImagePyramid.h"
#include "../view_map/SteerableViewMap.h"
#include "StyleModule.h"

//soc #include <qimage.h>
//soc #include <QString>
#include <sstream>	

extern "C" {
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}

using namespace std;

LIB_STROKE_EXPORT
Canvas * Canvas::_pInstance = 0;

LIB_STROKE_EXPORT
const char * Canvas::_MapsPath = 0;

using namespace std;

Canvas::Canvas()
{
  _SelectedFEdge = 0;
  _pInstance = this;
  PseudoNoise::init(42);
  _Renderer = 0;
  _current_sm = NULL;
  _steerableViewMap = new SteerableViewMap(NB_STEERABLE_VIEWMAP-1);
  _basic = false;
}

Canvas::Canvas(const Canvas& iBrother)
{
  _SelectedFEdge = iBrother._SelectedFEdge;
  _pInstance = this;
  PseudoNoise::init(42);
  _Renderer = iBrother._Renderer;
  _current_sm = iBrother._current_sm;
  _steerableViewMap = new SteerableViewMap(*(iBrother._steerableViewMap));
  _basic = iBrother._basic;
}

Canvas::~Canvas()
{
  _pInstance = 0;

  Clear();
  if(_Renderer)
  {
    delete _Renderer;
    _Renderer = 0;
  }
  // FIXME: think about an easy control 
  // for the maps memory management...
  if(!_maps.empty()){
    for(mapsMap::iterator m=_maps.begin(), mend=_maps.end();
    m!=mend;
    ++m){
      delete ((*m).second);
    }
    _maps.clear();
  }
  if(_steerableViewMap)
    delete _steerableViewMap;
}

void Canvas::preDraw() {}

void Canvas::Draw()
{
  if(_StyleModules.empty())
    return;
  preDraw();
  TimeStamp *timestamp = TimeStamp::instance();

  for(unsigned i = 0; i < _StyleModules.size(); i++) {
    _current_sm = _StyleModules[i];

    if (i < _Layers.size() && _Layers[i])
      delete _Layers[i];

    _Layers[i] = _StyleModules[i]->execute();
	if (!_Layers[i])
		continue;

	stroke_count += _Layers[i]->strokes_size();

    timestamp->increment();
  }
  postDraw();
}

void Canvas::postDraw()
{
  update();
}


void Canvas::Clear()
{
  if(!_Layers.empty()) {
    for (deque<StrokeLayer*>::iterator sl=_Layers.begin(), slend=_Layers.end();
	 sl != slend;
	 ++sl)
      if (*sl)
	delete (*sl);
    _Layers.clear();  
  }

  if(!_StyleModules.empty()) {
    for (deque<StyleModule*>::iterator s=_StyleModules.begin(), send=_StyleModules.end();
	 s != send;
	 ++s)
      if (*s)
	delete (*s);
    _StyleModules.clear();
  }
  if(_steerableViewMap)
    _steerableViewMap->Reset();

	stroke_count = 0;
}

void Canvas::Erase()
{
  if(!_Layers.empty())
  {
    for (deque<StrokeLayer*>::iterator sl=_Layers.begin(), slend=_Layers.end();
	 sl != slend;
	 ++sl)
      if (*sl)
	(*sl)->clear();
  }
  if(_steerableViewMap)
    _steerableViewMap->Reset();
  update();

stroke_count = 0;
}

void Canvas::PushBackStyleModule(StyleModule *iStyleModule) {
  StrokeLayer* layer = new StrokeLayer();
  _StyleModules.push_back(iStyleModule);
  _Layers.push_back(layer);
}

void Canvas::InsertStyleModule(unsigned index, StyleModule *iStyleModule) {
  unsigned size = _StyleModules.size();
  StrokeLayer* layer = new StrokeLayer();
  if((_StyleModules.empty()) || (index == size)) {
    _StyleModules.push_back(iStyleModule);
    _Layers.push_back(layer);
    return;
  }
	_StyleModules.insert(_StyleModules.begin() + index, iStyleModule);
  _Layers.insert(_Layers.begin()+index, layer);
}

void Canvas::RemoveStyleModule(unsigned index)
{
  unsigned i=0;
  if (!_StyleModules.empty())
    {
      for(deque<StyleModule*>::iterator s=_StyleModules.begin(), send=_StyleModules.end();
	  s!=send;
	  ++s)
	{
	  if(i == index)
	    { 
	      // remove shader
	      if (*s)
		delete *s;
	      _StyleModules.erase(s);
	      break;
	    } 
	  ++i;
	}
    }
  i=0;
  if(!_Layers.empty())
  {
    for(deque<StrokeLayer*>::iterator sl=_Layers.begin(), slend=_Layers.end();
    sl!=slend;
    ++sl)
    {
      if(i == index)
      { 
        // remove layer
	if (*sl)
	  delete *sl;
        _Layers.erase(sl);
        break;
      } 
      ++i;
    }
  }
}


void Canvas::SwapStyleModules(unsigned i1, unsigned i2)
{
  StyleModule* tmp;
  tmp = _StyleModules[i1];
  _StyleModules[i1] = _StyleModules[i2];
  _StyleModules[i2] = tmp;

  StrokeLayer* tmp2;
  tmp2 = _Layers[i1];
  _Layers[i1] = _Layers[i2];
  _Layers[i2] = tmp2;
}

void Canvas::ReplaceStyleModule(unsigned index, StyleModule *iStyleModule)
{
  unsigned i=0;
  for(deque<StyleModule*>::iterator s=_StyleModules.begin(), send=_StyleModules.end();
      s != send;
      ++s)
    {
      if(i == index)
	{
	  if (*s)
	    delete *s;
	  *s = iStyleModule;
	  break;
	}
      ++i;
    }
}

void Canvas::setVisible(unsigned index, bool iVisible) {
  _StyleModules[index]->setDisplayed(iVisible);
}
 
void Canvas::setModified(unsigned index, bool iMod)
{
  _StyleModules[index]->setModified(iMod);
}

void Canvas::resetModified(bool iMod/* =false */)
{
  unsigned size = _StyleModules.size();
  for(unsigned i = 0; i < size; ++i)
    setModified(i,iMod);
}

void Canvas::causalStyleModules(vector<unsigned>& vec, unsigned index) {
  unsigned size = _StyleModules.size();

  for(unsigned i = index; i < size; ++i)
    if (_StyleModules[i]->getCausal())
      vec.push_back(i);
}

void Canvas::Render(const StrokeRenderer *iRenderer)
{
  for (unsigned i = 0; i < _StyleModules.size(); i++) {
    if(!_StyleModules[i]->getDisplayed() || !_Layers[i])
      continue;
    _Layers[i]->Render(iRenderer);
  }
}

void Canvas::RenderBasic(const StrokeRenderer *iRenderer)

{
  for (unsigned i = 0; i < _StyleModules.size(); i++) {
    if(!_StyleModules[i]->getDisplayed() || !_Layers[i])
      continue;
    _Layers[i]->RenderBasic(iRenderer);
  }
}

void Canvas::loadMap(const char *iFileName, const char *iMapName, unsigned int iNbLevels, float iSigma){
  // check whether this map was already loaded:
  if(!_maps.empty()){
    mapsMap::iterator m = _maps.find(iMapName);
    if(m!=_maps.end()){
      // lazy check for size changes
      ImagePyramid * pyramid = (*m).second;
      if((pyramid->width() != width()) || (pyramid->height() != height())){
        delete pyramid;
      }else{
        return;
      }
    }
  }
  string filePath;
  if(_MapsPath){
    filePath = _MapsPath;
    filePath += iFileName;
  }else{
    filePath = iFileName;
  }

  //soc
  // QImage *qimg;
  // QImage newMap(filePath.c_str()); 
  // if(newMap.isNull()){
  //   cout << "Could not load image file " << filePath << endl;
  //   return;
  // }
  // qimg = &newMap;
	/* OCIO_TODO: support different input color space */
  	ImBuf *qimg = IMB_loadiffname(filePath.c_str(), 0, NULL);
  	if( qimg == 0 ){
		cout << "Could not load image file " << filePath << endl;
    	return;	
	}

  // soc
  //resize
  // QImage scaledImg;
  // if((newMap.width()!=width()) || (newMap.height()!=height())){
  // 	  scaledImg = newMap.scaled(width(), height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  //   qimg = &scaledImg;
  // }
	ImBuf *scaledImg;
  if( ( qimg->x != width() ) || ( qimg->y != height() ) ){
	scaledImg = IMB_dupImBuf(qimg);
	IMB_scaleImBuf(scaledImg, width(), height());
  }


  // deal with color image 
  //  if(newMap->depth() != 8){
  //    int w = newMap->width();
  //    int h = newMap->height();
  //    QImage *tmp = new QImage(w, h, 8);
  //    for(unsigned y=0;y<h;++y){
  //      for(unsigned x=0;x<w;++x){
  //        int c = qGray(newMap->pixel(x,y));
  //        tmp->setPixel(x,y,c);
  //      }
  //    }
  //    delete newMap;
  //    newMap = tmp;
  //  }

  int x,y;
  int w = qimg->x;
  int h = qimg->y;
int rowbytes = w*4;
  GrayImage tmp(w,h);
  char *pix;
  
  for(y=0; y<h;++y){
    for(x=0;x<w;++x){
		pix = (char*)qimg->rect + y*rowbytes + x*4;
	  float c = (pix[0]*11 + pix[1]*16 + pix[2]*5)/32;
      tmp.setPixel(x,y,c);
    }
  }

  //  GrayImage blur(w,h);
  //  GaussianFilter gf(4.f);
  //  //int bound = gf.getBound();
  //  for(y=0; y<h;++y){
  //    for(x=0;x<w;++x){
  //      int c = gf.getSmoothedPixel<GrayImage>(&tmp, x,y);
  //      blur.setPixel(x,y,c);
  //    }
  //  }

  GaussianPyramid *pyramid = new GaussianPyramid(tmp, iNbLevels, iSigma);
  int ow = pyramid->width(0);
  int oh = pyramid->height(0);
  string base(iMapName); //soc
  for(int i=0; i<pyramid->getNumberOfLevels(); ++i){
    // save each image:
    //    w = pyramid.width(i);
    //    h = pyramid.height(i);
	
	//soc  QImage qtmp(ow, oh, QImage::Format_RGB32);
    ImBuf *qtmp = IMB_allocImBuf(ow, oh, 32, IB_rect);

//int k = (1<<i);
    for(y=0;y<oh;++y){
      for(x=0;x<ow;++x){
        int c = pyramid->pixel(x,y,i);//255*pyramid->pixel(x,y,i);
        //soc qtmp.setPixel(x,y,qRgb(c,c,c));
		pix = (char*)qtmp->rect + y*rowbytes + x*4;
		pix[0] = pix [1] = pix[2] = c;
      }
    }
    //soc qtmp.save(base+QString::number(i)+".bmp", "BMP");
	stringstream filename;
	filename << base;
	filename << i << ".bmp";	
	qtmp->ftype= BMP;
	IMB_saveiff(qtmp, const_cast<char *>(filename.str().c_str()), 0);
	
  }
  //  QImage *qtmp = new QImage(w, h, 32);
  //  for(y=0;y<h;++y){
  //    for(x=0;x<w;++x){
  //      int c = (int)blur.pixel(x,y);
  //      qtmp->setPixel(x,y,qRgb(c,c,c));
  //    }
  //  }
  //  delete newMap;
  //  newMap = qtmp;
  //
  _maps[iMapName] = pyramid;
  //  newMap->save("toto.bmp", "BMP");
}

float Canvas::readMapPixel(const char *iMapName, int level, int x, int y){
  if(_maps.empty()){
    cout << "readMapPixel warning: no map was loaded "<< endl;
    return -1;
  }
  mapsMap::iterator m = _maps.find(iMapName);
  if(m==_maps.end()){
    cout << "readMapPixel warning: no map was loaded with the name " << iMapName << endl;
    return -1;
  }
  ImagePyramid *pyramid = (*m).second;
  if((x<0) || (x>=pyramid->width()) || (y<0) || (y>=pyramid->height()))
    return 0;
  
  return pyramid->pixel(x,height()-1-y,level);
}
