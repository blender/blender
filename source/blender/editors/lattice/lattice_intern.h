/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edlattice
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* `editlattice_select.cc` */

void LATTICE_OT_select_all(struct wmOperatorType *ot);
void LATTICE_OT_select_more(struct wmOperatorType *ot);
void LATTICE_OT_select_less(struct wmOperatorType *ot);
void LATTICE_OT_select_ungrouped(struct wmOperatorType *ot);
void LATTICE_OT_select_random(struct wmOperatorType *ot);
void LATTICE_OT_select_mirror(struct wmOperatorType *ot);

/* `editlattice_tools.cc` */

void LATTICE_OT_make_regular(struct wmOperatorType *ot);
void LATTICE_OT_flip(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
