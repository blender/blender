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

/** \file blender/gpu/gwn_vertex_format.h
 *  \ingroup gpu
 *
 * Gawain vertex format
 */

#ifndef __GWN_VERTEX_FORMAT_H__
#define __GWN_VERTEX_FORMAT_H__

#include "gwn_common.h"

#define GWN_VERT_ATTR_MAX_LEN 16
#define GWN_VERT_ATTR_MAX_NAMES 3
#define GWN_VERT_ATTR_NAME_AVERAGE_LEN 11
#define GWN_VERT_ATTR_NAMES_BUF_LEN ((GWN_VERT_ATTR_NAME_AVERAGE_LEN + 1) * GWN_VERT_ATTR_MAX_LEN)

typedef enum {
	GWN_COMP_I8,
	GWN_COMP_U8,
	GWN_COMP_I16,
	GWN_COMP_U16,
	GWN_COMP_I32,
	GWN_COMP_U32,

	GWN_COMP_F32,

	GWN_COMP_I10
} Gwn_VertCompType;

typedef enum {
	GWN_FETCH_FLOAT,
	GWN_FETCH_INT,
	GWN_FETCH_INT_TO_FLOAT_UNIT, /* 127 (ubyte) -> 0.5 (and so on for other int types) */
	GWN_FETCH_INT_TO_FLOAT /* 127 (any int type) -> 127.0 */
} Gwn_VertFetchMode;

typedef struct Gwn_VertAttr {
	Gwn_VertFetchMode fetch_mode;
	Gwn_VertCompType comp_type;
	uint gl_comp_type;
	uint comp_len; /* 1 to 4 or 8 or 12 or 16 */
	uint sz; /* size in bytes, 1 to 64 */
	uint offset; /* from beginning of vertex, in bytes */
	uint name_len; /* up to GWN_VERT_ATTR_MAX_NAMES */
	const char* name[GWN_VERT_ATTR_MAX_NAMES];
} Gwn_VertAttr;

typedef struct Gwn_VertFormat {
	uint attr_len; /* 0 to 16 (GWN_VERT_ATTR_MAX_LEN) */
	uint name_len; /* total count of active vertex attrib */
	uint stride; /* stride in bytes, 1 to 256 */
	uint name_offset;
	bool packed;
	char names[GWN_VERT_ATTR_NAMES_BUF_LEN];
	Gwn_VertAttr attribs[GWN_VERT_ATTR_MAX_LEN]; /* TODO: variable-size attribs array */
} Gwn_VertFormat;

void GWN_vertformat_clear(Gwn_VertFormat*);
void GWN_vertformat_copy(Gwn_VertFormat* dest, const Gwn_VertFormat* src);

uint GWN_vertformat_attr_add(Gwn_VertFormat*, const char* name, Gwn_VertCompType, uint comp_len, Gwn_VertFetchMode);
void GWN_vertformat_alias_add(Gwn_VertFormat*, const char* alias);

/* format conversion */

typedef struct Gwn_PackedNormal {
	int x : 10;
	int y : 10;
	int z : 10;
	int w : 2;	/* 0 by default, can manually set to { -2, -1, 0, 1 } */
} Gwn_PackedNormal;

Gwn_PackedNormal GWN_normal_convert_i10_v3(const float data[3]);
Gwn_PackedNormal GWN_normal_convert_i10_s3(const short data[3]);

#endif /* __GWN_VERTEX_FORMAT_H__ */
