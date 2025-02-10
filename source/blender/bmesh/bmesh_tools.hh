/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 *
 * Utility functions that operate directly on the BMesh,
 * These can be used by both Modifiers and BMesh-Operators.
 */

#include "tools/bmesh_beautify.hh"        // IWYU pragma: export
#include "tools/bmesh_bevel.hh"           // IWYU pragma: export
#include "tools/bmesh_bisect_plane.hh"    // IWYU pragma: export
#include "tools/bmesh_boolean.hh"         // IWYU pragma: export
#include "tools/bmesh_decimate.hh"        // IWYU pragma: export
#include "tools/bmesh_edgenet.hh"         // IWYU pragma: export
#include "tools/bmesh_edgesplit.hh"       // IWYU pragma: export
#include "tools/bmesh_path.hh"            // IWYU pragma: export
#include "tools/bmesh_path_region.hh"     // IWYU pragma: export
#include "tools/bmesh_path_region_uv.hh"  // IWYU pragma: export
#include "tools/bmesh_path_uv.hh"         // IWYU pragma: export
#include "tools/bmesh_region_match.hh"    // IWYU pragma: export
#include "tools/bmesh_separate.hh"        // IWYU pragma: export
#include "tools/bmesh_triangulate.hh"     // IWYU pragma: export
