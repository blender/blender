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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __DRAW_CACHE_H__
#define __DRAW_CACHE_H__

struct GPUBatch;
struct GPUMaterial;
struct ModifierData;
struct Object;
struct PTCacheEdit;

void DRW_shape_cache_free(void);
void DRW_shape_cache_reset(void);

/* 3D cursor */
struct GPUBatch *DRW_cache_cursor_get(bool crosshair_lines);

/* Common Shapes */
struct GPUBatch *DRW_cache_grid_get(void);
struct GPUBatch *DRW_cache_fullscreen_quad_get(void);
struct GPUBatch *DRW_cache_quad_get(void);
struct GPUBatch *DRW_cache_quad_wires_get(void);
struct GPUBatch *DRW_cache_cube_get(void);
struct GPUBatch *DRW_cache_sphere_get(void);
struct GPUBatch *DRW_cache_single_vert_get(void);
struct GPUBatch *DRW_cache_single_line_get(void);
struct GPUBatch *DRW_cache_single_line_endpoints_get(void);
struct GPUBatch *DRW_cache_screenspace_circle_get(void);

/* Common Object */
struct GPUBatch *DRW_cache_object_all_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_object_edge_detection_get(struct Object *ob, bool *r_is_manifold);
struct GPUBatch *DRW_cache_object_surface_get(struct Object *ob);
struct GPUBatch *DRW_cache_object_loose_edges_get(struct Object *ob);
struct GPUBatch **DRW_cache_object_surface_material_get(struct Object *ob,
                                                        struct GPUMaterial **gpumat_array,
                                                        uint gpumat_array_len,
                                                        char **auto_layer_names,
                                                        int **auto_layer_is_srgb,
                                                        int *auto_layer_count);
struct GPUBatch *DRW_cache_object_face_wireframe_get(Object *ob);

/* Empties */
struct GPUBatch *DRW_cache_plain_axes_get(void);
struct GPUBatch *DRW_cache_single_arrow_get(void);
struct GPUBatch *DRW_cache_empty_cube_get(void);
struct GPUBatch *DRW_cache_circle_get(void);
struct GPUBatch *DRW_cache_square_get(void);
struct GPUBatch *DRW_cache_empty_sphere_get(void);
struct GPUBatch *DRW_cache_empty_cylinder_get(void);
struct GPUBatch *DRW_cache_empty_cone_get(void);
struct GPUBatch *DRW_cache_empty_capsule_cap_get(void);
struct GPUBatch *DRW_cache_empty_capsule_body_get(void);
struct GPUBatch *DRW_cache_image_plane_get(void);
struct GPUBatch *DRW_cache_image_plane_wire_get(void);

/* Force Field */
struct GPUBatch *DRW_cache_field_wind_get(void);
struct GPUBatch *DRW_cache_field_force_get(void);
struct GPUBatch *DRW_cache_field_vortex_get(void);
struct GPUBatch *DRW_cache_field_tube_limit_get(void);
struct GPUBatch *DRW_cache_field_cone_limit_get(void);

/* Grease Pencil */
struct GPUBatch *DRW_cache_gpencil_axes_get(void);

/* Lights */
struct GPUBatch *DRW_cache_light_get(void);
struct GPUBatch *DRW_cache_light_shadows_get(void);
struct GPUBatch *DRW_cache_light_sunrays_get(void);
struct GPUBatch *DRW_cache_light_area_square_get(void);
struct GPUBatch *DRW_cache_light_area_disk_get(void);
struct GPUBatch *DRW_cache_light_hemi_get(void);
struct GPUBatch *DRW_cache_light_spot_get(void);
struct GPUBatch *DRW_cache_light_spot_volume_get(void);
struct GPUBatch *DRW_cache_light_spot_square_get(void);
struct GPUBatch *DRW_cache_light_spot_square_volume_get(void);

/* Camera */
struct GPUBatch *DRW_cache_camera_get(void);
struct GPUBatch *DRW_cache_camera_frame_get(void);
struct GPUBatch *DRW_cache_camera_tria_get(void);

/* Speaker */
struct GPUBatch *DRW_cache_speaker_get(void);

/* Probe */
struct GPUBatch *DRW_cache_lightprobe_cube_get(void);
struct GPUBatch *DRW_cache_lightprobe_grid_get(void);
struct GPUBatch *DRW_cache_lightprobe_planar_get(void);

/* Bones */
struct GPUBatch *DRW_cache_bone_octahedral_get(void);
struct GPUBatch *DRW_cache_bone_octahedral_wire_get(void);
struct GPUBatch *DRW_cache_bone_box_get(void);
struct GPUBatch *DRW_cache_bone_box_wire_get(void);
struct GPUBatch *DRW_cache_bone_envelope_solid_get(void);
struct GPUBatch *DRW_cache_bone_envelope_outline_get(void);
struct GPUBatch *DRW_cache_bone_point_get(void);
struct GPUBatch *DRW_cache_bone_point_wire_outline_get(void);
struct GPUBatch *DRW_cache_bone_stick_get(void);
struct GPUBatch *DRW_cache_bone_arrows_get(void);
struct GPUBatch *DRW_cache_bone_dof_sphere_get(void);
struct GPUBatch *DRW_cache_bone_dof_lines_get(void);

