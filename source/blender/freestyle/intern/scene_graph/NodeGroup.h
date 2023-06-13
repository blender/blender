/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to represent a group node. This node can contains several children.
 * \brief It also contains a transform matrix indicating the transform state of the underlying
 * children.
 */

#include <vector>

#include "Node.h"

#include "../system/FreestyleConfig.h"

using namespace std;

namespace Freestyle {

class NodeGroup : public Node {
 public:
  inline NodeGroup() : Node() {}
  virtual ~NodeGroup() {}

  /** Adds a child. Makes a addRef on the iChild reference counter */
  virtual void AddChild(Node *iChild);

  /** destroys all the underlying nodes
   *  Returns the reference counter after having done a release()
   */
  virtual int destroy();

  /** Detaches all the children */
  virtual void DetachChildren();

  /** Detached the specified child */
  virtual void DetachChild(Node *iChild);

  /** Retrieve children */
  virtual void RetrieveChildren(vector<Node *> &oNodes);

  /** Renders every children */
  //  virtual void Render(Renderer *iRenderer);

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);

  /** Updates the BBox */
  virtual const BBox<Vec3r> &UpdateBBox();

  /** Returns the number of children */
  virtual int numberOfChildren()
  {
    return _Children.size();
  }

 protected:
  vector<Node *> _Children;
};

} /* namespace Freestyle */
