/*
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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU immediate mode work-alike
 */

#ifndef __GPU_IMMEDIATE_H__
#define __GPU_IMMEDIATE_H__

#include "GPU_batch.h"
#include "GPU_immediate_util.h"
#include "GPU_primitive.h"
#include "GPU_shader.h"
#include "GPU_shader_interface.h"
#include "GPU_vertex_format.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns a cleared vertex format, ready for #add_attr. */
GPUVertFormat *immVertexFormat(void);

/** Every immBegin must have a program bound first. */
void immBindProgram(uint32_t program, const GPUShaderInterface *);
/** Call after your last immEnd, or before binding another program. */
void immUnbindProgram(void);

/** Must supply exactly vertex_len vertices. */
void immBegin(GPUPrimType, uint vertex_len);
/** Can supply fewer vertices. */
void immBeginAtMost(GPUPrimType, uint max_vertex_len);
void immEnd(void); /* finishes and draws. */

/* immBegin a batch, then use standard immFunctions as usual. */
/* immEnd will finalize the batch instead of drawing. */
/* Then you can draw it as many times as you like!
 * Partially replaces the need for display lists. */
GPUBatch *immBeginBatch(GPUPrimType, uint vertex_len);
GPUBatch *immBeginBatchAtMost(GPUPrimType, uint vertex_len);

/* Provide attribute values that can change per vertex. */
/* First vertex after immBegin must have all its attributes specified. */
/* Skipped attributes will continue using the previous value for that attr_id. */
void immAttr1f(uint attr_id, float x);
void immAttr2f(uint attr_id, float x, float y);
void immAttr3f(uint attr_id, float x, float y, float z);
void immAttr4f(uint attr_id, float x, float y, float z, float w);

void immAttr2i(uint attr_id, int x, int y);

void immAttr1u(uint attr_id, uint x);

void immAttr2s(uint attr_id, short x, short y);

void immAttr2fv(uint attr_id, const float data[2]);
void immAttr3fv(uint attr_id, const float data[3]);
void immAttr4fv(uint attr_id, const float data[4]);

void immAttr3ub(uint attr_id, unsigned char r, unsigned char g, unsigned char b);
void immAttr4ub(uint attr_id, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

void immAttr3ubv(uint attr_id, const unsigned char data[3]);
void immAttr4ubv(uint attr_id, const unsigned char data[4]);

/* Explicitly skip an attribute. */
/* This advanced option kills automatic value copying for this attr_id. */
void immAttrSkip(uint attr_id);

/* Provide one last attribute value & end the current vertex. */
/* This is most often used for 2D or 3D position (similar to glVertex). */
void immVertex2f(uint attr_id, float x, float y);
void immVertex3f(uint attr_id, float x, float y, float z);
void immVertex4f(uint attr_id, float x, float y, float z, float w);

void immVertex2i(uint attr_id, int x, int y);

void immVertex2s(uint attr_id, short x, short y);

void immVertex2fv(uint attr_id, const float data[2]);
void immVertex3fv(uint attr_id, const float data[3]);

void immVertex2iv(uint attr_id, const int data[2]);

/* Provide uniform values that don't change for the entire draw call. */
void immUniform1i(const char *name, int x);
void immUniform4iv(const char *name, const int data[4]);
void immUniform1f(const char *name, float x);
void immUniform2f(const char *name, float x, float y);
void immUniform2fv(const char *name, const float data[2]);
void immUniform3f(const char *name, float x, float y, float z);
void immUniform3fv(const char *name, const float data[3]);
void immUniformArray3fv(const char *name, const float *data, int count);
void immUniform4f(const char *name, float x, float y, float z, float w);
void immUniform4fv(const char *name, const float data[4]);
void immUniformArray4fv(const char *bare_name, const float *data, int count);
void immUniformMatrix4fv(const char *name, const float data[4][4]);

/* Convenience functions for setting "uniform vec4 color". */
/* The rgb functions have implicit alpha = 1.0. */
void immUniformColor4f(float r, float g, float b, float a);
void immUniformColor4fv(const float rgba[4]);
void immUniformColor3f(float r, float g, float b);
void immUniformColor3fv(const float rgb[3]);
void immUniformColor3fvAlpha(const float rgb[3], float a);

void immUniformColor3ub(unsigned char r, unsigned char g, unsigned char b);
void immUniformColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void immUniformColor3ubv(const unsigned char rgb[3]);
void immUniformColor3ubvAlpha(const unsigned char rgb[3], unsigned char a);
void immUniformColor4ubv(const unsigned char rgba[4]);

/**
 * Extend #immBindProgram to use Blender’s library of built-in shader programs.
 * Use #immUnbindProgram() when done.
 */
void immBindBuiltinProgram(eGPUBuiltinShader shader_id);

/* Extend immUniformColor to take Blender's themes */
void immUniformThemeColor(int color_id);
void immUniformThemeColor3(int color_id);
void immUniformThemeColorShade(int color_id, int offset);
void immUniformThemeColorShadeAlpha(int color_id, int color_offset, int alpha_offset);
void immUniformThemeColorBlendShade(int color_id1, int color_id2, float fac, int offset);
void immUniformThemeColorBlend(int color_id1, int color_id2, float fac);
void immThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset);

/* These are called by the system -- not part of drawing API. */
void immInit(void);
void immActivate(void);
void immDeactivate(void);
void immDestroy(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_IMMEDIATE_H__ */
