/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a chain of viewedges.
 */

#include "Chain.h"

#include "../view_map/ViewMapAdvancedIterators.h"
#include "../view_map/ViewMapIterators.h"

namespace Freestyle {

void Chain::push_viewedge_back(ViewEdge *iViewEdge, bool orientation)
{
  ViewEdge::vertex_iterator v;
  ViewEdge::vertex_iterator vend;
  ViewEdge::vertex_iterator vfirst;
  Vec3r previous, current;
  if (true == orientation) {
    v = iViewEdge->vertices_begin();
    vfirst = v;
    vend = iViewEdge->vertices_end();
  }
  else {
    v = iViewEdge->vertices_last();
    vfirst = v;
    vend = iViewEdge->vertices_end();
  }

  if (!_Vertices.empty()) {
    previous = _Vertices.back()->point2d();
    if (orientation) {
      ++v;
    }
    else {
      --v;
    }
    // Ensure the continuity of underlying FEdges
    CurvePoint *cp =
        _Vertices.back();  // assumed to be instantiated as new CurvePoint(iSVertex, 0, 0.0f);
    SVertex *sv_first = (*vfirst);
    FEdge *fe = _fedgeB->duplicate();
    fe->setTemporary(true);
    fe->setVertexB(sv_first);
    fe->vertexA()->shape()->AddEdge(fe);
    fe->vertexA()->AddFEdge(fe);
    fe->vertexB()->AddFEdge(fe);
    cp->setA(sv_first);
  }
  else {
    previous = (*v)->point2d();
  }
  do {
    current = (*v)->point2d();
    Curve::push_vertex_back(*v);
    //_Length += (current - previous).norm();
    previous = current;
    if (orientation) {
      ++v;
    }
    else {
      --v;
    }
  } while ((v != vend) && (v != vfirst));

  if (v == vfirst) {
    // Add last one:
    current = (*v)->point2d();
    Curve::push_vertex_back(*v);
    //_Length += (current - previous).norm();
  }

  _fedgeB = (orientation) ? iViewEdge->fedgeB() : iViewEdge->fedgeA();
}

void Chain::push_viewedge_front(ViewEdge *iViewEdge, bool orientation)
{
  orientation = !orientation;
  ViewEdge::vertex_iterator v;
  ViewEdge::vertex_iterator vend;
  ViewEdge::vertex_iterator vfirst;
  Vec3r previous, current;
  if (true == orientation) {
    v = iViewEdge->vertices_begin();
    vfirst = v;
    vend = iViewEdge->vertices_end();
  }
  else {
    v = iViewEdge->vertices_last();
    vfirst = v;
    vend = iViewEdge->vertices_end();
  }

  if (!_Vertices.empty()) {
    previous = _Vertices.front()->point2d();
    if (orientation) {
      ++v;
    }
    else {
      --v;
    }
    // Ensure the continuity of underlying FEdges
    CurvePoint *cp =
        _Vertices.front();  // assumed to be instantiated as new CurvePoint(iSVertex, 0, 0.0f);
    SVertex *sv_last = cp->A();
    SVertex *sv_curr = (*v);
    FEdge *fe = (orientation) ? iViewEdge->fedgeA() : iViewEdge->fedgeB();
    FEdge *fe2 = fe->duplicate();
    fe2->setTemporary(true);
    fe2->setVertexA(sv_curr);
    fe2->setVertexB(sv_last);
    sv_last->AddFEdge(fe2);
    sv_curr->AddFEdge(fe2);
    sv_curr->shape()->AddEdge(fe2);
  }
  else {
    previous = (*v)->point2d();
  }
  do {
    current = (*v)->point2d();
    Curve::push_vertex_front(*v);
    //_Length += (current - previous).norm();
    previous = current;
    if (orientation) {
      ++v;
    }
    else {
      --v;
    }
  } while ((v != vend) && (v != vfirst));

  if (v == vfirst) {
    // Add last one:
    current = (*v)->point2d();
    Curve::push_vertex_front(*v);
    //_Length += (current - previous).norm();
  }

  if (!_fedgeB) {
    _fedgeB = (orientation) ? iViewEdge->fedgeB() : iViewEdge->fedgeA();
  }
}

} /* namespace Freestyle */
