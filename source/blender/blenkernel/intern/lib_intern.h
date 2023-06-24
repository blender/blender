/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BKE_lib_remap.h"

#ifdef __cplusplus
extern "C" {
#endif

extern BKE_library_free_notifier_reference_cb free_notifier_reference_cb;

extern BKE_library_remap_editor_id_reference_cb remap_editor_id_reference_cb;

struct ID;
struct Main;

void lib_id_copy_ensure_local(struct Main *bmain,
                              const struct ID *old_id,
                              struct ID *new_id,
                              const int flags);

#ifdef __cplusplus
}
#endif
