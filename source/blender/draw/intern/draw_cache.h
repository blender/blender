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

struct Gwn_Batch;
struct GPUMaterial;
struct ModifierData;
struct Object;
struct PTCacheEdit;

void DRW_shape_cache_free(void);
void DRW_shape_cache_reset(void);

/* 3D cursor */
struct Gwn_Batch *DRW_cache_cursor_get(bool crosshair_lines);

/* Common Shapes */
struct Gwn_Batch *DRW_cache_fullscreen_quad_get(void);
struct Gwn_Batch *DRW_cache_quad_get(void);
struct Gwn_Batch *DRW_cache_cube_get(void);
struct Gwn_Batch *DRW_cache_sphere_get(void);
struct Gwn_Batch *DRW_cache_single_vert_get(void);
struct Gwn_Batch *DRW_cache_single_line_get(void);
struct Gwn_Batch *DRW_cache_single_line_endpoints_get(void);
struct Gwn_Batch *DRW_cache_screenspace_circle_get(void);

/* Common Object */
struct Gwn_Batch *DRW_cache_object_wire_outline_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_object_edge_detection_get(struct Object *ob, bool *r_is_manifold);
struct Gwn_Batch *DRW_cache_object_surface_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_object_loose_edges_get(struct Object *ob);
struct Gwn_Batch **DRW_cache_object_surface_material_get(
        struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len,
        char **auto_layer_names, int **auto_layer_is_srgb, int *auto_layer_count);
void DRW_cache_object_face_wireframe_get(
        Object *ob, struct GPUTexture **r_vert_tx, struct GPUTexture **r_faceid_tx, int *r_tri_count);

/* Empties */
struct Gwn_Batch *DRW_cache_plain_axes_get(void);
struct Gwn_Batch *DRW_cache_single_arrow_get(void);
struct Gwn_Batch *DRW_cache_empty_cube_get(void);
struct Gwn_Batch *DRW_cache_circle_get(void);
struct Gwn_Batch *DRW_cache_square_get(void);
struct Gwn_Batch *DRW_cache_empty_sphere_get(void);
struct Gwn_Batch *DRW_cache_empty_cylinder_get(void);
struct Gwn_Batch *DRW_cache_empty_cone_get(void);
struct Gwn_Batch *DRW_cache_empty_capsule_cap_get(void);
struct Gwn_Batch *DRW_cache_empty_capsule_body_get(void);
struct Gwn_Batch *DRW_cache_arrows_get(void);
struct Gwn_Batch *DRW_cache_axis_names_get(void);
struct Gwn_Batch *DRW_cache_image_plane_get(void);
struct Gwn_Batch *DRW_cache_image_plane_wire_get(void);

/* Force Field */
struct Gwn_Batch *DRW_cache_field_wind_get(void);
struct Gwn_Batch *DRW_cache_field_force_get(void);
struct Gwn_Batch *DRW_cache_field_vortex_get(void);
struct Gwn_Batch *DRW_cache_field_tube_limit_get(void);
struct Gwn_Batch *DRW_cache_field_cone_limit_get(void);

/* Lamps */
struct Gwn_Batch *DRW_cache_lamp_get(void);
struct Gwn_Batch *DRW_cache_lamp_shadows_get(void);
struct Gwn_Batch *DRW_cache_lamp_sunrays_get(void);
struct Gwn_Batch *DRW_cache_lamp_area_square_get(void);
struct Gwn_Batch *DRW_cache_lamp_area_disk_get(void);
struct Gwn_Batch *DRW_cache_lamp_hemi_get(void);
struct Gwn_Batch *DRW_cache_lamp_spot_get(void);
struct Gwn_Batch *DRW_cache_lamp_spot_square_get(void);

/* Camera */
struct Gwn_Batch *DRW_cache_camera_get(void);
struct Gwn_Batch *DRW_cache_camera_frame_get(void);
struct Gwn_Batch *DRW_cache_camera_tria_get(void);

/* Speaker */
struct Gwn_Batch *DRW_cache_speaker_get(void);

/* Probe */
struct Gwn_Batch *DRW_cache_lightprobe_cube_get(void);
struct Gwn_Batch *DRW_cache_lightprobe_grid_get(void);
struct Gwn_Batch *DRW_cache_lightprobe_planar_get(void);

