/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmeta
 */

#pragma once

#include "DNA_object_types.h"

#include "DNA_windowmanager_types.h"

void MBALL_OT_hide_metaelems(wmOperatorType *ot);
void MBALL_OT_reveal_metaelems(wmOperatorType *ot);

void MBALL_OT_delete_metaelems(wmOperatorType *ot);
void MBALL_OT_duplicate_metaelems(wmOperatorType *ot);

void MBALL_OT_select_all(wmOperatorType *ot);
void MBALL_OT_select_similar(wmOperatorType *ot);
void MBALL_OT_select_random_metaelems(wmOperatorType *ot);
