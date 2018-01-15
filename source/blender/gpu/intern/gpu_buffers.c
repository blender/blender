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
 * Mesh drawing using OpenGL VBO (Vertex Buffer Objects)
 */

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "DNA_meshdata_types.h"

#include "BKE_ccg.h"
#include "BKE_DerivedMesh.h"
#include "BKE_paint.h"
#include "BKE_mesh.h"
#include "BKE_pbvh.h"

#include "GPU_buffers.h"
#include "GPU_draw.h"
#include "GPU_immediate.h"
#include "GPU_batch.h"

#include "bmesh.h"

/* TODO: gawain support for baseelemarray */
// #define USE_BASE_ELEM

typedef enum {
	GPU_BUFFER_VERTEX_STATE = (1 << 0),
	GPU_BUFFER_NORMAL_STATE = (1 << 1),
	GPU_BUFFER_TEXCOORD_UNIT_0_STATE = (1 << 2),
	GPU_BUFFER_TEXCOORD_UNIT_2_STATE = (1 << 3),
	GPU_BUFFER_COLOR_STATE = (1 << 4),
	GPU_BUFFER_ELEMENT_STATE = (1 << 5),
} GPUBufferState;

typedef struct {
	GLenum gl_buffer_type;
	int num_components; /* number of data components for one vertex */
} GPUBufferTypeSettings;


static size_t gpu_buffer_size_from_type(DerivedMesh *dm, GPUBufferType type);

static const GPUBufferTypeSettings gpu_buffer_type_settings[] = {
    /* vertex */
    {GL_ARRAY_BUFFER, 3},
    /* normal */
    {GL_ARRAY_BUFFER, 4}, /* we copy 3 shorts per normal but we add a fourth for alignment */
    /* mcol */
    {GL_ARRAY_BUFFER, 4},
    /* uv */
    {GL_ARRAY_BUFFER, 2},
    /* uv for texpaint */
    {GL_ARRAY_BUFFER, 4},
    /* edge */
    {GL_ELEMENT_ARRAY_BUFFER, 2},
    /* uv edge */
    {GL_ELEMENT_ARRAY_BUFFER, 4},
    /* triangles, 1 point since we are allocating from tottriangle points, which account for all points */
    {GL_ELEMENT_ARRAY_BUFFER, 1},
};

#define MAX_GPU_ATTRIB_DATA 32

#define BUFFER_OFFSET(n) ((GLubyte *)NULL + (n))

static GPUBufferState GLStates = 0;
static GPUAttrib attribData[MAX_GPU_ATTRIB_DATA] = { { -1, 0, 0 } };

static ThreadMutex buffer_mutex = BLI_MUTEX_INITIALIZER;

/* multires global buffer, can be used for many grids having the same grid size */
typedef struct GridCommonGPUBuffer {
	Gwn_IndexBuf *mres_buffer;
	int mres_prev_gridsize;
	unsigned mres_prev_totquad;
} GridCommonGPUBuffer;

void GPU_buffer_material_finalize(GPUDrawObject *gdo, GPUBufferMaterial *matinfo, int totmat)
{
	int i, curmat, curelement;

	/* count the number of materials used by this DerivedMesh */
	for (i = 0; i < totmat; i++) {
		if (matinfo[i].totelements > 0)
			gdo->totmaterial++;
	}

	/* allocate an array of materials used by this DerivedMesh */
	gdo->materials = MEM_mallocN(sizeof(GPUBufferMaterial) * gdo->totmaterial,
	                             "GPUDrawObject.materials");

	/* initialize the materials array */
	for (i = 0, curmat = 0, curelement = 0; i < totmat; i++) {
		if (matinfo[i].totelements > 0) {
			gdo->materials[curmat] = matinfo[i];
			gdo->materials[curmat].start = curelement;
			gdo->materials[curmat].mat_nr = i;
			gdo->materials[curmat].polys = MEM_mallocN(sizeof(int) * matinfo[i].totpolys, "GPUBufferMaterial.polys");

			curelement += matinfo[i].totelements;
			curmat++;
		}
	}

	MEM_freeN(matinfo);
}


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
	/* actual allocated length of the arrays */
	int maxsize;
	GPUBuffer **buffers;
} GPUBufferPool;
#define MAX_FREE_GPU_BUFFERS 8

/* create a new GPUBufferPool */
static GPUBufferPool *gpu_buffer_pool_new(void)
{
	GPUBufferPool *pool;

	pool = MEM_callocN(sizeof(GPUBufferPool), "GPUBuffer_Pool");

	pool->maxsize = MAX_FREE_GPU_BUFFERS;
	pool->buffers = MEM_mallocN(sizeof(*pool->buffers) * pool->maxsize,
	                            "GPUBufferPool.buffers");
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
	glDeleteBuffers(1, &last->id);

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
	MEM_freeN(pool);
}

