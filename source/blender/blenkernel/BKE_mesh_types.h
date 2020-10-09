/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 by Blender Foundation
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bke
 */

typedef enum eMeshBatchDirtyMode {
  BKE_MESH_BATCH_DIRTY_ALL = 0,
  BKE_MESH_BATCH_DIRTY_SELECT,
  BKE_MESH_BATCH_DIRTY_SELECT_PAINT,
  BKE_MESH_BATCH_DIRTY_SHADING,
  BKE_MESH_BATCH_DIRTY_UVEDIT_ALL,
  BKE_MESH_BATCH_DIRTY_UVEDIT_SELECT,
} eMeshBatchDirtyMode;
