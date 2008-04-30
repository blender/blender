
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

# include "Functions1D.h"
using namespace std;

namespace Functions1D {

  real GetXF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  real GetYF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  real GetZF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  real GetProjectedXF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  real GetProjectedYF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  real GetProjectedZF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  Vec2f Orientation2DF1D::operator()(Interface1D& inter) {
    FEdge * fe = dynamic_cast<FEdge*>(&inter);
    if(fe){
      Vec3r res = fe->orientation2d();
      return Vec2f(res[0], res[1]);
    }
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  Vec3f Orientation3DF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  real ZDiscontinuityF1D::operator()(Interface1D& inter) {
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }
  
  unsigned QuantitativeInvisibilityF1D::operator()(Interface1D& inter) {
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve)
      return ve->qi();
    FEdge *fe = dynamic_cast<FEdge*>(&inter);
    if(fe)
      return ve->qi();
    return integrate(_func, inter.verticesBegin(), inter.verticesEnd(), _integration);
  }

  Nature::EdgeNature CurveNatureF1D::operator()(Interface1D& inter) {
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve)
      return ve->getNature();
    else{
      // we return a nature that contains every 
      // natures of the viewedges spanned by the chain.
      Nature::EdgeNature nat = Nature::NO_FEATURE;
      Interface0DIterator it = inter.verticesBegin();
      while(!it.isEnd()){
        nat |= _func(it);
        ++it;
      }
      return nat;
    }
  }
  
  void TimeStampF1D::operator()(Interface1D& inter) {
    TimeStamp *timestamp = TimeStamp::instance();
    inter.setTimeStamp(timestamp->getTimeStamp());
  }

  void ChainingTimeStampF1D::operator()(Interface1D& inter) {
    TimeStamp *timestamp = TimeStamp::instance();
    ViewEdge *ve = dynamic_cast<ViewEdge*>(&inter);
    if(ve)
      ve->setChainingTimeStamp(timestamp->getTimeStamp());
  }
  
  void IncrementChainingTimeStampF1D::operator()(Interface1D& inter) {
    ViewEdge *ve = dynamic_cast<ViewEdge*>(&inter);
    if(ve)
      ve->setChainingTimeStamp(ve->getChainingTimeStamp()+1);
  }

  vector<ViewShape*> GetShapeF1D::operator()(Interface1D& inter) {
    vector<ViewShape*> shapesVector;
    set<ViewShape*> shapesSet;
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve){
      shapesVector.push_back(ve->viewShape());
    }else{
      Interface0DIterator it=inter.verticesBegin(), itend=inter.verticesEnd();
      for(;it!=itend;++it)
        shapesSet.insert(Functions0D::getShapeF0D(it));
      shapesVector.insert<set<ViewShape*>::iterator>(shapesVector.begin(), shapesSet.begin(), shapesSet.end());
    }
    return shapesVector;
  }

  vector<ViewShape*> GetOccludersF1D::operator()(Interface1D& inter) {
    vector<ViewShape*> shapesVector;
    set<ViewShape*> shapesSet;
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve){
      return ve->occluders();
    }else{
      Interface0DIterator it=inter.verticesBegin(), itend=inter.verticesEnd();
      for(;it!=itend;++it){
        Functions0D::getOccludersF0D(it, shapesSet);
      }
      shapesVector.insert(shapesVector.begin(), shapesSet.begin(), shapesSet.end());
    }
    return shapesVector;
  }

  vector<ViewShape*> GetOccludeeF1D::operator()(Interface1D& inter) {
    vector<ViewShape*> shapesVector;
    set<ViewShape*> shapesSet;
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve){
      ViewShape * aShape = ve->aShape();
      shapesVector.push_back(aShape);
    }else{
      Interface0DIterator it=inter.verticesBegin(), itend=inter.verticesEnd();
      for(;it!=itend;++it){
        shapesSet.insert(Functions0D::getOccludeeF0D(it));
      }
    shapesVector.insert<set<ViewShape*>::iterator>(shapesVector.begin(), shapesSet.begin(), shapesSet.end());
    }
    return shapesVector;
  }
  // Internal
  ////////////

  void getOccludeeF1D(Interface1D& inter, set<ViewShape*>& oShapes){
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve){
      ViewShape * aShape = ve->aShape();
      if(aShape == 0){
        oShapes.insert(0);
        return;
      }
      oShapes.insert(aShape);
    }
    else{
      Interface0DIterator it=inter.verticesBegin(), itend=inter.verticesEnd();
      for(;it!=itend;++it)
        oShapes.insert(Functions0D::getOccludeeF0D(it));
    }
  }

  void getOccludersF1D(Interface1D& inter, set<ViewShape*>& oShapes){
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve){
      vector<ViewShape*>& occluders = ve->occluders();
      oShapes.insert<vector<ViewShape*>::iterator>(occluders.begin(), occluders.end());
    }
    else{
      Interface0DIterator it=inter.verticesBegin(), itend=inter.verticesEnd();
      for(;it!=itend;++it){
        set<ViewShape*> shapes;
        Functions0D::getOccludersF0D(it, shapes);
        for(set<ViewShape*>::iterator s=shapes.begin(), send=shapes.end();
            s!=send;
            ++s)
          oShapes.insert(*s);
      }
    }
  }

  void getShapeF1D(Interface1D& inter, set<ViewShape*>& oShapes){
    ViewEdge* ve = dynamic_cast<ViewEdge*>(&inter);
    if (ve){
      oShapes.insert(ve->viewShape());
    }else{
      Interface0DIterator it=inter.verticesBegin(), itend=inter.verticesEnd();
      for(;it!=itend;++it)
        oShapes.insert(Functions0D::getShapeF0D(it));
    }
  }
} // end of namespace Functions1D
