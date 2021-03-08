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

/** \file
 * \ingroup spoutliner
 */

#include "BLI_listbase_wrapper.hh"

#include "BKE_fcurve_driver.h"

#include "DNA_anim_types.h"
#include "DNA_listBase.h"

#include "BLT_translation.h"

#include "../outliner_intern.h"
#include "tree_display.h"

#include "tree_element_driver.hh"

namespace blender::ed::outliner {

TreeElementDriverBase::TreeElementDriverBase(TreeElement &legacy_te, AnimData &anim_data)
    : AbstractTreeElement(legacy_te), anim_data_(anim_data)
{
  BLI_assert(legacy_te.store_elem->type == TSE_DRIVER_BASE);
  legacy_te.name = IFACE_("Drivers");
}

void TreeElementDriverBase::expand(SpaceOutliner &space_outliner) const
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
            outliner_add_element(
                &space_outliner, &legacy_te_.subtree, dtar->id, &legacy_te_, TSE_LINKED_OB, 0);
            lastadded = dtar->id;
          }
        }
        DRIVER_TARGETS_LOOPER_END;
      }
    }
  }
}

}  // namespace blender::ed::outliner
