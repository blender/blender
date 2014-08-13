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

/** \file blender/gpu/intern/gpu_buffers.c
 *  \ingroup gpu
 *
 * Mesh drawing using OpenGL VBO (Vertex Buffer Objects),
 * with fall-back to vertex arrays.
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_ccg.h"
#include "BKE_DerivedMesh.h"
#include "BKE_paint.h"
#include "BKE_material.h"
#include "BKE_pbvh.h"

#include "DNA_userdef_types.h"

#include "GPU_buffers.h"
#include "GPU_draw.h"

#include "bmesh.h"

typedef enum {
	GPU_BUFFER_VERTEX_STATE = 1,
	GPU_BUFFER_NORMAL_STATE = 2,
	GPU_BUFFER_TEXCOORD_UNIT_0_STATE = 4,
	GPU_BUFFER_TEXCOORD_UNIT_1_STATE = 8,
	GPU_BUFFER_COLOR_STATE = 16,
	GPU_BUFFER_ELEMENT_STATE = 32,
} GPUBufferState;

#define MAX_GPU_ATTRIB_DATA 32

#define BUFFER_OFFSET(n) ((GLubyte *)NULL + (n))

/* -1 - undefined, 0 - vertex arrays, 1 - VBOs */
static int useVBOs = -1;
static GPUBufferState GLStates = 0;
static GPUAttrib attribData[MAX_GPU_ATTRIB_DATA] = { { -1, 0, 0 } };

static ThreadMutex buffer_mutex = BLI_MUTEX_INITIALIZER;

/* stores recently-deleted buffers so that new buffers won't have to
 * be recreated as often
 *
 * only one instance of this pool is created, stored in
 * gpu_buffer_pool
 *
 * note that the number of buffers in the pool is usually limited to
 * MAX_FREE_GPU_BUFFERS, but this limit may be exceeded temporarily
 * when a GPUBuffer is released outside the main thread; due to OpenGL
 * restrictions it cannot be immediately released
 */
typedef struct GPUBufferPool {
	/* number of allocated buffers stored */
	int totbuf;
	int totpbvhbufids;
	/* actual allocated length of the arrays */
	int maxsize;
	int maxpbvhsize;
	GPUBuffer **buffers;
	GLuint *pbvhbufids;
} GPUBufferPool;
#define MAX_FREE_GPU_BUFFERS 8
#define MAX_FREE_GPU_BUFF_IDS 100

/* create a new GPUBufferPool */
static GPUBufferPool *gpu_buffer_pool_new(void)
{
	GPUBufferPool *pool;

	/* enable VBOs if supported */
	if (useVBOs == -1)
		useVBOs = (GLEW_ARB_vertex_buffer_object ? 1 : 0);

	pool = MEM_callocN(sizeof(GPUBufferPool), "GPUBuffer_Pool");

	pool->maxsize = MAX_FREE_GPU_BUFFERS;
	pool->maxpbvhsize = MAX_FREE_GPU_BUFF_IDS;
	pool->buffers = MEM_mallocN(sizeof(*pool->buffers) * pool->maxsize,
	                            "GPUBufferPool.buffers");
	pool->pbvhbufids = MEM_mallocN(sizeof(*pool->pbvhbufids) * pool->maxpbvhsize,
	                               "GPUBufferPool.pbvhbuffers");
	return pool;
}

/* remove a GPUBuffer from the pool (does not free the GPUBuffer) */
static void gpu_buffer_pool_remove_index(GPUBufferPool *pool, int index)
{
	int i;

	if (!pool || index < 0 || index >= pool->totbuf)
		return;

	/* shift entries down, overwriting the buffer at `index' */
	for (i = index; i < pool->totbuf - 1; i++)
		pool->buffers[i] = pool->buffers[i + 1];

	/* clear the last entry */
	if (pool->totbuf > 0)
		pool->buffers[pool->totbuf - 1] = NULL;

	pool->totbuf--;
}

/* delete the last entry in the pool */
static void gpu_buffer_pool_delete_last(GPUBufferPool *pool)
{
	GPUBuffer *last;

	if (pool->totbuf <= 0)
		return;

	/* get the last entry */
	if (!(last = pool->buffers[pool->totbuf - 1]))
		return;

	/* delete the buffer's data */
	if (useVBOs)
		glDeleteBuffersARB(1, &last->id);
	else
		MEM_freeN(last->pointer);

	/* delete the buffer and remove from pool */
	MEM_freeN(last);
	pool->totbuf--;
	pool->buffers[pool->totbuf] = NULL;
}

/* free a GPUBufferPool; also frees the data in the pool's
 * GPUBuffers */
static void gpu_buffer_pool_free(GPUBufferPool *pool)
{
	if (!pool)
		return;
	
	while (pool->totbuf)
		gpu_buffer_pool_delete_last(pool);

	MEM_freeN(pool->buffers);
	MEM_freeN(pool->pbvhbufids);
	MEM_freeN(pool);
}

static void gpu_buffer_pool_free_unused(GPUBufferPool *pool)
{
	if (!pool)
		return;

	BLI_mutex_lock(&buffer_mutex);
	
	while (pool->totbuf)
		gpu_buffer_pool_delete_last(pool);

	glDeleteBuffersARB(pool->totpbvhbufids, pool->pbvhbufids);
	pool->totpbvhbufids = 0;

	BLI_mutex_unlock(&buffer_mutex);
}

static GPUBufferPool *gpu_buffer_pool = NULL;
static GPUBufferPool *gpu_get_global_buffer_pool(void)
{
	/* initialize the pool */
	if (!gpu_buffer_pool)
		gpu_buffer_pool = gpu_buffer_pool_new();

	return gpu_buffer_pool;
}

void GPU_global_buffer_pool_free(void)
{
	gpu_buffer_pool_free(gpu_buffer_pool);
	gpu_buffer_pool = NULL;
}

void GPU_global_buffer_pool_free_unused(void)
{
	gpu_buffer_pool_free_unused(gpu_buffer_pool);
}

/* get a GPUBuffer of at least `size' bytes; uses one from the buffer
 * pool if possible, otherwise creates a new one
 *
 * Thread-unsafe version for internal usage only.
 */
static GPUBuffer *gpu_buffer_alloc_intern(int size)
{
	GPUBufferPool *pool;
	GPUBuffer *buf;
	int i, bufsize, bestfit = -1;

	/* bad case, leads to leak of buf since buf->pointer will allocate
	 * NULL, leading to return without cleanup. In any case better detect early
	 * psy-fi */
	if (size == 0)
		return NULL;

	pool = gpu_get_global_buffer_pool();

	/* not sure if this buffer pool code has been profiled much,
	 * seems to me that the graphics driver and system memory
	 * management might do this stuff anyway. --nicholas
	 */

	/* check the global buffer pool for a recently-deleted buffer
	 * that is at least as big as the request, but not more than
	 * twice as big */
	for (i = 0; i < pool->totbuf; i++) {
		bufsize = pool->buffers[i]->size;

		/* check for an exact size match */
		if (bufsize == size) {
			bestfit = i;
			break;
		}
		/* smaller buffers won't fit data and buffers at least
		 * twice as big are a waste of memory */
		else if (bufsize > size && size > (bufsize / 2)) {
			/* is it closer to the required size than the
			 * last appropriate buffer found. try to save
			 * memory */
			if (bestfit == -1 || pool->buffers[bestfit]->size > bufsize) {
				bestfit = i;
			}
		}
	}

	/* if an acceptable buffer was found in the pool, remove it
	 * from the pool and return it */
	if (bestfit != -1) {
		buf = pool->buffers[bestfit];
		gpu_buffer_pool_remove_index(pool, bestfit);
		return buf;
	}

	/* no acceptable buffer found in the pool, create a new one */
	buf = MEM_callocN(sizeof(GPUBuffer), "GPUBuffer");
	buf->size = size;

	if (useVBOs == 1) {
		/* create a new VBO and initialize it to the requested
		 * size */
		glGenBuffersARB(1, &buf->id);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buf->id);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, size, NULL, GL_STATIC_DRAW_ARB);
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
	else {
		buf->pointer = MEM_mallocN(size, "GPUBuffer.pointer");
		
		/* purpose of this seems to be dealing with
		 * out-of-memory errors? looks a bit iffy to me
		 * though, at least on Linux I expect malloc() would
		 * just overcommit. --nicholas */
		while (!buf->pointer && pool->totbuf > 0) {
			gpu_buffer_pool_delete_last(pool);
			buf->pointer = MEM_mallocN(size, "GPUBuffer.pointer");
		}
		if (!buf->pointer)
			return NULL;
	}

	return buf;
}

/* Same as above, but safe for threading. */
GPUBuffer *GPU_buffer_alloc(int size)
{
	GPUBuffer *buffer;

	if (size == 0) {
		/* Early out, no lock needed in this case. */
		return NULL;
	}

	BLI_mutex_lock(&buffer_mutex);
	buffer = gpu_buffer_alloc_intern(size);
	BLI_mutex_unlock(&buffer_mutex);

	return buffer;
}

/* release a GPUBuffer; does not free the actual buffer or its data,
 * but rather moves it to the pool of recently-freed buffers for
 * possible re-use
 *
 * Thread-unsafe version for internal usage only.
 */
static void gpu_buffer_free_intern(GPUBuffer *buffer)
{
	GPUBufferPool *pool;
	int i;

	if (!buffer)
		return;

	pool = gpu_get_global_buffer_pool();

	/* free the last used buffer in the queue if no more space, but only
	 * if we are in the main thread. for e.g. rendering or baking it can
	 * happen that we are in other thread and can't call OpenGL, in that
	 * case cleanup will be done GPU_buffer_pool_free_unused */
	if (BLI_thread_is_main()) {
		/* in main thread, safe to decrease size of pool back
		 * down to MAX_FREE_GPU_BUFFERS */
		while (pool->totbuf >= MAX_FREE_GPU_BUFFERS)
			gpu_buffer_pool_delete_last(pool);
	}
	else {
		/* outside of main thread, can't safely delete the
		 * buffer, so increase pool size */
		if (pool->maxsize == pool->totbuf) {
			pool->maxsize += MAX_FREE_GPU_BUFFERS;
			pool->buffers = MEM_reallocN(pool->buffers,
			                             sizeof(GPUBuffer *) * pool->maxsize);
		}
	}

	/* shift pool entries up by one */
	for (i = pool->totbuf; i > 0; i--)
		pool->buffers[i] = pool->buffers[i - 1];

	/* insert the buffer into the beginning of the pool */
	pool->buffers[0] = buffer;
	pool->totbuf++;
}

/* Same as above, but safe for threading. */
void GPU_buffer_free(GPUBuffer *buffer)
{
	if (!buffer) {
		/* Early output, no need to lock in this case, */
		return;
	}

	BLI_mutex_lock(&buffer_mutex);
	gpu_buffer_free_intern(buffer);
	BLI_mutex_unlock(&buffer_mutex);
}

/* currently unused */
// #define USE_GPU_POINT_LINK

typedef struct GPUVertPointLink {
#ifdef USE_GPU_POINT_LINK
	struct GPUVertPointLink *next;
#endif
	/* -1 means uninitialized */
	int point_index;
} GPUVertPointLink;


/* add a new point to the list of points related to a particular
 * vertex */
