/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase_wrapper.hh"

#include "BKE_fcurve_driver.h"

#include "DNA_anim_types.h"
#include "DNA_listBase.h"
#include "DNA_space_types.h"

#include "BLT_translation.h"

#include "../outliner_intern.hh"

#include "tree_element_driver.hh"

namespace blender::ed::outliner {

TreeElementDriverBase::TreeElementDriverBase(TreeElement &legacy_te, AnimData &anim_data)
    : AbstractTreeElement(legacy_te), anim_data_(anim_data)
{
  BLI_assert(legacy_te.store_elem->type == TSE_DRIVER_BASE);
  legacy_te.name = IFACE_("Drivers");
}

void TreeElementDriverBase::expand(SpaceOutliner & /*space_outliner*/) const
{
  ID *lastadded = nullptr;

  for (FCurve *fcu : blender::ListBaseWrapper<FCurve>(anim_data_.drivers)) {
    if (fcu->driver && fcu->driver->variables.first) {
      ChannelDriver *driver = fcu->driver;

      for (DriverVar *dvar : blender::ListBaseWrapper<DriverVar>(driver->variables)) {
        /* loop over all targets used here */
        DRIVER_TARGETS_USED_LOOPER_BEGIN (dvar) {
          if (lastadded != dtar->id) {
            /* XXX this lastadded check is rather lame, and also fails quite badly... */
            add_element(&legacy_te_.subtree, dtar->id, nullptr, &legacy_te_, TSE_LINKED_OB, 0);
            lastadded = dtar->id;
          }
        }
        DRIVER_TARGETS_LOOPER_END;
      }
    }
  }
}

}  // namespace blender::ed::outliner