/* Bones */
struct Gwn_Batch *DRW_cache_bone_octahedral_get(void);
struct Gwn_Batch *DRW_cache_bone_octahedral_wire_get(void);
struct Gwn_Batch *DRW_cache_bone_box_get(void);
struct Gwn_Batch *DRW_cache_bone_box_wire_get(void);
struct Gwn_Batch *DRW_cache_bone_envelope_solid_get(void);
struct Gwn_Batch *DRW_cache_bone_envelope_outline_get(void);
struct Gwn_Batch *DRW_cache_bone_envelope_head_wire_outline_get(void);
struct Gwn_Batch *DRW_cache_bone_point_get(void);
struct Gwn_Batch *DRW_cache_bone_point_wire_outline_get(void);
struct Gwn_Batch *DRW_cache_bone_stick_get(void);
struct Gwn_Batch *DRW_cache_bone_arrows_get(void);

/* Meshes */
struct Gwn_Batch *DRW_cache_mesh_surface_overlay_get(struct Object *ob);
void DRW_cache_mesh_wire_overlay_get(
        struct Object *ob,
        struct Gwn_Batch **r_tris, struct Gwn_Batch **r_ledges, struct Gwn_Batch **r_lverts);
void DRW_cache_mesh_normals_overlay_get(
        struct Object *ob,
        struct Gwn_Batch **r_tris, struct Gwn_Batch **r_ledges, struct Gwn_Batch **r_lverts);
struct Gwn_Batch *DRW_cache_face_centers_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_wire_outline_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_edge_detection_get(struct Object *ob, bool *r_is_manifold);
struct Gwn_Batch *DRW_cache_mesh_surface_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_loose_edges_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_surface_weights_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_surface_vert_colors_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_surface_verts_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_edges_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_verts_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_edges_paint_overlay_get(struct Object *ob, bool use_wire, bool use_sel);
struct Gwn_Batch *DRW_cache_mesh_faces_weight_overlay_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_verts_weight_overlay_get(struct Object *ob);
struct Gwn_Batch **DRW_cache_mesh_surface_shaded_get(
        struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len,
        char **auto_layer_names, int **auto_layer_is_srgb, int *auto_layer_count);
struct Gwn_Batch **DRW_cache_mesh_surface_texpaint_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_mesh_surface_texpaint_single_get(struct Object *ob);

void DRW_cache_mesh_sculpt_coords_ensure(struct Object *ob);

/* Curve */
struct Gwn_Batch *DRW_cache_curve_surface_get(struct Object *ob);
struct Gwn_Batch **DRW_cache_curve_surface_shaded_get(
        struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len);
struct Gwn_Batch *DRW_cache_curve_surface_verts_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_curve_edge_wire_get(struct Object *ob);
/* edit-mode */
struct Gwn_Batch *DRW_cache_curve_edge_normal_get(struct Object *ob, float normal_size);
struct Gwn_Batch *DRW_cache_curve_edge_overlay_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_curve_vert_overlay_get(struct Object *ob);

/* Font */
struct Gwn_Batch *DRW_cache_text_edge_wire_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_text_surface_get(struct Object *ob);
struct Gwn_Batch **DRW_cache_text_surface_shaded_get(
        struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len);
/* edit-mode */
struct Gwn_Batch *DRW_cache_text_cursor_overlay_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_text_select_overlay_get(struct Object *ob);

/* Surface */
struct Gwn_Batch *DRW_cache_surf_surface_get(struct Object *ob);
struct Gwn_Batch **DRW_cache_surf_surface_shaded_get(
        struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len);

/* Lattice */
struct Gwn_Batch *DRW_cache_lattice_verts_get(struct Object *ob);
struct Gwn_Batch *DRW_cache_lattice_wire_get(struct Object *ob, bool use_weight);
struct Gwn_Batch *DRW_cache_lattice_vert_overlay_get(struct Object *ob);

/* Particles */
struct Gwn_Batch *DRW_cache_particles_get_hair(
        struct Object *object, struct ParticleSystem *psys, struct ModifierData *md);
struct Gwn_Batch *DRW_cache_particles_get_dots(
        struct Object *object, struct ParticleSystem *psys);
struct Gwn_Batch *DRW_cache_particles_get_edit_strands(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);
struct Gwn_Batch *DRW_cache_particles_get_edit_inner_points(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);
struct Gwn_Batch *DRW_cache_particles_get_edit_tip_points(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);
struct Gwn_Batch *DRW_cache_particles_get_prim(int type);

/* Metaball */
struct Gwn_Batch *DRW_cache_mball_surface_get(struct Object *ob);
struct Gwn_Batch **DRW_cache_mball_surface_shaded_get(struct Object *ob, struct GPUMaterial **gpumat_array, uint gpumat_array_len);

#endif /* __DRAW_CACHE_H__ */
