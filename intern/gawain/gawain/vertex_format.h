
// Gawain vertex format
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "common.h"

#define GWN_VERT_ATTR_MAX_LEN 16
#define MAX_ATTRIB_NAMES 3
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
	GWN_FETCH_INT_TO_FLOAT_UNIT, // 127 (ubyte) -> 0.5 (and so on for other int types)
	GWN_FETCH_INT_TO_FLOAT // 127 (any int type) -> 127.0
} Gwn_VertFetchMode;

typedef struct {
	Gwn_VertCompType comp_type;
	unsigned gl_comp_type;
	unsigned comp_ct; // 1 to 4
	unsigned sz; // size in bytes, 1 to 16
	unsigned offset; // from beginning of vertex, in bytes
	Gwn_VertFetchMode fetch_mode;
	const char* name[MAX_ATTRIB_NAMES];
	unsigned name_ct;
} Gwn_VertAttr;

typedef struct {
	unsigned attrib_ct; // 0 to 16 (GWN_VERT_ATTR_MAX_LEN)
	unsigned name_ct; // total count of active vertex attrib
	unsigned stride; // stride in bytes, 1 to 256
	bool packed;
	Gwn_VertAttr attribs[GWN_VERT_ATTR_MAX_LEN]; // TODO: variable-size attribs array
	char names[GWN_VERT_ATTR_NAMES_BUF_LEN];
	unsigned name_offset;
} Gwn_VertFormat;

void GWN_vertformat_clear(Gwn_VertFormat*);
void GWN_vertformat_copy(Gwn_VertFormat* dest, const Gwn_VertFormat* src);

unsigned GWN_vertformat_attr_add(Gwn_VertFormat*, const char* name, Gwn_VertCompType, unsigned comp_ct, Gwn_VertFetchMode);
void GWN_vertformat_alias_add(Gwn_VertFormat*, const char* alias);

// format conversion

typedef struct {
	int x : 10;
	int y : 10;
	int z : 10;
	int w : 2;	// 0 by default, can manually set to { -2, -1, 0, 1 }
} PackedNormal;

PackedNormal convert_i10_v3(const float data[3]);
PackedNormal convert_i10_s3(const short data[3]);
