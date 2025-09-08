/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 */

#include "DNA_outliner_types.h"
#include "DNA_sequence_types.h"

#include "BLI_listbase.h"

#include "BLT_translation.hh"

#include "../outliner_intern.hh"
#include "tree_element_seq.hh"

namespace blender::ed::outliner {

TreeElementStrip::TreeElementStrip(TreeElement &legacy_te, Strip &strip)
    : AbstractTreeElement(legacy_te), strip_(strip)
{
  BLI_assert(legacy_te.store_elem->type == TSE_STRIP);
  legacy_te.name = strip_.name + 2;
}

bool TreeElementStrip::expand_poll(const SpaceOutliner & /*space_outliner*/) const
{
  return !strip_.is_effect();
}

void TreeElementStrip::expand(SpaceOutliner & /*space_outliner*/) const
{
  /*
   * This work like the strip.
   * If the strip have a name (not default name)
   * show it, in other case put the filename.
   */

  if (strip_.type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, child, &strip_.seqbase) {
      add_element(&legacy_te_.subtree, nullptr, child, &legacy_te_, TSE_STRIP, 0);
    }
  }
  else {
    add_element(&legacy_te_.subtree, nullptr, strip_.data, &legacy_te_, TSE_STRIP_DATA, 0);
  }
}

Strip &TreeElementStrip::get_strip() const
{
  return strip_;
}

StripType TreeElementStrip::get_strip_type() const
{
  return StripType(strip_.type);
}

/* -------------------------------------------------------------------- */
/* Strip */

TreeElementStripData::TreeElementStripData(TreeElement &legacy_te, StripData &strip)
    : AbstractTreeElement(legacy_te)
{
  BLI_assert(legacy_te.store_elem->type == TSE_STRIP_DATA);

  if (strip.dirpath[0] != '\0') {
    legacy_te_.name = strip.dirpath;
  }
  else {
    legacy_te_.name = IFACE_("Strip None");
  }
}

/* -------------------------------------------------------------------- */
/* Strip Duplicate */

TreeElementStripDuplicate::TreeElementStripDuplicate(TreeElement &legacy_te, Strip &strip)
    : AbstractTreeElement(legacy_te), strip_(strip)
{
  BLI_assert(legacy_te.store_elem->type == TSE_STRIP_DUP);
  legacy_te_.name = strip.data->stripdata->filename;
}

Strip &TreeElementStripDuplicate::get_strip() const
{
  return strip_;
}

}  // namespace blender::ed::outliner
