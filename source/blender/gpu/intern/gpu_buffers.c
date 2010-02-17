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

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_ghash.h"

#include "DNA_meshdata_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"

#include "DNA_userdef_types.h"

#include "gpu_buffers.h"

#define GPU_BUFFER_VERTEX_STATE 1
#define GPU_BUFFER_NORMAL_STATE 2
#define GPU_BUFFER_TEXCOORD_STATE 4
#define GPU_BUFFER_COLOR_STATE 8
#define GPU_BUFFER_ELEMENT_STATE 16

#define MAX_GPU_ATTRIB_DATA 32

/* -1 - undefined, 0 - vertex arrays, 1 - VBOs */
int useVBOs = -1;
GPUBufferPool *globalPool = 0;
int GLStates = 0;
GPUAttrib attribData[MAX_GPU_ATTRIB_DATA] = { { -1, 0, 0 } };

GPUBufferPool *GPU_buffer_pool_new()
{
	GPUBufferPool *pool;

	DEBUG_VBO("GPU_buffer_pool_new\n");

	if( useVBOs < 0 ) {
		if( GL_ARB_vertex_buffer_object ) {
			DEBUG_VBO( "Vertex Buffer Objects supported.\n" );
			useVBOs = 1;
		}
		else {
			DEBUG_VBO( "Vertex Buffer Objects NOT supported.\n" );
			useVBOs = 0;
		}
	}

	pool = MEM_callocN(sizeof(GPUBufferPool), "GPU_buffer_pool_new");

	return pool;
}

void GPU_buffer_pool_free(GPUBufferPool *pool)
{
	int i;

	DEBUG_VBO("GPU_buffer_pool_free\n");

	if( pool == 0 )
		pool = globalPool;
	if( pool == 0 )
		return;

	for( i = 0; i < pool->size; i++ ) {
		if( pool->buffers[i] != 0 ) {
			if( useVBOs ) {
				glDeleteBuffersARB( 1, &pool->buffers[i]->id );
			}
			else {
				MEM_freeN( pool->buffers[i]->pointer );
			}
			MEM_freeN(pool->buffers[i]);
		} else {
			ERROR_VBO("Why are we accessing a null buffer in GPU_buffer_pool_free?\n");
		}
	}
	MEM_freeN(pool);
}

void GPU_buffer_pool_remove( int index, GPUBufferPool *pool )
{
	int i;

	if( index >= pool->size || index < 0 ) {
		ERROR_VBO("Wrong index, out of bounds in call to GPU_buffer_pool_remove");
		return;
	}
	DEBUG_VBO("GPU_buffer_pool_remove\n");

	for( i = index; i < pool->size-1; i++ ) {
		pool->buffers[i] = pool->buffers[i+1];
	}
	if( pool->size > 0 )
		pool->buffers[pool->size-1] = 0;

	pool->size--;
}

void GPU_buffer_pool_delete_last( GPUBufferPool *pool )
{
	int last;

	DEBUG_VBO("GPU_buffer_pool_delete_last\n");

	if( pool->size <= 0 )
		return;

	last = pool->size-1;

	if( pool->buffers[last] != 0 ) {
		if( useVBOs ) {
			glDeleteBuffersARB(1,&pool->buffers[last]->id);
			MEM_freeN( pool->buffers[last] );
		}
		else {
			MEM_freeN( pool->buffers[last]->pointer );
			MEM_freeN( pool->buffers[last] );
		}
		pool->buffers[last] = 0;
	} else {
		DEBUG_VBO("Why are we accessing a null buffer?\n");
	}
	pool->size--;
}

GPUBuffer *GPU_buffer_alloc( int size, GPUBufferPool *pool )
{
	char buffer[60];
	int i;
	int cursize;
	GPUBuffer *allocated;
	int bestfit = -1;

	DEBUG_VBO("GPU_buffer_alloc\n");

	if( pool == 0 ) {
		if( globalPool == 0 )
			globalPool = GPU_buffer_pool_new();
		pool = globalPool;
	}

	for( i = 0; i < pool->size; i++ ) {
		cursize = pool->buffers[i]->size;
		if( cursize == size ) {
			allocated = pool->buffers[i];
			GPU_buffer_pool_remove(i,pool);
			DEBUG_VBO("free buffer of exact size found\n");
			return allocated;
		}
		/* smaller buffers won't fit data and buffers at least twice as big are a waste of memory */
		else if( cursize > size && size > cursize/2 ) {
			/* is it closer to the required size than the last appropriate buffer found. try to save memory */
			if( bestfit == -1 || pool->buffers[bestfit]->size > cursize ) {
				bestfit = i;
			}
		}
	}
	if( bestfit == -1 ) {
		DEBUG_VBO("allocating a new buffer\n");

		allocated = MEM_mallocN(sizeof(GPUBuffer), "GPU_buffer_alloc");
		allocated->size = size;
		if( useVBOs == 1 ) {
			glGenBuffersARB( 1, &allocated->id );
			glBindBufferARB( GL_ARRAY_BUFFER_ARB, allocated->id );
			glBufferDataARB( GL_ARRAY_BUFFER_ARB, size, 0, GL_STATIC_DRAW_ARB );
			glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		}
		else {
			allocated->pointer = MEM_mallocN(size, "GPU_buffer_alloc_vertexarray");
			while( allocated->pointer == 0 && pool->size > 0 ) {
				GPU_buffer_pool_delete_last(pool);
				allocated->pointer = MEM_mallocN(size, "GPU_buffer_alloc_vertexarray");
			}
			if( allocated->pointer == 0 && pool->size == 0 ) {
				return 0;
			}
		}
	}
	else {
		sprintf(buffer,"free buffer found. Wasted %d bytes\n", pool->buffers[bestfit]->size-size);
		DEBUG_VBO(buffer);

		allocated = pool->buffers[bestfit];
		GPU_buffer_pool_remove(bestfit,pool);
	}
	return allocated;
}