static void gpu_buffer_pool_free_unused(GPUBufferPool *pool)
{
	if (!pool)
		return;

	BLI_mutex_lock(&buffer_mutex);
	
	while (pool->totbuf)
		gpu_buffer_pool_delete_last(pool);

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
static GPUBuffer *gpu_buffer_alloc_intern(size_t size)
{
	GPUBufferPool *pool;
	GPUBuffer *buf;
	int i, bestfit = -1;
	size_t bufsize;

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

	glGenBuffers(1, &buf->id);
	glBindBuffer(GL_ARRAY_BUFFER, buf->id);
	glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return buf;
}

/* Same as above, but safe for threading. */
GPUBuffer *GPU_buffer_alloc(size_t size)
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

void GPU_drawobject_free(DerivedMesh *dm)
{
	GPUDrawObject *gdo;
	int i;

	if (!dm || !(gdo = dm->drawObject))
		return;

	for (i = 0; i < gdo->totmaterial; i++) {
		if (gdo->materials[i].polys)
			MEM_freeN(gdo->materials[i].polys);
	}

	MEM_freeN(gdo->materials);
	if (gdo->vert_points)
		MEM_freeN(gdo->vert_points);
#ifdef USE_GPU_POINT_LINK
	MEM_freeN(gdo->vert_points_mem);
#endif
	GPU_buffer_free(gdo->points);
	GPU_buffer_free(gdo->normals);
	GPU_buffer_free(gdo->uv);
	GPU_buffer_free(gdo->uv_tex);
	GPU_buffer_free(gdo->colors);
	GPU_buffer_free(gdo->edges);
	GPU_buffer_free(gdo->uvedges);
	GPU_buffer_free(gdo->triangles);

	MEM_freeN(gdo);
	dm->drawObject = NULL;
}

static GPUBuffer *gpu_try_realloc(GPUBufferPool *pool, GPUBuffer *buffer, size_t size)
{
	/* try freeing an entry from the pool
	 * and reallocating the buffer */
	gpu_buffer_free_intern(buffer);

	buffer = NULL;

	while (pool->totbuf && !buffer) {
		gpu_buffer_pool_delete_last(pool);
		buffer = gpu_buffer_alloc_intern(size);
	}

	return buffer;
}

static GPUBuffer *gpu_buffer_setup(DerivedMesh *dm, GPUDrawObject *object,
                                   int type, void *user, GPUBuffer *buffer)
{
	GPUBufferPool *pool;
	float *varray;
	int *mat_orig_to_new;
	int i;
	const GPUBufferTypeSettings *ts = &gpu_buffer_type_settings[type];
	GLenum target = ts->gl_buffer_type;
	size_t size = gpu_buffer_size_from_type(dm, type);
	GLboolean uploaded;

	pool = gpu_get_global_buffer_pool();

	BLI_mutex_lock(&buffer_mutex);

	/* alloc a GPUBuffer; fall back to legacy mode on failure */
	if (!buffer) {
		if (!(buffer = gpu_buffer_alloc_intern(size))) {
			BLI_mutex_unlock(&buffer_mutex);
			return NULL;
		}
	}

	mat_orig_to_new = MEM_mallocN(sizeof(*mat_orig_to_new) * dm->totmat,
	                              "GPU_buffer_setup.mat_orig_to_new");
	for (i = 0; i < object->totmaterial; i++) {
		/* map from original material index to new
		 * GPUBufferMaterial index */
		mat_orig_to_new[object->materials[i].mat_nr] = i;
	}

	/* bind the buffer and discard previous data,
	 * avoids stalling gpu */
	glBindBuffer(target, buffer->id);
	glBufferData(target, buffer->size, NULL, GL_STATIC_DRAW);

	/* attempt to map the buffer */
	if (!(varray = glMapBuffer(target, GL_WRITE_ONLY))) {
		buffer = gpu_try_realloc(pool, buffer, size);

		/* allocation still failed; unfortunately we need to exit */
		if (!(buffer && (varray = glMapBuffer(target, GL_WRITE_ONLY)))) {
			if (buffer)
				gpu_buffer_free_intern(buffer);
			BLI_mutex_unlock(&buffer_mutex);
			return NULL;
		}
	}

	uploaded = GL_FALSE;

	/* attempt to upload the data to the VBO */
	while (uploaded == GL_FALSE) {
		dm->copy_gpu_data(dm, type, varray, mat_orig_to_new, user);
		/* glUnmapBuffer returns GL_FALSE if
		 * the data store is corrupted; retry
		 * in that case */
		uploaded = glUnmapBuffer(target);
	}
	glBindBuffer(target, 0);

	MEM_freeN(mat_orig_to_new);

	BLI_mutex_unlock(&buffer_mutex);

	return buffer;
}

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
			return &gdo->uv_tex;
		case GPU_BUFFER_EDGE:
			return &gdo->edges;
		case GPU_BUFFER_UVEDGE:
			return &gdo->uvedges;
		case GPU_BUFFER_TRIANGLES:
			return &gdo->triangles;
		default:
			return NULL;
	}
}

/* get the amount of space to allocate for a buffer of a particular type */
static size_t gpu_buffer_size_from_type(DerivedMesh *dm, GPUBufferType type)
{
	const int components = gpu_buffer_type_settings[type].num_components;
	switch (type) {
		case GPU_BUFFER_VERTEX:
			return sizeof(float) * components * (dm->drawObject->tot_loop_verts + dm->drawObject->tot_loose_point);
		case GPU_BUFFER_NORMAL:
			return sizeof(short) * components * dm->drawObject->tot_loop_verts;
		case GPU_BUFFER_COLOR:
			return sizeof(char) * components * dm->drawObject->tot_loop_verts;
		case GPU_BUFFER_UV:
			return sizeof(float) * components * dm->drawObject->tot_loop_verts;
		case GPU_BUFFER_UV_TEXPAINT:
			return sizeof(float) * components * dm->drawObject->tot_loop_verts;
		case GPU_BUFFER_EDGE:
			return sizeof(int) * components * dm->drawObject->totedge;
		case GPU_BUFFER_UVEDGE:
			return sizeof(int) * components * dm->drawObject->tot_loop_verts;
		case GPU_BUFFER_TRIANGLES:
			return sizeof(int) * components * dm->drawObject->tot_triangle_point;
		default:
			return -1;
	}
}

/* call gpu_buffer_setup with settings for a particular type of buffer */
static GPUBuffer *gpu_buffer_setup_type(DerivedMesh *dm, GPUBufferType type, GPUBuffer *buf)
{
	void *user_data = NULL;

	/* special handling for MCol and UV buffers */
	if (type == GPU_BUFFER_COLOR) {
		if (!(user_data = DM_get_loop_data_layer(dm, dm->drawObject->colType)))
			return NULL;
	}
	else if (ELEM(type, GPU_BUFFER_UV, GPU_BUFFER_UV_TEXPAINT)) {
		if (!DM_get_loop_data_layer(dm, CD_MLOOPUV))
			return NULL;
	}

	buf = gpu_buffer_setup(dm, dm->drawObject, type, user_data, buf);

	return buf;
}

/* get the buffer of `type', initializing the GPUDrawObject and
 * buffer if needed */
static GPUBuffer *gpu_buffer_setup_common(DerivedMesh *dm, GPUBufferType type, bool update)
{
	GPUBuffer **buf;

	if (!dm->drawObject)
		dm->drawObject = dm->gpuObjectNew(dm);

	buf = gpu_drawobject_buffer_from_type(dm->drawObject, type);
	if (!(*buf))
		*buf = gpu_buffer_setup_type(dm, type, NULL);
	else if (update)
		*buf = gpu_buffer_setup_type(dm, type, *buf);

	return *buf;
}

void GPU_vertex_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_VERTEX, false))
		return;

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, dm->drawObject->points->id);
	glVertexPointer(3, GL_FLOAT, 0, 0);
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

void GPU_normal_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_NORMAL, false))
		return;

	glEnableClientState(GL_NORMAL_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, dm->drawObject->normals->id);
	glNormalPointer(GL_SHORT, 4 * sizeof(short), 0);

	GLStates |= GPU_BUFFER_NORMAL_STATE;
}

void GPU_uv_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_UV, false))
		return;

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, dm->drawObject->uv->id);
	glTexCoordPointer(2, GL_FLOAT, 0, 0);

	GLStates |= GPU_BUFFER_TEXCOORD_UNIT_0_STATE;
}