#ifdef USE_GPU_POINT_LINK

static void gpu_drawobject_add_vert_point(GPUDrawObject *gdo, int vert_index, int point_index)
{
	GPUVertPointLink *lnk;

	lnk = &gdo->vert_points[vert_index];

	/* if first link is in use, add a new link at the end */
	if (lnk->point_index != -1) {
		/* get last link */
		for (; lnk->next; lnk = lnk->next) ;

		/* add a new link from the pool */
		lnk = lnk->next = &gdo->vert_points_mem[gdo->vert_points_usage];
		gdo->vert_points_usage++;
	}

	lnk->point_index = point_index;
}

#else

static void gpu_drawobject_add_vert_point(GPUDrawObject *gdo, int vert_index, int point_index)
{
	GPUVertPointLink *lnk;
	lnk = &gdo->vert_points[vert_index];
	if (lnk->point_index == -1) {
		lnk->point_index = point_index;
	}
}

#endif  /* USE_GPU_POINT_LINK */

/* update the vert_points and triangle_to_mface fields with a new
 * triangle */
static void gpu_drawobject_add_triangle(GPUDrawObject *gdo,
                                        int base_point_index,
                                        int face_index,
                                        int v1, int v2, int v3)
{
	int i, v[3] = {v1, v2, v3};
	for (i = 0; i < 3; i++)
		gpu_drawobject_add_vert_point(gdo, v[i], base_point_index + i);
	gdo->triangle_to_mface[base_point_index / 3] = face_index;
}

/* for each vertex, build a list of points related to it; these lists
 * are stored in an array sized to the number of vertices */
static void gpu_drawobject_init_vert_points(GPUDrawObject *gdo, MFace *f, int totface, int totmat)
{
	GPUBufferMaterial *mat;
	int i, *mat_orig_to_new;

	mat_orig_to_new = MEM_callocN(sizeof(*mat_orig_to_new) * totmat,
	                                             "GPUDrawObject.mat_orig_to_new");
	/* allocate the array and space for links */
	gdo->vert_points = MEM_mallocN(sizeof(GPUVertPointLink) * gdo->totvert,
	                               "GPUDrawObject.vert_points");
#ifdef USE_GPU_POINT_LINK
	gdo->vert_points_mem = MEM_callocN(sizeof(GPUVertPointLink) * gdo->tot_triangle_point,
	                                   "GPUDrawObject.vert_points_mem");
	gdo->vert_points_usage = 0;
#endif

	/* build a map from the original material indices to the new
	 * GPUBufferMaterial indices */
	for (i = 0; i < gdo->totmaterial; i++)
		mat_orig_to_new[gdo->materials[i].mat_nr] = i;

	/* -1 indicates the link is not yet used */
	for (i = 0; i < gdo->totvert; i++) {
#ifdef USE_GPU_POINT_LINK
		gdo->vert_points[i].link = NULL;
#endif
		gdo->vert_points[i].point_index = -1;
	}

	for (i = 0; i < totface; i++, f++) {
		mat = &gdo->materials[mat_orig_to_new[f->mat_nr]];

		/* add triangle */
		gpu_drawobject_add_triangle(gdo, mat->start + mat->totpoint,
		                            i, f->v1, f->v2, f->v3);
		mat->totpoint += 3;

		/* add second triangle for quads */
		if (f->v4) {
			gpu_drawobject_add_triangle(gdo, mat->start + mat->totpoint,
			                            i, f->v3, f->v4, f->v1);
			mat->totpoint += 3;
		}
	}

	/* map any unused vertices to loose points */
	for (i = 0; i < gdo->totvert; i++) {
		if (gdo->vert_points[i].point_index == -1) {
			gdo->vert_points[i].point_index = gdo->tot_triangle_point + gdo->tot_loose_point;
			gdo->tot_loose_point++;
		}
	}

	MEM_freeN(mat_orig_to_new);
}

/* see GPUDrawObject's structure definition for a description of the
 * data being initialized here */
GPUDrawObject *GPU_drawobject_new(DerivedMesh *dm)
{
	GPUDrawObject *gdo;
	MFace *mface;
	int totmat = dm->totmat;
	int *points_per_mat;
	int i, curmat, curpoint, totface;

	/* object contains at least one material (default included) so zero means uninitialized dm */
	BLI_assert(totmat != 0);

	mface = dm->getTessFaceArray(dm);
	totface = dm->getNumTessFaces(dm);

	/* get the number of points used by each material, treating
	 * each quad as two triangles */
	points_per_mat = MEM_callocN(sizeof(*points_per_mat) * totmat, "GPU_drawobject_new.mat_orig_to_new");
	for (i = 0; i < totface; i++)
		points_per_mat[mface[i].mat_nr] += mface[i].v4 ? 6 : 3;

	/* create the GPUDrawObject */
	gdo = MEM_callocN(sizeof(GPUDrawObject), "GPUDrawObject");
	gdo->totvert = dm->getNumVerts(dm);
	gdo->totedge = dm->getNumEdges(dm);

	/* count the number of materials used by this DerivedMesh */
	for (i = 0; i < totmat; i++) {
		if (points_per_mat[i] > 0)
			gdo->totmaterial++;
	}

	/* allocate an array of materials used by this DerivedMesh */
	gdo->materials = MEM_mallocN(sizeof(GPUBufferMaterial) * gdo->totmaterial,
	                             "GPUDrawObject.materials");

	/* initialize the materials array */
	for (i = 0, curmat = 0, curpoint = 0; i < totmat; i++) {
		if (points_per_mat[i] > 0) {
			gdo->materials[curmat].start = curpoint;
			gdo->materials[curmat].totpoint = 0;
			gdo->materials[curmat].mat_nr = i;

			curpoint += points_per_mat[i];
			curmat++;
		}
	}

	/* store total number of points used for triangles */
	gdo->tot_triangle_point = curpoint;

	gdo->triangle_to_mface = MEM_mallocN(sizeof(int) * (gdo->tot_triangle_point / 3),
	                                     "GPUDrawObject.triangle_to_mface");

	gpu_drawobject_init_vert_points(gdo, mface, totface, totmat);
	MEM_freeN(points_per_mat);

	return gdo;
}

void GPU_drawobject_free(DerivedMesh *dm)
{
	GPUDrawObject *gdo;

	if (!dm || !(gdo = dm->drawObject))
		return;

	MEM_freeN(gdo->materials);
	MEM_freeN(gdo->triangle_to_mface);
	MEM_freeN(gdo->vert_points);
#ifdef USE_GPU_POINT_LINK
	MEM_freeN(gdo->vert_points_mem);
#endif
	GPU_buffer_free(gdo->points);
	GPU_buffer_free(gdo->normals);
	GPU_buffer_free(gdo->uv);
	GPU_buffer_free(gdo->colors);
	GPU_buffer_free(gdo->edges);
	GPU_buffer_free(gdo->uvedges);

	MEM_freeN(gdo);
	dm->drawObject = NULL;
}

typedef void (*GPUBufferCopyFunc)(DerivedMesh *dm, float *varray, int *index,
                                  int *mat_orig_to_new, void *user_data);

static GPUBuffer *gpu_buffer_setup(DerivedMesh *dm, GPUDrawObject *object,
                                   int vector_size, int size, GLenum target,
                                   void *user, GPUBufferCopyFunc copy_f)
{
	GPUBufferPool *pool;
	GPUBuffer *buffer;
	float *varray;
	int *mat_orig_to_new;
	int *cur_index_per_mat;
	int i;
	bool success;
	GLboolean uploaded;

	pool = gpu_get_global_buffer_pool();

	BLI_mutex_lock(&buffer_mutex);

	/* alloc a GPUBuffer; fall back to legacy mode on failure */
	if (!(buffer = gpu_buffer_alloc_intern(size)))
		dm->drawObject->legacy = 1;

	/* nothing to do for legacy mode */
	if (dm->drawObject->legacy) {
		BLI_mutex_unlock(&buffer_mutex);
		return NULL;
	}

	mat_orig_to_new = MEM_mallocN(sizeof(*mat_orig_to_new) * dm->totmat,
	                              "GPU_buffer_setup.mat_orig_to_new");
	cur_index_per_mat = MEM_mallocN(sizeof(int) * object->totmaterial,
	                                "GPU_buffer_setup.cur_index_per_mat");
	for (i = 0; i < object->totmaterial; i++) {
		/* for each material, the current index to copy data to */
		cur_index_per_mat[i] = object->materials[i].start * vector_size;

		/* map from original material index to new
		 * GPUBufferMaterial index */
		mat_orig_to_new[object->materials[i].mat_nr] = i;
	}

	if (useVBOs) {
		success = 0;

		while (!success) {
			/* bind the buffer and discard previous data,
			 * avoids stalling gpu */
			glBindBufferARB(target, buffer->id);
			glBufferDataARB(target, buffer->size, NULL, GL_STATIC_DRAW_ARB);

			/* attempt to map the buffer */
			if (!(varray = glMapBufferARB(target, GL_WRITE_ONLY_ARB))) {
				/* failed to map the buffer; delete it */
				gpu_buffer_free_intern(buffer);
				gpu_buffer_pool_delete_last(pool);
				buffer = NULL;

				/* try freeing an entry from the pool
				 * and reallocating the buffer */
				if (pool->totbuf > 0) {
					gpu_buffer_pool_delete_last(pool);
					buffer = gpu_buffer_alloc_intern(size);
				}

				/* allocation still failed; fall back
				 * to legacy mode */
				if (!buffer) {
					dm->drawObject->legacy = 1;
					success = 1;
				}
			}
			else {
				success = 1;
			}
		}

		/* check legacy fallback didn't happen */
		if (dm->drawObject->legacy == 0) {
			uploaded = GL_FALSE;
			/* attempt to upload the data to the VBO */
			while (uploaded == GL_FALSE) {
				(*copy_f)(dm, varray, cur_index_per_mat, mat_orig_to_new, user);
				/* glUnmapBuffer returns GL_FALSE if
				 * the data store is corrupted; retry
				 * in that case */
				uploaded = glUnmapBufferARB(target);
			}
		}
		glBindBufferARB(target, 0);
	}
	else {
		/* VBO not supported, use vertex array fallback */
		if (buffer->pointer) {
			varray = buffer->pointer;
			(*copy_f)(dm, varray, cur_index_per_mat, mat_orig_to_new, user);
		}
		else {
			dm->drawObject->legacy = 1;
		}
	}

	MEM_freeN(cur_index_per_mat);
	MEM_freeN(mat_orig_to_new);

	BLI_mutex_unlock(&buffer_mutex);

	return buffer;
}

