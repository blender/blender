/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

struct wmGizmoGroupType;
struct wmGizmoType;

/* Exposed to RNA/WM API. */
void BPY_RNA_gizmo_wrapper(wmGizmoType *gzt, void *userdata);
void BPY_RNA_gizmogroup_wrapper(wmGizmoGroupType *gzgt, void *userdata);