/* Meshes */
struct GPUBatch *DRW_cache_mesh_all_verts_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_all_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_loose_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_edge_detection_get(struct Object *ob, bool *r_is_manifold);
struct GPUBatch *DRW_cache_mesh_surface_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_edges_get(struct Object *ob);
struct GPUBatch **DRW_cache_mesh_surface_shaded_get(struct Object *ob,
                                                    struct GPUMaterial **gpumat_array,
                                                    uint gpumat_array_len,
                                                    char **auto_layer_names,
                                                    int **auto_layer_is_srgb,
                                                    int *auto_layer_count);
struct GPUBatch **DRW_cache_mesh_surface_texpaint_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_texpaint_single_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_vertpaint_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_weights_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_mesh_analysis_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_face_wireframe_get(struct Object *ob);

void DRW_cache_mesh_sculpt_coords_ensure(struct Object *ob);

/* Curve */
struct GPUBatch *DRW_cache_curve_surface_get(struct Object *ob);
struct GPUBatch **DRW_cache_curve_surface_shaded_get(struct Object *ob,
                                                     struct GPUMaterial **gpumat_array,
                                                     uint gpumat_array_len);
struct GPUBatch *DRW_cache_curve_loose_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_curve_edge_wire_get(struct Object *ob);
struct GPUBatch *DRW_cache_curve_face_wireframe_get(Object *ob);
struct GPUBatch *DRW_cache_curve_edge_detection_get(struct Object *ob, bool *r_is_manifold);
/* edit-mode */
struct GPUBatch *DRW_cache_curve_edge_normal_get(struct Object *ob);
struct GPUBatch *DRW_cache_curve_edge_overlay_get(struct Object *ob);
struct GPUBatch *DRW_cache_curve_vert_overlay_get(struct Object *ob, bool handles);

/* Font */
struct GPUBatch *DRW_cache_text_surface_get(struct Object *ob);
struct GPUBatch *DRW_cache_text_edge_detection_get(Object *ob, bool *r_is_manifold);
struct GPUBatch *DRW_cache_text_loose_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_text_edge_wire_get(struct Object *ob);
struct GPUBatch **DRW_cache_text_surface_shaded_get(struct Object *ob,
                                                    struct GPUMaterial **gpumat_array,
                                                    uint gpumat_array_len);
struct GPUBatch *DRW_cache_text_face_wireframe_get(Object *ob);

/* Surface */
struct GPUBatch *DRW_cache_surf_surface_get(struct Object *ob);
struct GPUBatch *DRW_cache_surf_edge_wire_get(struct Object *ob);
struct GPUBatch *DRW_cache_surf_loose_edges_get(struct Object *ob);
struct GPUBatch **DRW_cache_surf_surface_shaded_get(struct Object *ob,
                                                    struct GPUMaterial **gpumat_array,
                                                    uint gpumat_array_len);
struct GPUBatch *DRW_cache_surf_face_wireframe_get(Object *ob);
struct GPUBatch *DRW_cache_surf_edge_detection_get(struct Object *ob, bool *r_is_manifold);

/* Lattice */
struct GPUBatch *DRW_cache_lattice_verts_get(struct Object *ob);
struct GPUBatch *DRW_cache_lattice_wire_get(struct Object *ob, bool use_weight);
struct GPUBatch *DRW_cache_lattice_vert_overlay_get(struct Object *ob);

/* Particles */
struct GPUBatch *DRW_cache_particles_get_hair(struct Object *object,
                                              struct ParticleSystem *psys,
                                              struct ModifierData *md);
struct GPUBatch *DRW_cache_particles_get_dots(struct Object *object, struct ParticleSystem *psys);
struct GPUBatch *DRW_cache_particles_get_edit_strands(struct Object *object,
                                                      struct ParticleSystem *psys,
                                                      struct PTCacheEdit *edit,
                                                      bool use_weight);
struct GPUBatch *DRW_cache_particles_get_edit_inner_points(struct Object *object,
                                                           struct ParticleSystem *psys,
                                                           struct PTCacheEdit *edit);
struct GPUBatch *DRW_cache_particles_get_edit_tip_points(struct Object *object,
                                                         struct ParticleSystem *psys,
                                                         struct PTCacheEdit *edit);
struct GPUBatch *DRW_cache_particles_get_prim(int type);

/* Metaball */
struct GPUBatch *DRW_cache_mball_surface_get(struct Object *ob);
struct GPUBatch **DRW_cache_mball_surface_shaded_get(struct Object *ob,
                                                     struct GPUMaterial **gpumat_array,
                                                     uint gpumat_array_len);
struct GPUBatch *DRW_cache_mball_face_wireframe_get(Object *ob);
struct GPUBatch *DRW_cache_mball_edge_detection_get(struct Object *ob, bool *r_is_manifold);

#endif /* __DRAW_CACHE_H__ */