static void GPU_buffer_copy_vertex(DerivedMesh *dm, float *varray, int *index, int *mat_orig_to_new, void *UNUSED(user))
{
	MVert *mvert;
	MFace *f;
	int i, j, start, totface;

	mvert = dm->getVertArray(dm);
	f = dm->getTessFaceArray(dm);

	totface = dm->getNumTessFaces(dm);
	for (i = 0; i < totface; i++, f++) {
		start = index[mat_orig_to_new[f->mat_nr]];

		/* v1 v2 v3 */
		copy_v3_v3(&varray[start], mvert[f->v1].co);
		copy_v3_v3(&varray[start + 3], mvert[f->v2].co);
		copy_v3_v3(&varray[start + 6], mvert[f->v3].co);
		index[mat_orig_to_new[f->mat_nr]] += 9;

		if (f->v4) {
			/* v3 v4 v1 */
			copy_v3_v3(&varray[start + 9], mvert[f->v3].co);
			copy_v3_v3(&varray[start + 12], mvert[f->v4].co);
			copy_v3_v3(&varray[start + 15], mvert[f->v1].co);
			index[mat_orig_to_new[f->mat_nr]] += 9;
		}
	}

	/* copy loose points */
	j = dm->drawObject->tot_triangle_point * 3;
	for (i = 0; i < dm->drawObject->totvert; i++) {
		if (dm->drawObject->vert_points[i].point_index >= dm->drawObject->tot_triangle_point) {
			copy_v3_v3(&varray[j], mvert[i].co);
			j += 3;
		}
	}
}

static void GPU_buffer_copy_normal(DerivedMesh *dm, float *varray, int *index, int *mat_orig_to_new, void *UNUSED(user))
{
	int i, totface;
	int start;
	float f_no[3];

	const float *nors = dm->getTessFaceDataArray(dm, CD_NORMAL);
	short (*tlnors)[4][3] = dm->getTessFaceDataArray(dm, CD_TESSLOOPNORMAL);
	MVert *mvert = dm->getVertArray(dm);
	MFace *f = dm->getTessFaceArray(dm);

	totface = dm->getNumTessFaces(dm);
	for (i = 0; i < totface; i++, f++) {
		const int smoothnormal = (f->flag & ME_SMOOTH);

		start = index[mat_orig_to_new[f->mat_nr]];
		index[mat_orig_to_new[f->mat_nr]] += f->v4 ? 18 : 9;

		if (tlnors) {
			short (*tlnor)[3] = tlnors[i];
			/* Copy loop normals */
			normal_short_to_float_v3(&varray[start], tlnor[0]);
			normal_short_to_float_v3(&varray[start + 3], tlnor[1]);
			normal_short_to_float_v3(&varray[start + 6], tlnor[2]);

			if (f->v4) {
				normal_short_to_float_v3(&varray[start + 9], tlnor[2]);
				normal_short_to_float_v3(&varray[start + 12], tlnor[3]);
				normal_short_to_float_v3(&varray[start + 15], tlnor[0]);
			}
		}
		else if (smoothnormal) {
			/* copy vertex normal */
			normal_short_to_float_v3(&varray[start], mvert[f->v1].no);
			normal_short_to_float_v3(&varray[start + 3], mvert[f->v2].no);
			normal_short_to_float_v3(&varray[start + 6], mvert[f->v3].no);

			if (f->v4) {
				normal_short_to_float_v3(&varray[start + 9], mvert[f->v3].no);
				normal_short_to_float_v3(&varray[start + 12], mvert[f->v4].no);
				normal_short_to_float_v3(&varray[start + 15], mvert[f->v1].no);
			}
		}
		else if (nors) {
			/* copy cached face normal */
			copy_v3_v3(&varray[start], &nors[i * 3]);
			copy_v3_v3(&varray[start + 3], &nors[i * 3]);
			copy_v3_v3(&varray[start + 6], &nors[i * 3]);

			if (f->v4) {
				copy_v3_v3(&varray[start + 9], &nors[i * 3]);
				copy_v3_v3(&varray[start + 12], &nors[i * 3]);
				copy_v3_v3(&varray[start + 15], &nors[i * 3]);
			}
		}
		else {
			/* calculate face normal */
			if (f->v4)
				normal_quad_v3(f_no, mvert[f->v1].co, mvert[f->v2].co, mvert[f->v3].co, mvert[f->v4].co);
			else
				normal_tri_v3(f_no, mvert[f->v1].co, mvert[f->v2].co, mvert[f->v3].co);

			copy_v3_v3(&varray[start], f_no);
			copy_v3_v3(&varray[start + 3], f_no);
			copy_v3_v3(&varray[start + 6], f_no);

			if (f->v4) {
				copy_v3_v3(&varray[start + 9], f_no);
				copy_v3_v3(&varray[start + 12], f_no);
				copy_v3_v3(&varray[start + 15], f_no);
			}
		}
	}
}

static void GPU_buffer_copy_uv(DerivedMesh *dm, float *varray, int *index, int *mat_orig_to_new, void *UNUSED(user))
{
	int start;
	int i, totface;

	MTFace *mtface;
	MFace *f;

	if (!(mtface = DM_get_tessface_data_layer(dm, CD_MTFACE)))
		return;
	f = dm->getTessFaceArray(dm);
		
	totface = dm->getNumTessFaces(dm);
	for (i = 0; i < totface; i++, f++) {
		start = index[mat_orig_to_new[f->mat_nr]];

		/* v1 v2 v3 */
		copy_v2_v2(&varray[start], mtface[i].uv[0]);
		copy_v2_v2(&varray[start + 2], mtface[i].uv[1]);
		copy_v2_v2(&varray[start + 4], mtface[i].uv[2]);
		index[mat_orig_to_new[f->mat_nr]] += 6;

		if (f->v4) {
			/* v3 v4 v1 */
			copy_v2_v2(&varray[start + 6], mtface[i].uv[2]);
			copy_v2_v2(&varray[start + 8], mtface[i].uv[3]);
			copy_v2_v2(&varray[start + 10], mtface[i].uv[0]);
			index[mat_orig_to_new[f->mat_nr]] += 6;
		}
	}
}


static void GPU_buffer_copy_uv_texpaint(DerivedMesh *dm, float *varray, int *index, int *mat_orig_to_new, void *UNUSED(user))
{
	int start;
	int i, totface;

	int totmaterial = dm->totmat;
	MTFace **mtface_base;
	MTFace *stencil_base;
	int stencil;
	MFace *mf;

	/* should have been checked for before, reassert */
	BLI_assert(DM_get_tessface_data_layer(dm, CD_MTFACE));
	mf = dm->getTessFaceArray(dm);
	mtface_base = MEM_mallocN(totmaterial * sizeof(*mtface_base), "texslots");

	for (i = 0; i < totmaterial; i++) {
		mtface_base[i] = DM_paint_uvlayer_active_get(dm, i);
	}

	stencil = CustomData_get_stencil_layer(&dm->faceData, CD_MTFACE);
	stencil_base = CustomData_get_layer_n(&dm->faceData, CD_MTFACE, stencil);

	totface = dm->getNumTessFaces(dm);

	for (i = 0; i < totface; i++, mf++) {
		int mat_i = mf->mat_nr;
		start = index[mat_orig_to_new[mat_i]];

		/* v1 v2 v3 */
		copy_v2_v2(&varray[start], mtface_base[mat_i][i].uv[0]);
		copy_v2_v2(&varray[start + 2], stencil_base[i].uv[0]);
		copy_v2_v2(&varray[start + 4], mtface_base[mat_i][i].uv[1]);
		copy_v2_v2(&varray[start + 6], stencil_base[i].uv[1]);
		copy_v2_v2(&varray[start + 8], mtface_base[mat_i][i].uv[2]);
		copy_v2_v2(&varray[start + 10], stencil_base[i].uv[2]);
		index[mat_orig_to_new[mat_i]] += 12;

		if (mf->v4) {
			/* v3 v4 v1 */
			copy_v2_v2(&varray[start + 12], mtface_base[mat_i][i].uv[2]);
			copy_v2_v2(&varray[start + 14], stencil_base[i].uv[2]);
			copy_v2_v2(&varray[start + 16], mtface_base[mat_i][i].uv[3]);
			copy_v2_v2(&varray[start + 18], stencil_base[i].uv[3]);
			copy_v2_v2(&varray[start + 20], mtface_base[mat_i][i].uv[0]);
			copy_v2_v2(&varray[start + 22], stencil_base[i].uv[0]);
			index[mat_orig_to_new[mat_i]] += 12;
		}
	}

	MEM_freeN(mtface_base);
}


static void copy_mcol_uc3(unsigned char *v, unsigned char *col)
{
	v[0] = col[3];
	v[1] = col[2];
	v[2] = col[1];
}

/* treat varray_ as an array of MCol, four MCol's per face */
static void GPU_buffer_copy_mcol(DerivedMesh *dm, float *varray_, int *index, int *mat_orig_to_new, void *user)
{
	int i, totface;
	unsigned char *varray = (unsigned char *)varray_;
	unsigned char *mcol = (unsigned char *)user;
	MFace *f = dm->getTessFaceArray(dm);

	totface = dm->getNumTessFaces(dm);
	for (i = 0; i < totface; i++, f++) {
		int start = index[mat_orig_to_new[f->mat_nr]];

		/* v1 v2 v3 */
		copy_mcol_uc3(&varray[start], &mcol[i * 16]);
		copy_mcol_uc3(&varray[start + 3], &mcol[i * 16 + 4]);
		copy_mcol_uc3(&varray[start + 6], &mcol[i * 16 + 8]);
		index[mat_orig_to_new[f->mat_nr]] += 9;

		if (f->v4) {
			/* v3 v4 v1 */
			copy_mcol_uc3(&varray[start + 9], &mcol[i * 16 + 8]);
			copy_mcol_uc3(&varray[start + 12], &mcol[i * 16 + 12]);
			copy_mcol_uc3(&varray[start + 15], &mcol[i * 16]);
			index[mat_orig_to_new[f->mat_nr]] += 9;
		}
	}
}

static void GPU_buffer_copy_edge(DerivedMesh *dm, float *varray_, int *UNUSED(index), int *UNUSED(mat_orig_to_new), void *UNUSED(user))
{
	MEdge *medge;
	unsigned int *varray = (unsigned int *)varray_;
	int i, totedge;

	medge = dm->getEdgeArray(dm);
	totedge = dm->getNumEdges(dm);

	for (i = 0; i < totedge; i++, medge++) {
		varray[i * 2] = dm->drawObject->vert_points[medge->v1].point_index;
		varray[i * 2 + 1] = dm->drawObject->vert_points[medge->v2].point_index;
	}
}

static void GPU_buffer_copy_uvedge(DerivedMesh *dm, float *varray, int *UNUSED(index), int *UNUSED(mat_orig_to_new), void *UNUSED(user))
{
	MTFace *tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
	int i, j = 0;

	if (!tf)
		return;

	for (i = 0; i < dm->numTessFaceData; i++, tf++) {
		MFace mf;
		dm->getTessFace(dm, i, &mf);

		copy_v2_v2(&varray[j], tf->uv[0]);
		copy_v2_v2(&varray[j + 2], tf->uv[1]);

		copy_v2_v2(&varray[j + 4], tf->uv[1]);
		copy_v2_v2(&varray[j + 6], tf->uv[2]);

		if (!mf.v4) {
			copy_v2_v2(&varray[j + 8], tf->uv[2]);
			copy_v2_v2(&varray[j + 10], tf->uv[0]);
			j += 12;
		}
		else {
			copy_v2_v2(&varray[j + 8], tf->uv[2]);
			copy_v2_v2(&varray[j + 10], tf->uv[3]);

			copy_v2_v2(&varray[j + 12], tf->uv[3]);
			copy_v2_v2(&varray[j + 14], tf->uv[0]);
			j += 16;
		}
	}
}

