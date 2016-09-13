
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

typedef enum {
	KEEP_FLOAT,
	KEEP_INT,
	NORMALIZE_INT_TO_FLOAT, // 127 (ubyte) -> 0.5 (and so on for other int types)
	CONVERT_INT_TO_FLOAT // 127 (any int type) -> 127.0
} VertexFetchMode;

typedef struct {
	GLenum comp_type;
	unsigned comp_ct; // 1 to 4
	unsigned sz; // size in bytes, 1 to 16
	unsigned offset; // from beginning of vertex, in bytes
	VertexFetchMode fetch_mode;
	char* name; // TODO: shared allocation of all names within a VertexFormat
} Attrib;

typedef struct {
	unsigned attrib_ct; // 0 to 16 (MAX_VERTEX_ATTRIBS)
	unsigned stride; // stride in bytes, 1 to 256
	bool packed;
	Attrib attribs[MAX_VERTEX_ATTRIBS]; // TODO: variable-size attribs array
} VertexFormat;

void clear_VertexFormat(VertexFormat*);
unsigned add_attrib(VertexFormat*, const char* name, GLenum comp_type, unsigned comp_ct, VertexFetchMode);

// for internal use
void pack(VertexFormat*);
unsigned padding(unsigned offset, unsigned alignment);
unsigned vertex_buffer_size(const VertexFormat*, unsigned vertex_ct);
