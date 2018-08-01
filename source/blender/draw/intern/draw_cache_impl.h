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

/** \file draw_cache_impl.h
 *  \ingroup draw
 */

#ifndef __DRAW_CACHE_IMPL_H__
#define __DRAW_CACHE_IMPL_H__

struct CurveCache;
struct GPUMaterial;
struct GPUTexture;
struct GPUBatch;
struct GPUIndexBuf;
struct GPUVertBuf;
struct ListBase;
struct ModifierData;
struct ParticleSystem;
struct PTCacheEdit;

struct Curve;
struct Lattice;
struct Mesh;
struct MetaBall;
struct bGPdata;

/* Expose via BKE callbacks */
void DRW_mball_batch_cache_dirty(struct MetaBall *mb, int mode);
void DRW_mball_batch_cache_free(struct MetaBall *mb);

void DRW_curve_batch_cache_dirty(struct Curve *cu, int mode);
void DRW_curve_batch_cache_free(struct Curve *cu);

void DRW_mesh_batch_cache_dirty(struct Mesh *me, int mode);
void DRW_mesh_batch_cache_free(struct Mesh *me);

void DRW_lattice_batch_cache_dirty(struct Lattice *lt, int mode);
void DRW_lattice_batch_cache_free(struct Lattice *lt);

void DRW_particle_batch_cache_dirty(struct ParticleSystem *psys, int mode);
void DRW_particle_batch_cache_free(struct ParticleSystem *psys);

void DRW_gpencil_batch_cache_dirty(struct bGPdata *gpd);
void DRW_gpencil_batch_cache_free(struct bGPdata *gpd);

/* Curve */
struct GPUBatch *DRW_curve_batch_cache_get_wire_edge(struct Curve *cu, struct CurveCache *ob_curve_cache);
struct GPUBatch *DRW_curve_batch_cache_get_normal_edge(
        struct Curve *cu, struct CurveCache *ob_curve_cache, float normal_size);
struct GPUBatch *DRW_curve_batch_cache_get_overlay_edges(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_overlay_verts(struct Curve *cu);

struct GPUBatch *DRW_curve_batch_cache_get_triangles_with_normals(
        struct Curve *cu, struct CurveCache *ob_curve_cache);
struct GPUBatch **DRW_curve_batch_cache_get_surface_shaded(
        struct Curve *cu, struct CurveCache *ob_curve_cache,
        struct GPUMaterial **gpumat_array, uint gpumat_array_len);

/* Metaball */
struct GPUBatch *DRW_metaball_batch_cache_get_triangles_with_normals(struct Object *ob);
struct GPUBatch **DRW_metaball_batch_cache_get_surface_shaded(struct Object *ob, struct MetaBall *mb, struct GPUMaterial **gpumat_array, uint gpumat_array_len);

/* Curve (Font) */
struct GPUBatch *DRW_curve_batch_cache_get_overlay_cursor(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_overlay_select(struct Curve *cu);

/* DispList */
struct GPUVertBuf *DRW_displist_vertbuf_calc_pos_with_normals(struct ListBase *lb);
struct GPUIndexBuf *DRW_displist_indexbuf_calc_triangles_in_order(struct ListBase *lb);
struct GPUIndexBuf **DRW_displist_indexbuf_calc_triangles_in_order_split_by_material(
        struct ListBase *lb, uint gpumat_array_len);
struct GPUBatch **DRW_displist_batch_calc_tri_pos_normals_and_uv_split_by_material(
        struct ListBase *lb, uint gpumat_array_len);

/* Lattice */
struct GPUBatch *DRW_lattice_batch_cache_get_all_edges(struct Lattice *lt, bool use_weight, const int actdef);
struct GPUBatch *DRW_lattice_batch_cache_get_all_verts(struct Lattice *lt);
struct GPUBatch *DRW_lattice_batch_cache_get_overlay_verts(struct Lattice *lt);

/* Mesh */

struct GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(
        struct Mesh *me, struct GPUMaterial **gpumat_array, uint gpumat_array_len,
        char **auto_layer_names, int **auto_layer_is_srgb, int *auto_layer_count);
struct GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_weight_overlay_edges(struct Mesh *me, bool use_wire, bool use_sel);
struct GPUBatch *DRW_mesh_batch_cache_get_weight_overlay_faces(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_weight_overlay_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_triangles(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_normals_and_weights(struct Mesh *me, int defgroup);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_normals_and_vert_colors(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(struct Mesh *me, bool use_hide, uint select_id_offset);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_mask(struct Mesh *me, bool use_hide);
struct GPUBatch *DRW_mesh_batch_cache_get_loose_edges_with_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_points_with_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_fancy_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edge_detection(struct Mesh *me, bool *r_is_manifold);
struct GPUBatch *DRW_mesh_batch_cache_get_overlay_triangles(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_overlay_triangles_nor(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_overlay_loose_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_overlay_loose_edges_nor(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_overlay_loose_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_overlay_facedots(struct Mesh *me);
/* edit-mesh selection (use generic function for faces) */
struct GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(struct Mesh *me, uint select_id_offset);
struct GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(struct Mesh *me, uint select_id_offset);
struct GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(struct Mesh *me, uint select_id_offset);
/* Object mode Wireframe overlays */
void DRW_mesh_batch_cache_get_wireframes_face_texbuf(
        struct Mesh *me, struct GPUTexture **verts_data, struct GPUTexture **face_indices, int *tri_count);

void DRW_mesh_cache_sculpt_coords_ensure(struct Mesh *me);

/* Particles */
struct GPUBatch *DRW_particles_batch_cache_get_hair(
        struct Object *object, struct ParticleSystem *psys, struct ModifierData *md);
struct GPUBatch *DRW_particles_batch_cache_get_dots(
        struct Object *object, struct ParticleSystem *psys);
struct GPUBatch *DRW_particles_batch_cache_get_edit_strands(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);
struct GPUBatch *DRW_particles_batch_cache_get_edit_inner_points(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);
struct GPUBatch *DRW_particles_batch_cache_get_edit_tip_points(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);

#endif /* __DRAW_CACHE_IMPL_H__ */
