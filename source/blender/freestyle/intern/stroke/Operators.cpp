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
 * \brief Class gathering stroke creation algorithms
 */

#include <algorithm>
#include <stdexcept>

#include "Operators.h"
#include "Canvas.h"
#include "Stroke.h"
#include "StrokeIterators.h"
#include "CurveIterators.h"

#include "BKE_global.h"

namespace Freestyle {

Operators::I1DContainer Operators::_current_view_edges_set;
Operators::I1DContainer Operators::_current_chains_set;
Operators::I1DContainer *Operators::_current_set = NULL;
Operators::StrokesContainer Operators::_current_strokes_set;

int Operators::select(UnaryPredicate1D &pred)
{
  if (!_current_set) {
    return 0;
  }
  if (_current_set->empty()) {
    return 0;
  }
  I1DContainer new_set;
  I1DContainer rejected;
  Functions1D::ChainingTimeStampF1D cts;
  Functions1D::TimeStampF1D ts;
  I1DContainer::iterator it = _current_set->begin();
  I1DContainer::iterator itbegin = it;
  while (it != _current_set->end()) {
    Interface1D *i1d = *it;
    cts(*i1d);  // mark everyone's chaining time stamp anyway
    if (pred(*i1d) < 0) {
      new_set.clear();
      rejected.clear();
      return -1;
    }
    if (pred.result) {
      new_set.push_back(i1d);
      ts(*i1d);
    }
    else {
      rejected.push_back(i1d);
    }
    ++it;
  }
  if ((*itbegin)->getExactTypeName() != "ViewEdge") {
    for (it = rejected.begin(); it != rejected.end(); ++it) {
      delete *it;
    }
  }
  rejected.clear();
  _current_set->clear();
  *_current_set = new_set;
  return 0;
}

int Operators::chain(ViewEdgeInternal::ViewEdgeIterator &it,
                     UnaryPredicate1D &pred,
                     UnaryFunction1D_void &modifier)
{
  if (_current_view_edges_set.empty()) {
    return 0;
  }

  unsigned id = 0;
  ViewEdge *edge;
  I1DContainer new_chains_set;

  for (I1DContainer::iterator it_edge = _current_view_edges_set.begin();
       it_edge != _current_view_edges_set.end();
       ++it_edge) {
    if (pred(**it_edge) < 0) {
      goto error;
    }
    if (pred.result) {
      continue;
    }

    edge = dynamic_cast<ViewEdge *>(*it_edge);
    it.setBegin(edge);
    it.setCurrentEdge(edge);

    Chain *new_chain = new Chain(id);
    ++id;
    while (true) {
      new_chain->push_viewedge_back(*it, it.getOrientation());
      if (modifier(**it) < 0) {
        delete new_chain;
        goto error;
      }
      ++it;
      if (it.isEnd()) {
        break;
      }
      if (pred(**it) < 0) {
        delete new_chain;
        goto error;
      }
      if (pred.result) {
        break;
      }
    }
    new_chains_set.push_back(new_chain);
  }

  if (!new_chains_set.empty()) {
    for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
      _current_chains_set.push_back(*it);
    }
    new_chains_set.clear();
    _current_set = &_current_chains_set;
  }
  return 0;

error:
  for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
    delete (*it);
  }
  new_chains_set.clear();
  return -1;
}

int Operators::chain(ViewEdgeInternal::ViewEdgeIterator &it, UnaryPredicate1D &pred)
{
  if (_current_view_edges_set.empty()) {
    return 0;
  }

  unsigned id = 0;
  Functions1D::IncrementChainingTimeStampF1D ts;
  Predicates1D::EqualToChainingTimeStampUP1D pred_ts(TimeStamp::instance()->getTimeStamp() + 1);
  ViewEdge *edge;
  I1DContainer new_chains_set;

  for (I1DContainer::iterator it_edge = _current_view_edges_set.begin();
       it_edge != _current_view_edges_set.end();
       ++it_edge) {
    if (pred(**it_edge) < 0) {
      goto error;
    }
    if (pred.result) {
      continue;
    }
    if (pred_ts(**it_edge) < 0) {
      goto error;
    }
    if (pred_ts.result) {
      continue;
    }

    edge = dynamic_cast<ViewEdge *>(*it_edge);
    it.setBegin(edge);
    it.setCurrentEdge(edge);

    Chain *new_chain = new Chain(id);
    ++id;
    while (true) {
      new_chain->push_viewedge_back(*it, it.getOrientation());
      ts(**it);
      ++it;
      if (it.isEnd()) {
        break;
      }
      if (pred(**it) < 0) {
        delete new_chain;
        goto error;
      }
      if (pred.result) {
        break;
      }
      if (pred_ts(**it) < 0) {
        delete new_chain;
        goto error;
      }
      if (pred_ts.result) {
        break;
      }
    }
    new_chains_set.push_back(new_chain);
  }

  if (!new_chains_set.empty()) {
    for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
      _current_chains_set.push_back(*it);
    }
    new_chains_set.clear();
    _current_set = &_current_chains_set;
  }
  return 0;

