/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edundo
 */

#pragma once

/* internal exports only */

struct UndoType;

/* memfile_undo.cc */

/** Export for ED_undo_sys. */
void ED_memfile_undosys_type(UndoType *ut);
