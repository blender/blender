/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to define a chain of view-edges.
 */

#include "Curve.h"

#include "../view_map/ViewMap.h"

namespace Freestyle {

/** Class to represent a 1D elements issued from the chaining process.
 *  A Chain is the last step before the Stroke and is used in the Splitting and Creation processes.
 */
class Chain : public Curve {
 protected:
  // tmp
  Id *_splittingId;
  FEdge *
      _fedgeB;  // the last FEdge of the ViewEdge passed to the last call for push_viewedge_back().

 public:
  /** Default constructor. */
  Chain() : Curve()
  {
    _splittingId = 0;
    _fedgeB = 0;
  }

  /** Builds a chain from its Id. */
  Chain(const Id &id) : Curve(id)
  {
    _splittingId = 0;
    _fedgeB = 0;
  }

  /** Copy Constructor */
  Chain(const Chain &iBrother) : Curve(iBrother)
  {
    _splittingId = iBrother._splittingId;
    _fedgeB = iBrother._fedgeB;
  }

  /** Destructor. */
  virtual ~Chain()
  {
    // only the last split deletes this id
    if (_splittingId) {
      if (*_splittingId == _Id) {
        delete _splittingId;
      }
    }
  }

  /** Returns the string "Chain" */
  virtual string getExactTypeName() const
  {
    return "Chain";
  }

  /** Adds a ViewEdge at the end of the chain
   *  \param iViewEdge:
   *    The ViewEdge that must be added.
   *  \param orientation:
   *    The orientation with which this ViewEdge must be processed.
   */
  void push_viewedge_back(ViewEdge *iViewEdge, bool orientation);

  /** Adds a ViewEdge at the beginning of the chain
   *  \param iViewEdge:
   *    The ViewEdge that must be added.
   *  \param orientation:
   *    The orientation with which this ViewEdge must be processed.
   */
  void push_viewedge_front(ViewEdge *iViewEdge, bool orientation);

  inline void setSplittingId(Id *sid)
  {
    _splittingId = sid;
  }

  inline Id *getSplittingId()
  {
    return _splittingId;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Chain")
};

} /* namespace Freestyle */