void GPU_texpaint_uv_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_UV_TEXPAINT, false))
		return;

	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, dm->drawObject->uv_tex->id);
	glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), 0);
	glClientActiveTexture(GL_TEXTURE2);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 4 * sizeof(float), BUFFER_OFFSET(2 * sizeof(float)));
	glClientActiveTexture(GL_TEXTURE0);

	GLStates |= GPU_BUFFER_TEXCOORD_UNIT_0_STATE | GPU_BUFFER_TEXCOORD_UNIT_2_STATE;
}


void GPU_color_setup(DerivedMesh *dm, int colType)
{
	bool update = false;

	if (!dm->drawObject) {
		/* XXX Not really nice, but we need a valid gpu draw object to set the colType...
		 *     Else we would have to add a new param to gpu_buffer_setup_common. */
		dm->drawObject = dm->gpuObjectNew(dm);
		dm->dirty &= ~DM_DIRTY_MCOL_UPDATE_DRAW;
		dm->drawObject->colType = colType;
	}
	/* In paint mode, dm may stay the same during stroke, however we still want to update colors!
	 * Also check in case we changed color type (i.e. which MCol cdlayer we use). */
	else if ((dm->dirty & DM_DIRTY_MCOL_UPDATE_DRAW) || (colType != dm->drawObject->colType)) {
		update = true;
		dm->dirty &= ~DM_DIRTY_MCOL_UPDATE_DRAW;
		dm->drawObject->colType = colType;
	}

	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_COLOR, update))
		return;

	glEnableClientState(GL_COLOR_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, dm->drawObject->colors->id);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);

	GLStates |= GPU_BUFFER_COLOR_STATE;
}

void GPU_buffer_bind_as_color(GPUBuffer *buffer)
{
	glEnableClientState(GL_COLOR_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, buffer->id);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, 0);

	GLStates |= GPU_BUFFER_COLOR_STATE;
}


void GPU_edge_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_EDGE, false))
		return;

	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_VERTEX, false))
		return;

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, dm->drawObject->points->id);
	glVertexPointer(3, GL_FLOAT, 0, 0);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dm->drawObject->edges->id);

	GLStates |= (GPU_BUFFER_VERTEX_STATE | GPU_BUFFER_ELEMENT_STATE);
}

void GPU_uvedge_setup(DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_UVEDGE, false))
		return;

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, dm->drawObject->uvedges->id);
	glVertexPointer(2, GL_FLOAT, 0, 0);
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

void GPU_triangle_setup(struct DerivedMesh *dm)
{
	if (!gpu_buffer_setup_common(dm, GPU_BUFFER_TRIANGLES, false))
		return;

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dm->drawObject->triangles->id);
	GLStates |= GPU_BUFFER_ELEMENT_STATE;
}

static int gpu_typesize(int type)
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
		int typesize = gpu_typesize(data[i].type);
		if (typesize != 0)
			elementsize += typesize * data[i].size;
	}
	return elementsize;
}

void GPU_interleaved_attrib_setup(GPUBuffer *buffer, GPUAttrib data[], int numdata, int element_size)
{
	int i;
	int elementsize;
	size_t offset = 0;

	for (i = 0; i < MAX_GPU_ATTRIB_DATA; i++) {
		if (attribData[i].index != -1) {
			glDisableVertexAttribArray(attribData[i].index);
		}
		else
			break;
	}
	if (element_size == 0)
		elementsize = GPU_attrib_element_size(data, numdata);
	else
		elementsize = element_size;

	glBindBuffer(GL_ARRAY_BUFFER, buffer->id);
	
	for (i = 0; i < numdata; i++) {
		glEnableVertexAttribArray(data[i].index);
		int info = 0;
		if (data[i].type == GL_UNSIGNED_BYTE) {
			info |= GPU_ATTR_INFO_SRGB;
		}
		glUniform1i(data[i].info_index, info);

		glVertexAttribPointer(data[i].index, data[i].size, data[i].type,
		                         GL_TRUE, elementsize, BUFFER_OFFSET(offset));
		offset += data[i].size * gpu_typesize(data[i].type);
		
		attribData[i].index = data[i].index;
		attribData[i].size = data[i].size;
		attribData[i].type = data[i].type;
	}
	
	attribData[numdata].index = -1;	
}

void GPU_interleaved_attrib_unbind(void)
{
	int i;
	for (i = 0; i < MAX_GPU_ATTRIB_DATA; i++) {
		if (attribData[i].index != -1) {
			glDisableVertexAttribArray(attribData[i].index);
		}
		else
			break;
	}
	attribData[0].index = -1;
}

