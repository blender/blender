
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

# include "Functions0D.h"
# include "ViewMap.h"

using namespace std;

namespace Functions0D {

  // Internal function
  FEdge* getFEdge(Interface0D& it1, Interface0D& it2){
    return it1.getFEdge(it2);
  }

  void getFEdges(Interface0DIterator& it,
                 FEdge*& fe1,
                 FEdge*& fe2) {
    // count number of vertices
    Interface0DIterator prev = it, next = it;
    ++next;
    int count = 1;
    while((!prev.isBegin()) && (count < 3))
      { 
        --prev;
        ++count;
      }
    while((!next.isEnd()) && (count < 3))
      {
        ++next;
        ++count;
      } 
    if(count < 3)
      {
        // if we only have 2 vertices
        FEdge * fe = 0;
        Interface0DIterator tmp = it;
        if(it.isBegin())
          {
            ++tmp;
            fe = it->getFEdge(*tmp);
          }
        else
          {
            --tmp;
            fe = it->getFEdge(*tmp);
          }
        fe1 = fe;
        fe2 = 0;
      }  
    else 
      {
        // we have more than 2 vertices
        bool begin=false,last=false;
        Interface0DIterator previous = it;
        if(!previous.isBegin())
          --previous;
        else
          begin=true;
        Interface0DIterator next = it;
        ++next;
        if(next.isEnd())
          last = true;
        if(begin)
          {
            fe1 = it->getFEdge(*next);
            fe2 = 0;
          }
        else if(last)
          {
            fe1 = previous->getFEdge(*it);
            fe2 = 0;
          }
        else
          {
            fe1 = previous->getFEdge(*it);
            fe2 = it->getFEdge(*next);
          }
      }
  }

  void getViewEdges(Interface0DIterator &it,
                    ViewEdge *&ve1,
                    ViewEdge *&ve2)
  {
    FEdge * fe1, *fe2;
    getFEdges(it, fe1, fe2);
    ve1 = fe1->viewedge();
    if(fe2 != 0)
      {
        ve2 = fe2->viewedge();
        if(ve2 == ve1)
          ve2 = 0;
      }
    else
      ve2 = 0;
  }

  ViewShape* getShapeF0D(Interface0DIterator& it)
  {
    ViewEdge *ve1, *ve2;
    getViewEdges(it, ve1, ve2);
    return ve1->viewShape();
  }

  void getOccludersF0D(Interface0DIterator& it, set<ViewShape*>& oOccluders){
    ViewEdge * ve1, *ve2;
    getViewEdges(it, ve1, ve2);
    occluder_container::const_iterator oit = ve1->occluders_begin();
    occluder_container::const_iterator oitend = ve1->occluders_end();

    for(;oit!=oitend; ++oit)
      oOccluders.insert((*oit));  

    if(ve2!=0){
      oit = ve2->occluders_begin();
      oitend = ve2->occluders_end();
      for(;oit!=oitend; ++oit)
        oOccluders.insert((*oit)); 
    }
  }

  ViewShape * getOccludeeF0D(Interface0DIterator& it){
    ViewEdge * ve1, *ve2;
    getViewEdges(it, ve1, ve2);
    ViewShape *aShape = ve1->aShape();
    return aShape;
  }
    
  //
  Vec2f VertexOrientation2DF0D::operator()(Interface0DIterator& iter) {
    Vec2f A,C;
    Vec2f B(iter->getProjectedX(), iter->getProjectedY());
    if(iter.isBegin())
      A = Vec2f(iter->getProjectedX(), iter->getProjectedY());
    else
      {
	Interface0DIterator previous = iter;
	--previous ;
	A = Vec2f(previous->getProjectedX(), previous->getProjectedY());
      }
    Interface0DIterator next = iter;
    ++next ;
    if(next.isEnd())
      C = Vec2f(iter->getProjectedX(), iter->getProjectedY());
    else
      C = Vec2f(next->getProjectedX(), next->getProjectedY());

    Vec2f AB(B-A);
    if(AB.norm() != 0)
      AB.normalize();
    Vec2f BC(C-B);
    if(BC.norm() != 0)
      BC.normalize();
    Vec2f res (AB + BC);
    if(res.norm() != 0)
      res.normalize();
    return res;
  }

  Vec3f VertexOrientation3DF0D::operator()(Interface0DIterator& iter) {
    Vec3r A,C;
    Vec3r B(iter->getX(), iter->getY(), iter->getZ());
    if(iter.isBegin())
      A = Vec3r(iter->getX(), iter->getY(), iter->getZ());
    else
      {
	Interface0DIterator previous = iter;
	--previous ;
	A = Vec3r(previous->getX(), previous->getY(), previous->getZ());
      }
    Interface0DIterator next = iter;
    ++next ;
    if(next.isEnd())
      C = Vec3r(iter->getX(), iter->getY(), iter->getZ());
    else
      C = Vec3r(next->getX(), next->getY(), next->getZ());

    Vec3r AB(B-A);
    if(AB.norm() != 0)
      AB.normalize();
    Vec3r BC(C-B);
    if(BC.norm() != 0)
      BC.normalize();
    Vec3f res (AB + BC);
    if(res.norm() != 0)
      res.normalize();
    return res;
  }

