/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spscript
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* internal exports only */

/* `script_ops.cc` */

void script_operatortypes(void);
void script_keymap(struct wmKeyConfig *keyconf);

/* `script_edit.cc` */

void SCRIPT_OT_reload(struct wmOperatorType *ot);
void SCRIPT_OT_python_file_run(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
