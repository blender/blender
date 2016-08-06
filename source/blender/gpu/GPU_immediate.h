
// Gawain immediate mode work-alike, take 2
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "GPU_glew.h"
#include <stdbool.h>

#define PER_THREAD
// #define PER_THREAD __thread
// MSVC uses __declspec(thread) for C code

#define MAX_VERTEX_ATTRIBS 16

#define TRUST_NO_ONE 1

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
void pack(VertexFormat*);
// unsigned attrib_idx(const VertexFormat*, const char* name);
void bind_attrib_locations(const VertexFormat*, GLuint program);

extern PER_THREAD VertexFormat immVertexFormat; // so we don't have to copy or pass around

void immInit(void);
void immDestroy(void);

void immBegin(GLenum primitive, unsigned vertex_ct);
void immEnd(void);

void immAttrib1f(unsigned attrib_id, float x);
void immAttrib2f(unsigned attrib_id, float x, float y);
void immAttrib3f(unsigned attrib_id, float x, float y, float z);

void immAttrib3ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b);
void immAttrib4ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

void immEndVertex(void); // and move on to the next vertex

// provide 2D or 3D attribute value and end the current vertex, similar to glVertex:
void immVertex2f(unsigned attrib_id, float x, float y);
void immVertex3f(unsigned attrib_id, float x, float y, float z);