error:
  for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
    delete (*it);
  }
  new_chains_set.clear();
  return -1;
}

#if 0
void Operators::bidirectionalChain(ViewEdgeIterator &it,
                                   UnaryPredicate1D &pred,
                                   UnaryFunction1D_void &modifier)
{
  if (_current_view_edges_set.empty()) {
    return;
  }

  unsigned id = 0;
  ViewEdge *edge;
  Chain *new_chain;

  for (I1DContainer::iterator it_edge = _current_view_edges_set.begin();
       it_edge != _current_view_edges_set.end();
       ++it_edge) {
    if (pred(**it_edge)) {
      continue;
    }

    edge = dynamic_cast<ViewEdge *>(*it_edge);
    it.setBegin(edge);
    it.setCurrentEdge(edge);

    Chain *new_chain = new Chain(id);
    ++id;
#  if 0  // FIXME
    ViewEdgeIterator it_back(it);
    --it_back;
#  endif
    do {
      new_chain->push_viewedge_back(*it, it.getOrientation());
      modifier(**it);
      ++it;
    } while (!it.isEnd() && !pred(**it));
    it.setBegin(edge);
    it.setCurrentEdge(edge);
    --it;
    while (!it.isEnd() && !pred(**it)) {
      new_chain->push_viewedge_front(*it, it.getOrientation());
      modifier(**it);
      --it;
    }

    _current_chains_set.push_back(new_chain);
  }

  if (!_current_chains_set.empty()) {
    _current_set = &_current_chains_set;
  }
}

void Operators::bidirectionalChain(ViewEdgeIterator &it, UnaryPredicate1D &pred)
{
  if (_current_view_edges_set.empty()) {
    return;
  }

  unsigned id = 0;
  Functions1D::IncrementChainingTimeStampF1D ts;
  Predicates1D::EqualToChainingTimeStampUP1D pred_ts(TimeStamp::instance()->getTimeStamp() + 1);

  ViewEdge *edge;
  Chain *new_chain;

  for (I1DContainer::iterator it_edge = _current_view_edges_set.begin();
       it_edge != _current_view_edges_set.end();
       ++it_edge) {
    if (pred(**it_edge) || pred_ts(**it_edge)) {
      continue;
    }

    edge = dynamic_cast<ViewEdge *>(*it_edge);
    it.setBegin(edge);
    it.setCurrentEdge(edge);

    Chain *new_chain = new Chain(id);
    ++id;
#  if 0  // FIXME
    ViewEdgeIterator it_back(it);
    --it_back;
#  endif
    do {
      new_chain->push_viewedge_back(*it, it.getOrientation());
      ts(**it);
      ++it;
    } while (!it.isEnd() && !pred(**it) && !pred_ts(**it));
    it.setBegin(edge);
    it.setCurrentEdge(edge);
    --it;
    while (!it.isEnd() && !pred(**it) && !pred_ts(**it)) {
      new_chain->push_viewedge_front(*it, it.getOrientation());
      ts(**it);
      --it;
    }

    _current_chains_set.push_back(new_chain);
  }

  if (!_current_chains_set.empty()) {
    _current_set = &_current_chains_set;
  }
}
#endif

int Operators::bidirectionalChain(ChainingIterator &it, UnaryPredicate1D &pred)
{
  if (_current_view_edges_set.empty()) {
    return 0;
  }

  unsigned id = 0;
  Functions1D::IncrementChainingTimeStampF1D ts;
  Predicates1D::EqualToChainingTimeStampUP1D pred_ts(TimeStamp::instance()->getTimeStamp() + 1);
  ViewEdge *edge;
  I1DContainer new_chains_set;

  for (I1DContainer::iterator it_edge = _current_view_edges_set.begin();
       it_edge != _current_view_edges_set.end();
       ++it_edge) {
    if (pred(**it_edge) < 0) {
      goto error;
    }
    if (pred.result) {
      continue;
    }
    if (pred_ts(**it_edge) < 0) {
      goto error;
    }
    if (pred_ts.result) {
      continue;
    }

    edge = dynamic_cast<ViewEdge *>(*it_edge);
    // re-init iterator
    it.setBegin(edge);
    it.setCurrentEdge(edge);
    it.setOrientation(true);
    if (it.init() < 0) {
      goto error;
    }

    Chain *new_chain = new Chain(id);
    ++id;
#if 0  // FIXME
    ViewEdgeIterator it_back(it);
    --it_back;
#endif
    while (true) {
      new_chain->push_viewedge_back(*it, it.getOrientation());
      ts(**it);
      if (it.increment() < 0) {
        delete new_chain;
        goto error;
      }
      if (it.isEnd()) {
        break;
      }
      if (pred(**it) < 0) {
        delete new_chain;
        goto error;
      }
      if (pred.result) {
        break;
      }
    }
    it.setBegin(edge);
    it.setCurrentEdge(edge);
    it.setOrientation(true);
    if (it.decrement() < 0) {
      delete new_chain;
      goto error;
    }
    while (!it.isEnd()) {
      if (pred(**it) < 0) {
        delete new_chain;
        goto error;
      }
      if (pred.result) {
        break;
      }
      new_chain->push_viewedge_front(*it, it.getOrientation());
      ts(**it);
      if (it.decrement() < 0) {
        delete new_chain;
        goto error;
      }
    }
    new_chains_set.push_back(new_chain);
  }

  if (!new_chains_set.empty()) {
    for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
      _current_chains_set.push_back(*it);
    }
    new_chains_set.clear();
    _current_set = &_current_chains_set;
  }
  return 0;

