
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
#include "ChainingIterators.h"
#include "../system/TimeStamp.h"

ViewEdge* AdjacencyIterator::operator*() {
  return (*_internalIterator).first;
}
bool AdjacencyIterator::isIncoming() const{
  return (*_internalIterator).second;
}

void AdjacencyIterator::increment(){
  ++_internalIterator;
  while((!_internalIterator.isEnd()) && (!isValid((*_internalIterator).first)))
    ++_internalIterator;
}

bool AdjacencyIterator::isValid(ViewEdge* edge){
  if(_restrictToSelection)
    if(edge->getTimeStamp() != TimeStamp::instance()->getTimeStamp())
      return false;
  if(_restrictToUnvisited)
    if(edge->getChainingTimeStamp() > TimeStamp::instance()->getTimeStamp())
      return false;
    return true;
}

void ChainingIterator::increment() {
  _increment = true;
  ViewVertex * vertex = getVertex();
  if(!vertex){
    _edge = 0;
    return;
  }
  AdjacencyIterator it = AdjacencyIterator(vertex, _restrictToSelection, _restrictToUnvisited);
  if(it.isEnd())
    _edge = 0;
  else
    _edge = traverse(it);
  if(_edge == 0)
    return;
  if(_edge->A() == vertex)
    _orientation = true;
  else
    _orientation = false;
}

void ChainingIterator::decrement() {
  _increment = false;
  ViewVertex * vertex = getVertex();
  if(!vertex){
    _edge = 0;
    return;
  }
  AdjacencyIterator it = AdjacencyIterator(vertex, _restrictToSelection, _restrictToUnvisited);
  if(it.isEnd())
    _edge = 0;
  else
    _edge = traverse(it);
  if(_edge == 0)
    return;
  if(_edge->B() == vertex)
    _orientation = true;
  else
    _orientation = false;
}

//
// ChainSilhouetteIterators
//
///////////////////////////////////////////////////////////

ViewEdge * ChainSilhouetteIterator::traverse(const AdjacencyIterator& ait){
  AdjacencyIterator it(ait);
  ViewVertex* nextVertex = getVertex();
  // we can't get a NULL nextVertex here, it was intercepted
  // before
  if(nextVertex->getNature() & Nature::T_VERTEX){
    TVertex * tvertex = (TVertex*)nextVertex;
    ViewEdge *mate = (tvertex)->mate(getCurrentEdge());
    while(!it.isEnd()){
      ViewEdge *ve = *it;
      if(ve == mate)
        return ve;
      ++it;
    }
    return 0;
  }
  if(nextVertex->getNature() & Nature::NON_T_VERTEX){
    NonTVertex * nontvertex = (NonTVertex*)nextVertex;
    ViewEdge * newEdge(0);
    // we'll try to chain the edges by keeping the same nature...
    // the preseance order is : SILHOUETTE, BORDER, CREASE, SUGGESTIVE, VALLEY, RIDGE
    Nature::EdgeNature natures[6] = {Nature::SILHOUETTE, Nature::BORDER, Nature::CREASE, Nature::SUGGESTIVE_CONTOUR, Nature::VALLEY, Nature::RIDGE};
    for(unsigned i=0; i<6; ++i){
      if(getCurrentEdge()->getNature() & natures[i]){
        int n = 0;
        while(!it.isEnd()){
          ViewEdge *ve = *it;
          if(ve->getNature() & natures[i]){
            ++n;
            newEdge = ve;
          } 
          ++it;
        } 
        if(n == 1){
          return newEdge;
        }else{
          return 0;
        }   
      } 
    }
  }
  return 0;
}

ViewEdge * ChainPredicateIterator::traverse(const AdjacencyIterator& ait){
  AdjacencyIterator it(ait);
  // Iterates over next edges to see if one of them 
  // respects the predicate:
  while(!it.isEnd()) {
    ViewEdge *ve = *it;
    if(((*_unary_predicate)(*ve)) && ((*_binary_predicate)(*(getCurrentEdge()),*(ve))))
        return ve;
    ++it;
  }
  return 0;
} 
