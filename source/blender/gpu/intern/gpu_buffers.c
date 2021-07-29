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

#include "GPU_glew.h"

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
#include "GPU_basic_shader.h"

#include "bmesh.h"

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
    {GL_ARRAY_BUFFER, 3},
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
	GPUBuffer *mres_buffer;
	int mres_prev_gridsize;
	GLenum mres_prev_index_type;
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
	glColorPointer(3, GL_UNSIGNED_BYTE, 0, 0);

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
	GPUBuffer *vert_buf, *index_buf, *index_buf_fast;
	GLenum index_type;

	int *baseelemarray;
	void **baseindex;

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
	bool use_matcaps;
	float diffuse_color[4];
};

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
        const int (*face_vert_indices)[3], bool show_diffuse_color)
{
	VertexBufferFormat *vert_data;
	int i;

	buffers->vmask = vmask;
	buffers->show_diffuse_color = show_diffuse_color;
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
		if (buffers->vert_buf)
			GPU_buffer_free(buffers->vert_buf);
		buffers->vert_buf = GPU_buffer_alloc(sizeof(VertexBufferFormat) * totelem);
		vert_data = GPU_buffer_lock(buffers->vert_buf, GPU_BINDING_ARRAY);

		if (vert_data) {
			/* Vertex data is shared if smooth-shaded, but separate
			 * copies are made for flat shading because normals
			 * shouldn't be shared. */
			if (buffers->smooth) {
				for (i = 0; i < totvert; ++i) {
					const MVert *v = &mvert[vert_indices[i]];
					VertexBufferFormat *out = vert_data + i;

					copy_v3_v3(out->co, v->co);
					memcpy(out->no, v->no, sizeof(short) * 3);
				}

				for (i = 0; i < buffers->face_indices_len; i++) {
					const MLoopTri *lt = &buffers->looptri[buffers->face_indices[i]];
					for (uint j = 0; j < 3; j++) {
						VertexBufferFormat *out = vert_data + face_vert_indices[i][j];

						if (vmask) {
							uint v_index = buffers->mloop[lt->tri[j]].v;
							gpu_color_from_mask_copy(vmask[v_index], diffuse_color, out->color);
						}
						else {
							copy_v3_v3_uchar(out->color, diffuse_color_ub);
						}
					}
				}
			}
			else {
				/* calculate normal for each polygon only once */
				unsigned int mpoly_prev = UINT_MAX;
				short no[3];

				for (i = 0; i < buffers->face_indices_len; ++i) {
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
					if (vmask) {
						float fmask = (vmask[vtri[0]] + vmask[vtri[1]] + vmask[vtri[2]]) / 3.0f;
						gpu_color_from_mask_copy(fmask, diffuse_color, color_ub);
					}
					else {
						copy_v3_v3_uchar(color_ub, diffuse_color_ub);
					}

					for (uint j = 0; j < 3; j++) {
						const MVert *v = &mvert[vtri[j]];
						VertexBufferFormat *out = vert_data;

						copy_v3_v3(out->co, v->co);
						copy_v3_v3_short(out->no, no);
						copy_v3_v3_uchar(out->color, color_ub);

						vert_data++;
					}
				}
			}

			GPU_buffer_unlock(buffers->vert_buf, GPU_BINDING_ARRAY);
		}
		else {
			GPU_buffer_free(buffers->vert_buf);
			buffers->vert_buf = NULL;
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
	unsigned short *tri_data;
	int i, j, tottri;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->index_type = GL_UNSIGNED_SHORT;
	buffers->smooth = mpoly[looptri[face_indices[0]].poly].flag & ME_SMOOTH;

	buffers->show_diffuse_color = false;
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
		buffers->index_buf = GPU_buffer_alloc(sizeof(unsigned short) * tottri * 3);
		buffers->is_index_buf_global = false;
	}

	if (buffers->index_buf) {
		/* Fill the triangle buffer */
		tri_data = GPU_buffer_lock(buffers->index_buf, GPU_BINDING_INDEX);
		if (tri_data) {
			for (i = 0; i < face_indices_len; ++i) {
				const MLoopTri *lt = &looptri[face_indices[i]];

				/* Skip hidden faces */
				if (paint_is_face_hidden(lt, mvert, mloop))
					continue;

				for (j = 0; j < 3; ++j) {
					*tri_data = face_vert_indices[i][j];
					tri_data++;
				}
			}
			GPU_buffer_unlock(buffers->index_buf, GPU_BINDING_INDEX);
		}
		else {
			if (!buffers->is_index_buf_global) {
				GPU_buffer_free(buffers->index_buf);
			}
			buffers->index_buf = NULL;
			buffers->is_index_buf_global = false;
		}
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
        int totgrid, const CCGKey *key, bool show_diffuse_color)
{
	VertexBufferFormat *vert_data;
	int i, j, k, x, y;

	buffers->show_diffuse_color = show_diffuse_color;
	buffers->use_matcaps = GPU_material_use_matcaps_get();
	buffers->smooth = grid_flag_mats[grid_indices[0]].flag & ME_SMOOTH;

	/* Build VBO */
	if (buffers->vert_buf) {
		const int has_mask = key->has_mask;
		float diffuse_color[4] = {0.8f, 0.8f, 0.8f, 1.0f};

		if (buffers->use_matcaps)
			diffuse_color[0] = diffuse_color[1] = diffuse_color[2] = 1.0;
		else if (show_diffuse_color) {
			const DMFlagMat *flags = &grid_flag_mats[grid_indices[0]];

			GPU_material_diffuse_get(flags->mat_nr + 1, diffuse_color);
		}

		copy_v4_v4(buffers->diffuse_color, diffuse_color);

		vert_data = GPU_buffer_lock_stream(buffers->vert_buf, GPU_BINDING_ARRAY);
		if (vert_data) {
			for (i = 0; i < totgrid; ++i) {
				VertexBufferFormat *vd = vert_data;
				CCGElem *grid = grids[grid_indices[i]];

				for (y = 0; y < key->grid_size; y++) {
					for (x = 0; x < key->grid_size; x++) {
						CCGElem *elem = CCG_grid_elem(key, grid, x, y);
						
						copy_v3_v3(vd->co, CCG_elem_co(key, elem));
						if (buffers->smooth) {
							normal_float_to_short_v3(vd->no, CCG_elem_no(key, elem));

							if (has_mask) {
								gpu_color_from_mask_copy(*CCG_elem_mask(key, elem),
								                         diffuse_color, vd->color);
							}
						}
						vd++;
					}
				}
				
				if (!buffers->smooth) {
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

			GPU_buffer_unlock(buffers->vert_buf, GPU_BINDING_ARRAY);
		}
		else {
			GPU_buffer_free(buffers->vert_buf);
			buffers->vert_buf = NULL;
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
#define FILL_QUAD_BUFFER(type_, tot_quad_, buffer_)                     \
    {                                                                   \
        type_ *tri_data;                                                \
        int offset = 0;                                                 \
        int i, j, k;                                                    \
        buffer_ = GPU_buffer_alloc(sizeof(type_) * (tot_quad_) * 6);    \
                                                                        \
        /* Fill the buffer */                                           \
        tri_data = GPU_buffer_lock(buffer_, GPU_BINDING_INDEX);         \
        if (tri_data) {                                                 \
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
                            continue;                                    \
                                                                          \
                        *(tri_data++) = offset + j * gridsize + k + 1;     \
                        *(tri_data++) = offset + j * gridsize + k;          \
                        *(tri_data++) = offset + (j + 1) * gridsize + k;     \
                                                                             \
                        *(tri_data++) = offset + (j + 1) * gridsize + k + 1; \
                        *(tri_data++) = offset + j * gridsize + k + 1;       \
                        *(tri_data++) = offset + (j + 1) * gridsize + k;    \
                    }                                                      \
                }                                                         \
                                                                         \
                offset += gridsize * gridsize;                          \
            }                                                           \
            GPU_buffer_unlock(buffer_, GPU_BINDING_INDEX);                         \
        }                                                               \
        else {                                                          \
            GPU_buffer_free(buffer_);                                   \
            (buffer_) = NULL;                                           \
        }                                                               \
    } (void)0
/* end FILL_QUAD_BUFFER */

static GPUBuffer *gpu_get_grid_buffer(
        int gridsize, GLenum *index_type, unsigned *totquad, GridCommonGPUBuffer **grid_common_gpu_buffer)
{
	/* used in the FILL_QUAD_BUFFER macro */
	BLI_bitmap * const *grid_hidden = NULL;
	const int *grid_indices = NULL;
	int totgrid = 1;

	GridCommonGPUBuffer *gridbuff = *grid_common_gpu_buffer;

	if (gridbuff == NULL) {
		*grid_common_gpu_buffer = gridbuff = MEM_mallocN(sizeof(GridCommonGPUBuffer), __func__);
		gridbuff->mres_buffer = NULL;
		gridbuff->mres_prev_gridsize = -1;
		gridbuff->mres_prev_index_type = 0;
		gridbuff->mres_prev_totquad = 0;
	}

	/* VBO is already built */
	if (gridbuff->mres_buffer && gridbuff->mres_prev_gridsize == gridsize) {
		*index_type = gridbuff->mres_prev_index_type;
		*totquad = gridbuff->mres_prev_totquad;
		return gridbuff->mres_buffer;
	}
	/* we can't reuse old, delete the existing buffer */
	else if (gridbuff->mres_buffer) {
		GPU_buffer_free(gridbuff->mres_buffer);
	}

	/* Build new VBO */
	*totquad = (gridsize - 1) * (gridsize - 1);

	if (gridsize * gridsize < USHRT_MAX) {
		*index_type = GL_UNSIGNED_SHORT;
		FILL_QUAD_BUFFER(unsigned short, *totquad, gridbuff->mres_buffer);
	}
	else {
		*index_type = GL_UNSIGNED_INT;
		FILL_QUAD_BUFFER(unsigned int, *totquad, gridbuff->mres_buffer);
	}

	gridbuff->mres_prev_gridsize = gridsize;
	gridbuff->mres_prev_index_type = *index_type;
	gridbuff->mres_prev_totquad = *totquad;
	return gridbuff->mres_buffer;
}

#define FILL_FAST_BUFFER(type_) \
{ \
	type_ *buffer; \
	buffers->index_buf_fast = GPU_buffer_alloc(sizeof(type_) * 6 * totgrid); \
	buffer = GPU_buffer_lock(buffers->index_buf_fast, GPU_BINDING_INDEX); \
	if (buffer) { \
		int i; \
		for (i = 0; i < totgrid; i++) { \
			int currentquad = i * 6; \
			buffer[currentquad]     = i * gridsize * gridsize + gridsize - 1; \
			buffer[currentquad + 1] = i * gridsize * gridsize; \
			buffer[currentquad + 2] = (i + 1) * gridsize * gridsize - gridsize; \
			buffer[currentquad + 3] = (i + 1) * gridsize * gridsize - 1; \
			buffer[currentquad + 4] = i * gridsize * gridsize + gridsize - 1; \
			buffer[currentquad + 5] = (i + 1) * gridsize * gridsize - gridsize; \
		} \
		GPU_buffer_unlock(buffers->index_buf_fast, GPU_BINDING_INDEX); \
	} \
	else { \
		GPU_buffer_free(buffers->index_buf_fast); \
		buffers->index_buf_fast = NULL; \
	} \
} (void)0

GPU_PBVH_Buffers *GPU_pbvh_grid_buffers_build(
        int *grid_indices, int totgrid, BLI_bitmap **grid_hidden, int gridsize, const CCGKey *key,
        GridCommonGPUBuffer **grid_common_gpu_buffer)
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

	/* create and fill indices of the fast buffer too */
	if (totgrid * gridsize * gridsize < USHRT_MAX) {
		FILL_FAST_BUFFER(unsigned short);
	}
	else {
		FILL_FAST_BUFFER(unsigned int);
	}

	if (totquad == fully_visible_totquad) {
		buffers->index_buf = gpu_get_grid_buffer(
		                         gridsize, &buffers->index_type, &buffers->tot_quad, grid_common_gpu_buffer);
		buffers->has_hidden = false;
		buffers->is_index_buf_global = true;
	}
	else {
		buffers->tot_quad = totquad;

		if (totgrid * gridsize * gridsize < USHRT_MAX) {
			buffers->index_type = GL_UNSIGNED_SHORT;
			FILL_QUAD_BUFFER(unsigned short, totquad, buffers->index_buf);
		}
		else {
			buffers->index_type = GL_UNSIGNED_INT;
			FILL_QUAD_BUFFER(unsigned int, totquad, buffers->index_buf);
		}

		buffers->has_hidden = true;
		buffers->is_index_buf_global = false;
	}

	/* Build coord/normal VBO */
	if (buffers->index_buf)
		buffers->vert_buf = GPU_buffer_alloc(sizeof(VertexBufferFormat) * totgrid * key->grid_area);

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
void GPU_pbvh_bmesh_buffers_update(
        GPU_PBVH_Buffers *buffers,
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
	if (buffers->vert_buf)
		GPU_buffer_free(buffers->vert_buf);
	buffers->vert_buf = GPU_buffer_alloc(sizeof(VertexBufferFormat) * totvert);

	/* Fill vertex buffer */
	vert_data = GPU_buffer_lock(buffers->vert_buf, GPU_BINDING_ARRAY);
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
						gpu_bmesh_vert_to_buffer_copy(v[i], vert_data,
						                              &v_index, f->no, &fmask,
						                              cd_vert_mask_offset, diffuse_color);
					}
				}
			}

			buffers->tot_tri = tottri;
		}

		GPU_buffer_unlock(buffers->vert_buf, GPU_BINDING_ARRAY);

		/* gpu_bmesh_vert_to_buffer_copy sets dirty index values */
		bm->elem_index_dirty |= BM_VERT;
	}
	else {
		/* Memory map failed */
		GPU_buffer_free(buffers->vert_buf);
		buffers->vert_buf = NULL;
		return;
	}

	if (buffers->smooth) {
		const int use_short = (maxvert < USHRT_MAX);

		/* Initialize triangle index buffer */
		if (buffers->index_buf && !buffers->is_index_buf_global)
			GPU_buffer_free(buffers->index_buf);
		buffers->is_index_buf_global = false;
		buffers->index_buf = GPU_buffer_alloc((use_short ?
		                                      sizeof(unsigned short) :
		                                      sizeof(unsigned int)) * 3 * tottri);

		/* Fill triangle index buffer */
		tri_data = GPU_buffer_lock(buffers->index_buf, GPU_BINDING_INDEX);
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

			GPU_buffer_unlock(buffers->index_buf, GPU_BINDING_INDEX);

			buffers->tot_tri = tottri;
			buffers->index_type = (use_short ?
			                       GL_UNSIGNED_SHORT :
			                       GL_UNSIGNED_INT);
		}
		else {
			/* Memory map failed */
			if (!buffers->is_index_buf_global) {
				GPU_buffer_free(buffers->index_buf);
			}
			buffers->index_buf = NULL;
			buffers->is_index_buf_global = false;
		}
	}
	else if (buffers->index_buf) {
		if (!buffers->is_index_buf_global) {
			GPU_buffer_free(buffers->index_buf);
		}
		buffers->index_buf = NULL;
		buffers->is_index_buf_global = false;
	}
}