error:
  for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
    delete (*it);
  }
  new_chains_set.clear();
  return -1;
}

int Operators::bidirectionalChain(ChainingIterator &it)
{
  if (_current_view_edges_set.empty()) {
    return 0;
  }

  unsigned id = 0;
  Functions1D::IncrementChainingTimeStampF1D ts;
  Predicates1D::EqualToChainingTimeStampUP1D pred_ts(TimeStamp::instance()->getTimeStamp() + 1);
  ViewEdge *edge;
  I1DContainer new_chains_set;

  for (I1DContainer::iterator it_edge = _current_view_edges_set.begin();
       it_edge != _current_view_edges_set.end();
       ++it_edge) {
    if (pred_ts(**it_edge) < 0) {
      goto error;
    }
    if (pred_ts.result) {
      continue;
    }

    edge = dynamic_cast<ViewEdge *>(*it_edge);
    // re-init iterator
    it.setBegin(edge);
    it.setCurrentEdge(edge);
    it.setOrientation(true);
    if (it.init() < 0) {
      goto error;
    }

    Chain *new_chain = new Chain(id);
    ++id;
#if 0  // FIXME
    ViewEdgeIterator it_back(it);
    --it_back;
#endif
    do {
      new_chain->push_viewedge_back(*it, it.getOrientation());
      ts(**it);
      if (it.increment() < 0) {  // FIXME
        delete new_chain;
        goto error;
      }
    } while (!it.isEnd());
    it.setBegin(edge);
    it.setCurrentEdge(edge);
    it.setOrientation(true);
    if (it.decrement() < 0) {  // FIXME
      delete new_chain;
      goto error;
    }
    while (!it.isEnd()) {
      new_chain->push_viewedge_front(*it, it.getOrientation());
      ts(**it);
      if (it.decrement() < 0) {  // FIXME
        delete new_chain;
        goto error;
      }
    }
    new_chains_set.push_back(new_chain);
  }

  if (!new_chains_set.empty()) {
    for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
      _current_chains_set.push_back(*it);
    }
    new_chains_set.clear();
    _current_set = &_current_chains_set;
  }
  return 0;

error:
  for (I1DContainer::iterator it = new_chains_set.begin(); it != new_chains_set.end(); ++it) {
    delete (*it);
  }
  new_chains_set.clear();
  return -1;
}

int Operators::sequentialSplit(UnaryPredicate0D &pred, float sampling)
{
  if (_current_chains_set.empty()) {
    cerr << "Warning: current set empty" << endl;
    return 0;
  }
  CurvePoint *point;
  Chain *new_curve;
  I1DContainer splitted_chains;
  Interface0DIterator first;
  Interface0DIterator end;
  Interface0DIterator last;
  Interface0DIterator it;
  I1DContainer::iterator cit = _current_chains_set.begin(), citend = _current_chains_set.end();
  for (; cit != citend; ++cit) {
    Id currentId = (*cit)->getId();
    new_curve = new Chain(currentId);
    first = (*cit)->pointsBegin(sampling);
    end = (*cit)->pointsEnd(sampling);
    last = end;
    --last;
    it = first;

    point = dynamic_cast<CurvePoint *>(&(*it));
    new_curve->push_vertex_back(point);
    ++it;
    for (; it != end; ++it) {
      point = dynamic_cast<CurvePoint *>(&(*it));
      new_curve->push_vertex_back(point);
      if (pred(it) < 0) {
        delete new_curve;
        goto error;
      }
      if (pred.result && (it != last)) {
        splitted_chains.push_back(new_curve);
        currentId.setSecond(currentId.getSecond() + 1);
        new_curve = new Chain(currentId);
        new_curve->push_vertex_back(point);
      }
    }
    if (new_curve->nSegments() == 0) {
      delete new_curve;
      return 0;
    }

    splitted_chains.push_back(new_curve);
  }

  // Update the current set of chains:
  cit = _current_chains_set.begin();
  for (; cit != citend; ++cit) {
    delete (*cit);
  }
  _current_chains_set.clear();
#if 0
  _current_chains_set = splitted_chains;
#else
  for (cit = splitted_chains.begin(), citend = splitted_chains.end(); cit != citend; ++cit) {
    if ((*cit)->getLength2D() < M_EPSILON) {
      delete (*cit);
      continue;
    }
    _current_chains_set.push_back(*cit);
  }
#endif
  splitted_chains.clear();

  if (!_current_chains_set.empty()) {
    _current_set = &_current_chains_set;
  }
  return 0;

error:
  cit = splitted_chains.begin();
  citend = splitted_chains.end();
  for (; cit != citend; ++cit) {
    delete (*cit);
  }
  splitted_chains.clear();
  return -1;
}

