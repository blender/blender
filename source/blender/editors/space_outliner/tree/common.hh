/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#pragma once

struct ListBase;

namespace blender::ed::outliner {

const char *outliner_idcode_to_plural(short idcode);

void outliner_make_object_parent_hierarchy(ListBase *lb);
bool outliner_animdata_test(const struct AnimData *adt);

}  // namespace blender::ed::outliner
