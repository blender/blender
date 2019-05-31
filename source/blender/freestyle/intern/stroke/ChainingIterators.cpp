/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup freestyle
 * \brief Chaining iterators
 */

#include "../python/Director.h"

#include "ChainingIterators.h"

#include "../system/TimeStamp.h"

namespace Freestyle {

ViewEdge *AdjacencyIterator::operator*()
{
  return (*_internalIterator).first;
}

bool AdjacencyIterator::isIncoming() const
{
  return (*_internalIterator).second;
}

int AdjacencyIterator::increment()
{
  ++_internalIterator;
  while ((!_internalIterator.isEnd()) && (!isValid((*_internalIterator).first))) {
    ++_internalIterator;
  }
  return 0;
}

bool AdjacencyIterator::isValid(ViewEdge *edge)
{
  if (_restrictToSelection) {
    if (edge->getTimeStamp() != TimeStamp::instance()->getTimeStamp()) {
      return false;
    }
  }
  if (_restrictToUnvisited) {
    if (edge->getChainingTimeStamp() > TimeStamp::instance()->getTimeStamp()) {
      return false;
    }
  }
  return true;
}

int ChainingIterator::init()
{
  return Director_BPy_ChainingIterator_init(this);
}

int ChainingIterator::traverse(const AdjacencyIterator &it)
{
  return Director_BPy_ChainingIterator_traverse(this, const_cast<AdjacencyIterator &>(it));
}

int ChainingIterator::increment()
{
  _increment = true;
  ViewVertex *vertex = getVertex();
  if (!vertex) {
    _edge = 0;
    return 0;
  }
  AdjacencyIterator it = AdjacencyIterator(vertex, _restrictToSelection, _restrictToUnvisited);
  if (it.isEnd()) {
    _edge = 0;
    return 0;
  }
  if (traverse(it) < 0) {
    return -1;
  }
  _edge = result;
  if (_edge == 0) {
    return 0;
  }
  if (_edge->A() == vertex) {
    _orientation = true;
  }
  else {
    _orientation = false;
  }
  return 0;
}

int ChainingIterator::decrement()
{
  _increment = false;
  ViewVertex *vertex = getVertex();
  if (!vertex) {
    _edge = 0;
    return 0;
  }
  AdjacencyIterator it = AdjacencyIterator(vertex, _restrictToSelection, _restrictToUnvisited);
  if (it.isEnd()) {
    _edge = 0;
    return 0;
  }
  if (traverse(it) < 0) {
    return -1;
  }
  _edge = result;
  if (_edge == 0) {
    return 0;
  }
  if (_edge->B() == vertex) {
    _orientation = true;
  }
  else {
    _orientation = false;
  }
  return 0;
}

//
// ChainSilhouetteIterators
//
///////////////////////////////////////////////////////////

int ChainSilhouetteIterator::traverse(const AdjacencyIterator &ait)
{
  AdjacencyIterator it(ait);
  ViewVertex *nextVertex = getVertex();
  // we can't get a NULL nextVertex here, it was intercepted before
  if (nextVertex->getNature() & Nature::T_VERTEX) {
    TVertex *tvertex = (TVertex *)nextVertex;
    ViewEdge *mate = (tvertex)->mate(getCurrentEdge());
    while (!it.isEnd()) {
      ViewEdge *ve = *it;
      if (ve == mate) {
        result = ve;
        return 0;
      }
      ++it;
    }
    result = 0;
    return 0;
  }
  if (nextVertex->getNature() & Nature::NON_T_VERTEX) {
    // soc NonTVertex *nontvertex = (NonTVertex*)nextVertex;
    ViewEdge *newEdge(0);
    // we'll try to chain the edges by keeping the same nature...
    // the preseance order is : SILHOUETTE, BORDER, CREASE, MATERIAL_BOUNDARY, EDGE_MARK,
    // SUGGESTIVE, VALLEY, RIDGE
    Nature::EdgeNature natures[8] = {
        Nature::SILHOUETTE,
        Nature::BORDER,
        Nature::CREASE,
        Nature::MATERIAL_BOUNDARY,
        Nature::EDGE_MARK,
        Nature::SUGGESTIVE_CONTOUR,
        Nature::VALLEY,
        Nature::RIDGE,
    };
    int numNatures = sizeof(natures) / sizeof(Nature::EdgeNature);
    for (int i = 0; i < numNatures; ++i) {
      if (getCurrentEdge()->getNature() & natures[i]) {
        int n = 0;
        while (!it.isEnd()) {
          ViewEdge *ve = *it;
          if (ve->getNature() & natures[i]) {
            ++n;
            newEdge = ve;
          }
          ++it;
        }
        if (n == 1) {
          result = newEdge;
        }
        else {
          result = 0;
        }
        return 0;
      }
    }
  }
  result = 0;
  return 0;
}

int ChainPredicateIterator::traverse(const AdjacencyIterator &ait)
{
  if (!_unary_predicate || !_binary_predicate) {
    return -1;
  }
  AdjacencyIterator it(ait);
  // Iterates over next edges to see if one of them respects the predicate:
  while (!it.isEnd()) {
    ViewEdge *ve = *it;
    if (_unary_predicate->operator()(*ve) < 0) {
      return -1;
    }
    if (_unary_predicate->result) {
      if (_binary_predicate->operator()(*(getCurrentEdge()), *(ve)) < 0) {
        return -1;
      }
      if (_binary_predicate->result) {
        result = ve;
        return 0;
      }
    }
    ++it;
  }
  result = 0;
  return 0;
}

} /* namespace Freestyle */