int Operators::sequentialSplit(UnaryPredicate0D &startingPred,
                               UnaryPredicate0D &stoppingPred,
                               float sampling)
{
  if (_current_chains_set.empty()) {
    cerr << "Warning: current set empty" << endl;
    return 0;
  }
  CurvePoint *point;
  Chain *new_curve;
  I1DContainer splitted_chains;
  Interface0DIterator first;
  Interface0DIterator end;
  Interface0DIterator last;
  Interface0DIterator itStart;
  Interface0DIterator itStop;
  I1DContainer::iterator cit = _current_chains_set.begin(), citend = _current_chains_set.end();
  for (; cit != citend; ++cit) {
    Id currentId = (*cit)->getId();
    first = (*cit)->pointsBegin(sampling);
    end = (*cit)->pointsEnd(sampling);
    last = end;
    --last;
    itStart = first;
    do {
      itStop = itStart;
      ++itStop;

      new_curve = new Chain(currentId);
      currentId.setSecond(currentId.getSecond() + 1);

      point = dynamic_cast<CurvePoint *>(&(*itStart));
      new_curve->push_vertex_back(point);
      do {
        point = dynamic_cast<CurvePoint *>(&(*itStop));
        new_curve->push_vertex_back(point);
        ++itStop;
        if (itStop == end) {
          break;
        }
        if (stoppingPred(itStop) < 0) {
          delete new_curve;
          goto error;
        }
      } while (!stoppingPred.result);
      if (itStop != end) {
        point = dynamic_cast<CurvePoint *>(&(*itStop));
        new_curve->push_vertex_back(point);
      }
      if (new_curve->nSegments() == 0) {
        delete new_curve;
      }
      else {
        splitted_chains.push_back(new_curve);
      }
      // find next start
      do {
        ++itStart;
        if (itStart == end) {
          break;
        }
        if (startingPred(itStart) < 0) {
          goto error;
        }
      } while (!startingPred.result);
    } while ((itStart != end) && (itStart != last));
  }

  // Update the current set of chains:
  cit = _current_chains_set.begin();
  for (; cit != citend; ++cit) {
    delete (*cit);
  }
  _current_chains_set.clear();
#if 0
  _current_chains_set = splitted_chains;
#else
  for (cit = splitted_chains.begin(), citend = splitted_chains.end(); cit != citend; ++cit) {
    if ((*cit)->getLength2D() < M_EPSILON) {
      delete (*cit);
      continue;
    }
    _current_chains_set.push_back(*cit);
  }
#endif
  splitted_chains.clear();

  if (!_current_chains_set.empty()) {
    _current_set = &_current_chains_set;
  }
  return 0;

error:
  cit = splitted_chains.begin();
  citend = splitted_chains.end();
  for (; cit != citend; ++cit) {
    delete (*cit);
  }
  splitted_chains.clear();
  return -1;
}