void GPU_buffers_unbind(void)
{
	int i;

	if (GLStates & GPU_BUFFER_VERTEX_STATE)
		glDisableClientState(GL_VERTEX_ARRAY);
	if (GLStates & GPU_BUFFER_NORMAL_STATE)
		glDisableClientState(GL_NORMAL_ARRAY);
	if (GLStates & GPU_BUFFER_TEXCOORD_UNIT_0_STATE)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	if (GLStates & GPU_BUFFER_TEXCOORD_UNIT_2_STATE) {
		glClientActiveTexture(GL_TEXTURE2);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glClientActiveTexture(GL_TEXTURE0);
	}
	if (GLStates & GPU_BUFFER_COLOR_STATE)
		glDisableClientState(GL_COLOR_ARRAY);
	if (GLStates & GPU_BUFFER_ELEMENT_STATE)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	GLStates &= ~(GPU_BUFFER_VERTEX_STATE | GPU_BUFFER_NORMAL_STATE |
	              GPU_BUFFER_TEXCOORD_UNIT_0_STATE | GPU_BUFFER_TEXCOORD_UNIT_2_STATE |
	              GPU_BUFFER_COLOR_STATE | GPU_BUFFER_ELEMENT_STATE);

	for (i = 0; i < MAX_GPU_ATTRIB_DATA; i++) {
		if (attribData[i].index != -1) {
			glDisableVertexAttribArray(attribData[i].index);
		}
		else
			break;
	}
	attribData[0].index = -1;

	glBindBuffer(GL_ARRAY_BUFFER, 0);
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

static int gpu_binding_type_gl[] =
{
	GL_ARRAY_BUFFER,
	GL_ELEMENT_ARRAY_BUFFER
};

void *GPU_buffer_lock(GPUBuffer *buffer, GPUBindingType binding)
{
	float *varray;
	int bindtypegl;

	if (!buffer)
		return 0;

	bindtypegl = gpu_binding_type_gl[binding];
	glBindBuffer(bindtypegl, buffer->id);
	varray = glMapBuffer(bindtypegl, GL_WRITE_ONLY);
	return varray;
}

void *GPU_buffer_lock_stream(GPUBuffer *buffer, GPUBindingType binding)
{
	float *varray;
	int bindtypegl;

	if (!buffer)
		return 0;

	bindtypegl = gpu_binding_type_gl[binding];
	glBindBuffer(bindtypegl, buffer->id);
	/* discard previous data, avoid stalling gpu */
	glBufferData(bindtypegl, buffer->size, 0, GL_STREAM_DRAW);
	varray = glMapBuffer(bindtypegl, GL_WRITE_ONLY);
	return varray;
}

void GPU_buffer_unlock(GPUBuffer *UNUSED(buffer), GPUBindingType binding)
{
	int bindtypegl = gpu_binding_type_gl[binding];
	/* note: this operation can fail, could return
	 * an error code from this function? */
	glUnmapBuffer(bindtypegl);
	glBindBuffer(bindtypegl, 0);
}

void GPU_buffer_bind(GPUBuffer *buffer, GPUBindingType binding)
{
	int bindtypegl = gpu_binding_type_gl[binding];
	glBindBuffer(bindtypegl, buffer->id);
}

void GPU_buffer_unbind(GPUBuffer *UNUSED(buffer), GPUBindingType binding)
{
	int bindtypegl = gpu_binding_type_gl[binding];
	glBindBuffer(bindtypegl, 0);
}

/* used for drawing edges */
void GPU_buffer_draw_elements(GPUBuffer *UNUSED(elements), unsigned int mode, int start, int count)
{
	glDrawElements(mode, count, GL_UNSIGNED_INT, BUFFER_OFFSET(start * sizeof(unsigned int)));
}


/* XXX: the rest of the code in this file is used for optimized PBVH
 * drawing and doesn't interact at all with the buffer code above */

struct GPU_PBVH_Buffers {
	Gwn_IndexBuf *index_buf, *index_buf_fast;
	Gwn_VertBuf *vert_buf;

	Gwn_Batch *triangles;
	Gwn_Batch *triangles_fast;

	/* mesh pointers in case buffer allocation fails */
	const MPoly *mpoly;
	const MLoop *mloop;
	const MLoopTri *looptri;
	const MVert *mvert;

	const int *face_indices;
	int        face_indices_len;
	const float *vmask;

	/* grid pointers */
	CCGKey gridkey;
	CCGElem **grids;
	const DMFlagMat *grid_flag_mats;
	BLI_bitmap * const *grid_hidden;
	const int *grid_indices;
	int totgrid;
	bool has_hidden;
	bool is_index_buf_global;  /* Means index_buf uses global bvh's grid_common_gpu_buffer, **DO NOT** free it! */

	bool use_bmesh;

	unsigned int tot_tri, tot_quad;

	/* The PBVH ensures that either all faces in the node are
	 * smooth-shaded or all faces are flat-shaded */
	bool smooth;

	bool show_diffuse_color;
	bool show_mask;

	bool use_matcaps;
	float diffuse_color[4];
};

typedef struct {
	uint pos, nor, col;
} VertexBufferAttrID;

static void gpu_pbvh_vert_format_init__gwn(Gwn_VertFormat *format, VertexBufferAttrID *vbo_id)
{
	vbo_id->pos = GWN_vertformat_attr_add(format, "pos", GWN_COMP_F32, 3, GWN_FETCH_FLOAT);
	vbo_id->nor = GWN_vertformat_attr_add(format, "nor", GWN_COMP_I16, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
	vbo_id->col = GWN_vertformat_attr_add(format, "color", GWN_COMP_U8, 3, GWN_FETCH_INT_TO_FLOAT_UNIT);
}

static void gpu_pbvh_batch_init(GPU_PBVH_Buffers *buffers)
{
	/* force flushing to the GPU */
	if (buffers->vert_buf->data) {
		GWN_vertbuf_use(buffers->vert_buf);
	}

	GWN_BATCH_DISCARD_SAFE(buffers->triangles);
	buffers->triangles = GWN_batch_create(
	        GWN_PRIM_TRIS, buffers->vert_buf,
	        /* can be NULL */
	        buffers->index_buf);

	GWN_BATCH_DISCARD_SAFE(buffers->triangles_fast);
	if (buffers->index_buf_fast) {
		buffers->triangles_fast = GWN_batch_create(
		        GWN_PRIM_TRIS, buffers->vert_buf,
		        /* can be NULL */
		        buffers->index_buf_fast);
	}
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

void GPU_pbvh_mesh_buffers_update(
        GPU_PBVH_Buffers *buffers, const MVert *mvert,
        const int *vert_indices, int totvert, const float *vmask,
        const int (*face_vert_indices)[3],
        const int update_flags)
{
	const bool show_diffuse_color = (update_flags & GPU_PBVH_BUFFERS_SHOW_DIFFUSE_COLOR) != 0;
	const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;

	buffers->vmask = vmask;
	buffers->show_diffuse_color = show_diffuse_color;
	buffers->show_mask = show_mask;
	buffers->use_matcaps = GPU_material_use_matcaps_get();

	{
		int totelem = (buffers->smooth ? totvert : (buffers->tot_tri * 3));
		float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 0.8f};

		if (buffers->use_matcaps)
			diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
		else if (show_diffuse_color) {
			const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
			const MPoly *mp = &buffers->mpoly[lt->poly];

			GPU_material_diffuse_get(mp->mat_nr + 1, diffuse_color);
		}

		copy_v4_v4(buffers->diffuse_color, diffuse_color);

		uchar diffuse_color_ub[4];
		rgba_float_to_uchar(diffuse_color_ub, diffuse_color);

		/* Build VBO */
		GWN_VERTBUF_DISCARD_SAFE(buffers->vert_buf);

		/* match 'VertexBufferFormat' */
		Gwn_VertFormat format = {0};
		VertexBufferAttrID vbo_id;
		gpu_pbvh_vert_format_init__gwn(&format, &vbo_id);

		buffers->vert_buf = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(buffers->vert_buf, totelem);

		if (buffers->vert_buf->data) {
			/* Vertex data is shared if smooth-shaded, but separate
			 * copies are made for flat shading because normals
			 * shouldn't be shared. */
			if (buffers->smooth) {
				for (uint i = 0; i < totvert; ++i) {
					const MVert *v = &mvert[vert_indices[i]];
					GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.pos, i, v->co);
					GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.nor, i, v->no);
				}

				for (uint i = 0; i < buffers->face_indices_len; i++) {
					const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
					for (uint j = 0; j < 3; j++) {
						int vidx = face_vert_indices[i][j];
						if (vmask && show_mask) {
							int v_index = buffers->mloop[lt->tri[j]].v;
							uchar color_ub[3];
							gpu_color_from_mask_copy(vmask[v_index], diffuse_color, color_ub);
							GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.col, vidx, color_ub);
						}
						else {
							GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.col, vidx, diffuse_color_ub);
						}
					}
				}
			}
			else {
				/* calculate normal for each polygon only once */
				unsigned int mpoly_prev = UINT_MAX;
				short no[3];
				int vbo_index = 0;

				for (uint i = 0; i < buffers->face_indices_len; i++) {
					const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
					const unsigned int vtri[3] = {
					    buffers->mloop[lt->tri[0]].v,
					    buffers->mloop[lt->tri[1]].v,
					    buffers->mloop[lt->tri[2]].v,
					};

					if (paint_is_face_hidden(lt, mvert, buffers->mloop))
						continue;

					/* Face normal and mask */
					if (lt->poly != mpoly_prev) {
						const MPoly *mp = &buffers->mpoly[lt->poly];
						float fno[3];
						BKE_mesh_calc_poly_normal(mp, &buffers->mloop[mp->loopstart], mvert, fno);
						normal_float_to_short_v3(no, fno);
						mpoly_prev = lt->poly;
					}

					uchar color_ub[3];
					if (vmask && show_mask) {
						float fmask = (vmask[vtri[0]] + vmask[vtri[1]] + vmask[vtri[2]]) / 3.0f;
						gpu_color_from_mask_copy(fmask, diffuse_color, color_ub);
					}
					else {
						copy_v3_v3_uchar(color_ub, diffuse_color_ub);
					}

					for (uint j = 0; j < 3; j++) {
						const MVert *v = &mvert[vtri[j]];

						GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.pos, vbo_index, v->co);
						GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.nor, vbo_index, no);
						GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.col, vbo_index, color_ub);

						vbo_index++;
					}
				}
			}

			gpu_pbvh_batch_init(buffers);
		}
		else {
			GWN_VERTBUF_DISCARD_SAFE(buffers->vert_buf);
		}
	}

	buffers->mvert = mvert;
}

