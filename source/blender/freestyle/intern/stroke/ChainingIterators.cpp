/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  while (!_internalIterator.isEnd() && !isValid((*_internalIterator).first)) {
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
    _edge = nullptr;
    return 0;
  }
  AdjacencyIterator it = AdjacencyIterator(vertex, _restrictToSelection, _restrictToUnvisited);
  if (it.isEnd()) {
    _edge = nullptr;
    return 0;
  }
  if (traverse(it) < 0) {
    return -1;
  }
  _edge = result;
  if (_edge == nullptr) {
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
    _edge = nullptr;
    return 0;
  }
  AdjacencyIterator it = AdjacencyIterator(vertex, _restrictToSelection, _restrictToUnvisited);
  if (it.isEnd()) {
    _edge = nullptr;
    return 0;
  }
  if (traverse(it) < 0) {
    return -1;
  }
  _edge = result;
  if (_edge == nullptr) {
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
  // we can't get a nullptr nextVertex here, it was intercepted before
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
    result = nullptr;
    return 0;
  }
  if (nextVertex->getNature() & Nature::NON_T_VERTEX) {
    // soc NonTVertex *nontvertex = (NonTVertex*)nextVertex;
    ViewEdge *newEdge(nullptr);
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
    int numNatures = ARRAY_SIZE(natures);
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
          result = nullptr;
        }
        return 0;
      }
    }
  }
  result = nullptr;
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
  result = nullptr;
  return 0;
}

} /* namespace Freestyle */
