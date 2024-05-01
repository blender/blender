/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edlattice
 */

#pragma once

/* `editlattice_select.cc` */

void LATTICE_OT_select_all(wmOperatorType *ot);
void LATTICE_OT_select_more(wmOperatorType *ot);
void LATTICE_OT_select_less(wmOperatorType *ot);
void LATTICE_OT_select_ungrouped(wmOperatorType *ot);
void LATTICE_OT_select_random(wmOperatorType *ot);
void LATTICE_OT_select_mirror(wmOperatorType *ot);

/* `editlattice_tools.cc` */

void LATTICE_OT_make_regular(wmOperatorType *ot);
void LATTICE_OT_flip(wmOperatorType *ot);
