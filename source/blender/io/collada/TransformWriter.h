/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "COLLADASWNode.h"

#include "DNA_object_types.h"

#include "collada.h"
#include "collada_internal.h"
#include "collada_utils.h"

class TransformWriter {
 protected:
  void add_joint_transform(COLLADASW::Node &node,
                           float mat[4][4],
                           float parent_mat[4][4],
                           BCExportSettings &export_settings,
                           bool has_restmat);

  void add_node_transform_ob(COLLADASW::Node &node, Object *ob, BCExportSettings &export_settings);

  void add_node_transform_identity(COLLADASW::Node &node, BCExportSettings &export_settings);

 private:
  void add_transform(COLLADASW::Node &node,
                     const float loc[3],
                     const float rot[3],
                     const float scale[3]);
};
