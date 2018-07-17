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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/gwn_vertex_buffer.h
 *  \ingroup gpu
 *
 * Gawain vertex buffer
 */

#ifndef __GWN_VERTEX_BUFFER_H__
#define __GWN_VERTEX_BUFFER_H__

#include "gwn_vertex_format.h"

#define VRAM_USAGE 1
/* How to create a Gwn_VertBuf: */
/* 1) verts = GWN_vertbuf_create() or GWN_vertbuf_init(verts) */
/* 2) GWN_vertformat_attr_add(verts->format, ...) */
/* 3) GWN_vertbuf_data_alloc(verts, vertex_len) <-- finalizes/packs vertex format */
/* 4) GWN_vertbuf_attr_fill(verts, pos, application_pos_buffer) */

/* Is Gwn_VertBuf always used as part of a Gwn_Batch? */

typedef enum {
	/* can be extended to support more types */
	GWN_USAGE_STREAM,
	GWN_USAGE_STATIC, /* do not keep data in memory */
	GWN_USAGE_DYNAMIC
} Gwn_UsageType;

typedef struct Gwn_VertBuf {
	Gwn_VertFormat format;
	uint vertex_len;    /* number of verts we want to draw */
	uint vertex_alloc;  /* number of verts data */
	bool dirty;
	unsigned char* data; /* NULL indicates data in VRAM (unmapped) */
	uint32_t vbo_id; /* 0 indicates not yet allocated */
	Gwn_UsageType usage; /* usage hint for GL optimisation */
} Gwn_VertBuf;

Gwn_VertBuf* GWN_vertbuf_create(Gwn_UsageType);
Gwn_VertBuf* GWN_vertbuf_create_with_format_ex(const Gwn_VertFormat*, Gwn_UsageType);

#define GWN_vertbuf_create_with_format(format) \
	GWN_vertbuf_create_with_format_ex(format, GWN_USAGE_STATIC)

void GWN_vertbuf_discard(Gwn_VertBuf*);

void GWN_vertbuf_init(Gwn_VertBuf*, Gwn_UsageType);
void GWN_vertbuf_init_with_format_ex(Gwn_VertBuf*, const Gwn_VertFormat*, Gwn_UsageType);

#define GWN_vertbuf_init_with_format(verts, format) \
	GWN_vertbuf_init_with_format_ex(verts, format, GWN_USAGE_STATIC)

uint GWN_vertbuf_size_get(const Gwn_VertBuf*);
void GWN_vertbuf_data_alloc(Gwn_VertBuf*, uint v_len);
void GWN_vertbuf_data_resize(Gwn_VertBuf*, uint v_len);
void GWN_vertbuf_vertex_count_set(Gwn_VertBuf*, uint v_len);

/* The most important set_attrib variant is the untyped one. Get it right first. */
/* It takes a void* so the app developer is responsible for matching their app data types */
/* to the vertex attribute's type and component count. They're in control of both, so this */
/* should not be a problem. */

void GWN_vertbuf_attr_set(Gwn_VertBuf*, uint a_idx, uint v_idx, const void* data);
void GWN_vertbuf_attr_fill(Gwn_VertBuf*, uint a_idx, const void* data); /* tightly packed, non interleaved input data */
void GWN_vertbuf_attr_fill_stride(Gwn_VertBuf*, uint a_idx, uint stride, const void* data);

/* For low level access only */
typedef struct Gwn_VertBufRaw {
	uint size;
	uint stride;
	unsigned char* data;
	unsigned char* data_init;
#if TRUST_NO_ONE
	/* Only for overflow check */
	unsigned char* _data_end;
#endif
} Gwn_VertBufRaw;

GWN_INLINE void *GWN_vertbuf_raw_step(Gwn_VertBufRaw *a)
{
	unsigned char* data = a->data;
	a->data += a->stride;
#if TRUST_NO_ONE
	assert(data < a->_data_end);
#endif
	return (void *)data;
}

GWN_INLINE uint GWN_vertbuf_raw_used(Gwn_VertBufRaw *a)
{
	return ((a->data - a->data_init) / a->stride);
}

void GWN_vertbuf_attr_get_raw_data(Gwn_VertBuf*, uint a_idx, Gwn_VertBufRaw *access);

/* TODO: decide whether to keep the functions below */
/* doesn't immediate mode satisfy these needs? */

/*	void setAttrib1f(uint a_idx, uint v_idx, float x); */
/*	void setAttrib2f(uint a_idx, unsigned v_idx, float x, float y); */
/*	void setAttrib3f(unsigned a_idx, unsigned v_idx, float x, float y, float z); */
/*	void setAttrib4f(unsigned a_idx, unsigned v_idx, float x, float y, float z, float w); */

/*	void setAttrib3ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b); */
/*	void setAttrib4ub(unsigned a_idx, unsigned v_idx, unsigned char r, unsigned char g, unsigned char b, unsigned char a); */

void GWN_vertbuf_use(Gwn_VertBuf*);

/* Metrics */
uint GWN_vertbuf_get_memory_usage(void);

/* Macros */
#define GWN_VERTBUF_DISCARD_SAFE(verts) do { \
	if (verts != NULL) { \
		GWN_vertbuf_discard(verts); \
		verts = NULL; \
	} \
} while (0)

#endif /* __GWN_VERTEX_BUFFER_H__ */
