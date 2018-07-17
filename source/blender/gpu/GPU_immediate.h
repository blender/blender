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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/GPU_immediate.h
 *  \ingroup gpu
 *
 * Gawain immediate mode work-alike
 */

#ifndef __GPU_IMMEDIATE_H__
#define __GPU_IMMEDIATE_H__

#include "GPU_vertex_format.h"
#include "GPU_primitive.h"
#include "GPU_shader_interface.h"
#include "GPU_batch.h"
#include "GPU_immediate_util.h"
#include "GPU_shader.h"

Gwn_VertFormat* immVertexFormat(void); /* returns a cleared vertex format, ready for add_attrib. */

void immBindProgram(uint32_t program, const Gwn_ShaderInterface*); /* every immBegin must have a program bound first. */
void immUnbindProgram(void); /* call after your last immEnd, or before binding another program. */

void immBegin(Gwn_PrimType, uint vertex_len); /* must supply exactly vertex_len vertices. */
void immBeginAtMost(Gwn_PrimType, uint max_vertex_len); /* can supply fewer vertices. */
void immEnd(void); /* finishes and draws. */

/* ImmBegin a batch, then use standard immFunctions as usual. */
/* ImmEnd will finalize the batch instead of drawing. */
/* Then you can draw it as many times as you like! Partially replaces the need for display lists. */
Gwn_Batch* immBeginBatch(Gwn_PrimType, uint vertex_len);
Gwn_Batch* immBeginBatchAtMost(Gwn_PrimType, uint vertex_len);

/* Provide attribute values that can change per vertex. */
/* First vertex after immBegin must have all its attributes specified. */
/* Skipped attributes will continue using the previous value for that attrib_id. */
void immAttrib1f(uint attrib_id, float x);
void immAttrib2f(uint attrib_id, float x, float y);
void immAttrib3f(uint attrib_id, float x, float y, float z);
void immAttrib4f(uint attrib_id, float x, float y, float z, float w);

void immAttrib2i(uint attrib_id, int x, int y);

void immAttrib1u(uint attrib_id, uint x);

void immAttrib2s(uint attrib_id, short x, short y);

void immAttrib2fv(uint attrib_id, const float data[2]);
void immAttrib3fv(uint attrib_id, const float data[3]);
void immAttrib4fv(uint attrib_id, const float data[4]);

void immAttrib3ub(uint attrib_id, unsigned char r, unsigned char g, unsigned char b);
void immAttrib4ub(uint attrib_id, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

void immAttrib3ubv(uint attrib_id, const unsigned char data[4]);
void immAttrib4ubv(uint attrib_id, const unsigned char data[4]);

/* Explicitly skip an attribute. */
/* This advanced option kills automatic value copying for this attrib_id. */
void immSkipAttrib(uint attrib_id);

/* Provide one last attribute value & end the current vertex. */
/* This is most often used for 2D or 3D position (similar to glVertex). */
void immVertex2f(uint attrib_id, float x, float y);
void immVertex3f(uint attrib_id, float x, float y, float z);
void immVertex4f(uint attrib_id, float x, float y, float z, float w);

void immVertex2i(uint attrib_id, int x, int y);

void immVertex2s(uint attrib_id, short x, short y);

void immVertex2fv(uint attrib_id, const float data[2]);
void immVertex3fv(uint attrib_id, const float data[3]);

void immVertex2iv(uint attrib_id, const int data[2]);

/* Provide uniform values that don't change for the entire draw call. */
void immUniform1i(const char* name, int x);
void immUniform4iv(const char* name, const int data[4]);
void immUniform1f(const char* name, float x);
void immUniform2f(const char* name, float x, float y);
void immUniform2fv(const char* name, const float data[2]);
void immUniform3f(const char* name, float x, float y, float z);
void immUniform3fv(const char* name, const float data[3]);
void immUniformArray3fv(const char* name, const float *data, int count);
void immUniform4f(const char* name, float x, float y, float z, float w);
void immUniform4fv(const char* name, const float data[4]);
void immUniformArray4fv(const char* bare_name, const float *data, int count);
void immUniformMatrix4fv(const char* name, const float data[4][4]);

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

/* Extend immBindProgram to use Blender’s library of built-in shader programs.
 * Use immUnbindProgram() when done. */
void immBindBuiltinProgram(GPUBuiltinShader shader_id);

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

#endif  /* __GPU_IMMEDIATE_H__ */