void GPU_buffer_free( GPUBuffer *buffer, GPUBufferPool *pool )
{
	int i;
	DEBUG_VBO("GPU_buffer_free\n");

	if( buffer == 0 )
		return;
	if( pool == 0 )
		pool = globalPool;
	if( pool == 0 )
		globalPool = GPU_buffer_pool_new();

	/* free the last used buffer in the queue if no more space */
	if( pool->size == MAX_FREE_GPU_BUFFERS ) {
		GPU_buffer_pool_delete_last( pool );
	}

	for( i =pool->size; i > 0; i-- ) {
		pool->buffers[i] = pool->buffers[i-1];
	}
	pool->buffers[0] = buffer;
	pool->size++;
}

GPUDrawObject *GPU_drawobject_new( DerivedMesh *dm )
{
	GPUDrawObject *object;
	MVert *mvert;
	MFace *mface;
	int numverts[32768];	/* material number is an 16-bit short so there's at most 32768 materials */
	int redir[32768];		/* material number is an 16-bit short so there's at most 32768 materials */
	int *index;
	int i;
	int curmat, curverts, numfaces;

	DEBUG_VBO("GPU_drawobject_new\n");

	object = MEM_callocN(sizeof(GPUDrawObject),"GPU_drawobject_new_object");
	object->nindices = dm->getNumVerts(dm);
	object->indices = MEM_mallocN(sizeof(IndexLink)*object->nindices, "GPU_drawobject_new_indices");
	object->nedges = dm->getNumEdges(dm);

	for( i = 0; i < object->nindices; i++ ) {
		object->indices[i].element = -1;
		object->indices[i].next = 0;
	}
	/*object->legacy = 1;*/
	memset(numverts,0,sizeof(int)*32768);

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		if( mface[i].v4 )
			numverts[mface[i].mat_nr+16383] += 6;	/* split every quad into two triangles */
		else
			numverts[mface[i].mat_nr+16383] += 3;
	}

	for( i = 0; i < 32768; i++ ) {
		if( numverts[i] > 0 ) {
			object->nmaterials++;
			object->nelements += numverts[i];
		}
	}
	object->materials = MEM_mallocN(sizeof(GPUBufferMaterial)*object->nmaterials,"GPU_drawobject_new_materials");
	index = MEM_mallocN(sizeof(int)*object->nmaterials,"GPU_drawobject_new_index");

	curmat = curverts = 0;
	for( i = 0; i < 32768; i++ ) {
		if( numverts[i] > 0 ) {
			object->materials[curmat].mat_nr = i-16383;
			object->materials[curmat].start = curverts;
			index[curmat] = curverts/3;
			object->materials[curmat].end = curverts+numverts[i];
			curverts += numverts[i];
			curmat++;
		}
	}
	object->faceRemap = MEM_mallocN(sizeof(int)*object->nelements/3,"GPU_drawobject_new_faceRemap");
	for( i = 0; i < object->nmaterials; i++ ) {
		redir[object->materials[i].mat_nr+16383] = i;	/* material number -> material index */
	}

	object->indexMem = MEM_callocN(sizeof(IndexLink)*object->nelements,"GPU_drawobject_new_indexMem");
	object->indexMemUsage = 0;

#define ADDLINK( INDEX, ACTUAL ) \
		if( object->indices[INDEX].element == -1 ) { \
			object->indices[INDEX].element = ACTUAL; \
		} else { \
			IndexLink *lnk = &object->indices[INDEX]; \
			while( lnk->next != 0 ) lnk = lnk->next; \
			lnk->next = &object->indexMem[object->indexMemUsage]; \
			lnk->next->element = ACTUAL; \
			object->indexMemUsage++; \
		}

	for( i=0; i < numfaces; i++ ) {
		int curInd = index[redir[mface[i].mat_nr+16383]];
		object->faceRemap[curInd] = i; 
		ADDLINK( mface[i].v1, curInd*3 );
		ADDLINK( mface[i].v2, curInd*3+1 );
		ADDLINK( mface[i].v3, curInd*3+2 );
		if( mface[i].v4 ) {
			object->faceRemap[curInd+1] = i;
			ADDLINK( mface[i].v3, curInd*3+3 );
			ADDLINK( mface[i].v4, curInd*3+4 );
			ADDLINK( mface[i].v1, curInd*3+5 );

			index[redir[mface[i].mat_nr+16383]]+=2;
		}
		else {
			index[redir[mface[i].mat_nr+16383]]++;
		}
	}

	for( i = 0; i < object->nindices; i++ ) {
		if( object->indices[i].element == -1 ) {
			object->indices[i].element = object->nelements + object->nlooseverts;
			object->nlooseverts++;
		}
	}
#undef ADDLINK

	MEM_freeN(index);
	return object;
}

void GPU_drawobject_free( DerivedMesh *dm )
{
	GPUDrawObject *object;

	DEBUG_VBO("GPU_drawobject_free\n");

	if( dm == 0 )
		return;
	object = dm->drawObject;
	if( object == 0 )
		return;

	MEM_freeN(object->materials);
	MEM_freeN(object->faceRemap);
	MEM_freeN(object->indices);
	MEM_freeN(object->indexMem);
	GPU_buffer_free( object->vertices, globalPool );
	GPU_buffer_free( object->normals, globalPool );
	GPU_buffer_free( object->uv, globalPool );
	GPU_buffer_free( object->colors, globalPool );
	GPU_buffer_free( object->edges, globalPool );
	GPU_buffer_free( object->uvedges, globalPool );

	MEM_freeN(object);
	dm->drawObject = 0;
}

/* Convenience struct for building the VBO. */
typedef struct {
	float co[3];
	short no[3];
} VertexBufferFormat;

typedef struct {
	/* opengl buffer handles */
	GLuint vert_buf, index_buf;
	GLenum index_type;

	/* mesh pointers in case buffer allocation fails */
	MFace *mface;
	MVert *mvert;
	int *face_indices;
	int totface;

	/* grid pointers */
	DMGridData **grids;
	int *grid_indices;
	int totgrid;
	int gridsize;

	unsigned int tot_tri, tot_quad;
} GPU_Buffers;

