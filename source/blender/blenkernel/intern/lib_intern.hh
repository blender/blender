/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BKE_lib_remap.hh"

extern BKE_library_free_notifier_reference_cb free_notifier_reference_cb;

extern BKE_library_remap_editor_id_reference_cb remap_editor_id_reference_cb;

struct ID;
struct Main;

/**
 * Ensure new (copied) ID is fully made local.
 */
void lib_id_copy_ensure_local(Main *bmain, const ID *old_id, ID *new_id, const int flags);
