/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_cache.h
 *  \ingroup draw
 */

#ifndef __DRAW_CACHE_H__
#define __DRAW_CACHE_H__

struct Batch;
struct Object;

void DRW_shape_cache_free(void);

/* Common Shapes */
struct Batch *DRW_cache_fullscreen_quad_get(void);
struct Batch *DRW_cache_single_vert_get(void);
struct Batch *DRW_cache_single_line_get(void);
struct Batch *DRW_cache_single_line_endpoints_get(void);
struct Batch *DRW_cache_screenspace_circle_get(void);

/* Empties */
struct Batch *DRW_cache_plain_axes_get(void);
struct Batch *DRW_cache_single_arrow_get(void);
struct Batch *DRW_cache_cube_get(void);
struct Batch *DRW_cache_circle_get(void);
struct Batch *DRW_cache_square_get(void);
struct Batch *DRW_cache_empty_sphere_get(void);
struct Batch *DRW_cache_empty_cone_get(void);
struct Batch *DRW_cache_arrows_get(void);
struct Batch *DRW_cache_axis_names_get(void);

/* Force Field */
struct Batch *DRW_cache_field_wind_get(void);
struct Batch *DRW_cache_field_force_get(void);
struct Batch *DRW_cache_field_vortex_get(void);
struct Batch *DRW_cache_field_tube_limit_get(void);
struct Batch *DRW_cache_field_cone_limit_get(void);

/* Lamps */
struct Batch *DRW_cache_lamp_get(void);
struct Batch *DRW_cache_lamp_sunrays_get(void);
struct Batch *DRW_cache_lamp_area_get(void);
struct Batch *DRW_cache_lamp_hemi_get(void);
struct Batch *DRW_cache_lamp_spot_get(void);
struct Batch *DRW_cache_lamp_spot_square_get(void);

/* Camera */
struct Batch *DRW_cache_camera_get(void);
struct Batch *DRW_cache_camera_tria_get(void);

/* Speaker */
struct Batch *DRW_cache_speaker_get(void);

/* Bones */
struct Batch *DRW_cache_bone_octahedral_get(void);
struct Batch *DRW_cache_bone_octahedral_wire_outline_get(void);
struct Batch *DRW_cache_bone_point_get(void);
struct Batch *DRW_cache_bone_point_wire_outline_get(void);
struct Batch *DRW_cache_bone_arrows_get(void);

/* Meshes */
void DRW_cache_mesh_wire_overlay_get(
        struct Object *ob, struct Batch **tris, struct Batch **ledges, struct Batch **lverts);
struct Batch *DRW_cache_face_centers_get(struct Object *ob);
struct Batch *DRW_cache_mesh_wire_outline_get(struct Object *ob);
struct Batch *DRW_cache_mesh_surface_get(struct Object *ob);
struct Batch *DRW_cache_mesh_surface_verts_get(struct Object *ob);
struct Batch *DRW_cache_mesh_verts_get(struct Object *ob);

/* Lattice */
struct Batch *DRW_cache_lattice_verts_get(struct Object *ob);
struct Batch *DRW_cache_lattice_wire_get(struct Object *ob);

#endif /* __DRAW_CACHE_H__ */