void GPU_update_mesh_buffers(void *buffers_v, MVert *mvert,
			int *vert_indices, int totvert)
{
	GPU_Buffers *buffers = buffers_v;
	VertexBufferFormat *vert_data;
	int i;

	if(buffers->vert_buf) {
		/* Build VBO */
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
				 sizeof(VertexBufferFormat) * totvert,
				 NULL, GL_STATIC_DRAW_ARB);
		vert_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

		if(vert_data) {
			for(i = 0; i < totvert; ++i) {
				MVert *v = mvert + vert_indices[i];
				VertexBufferFormat *out = vert_data + i;

				copy_v3_v3(out->co, v->co);
				memcpy(out->no, v->no, sizeof(short) * 3);
			}

			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
		}
		else {
			glDeleteBuffersARB(1, &buffers->vert_buf);
			buffers->vert_buf = 0;
		}

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}

	buffers->mvert = mvert;
}

void *GPU_build_mesh_buffers(GHash *map, MVert *mvert, MFace *mface,
			int *face_indices, int totface,
			int *vert_indices, int tot_uniq_verts,
			int totvert)
{
	GPU_Buffers *buffers;
	unsigned short *tri_data;
	int i, j, k, tottri;

	buffers = MEM_callocN(sizeof(GPU_Buffers), "GPU_Buffers");
	buffers->index_type = GL_UNSIGNED_SHORT;

	/* Count the number of triangles */
	for(i = 0, tottri = 0; i < totface; ++i)
		tottri += mface[face_indices[i]].v4 ? 2 : 1;
	
	if(GL_ARB_vertex_buffer_object)
		glGenBuffersARB(1, &buffers->index_buf);

	if(buffers->index_buf) {
		/* Generate index buffer object */
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
				 sizeof(unsigned short) * tottri * 3, NULL, GL_STATIC_DRAW_ARB);

		/* Fill the triangle buffer */
		tri_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if(tri_data) {
			for(i = 0; i < totface; ++i) {
				MFace *f = mface + face_indices[i];
				int v[3] = {f->v1, f->v2, f->v3};

				for(j = 0; j < (f->v4 ? 2 : 1); ++j) {
					for(k = 0; k < 3; ++k) {
						void *value, *key = SET_INT_IN_POINTER(v[k]);
						int vbo_index;

						value = BLI_ghash_lookup(map, key);
						vbo_index = GET_INT_FROM_POINTER(value);

						if(vbo_index < 0) {
							vbo_index = -vbo_index +
								tot_uniq_verts - 1;
						}

						*tri_data = vbo_index;
						++tri_data;
					}
					v[0] = f->v4;
					v[1] = f->v1;
					v[2] = f->v3;
				}
			}
			glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
		}
		else {
			glDeleteBuffersARB(1, &buffers->index_buf);
			buffers->index_buf = 0;
		}

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	if(buffers->index_buf)
		glGenBuffersARB(1, &buffers->vert_buf);
	GPU_update_mesh_buffers(buffers, mvert, vert_indices, totvert);

	buffers->tot_tri = tottri;

	buffers->mface = mface;
	buffers->face_indices = face_indices;
	buffers->totface = totface;

	return buffers;
}

void GPU_update_grid_buffers(void *buffers_v, DMGridData **grids,
	int *grid_indices, int totgrid, int gridsize)
{
	GPU_Buffers *buffers = buffers_v;
	DMGridData *vert_data;
	int i, totvert;

	totvert= gridsize*gridsize*totgrid;

	/* Build VBO */
	if(buffers->vert_buf) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
				 sizeof(DMGridData) * totvert,
				 NULL, GL_STATIC_DRAW_ARB);
		vert_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if(vert_data) {
			for(i = 0; i < totgrid; ++i) {
				DMGridData *grid= grids[grid_indices[i]];
				memcpy(vert_data, grid, sizeof(DMGridData)*gridsize*gridsize);
				vert_data += gridsize*gridsize;
			}
			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
		}
		else {
			glDeleteBuffersARB(1, &buffers->vert_buf);
			buffers->vert_buf = 0;
		}
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}

	buffers->grids = grids;
	buffers->grid_indices = grid_indices;
	buffers->totgrid = totgrid;
	buffers->gridsize = gridsize;

	//printf("node updated %p\n", buffers_v);
}

void *GPU_build_grid_buffers(DMGridData **grids,
	int *grid_indices, int totgrid, int gridsize)
{
	GPU_Buffers *buffers;
	int i, j, k, totquad, offset= 0;

	buffers = MEM_callocN(sizeof(GPU_Buffers), "GPU_Buffers");

	/* Count the number of quads */
	totquad= (gridsize-1)*(gridsize-1)*totgrid;

	/* Generate index buffer object */
	if(GL_ARB_vertex_buffer_object)
		glGenBuffersARB(1, &buffers->index_buf);

	if(buffers->index_buf) {
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);

		if(totquad < USHRT_MAX) {
			unsigned short *quad_data;

			buffers->index_type = GL_UNSIGNED_SHORT;
			glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
					 sizeof(unsigned short) * totquad * 4, NULL, GL_STATIC_DRAW_ARB);

			/* Fill the quad buffer */
			quad_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
			if(quad_data) {
				for(i = 0; i < totgrid; ++i) {
					for(j = 0; j < gridsize-1; ++j) {
						for(k = 0; k < gridsize-1; ++k) {
							*(quad_data++)= offset + j*gridsize + k;
							*(quad_data++)= offset + (j+1)*gridsize + k;
							*(quad_data++)= offset + (j+1)*gridsize + k+1;
							*(quad_data++)= offset + j*gridsize + k+1;
						}
					}

					offset += gridsize*gridsize;
				}
				glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
			}
			else {
				glDeleteBuffersARB(1, &buffers->index_buf);
				buffers->index_buf = 0;
			}
		}
		else {
			unsigned int *quad_data;

			buffers->index_type = GL_UNSIGNED_INT;
			glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
					 sizeof(unsigned int) * totquad * 4, NULL, GL_STATIC_DRAW_ARB);

			/* Fill the quad buffer */
			quad_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

			if(quad_data) {
				for(i = 0; i < totgrid; ++i) {
					for(j = 0; j < gridsize-1; ++j) {
						for(k = 0; k < gridsize-1; ++k) {
							*(quad_data++)= offset + j*gridsize + k;
							*(quad_data++)= offset + (j+1)*gridsize + k;
							*(quad_data++)= offset + (j+1)*gridsize + k+1;
							*(quad_data++)= offset + j*gridsize + k+1;
						}
					}

					offset += gridsize*gridsize;
				}
				glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
			}
			else {
				glDeleteBuffersARB(1, &buffers->index_buf);
				buffers->index_buf = 0;
			}
		}

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	/* Build VBO */
	if(buffers->index_buf)
		glGenBuffersARB(1, &buffers->vert_buf);
	GPU_update_grid_buffers(buffers, grids, grid_indices, totgrid, gridsize);

	buffers->tot_quad = totquad;

	return buffers;
}