typedef enum {
	GPU_BUFFER_VERTEX = 0,
	GPU_BUFFER_NORMAL,
	GPU_BUFFER_COLOR,
	GPU_BUFFER_UV,
	GPU_BUFFER_UV_TEXPAINT,
	GPU_BUFFER_EDGE,
	GPU_BUFFER_UVEDGE,
} GPUBufferType;

typedef struct {
	GPUBufferCopyFunc copy;
	GLenum gl_buffer_type;
	int vector_size;
} GPUBufferTypeSettings;

const GPUBufferTypeSettings gpu_buffer_type_settings[] = {
	{GPU_buffer_copy_vertex, GL_ARRAY_BUFFER_ARB, 3},
	{GPU_buffer_copy_normal, GL_ARRAY_BUFFER_ARB, 3},
	{GPU_buffer_copy_mcol, GL_ARRAY_BUFFER_ARB, 3},
	{GPU_buffer_copy_uv, GL_ARRAY_BUFFER_ARB, 2},
    {GPU_buffer_copy_uv_texpaint, GL_ARRAY_BUFFER_ARB, 4},
	{GPU_buffer_copy_edge, GL_ELEMENT_ARRAY_BUFFER_ARB, 2},
	{GPU_buffer_copy_uvedge, GL_ELEMENT_ARRAY_BUFFER_ARB, 4}
};

/* get the GPUDrawObject buffer associated with a type */
static GPUBuffer **gpu_drawobject_buffer_from_type(GPUDrawObject *gdo, GPUBufferType type)
{
	switch (type) {
		case GPU_BUFFER_VERTEX:
			return &gdo->points;
		case GPU_BUFFER_NORMAL:
			return &gdo->normals;
		case GPU_BUFFER_COLOR:
			return &gdo->colors;
		case GPU_BUFFER_UV:
			return &gdo->uv;
		case GPU_BUFFER_UV_TEXPAINT:
			return &gdo->uv;
		case GPU_BUFFER_EDGE:
			return &gdo->edges;
		case GPU_BUFFER_UVEDGE:
			return &gdo->uvedges;
		default:
			return NULL;
	}
}

/* get the amount of space to allocate for a buffer of a particular type */
static int gpu_buffer_size_from_type(DerivedMesh *dm, GPUBufferType type)
{
	switch (type) {
		case GPU_BUFFER_VERTEX:
			return sizeof(float) * 3 * (dm->drawObject->tot_triangle_point + dm->drawObject->tot_loose_point);
		case GPU_BUFFER_NORMAL:
			return sizeof(float) * 3 * dm->drawObject->tot_triangle_point;
		case GPU_BUFFER_COLOR:
			return sizeof(char) * 3 * dm->drawObject->tot_triangle_point;
		case GPU_BUFFER_UV:
			return sizeof(float) * 2 * dm->drawObject->tot_triangle_point;
		case GPU_BUFFER_UV_TEXPAINT:
			return sizeof(float) * 4 * dm->drawObject->tot_triangle_point;
		case GPU_BUFFER_EDGE:
			return sizeof(int) * 2 * dm->drawObject->totedge;
		case GPU_BUFFER_UVEDGE:
			/* each face gets 3 points, 3 edges per triangle, and
			 * each edge has its own, non-shared coords, so each
			 * tri corner needs minimum of 4 floats, quads used
			 * less so here we can over allocate and assume all
			 * tris. */
			return sizeof(float) * 4 * dm->drawObject->tot_triangle_point;
		default:
			return -1;
	}
}

/* call gpu_buffer_setup with settings for a particular type of buffer */
static GPUBuffer *gpu_buffer_setup_type(DerivedMesh *dm, GPUBufferType type)
{
	const GPUBufferTypeSettings *ts;
	void *user_data = NULL;
	GPUBuffer *buf;

	ts = &gpu_buffer_type_settings[type];

	/* special handling for MCol and UV buffers */
	if (type == GPU_BUFFER_COLOR) {
		if (!(user_data = DM_get_tessface_data_layer(dm, dm->drawObject->colType)))
			return NULL;
	}
	else if (ELEM(type, GPU_BUFFER_UV, GPU_BUFFER_UV_TEXPAINT)) {
		if (!DM_get_tessface_data_layer(dm, CD_MTFACE))
			return NULL;
	}

	buf = gpu_buffer_setup(dm, dm->drawObject, ts->vector_size,
	                       gpu_buffer_size_from_type(dm, type),
	                       ts->gl_buffer_type, user_data, ts->copy);

	return buf;
}

/* get the buffer of `type', initializing the GPUDrawObject and
 * buffer if needed */
static GPUBuffer *gpu_buffer_setup_common(DerivedMesh *dm, GPUBufferType type)
{
	GPUBuffer **buf;

	if (!dm->drawObject)
		dm->drawObject = GPU_drawobject_new(dm);

	buf = gpu_drawobject_buffer_from_type(dm->drawObject, type);
	if (!(*buf))
		*buf = gpu_buffer_setup_type(dm, type);

	return *buf;
}

void GPU_vertex_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_VERTEX))
		return;

	glEnableClientState(GL_VERTEX_ARRAY);
	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, dm->drawObject->points->id);
		glVertexPointer(3, GL_FLOAT, 0, 0);
	}
	else {
		glVertexPointer(3, GL_FLOAT, 0, dm->drawObject->points->pointer);
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

void GPU_normal_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_NORMAL))
		return;

	glEnableClientState(GL_NORMAL_ARRAY);
	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, dm->drawObject->normals->id);
		glNormalPointer(GL_FLOAT, 0, 0);
	}
	else {
		glNormalPointer(GL_FLOAT, 0, dm->drawObject->normals->pointer);
	}

	GLStates |= GPU_BUFFER_NORMAL_STATE;
}

void GPU_uv_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_UV))
		return;

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, dm->drawObject->uv->id);
		glTexCoordPointer(2, GL_FLOAT, 0, 0);
	}
	else {
		glTexCoordPointer(2, GL_FLOAT, 0, dm->drawObject->uv->pointer);
	}

	GLStates |= GPU_BUFFER_TEXCOORD_UNIT_0_STATE;
}

void GPU_texpaint_uv_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_UV_TEXPAINT))
		return;

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, dm->drawObject->uv->id);
		glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), 0);
		glClientActiveTexture(GL_TEXTURE1);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), BUFFER_OFFSET(2 * sizeof(float)));
		glClientActiveTexture(GL_TEXTURE0);
	}
	else {
		glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), dm->drawObject->uv->pointer);
		glClientActiveTexture(GL_TEXTURE1);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), (char *)dm->drawObject->uv->pointer + 2 * sizeof(float));
		glClientActiveTexture(GL_TEXTURE0);
	}

	GLStates |= GPU_BUFFER_TEXCOORD_UNIT_0_STATE | GPU_BUFFER_TEXCOORD_UNIT_1_STATE;
}


void GPU_color_setup(DerivedMesh *dm, int colType)
{
	if (!dm->drawObject) {
		/* XXX Not really nice, but we need a valid gpu draw object to set the colType...
		 *     Else we would have to add a new param to gpu_buffer_setup_common. */
		dm->drawObject = GPU_drawobject_new(dm);
		dm->dirty &= ~DM_DIRTY_MCOL_UPDATE_DRAW;
		dm->drawObject->colType = colType;
	}
	/* In paint mode, dm may stay the same during stroke, however we still want to update colors!
	 * Also check in case we changed color type (i.e. which MCol cdlayer we use). */
	else if ((dm->dirty & DM_DIRTY_MCOL_UPDATE_DRAW) || (colType != dm->drawObject->colType)) {
		GPUBuffer **buf = gpu_drawobject_buffer_from_type(dm->drawObject, GPU_BUFFER_COLOR);
		/* XXX Freeing this buffer is a bit stupid, as geometry has not changed, size should remain the same.
		 *     Not sure though it would be worth defining a sort of gpu_buffer_update func - nor whether
		 *     it is even possible ! */
		GPU_buffer_free(*buf);
		*buf = NULL;
		dm->dirty &= ~DM_DIRTY_MCOL_UPDATE_DRAW;
		dm->drawObject->colType = colType;
	}

	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_COLOR))
		return;

	glEnableClientState(GL_COLOR_ARRAY);
	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, dm->drawObject->colors->id);
		glColorPointer(3, GL_UNSIGNED_BYTE, 0, 0);
	}
	else {
		glColorPointer(3, GL_UNSIGNED_BYTE, 0, dm->drawObject->colors->pointer);
	}

	GLStates |= GPU_BUFFER_COLOR_STATE;
}

void GPU_edge_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_EDGE))
		return;

	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_VERTEX))
		return;

	glEnableClientState(GL_VERTEX_ARRAY);
	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, dm->drawObject->points->id);
		glVertexPointer(3, GL_FLOAT, 0, 0);
	}
	else {
		glVertexPointer(3, GL_FLOAT, 0, dm->drawObject->points->pointer);
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;

	if (useVBOs)
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, dm->drawObject->edges->id);

	GLStates |= GPU_BUFFER_ELEMENT_STATE;
}

