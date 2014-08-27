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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_buffers.h
 *  \ingroup gpu
 */

#ifndef __GPU_BUFFERS_H__
#define __GPU_BUFFERS_H__

#ifdef DEBUG
/*  #define DEBUG_VBO(X) printf(X)*/
#  define DEBUG_VBO(X)
#else
#  define DEBUG_VBO(X)
#endif

struct BMesh;
struct CCGElem;
struct CCGKey;
struct CustomData;
struct DMFlagMat;
struct DerivedMesh;
struct GHash;
struct GSet;
struct GPUVertPointLink;
struct PBVH;

typedef struct GPUBuffer {
	int size;	/* in bytes */
	void *pointer;	/* used with vertex arrays */
	unsigned int id;	/* used with vertex buffer objects */
} GPUBuffer;

typedef struct GPUBufferMaterial {
	/* range of points used for this material */
	int start;
	int totpoint;

	/* original material index */
	short mat_nr;
} GPUBufferMaterial;

/* meshes are split up by material since changing materials requires
 * GL state changes that can't occur in the middle of drawing an
 * array.
 *
 * some simplifying assumptions are made:
 * - all quads are treated as two triangles.
 * - no vertex sharing is used; each triangle gets its own copy of the
 *   vertices it uses (this makes it easy to deal with a vertex used
 *   by faces with different properties, such as smooth/solid shading,
 *   different MCols, etc.)
 *
 * to avoid confusion between the original MVert vertices and the
 * arrays of OpenGL vertices, the latter are referred to here and in
 * the source as `points'. similarly, the OpenGL triangles generated
 * for MFaces are referred to as triangles rather than faces.
 */
typedef struct GPUDrawObject {
	GPUBuffer *points;
	GPUBuffer *normals;
	GPUBuffer *uv;
	GPUBuffer *colors;
	GPUBuffer *edges;
	GPUBuffer *uvedges;

	/* for each triangle, the original MFace index */
	int *triangle_to_mface;

	/* for each original vertex, the list of related points */
	struct GPUVertPointLink *vert_points;

	/* see: USE_GPU_POINT_LINK define */
#if 0
	/* storage for the vert_points lists */
	struct GPUVertPointLink *vert_points_mem;
	int vert_points_usage;
#endif
	
	int colType;

	GPUBufferMaterial *materials;
	int totmaterial;
	
	int tot_triangle_point;
	int tot_loose_point;
	
	/* caches of the original DerivedMesh values */
	int totvert;
	int totedge;

	/* if there was a failure allocating some buffer, use old
	 * rendering code */
	bool legacy;
} GPUDrawObject;

/* used for GLSL materials */
typedef struct GPUAttrib {
	int index;
	int size;
	int type;
} GPUAttrib;

void GPU_global_buffer_pool_free(void);
void GPU_global_buffer_pool_free_unused(void);

GPUBuffer *GPU_buffer_alloc(int size);
void GPU_buffer_free(GPUBuffer *buffer);

GPUDrawObject *GPU_drawobject_new(struct DerivedMesh *dm);
void GPU_drawobject_free(struct DerivedMesh *dm);

/* called before drawing */
void GPU_vertex_setup(struct DerivedMesh *dm);
void GPU_normal_setup(struct DerivedMesh *dm);
void GPU_uv_setup(struct DerivedMesh *dm);
void GPU_texpaint_uv_setup(struct DerivedMesh *dm);
/* colType is the cddata MCol type to use! */
void GPU_color_setup(struct DerivedMesh *dm, int colType);
void GPU_edge_setup(struct DerivedMesh *dm); /* does not mix with other data */
void GPU_uvedge_setup(struct DerivedMesh *dm);
int GPU_attrib_element_size(GPUAttrib data[], int numdata);
void GPU_interleaved_attrib_setup(GPUBuffer *buffer, GPUAttrib data[], int numdata);

/* can't lock more than one buffer at once */
void *GPU_buffer_lock(GPUBuffer *buffer);
void *GPU_buffer_lock_stream(GPUBuffer *buffer);
void GPU_buffer_unlock(GPUBuffer *buffer);

/* switch color rendering on=1/off=0 */
void GPU_color_switch(int mode);

/* used for drawing edges */
void GPU_buffer_draw_elements(GPUBuffer *elements, unsigned int mode, int start, int count);

/* called after drawing */
void GPU_buffer_unbind(void);

/* used to check whether to use the old (without buffers) code */
bool GPU_buffer_legacy(struct DerivedMesh *dm);

/* Buffers for non-DerivedMesh drawing */
typedef struct GPU_PBVH_Buffers GPU_PBVH_Buffers;

/* build */
GPU_PBVH_Buffers *GPU_build_mesh_pbvh_buffers(int (*face_vert_indices)[4],
                                    struct MFace *mface, struct MVert *mvert,
                                    int *face_indices, int totface);

GPU_PBVH_Buffers *GPU_build_grid_pbvh_buffers(int *grid_indices, int totgrid,
                                    unsigned int **grid_hidden, int gridsize);

GPU_PBVH_Buffers *GPU_build_bmesh_pbvh_buffers(int smooth_shading);

/* update */

void GPU_update_mesh_pbvh_buffers(GPU_PBVH_Buffers *buffers, MVert *mvert,
							 int *vert_indices, int totvert, const float *vmask,
							 int (*face_vert_indices)[4], bool show_diffuse_color);

void GPU_update_bmesh_pbvh_buffers(GPU_PBVH_Buffers *buffers,
                              struct BMesh *bm,
                              struct GSet *bm_faces,
                              struct GSet *bm_unique_verts,
                              struct GSet *bm_other_verts,
                              bool show_diffuse_color);

void GPU_update_grid_pbvh_buffers(GPU_PBVH_Buffers *buffers, struct CCGElem **grids,
                             const struct DMFlagMat *grid_flag_mats,
                             int *grid_indices, int totgrid, const struct CCGKey *key,
                             bool show_diffuse_color);

/* draw */
void GPU_draw_pbvh_buffers(GPU_PBVH_Buffers *buffers, DMSetMaterial setMaterial,
                           bool wireframe);

/* debug PBVH draw*/
void GPU_draw_pbvh_BB(float min[3], float max[3], bool leaf);
void GPU_end_draw_pbvh_BB(void);
void GPU_init_draw_pbvh_BB(void);

bool GPU_pbvh_buffers_diffuse_changed(GPU_PBVH_Buffers *buffers, struct GSet *bm_faces, bool show_diffuse_color);

void GPU_free_pbvh_buffers(GPU_PBVH_Buffers *buffers);

#endif
