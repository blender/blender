/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to represent a view layer in Blender.
 */

#include "Node.h"

#include "DNA_layer_types.h"
#include "DNA_scene_types.h"

using namespace std;

namespace Freestyle {

class NodeViewLayer : public Node {
 public:
  inline NodeViewLayer(blender::Scene &scene, blender::ViewLayer &view_layer)
      : Node(), _Scene(scene), _ViewLayer(view_layer)
  {
  }
  virtual ~NodeViewLayer() {}

  inline struct blender::Scene &scene() const
  {
    return _Scene;
  }

  inline struct blender::ViewLayer &sceneLayer() const
  {
    return _ViewLayer;
  }

  /** Accept the corresponding visitor */
  virtual void accept(SceneVisitor &v);

 protected:
  blender::Scene &_Scene;
  blender::ViewLayer &_ViewLayer;
};

} /* namespace Freestyle */