void GPU_uvedge_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_UVEDGE))
		return;

	glEnableClientState(GL_VERTEX_ARRAY);
	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, dm->drawObject->uvedges->id);
		glVertexPointer(2, GL_FLOAT, 0, 0);
	}
	else {
		glVertexPointer(2, GL_FLOAT, 0, dm->drawObject->uvedges->pointer);
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

static int GPU_typesize(int type)
{
	switch (type) {
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

int GPU_attrib_element_size(GPUAttrib data[], int numdata)
{
	int i, elementsize = 0;

	for (i = 0; i < numdata; i++) {
		int typesize = GPU_typesize(data[i].type);
		if (typesize != 0)
			elementsize += typesize * data[i].size;
	}
	return elementsize;
}

void GPU_interleaved_attrib_setup(GPUBuffer *buffer, GPUAttrib data[], int numdata)
{
	int i;
	int elementsize;
	intptr_t offset = 0;

	for (i = 0; i < MAX_GPU_ATTRIB_DATA; i++) {
		if (attribData[i].index != -1) {
			glDisableVertexAttribArrayARB(attribData[i].index);
		}
		else
			break;
	}
	elementsize = GPU_attrib_element_size(data, numdata);

	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer->id);
		for (i = 0; i < numdata; i++) {
			glEnableVertexAttribArrayARB(data[i].index);
			glVertexAttribPointerARB(data[i].index, data[i].size, data[i].type,
			                         GL_FALSE, elementsize, (void *)offset);
			offset += data[i].size * GPU_typesize(data[i].type);

			attribData[i].index = data[i].index;
			attribData[i].size = data[i].size;
			attribData[i].type = data[i].type;
		}
		attribData[numdata].index = -1;
	}
	else {
		for (i = 0; i < numdata; i++) {
			glEnableVertexAttribArrayARB(data[i].index);
			glVertexAttribPointerARB(data[i].index, data[i].size, data[i].type,
			                         GL_FALSE, elementsize, (char *)buffer->pointer + offset);
			offset += data[i].size * GPU_typesize(data[i].type);
		}
	}
}


void GPU_buffer_unbind(void)
{
	int i;

	if (GLStates & GPU_BUFFER_VERTEX_STATE)
		glDisableClientState(GL_VERTEX_ARRAY);
	if (GLStates & GPU_BUFFER_NORMAL_STATE)
		glDisableClientState(GL_NORMAL_ARRAY);
	if (GLStates & GPU_BUFFER_TEXCOORD_UNIT_0_STATE)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (GLStates & GPU_BUFFER_TEXCOORD_UNIT_1_STATE) {
		glClientActiveTexture(GL_TEXTURE1);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTexture(GL_TEXTURE0);
	}
	if (GLStates & GPU_BUFFER_COLOR_STATE)
		glDisableClientState(GL_COLOR_ARRAY);
	if (GLStates & GPU_BUFFER_ELEMENT_STATE) {
		if (useVBOs) {
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		}
	}
	GLStates &= ~(GPU_BUFFER_VERTEX_STATE | GPU_BUFFER_NORMAL_STATE |
	              GPU_BUFFER_TEXCOORD_UNIT_0_STATE | GPU_BUFFER_TEXCOORD_UNIT_1_STATE |
	              GPU_BUFFER_COLOR_STATE | GPU_BUFFER_ELEMENT_STATE);

	for (i = 0; i < MAX_GPU_ATTRIB_DATA; i++) {
		if (attribData[i].index != -1) {
			glDisableVertexAttribArrayARB(attribData[i].index);
		}
		else
			break;
	}

	if (useVBOs)
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void GPU_color_switch(int mode)
{
	if (mode) {
		if (!(GLStates & GPU_BUFFER_COLOR_STATE))
			glEnableClientState(GL_COLOR_ARRAY);
		GLStates |= GPU_BUFFER_COLOR_STATE;
	}
	else {
		if (GLStates & GPU_BUFFER_COLOR_STATE)
			glDisableClientState(GL_COLOR_ARRAY);
		GLStates &= ~GPU_BUFFER_COLOR_STATE;
	}
}

/* return 1 if drawing should be done using old immediate-mode
 * code, 0 otherwise */
bool GPU_buffer_legacy(DerivedMesh *dm)
{
	int test = (U.gameflags & USER_DISABLE_VBO);
	if (test)
		return 1;

	if (dm->drawObject == NULL)
		dm->drawObject = GPU_drawobject_new(dm);
	return dm->drawObject->legacy;
}

void *GPU_buffer_lock(GPUBuffer *buffer)
{
	float *varray;

	if (!buffer)
		return 0;

	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer->id);
		varray = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		return varray;
	}
	else {
		return buffer->pointer;
	}
}

void *GPU_buffer_lock_stream(GPUBuffer *buffer)
{
	float *varray;

	if (!buffer)
		return 0;

	if (useVBOs) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer->id);
		/* discard previous data, avoid stalling gpu */
		glBufferDataARB(GL_ARRAY_BUFFER_ARB, buffer->size, 0, GL_STREAM_DRAW_ARB);
		varray = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		return varray;
	}
	else {
		return buffer->pointer;
	}
}

void GPU_buffer_unlock(GPUBuffer *buffer)
{
	if (useVBOs) {
		if (buffer) {
			/* note: this operation can fail, could return
			 * an error code from this function? */
			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
		}
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
}

/* used for drawing edges */
void GPU_buffer_draw_elements(GPUBuffer *elements, unsigned int mode, int start, int count)
{
	glDrawElements(mode, count, GL_UNSIGNED_INT,
	               (useVBOs ?
	                (void *)(start * sizeof(unsigned int)) :
	                ((int *)elements->pointer) + start));
}


/* XXX: the rest of the code in this file is used for optimized PBVH
 * drawing and doesn't interact at all with the buffer code above */

/* Return false if VBO is either unavailable or disabled by the user,
 * true otherwise */
static int gpu_vbo_enabled(void)
{
	return (GLEW_ARB_vertex_buffer_object &&
	        !(U.gameflags & USER_DISABLE_VBO));
}

/* Convenience struct for building the VBO. */
typedef struct {
	float co[3];
	short no[3];

	/* inserting this to align the 'color' field to a four-byte
	 * boundary; drastically increases viewport performance on my
	 * drivers (Gallium/Radeon) --nicholasbishop */
	char pad[2];
	
	unsigned char color[3];
} VertexBufferFormat;

struct GPU_PBVH_Buffers {
	/* opengl buffer handles */
	GLuint vert_buf, index_buf;
	GLenum index_type;

	/* mesh pointers in case buffer allocation fails */
	MFace *mface;
	MVert *mvert;
	const int *face_indices;
	int totface;
	const float *vmask;

	/* grid pointers */
	CCGKey gridkey;
	CCGElem **grids;
	const DMFlagMat *grid_flag_mats;
	BLI_bitmap * const *grid_hidden;
	const int *grid_indices;
	int totgrid;
	int has_hidden;

	int use_bmesh;

	unsigned int tot_tri, tot_quad;

	/* The PBVH ensures that either all faces in the node are
	 * smooth-shaded or all faces are flat-shaded */
	int smooth;

	bool show_diffuse_color;
	bool use_matcaps;
	float diffuse_color[4];
};
typedef enum {
	VBO_ENABLED,
	VBO_DISABLED
} VBO_State;

static void gpu_colors_enable(VBO_State vbo_state)
{
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);
	if (vbo_state == VBO_ENABLED)
		glEnableClientState(GL_COLOR_ARRAY);
}

static void gpu_colors_disable(VBO_State vbo_state)
{
	glDisable(GL_COLOR_MATERIAL);
	if (vbo_state == VBO_ENABLED)
		glDisableClientState(GL_COLOR_ARRAY);
}

static float gpu_color_from_mask(float mask)
{
	return 1.0f - mask * 0.75f;
}

static void gpu_color_from_mask_copy(float mask, const float diffuse_color[4], unsigned char out[3])
{
	float mask_color;

	mask_color = gpu_color_from_mask(mask) * 255.0f;

	out[0] = diffuse_color[0] * mask_color;
	out[1] = diffuse_color[1] * mask_color;
	out[2] = diffuse_color[2] * mask_color;
}

static void gpu_color_from_mask_set(float mask, float diffuse_color[4])
{
	float color = gpu_color_from_mask(mask);
	glColor3f(diffuse_color[0] * color, diffuse_color[1] * color, diffuse_color[2] * color);
}

static float gpu_color_from_mask_quad(const CCGKey *key,
                                      CCGElem *a, CCGElem *b,
                                      CCGElem *c, CCGElem *d)
{
	return gpu_color_from_mask((*CCG_elem_mask(key, a) +
	                            *CCG_elem_mask(key, b) +
	                            *CCG_elem_mask(key, c) +
	                            *CCG_elem_mask(key, d)) * 0.25f);
}

static void gpu_color_from_mask_quad_copy(const CCGKey *key,
                                          CCGElem *a, CCGElem *b,
                                          CCGElem *c, CCGElem *d,
                                          const float *diffuse_color,
                                          unsigned char out[3])
{
	float mask_color =
	    gpu_color_from_mask((*CCG_elem_mask(key, a) +
	                         *CCG_elem_mask(key, b) +
	                         *CCG_elem_mask(key, c) +
	                         *CCG_elem_mask(key, d)) * 0.25f) * 255.0f;

	out[0] = diffuse_color[0] * mask_color;
	out[1] = diffuse_color[1] * mask_color;
	out[2] = diffuse_color[2] * mask_color;
}

static void gpu_color_from_mask_quad_set(const CCGKey *key,
                                         CCGElem *a, CCGElem *b,
                                         CCGElem *c, CCGElem *d,
                                         const float diffuse_color[4])
{
	float color = gpu_color_from_mask_quad(key, a, b, c, d);
	glColor3f(diffuse_color[0] * color, diffuse_color[1] * color, diffuse_color[2] * color);
}

