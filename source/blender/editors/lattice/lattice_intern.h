/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup edlattice
 */

#pragma once

/* editlattice_select.c */

void LATTICE_OT_select_all(struct wmOperatorType *ot);
void LATTICE_OT_select_more(struct wmOperatorType *ot);
void LATTICE_OT_select_less(struct wmOperatorType *ot);
void LATTICE_OT_select_ungrouped(struct wmOperatorType *ot);
void LATTICE_OT_select_random(struct wmOperatorType *ot);
void LATTICE_OT_select_mirror(struct wmOperatorType *ot);

/* editlattice_tools.c */

void LATTICE_OT_make_regular(struct wmOperatorType *ot);
void LATTICE_OT_flip(struct wmOperatorType *ot);