void GPU_draw_buffers(void *buffers_v)
{
	GPU_Buffers *buffers = buffers_v;

	if(buffers->vert_buf && buffers->index_buf) {
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);

		if(buffers->tot_quad) {
			glVertexPointer(3, GL_FLOAT, sizeof(DMGridData), (void*)offsetof(DMGridData, co));
			glNormalPointer(GL_FLOAT, sizeof(DMGridData), (void*)offsetof(DMGridData, no));

			glDrawElements(GL_QUADS, buffers->tot_quad * 4, buffers->index_type, 0);
		}
		else {
			glVertexPointer(3, GL_FLOAT, sizeof(VertexBufferFormat), (void*)offsetof(VertexBufferFormat, co));
			glNormalPointer(GL_SHORT, sizeof(VertexBufferFormat), (void*)offsetof(VertexBufferFormat, no));

			glDrawElements(GL_TRIANGLES, buffers->tot_tri * 3, buffers->index_type, 0);
		}

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
	}
	else if(buffers->totface) {
		/* fallback if we are out of memory */
		int i;

		for(i = 0; i < buffers->totface; ++i) {
			MFace *f = buffers->mface + buffers->face_indices[i];

			glBegin((f->v4)? GL_QUADS: GL_TRIANGLES);
			glNormal3sv(buffers->mvert[f->v1].no);
			glVertex3fv(buffers->mvert[f->v1].co);
			glNormal3sv(buffers->mvert[f->v2].no);
			glVertex3fv(buffers->mvert[f->v2].co);
			glNormal3sv(buffers->mvert[f->v3].no);
			glVertex3fv(buffers->mvert[f->v3].co);
			if(f->v4) {
				glNormal3sv(buffers->mvert[f->v4].no);
				glVertex3fv(buffers->mvert[f->v4].co);
			}
			glEnd();
		}
	}
	else if(buffers->totgrid) {
		int i, x, y, gridsize = buffers->gridsize;

		for(i = 0; i < buffers->totgrid; ++i) {
			DMGridData *grid = buffers->grids[buffers->grid_indices[i]];

			for(y = 0; y < gridsize-1; y++) {
				glBegin(GL_QUAD_STRIP);
				for(x = 0; x < gridsize; x++) {
					DMGridData *a = &grid[y*gridsize + x];
					DMGridData *b = &grid[(y+1)*gridsize + x];

					glNormal3fv(a->no);
					glVertex3fv(a->co);
					glNormal3fv(b->no);
					glVertex3fv(b->co);
				}
				glEnd();
			}
		}
	}
}

void GPU_free_buffers(void *buffers_v)
{
	if(buffers_v) {
		GPU_Buffers *buffers = buffers_v;
		
		if(buffers->vert_buf)
			glDeleteBuffersARB(1, &buffers->vert_buf);
		if(buffers->index_buf)
			glDeleteBuffersARB(1, &buffers->index_buf);

		MEM_freeN(buffers);
	}
}

GPUBuffer *GPU_buffer_setup( DerivedMesh *dm, GPUDrawObject *object, int size, GLenum target, void *user, void (*copy_f)(DerivedMesh *, float *, int *, int *, void *) )
{
	GPUBuffer *buffer;
	float *varray;
	int redir[32768];
	int *index;
	int i;
	int success;
	GLboolean uploaded;

	DEBUG_VBO("GPU_buffer_setup\n");

	if( globalPool == 0 )
		globalPool = GPU_buffer_pool_new();

	buffer = GPU_buffer_alloc(size,globalPool);
	if( buffer == 0 ) {
		dm->drawObject->legacy = 1;
	}
	if( dm->drawObject->legacy ) {
		return 0;
	}

	index = MEM_mallocN(sizeof(int)*object->nmaterials,"GPU_buffer_setup");
	for( i = 0; i < object->nmaterials; i++ ) {
		index[i] = object->materials[i].start*3;
		redir[object->materials[i].mat_nr+16383] = i;
	}

	if( useVBOs ) {
		success = 0;
		while( success == 0 ) {
			glBindBufferARB( target, buffer->id );
			glBufferDataARB( target, buffer->size, 0, GL_STATIC_DRAW_ARB );	/* discard previous data, avoid stalling gpu */
			varray = glMapBufferARB( target, GL_WRITE_ONLY_ARB );
			if( varray == 0 ) {
				DEBUG_VBO( "Failed to map buffer to client address space\n" ); 
				GPU_buffer_free( buffer, globalPool );
				GPU_buffer_pool_delete_last( globalPool );
				buffer= NULL;
				if( globalPool->size > 0 ) {
					GPU_buffer_pool_delete_last( globalPool );
					buffer = GPU_buffer_alloc( size, globalPool );
					if( buffer == 0 ) {
						dm->drawObject->legacy = 1;
						success = 1;
					}
				}
				else {
					dm->drawObject->legacy = 1;
					success = 1;
				}
			}
			else {
				success = 1;
			}
		}

		if( dm->drawObject->legacy == 0 ) {
			uploaded = GL_FALSE;
			while( !uploaded ) {
				(*copy_f)( dm, varray, index, redir, user );
				uploaded = glUnmapBufferARB( target );	/* returns false if data got corruped during transfer */
			}
		}
		glBindBufferARB(target, 0);
	}
	else {
		if( buffer->pointer != 0 ) {
			varray = buffer->pointer;
			(*copy_f)( dm, varray, index, redir, user );
		}
		else {
			dm->drawObject->legacy = 1;
		}
	}

	MEM_freeN(index);

	return buffer;
}