  real Curvature2DAngleF0D::operator()(Interface0DIterator& iter) {
    Interface0DIterator tmp1 = iter, tmp2 = iter;
    ++tmp2;
    unsigned count = 1;
    while((!tmp1.isBegin()) && (count < 3))
      {
	--tmp1;
	++count;
      }
    while((!tmp2.isEnd()) && (count < 3))
      {
	++tmp2;
	++count;
      }
    if(count < 3)
      return 0; // if we only have 2 vertices

    Interface0DIterator v = iter;
    if(iter.isBegin())
      ++v;
    Interface0DIterator next=v;
    ++next;
    if(next.isEnd())
      {
	next = v;
	--v;
      }
    Interface0DIterator prev=v;
    --prev;

    Vec2r A(prev->getProjectedX(), prev->getProjectedY());
    Vec2r B(v->getProjectedX(), v->getProjectedY());
    Vec2r C(next->getProjectedX(), next->getProjectedY());
    Vec2r AB(B-A);
    Vec2r BC(C-B);
    Vec2r N1(-AB[1], AB[0]);
    if(N1.norm() != 0)
      N1.normalize();
    Vec2r N2(-BC[1], BC[0]);
    if(N2.norm() != 0)
      N2.normalize();
    if((N1.norm() == 0) && (N2.norm() == 0))
      {
	Exception::raiseException();
	return 0; 
      }
    double cosin = N1*N2;
    if(cosin > 1)
      cosin = 1;
    if(cosin < -1)
      cosin = -1;
    return acos(cosin);
  }

  real ZDiscontinuityF0D::operator()(Interface0DIterator& iter) {
    FEdge *fe1, *fe2;
    getFEdges(iter, fe1, fe2);
    real result ;
    result = fe1->z_discontinuity();
    if(fe2!=0){
      result += fe2->z_discontinuity();
      result /= 2.f;
    }
    return result;
  }

  Vec2f Normal2DF0D::operator()(Interface0DIterator& iter) {
    FEdge *fe1, *fe2;
    getFEdges(iter,fe1,fe2);
    Vec3f e1(fe1->orientation2d());
    Vec2f n1(e1[1], -e1[0]);
    Vec2f n(n1);
    if(fe2 != 0)
      {
	Vec3f e2(fe2->orientation2d());
	Vec2f n2(e2[1], -e2[0]);
	n += n2;
      }
    n.normalize();
    return n;
  }

  Material MaterialF0D::operator()(Interface0DIterator& iter) {
    FEdge *fe1, *fe2;
    getFEdges(iter,fe1,fe2);
    
    if(fe1 == 0)
      getFEdges(iter, fe1, fe2);
    Material mat;
    if(fe1->isSmooth())
      mat = ((FEdgeSmooth*)fe1)->material();
    else
      mat = ((FEdgeSharp*)fe1)->bMaterial();
    //    const SShape * sshape = getShapeF0D(iter);
    //    return sshape->material();
    return mat;
  }

  Id ShapeIdF0D::operator()(Interface0DIterator& iter) {
    ViewShape * vshape = getShapeF0D(iter);
    return vshape->getId();
  }

  unsigned int QuantitativeInvisibilityF0D::operator()(Interface0DIterator& iter) {
    ViewEdge * ve1, *ve2;
    getViewEdges(iter,ve1,ve2);
    unsigned int qi1, qi2;
    qi1 = ve1->qi();
    if(ve2 != 0){
      qi2 = ve2->qi();
      if(qi2!=qi1)
        cout << "QuantitativeInvisibilityF0D: ambiguous evaluation for point " << iter->getId() << endl;
    }
    return qi1;
  }

  Nature::EdgeNature CurveNatureF0D::operator()(Interface0DIterator& iter) {
    Nature::EdgeNature nat = 0;
    ViewEdge * ve1, *ve2;
    getViewEdges(iter, ve1, ve2);
    nat |= ve1->getNature();
    if(ve2!=0)
      nat |= ve2->getNature();
    return nat;
  }

  vector<ViewShape*> GetOccludersF0D::operator()(Interface0DIterator& iter) {
    set<ViewShape*> occluders;
    getOccludersF0D(iter,occluders);
    vector<ViewShape*> vsOccluders;
    // vsOccluders.insert(vsOccluders.begin(), occluders.begin(), occluders.end());
    for(set<ViewShape*>::iterator it=occluders.begin(), itend=occluders.end();
    it!=itend;
    ++it){
      vsOccluders.push_back((*it));
    }
    return vsOccluders;
  }

  ViewShape* GetShapeF0D::operator()(Interface0DIterator& iter) {
    return getShapeF0D(iter);
  }

  ViewShape* GetOccludeeF0D::operator()(Interface0DIterator& iter) {
    return getOccludeeF0D(iter);
  }

} // end of namespace Functions0D