GPU_PBVH_Buffers *GPU_pbvh_mesh_buffers_build(
        const int (*face_vert_indices)[3],
        const MPoly *mpoly, const MLoop *mloop, const MLoopTri *looptri,
        const MVert *mvert,
        const int *face_indices,
        const int  face_indices_len)
{
	GPU_PBVH_Buffers *buffers;
	int i, tottri;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");

	/* smooth or flat for all */
#if 0
	buffers->smooth = mpoly[looptri[face_indices[0]].poly].flag & ME_SMOOTH;
#else
	/* for DrawManager we dont support mixed smooth/flat */
	buffers->smooth = (mpoly[0].flag & ME_SMOOTH) != 0;
#endif

	buffers->show_diffuse_color = false;
	buffers->show_mask = true;
	buffers->use_matcaps = false;

	/* Count the number of visible triangles */
	for (i = 0, tottri = 0; i < face_indices_len; ++i) {
		const MLoopTri *lt = &looptri[face_indices[i]];
		if (!paint_is_face_hidden(lt, mvert, mloop))
			tottri++;
	}

	if (tottri == 0) {
		buffers->tot_tri = 0;

		buffers->mpoly = mpoly;
		buffers->mloop = mloop;
		buffers->looptri = looptri;
		buffers->face_indices = face_indices;
		buffers->face_indices_len = 0;

		return buffers;
	}

	/* An element index buffer is used for smooth shading, but flat
	 * shading requires separate vertex normals so an index buffer is
	 * can't be used there. */
	if (buffers->smooth) {
		/* Fill the triangle buffer */
		buffers->index_buf = NULL;
		Gwn_IndexBufBuilder elb;
		GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, tottri, INT_MAX);

		for (i = 0; i < face_indices_len; ++i) {
			const MLoopTri *lt = &looptri[face_indices[i]];

			/* Skip hidden faces */
			if (paint_is_face_hidden(lt, mvert, mloop))
				continue;

			GWN_indexbuf_add_tri_verts(&elb, UNPACK3(face_vert_indices[i]));
		}
		buffers->index_buf = GWN_indexbuf_build(&elb);
	}
	else {
		if (!buffers->is_index_buf_global) {
			GWN_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
		}
		buffers->index_buf = NULL;
		buffers->is_index_buf_global = false;
	}

	buffers->tot_tri = tottri;

	buffers->mpoly = mpoly;
	buffers->mloop = mloop;
	buffers->looptri = looptri;

	buffers->face_indices = face_indices;
	buffers->face_indices_len = face_indices_len;

	return buffers;
}

void GPU_pbvh_grid_buffers_update(
        GPU_PBVH_Buffers *buffers, CCGElem **grids,
        const DMFlagMat *grid_flag_mats, int *grid_indices,
        int totgrid, const CCGKey *key,
        const int update_flags)
{
	const bool show_diffuse_color = (update_flags & GPU_PBVH_BUFFERS_SHOW_DIFFUSE_COLOR) != 0;
	const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
	int i, j, k, x, y;

	buffers->show_diffuse_color = show_diffuse_color;
	buffers->show_mask = show_mask;
	buffers->use_matcaps = GPU_material_use_matcaps_get();
	buffers->smooth = grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH;

	/* Build VBO */
	if (buffers->index_buf) {
		const int has_mask = key->has_mask;
		float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

		if (buffers->use_matcaps) {
			diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
		}
		else if (show_diffuse_color) {
			const DMFlagMat *flags = &grid_flag_mats[grid_indices[0]];

			GPU_material_diffuse_get(flags->mat_nr + 1, diffuse_color);
		}

		copy_v4_v4(buffers->diffuse_color, diffuse_color);

		Gwn_VertFormat format = {0};
		VertexBufferAttrID vbo_id;
		gpu_pbvh_vert_format_init__gwn(&format, &vbo_id);

		/* Build coord/normal VBO */
		GWN_VERTBUF_DISCARD_SAFE(buffers->vert_buf);
		buffers->vert_buf = GWN_vertbuf_create_with_format(&format);
		GWN_vertbuf_data_alloc(buffers->vert_buf, totgrid * key->grid_area);

		uint vbo_index_offset = 0;
		if (buffers->vert_buf->data) {
			for (i = 0; i < totgrid; ++i) {
				CCGElem *grid = grids[grid_indices[i]];
				int vbo_index = vbo_index_offset;

				for (y = 0; y < key->grid_size; y++) {
					for (x = 0; x < key->grid_size; x++) {
						CCGElem *elem = CCG_grid_elem(key, grid, x, y);
						GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.pos, vbo_index, CCG_elem_co(key, elem));

						if (buffers->smooth) {
							short no_short[3];
							normal_float_to_short_v3(no_short, CCG_elem_no(key, elem));
							GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.nor, vbo_index, no_short);

							if (has_mask && show_mask) {
								uchar color_ub[3];
								gpu_color_from_mask_copy(*CCG_elem_mask(key, elem),
									                     diffuse_color, color_ub);
								GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.col, vbo_index, color_ub);
							}
						}
						vbo_index += 1;
					}
				}
				
				if (!buffers->smooth) {
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

							vbo_index = vbo_index_offset + ((j + 1) * key->grid_size + k);
							short no_short[3];
							normal_float_to_short_v3(no_short, fno);
							GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.nor, vbo_index, no_short);

							if (has_mask) {
								uchar color_ub[3];
								gpu_color_from_mask_quad_copy(key,
								                              elems[0],
								                              elems[1],
								                              elems[2],
								                              elems[3],
								                              diffuse_color,
								                              color_ub);
								GWN_vertbuf_attr_set(buffers->vert_buf, vbo_id.col, vbo_index, color_ub);
							}
						}
					}
				}

				vbo_index_offset += key->grid_area;
			}

			gpu_pbvh_batch_init(buffers);
		}
		else {
			GWN_VERTBUF_DISCARD_SAFE(buffers->vert_buf);
		}
	}

	buffers->grids = grids;
	buffers->grid_indices = grid_indices;
	buffers->totgrid = totgrid;
	buffers->grid_flag_mats = grid_flag_mats;
	buffers->gridkey = *key;

	//printf("node updated %p\n", buffers);
}

