/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Object;

void BKE_editlattice_free(struct Object *ob);
void BKE_editlattice_make(struct Object *obedit);
void BKE_editlattice_load(struct Object *obedit);

#ifdef __cplusplus
}
#endif