// Internal function
static int __recursiveSplit(Chain *_curve,
                            UnaryFunction0D<double> &func,
                            UnaryPredicate1D &pred,
                            float sampling,
                            Operators::I1DContainer &newChains,
                            Operators::I1DContainer &splitted_chains)
{
  if (((_curve->nSegments() == 1) && (sampling == 0)) || (_curve->getLength2D() <= sampling)) {
    newChains.push_back(_curve);
    return 0;
  }

  CurveInternal::CurvePointIterator first = _curve->curvePointsBegin(sampling);
  CurveInternal::CurvePointIterator second = first;
  ++second;
  CurveInternal::CurvePointIterator end = _curve->curvePointsEnd(sampling);
  CurveInternal::CurvePointIterator it = second;
  CurveInternal::CurvePointIterator split = second;
  Interface0DIterator it0d = it.castToInterface0DIterator();
  real _min = FLT_MAX;  // func(it0d);
  ++it;
  CurveInternal::CurvePointIterator next = it;
  ++next;

  bool bsplit = false;
  for (; ((it != end) && (next != end)); ++it, ++next) {
    it0d = it.castToInterface0DIterator();
    if (func(it0d) < 0) {
      return -1;
    }
    if (func.result < _min) {
      _min = func.result;
      split = it;
      bsplit = true;
    }
  }

  if (!bsplit) {  // we didn't find any minimum
    newChains.push_back(_curve);
    return 0;
  }

  // retrieves the current splitting id
  Id *newId = _curve->getSplittingId();
  if (newId == 0) {
    newId = new Id(_curve->getId());
    _curve->setSplittingId(newId);
  }

  Chain *new_curve_a = new Chain(*newId);
  newId->setSecond(newId->getSecond() + 1);
  new_curve_a->setSplittingId(newId);
  Chain *new_curve_b = new Chain(*newId);
  newId->setSecond(newId->getSecond() + 1);
  new_curve_b->setSplittingId(newId);

  CurveInternal::CurvePointIterator vit = _curve->curveVerticesBegin(),
                                    vitend = _curve->curveVerticesEnd();
  CurveInternal::CurvePointIterator vnext = vit;
  ++vnext;

  for (; (vit != vitend) && (vnext != vitend) &&
         (vnext._CurvilinearLength < split._CurvilinearLength);
       ++vit, ++vnext) {
    new_curve_a->push_vertex_back(&(*vit));
  }
  if ((vit == vitend) || (vnext == vitend)) {
    if (G.debug & G_DEBUG_FREESTYLE) {
      cout << "The split takes place in bad location" << endl;
    }
    newChains.push_back(_curve);
    delete new_curve_a;
    delete new_curve_b;
    return 0;
  }

  // build the two resulting chains
  new_curve_a->push_vertex_back(&(*vit));
  new_curve_a->push_vertex_back(&(*split));
  new_curve_b->push_vertex_back(&(*split));

  for (vit = vnext; vit != vitend; ++vit) {
    new_curve_b->push_vertex_back(&(*vit));
  }

  // let's check whether one or two of the two new curves satisfy the stopping condition or not.
  // (if one of them satisfies it, we don't split)
  if (pred(*new_curve_a) < 0 || (!pred.result && pred(*new_curve_b) < 0)) {
    delete new_curve_a;
    delete new_curve_b;
    return -1;
  }
  if (pred.result) {
    // we don't actually create these two chains
    newChains.push_back(_curve);
    delete new_curve_a;
    delete new_curve_b;
    return 0;
  }
  // here we know we'll split _curve:
  splitted_chains.push_back(_curve);

  __recursiveSplit(new_curve_a, func, pred, sampling, newChains, splitted_chains);
  __recursiveSplit(new_curve_b, func, pred, sampling, newChains, splitted_chains);
  return 0;
}

int Operators::recursiveSplit(UnaryFunction0D<double> &func,
                              UnaryPredicate1D &pred,
                              float sampling)
{
  if (_current_chains_set.empty()) {
    cerr << "Warning: current set empty" << endl;
    return 0;
  }

  Chain *currentChain = 0;
  I1DContainer splitted_chains;
  I1DContainer newChains;
  I1DContainer::iterator cit = _current_chains_set.begin(), citend = _current_chains_set.end();
  for (; cit != citend; ++cit) {
    currentChain = dynamic_cast<Chain *>(*cit);
    if (!currentChain) {
      continue;
    }
    // let's check the first one:
    if (pred(*currentChain) < 0) {
      return -1;
    }
    if (!pred.result) {
      __recursiveSplit(currentChain, func, pred, sampling, newChains, splitted_chains);
    }
    else {
      newChains.push_back(currentChain);
    }
  }
  // Update the current set of chains:
  if (!splitted_chains.empty()) {
    for (cit = splitted_chains.begin(), citend = splitted_chains.end(); cit != citend; ++cit) {
      delete (*cit);
    }
    splitted_chains.clear();
  }

  _current_chains_set.clear();
#if 0
  _current_chains_set = newChains;
#else
  for (cit = newChains.begin(), citend = newChains.end(); cit != citend; ++cit) {
    if ((*cit)->getLength2D() < M_EPSILON) {
      delete (*cit);
      continue;
    }
    _current_chains_set.push_back(*cit);
  }
#endif
  newChains.clear();

  if (!_current_chains_set.empty()) {
    _current_set = &_current_chains_set;
  }
  return 0;
}

