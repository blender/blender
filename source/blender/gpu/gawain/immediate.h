
// Gawain immediate mode work-alike
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2016 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "vertex_format.h"

#define IMM_BATCH_COMBO 1

void immInit(void);
void immDestroy(void);

VertexFormat* immVertexFormat(void); // returns a cleared vertex format, ready for add_attrib

void immBindProgram(GLuint program);
void immUnbindProgram(void);

void immBegin(GLenum primitive, unsigned vertex_ct); // must supply exactly vertex_ct vertices
void immBeginAtMost(GLenum primitive, unsigned max_vertex_ct); // can supply fewer vertices
void immEnd(void); // finishes and draws

#if IMM_BATCH_COMBO
#include "gawain/batch.h"
// immBegin a batch, then use standard immFunctions as usual.
// immEnd will finalize the batch instead of drawing.
// Then you can draw it as many times as you like! Partially replaces the need for display lists.
Batch* immBeginBatch(GLenum prim_type, unsigned vertex_ct);
Batch* immBeginBatchAtMost(GLenum prim_type, unsigned vertex_ct);
#endif

void immAttrib1f(unsigned attrib_id, float x);
void immAttrib2f(unsigned attrib_id, float x, float y);
void immAttrib3f(unsigned attrib_id, float x, float y, float z);
void immAttrib4f(unsigned attrib_id, float x, float y, float z, float w);

void immAttrib2i(unsigned attrib_id, int x, int y);

void immAttrib3fv(unsigned attrib_id, const float data[3]);
void immAttrib4fv(unsigned attrib_id, const float data[4]);

void immAttrib3ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b);
void immAttrib4ub(unsigned attrib_id, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

void immAttrib3ubv(unsigned attrib_id, const unsigned char data[4]);
void immAttrib4ubv(unsigned attrib_id, const unsigned char data[4]);

void immEndVertex(void); // and move on to the next vertex

// provide 2D or 3D attribute value and end the current vertex, similar to glVertex:
void immVertex2f(unsigned attrib_id, float x, float y);
void immVertex3f(unsigned attrib_id, float x, float y, float z);

void immVertex2i(unsigned attrib_id, int x, int y);

void immVertex2fv(unsigned attrib_id, const float data[2]);
void immVertex3fv(unsigned attrib_id, const float data[3]);

void immVertex2iv(unsigned attrib_id, const int data[2]);

// provide values that don't change for the entire draw call
void immUniform1f(const char* name, float x);
void immUniform4f(const char* name, float x, float y, float z, float w);

// these set "uniform vec4 color"
void immUniformColor3fv(const float rgb[3]);
void immUniformColor4fv(const float rgba[4]);
// TODO: v-- treat as sRGB? --v
void immUniformColor3ub(unsigned char r, unsigned char g, unsigned char b);
void immUniformColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void immUniformColor3ubv(const unsigned char data[3]);
void immUniformColor4ubv(const unsigned char data[4]);

void immUniform1i(const char *name, const unsigned int data);
