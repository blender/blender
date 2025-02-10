/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 */

#pragma once

#include "BLI_ghash.h"

#include "bmesh_class.hh"

bool BM_mesh_intersect_edges(
    BMesh *bm, char hflag, float dist, bool split_faces, GHash *r_targetmap);