// recursive split with pred 0D
static int __recursiveSplit(Chain *_curve,
                            UnaryFunction0D<double> &func,
                            UnaryPredicate0D &pred0d,
                            UnaryPredicate1D &pred,
                            float sampling,
                            Operators::I1DContainer &newChains,
                            Operators::I1DContainer &splitted_chains)
{
  if (((_curve->nSegments() == 1) && (sampling == 0)) || (_curve->getLength2D() <= sampling)) {
    newChains.push_back(_curve);
    return 0;
  }

  CurveInternal::CurvePointIterator first = _curve->curvePointsBegin(sampling);
  CurveInternal::CurvePointIterator second = first;
  ++second;
  CurveInternal::CurvePointIterator end = _curve->curvePointsEnd(sampling);
  CurveInternal::CurvePointIterator it = second;
  CurveInternal::CurvePointIterator split = second;
  Interface0DIterator it0d = it.castToInterface0DIterator();
#if 0
  real _min = func(it0d);
  ++it;
#endif
  real _min = FLT_MAX;
  ++it;
  real mean = 0.f;
  // soc unused - real variance                              = 0.0f;
  unsigned count = 0;
  CurveInternal::CurvePointIterator next = it;
  ++next;

  bool bsplit = false;
  for (; ((it != end) && (next != end)); ++it, ++next) {
    ++count;
    it0d = it.castToInterface0DIterator();
    if (pred0d(it0d) < 0) {
      return -1;
    }
    if (!pred0d.result) {
      continue;
    }
    if (func(it0d) < 0) {
      return -1;
    }
    mean += func.result;
    if (func.result < _min) {
      _min = func.result;
      split = it;
      bsplit = true;
    }
  }
  mean /= (float)count;

  // if ((!bsplit) || (mean - _min > mean)) { // we didn't find any minimum
  if (!bsplit) {  // we didn't find any minimum
    newChains.push_back(_curve);
    return 0;
  }

  // retrieves the current splitting id
  Id *newId = _curve->getSplittingId();
  if (newId == NULL) {
    newId = new Id(_curve->getId());
    _curve->setSplittingId(newId);
  }

  Chain *new_curve_a = new Chain(*newId);
  newId->setSecond(newId->getSecond() + 1);
  new_curve_a->setSplittingId(newId);
  Chain *new_curve_b = new Chain(*newId);
  newId->setSecond(newId->getSecond() + 1);
  new_curve_b->setSplittingId(newId);

  CurveInternal::CurvePointIterator vit = _curve->curveVerticesBegin(),
                                    vitend = _curve->curveVerticesEnd();
  CurveInternal::CurvePointIterator vnext = vit;
  ++vnext;

  for (; (vit != vitend) && (vnext != vitend) &&
         (vnext._CurvilinearLength < split._CurvilinearLength);
       ++vit, ++vnext) {
    new_curve_a->push_vertex_back(&(*vit));
  }
  if ((vit == vitend) || (vnext == vitend)) {
    if (G.debug & G_DEBUG_FREESTYLE) {
      cout << "The split takes place in bad location" << endl;
    }
    newChains.push_back(_curve);
    delete new_curve_a;
    delete new_curve_b;
    return 0;
  }

  // build the two resulting chains
  new_curve_a->push_vertex_back(&(*vit));
  new_curve_a->push_vertex_back(&(*split));
  new_curve_b->push_vertex_back(&(*split));

  for (vit = vnext; vit != vitend; ++vit) {
    new_curve_b->push_vertex_back(&(*vit));
  }

  // let's check whether one or two of the two new curves satisfy the stopping condition or not.
  // (if one of them satisfies it, we don't split)
  if (pred(*new_curve_a) < 0 || (!pred.result && pred(*new_curve_b) < 0)) {
    delete new_curve_a;
    delete new_curve_b;
    return -1;
  }
  if (pred.result) {
    // we don't actually create these two chains
    newChains.push_back(_curve);
    delete new_curve_a;
    delete new_curve_b;
    return 0;
  }
  // here we know we'll split _curve:
  splitted_chains.push_back(_curve);

  __recursiveSplit(new_curve_a, func, pred0d, pred, sampling, newChains, splitted_chains);
  __recursiveSplit(new_curve_b, func, pred0d, pred, sampling, newChains, splitted_chains);
  return 0;
}

int Operators::recursiveSplit(UnaryFunction0D<double> &func,
                              UnaryPredicate0D &pred0d,
                              UnaryPredicate1D &pred,
                              float sampling)
{
  if (_current_chains_set.empty()) {
    cerr << "Warning: current set empty" << endl;
    return 0;
  }

  Chain *currentChain = 0;
  I1DContainer splitted_chains;
  I1DContainer newChains;
  I1DContainer::iterator cit = _current_chains_set.begin(), citend = _current_chains_set.end();
  for (; cit != citend; ++cit) {
    currentChain = dynamic_cast<Chain *>(*cit);
    if (!currentChain) {
      continue;
    }
    // let's check the first one:
    if (pred(*currentChain) < 0) {
      return -1;
    }
    if (!pred.result) {
      __recursiveSplit(currentChain, func, pred0d, pred, sampling, newChains, splitted_chains);
    }
    else {
      newChains.push_back(currentChain);
    }
  }
  // Update the current set of chains:
  if (!splitted_chains.empty()) {
    for (cit = splitted_chains.begin(), citend = splitted_chains.end(); cit != citend; ++cit) {
      delete (*cit);
    }
    splitted_chains.clear();
  }

  _current_chains_set.clear();
#if 0
  _current_chains_set = newChains;
#else
  for (cit = newChains.begin(), citend = newChains.end(); cit != citend; ++cit) {
    if ((*cit)->getLength2D() < M_EPSILON) {
      delete (*cit);
      continue;
    }
    _current_chains_set.push_back(*cit);
  }
#endif
  newChains.clear();

  if (!_current_chains_set.empty()) {
    _current_set = &_current_chains_set;
  }
  return 0;
}