GPU_PBVH_Buffers *GPU_pbvh_bmesh_buffers_build(bool smooth_shading)
{
	GPU_PBVH_Buffers *buffers;

	buffers = MEM_callocN(sizeof(GPU_PBVH_Buffers), "GPU_Buffers");
	buffers->use_bmesh = true;
	buffers->smooth = smooth_shading;
	buffers->show_diffuse_color = false;
	buffers->use_matcaps = false;

	return buffers;
}

void GPU_pbvh_buffers_draw(
        GPU_PBVH_Buffers *buffers, DMSetMaterial setMaterial,
        bool wireframe, bool fast)
{
	bool do_fast = fast && buffers->index_buf_fast;
	/* sets material from the first face, to solve properly face would need to
	 * be sorted in buckets by materials */
	if (setMaterial) {
		if (buffers->face_indices_len) {
			const MLoopTri *lt = &buffers->looptri[buffers->face_indices[0]];
			const MPoly *mp = &buffers->mpoly[lt->poly];
			if (!setMaterial(mp->mat_nr + 1, NULL))
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

	if (buffers->vert_buf) {
		char *base = NULL;
		char *index_base = NULL;
		/* weak inspection of bound options, should not be necessary ideally */
		const int bound_options_old = GPU_basic_shader_bound_options();
		int bound_options_new = 0;
		glEnableClientState(GL_VERTEX_ARRAY);
		if (!wireframe) {
			glEnableClientState(GL_NORMAL_ARRAY);
			glEnableClientState(GL_COLOR_ARRAY);

			bound_options_new |= GPU_SHADER_USE_COLOR;
		}

		GPU_buffer_bind(buffers->vert_buf, GPU_BINDING_ARRAY);

		if (do_fast) {
			GPU_buffer_bind(buffers->index_buf_fast, GPU_BINDING_INDEX);
		}
		else if (buffers->index_buf) {
			GPU_buffer_bind(buffers->index_buf, GPU_BINDING_INDEX);
		}

		if (wireframe) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else {
			if ((buffers->smooth == false) && (buffers->face_indices_len == 0)) {
				bound_options_new |= GPU_SHADER_FLAT_NORMAL;
			}
		}

		if (bound_options_new & ~bound_options_old) {
			GPU_basic_shader_bind(bound_options_old | bound_options_new);
		}

		if (buffers->tot_quad) {
			const char *offset = base;
			const bool drawall = !(buffers->has_hidden || do_fast);

			if (GLEW_ARB_draw_elements_base_vertex && drawall) {

				glVertexPointer(3, GL_FLOAT, sizeof(VertexBufferFormat),
				                offset + offsetof(VertexBufferFormat, co));
				if (!wireframe) {
					glNormalPointer(GL_SHORT, sizeof(VertexBufferFormat),
					                offset + offsetof(VertexBufferFormat, no));
					glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(VertexBufferFormat),
					               offset + offsetof(VertexBufferFormat, color));
				}

				glMultiDrawElementsBaseVertex(GL_TRIANGLES, buffers->baseelemarray, buffers->index_type,
				                              (const void * const *)buffers->baseindex,
				                              buffers->totgrid, &buffers->baseelemarray[buffers->totgrid]);
			}
			else {
				int i, last = drawall ? buffers->totgrid : 1;

				/* we could optimize this to one draw call, but it would need more memory */
				for (i = 0; i < last; i++) {
					glVertexPointer(3, GL_FLOAT, sizeof(VertexBufferFormat),
					                offset + offsetof(VertexBufferFormat, co));
					if (!wireframe) {
						glNormalPointer(GL_SHORT, sizeof(VertexBufferFormat),
						                offset + offsetof(VertexBufferFormat, no));
						glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(VertexBufferFormat),
						               offset + offsetof(VertexBufferFormat, color));
					}

					if (do_fast)
						glDrawElements(GL_TRIANGLES, buffers->totgrid * 6, buffers->index_type, index_base);
					else
						glDrawElements(GL_TRIANGLES, buffers->tot_quad * 6, buffers->index_type, index_base);

					offset += buffers->gridkey.grid_area * sizeof(VertexBufferFormat);
				}
			}
		}
		else if (buffers->tot_tri) {
			int totelem = buffers->tot_tri * 3;

			glVertexPointer(3, GL_FLOAT, sizeof(VertexBufferFormat),
			                (void *)(base + offsetof(VertexBufferFormat, co)));

			if (!wireframe) {
				glNormalPointer(GL_SHORT, sizeof(VertexBufferFormat),
				                (void *)(base + offsetof(VertexBufferFormat, no)));
				glColorPointer(3, GL_UNSIGNED_BYTE, sizeof(VertexBufferFormat),
				               (void *)(base + offsetof(VertexBufferFormat, color)));
			}

			if (buffers->index_buf)
				glDrawElements(GL_TRIANGLES, totelem, buffers->index_type, index_base);
			else
				glDrawArrays(GL_TRIANGLES, 0, totelem);
		}

		if (wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		GPU_buffer_unbind(buffers->vert_buf, GPU_BINDING_ARRAY);
		if (buffers->index_buf || do_fast)
			GPU_buffer_unbind(do_fast ? buffers->index_buf_fast : buffers->index_buf, GPU_BINDING_INDEX);

		glDisableClientState(GL_VERTEX_ARRAY);
		if (!wireframe) {
			glDisableClientState(GL_NORMAL_ARRAY);
			glDisableClientState(GL_COLOR_ARRAY);
		}

		if (bound_options_new & ~bound_options_old) {
			GPU_basic_shader_bind(bound_options_old);
		}
	}
}

bool GPU_pbvh_buffers_diffuse_changed(
        GPU_PBVH_Buffers *buffers, GSet *bm_faces, bool show_diffuse_color)
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

void GPU_pbvh_buffers_free(GPU_PBVH_Buffers *buffers)
{
	if (buffers) {
		if (buffers->vert_buf)
			GPU_buffer_free(buffers->vert_buf);
		if (buffers->index_buf && !buffers->is_index_buf_global)
			GPU_buffer_free(buffers->index_buf);
		if (buffers->index_buf_fast)
			GPU_buffer_free(buffers->index_buf_fast);
		if (buffers->baseelemarray)
			MEM_freeN(buffers->baseelemarray);
		if (buffers->baseindex)
			MEM_freeN(buffers->baseindex);

		MEM_freeN(buffers);
	}
}

void GPU_pbvh_multires_buffers_free(GridCommonGPUBuffer **grid_common_gpu_buffer)
{
	GridCommonGPUBuffer *gridbuff = *grid_common_gpu_buffer;

	if (gridbuff) {
		if (gridbuff->mres_buffer) {
			BLI_mutex_lock(&buffer_mutex);
			gpu_buffer_free_intern(gridbuff->mres_buffer);
			BLI_mutex_unlock(&buffer_mutex);
		}
		MEM_freeN(gridbuff);
		*grid_common_gpu_buffer = NULL;
	}
}

/* debug function, draws the pbvh BB */
void GPU_pbvh_BB_draw(float min[3], float max[3], bool leaf)
{
	const float quads[4][4][3] = {
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

void GPU_pbvh_BB_draw_init(void)
{
	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_CULL_FACE);
	glEnableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glEnable(GL_BLEND);
}

void GPU_pbvh_BB_draw_end(void)
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glPopAttrib();
}