void GPU_buffer_copy_vertex( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int start;
	int i, j, numfaces;

	MVert *mvert;
	MFace *mface;

	DEBUG_VBO("GPU_buffer_copy_vertex\n");

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

		/* v1 v2 v3 */
		VECCOPY(&varray[start],mvert[mface[i].v1].co);
		VECCOPY(&varray[start+3],mvert[mface[i].v2].co);
		VECCOPY(&varray[start+6],mvert[mface[i].v3].co);

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY(&varray[start+9],mvert[mface[i].v3].co);
			VECCOPY(&varray[start+12],mvert[mface[i].v4].co);
			VECCOPY(&varray[start+15],mvert[mface[i].v1].co);
		}
	}
	j = dm->drawObject->nelements*3;
	for( i = 0; i < dm->drawObject->nindices; i++ ) {
		if( dm->drawObject->indices[i].element >= dm->drawObject->nelements ) {
			VECCOPY(&varray[j],mvert[i].co);
			j+=3;
		}
	}
}

GPUBuffer *GPU_buffer_vertex( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_vertex\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*3*(dm->drawObject->nelements+dm->drawObject->nlooseverts), GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_vertex);
}

void GPU_buffer_copy_normal( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int i, numfaces;
	int start;
	float norm[3];

	float *nors= dm->getFaceDataArray(dm, CD_NORMAL);
	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getFaceArray(dm);

	DEBUG_VBO("GPU_buffer_copy_normal\n");

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

		/* v1 v2 v3 */
		if( mface[i].flag & ME_SMOOTH ) {
			VECCOPY(&varray[start],mvert[mface[i].v1].no);
			VECCOPY(&varray[start+3],mvert[mface[i].v2].no);
			VECCOPY(&varray[start+6],mvert[mface[i].v3].no);
		}
		else {
			if( nors ) {
				VECCOPY(&varray[start],&nors[i*3]);
				VECCOPY(&varray[start+3],&nors[i*3]);
				VECCOPY(&varray[start+6],&nors[i*3]);
			}
			if( mface[i].v4 )
				normal_quad_v3( norm,mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co, mvert[mface[i].v4].co);
			else
				normal_tri_v3( norm,mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co);
			VECCOPY(&varray[start],norm);
			VECCOPY(&varray[start+3],norm);
			VECCOPY(&varray[start+6],norm);
		}

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			if( mface[i].flag & ME_SMOOTH ) {
				VECCOPY(&varray[start+9],mvert[mface[i].v3].no);
				VECCOPY(&varray[start+12],mvert[mface[i].v4].no);
				VECCOPY(&varray[start+15],mvert[mface[i].v1].no);
			}
			else {
				VECCOPY(&varray[start+9],norm);
				VECCOPY(&varray[start+12],norm);
				VECCOPY(&varray[start+15],norm);
			}
		}
	}
}

GPUBuffer *GPU_buffer_normal( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_normal\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_normal);
}

void GPU_buffer_copy_uv( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int start;
	int i, numfaces;

	MTFace *mtface;
	MFace *mface;

	DEBUG_VBO("GPU_buffer_copy_uv\n");

	mface = dm->getFaceArray(dm);
	mtface = DM_get_face_data_layer(dm, CD_MTFACE);

	if( mtface == 0 ) {
		DEBUG_VBO("Texture coordinates do not exist for this mesh");
		return;
	}
		
	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 12;
		else
			index[redir[mface[i].mat_nr+16383]] += 6;

		/* v1 v2 v3 */
		VECCOPY2D(&varray[start],mtface[i].uv[0]);
		VECCOPY2D(&varray[start+2],mtface[i].uv[1]);
		VECCOPY2D(&varray[start+4],mtface[i].uv[2]);

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY2D(&varray[start+6],mtface[i].uv[2]);
			VECCOPY2D(&varray[start+8],mtface[i].uv[3]);
			VECCOPY2D(&varray[start+10],mtface[i].uv[0]);
		}
	}
}

GPUBuffer *GPU_buffer_uv( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_uv\n");
	if( DM_get_face_data_layer(dm, CD_MTFACE) != 0 ) /* was sizeof(float)*2 but caused buffer overrun  */
		return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_uv);
	else
		return 0;
}

void GPU_buffer_copy_color3( DerivedMesh *dm, float *varray_, int *index, int *redir, void *user )
{
	int i, numfaces;
	unsigned char *varray = (unsigned char *)varray_;
	unsigned char *mcol = (unsigned char *)user;
	MFace *mface = dm->getFaceArray(dm);

	DEBUG_VBO("GPU_buffer_copy_color3\n");

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		int start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

		/* v1 v2 v3 */
		VECCOPY(&varray[start],&mcol[i*12]);
		VECCOPY(&varray[start+3],&mcol[i*12+3]);
		VECCOPY(&varray[start+6],&mcol[i*12+6]);
		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY(&varray[start+9],&mcol[i*12+6]);
			VECCOPY(&varray[start+12],&mcol[i*12+9]);
			VECCOPY(&varray[start+15],&mcol[i*12]);
		}
	}
}

