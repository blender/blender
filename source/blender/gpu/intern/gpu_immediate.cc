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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * Mimics old style opengl immediate mode drawing.
 */

#ifndef GPU_STANDALONE
#  include "UI_resources.h"
#endif

#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_texture.h"

#include "gpu_context_private.hh"
#include "gpu_immediate_private.hh"
#include "gpu_shader_private.hh"
#include "gpu_vertex_format_private.h"

using namespace blender::gpu;

static Immediate *imm = NULL;

void immInit(void)
{
  /* TODO Remove */
}

void immActivate(void)
{
  imm = GPU_context_active_get()->imm;
}

void immDeactivate(void)
{
  imm = NULL;
}

void immDestroy(void)
{
  /* TODO Remove */
}

GPUVertFormat *immVertexFormat(void)
{
  GPU_vertformat_clear(&imm->vertex_format);
  return &imm->vertex_format;
}

void immBindShader(GPUShader *shader)
{
  BLI_assert(imm->shader == NULL);

  imm->shader = shader;

  if (!imm->vertex_format.packed) {
    VertexFormat_pack(&imm->vertex_format);
    imm->enabled_attr_bits = 0xFFFFu & ~(0xFFFFu << imm->vertex_format.attr_len);
  }

  GPU_shader_bind(shader);
  GPU_matrix_bind(shader);
  GPU_shader_set_srgb_uniform(shader);
}

void immBindBuiltinProgram(eGPUBuiltinShader shader_id)
{
  GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);
  immBindShader(shader);
}

void immUnbindProgram(void)
{
  BLI_assert(imm->shader != NULL);

  GPU_shader_unbind();
  imm->shader = NULL;
}

/* XXX do not use it. Special hack to use OCIO with batch API. */
GPUShader *immGetShader(void)
{
  return imm->shader;
}

#ifndef NDEBUG
static bool vertex_count_makes_sense_for_primitive(uint vertex_len, GPUPrimType prim_type)
{
  /* does vertex_len make sense for this primitive type? */
  if (vertex_len == 0) {
    return false;
  }

  switch (prim_type) {
    case GPU_PRIM_POINTS:
      return true;
    case GPU_PRIM_LINES:
      return vertex_len % 2 == 0;
    case GPU_PRIM_LINE_STRIP:
    case GPU_PRIM_LINE_LOOP:
      return vertex_len >= 2;
    case GPU_PRIM_LINE_STRIP_ADJ:
      return vertex_len >= 4;
    case GPU_PRIM_TRIS:
      return vertex_len % 3 == 0;
    case GPU_PRIM_TRI_STRIP:
    case GPU_PRIM_TRI_FAN:
      return vertex_len >= 3;
    default:
      return false;
  }
}
#endif

void immBegin(GPUPrimType prim_type, uint vertex_len)
{
  BLI_assert(imm->prim_type == GPU_PRIM_NONE); /* Make sure we haven't already begun. */
  BLI_assert(vertex_count_makes_sense_for_primitive(vertex_len, prim_type));

  imm->prim_type = prim_type;
  imm->vertex_len = vertex_len;
  imm->vertex_idx = 0;
  imm->unassigned_attr_bits = imm->enabled_attr_bits;

  imm->vertex_data = imm->begin();
}

void immBeginAtMost(GPUPrimType prim_type, uint vertex_len)
{
  BLI_assert(vertex_len > 0);
  imm->strict_vertex_len = false;
  immBegin(prim_type, vertex_len);
}

GPUBatch *immBeginBatch(GPUPrimType prim_type, uint vertex_len)
{
  BLI_assert(imm->prim_type == GPU_PRIM_NONE); /* Make sure we haven't already begun. */
  BLI_assert(vertex_count_makes_sense_for_primitive(vertex_len, prim_type));

  imm->prim_type = prim_type;
  imm->vertex_len = vertex_len;
  imm->vertex_idx = 0;
  imm->unassigned_attr_bits = imm->enabled_attr_bits;

  GPUVertBuf *verts = GPU_vertbuf_create_with_format(&imm->vertex_format);
  GPU_vertbuf_data_alloc(verts, vertex_len);

  imm->vertex_data = verts->data;

  imm->batch = GPU_batch_create_ex(prim_type, verts, NULL, GPU_BATCH_OWNS_VBO);
  imm->batch->flag |= GPU_BATCH_BUILDING;

  return imm->batch;
}

