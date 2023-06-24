/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmeta
 */

#pragma once

#include "DNA_object_types.h"

#include "DNA_windowmanager_types.h"

void MBALL_OT_hide_metaelems(struct wmOperatorType *ot);
void MBALL_OT_reveal_metaelems(struct wmOperatorType *ot);

void MBALL_OT_delete_metaelems(struct wmOperatorType *ot);
void MBALL_OT_duplicate_metaelems(struct wmOperatorType *ot);

void MBALL_OT_select_all(struct wmOperatorType *ot);
void MBALL_OT_select_similar(struct wmOperatorType *ot);
void MBALL_OT_select_random_metaelems(struct wmOperatorType *ot);
