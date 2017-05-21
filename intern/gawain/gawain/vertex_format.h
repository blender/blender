
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

#define MAX_VERTEX_ATTRIBS 16
#define MAX_ATTRIB_NAMES 3
#define AVG_VERTEX_ATTRIB_NAME_LEN 11
#define VERTEX_ATTRIB_NAMES_BUFFER_LEN ((AVG_VERTEX_ATTRIB_NAME_LEN + 1) * MAX_VERTEX_ATTRIBS)

typedef enum {
	COMP_I8,
	COMP_U8,
	COMP_I16,
	COMP_U16,
	COMP_I32,
	COMP_U32,

	COMP_F32,

	COMP_I10
} VertexCompType;

typedef enum {
	KEEP_FLOAT,
	KEEP_INT,
	NORMALIZE_INT_TO_FLOAT, // 127 (ubyte) -> 0.5 (and so on for other int types)
	CONVERT_INT_TO_FLOAT // 127 (any int type) -> 127.0
} VertexFetchMode;

typedef struct {
	VertexCompType comp_type;
	unsigned gl_comp_type;
	unsigned comp_ct; // 1 to 4
	unsigned sz; // size in bytes, 1 to 16
	unsigned offset; // from beginning of vertex, in bytes
	VertexFetchMode fetch_mode;
	const char* name[MAX_ATTRIB_NAMES];
	unsigned name_ct;
} Attrib;

typedef struct {
	unsigned attrib_ct; // 0 to 16 (MAX_VERTEX_ATTRIBS)
	unsigned name_ct; // total count of active vertex attrib
	unsigned stride; // stride in bytes, 1 to 256
	bool packed;
	Attrib attribs[MAX_VERTEX_ATTRIBS]; // TODO: variable-size attribs array
	char names[VERTEX_ATTRIB_NAMES_BUFFER_LEN];
	unsigned name_offset;
} VertexFormat;

void VertexFormat_clear(VertexFormat*);
void VertexFormat_copy(VertexFormat* dest, const VertexFormat* src);

unsigned VertexFormat_add_attrib(VertexFormat*, const char* name, VertexCompType, unsigned comp_ct, VertexFetchMode);
void VertexFormat_add_alias(VertexFormat*, const char* alias);

// format conversion

typedef struct {
	int x : 10;
	int y : 10;
	int z : 10;
	int w : 2;	// 0 by default, can manually set to { -2, -1, 0, 1 }
} PackedNormal;

PackedNormal convert_i10_v3(const float data[3]);
PackedNormal convert_i10_s3(const short data[3]);
