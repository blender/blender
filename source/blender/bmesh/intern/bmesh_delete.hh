/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "bmesh_class.hh"

#include "BLI_function_ref.hh"

void BMO_mesh_delete_oflag_tagged(BMesh *bm, short oflag, char htype);
void BM_mesh_delete_hflag_tagged(BMesh *bm, char hflag, char htype);

/**
 * \param oflag: Geometry tagged with this operator flag is deleted.
 * This flag applies to different types in some contexts, not just the type being removed.
 *
 * \param prepare_fn: Optional callback that runs before deleting geometry,
 * use this to execute any logic that needs to ensure references to deleted geometry
 * aren't held by the caller.
 */
void BMO_mesh_delete_oflag_context(BMesh *bm,
                                   short oflag,
                                   int type,
                                   blender::FunctionRef<void()> prepare_fn);

/**
 * \param oflag: Geometry tagged with this operator flag is deleted.
 * This flag applies to different types in some contexts, not just the type being removed.
 */
void BM_mesh_delete_hflag_context(BMesh *bm, char hflag, int type);