void GPU_buffer_copy_color4( DerivedMesh *dm, float *varray_, int *index, int *redir, void *user )
{
	int i, numfaces;
	unsigned char *varray = (unsigned char *)varray_;
	unsigned char *mcol = (unsigned char *)user;
	MFace *mface = dm->getFaceArray(dm);

	DEBUG_VBO("GPU_buffer_copy_color4\n");

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		int start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

		/* v1 v2 v3 */
		VECCOPY(&varray[start],&mcol[i*16]);
		VECCOPY(&varray[start+3],&mcol[i*16+4]);
		VECCOPY(&varray[start+6],&mcol[i*16+8]);
		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY(&varray[start+9],&mcol[i*16+8]);
			VECCOPY(&varray[start+12],&mcol[i*16+12]);
			VECCOPY(&varray[start+15],&mcol[i*16]);
		}
	}
}

GPUBuffer *GPU_buffer_color( DerivedMesh *dm )
{
	unsigned char *colors;
	int i, numfaces;
	MCol *mcol;
	GPUBuffer *result;
	DEBUG_VBO("GPU_buffer_color\n");

	mcol = DM_get_face_data_layer(dm, CD_ID_MCOL);
	dm->drawObject->colType = CD_ID_MCOL;
	if(!mcol) {
		mcol = DM_get_face_data_layer(dm, CD_WEIGHT_MCOL);
		dm->drawObject->colType = CD_WEIGHT_MCOL;
	}
	if(!mcol) {
		mcol = DM_get_face_data_layer(dm, CD_MCOL);
		dm->drawObject->colType = CD_MCOL;
	}

	numfaces= dm->getNumFaces(dm);
	colors = MEM_mallocN(numfaces*12*sizeof(unsigned char), "GPU_buffer_color");
	for( i=0; i < numfaces*4; i++ ) {
		colors[i*3] = mcol[i].b;
		colors[i*3+1] = mcol[i].g;
		colors[i*3+2] = mcol[i].r;
	}

	result = GPU_buffer_setup( dm, dm->drawObject, sizeof(char)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, colors, GPU_buffer_copy_color3 );

	MEM_freeN(colors);
	return result;
}

void GPU_buffer_copy_edge( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int i;

	MVert *mvert;
	MEdge *medge;
	unsigned int *varray_ = (unsigned int *)varray;
	int numedges;
 
	DEBUG_VBO("GPU_buffer_copy_edge\n");

	mvert = dm->getVertArray(dm);
	medge = dm->getEdgeArray(dm);

	numedges= dm->getNumEdges(dm);
	for(i = 0; i < numedges; i++) {
		varray_[i*2] = (unsigned int)dm->drawObject->indices[medge[i].v1].element;
		varray_[i*2+1] = (unsigned int)dm->drawObject->indices[medge[i].v2].element;
	}
}

GPUBuffer *GPU_buffer_edge( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_edge\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(int)*2*dm->drawObject->nedges, GL_ELEMENT_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_edge);
}

void GPU_buffer_copy_uvedge( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	MTFace *tf = DM_get_face_data_layer(dm, CD_MTFACE);
	int i, j=0;

	DEBUG_VBO("GPU_buffer_copy_uvedge\n");

	if(tf) {
		for(i = 0; i < dm->numFaceData; i++, tf++) {
			MFace mf;
			dm->getFace(dm,i,&mf);

			VECCOPY2D(&varray[j],tf->uv[0]);
			VECCOPY2D(&varray[j+2],tf->uv[1]);

			VECCOPY2D(&varray[j+4],tf->uv[1]);
			VECCOPY2D(&varray[j+6],tf->uv[2]);

			if(!mf.v4) {
				VECCOPY2D(&varray[j+8],tf->uv[2]);
				VECCOPY2D(&varray[j+10],tf->uv[0]);
				j+=12;
			} else {
				VECCOPY2D(&varray[j+8],tf->uv[2]);
				VECCOPY2D(&varray[j+10],tf->uv[3]);

				VECCOPY2D(&varray[j+12],tf->uv[3]);
				VECCOPY2D(&varray[j+14],tf->uv[0]);
				j+=16;
			}
		}
	}
	else {
		DEBUG_VBO("Could not get MTFACE data layer");
	}
}

GPUBuffer *GPU_buffer_uvedge( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_uvedge\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*2*(dm->drawObject->nelements/3)*2, GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_uvedge);
}


void GPU_vertex_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_vertex_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->vertices == 0 )
		dm->drawObject->vertices = GPU_buffer_vertex( dm );
	if( dm->drawObject->vertices == 0 ) {
		DEBUG_VBO( "Failed to setup vertices\n" );
		return;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->vertices->id );
		glVertexPointer( 3, GL_FLOAT, 0, 0 );
	}
	else {
		glVertexPointer( 3, GL_FLOAT, 0, dm->drawObject->vertices->pointer );
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

void GPU_normal_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_normal_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->normals == 0 )
		dm->drawObject->normals = GPU_buffer_normal( dm );
	if( dm->drawObject->normals == 0 ) {
		DEBUG_VBO( "Failed to setup normals\n" );
		return;
	}
	glEnableClientState( GL_NORMAL_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->normals->id );
		glNormalPointer( GL_FLOAT, 0, 0 );
	}
	else {
		glNormalPointer( GL_FLOAT, 0, dm->drawObject->normals->pointer );
	}

	GLStates |= GPU_BUFFER_NORMAL_STATE;
}

void GPU_uv_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_uv_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->uv == 0 )
		dm->drawObject->uv = GPU_buffer_uv( dm );
	
	if( dm->drawObject->uv != 0 ) {
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		if( useVBOs ) {
			glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->uv->id );
			glTexCoordPointer( 2, GL_FLOAT, 0, 0 );
		}
		else {
			glTexCoordPointer( 2, GL_FLOAT, 0, dm->drawObject->uv->pointer );
		}

		GLStates |= GPU_BUFFER_TEXCOORD_STATE;
	}
}