GPUBatch *immBeginBatchAtMost(GPUPrimType prim_type, uint vertex_len)
{
  BLI_assert(vertex_len > 0);
  imm->strict_vertex_len = false;
  return immBeginBatch(prim_type, vertex_len);
}

void immEnd(void)
{
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* Make sure we're between a Begin/End pair. */
  BLI_assert(imm->vertex_data || imm->batch);

  if (imm->strict_vertex_len) {
    BLI_assert(imm->vertex_idx == imm->vertex_len); /* With all vertices defined. */
  }
  else {
    BLI_assert(imm->vertex_idx <= imm->vertex_len);
    BLI_assert(imm->vertex_idx == 0 ||
               vertex_count_makes_sense_for_primitive(imm->vertex_idx, imm->prim_type));
  }

  if (imm->batch) {
    if (imm->vertex_idx < imm->vertex_len) {
      GPU_vertbuf_data_resize(imm->batch->verts[0], imm->vertex_len);
      /* TODO: resize only if vertex count is much smaller */
    }
    GPU_batch_set_shader(imm->batch, imm->shader);
    imm->batch->flag &= ~GPU_BATCH_BUILDING;
    imm->batch = NULL; /* don't free, batch belongs to caller */
  }
  else {
    imm->end();
  }

  /* Prepare for next immBegin. */
  imm->prim_type = GPU_PRIM_NONE;
  imm->strict_vertex_len = true;
  imm->vertex_data = NULL;
}

static void setAttrValueBit(uint attr_id)
{
  uint16_t mask = 1 << attr_id;
  BLI_assert(imm->unassigned_attr_bits & mask); /* not already set */
  imm->unassigned_attr_bits &= ~mask;
}

/* --- generic attribute functions --- */

void immAttr1f(uint attr_id, float x)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_F32);
  BLI_assert(attr->comp_len == 1);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  float *data = (float *)(imm->vertex_data + attr->offset);
  /*  printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm->buffer_data, data); */

  data[0] = x;
}

void immAttr2f(uint attr_id, float x, float y)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_F32);
  BLI_assert(attr->comp_len == 2);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  float *data = (float *)(imm->vertex_data + attr->offset);
  /*  printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm->buffer_data, data); */

  data[0] = x;
  data[1] = y;
}

void immAttr3f(uint attr_id, float x, float y, float z)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_F32);
  BLI_assert(attr->comp_len == 3);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  float *data = (float *)(imm->vertex_data + attr->offset);
  /*  printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm->buffer_data, data); */

  data[0] = x;
  data[1] = y;
  data[2] = z;
}

void immAttr4f(uint attr_id, float x, float y, float z, float w)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_F32);
  BLI_assert(attr->comp_len == 4);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  float *data = (float *)(imm->vertex_data + attr->offset);
  /*  printf("%s %td %p\n", __FUNCTION__, (GLubyte*)data - imm->buffer_data, data); */

  data[0] = x;
  data[1] = y;
  data[2] = z;
  data[3] = w;
}

void immAttr1u(uint attr_id, uint x)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_U32);
  BLI_assert(attr->comp_len == 1);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  uint *data = (uint *)(imm->vertex_data + attr->offset);

  data[0] = x;
}

void immAttr2i(uint attr_id, int x, int y)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_I32);
  BLI_assert(attr->comp_len == 2);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  int *data = (int *)(imm->vertex_data + attr->offset);

  data[0] = x;
  data[1] = y;
}

void immAttr2s(uint attr_id, short x, short y)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_I16);
  BLI_assert(attr->comp_len == 2);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  short *data = (short *)(imm->vertex_data + attr->offset);

  data[0] = x;
  data[1] = y;
}

void immAttr2fv(uint attr_id, const float data[2])
{
  immAttr2f(attr_id, data[0], data[1]);
}

void immAttr3fv(uint attr_id, const float data[3])
{
  immAttr3f(attr_id, data[0], data[1], data[2]);
}

void immAttr4fv(uint attr_id, const float data[4])
{
  immAttr4f(attr_id, data[0], data[1], data[2], data[3]);
}

void immAttr3ub(uint attr_id, uchar r, uchar g, uchar b)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_U8);
  BLI_assert(attr->comp_len == 3);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  uchar *data = imm->vertex_data + attr->offset;
  /*  printf("%s %td %p\n", __FUNCTION__, data - imm->buffer_data, data); */

  data[0] = r;
  data[1] = g;
  data[2] = b;
}

