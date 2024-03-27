/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spscript
 */

#pragma once

/* internal exports only */

/* `script_ops.cc` */

void script_operatortypes();
void script_keymap(wmKeyConfig *keyconf);

/* `script_edit.cc` */

void SCRIPT_OT_reload(wmOperatorType *ot);
void SCRIPT_OT_python_file_run(wmOperatorType *ot);
