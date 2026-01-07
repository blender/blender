/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

#include "DNA_listBase.h"
struct TreeElement;
namespace blender {

struct AnimData;
namespace ed::outliner {

const char *outliner_idcode_to_plural(short idcode);

void outliner_make_object_parent_hierarchy(ListBaseT<TreeElement> *lb);
bool outliner_animdata_test(const AnimData *adt);

}  // namespace ed::outliner
}  // namespace blender