void GPU_update_mesh_pbvh_buffers(GPU_PBVH_Buffers *buffers, MVert *mvert,
                             int *vert_indices, int totvert, const float *vmask,
                             int (*face_vert_indices)[4], bool show_diffuse_color)
{
	VertexBufferFormat *vert_data;
	int i, j, k;

	buffers->vmask = vmask;
	buffers->show_diffuse_color = show_diffuse_color;
	buffers->use_matcaps = GPU_material_use_matcaps_get();

	if (buffers->vert_buf) {
		int totelem = (buffers->smooth ? totvert : (buffers->tot_tri * 3));
		float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 0.8f};

		if (buffers->use_matcaps)
			diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
		else if (show_diffuse_color) {
			MFace *f = buffers->mface + buffers->face_indices[0];

			GPU_material_diffuse_get(f->mat_nr + 1, diffuse_color);
		}

		copy_v4_v4(buffers->diffuse_color, diffuse_color);

		/* Build VBO */
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
						sizeof(VertexBufferFormat) * totelem,
						NULL, GL_STATIC_DRAW_ARB);

		vert_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

		if (vert_data) {
			/* Vertex data is shared if smooth-shaded, but separate
			 * copies are made for flat shading because normals
			 * shouldn't be shared. */
			if (buffers->smooth) {
				for (i = 0; i < totvert; ++i) {
					MVert *v = mvert + vert_indices[i];
					VertexBufferFormat *out = vert_data + i;

					copy_v3_v3(out->co, v->co);
					memcpy(out->no, v->no, sizeof(short) * 3);
				}

#define UPDATE_VERTEX(face, vertex, index, diffuse_color) \
				{ \
					VertexBufferFormat *out = vert_data + face_vert_indices[face][index]; \
					if (vmask) \
						gpu_color_from_mask_copy(vmask[vertex], diffuse_color, out->color); \
					else \
						rgb_float_to_uchar(out->color, diffuse_color); \
				} (void)0

				for (i = 0; i < buffers->totface; i++) {
					MFace *f = buffers->mface + buffers->face_indices[i];

					UPDATE_VERTEX(i, f->v1, 0, diffuse_color);
					UPDATE_VERTEX(i, f->v2, 1, diffuse_color);
					UPDATE_VERTEX(i, f->v3, 2, diffuse_color);
					if (f->v4)
						UPDATE_VERTEX(i, f->v4, 3, diffuse_color);
				}
#undef UPDATE_VERTEX
			}
			else {
				for (i = 0; i < buffers->totface; ++i) {
					const MFace *f = &buffers->mface[buffers->face_indices[i]];
					const unsigned int *fv = &f->v1;
					const int vi[2][3] = {{0, 1, 2}, {3, 0, 2}};
					float fno[3];
					short no[3];

					float fmask;

					if (paint_is_face_hidden(f, mvert))
						continue;

					/* Face normal and mask */
					if (f->v4) {
						normal_quad_v3(fno,
									   mvert[fv[0]].co,
									   mvert[fv[1]].co,
									   mvert[fv[2]].co,
									   mvert[fv[3]].co);
						if (vmask) {
							fmask = (vmask[fv[0]] +
									 vmask[fv[1]] +
									 vmask[fv[2]] +
									 vmask[fv[3]]) * 0.25f;
						}
					}
					else {
						normal_tri_v3(fno,
									  mvert[fv[0]].co,
									  mvert[fv[1]].co,
									  mvert[fv[2]].co);
						if (vmask) {
							fmask = (vmask[fv[0]] +
									 vmask[fv[1]] +
									 vmask[fv[2]]) / 3.0f;
						}
					}
					normal_float_to_short_v3(no, fno);

					for (j = 0; j < (f->v4 ? 2 : 1); j++) {
						for (k = 0; k < 3; k++) {
							const MVert *v = &mvert[fv[vi[j][k]]];
							VertexBufferFormat *out = vert_data;

							copy_v3_v3(out->co, v->co);
							memcpy(out->no, no, sizeof(short) * 3);

							if (vmask)
								gpu_color_from_mask_copy(fmask, diffuse_color, out->color);
							else
								rgb_float_to_uchar(out->color, diffuse_color);

							vert_data++;
						}
					}
				}
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

GPU_PBVH_Buffers *GPU_build_mesh_pbvh_buffers(int (*face_vert_indices)[4],
                                    MFace *mface, MVert *mvert,
                                    int *face_indices,
                                    int totface)
{
	GPU_PBVH_Buffers *buffers;
	unsigned short *tri_data;
	int i, j, k, tottri;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->index_type = GL_UNSIGNED_SHORT;
	buffers->smooth = mface[face_indices[0]].flag & ME_SMOOTH;

	buffers->show_diffuse_color = false;
	buffers->use_matcaps = false;

	/* Count the number of visible triangles */
	for (i = 0, tottri = 0; i < totface; ++i) {
		const MFace *f = &mface[face_indices[i]];
		if (!paint_is_face_hidden(f, mvert))
			tottri += f->v4 ? 2 : 1;
	}

	if (tottri == 0) {
		buffers->tot_tri = 0;

		buffers->mface = mface;
		buffers->face_indices = face_indices;
		buffers->totface = 0;

		return buffers;
	}

	/* An element index buffer is used for smooth shading, but flat
	 * shading requires separate vertex normals so an index buffer is
	 * can't be used there. */
	if (gpu_vbo_enabled() && buffers->smooth)
		glGenBuffersARB(1, &buffers->index_buf);

	if (buffers->index_buf) {
		/* Generate index buffer object */
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
		                sizeof(unsigned short) * tottri * 3, NULL, GL_STATIC_DRAW_ARB);

		/* Fill the triangle buffer */
		tri_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if (tri_data) {
			for (i = 0; i < totface; ++i) {
				const MFace *f = mface + face_indices[i];
				int v[3];

				/* Skip hidden faces */
				if (paint_is_face_hidden(f, mvert))
					continue;

				v[0] = 0;
				v[1] = 1;
				v[2] = 2;

				for (j = 0; j < (f->v4 ? 2 : 1); ++j) {
					for (k = 0; k < 3; ++k) {
						*tri_data = face_vert_indices[i][v[k]];
						tri_data++;
					}
					v[0] = 3;
					v[1] = 0;
					v[2] = 2;
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

	if (gpu_vbo_enabled() && (buffers->index_buf || !buffers->smooth))
		glGenBuffersARB(1, &buffers->vert_buf);

	buffers->tot_tri = tottri;

	buffers->mface = mface;
	buffers->face_indices = face_indices;
	buffers->totface = totface;

	return buffers;
}

void GPU_update_grid_pbvh_buffers(GPU_PBVH_Buffers *buffers, CCGElem **grids,
                             const DMFlagMat *grid_flag_mats, int *grid_indices,
                             int totgrid, const CCGKey *key, bool show_diffuse_color)
{
	VertexBufferFormat *vert_data;
	int i, j, k, x, y;

	buffers->show_diffuse_color = show_diffuse_color;
	buffers->use_matcaps = GPU_material_use_matcaps_get();

	/* Build VBO */
	if (buffers->vert_buf) {
		int totvert = key->grid_area * totgrid;
		int smooth = grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH;
		const int has_mask = key->has_mask;
		float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

		if (buffers->use_matcaps)
			diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
		else if (show_diffuse_color) {
			const DMFlagMat *flags = &grid_flag_mats[grid_indices[0]];

			GPU_material_diffuse_get(flags->mat_nr + 1, diffuse_color);
		}

		copy_v4_v4(buffers->diffuse_color, diffuse_color);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
		                sizeof(VertexBufferFormat) * totvert,
		                NULL, GL_STATIC_DRAW_ARB);
		vert_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if (vert_data) {
			for (i = 0; i < totgrid; ++i) {
				VertexBufferFormat *vd = vert_data;
				CCGElem *grid = grids[grid_indices[i]];

				for (y = 0; y < key->grid_size; y++) {
					for (x = 0; x < key->grid_size; x++) {
						CCGElem *elem = CCG_grid_elem(key, grid, x, y);
						
						copy_v3_v3(vd->co, CCG_elem_co(key, elem));
						if (smooth) {
							normal_float_to_short_v3(vd->no, CCG_elem_no(key, elem));

							if (has_mask) {
								gpu_color_from_mask_copy(*CCG_elem_mask(key, elem),
								                         diffuse_color, vd->color);
							}
						}
						vd++;
					}
				}
				
				if (!smooth) {
					/* for flat shading, recalc normals and set the last vertex of
					 * each triangle in the index buffer to have the flat normal as
					 * that is what opengl will use */
					for (j = 0; j < key->grid_size - 1; j++) {
						for (k = 0; k < key->grid_size - 1; k++) {
							CCGElem *elems[4] = {
								CCG_grid_elem(key, grid, k, j + 1),
								CCG_grid_elem(key, grid, k + 1, j + 1),
								CCG_grid_elem(key, grid, k + 1, j),
								CCG_grid_elem(key, grid, k, j)
							};
							float fno[3];

							normal_quad_v3(fno,
							               CCG_elem_co(key, elems[0]),
							               CCG_elem_co(key, elems[1]),
							               CCG_elem_co(key, elems[2]),
							               CCG_elem_co(key, elems[3]));

							vd = vert_data + (j + 1) * key->grid_size + k;
							normal_float_to_short_v3(vd->no, fno);

							if (has_mask) {
								gpu_color_from_mask_quad_copy(key,
								                              elems[0],
								                              elems[1],
								                              elems[2],
								                              elems[3],
								                              diffuse_color,
								                              vd->color);
							}
						}
					}
				}

				vert_data += key->grid_area;
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
	buffers->grid_flag_mats = grid_flag_mats;
	buffers->gridkey = *key;

	buffers->smooth = grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH;

	//printf("node updated %p\n", buffers);
}

/* Build the element array buffer of grid indices using either
 * unsigned shorts or unsigned ints. */
#define FILL_QUAD_BUFFER(type_, tot_quad_, buffer_)                     \
	{                                                                   \
		type_ *tri_data;                                               \
		int offset = 0;                                                 \
		int i, j, k;                                                    \
		                                                                \
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,                    \
						sizeof(type_) * (tot_quad_) * 6, NULL,          \
		                GL_STATIC_DRAW_ARB);                            \
		                                                                \
		/* Fill the buffer */                                      \
		tri_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,         \
		                           GL_WRITE_ONLY_ARB);                  \
		if (tri_data) {                                                \
			for (i = 0; i < totgrid; ++i) {                             \
				BLI_bitmap *gh = NULL;                                  \
				if (grid_hidden)                                        \
					gh = grid_hidden[(grid_indices)[i]];                \
																		\
				for (j = 0; j < gridsize - 1; ++j) {                    \
					for (k = 0; k < gridsize - 1; ++k) {                \
						/* Skip hidden grid face */                     \
						if (gh &&                                       \
						    paint_is_grid_face_hidden(gh,               \
						                              gridsize, k, j))  \
							continue;                                   \
																		\
						*(tri_data++) = offset + j * gridsize + k + 1; \
						*(tri_data++) = offset + j * gridsize + k;     \
						*(tri_data++) = offset + (j + 1) * gridsize + k; \
																		\
						*(tri_data++) = offset + (j + 1) * gridsize + k + 1; \
						*(tri_data++) = offset + j * gridsize + k + 1; \
						*(tri_data++) = offset + (j + 1) * gridsize + k; \
					}                                                   \
				}                                                       \
																		\
				offset += gridsize * gridsize;                          \
			}                                                           \
			glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);              \
		}                                                               \
		else {                                                          \
			glDeleteBuffersARB(1, &(buffer_));                          \
			(buffer_) = 0;                                              \
		}                                                               \
	} (void)0
/* end FILL_QUAD_BUFFER */

static GLuint gpu_get_grid_buffer(int gridsize, GLenum *index_type, unsigned *totquad)
{
	static int prev_gridsize = -1;
	static GLenum prev_index_type = 0;
	static GLuint buffer = 0;
	static unsigned prev_totquad;

	/* used in the FILL_QUAD_BUFFER macro */
	BLI_bitmap * const *grid_hidden = NULL;
	const int *grid_indices = NULL;
	int totgrid = 1;

	/* VBO is disabled; delete the previous buffer (if it exists) and
	 * return an invalid handle */
	if (!gpu_vbo_enabled()) {
		if (buffer)
			glDeleteBuffersARB(1, &buffer);
		return 0;
	}

	/* VBO is already built */
	if (buffer && prev_gridsize == gridsize) {
		*index_type = prev_index_type;
		*totquad = prev_totquad;
		return buffer;
	}

	/* Build new VBO */
	glGenBuffersARB(1, &buffer);
	if (buffer) {
		*totquad = (gridsize - 1) * (gridsize - 1);

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);

		if (gridsize * gridsize < USHRT_MAX) {
			*index_type = GL_UNSIGNED_SHORT;
			FILL_QUAD_BUFFER(unsigned short, *totquad, buffer);
		}
		else {
			*index_type = GL_UNSIGNED_INT;
			FILL_QUAD_BUFFER(unsigned int, *totquad, buffer);
		}

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	prev_gridsize = gridsize;
	prev_index_type = *index_type;
	prev_totquad = *totquad;
	return buffer;
}

GPU_PBVH_Buffers *GPU_build_grid_pbvh_buffers(int *grid_indices, int totgrid,
											  BLI_bitmap **grid_hidden, int gridsize)
{
	GPU_PBVH_Buffers *buffers;
	int totquad;
	int fully_visible_totquad = (gridsize - 1) * (gridsize - 1) * totgrid;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->grid_hidden = grid_hidden;
	buffers->totgrid = totgrid;

	buffers->show_diffuse_color = false;
	buffers->use_matcaps = false;

	/* Count the number of quads */
	totquad = BKE_pbvh_count_grid_quads(grid_hidden, grid_indices, totgrid, gridsize);

	/* totally hidden node, return here to avoid BufferData with zero below. */
	if (totquad == 0)
		return buffers;

	if (totquad == fully_visible_totquad) {
		buffers->index_buf = gpu_get_grid_buffer(gridsize, &buffers->index_type, &buffers->tot_quad);
		buffers->has_hidden = 0;
	}
	else if (GLEW_ARB_vertex_buffer_object && !(U.gameflags & USER_DISABLE_VBO)) {
		/* Build new VBO */
		glGenBuffersARB(1, &buffers->index_buf);
		if (buffers->index_buf) {
			buffers->tot_quad = totquad;

			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);

			if (totgrid * gridsize * gridsize < USHRT_MAX) {
				buffers->index_type = GL_UNSIGNED_SHORT;
				FILL_QUAD_BUFFER(unsigned short, totquad, buffers->index_buf);
			}
			else {
				buffers->index_type = GL_UNSIGNED_INT;
				FILL_QUAD_BUFFER(unsigned int, totquad, buffers->index_buf);
			}

			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		}

		buffers->has_hidden = 1;
	}

	/* Build coord/normal VBO */
	if (buffers->index_buf)
		glGenBuffersARB(1, &buffers->vert_buf);

	return buffers;
}

#undef FILL_QUAD_BUFFER

/* Output a BMVert into a VertexBufferFormat array
 *
 * The vertex is skipped if hidden, otherwise the output goes into
 * index '*v_index' in the 'vert_data' array and '*v_index' is
 * incremented.
 */
static void gpu_bmesh_vert_to_buffer_copy(BMVert *v,
                                          VertexBufferFormat *vert_data,
                                          int *v_index,
                                          const float fno[3],
                                          const float *fmask,
                                          const int cd_vert_mask_offset,
                                          const float diffuse_color[4])
{
	if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
		VertexBufferFormat *vd = &vert_data[*v_index];

		/* Set coord, normal, and mask */
		copy_v3_v3(vd->co, v->co);
		normal_float_to_short_v3(vd->no, fno ? fno : v->no);

		gpu_color_from_mask_copy(
		        fmask ? *fmask :
		                BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset),
		        diffuse_color,
		        vd->color);
		

		/* Assign index for use in the triangle index buffer */
		/* note: caller must set:  bm->elem_index_dirty |= BM_VERT; */
		BM_elem_index_set(v, (*v_index)); /* set_dirty! */

		(*v_index)++;
	}
}

/* Return the total number of vertices that don't have BM_ELEM_HIDDEN set */
static int gpu_bmesh_vert_visible_count(GSet *bm_unique_verts,
                                        GSet *bm_other_verts)
{
	GSetIterator gs_iter;
	int totvert = 0;

	GSET_ITER (gs_iter, bm_unique_verts) {
		BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			totvert++;
	}
	GSET_ITER (gs_iter, bm_other_verts) {
		BMVert *v = BLI_gsetIterator_getKey(&gs_iter);
		if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN))
			totvert++;
	}

	return totvert;
}

