/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#pragma once

#include <array>
#include <string>

#include "DNA_userdef_types.h"

namespace blender {

struct wmOperatorType;

/* internal exports only */

/* `userpref_ops.cc` */

void PREFERENCES_OT_start_filter(wmOperatorType *ot);
void PREFERENCES_OT_clear_filter(wmOperatorType *ot);

struct SpaceUserPref_Runtime {
  /** For filtering properties displayed in the space. */
  std::string search_string;
  /**
   * Results (in the same order as the tabs) for whether each tab has properties
   * that match the search filter. Only valid when #search_string is set.
   */
  std::array<bool, USER_SECTION_DEVELOPER_TOOLS * 2> tab_search_results = {};
};

}  // namespace blender