void GPU_color_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_color_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->colors == 0 )
		dm->drawObject->colors = GPU_buffer_color( dm );
	if( dm->drawObject->colors == 0 ) {
		DEBUG_VBO( "Failed to setup colors\n" );
		return;
	}
	glEnableClientState( GL_COLOR_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->colors->id );
		glColorPointer( 3, GL_UNSIGNED_BYTE, 0, 0 );
	}
	else {
		glColorPointer( 3, GL_UNSIGNED_BYTE, 0, dm->drawObject->colors->pointer );
	}

	GLStates |= GPU_BUFFER_COLOR_STATE;
}

void GPU_edge_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_edge_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->edges == 0 )
		dm->drawObject->edges = GPU_buffer_edge( dm );
	if( dm->drawObject->edges == 0 ) {
		DEBUG_VBO( "Failed to setup edges\n" );
		return;
	}
	if( dm->drawObject->vertices == 0 )
		dm->drawObject->vertices = GPU_buffer_vertex( dm );
	if( dm->drawObject->vertices == 0 ) {
		DEBUG_VBO( "Failed to setup vertices\n" );
		return;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->vertices->id );
		glVertexPointer( 3, GL_FLOAT, 0, 0 );
	}
	else {
		glVertexPointer( 3, GL_FLOAT, 0, dm->drawObject->vertices->pointer );
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;

	if( useVBOs ) {
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, dm->drawObject->edges->id );
	}

	GLStates |= GPU_BUFFER_ELEMENT_STATE;
}

