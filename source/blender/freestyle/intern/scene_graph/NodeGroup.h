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

#ifndef __FREESTYLE_NODE_GROUP_H__
#define __FREESTYLE_NODE_GROUP_H__

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
  inline NodeGroup() : Node()
  {
  }
  virtual ~NodeGroup()
  {
  }

  /*! Adds a child. Makes a addRef on the iChild reference counter */
  virtual void AddChild(Node *iChild);

  /*! destroys all the underlying nodes
   *  Returns the reference counter after having done a release()
   */
  virtual int destroy();

  /*! Detaches all the children */
  virtual void DetachChildren();

  /*! Detached the sepcified child */
  virtual void DetachChild(Node *iChild);

  /*! Retrieve children */
  virtual void RetrieveChildren(vector<Node *> &oNodes);

  /*! Renders every children */
  //  virtual void Render(Renderer *iRenderer);

  /*! Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);

  /*! Updates the BBox */
  virtual const BBox<Vec3r> &UpdateBBox();

  /*! Returns the number of children */
  virtual int numberOfChildren()
  {
    return _Children.size();
  }

 protected:
  vector<Node *> _Children;
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_NODE_GROUP_H__
