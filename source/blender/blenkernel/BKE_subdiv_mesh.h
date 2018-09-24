/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BKE_subdiv_mesh.h
 *  \ingroup bke
 *  \since September 2018
 *  \author Sergey Sharybin
 */

#ifndef __BKE_SUBDIV_MESH_H__
#define __BKE_SUBDIV_MESH_H__

#include "BLI_sys_types.h"

struct Mesh;
struct Subdiv;

typedef struct SubdivToMeshSettings {
	/* Resolution at which regular ptex (created for quad polygon) are being
	 * evaluated. This defines how many vertices final mesh will have: every
	 * regular ptex has resolution^2 vertices. Special (irregular, or ptex
	 * created for a corner of non-quad polygon) will have resolution of
	 * `resolution - 1`.
	 */
	int resolution;
} SubdivToMeshSettings;

/* Create real hi-res mesh from subdivision, all geometry is "real". */
struct Mesh *BKE_subdiv_to_mesh(
        struct Subdiv *subdiv,
        const SubdivToMeshSettings *settings,
        const struct Mesh *coarse_mesh);

#endif  /* __BKE_SUBDIV)MESH_H__ */