void immAttr4ub(uint attr_id, uchar r, uchar g, uchar b, uchar a)
{
  GPUVertAttr *attr = &imm->vertex_format.attrs[attr_id];
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(attr->comp_type == GPU_COMP_U8);
  BLI_assert(attr->comp_len == 4);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);

  uchar *data = imm->vertex_data + attr->offset;
  /*  printf("%s %td %p\n", __FUNCTION__, data - imm->buffer_data, data); */

  data[0] = r;
  data[1] = g;
  data[2] = b;
  data[3] = a;
}

void immAttr3ubv(uint attr_id, const uchar data[3])
{
  immAttr3ub(attr_id, data[0], data[1], data[2]);
}

void immAttr4ubv(uint attr_id, const uchar data[4])
{
  immAttr4ub(attr_id, data[0], data[1], data[2], data[3]);
}

void immAttrSkip(uint attr_id)
{
  BLI_assert(attr_id < imm->vertex_format.attr_len);
  BLI_assert(imm->vertex_idx < imm->vertex_len);
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  setAttrValueBit(attr_id);
}

static void immEndVertex(void) /* and move on to the next vertex */
{
  BLI_assert(imm->prim_type != GPU_PRIM_NONE); /* make sure we're between a Begin/End pair */
  BLI_assert(imm->vertex_idx < imm->vertex_len);

  /* Have all attributes been assigned values?
   * If not, copy value from previous vertex. */
  if (imm->unassigned_attr_bits) {
    BLI_assert(imm->vertex_idx > 0); /* first vertex must have all attributes specified */
    for (uint a_idx = 0; a_idx < imm->vertex_format.attr_len; a_idx++) {
      if ((imm->unassigned_attr_bits >> a_idx) & 1) {
        const GPUVertAttr *a = &imm->vertex_format.attrs[a_idx];

#if 0
        printf("copying %s from vertex %u to %u\n", a->name, imm->vertex_idx - 1, imm->vertex_idx);
#endif

        GLubyte *data = imm->vertex_data + a->offset;
        memcpy(data, data - imm->vertex_format.stride, a->sz);
        /* TODO: consolidate copy of adjacent attributes */
      }
    }
  }

  imm->vertex_idx++;
  imm->vertex_data += imm->vertex_format.stride;
  imm->unassigned_attr_bits = imm->enabled_attr_bits;
}

void immVertex2f(uint attr_id, float x, float y)
{
  immAttr2f(attr_id, x, y);
  immEndVertex();
}

void immVertex3f(uint attr_id, float x, float y, float z)
{
  immAttr3f(attr_id, x, y, z);
  immEndVertex();
}

void immVertex4f(uint attr_id, float x, float y, float z, float w)
{
  immAttr4f(attr_id, x, y, z, w);
  immEndVertex();
}

void immVertex2i(uint attr_id, int x, int y)
{
  immAttr2i(attr_id, x, y);
  immEndVertex();
}

void immVertex2s(uint attr_id, short x, short y)
{
  immAttr2s(attr_id, x, y);
  immEndVertex();
}

void immVertex2fv(uint attr_id, const float data[2])
{
  immAttr2f(attr_id, data[0], data[1]);
  immEndVertex();
}

void immVertex3fv(uint attr_id, const float data[3])
{
  immAttr3f(attr_id, data[0], data[1], data[2]);
  immEndVertex();
}

void immVertex2iv(uint attr_id, const int data[2])
{
  immAttr2i(attr_id, data[0], data[1]);
  immEndVertex();
}

/* --- generic uniform functions --- */

void immUniform1f(const char *name, float x)
{
  GPU_shader_uniform_1f(imm->shader, name, x);
}

void immUniform2f(const char *name, float x, float y)
{
  GPU_shader_uniform_2f(imm->shader, name, x, y);
}

void immUniform2fv(const char *name, const float data[2])
{
  GPU_shader_uniform_2fv(imm->shader, name, data);
}

void immUniform3f(const char *name, float x, float y, float z)
{
  GPU_shader_uniform_3f(imm->shader, name, x, y, z);
}

void immUniform3fv(const char *name, const float data[3])
{
  GPU_shader_uniform_3fv(imm->shader, name, data);
}

void immUniform4f(const char *name, float x, float y, float z, float w)
{
  GPU_shader_uniform_4f(imm->shader, name, x, y, z, w);
}

void immUniform4fv(const char *name, const float data[4])
{
  GPU_shader_uniform_4fv(imm->shader, name, data);
}