// Internal class
class PredicateWrapper {
 public:
  inline PredicateWrapper(BinaryPredicate1D &pred)
  {
    _pred = &pred;
  }

  inline bool operator()(Interface1D *i1, Interface1D *i2)
  {
    if (i1 == i2) {
      return false;
    }
    if ((*_pred)(*i1, *i2) < 0) {
      throw std::runtime_error("comparison failed");
    }
    return _pred->result;
  }

 private:
  BinaryPredicate1D *_pred;
};

int Operators::sort(BinaryPredicate1D &pred)
{
  if (!_current_set) {
    return 0;
  }
  PredicateWrapper wrapper(pred);
  try {
    std::sort(_current_set->begin(), _current_set->end(), wrapper);
  }
  catch (std::runtime_error &e) {
    cerr << "Warning: Operator.sort(): " << e.what() << endl;
    return -1;
  }
  return 0;
}

static Stroke *createStroke(Interface1D &inter)
{
  Stroke *stroke = new Stroke;
  stroke->setId(inter.getId());

  float currentCurvilignAbscissa = 0.0f;

  Interface0DIterator it = inter.verticesBegin(), itend = inter.verticesEnd();
  Interface0DIterator itfirst = it;

  Vec2r current(it->getPoint2D());
  Vec2r previous = current;
  SVertex *sv;
  CurvePoint *cp;
  StrokeVertex *stroke_vertex = NULL;
  bool hasSingularity = false;

  do {
    cp = dynamic_cast<CurvePoint *>(&(*it));
    if (!cp) {
      sv = dynamic_cast<SVertex *>(&(*it));
      if (!sv) {
        cerr << "Warning: unexpected Vertex type" << endl;
        continue;
      }
      stroke_vertex = new StrokeVertex(sv);
    }
    else {
      stroke_vertex = new StrokeVertex(cp);
    }
    current = stroke_vertex->getPoint2D();
    Vec2r vec_tmp(current - previous);
    real dist = vec_tmp.norm();
    if (dist < 1.0e-6) {
      hasSingularity = true;
    }
    currentCurvilignAbscissa += dist;
    stroke_vertex->setCurvilinearAbscissa(currentCurvilignAbscissa);
    stroke->push_back(stroke_vertex);
    previous = current;
    ++it;
  } while ((it != itend) && (it != itfirst));

  if (it == itfirst) {
    // Add last vertex:
    cp = dynamic_cast<CurvePoint *>(&(*it));
    if (!cp) {
      sv = dynamic_cast<SVertex *>(&(*it));
      if (!sv) {
        cerr << "Warning: unexpected Vertex type" << endl;
      }
      else {
        stroke_vertex = new StrokeVertex(sv);
      }
    }
    else {
      stroke_vertex = new StrokeVertex(cp);
    }
    current = stroke_vertex->getPoint2D();
    Vec2r vec_tmp(current - previous);
    real dist = vec_tmp.norm();
    if (dist < 1.0e-6) {
      hasSingularity = true;
    }
    currentCurvilignAbscissa += dist;
    stroke_vertex->setCurvilinearAbscissa(currentCurvilignAbscissa);
    stroke->push_back(stroke_vertex);
  }
  // Discard the stroke if the number of stroke vertices is less than two
  if (stroke->strokeVerticesSize() < 2) {
    delete stroke;
    return NULL;
  }
  stroke->setLength(currentCurvilignAbscissa);
  if (hasSingularity) {
    // Try to address singular points such that the distance between two subsequent vertices
    // are smaller than epsilon.
    StrokeInternal::StrokeVertexIterator v = stroke->strokeVerticesBegin();
    StrokeInternal::StrokeVertexIterator vnext = v;
    ++vnext;
    Vec2r next((*v).getPoint());
    while (!vnext.isEnd()) {
      current = next;
      next = (*vnext).getPoint();
      if ((next - current).norm() < 1.0e-6) {
        StrokeInternal::StrokeVertexIterator vprevious = v;
        if (!vprevious.isBegin()) {
          --vprevious;
        }

        // collect a set of overlapping vertices
        std::vector<StrokeVertex *> overlapping_vertices;
        overlapping_vertices.push_back(&(*v));
        do {
          overlapping_vertices.push_back(&(*vnext));
          current = next;
          ++v;
          ++vnext;
          if (vnext.isEnd()) {
            break;
          }
          next = (*vnext).getPoint();
        } while ((next - current).norm() < 1.0e-6);

        Vec2r target;
        bool reverse;
        if (!vnext.isEnd()) {
          target = (*vnext).getPoint();
          reverse = false;
        }
        else if (!vprevious.isBegin()) {
          target = (*vprevious).getPoint();
          reverse = true;
        }
        else {
          // Discard the stroke because all stroke vertices are overlapping
          delete stroke;
          return NULL;
        }
        current = overlapping_vertices.front()->getPoint();
        Vec2r dir(target - current);
        real dist = dir.norm();
        real len = 1.0e-3;  // default offset length
        int nvert = overlapping_vertices.size();
        if (dist < len * nvert) {
          len = dist / nvert;
        }
        dir.normalize();
        Vec2r offset(dir * len);
        // add the offset to the overlapping vertices
        StrokeVertex *sv;
        std::vector<StrokeVertex *>::iterator it = overlapping_vertices.begin();
        if (!reverse) {
          for (int n = 0; n < nvert; n++) {
            sv = (*it);
            sv->setPoint(sv->getPoint() + offset * (n + 1));
            ++it;
          }
        }
        else {
          for (int n = 0; n < nvert; n++) {
            sv = (*it);
            sv->setPoint(sv->getPoint() + offset * (nvert - n));
            ++it;
          }
        }

        if (vnext.isEnd()) {
          break;
        }
      }
      ++v;
      ++vnext;
    }
  }
  {
    // Check if the stroke no longer contains singular points
    Interface0DIterator v = stroke->verticesBegin();
    Interface0DIterator vnext = v;
    ++vnext;
    Vec2r next((*v).getPoint2D());
    bool warning = false;
    while (!vnext.isEnd()) {
      current = next;
      next = (*vnext).getPoint2D();
      if ((next - current).norm() < 1.0e-6) {
        warning = true;
        break;
      }
      ++v;
      ++vnext;
    }
    if (warning && G.debug & G_DEBUG_FREESTYLE) {
      printf("Warning: stroke contains singular points.\n");
    }
  }
  return stroke;
}

