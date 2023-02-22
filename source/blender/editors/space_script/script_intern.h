/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup spscript
 */

#pragma once

/* internal exports only */

/* script_ops.c */

void script_operatortypes(void);
void script_keymap(struct wmKeyConfig *keyconf);

/* script_edit.c */

void SCRIPT_OT_reload(struct wmOperatorType *ot);
void SCRIPT_OT_python_file_run(struct wmOperatorType *ot);