/* Return the total number of visible faces */
static int gpu_bmesh_face_visible_count(GSet *bm_faces)
{
	GSetIterator gh_iter;
	int totface = 0;

	GSET_ITER (gh_iter, bm_faces) {
		BMFace *f = BLI_gsetIterator_getKey(&gh_iter);

		if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN))
			totface++;
	}

	return totface;
}

/* Creates a vertex buffer (coordinate, normal, color) and, if smooth
 * shading, an element index buffer. */
void GPU_update_bmesh_pbvh_buffers(GPU_PBVH_Buffers *buffers,
                              BMesh *bm,
                              GSet *bm_faces,
                              GSet *bm_unique_verts,
                              GSet *bm_other_verts,
                              bool show_diffuse_color)
{
	VertexBufferFormat *vert_data;
	void *tri_data;
	int tottri, totvert, maxvert = 0;
	float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

	/* TODO, make mask layer optional for bmesh buffer */
	const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);

	buffers->show_diffuse_color = show_diffuse_color;
	buffers->use_matcaps = GPU_material_use_matcaps_get();

	if (!buffers->vert_buf || (buffers->smooth && !buffers->index_buf))
		return;

	/* Count visible triangles */
	tottri = gpu_bmesh_face_visible_count(bm_faces);

	if (buffers->smooth) {
		/* Count visible vertices */
		totvert = gpu_bmesh_vert_visible_count(bm_unique_verts, bm_other_verts);
	}
	else
		totvert = tottri * 3;

	if (!tottri) {
		buffers->tot_tri = 0;
		return;
	}

	if (buffers->use_matcaps)
		diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
	else if (show_diffuse_color) {
		/* due to dynamic nature of dyntopo, only get first material */
		GSetIterator gs_iter;
		BMFace *f;
		BLI_gsetIterator_init(&gs_iter, bm_faces);
		f = BLI_gsetIterator_getKey(&gs_iter);
		GPU_material_diffuse_get(f->mat_nr + 1, diffuse_color);
	}

	copy_v4_v4(buffers->diffuse_color, diffuse_color);

	/* Initialize vertex buffer */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB,
					sizeof(VertexBufferFormat) * totvert,
					NULL, GL_STATIC_DRAW_ARB);

	/* Fill vertex buffer */
	vert_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
	if (vert_data) {
		int v_index = 0;

		if (buffers->smooth) {
			GSetIterator gs_iter;

			/* Vertices get an index assigned for use in the triangle
			 * index buffer */
			bm->elem_index_dirty |= BM_VERT;

			GSET_ITER (gs_iter, bm_unique_verts) {
				gpu_bmesh_vert_to_buffer_copy(BLI_gsetIterator_getKey(&gs_iter),
				                              vert_data, &v_index, NULL, NULL,
				                              cd_vert_mask_offset, diffuse_color);
			}

			GSET_ITER (gs_iter, bm_other_verts) {
				gpu_bmesh_vert_to_buffer_copy(BLI_gsetIterator_getKey(&gs_iter),
				                              vert_data, &v_index, NULL, NULL,
				                              cd_vert_mask_offset, diffuse_color);
			}

			maxvert = v_index;
		}
		else {
			GSetIterator gs_iter;

			GSET_ITER (gs_iter, bm_faces) {
				BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

				BLI_assert(f->len == 3);

				if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
					BMVert *v[3];
					float fmask = 0;
					int i;

					// BM_iter_as_array(bm, BM_VERTS_OF_FACE, f, (void**)v, 3);
					BM_face_as_array_vert_tri(f, v);

					/* Average mask value */
					for (i = 0; i < 3; i++) {
						fmask += BM_ELEM_CD_GET_FLOAT(v[i], cd_vert_mask_offset);
					}
					fmask /= 3.0f;
					
					for (i = 0; i < 3; i++) {
						gpu_bmesh_vert_to_buffer_copy(v[i], vert_data,
						                              &v_index, f->no, &fmask,
						                              cd_vert_mask_offset, diffuse_color);
					}
				}
			}

			buffers->tot_tri = tottri;
		}

		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);

		/* gpu_bmesh_vert_to_buffer_copy sets dirty index values */
		bm->elem_index_dirty |= BM_VERT;
	}
	else {
		/* Memory map failed */
		glDeleteBuffersARB(1, &buffers->vert_buf);
		buffers->vert_buf = 0;
		return;
	}

	if (buffers->smooth) {
		const int use_short = (maxvert < USHRT_MAX);

		/* Initialize triangle index buffer */
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
						(use_short ?
						 sizeof(unsigned short) :
						 sizeof(unsigned int)) * 3 * tottri,
						NULL, GL_STATIC_DRAW_ARB);

		/* Fill triangle index buffer */
		tri_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if (tri_data) {
			GSetIterator gs_iter;

			GSET_ITER (gs_iter, bm_faces) {
				BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

				if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
					BMLoop *l_iter;
					BMLoop *l_first;

					l_iter = l_first = BM_FACE_FIRST_LOOP(f);
					do {
						BMVert *v = l_iter->v;
						if (use_short) {
							unsigned short *elem = tri_data;
							(*elem) = BM_elem_index_get(v);
							elem++;
							tri_data = elem;
						}
						else {
							unsigned int *elem = tri_data;
							(*elem) = BM_elem_index_get(v);
							elem++;
							tri_data = elem;
						}
					} while ((l_iter = l_iter->next) != l_first);
				}
			}

			glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);

			buffers->tot_tri = tottri;
			buffers->index_type = (use_short ?
								   GL_UNSIGNED_SHORT :
								   GL_UNSIGNED_INT);
		}
		else {
			/* Memory map failed */
			glDeleteBuffersARB(1, &buffers->index_buf);
			buffers->index_buf = 0;
		}
	}
}

GPU_PBVH_Buffers *GPU_build_bmesh_pbvh_buffers(int smooth_shading)
{
	GPU_PBVH_Buffers *buffers;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	if (smooth_shading)
		glGenBuffersARB(1, &buffers->index_buf);
	glGenBuffersARB(1, &buffers->vert_buf);
	buffers->use_bmesh = true;
	buffers->smooth = smooth_shading;
	buffers->show_diffuse_color = false;
	buffers->use_matcaps = false;

	return buffers;
}

static void gpu_draw_buffers_legacy_mesh(GPU_PBVH_Buffers *buffers)
{
	const MVert *mvert = buffers->mvert;
	int i, j;
	const int has_mask = (buffers->vmask != NULL);
	const MFace *face = &buffers->mface[buffers->face_indices[0]];
	float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

	if (buffers->use_matcaps)
		diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
	else if (buffers->show_diffuse_color)
		GPU_material_diffuse_get(face->mat_nr + 1, diffuse_color);

	if (has_mask) {
		gpu_colors_enable(VBO_DISABLED);
	}

	for (i = 0; i < buffers->totface; ++i) {
		MFace *f = buffers->mface + buffers->face_indices[i];
		int S = f->v4 ? 4 : 3;
		unsigned int *fv = &f->v1;

		if (paint_is_face_hidden(f, buffers->mvert))
			continue;

		glBegin((f->v4) ? GL_QUADS : GL_TRIANGLES);

		if (buffers->smooth) {
			for (j = 0; j < S; j++) {
				if (has_mask) {
					gpu_color_from_mask_set(buffers->vmask[fv[j]], diffuse_color);
				}
				glNormal3sv(mvert[fv[j]].no);
				glVertex3fv(mvert[fv[j]].co);
			}
		}
		else {
			float fno[3];

			/* calculate face normal */
			if (f->v4) {
				normal_quad_v3(fno, mvert[fv[0]].co, mvert[fv[1]].co,
				               mvert[fv[2]].co, mvert[fv[3]].co);
			}
			else
				normal_tri_v3(fno, mvert[fv[0]].co, mvert[fv[1]].co, mvert[fv[2]].co);
			glNormal3fv(fno);

			if (has_mask) {
				float fmask;

				/* calculate face mask color */
				fmask = (buffers->vmask[fv[0]] +
				         buffers->vmask[fv[1]] +
				         buffers->vmask[fv[2]]);
				if (f->v4)
					fmask = (fmask + buffers->vmask[fv[3]]) * 0.25f;
				else
					fmask /= 3.0f;
				gpu_color_from_mask_set(fmask, diffuse_color);
			}
			
			for (j = 0; j < S; j++)
				glVertex3fv(mvert[fv[j]].co);
		}
		
		glEnd();
	}

	if (has_mask) {
		gpu_colors_disable(VBO_DISABLED);
	}
}