/* Build the element array buffer of grid indices using either
 * unsigned shorts or unsigned ints. */
#define FILL_QUAD_BUFFER(max_vert_, tot_quad_, buffer_)                 \
    {                                                                   \
        int offset = 0;                                                 \
        int i, j, k;                                                    \
                                                                        \
        Gwn_IndexBufBuilder elb;                                        \
        GWN_indexbuf_init(                                              \
                &elb, GWN_PRIM_TRIS, tot_quad_ * 2, max_vert_);         \
                                                                        \
        /* Fill the buffer */                                           \
        for (i = 0; i < totgrid; ++i) {                                 \
            BLI_bitmap *gh = NULL;                                      \
            if (grid_hidden)                                            \
                gh = grid_hidden[(grid_indices)[i]];                    \
                                                                        \
            for (j = 0; j < gridsize - 1; ++j) {                        \
                for (k = 0; k < gridsize - 1; ++k) {                    \
                    /* Skip hidden grid face */                         \
                    if (gh && paint_is_grid_face_hidden(                \
                            gh, gridsize, k, j))                        \
                    {                                                   \
                        continue;                                       \
                    }                                                   \
                    GWN_indexbuf_add_generic_vert(&elb, offset + j * gridsize + k + 1); \
                    GWN_indexbuf_add_generic_vert(&elb, offset + j * gridsize + k);    \
                    GWN_indexbuf_add_generic_vert(&elb, offset + (j + 1) * gridsize + k); \
                                                                            \
                    GWN_indexbuf_add_generic_vert(&elb, offset + (j + 1) * gridsize + k + 1); \
                    GWN_indexbuf_add_generic_vert(&elb, offset + j * gridsize + k + 1); \
                    GWN_indexbuf_add_generic_vert(&elb, offset + (j + 1) * gridsize + k); \
                }                                                       \
            }                                                           \
                                                                        \
            offset += gridsize * gridsize;                              \
        }                                                               \
        buffer_ = GWN_indexbuf_build(&elb);                             \
    } (void)0
/* end FILL_QUAD_BUFFER */

static Gwn_IndexBuf *gpu_get_grid_buffer(
        int gridsize, unsigned *totquad, GridCommonGPUBuffer **grid_common_gpu_buffer,
        /* remove this arg  when gawain gets base-vertex support! */
        int totgrid)
{
	/* used in the FILL_QUAD_BUFFER macro */
	BLI_bitmap * const *grid_hidden = NULL;
	const int *grid_indices = NULL;
	// int totgrid = 1;

	GridCommonGPUBuffer *gridbuff = *grid_common_gpu_buffer;

	if (gridbuff == NULL) {
		*grid_common_gpu_buffer = gridbuff = MEM_mallocN(sizeof(GridCommonGPUBuffer), __func__);
		gridbuff->mres_buffer = NULL;
		gridbuff->mres_prev_gridsize = -1;
		gridbuff->mres_prev_totquad = 0;
	}

	/* VBO is already built */
	if (gridbuff->mres_buffer && gridbuff->mres_prev_gridsize == gridsize) {
		*totquad = gridbuff->mres_prev_totquad;
		return gridbuff->mres_buffer;
	}
	/* we can't reuse old, delete the existing buffer */
	else if (gridbuff->mres_buffer) {
		GWN_indexbuf_discard(gridbuff->mres_buffer);
		gridbuff->mres_buffer = NULL;
	}

	/* Build new VBO */
	*totquad = (gridsize - 1) * (gridsize - 1) * totgrid;
	int max_vert = gridsize * gridsize * totgrid;

	FILL_QUAD_BUFFER(max_vert, *totquad, gridbuff->mres_buffer);

	gridbuff->mres_prev_gridsize = gridsize;
	gridbuff->mres_prev_totquad = *totquad;
	return gridbuff->mres_buffer;
}

#define FILL_FAST_BUFFER() \
{ \
	Gwn_IndexBufBuilder elb; \
	GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, 6 * totgrid, INT_MAX); \
	for (int i = 0; i < totgrid; i++) { \
		GWN_indexbuf_add_generic_vert(&elb, i * gridsize * gridsize + gridsize - 1); \
		GWN_indexbuf_add_generic_vert(&elb, i * gridsize * gridsize); \
		GWN_indexbuf_add_generic_vert(&elb, (i + 1) * gridsize * gridsize - gridsize); \
		GWN_indexbuf_add_generic_vert(&elb, (i + 1) * gridsize * gridsize - 1); \
		GWN_indexbuf_add_generic_vert(&elb, i * gridsize * gridsize + gridsize - 1); \
		GWN_indexbuf_add_generic_vert(&elb, (i + 1) * gridsize * gridsize - gridsize); \
	} \
	buffers->index_buf_fast = GWN_indexbuf_build(&elb); \
} (void)0

GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(
        int *grid_indices, int totgrid, BLI_bitmap **grid_hidden, int gridsize, const CCGKey *UNUSED(key),
        GridCommonGPUBuffer **grid_common_gpu_buffer)
{
	GPU_PBVH_Buffers *buffers;
	int totquad;
	int fully_visible_totquad = (gridsize - 1) * (gridsize - 1) * totgrid;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->grid_hidden = grid_hidden;
	buffers->totgrid = totgrid;

	buffers->show_diffuse_color = false;
	buffers->show_mask = true;
	buffers->use_matcaps = false;

	/* Count the number of quads */
	totquad = BKE_pbvh_count_grid_quads(grid_hidden, grid_indices, totgrid, gridsize);

	/* totally hidden node, return here to avoid BufferData with zero below. */
	if (totquad == 0)
		return buffers;

	/* create and fill indices of the fast buffer too */
	FILL_FAST_BUFFER();

	if (totquad == fully_visible_totquad) {
		buffers->index_buf = gpu_get_grid_buffer(
		        gridsize, &buffers->tot_quad, grid_common_gpu_buffer, totgrid);
		buffers->has_hidden = false;
		buffers->is_index_buf_global = true;
	}
	else {
		uint max_vert = totgrid * gridsize * gridsize;
		buffers->tot_quad = totquad;

		FILL_QUAD_BUFFER(max_vert, totquad, buffers->index_buf);

		buffers->has_hidden = false;
		buffers->is_index_buf_global = false;
	}

#ifdef USE_BASE_ELEM
	/* Build coord/normal VBO */
	if (GLEW_ARB_draw_elements_base_vertex /* 3.2 */) {
		int i;
		buffers->baseelemarray = MEM_mallocN(sizeof(int) * totgrid * 2, "GPU_PBVH_Buffers.baseelemarray");
		buffers->baseindex = MEM_mallocN(sizeof(void *) * totgrid, "GPU_PBVH_Buffers.baseindex");
		for (i = 0; i < totgrid; i++) {
			buffers->baseelemarray[i] = buffers->tot_quad * 6;
			buffers->baseelemarray[i + totgrid] = i * key->grid_area;
			buffers->baseindex[i] = NULL;
		}
	}
#endif

	return buffers;
}

#undef FILL_QUAD_BUFFER

/* Output a BMVert into a VertexBufferFormat array
 *
 * The vertex is skipped if hidden, otherwise the output goes into
 * index '*v_index' in the 'vert_data' array and '*v_index' is
 * incremented.
 */
static void gpu_bmesh_vert_to_buffer_copy__gwn(
        BMVert *v,
        Gwn_VertBuf *vert_buf,
        const VertexBufferAttrID *vbo_id,
        int *v_index,
        const float fno[3],
        const float *fmask,
        const int cd_vert_mask_offset,
        const float diffuse_color[4],
        const bool show_mask)
{
	if (!BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {

		/* Set coord, normal, and mask */
		GWN_vertbuf_attr_set(vert_buf, vbo_id->pos, *v_index, v->co);

		{
			short no_short[3];
			normal_float_to_short_v3(no_short, fno ? fno : v->no);
			GWN_vertbuf_attr_set(vert_buf, vbo_id->nor, *v_index, no_short);
		}

		{
			uchar color_ub[3];
			float effective_mask;
			if (show_mask) {
				effective_mask = fmask ? *fmask
				                       : BM_ELEM_CD_GET_FLOAT(v, cd_vert_mask_offset);
			}
			else {
				effective_mask = 0.0f;
			}

			gpu_color_from_mask_copy(
			        effective_mask,
			        diffuse_color,
			        color_ub);
			GWN_vertbuf_attr_set(vert_buf, vbo_id->col, *v_index, color_ub);
		}

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
void GPU_pbvh_bmesh_buffers_update(
        GPU_PBVH_Buffers *buffers,
        BMesh *bm,
        GSet *bm_faces,
        GSet *bm_unique_verts,
        GSet *bm_other_verts,
        const int update_flags)
{
	const bool show_diffuse_color = (update_flags & GPU_PBVH_BUFFERS_SHOW_DIFFUSE_COLOR) != 0;
	const bool show_mask = (update_flags & GPU_PBVH_BUFFERS_SHOW_MASK) != 0;
	int tottri, totvert, maxvert = 0;
	float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

	/* TODO, make mask layer optional for bmesh buffer */
	const int cd_vert_mask_offset = CustomData_get_offset(&bm->vdata, CD_PAINT_MASK);

	buffers->show_diffuse_color = show_diffuse_color;
	buffers->show_mask = show_mask;
	buffers->use_matcaps = GPU_material_use_matcaps_get();

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
	GWN_VERTBUF_DISCARD_SAFE(buffers->vert_buf);
	/* match 'VertexBufferFormat' */
	Gwn_VertFormat format = {0};
	VertexBufferAttrID vbo_id;
	gpu_pbvh_vert_format_init__gwn(&format, &vbo_id);

	buffers->vert_buf = GWN_vertbuf_create_with_format(&format);
	GWN_vertbuf_data_alloc(buffers->vert_buf, totvert);

	/* Fill vertex buffer */
	if (buffers->vert_buf->data) {
		int v_index = 0;

		if (buffers->smooth) {
			GSetIterator gs_iter;

			/* Vertices get an index assigned for use in the triangle
			 * index buffer */
			bm->elem_index_dirty |= BM_VERT;

			GSET_ITER (gs_iter, bm_unique_verts) {
				gpu_bmesh_vert_to_buffer_copy__gwn(
				        BLI_gsetIterator_getKey(&gs_iter),
				        buffers->vert_buf, &vbo_id, &v_index, NULL, NULL,
				        cd_vert_mask_offset, diffuse_color,
				        show_mask);
			}

			GSET_ITER (gs_iter, bm_other_verts) {
				gpu_bmesh_vert_to_buffer_copy__gwn(
				        BLI_gsetIterator_getKey(&gs_iter),
				        buffers->vert_buf, &vbo_id, &v_index, NULL, NULL,
				        cd_vert_mask_offset, diffuse_color,
				        show_mask);
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

#if 0
					BM_iter_as_array(bm, BM_VERTS_OF_FACE, f, (void**)v, 3);
#endif
					BM_face_as_array_vert_tri(f, v);

					/* Average mask value */
					for (i = 0; i < 3; i++) {
						fmask += BM_ELEM_CD_GET_FLOAT(v[i], cd_vert_mask_offset);
					}
					fmask /= 3.0f;
					
					for (i = 0; i < 3; i++) {
						gpu_bmesh_vert_to_buffer_copy__gwn(
						        v[i], buffers->vert_buf, &vbo_id,
						        &v_index, f->no, &fmask,
						        cd_vert_mask_offset, diffuse_color,
						        show_mask);
					}
				}
			}

			buffers->tot_tri = tottri;
		}

		/* gpu_bmesh_vert_to_buffer_copy sets dirty index values */
		bm->elem_index_dirty |= BM_VERT;
	}
	else {
		GWN_VERTBUF_DISCARD_SAFE(buffers->vert_buf);
		/* Memory map failed */
		return;
	}

	if (buffers->smooth) {
		/* Fill the triangle buffer */
		buffers->index_buf = NULL;
		Gwn_IndexBufBuilder elb;
		GWN_indexbuf_init(&elb, GWN_PRIM_TRIS, tottri, maxvert);

		/* Initialize triangle index buffer */
		if (buffers->triangles && !buffers->is_index_buf_global) {
			GWN_BATCH_DISCARD_SAFE(buffers->triangles);
		}
		buffers->is_index_buf_global = false;

		/* Fill triangle index buffer */

		{
			GSetIterator gs_iter;

			GSET_ITER (gs_iter, bm_faces) {
				BMFace *f = BLI_gsetIterator_getKey(&gs_iter);

				if (!BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
					BMLoop *l_iter;
					BMLoop *l_first;

					l_iter = l_first = BM_FACE_FIRST_LOOP(f);
					do {
						GWN_indexbuf_add_generic_vert(&elb, BM_elem_index_get(l_iter->v));
					} while ((l_iter = l_iter->next) != l_first);
				}
			}

			buffers->tot_tri = tottri;

			buffers->index_buf = GWN_indexbuf_build(&elb);
		}
	}
	else if (buffers->index_buf) {
		if (!buffers->is_index_buf_global) {
			GWN_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
		}
		buffers->index_buf = NULL;
		buffers->is_index_buf_global = false;
	}

	gpu_pbvh_batch_init(buffers);
}

GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading)
{
	GPU_PBVH_Buffers *buffers;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->use_bmesh = true;
	buffers->smooth = smooth_shading;
	buffers->show_diffuse_color = false;
	buffers->show_mask = true;
	buffers->use_matcaps = false;

	return buffers;
}

void GPU_pbvh_buffers_draw(
        GPU_PBVH_Buffers *buffers, DMSetMaterial setMaterial,
        bool wireframe, bool fast)
{
	UNUSED_VARS(wireframe, fast, setMaterial);
	bool do_fast = fast && buffers->triangles_fast;
	Gwn_Batch *triangles = do_fast ? buffers->triangles_fast : buffers->triangles;

	if (triangles) {

		/* Simple Shader: use when drawing without the draw-manager (old 2.7x viewport) */
		if (triangles->interface == NULL) {
			GPUBuiltinShader shader_id =
			        buffers->smooth ? GPU_SHADER_SIMPLE_LIGHTING_SMOOTH_COLOR : GPU_SHADER_SIMPLE_LIGHTING_FLAT_COLOR;
			GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);

			GWN_batch_program_set(
			        triangles,
			        GPU_shader_get_program(shader), GPU_shader_get_interface(shader));

			static float light[3] = {-0.3f, 0.5f, 1.0f};
			static float alpha = 1.0f;
			static float world_light = 1.0f;

			GPU_shader_uniform_vector(shader, GPU_shader_get_uniform(shader, "light"), 3, 1, light);
			GPU_shader_uniform_vector(shader, GPU_shader_get_uniform(shader, "alpha"), 1, 1, &alpha);
			GPU_shader_uniform_vector(shader, GPU_shader_get_uniform(shader, "global"), 1, 1, &world_light);

		}
		GWN_batch_draw(triangles);
	}
}