/* Note array index is not supported for name (i.e: "array[0]"). */
void immUniformArray4fv(const char *name, const float *data, int count)
{
  GPU_shader_uniform_4fv_array(imm->shader, name, count, (float(*)[4])data);
}

void immUniformMatrix4fv(const char *name, const float data[4][4])
{
  GPU_shader_uniform_mat4(imm->shader, name, data);
}

void immUniform1i(const char *name, int x)
{
  GPU_shader_uniform_1i(imm->shader, name, x);
}

void immBindTexture(const char *name, GPUTexture *tex)
{
  int binding = GPU_shader_get_texture_binding(imm->shader, name);
  GPU_texture_bind(tex, binding);
}

void immBindTextureSampler(const char *name, GPUTexture *tex, eGPUSamplerState state)
{
  int binding = GPU_shader_get_texture_binding(imm->shader, name);
  GPU_texture_bind_ex(tex, state, binding, true);
}

/* --- convenience functions for setting "uniform vec4 color" --- */

void immUniformColor4f(float r, float g, float b, float a)
{
  int32_t uniform_loc = GPU_shader_get_builtin_uniform(imm->shader, GPU_UNIFORM_COLOR);
  BLI_assert(uniform_loc != -1);
  float data[4] = {r, g, b, a};
  GPU_shader_uniform_vector(imm->shader, uniform_loc, 4, 1, data);
}

void immUniformColor4fv(const float rgba[4])
{
  immUniformColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);
}

void immUniformColor3f(float r, float g, float b)
{
  immUniformColor4f(r, g, b, 1.0f);
}

void immUniformColor3fv(const float rgb[3])
{
  immUniformColor4f(rgb[0], rgb[1], rgb[2], 1.0f);
}

void immUniformColor3fvAlpha(const float rgb[3], float a)
{
  immUniformColor4f(rgb[0], rgb[1], rgb[2], a);
}

void immUniformColor3ub(uchar r, uchar g, uchar b)
{
  const float scale = 1.0f / 255.0f;
  immUniformColor4f(scale * r, scale * g, scale * b, 1.0f);
}

void immUniformColor4ub(uchar r, uchar g, uchar b, uchar a)
{
  const float scale = 1.0f / 255.0f;
  immUniformColor4f(scale * r, scale * g, scale * b, scale * a);
}

void immUniformColor3ubv(const uchar rgb[3])
{
  immUniformColor3ub(rgb[0], rgb[1], rgb[2]);
}

void immUniformColor3ubvAlpha(const uchar rgb[3], uchar alpha)
{
  immUniformColor4ub(rgb[0], rgb[1], rgb[2], alpha);
}

void immUniformColor4ubv(const uchar rgba[4])
{
  immUniformColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]);
}

#ifndef GPU_STANDALONE

void immUniformThemeColor(int color_id)
{
  float color[4];
  UI_GetThemeColor4fv(color_id, color);
  immUniformColor4fv(color);
}

void immUniformThemeColorAlpha(int color_id, float a)
{
  float color[4];
  UI_GetThemeColor3fv(color_id, color);
  color[3] = a;
  immUniformColor4fv(color);
}

void immUniformThemeColor3(int color_id)
{
  float color[3];
  UI_GetThemeColor3fv(color_id, color);
  immUniformColor3fv(color);
}

void immUniformThemeColorShade(int color_id, int offset)
{
  float color[4];
  UI_GetThemeColorShade4fv(color_id, offset, color);
  immUniformColor4fv(color);
}

void immUniformThemeColorShadeAlpha(int color_id, int color_offset, int alpha_offset)
{
  float color[4];
  UI_GetThemeColorShadeAlpha4fv(color_id, color_offset, alpha_offset, color);
  immUniformColor4fv(color);
}

void immUniformThemeColorBlendShade(int color_id1, int color_id2, float fac, int offset)
{
  float color[4];
  UI_GetThemeColorBlendShade4fv(color_id1, color_id2, fac, offset, color);
  immUniformColor4fv(color);
}

void immUniformThemeColorBlend(int color_id1, int color_id2, float fac)
{
  uint8_t color[3];
  UI_GetThemeColorBlend3ubv(color_id1, color_id2, fac, color);
  immUniformColor3ubv(color);
}

void immThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset)
{
  uchar col[4];
  UI_GetThemeColorShadeAlpha4ubv(colorid, coloffset, alphaoffset, col);
  immUniformColor4ub(col[0], col[1], col[2], col[3]);
}

#endif /* GPU_STANDALONE */
