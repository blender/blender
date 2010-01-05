/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifndef __GPU_BUFFERS_H__
#define __GPU_BUFFERS_H__

#define MAX_FREE_GPU_BUFFERS 8

#ifdef _DEBUG
/*#define DEBUG_VBO(X) printf(X)*/
#define DEBUG_VBO(X)
#else
#define DEBUG_VBO(X)
#endif

struct DerivedMesh;
struct GHash;

/* V - vertex, N - normal, T - uv, C - color
   F - float, UB - unsigned byte */
#define GPU_BUFFER_INTER_V3F	1
#define GPU_BUFFER_INTER_N3F	2
#define GPU_BUFFER_INTER_T2F	3
#define GPU_BUFFER_INTER_C3UB	4
#define GPU_BUFFER_INTER_C4UB	5
#define GPU_BUFFER_INTER_END	-1

typedef struct GPUBuffer
{
	int size;	/* in bytes */
	void *pointer;	/* used with vertex arrays */
	unsigned int id;	/* used with vertex buffer objects */
} GPUBuffer;

typedef struct GPUBufferPool
{
	int size;	/* number of allocated buffers stored */
	int start;	/* for a queue like structure */
				/* when running out of space for storing buffers,
				the last one used will be thrown away */

	GPUBuffer* buffers[MAX_FREE_GPU_BUFFERS];
} GPUBufferPool;

typedef struct GPUBufferMaterial
{
	int start;	/* at which vertex in the buffer the material starts */
	int end;	/* at which vertex it ends */
	char mat_nr;
} GPUBufferMaterial;

typedef struct IndexLink {
	int element;
	struct IndexLink *next;
} IndexLink;

typedef struct GPUDrawObject
{
	GPUBuffer *vertices;
	GPUBuffer *normals;
	GPUBuffer *uv;
	GPUBuffer *colors;
	GPUBuffer *edges;
	GPUBuffer *uvedges;

	int	*faceRemap;			/* at what index was the face originally in DerivedMesh */
	IndexLink *indices;		/* given an index, find all elements using it */
	IndexLink *indexMem;	/* for faster memory allocation/freeing */
	int indexMemUsage;		/* how many are already allocated */
	int colType;

	GPUBufferMaterial *materials;

	int nmaterials;
	int nelements;	/* (number of faces) * 3 */
	int nlooseverts;
	int nedges;
	int nindices;
	int legacy;	/* if there was a failure allocating some buffer, use old rendering code */

} GPUDrawObject;

typedef struct GPUAttrib
{
	int index;
	int size;
	int type;
} GPUAttrib;

GPUBufferPool *GPU_buffer_pool_new();
void GPU_buffer_pool_free( GPUBufferPool *pool );	/* TODO: Find a place where to call this function on exit */

GPUBuffer *GPU_buffer_alloc( int size, GPUBufferPool *pool );
void GPU_buffer_free( GPUBuffer *buffer, GPUBufferPool *pool );

GPUDrawObject *GPU_drawobject_new( struct DerivedMesh *dm );
void GPU_drawobject_free( struct DerivedMesh *dm );

/* Buffers for non-DerivedMesh drawing */
void *GPU_build_mesh_buffers(struct GHash *map, struct MVert *mvert,
			struct MFace *mface, int *face_indices,
			int totface, int *vert_indices, int uniq_verts,
			int totvert);
void GPU_update_mesh_buffers(void *buffers, struct MVert *mvert,
			int *vert_indices, int totvert);
void *GPU_build_grid_buffers(struct DMGridData **grids,
	int *grid_indices, int totgrid, int gridsize);
void GPU_update_grid_buffers(void *buffers_v, struct DMGridData **grids,
	int *grid_indices, int totgrid, int gridsize);
void GPU_draw_buffers(void *buffers);
void GPU_free_buffers(void *buffers);

/* called before drawing */
void GPU_vertex_setup( struct DerivedMesh *dm );
void GPU_normal_setup( struct DerivedMesh *dm );
void GPU_uv_setup( struct DerivedMesh *dm );
void GPU_color_setup( struct DerivedMesh *dm );
void GPU_edge_setup( struct DerivedMesh *dm );	/* does not mix with other data */
void GPU_uvedge_setup( struct DerivedMesh *dm );
void GPU_interleaved_setup( GPUBuffer *buffer, int data[] );
int GPU_attrib_element_size( GPUAttrib data[], int numdata );
void GPU_interleaved_attrib_setup( GPUBuffer *buffer, GPUAttrib data[], int numdata );

/* can't lock more than one buffer at once */
void *GPU_buffer_lock( GPUBuffer *buffer );	
void *GPU_buffer_lock_stream( GPUBuffer *buffer );
void GPU_buffer_unlock( GPUBuffer *buffer );

/* upload three unsigned chars, representing RGB colors, for each vertex. Resets dm->drawObject->colType to -1 */
void GPU_color3_upload( struct DerivedMesh *dm, unsigned char *data );
/* upload four unsigned chars, representing RGBA colors, for each vertex. Resets dm->drawObject->colType to -1 */
void GPU_color4_upload( struct DerivedMesh *dm, unsigned char *data );
/* switch color rendering on=1/off=0 */
void GPU_color_switch( int mode );

void GPU_buffer_draw_elements( GPUBuffer *elements, unsigned int mode, int start, int count );

/* called after drawing */
void GPU_buffer_unbind();

int GPU_buffer_legacy( struct DerivedMesh *dm );

#endif