inline int applyShading(Stroke &stroke, vector<StrokeShader *> &shaders)
{
  for (vector<StrokeShader *>::iterator it = shaders.begin(); it != shaders.end(); ++it) {
    if ((*it)->shade(stroke) < 0) {
      return -1;
    }
  }
  return 0;
}

int Operators::create(UnaryPredicate1D &pred, vector<StrokeShader *> shaders)
{
  // Canvas* canvas = Canvas::getInstance();
  if (!_current_set) {
    cerr << "Warning: current set empty" << endl;
    return 0;
  }
  StrokesContainer new_strokes_set;
  for (Operators::I1DContainer::iterator it = _current_set->begin(); it != _current_set->end();
       ++it) {
    if (pred(**it) < 0) {
      goto error;
    }
    if (!pred.result) {
      continue;
    }

    Stroke *stroke = createStroke(**it);
    if (stroke) {
      if (applyShading(*stroke, shaders) < 0) {
        delete stroke;
        goto error;
      }
      // canvas->RenderStroke(stroke);
      new_strokes_set.push_back(stroke);
    }
  }

  for (StrokesContainer::iterator it = new_strokes_set.begin(); it != new_strokes_set.end();
       ++it) {
    _current_strokes_set.push_back(*it);
  }
  new_strokes_set.clear();
  return 0;

error:
  for (StrokesContainer::iterator it = new_strokes_set.begin(); it != new_strokes_set.end();
       ++it) {
    delete (*it);
  }
  new_strokes_set.clear();
  return -1;
}

void Operators::reset(bool removeStrokes)
{
  ViewMap *vm = ViewMap::getInstance();
  if (!vm) {
    cerr << "Error: no ViewMap computed yet" << endl;
    return;
  }
  _current_view_edges_set.clear();
  for (I1DContainer::iterator it = _current_chains_set.begin(); it != _current_chains_set.end();
       ++it) {
    delete *it;
  }
  _current_chains_set.clear();

  ViewMap::viewedges_container &vedges = vm->ViewEdges();
  ViewMap::viewedges_container::iterator ve = vedges.begin(), veend = vedges.end();
  for (; ve != veend; ++ve) {
    if ((*ve)->getLength2D() < M_EPSILON) {
      continue;
    }
    _current_view_edges_set.push_back(*ve);
  }
  _current_set = &_current_view_edges_set;
  if (removeStrokes) {
    _current_strokes_set.clear();
  }
}

} /* namespace Freestyle */
