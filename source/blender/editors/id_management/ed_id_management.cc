/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edundo
 */

#include "MEM_guardedalloc.h"

#include "CLG_log.h"

#include "DNA_ID.h"

#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "WM_api.hh"

#include "ED_id_management.hh"

/** We only need this locally. */
static CLG_LogRef LOG = {"ed.id_management"};

bool ED_id_rename(Main &bmain, ID &id, blender::StringRefNull name)
{
  BLI_assert(ID_IS_EDITABLE(&id));

  const IDNewNameResult result = BKE_id_rename(
      bmain, id, name, IDNewNameMode::RenameExistingSameRoot);

  switch (result.action) {
    case IDNewNameResult::Action::UNCHANGED:
      CLOG_INFO(&LOG, 4, "ID '%s' not renamed, already using the requested name", id.name + 2);
      return false;
    case IDNewNameResult::Action::UNCHANGED_COLLISION:
      CLOG_INFO(&LOG,
                4,
                "ID '%s' not renamed, requested new name '%s' would collide with an existing one",
                id.name + 2,
                name.c_str());
      return false;
    case IDNewNameResult::Action::RENAMED_NO_COLLISION:
      CLOG_INFO(&LOG, 4, "ID '%s' renamed without any collision", id.name + 2);
      return true;
    case IDNewNameResult::Action::RENAMED_COLLISION_ADJUSTED:
      CLOG_INFO(&LOG,
                4,
                "ID '%s' renamed with adjustment from requested name '%s', to avoid name "
                "collision with another ID",
                id.name + 2,
                name.c_str());
      WM_reportf(RPT_INFO,
                 "Data-block renamed to '%s', try again to force renaming it to '%s'",
                 id.name + 2,
                 name.c_str());
      return true;
    case IDNewNameResult::Action::RENAMED_COLLISION_FORCED:
      CLOG_INFO(
          &LOG,
          4,
          "ID '%s' forcefully renamed, another ID had to also be renamed to avoid name collision",
          id.name + 2);
      WM_reportf(RPT_INFO,
                 "Name in use. The other data-block was renamed to ‘%s’",
                 result.other_id->name + 2);
      return true;
  }

  return false;
}