static void gpu_draw_buffers_legacy_grids(GPU_PBVH_Buffers *buffers)
{
	const CCGKey *key = &buffers->gridkey;
	int i, j, x, y, gridsize = buffers->gridkey.grid_size;
	const int has_mask = key->has_mask;
	const DMFlagMat *flags = &buffers->grid_flag_mats[buffers->grid_indices[0]];
	float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

	if (buffers->use_matcaps)
		diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
	else if (buffers->show_diffuse_color)
		GPU_material_diffuse_get(flags->mat_nr + 1, diffuse_color);

	if (has_mask) {
		gpu_colors_enable(VBO_DISABLED);
	}

	for (i = 0; i < buffers->totgrid; ++i) {
		int g = buffers->grid_indices[i];
		CCGElem *grid = buffers->grids[g];
		BLI_bitmap *gh = buffers->grid_hidden[g];

		/* TODO: could use strips with hiding as well */

		if (gh) {
			glBegin(GL_QUADS);
			
			for (y = 0; y < gridsize - 1; y++) {
				for (x = 0; x < gridsize - 1; x++) {
					CCGElem *e[4] = {
						CCG_grid_elem(key, grid, x + 1, y + 1),
						CCG_grid_elem(key, grid, x + 1, y),
						CCG_grid_elem(key, grid, x, y),
						CCG_grid_elem(key, grid, x, y + 1)
					};

					/* skip face if any of its corners are hidden */
					if (paint_is_grid_face_hidden(gh, gridsize, x, y))
						continue;

					if (buffers->smooth) {
						for (j = 0; j < 4; j++) {
							if (has_mask) {
								gpu_color_from_mask_set(*CCG_elem_mask(key, e[j]), diffuse_color);
							}
							glNormal3fv(CCG_elem_no(key, e[j]));
							glVertex3fv(CCG_elem_co(key, e[j]));
						}
					}
					else {
						float fno[3];
						normal_quad_v3(fno,
						               CCG_elem_co(key, e[0]),
						               CCG_elem_co(key, e[1]),
						               CCG_elem_co(key, e[2]),
						               CCG_elem_co(key, e[3]));
						glNormal3fv(fno);

						if (has_mask) {
							gpu_color_from_mask_quad_set(key, e[0], e[1], e[2], e[3], diffuse_color);
						}

						for (j = 0; j < 4; j++)
							glVertex3fv(CCG_elem_co(key, e[j]));
					}
				}
			}

			glEnd();
		}
		else if (buffers->smooth) {
			for (y = 0; y < gridsize - 1; y++) {
				glBegin(GL_QUAD_STRIP);
				for (x = 0; x < gridsize; x++) {
					CCGElem *a = CCG_grid_elem(key, grid, x, y);
					CCGElem *b = CCG_grid_elem(key, grid, x, y + 1);

					if (has_mask) {
						gpu_color_from_mask_set(*CCG_elem_mask(key, a), diffuse_color);
					}
					glNormal3fv(CCG_elem_no(key, a));
					glVertex3fv(CCG_elem_co(key, a));
					if (has_mask) {
						gpu_color_from_mask_set(*CCG_elem_mask(key, b), diffuse_color);
					}
					glNormal3fv(CCG_elem_no(key, b));
					glVertex3fv(CCG_elem_co(key, b));
				}
				glEnd();
			}
		}
		else {
			for (y = 0; y < gridsize - 1; y++) {
				glBegin(GL_QUAD_STRIP);
				for (x = 0; x < gridsize; x++) {
					CCGElem *a = CCG_grid_elem(key, grid, x, y);
					CCGElem *b = CCG_grid_elem(key, grid, x, y + 1);

					if (x > 0) {
						CCGElem *c = CCG_grid_elem(key, grid, x - 1, y);
						CCGElem *d = CCG_grid_elem(key, grid, x - 1, y + 1);

						float fno[3];
						normal_quad_v3(fno,
						               CCG_elem_co(key, d),
						               CCG_elem_co(key, b),
						               CCG_elem_co(key, a),
						               CCG_elem_co(key, c));
						glNormal3fv(fno);

						if (has_mask) {
							gpu_color_from_mask_quad_set(key, a, b, c, d, diffuse_color);
						}
					}

					glVertex3fv(CCG_elem_co(key, a));
					glVertex3fv(CCG_elem_co(key, b));
				}
				glEnd();
			}
		}
	}

	if (has_mask) {
		gpu_colors_disable(VBO_DISABLED);
	}
}

void GPU_draw_pbvh_buffers(GPU_PBVH_Buffers *buffers, DMSetMaterial setMaterial,
                           bool wireframe)
{
	/* sets material from the first face, to solve properly face would need to
	 * be sorted in buckets by materials */
	if (setMaterial) {
		if (buffers->totface) {
			const MFace *f = &buffers->mface[buffers->face_indices[0]];
			if (!setMaterial(f->mat_nr + 1, NULL))
				return;
		}
		else if (buffers->totgrid) {
			const DMFlagMat *f = &buffers->grid_flag_mats[buffers->grid_indices[0]];
			if (!setMaterial(f->mat_nr + 1, NULL))
				return;
		}
		else {
			if (!setMaterial(1, NULL))
				return;
		}
	}

	glShadeModel((buffers->smooth || buffers->totface) ? GL_SMOOTH : GL_FLAT);

	if (buffers->vert_buf) {
		glEnableClientState(GL_VERTEX_ARRAY);
		if (!wireframe) {
			glEnableClientState(GL_NORMAL_ARRAY);
			gpu_colors_enable(VBO_ENABLED);
		}

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);

		if (buffers->index_buf)
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);

		if (wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

		if (buffers->tot_quad) {
			const char *offset = 0;
			int i, last = buffers->has_hidden ? 1 : buffers->totgrid;
			for (i = 0; i < last; i++) {
				glVertexPointer(3, GL_FLOAT, sizeof(VertexBufferFormat),
				                offset + offsetof(VertexBufferFormat, co));
				glNormalPointer(GL_SHORT, sizeof(VertexBufferFormat),
				                offset + offsetof(VertexBufferFormat, no));
				glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(VertexBufferFormat),
				               offset + offsetof(VertexBufferFormat, color));
				
				glDrawElements(GL_TRIANGLES, buffers->tot_quad * 6, buffers->index_type, 0);

				offset += buffers->gridkey.grid_area * sizeof(VertexBufferFormat);
			}
		}
		else if (buffers->tot_tri) {
			int totelem = buffers->tot_tri * 3;

			glVertexPointer(3, GL_FLOAT, sizeof(VertexBufferFormat),
			                (void *)offsetof(VertexBufferFormat, co));
			glNormalPointer(GL_SHORT, sizeof(VertexBufferFormat),
			                (void *)offsetof(VertexBufferFormat, no));
			glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(VertexBufferFormat),
			               (void *)offsetof(VertexBufferFormat, color));

			if (buffers->index_buf)
				glDrawElements(GL_TRIANGLES, totelem, buffers->index_type, 0);
			else
				glDrawArrays(GL_TRIANGLES, 0, totelem);
		}

		if (wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		if (buffers->index_buf)
			glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

		glDisableClientState(GL_VERTEX_ARRAY);
		if (!wireframe) {
			glDisableClientState(GL_NORMAL_ARRAY);
			gpu_colors_disable(VBO_ENABLED);
		}
	}
	/* fallbacks if we are out of memory or VBO is disabled */
	else if (buffers->totface) {
		gpu_draw_buffers_legacy_mesh(buffers);
	}
	else if (buffers->totgrid) {
		gpu_draw_buffers_legacy_grids(buffers);
	}
}

bool GPU_pbvh_buffers_diffuse_changed(GPU_PBVH_Buffers *buffers, GSet *bm_faces, bool show_diffuse_color)
{
	float diffuse_color[4];
	bool use_matcaps = GPU_material_use_matcaps_get();

	if (buffers->show_diffuse_color != show_diffuse_color)
		return true;

	if (buffers->use_matcaps != use_matcaps)
		return true;

	if ((buffers->show_diffuse_color == false) || use_matcaps)
		return false;

	if (buffers->mface) {
		MFace *f = buffers->mface + buffers->face_indices[0];

		GPU_material_diffuse_get(f->mat_nr + 1, diffuse_color);
	}
	else if (buffers->use_bmesh) {
		/* due to dynamc nature of dyntopo, only get first material */
		if (BLI_gset_size(bm_faces) > 0) {
			GSetIterator gs_iter;
			BMFace *f;

			BLI_gsetIterator_init(&gs_iter, bm_faces);
			f = BLI_gsetIterator_getKey(&gs_iter);
			GPU_material_diffuse_get(f->mat_nr + 1, diffuse_color);
		}
		else {
			return false;
		}
	}
	else {
		const DMFlagMat *flags = &buffers->grid_flag_mats[buffers->grid_indices[0]];

		GPU_material_diffuse_get(flags->mat_nr + 1, diffuse_color);
	}

	return !equals_v3v3(diffuse_color, buffers->diffuse_color);
}

/* release a GPU_PBVH_Buffers id;
 *
 * Thread-unsafe version for internal usage only.
 */
static void gpu_pbvh_buffer_free_intern(GLuint id)
{
	GPUBufferPool *pool;

	/* zero id is vertex buffers off */
	if (!id)
		return;

	pool = gpu_get_global_buffer_pool();

	/* free the buffers immediately if we are on main thread */
	if (BLI_thread_is_main()) {
		glDeleteBuffersARB(1, &id);

		if (pool->totpbvhbufids > 0) {
			glDeleteBuffersARB(pool->totpbvhbufids, pool->pbvhbufids);
			pool->totpbvhbufids = 0;
		}
		return;
	}
	/* outside of main thread, can't safely delete the
	 * buffer, so increase pool size */
	if (pool->maxpbvhsize == pool->totpbvhbufids) {
		pool->maxpbvhsize += MAX_FREE_GPU_BUFF_IDS;
		pool->pbvhbufids = MEM_reallocN(pool->pbvhbufids,
										sizeof(*pool->pbvhbufids) * pool->maxpbvhsize);
	}

	/* insert the buffer into the beginning of the pool */
	pool->pbvhbufids[pool->totpbvhbufids++] = id;
}


void GPU_free_pbvh_buffers(GPU_PBVH_Buffers *buffers)
{
	if (buffers) {
		if (buffers->vert_buf)
			gpu_pbvh_buffer_free_intern(buffers->vert_buf);
		if (buffers->index_buf && (buffers->tot_tri || buffers->has_hidden))
			gpu_pbvh_buffer_free_intern(buffers->index_buf);

		MEM_freeN(buffers);
	}
}


/* debug function, draws the pbvh BB */
void GPU_draw_pbvh_BB(float min[3], float max[3], bool leaf)
{
	float quads[4][4][3] = {
	    {
	        {min[0], min[1], min[2]},
	        {max[0], min[1], min[2]},
	        {max[0], min[1], max[2]},
	        {min[0], min[1], max[2]}
	    },

	    {
	        {min[0], min[1], min[2]},
	        {min[0], max[1], min[2]},
	        {min[0], max[1], max[2]},
	        {min[0], min[1], max[2]}
	    },

	    {
	        {max[0], max[1], min[2]},
	        {max[0], min[1], min[2]},
	        {max[0], min[1], max[2]},
	        {max[0], max[1], max[2]}
	    },

	    {
	        {max[0], max[1], min[2]},
	        {min[0], max[1], min[2]},
	        {min[0], max[1], max[2]},
	        {max[0], max[1], max[2]}
	    },
	};

	if (leaf)
		glColor4f(0.0, 1.0, 0.0, 0.5);
	else
		glColor4f(1.0, 0.0, 0.0, 0.5);

	glVertexPointer(3, GL_FLOAT, 0, &quads[0][0][0]);
	glDrawArrays(GL_QUADS, 0, 16);
}

void GPU_init_draw_pbvh_BB(void)
{
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_CULL_FACE);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_LIGHTING);
	glDisable(GL_COLOR_MATERIAL);
	glEnable(GL_BLEND);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

void GPU_end_draw_pbvh_BB(void)
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glPopAttrib();
}