void GPU_uvedge_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_uvedge_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->uvedges == 0 )
		dm->drawObject->uvedges = GPU_buffer_uvedge( dm );
	if( dm->drawObject->uvedges == 0 ) {
		DEBUG_VBO( "Failed to setup UV edges\n" );
		return;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->uvedges->id );
		glVertexPointer( 2, GL_FLOAT, 0, 0 );
	}
	else {
		glVertexPointer( 2, GL_FLOAT, 0, dm->drawObject->uvedges->pointer );
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

void GPU_interleaved_setup( GPUBuffer *buffer, int data[] ) {
	int i;
	int elementsize = 0;
	intptr_t offset = 0;

	DEBUG_VBO("GPU_interleaved_setup\n");

	for( i = 0; data[i] != GPU_BUFFER_INTER_END; i++ ) {
		switch( data[i] ) {
			case GPU_BUFFER_INTER_V3F:
				elementsize += 3*sizeof(float);
				break;
			case GPU_BUFFER_INTER_N3F:
				elementsize += 3*sizeof(float);
				break;
			case GPU_BUFFER_INTER_T2F:
				elementsize += 2*sizeof(float);
				break;
			case GPU_BUFFER_INTER_C3UB:
				elementsize += 3*sizeof(unsigned char);
				break;
			case GPU_BUFFER_INTER_C4UB:
				elementsize += 4*sizeof(unsigned char);
				break;
			default:
				DEBUG_VBO( "Unknown element in data type array in GPU_interleaved_setup\n" );
		}
	}

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		for( i = 0; data[i] != GPU_BUFFER_INTER_END; i++ ) {
			switch( data[i] ) {
				case GPU_BUFFER_INTER_V3F:
					glEnableClientState( GL_VERTEX_ARRAY );
					glVertexPointer( 3, GL_FLOAT, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_VERTEX_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_N3F:
					glEnableClientState( GL_NORMAL_ARRAY );
					glNormalPointer( GL_FLOAT, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_NORMAL_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_T2F:
					glEnableClientState( GL_TEXTURE_COORD_ARRAY );
					glTexCoordPointer( 2, GL_FLOAT, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_TEXCOORD_STATE;
					offset += 2*sizeof(float);
					break;
				case GPU_BUFFER_INTER_C3UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 3, GL_UNSIGNED_BYTE, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 3*sizeof(unsigned char);
					break;
				case GPU_BUFFER_INTER_C4UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 4, GL_UNSIGNED_BYTE, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 4*sizeof(unsigned char);
					break;
			}
		}
	}
	else {
		for( i = 0; data[i] != GPU_BUFFER_INTER_END; i++ ) {
			switch( data[i] ) {
				case GPU_BUFFER_INTER_V3F:
					glEnableClientState( GL_VERTEX_ARRAY );
					glVertexPointer( 3, GL_FLOAT, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_VERTEX_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_N3F:
					glEnableClientState( GL_NORMAL_ARRAY );
					glNormalPointer( GL_FLOAT, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_NORMAL_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_T2F:
					glEnableClientState( GL_TEXTURE_COORD_ARRAY );
					glTexCoordPointer( 2, GL_FLOAT, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_TEXCOORD_STATE;
					offset += 2*sizeof(float);
					break;
				case GPU_BUFFER_INTER_C3UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 3, GL_UNSIGNED_BYTE, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 3*sizeof(unsigned char);
					break;
				case GPU_BUFFER_INTER_C4UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 4, GL_UNSIGNED_BYTE, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 4*sizeof(unsigned char);
					break;
			}
		}
	}
}

static int GPU_typesize( int type ) {
	switch( type ) {
		case GL_FLOAT:
			return sizeof(float);
		case GL_INT:
			return sizeof(int);
		case GL_UNSIGNED_INT:
			return sizeof(unsigned int);
		case GL_BYTE:
			return sizeof(char);
		case GL_UNSIGNED_BYTE:
			return sizeof(unsigned char);
		default:
			return 0;
	}
}

int GPU_attrib_element_size( GPUAttrib data[], int numdata ) {
	int i, elementsize = 0;

	for( i = 0; i < numdata; i++ ) {
		int typesize = GPU_typesize(data[i].type);
		if( typesize == 0 )
			DEBUG_VBO( "Unknown element in data type array in GPU_attrib_element_size\n" );
		else {
			elementsize += typesize*data[i].size;
		}
	}
	return elementsize;
}

void GPU_interleaved_attrib_setup( GPUBuffer *buffer, GPUAttrib data[], int numdata ) {
	int i;
	int elementsize;
	intptr_t offset = 0;

	DEBUG_VBO("GPU_interleaved_attrib_setup\n");

	for( i = 0; i < MAX_GPU_ATTRIB_DATA; i++ ) {
		if( attribData[i].index != -1 ) {
			glDisableVertexAttribArrayARB( attribData[i].index );
		}
		else
			break;
	}
	elementsize = GPU_attrib_element_size( data, numdata );

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		for( i = 0; i < numdata; i++ ) {
			glEnableVertexAttribArrayARB( data[i].index );
			glVertexAttribPointerARB( data[i].index, data[i].size, data[i].type, GL_TRUE, elementsize, (void *)offset );
			offset += data[i].size*GPU_typesize(data[i].type);

			attribData[i].index = data[i].index;
			attribData[i].size = data[i].size;
			attribData[i].type = data[i].type;
		}
		attribData[numdata].index = -1;
	}
	else {
		for( i = 0; i < numdata; i++ ) {
			glEnableVertexAttribArrayARB( data[i].index );
			glVertexAttribPointerARB( data[i].index, data[i].size, data[i].type, GL_TRUE, elementsize, (char *)buffer->pointer + offset );
			offset += data[i].size*GPU_typesize(data[i].type);
		}
	}
}


void GPU_buffer_unbind()
{
	int i;
	DEBUG_VBO("GPU_buffer_unbind\n");

	if( GLStates & GPU_BUFFER_VERTEX_STATE )
		glDisableClientState( GL_VERTEX_ARRAY );
	if( GLStates & GPU_BUFFER_NORMAL_STATE )
		glDisableClientState( GL_NORMAL_ARRAY );
	if( GLStates & GPU_BUFFER_TEXCOORD_STATE )
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	if( GLStates & GPU_BUFFER_COLOR_STATE )
		glDisableClientState( GL_COLOR_ARRAY );
	if( GLStates & GPU_BUFFER_ELEMENT_STATE ) {
		if( useVBOs ) {
			glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
		}
	}
	GLStates &= !(GPU_BUFFER_VERTEX_STATE | GPU_BUFFER_NORMAL_STATE | GPU_BUFFER_TEXCOORD_STATE | GPU_BUFFER_COLOR_STATE | GPU_BUFFER_ELEMENT_STATE);

	for( i = 0; i < MAX_GPU_ATTRIB_DATA; i++ ) {
		if( attribData[i].index != -1 ) {
			glDisableVertexAttribArrayARB( attribData[i].index );
		}
		else
			break;
	}
	if( GLStates != 0 )
		DEBUG_VBO( "Some weird OpenGL state is still set. Why?" );
	if( useVBOs )
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void GPU_color3_upload( DerivedMesh *dm, unsigned char *data )
{
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new(dm);
	GPU_buffer_free(dm->drawObject->colors,globalPool);
	dm->drawObject->colors = GPU_buffer_setup( dm, dm->drawObject, sizeof(char)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, data, GPU_buffer_copy_color3 );
}
void GPU_color4_upload( DerivedMesh *dm, unsigned char *data )
{
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new(dm);
	GPU_buffer_free(dm->drawObject->colors,globalPool);
	dm->drawObject->colors = GPU_buffer_setup( dm, dm->drawObject, sizeof(char)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, data, GPU_buffer_copy_color4 );
}

void GPU_color_switch( int mode )
{
	if( mode ) {
		if( !(GLStates & GPU_BUFFER_COLOR_STATE) )
			glEnableClientState( GL_COLOR_ARRAY );
		GLStates |= GPU_BUFFER_COLOR_STATE;
	}
	else {
		if( GLStates & GPU_BUFFER_COLOR_STATE )
			glDisableClientState( GL_COLOR_ARRAY );
		GLStates &= (!GPU_BUFFER_COLOR_STATE);
	}
}

int GPU_buffer_legacy( DerivedMesh *dm )
{
	int test= (U.gameflags & USER_DISABLE_VBO);
	if( test )
		return 1;

	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new(dm);
	return dm->drawObject->legacy;
}

void *GPU_buffer_lock( GPUBuffer *buffer )
{
	float *varray;

	DEBUG_VBO("GPU_buffer_lock\n");
	if( buffer == 0 ) {
		DEBUG_VBO( "Failed to lock NULL buffer\n" );
		return 0;
	}

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		varray = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		if( varray == 0 ) {
			DEBUG_VBO( "Failed to map buffer to client address space\n" ); 
		}
		return varray;
	}
	else {
		return buffer->pointer;
	}
}

void *GPU_buffer_lock_stream( GPUBuffer *buffer )
{
	float *varray;

	DEBUG_VBO("GPU_buffer_lock_stream\n");
	if( buffer == 0 ) {
		DEBUG_VBO( "Failed to lock NULL buffer\n" );
		return 0;
	}

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, buffer->size, 0, GL_STREAM_DRAW_ARB );	/* discard previous data, avoid stalling gpu */
		varray = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		if( varray == 0 ) {
			DEBUG_VBO( "Failed to map buffer to client address space\n" ); 
		}
		return varray;
	}
	else {
		return buffer->pointer;
	}
}

void GPU_buffer_unlock( GPUBuffer *buffer )
{
	DEBUG_VBO( "GPU_buffer_unlock\n" ); 
	if( useVBOs ) {
		if( buffer != 0 ) {
			if( glUnmapBufferARB( GL_ARRAY_BUFFER_ARB ) == 0 ) {
				DEBUG_VBO( "Failed to copy new data\n" ); 
			}
		}
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
}

void GPU_buffer_draw_elements( GPUBuffer *elements, unsigned int mode, int start, int count )
{
	if( useVBOs ) {
		glDrawElements( mode, count, GL_UNSIGNED_INT, (void *)(start*sizeof(unsigned int)) );
	}
	else {
		glDrawElements( mode, count, GL_UNSIGNED_INT, ((int *)elements->pointer)+start );
	}
}
