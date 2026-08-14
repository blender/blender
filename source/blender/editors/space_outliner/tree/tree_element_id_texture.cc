/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_outliner_types.h"
#include "DNA_texture_types.h"

#include "../outliner_intern.hh"

#include "tree_display.hh"
#include "tree_element_id_texture.hh"

namespace blender::ed::outliner {

TreeElementIDTexture::TreeElementIDTexture(TreeElement &legacy_te, Tex &texture)
    : TreeElementID(legacy_te, texture.id), texture_(texture)
{
}

void TreeElementIDTexture::expand(SpaceOutliner & /*space_outliner*/) const
{
  expand_animation_data(texture_.adt);

  expand_image();
}

void TreeElementIDTexture::expand_image() const
{
  add_id_element({}, reinterpret_cast<ID *>(texture_.ima));
}

}  // namespace blender::ed::outliner