Gwn_Batch *GPU_pbvh_buffers_batch_get(GPU_PBVH_Buffers *buffers, bool fast)
{
	return (fast && buffers->triangles_fast) ?
	        buffers->triangles_fast : buffers->triangles;
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

	if (buffers->looptri) {
		const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
		const MPoly *mp = &buffers->mpoly[lt->poly];

		GPU_material_diffuse_get(mp->mat_nr + 1, diffuse_color);
	}
	else if (buffers->use_bmesh) {
		/* due to dynamic nature of dyntopo, only get first material */
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

bool GPU_pbvh_buffers_mask_changed(GPU_PBVH_Buffers *buffers, bool show_mask)
{
	return (buffers->show_mask != show_mask);
}

void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers)
{
	if (buffers) {
		GWN_BATCH_DISCARD_SAFE(buffers->triangles);
		GWN_BATCH_DISCARD_SAFE(buffers->triangles_fast);
		if (!buffers->is_index_buf_global) {
			GWN_INDEXBUF_DISCARD_SAFE(buffers->index_buf);
		}
		GWN_INDEXBUF_DISCARD_SAFE(buffers->index_buf_fast);
		GWN_vertbuf_discard(buffers->vert_buf);

#ifdef USE_BASE_ELEM
		if (buffers->baseelemarray)
			MEM_freeN(buffers->baseelemarray);
		if (buffers->baseindex)
			MEM_freeN(buffers->baseindex);
#endif

		MEM_freeN(buffers);
	}
}

void GPU_pbvh_multires_buffers_free(GridCommonGPUBuffer **grid_common_gpu_buffer)
{
	GridCommonGPUBuffer *gridbuff = *grid_common_gpu_buffer;

	if (gridbuff) {
		if (gridbuff->mres_buffer) {
			BLI_mutex_lock(&buffer_mutex);
			GWN_INDEXBUF_DISCARD_SAFE(gridbuff->mres_buffer);
			BLI_mutex_unlock(&buffer_mutex);
		}
		MEM_freeN(gridbuff);
		*grid_common_gpu_buffer = NULL;
	}
}

/* debug function, draws the pbvh BB */
void GPU_pbvh_BB_draw(float min[3], float max[3], bool leaf, unsigned int pos)
{
	if (leaf)
		immUniformColor4f(0.0, 1.0, 0.0, 0.5);
	else
		immUniformColor4f(1.0, 0.0, 0.0, 0.5);

	/* TODO(merwin): revisit this after we have mutable VertexBuffers
	 * could keep a static batch & index buffer, change the VBO contents per draw
	 */

	immBegin(GWN_PRIM_LINES, 24);

	/* top */
	immVertex3f(pos, min[0], min[1], max[2]);
	immVertex3f(pos, min[0], max[1], max[2]);

	immVertex3f(pos, min[0], max[1], max[2]);
	immVertex3f(pos, max[0], max[1], max[2]);

	immVertex3f(pos, max[0], max[1], max[2]);
	immVertex3f(pos, max[0], min[1], max[2]);

	immVertex3f(pos, max[0], min[1], max[2]);
	immVertex3f(pos, min[0], min[1], max[2]);

	/* bottom */
	immVertex3f(pos, min[0], min[1], min[2]);
	immVertex3f(pos, min[0], max[1], min[2]);

	immVertex3f(pos, min[0], max[1], min[2]);
	immVertex3f(pos, max[0], max[1], min[2]);

	immVertex3f(pos, max[0], max[1], min[2]);
	immVertex3f(pos, max[0], min[1], min[2]);

	immVertex3f(pos, max[0], min[1], min[2]);
	immVertex3f(pos, min[0], min[1], min[2]);

	/* sides */
	immVertex3f(pos, min[0], min[1], min[2]);
	immVertex3f(pos, min[0], min[1], max[2]);

	immVertex3f(pos, min[0], max[1], min[2]);
	immVertex3f(pos, min[0], max[1], max[2]);

	immVertex3f(pos, max[0], max[1], min[2]);
	immVertex3f(pos, max[0], max[1], max[2]);

	immVertex3f(pos, max[0], min[1], min[2]);
	immVertex3f(pos, max[0], min[1], max[2]);

	immEnd();
}
