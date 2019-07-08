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
 * The Original Code is Copyright (C) 2017 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup draw
 */

#ifndef __DRAW_HAIR_PRIVATE_H__
#define __DRAW_HAIR_PRIVATE_H__

#define MAX_LAYER_NAME_CT 4 /* u0123456789, u, au, a0123456789 */
#define MAX_LAYER_NAME_LEN DECIMAL_DIGITS_BOUND(uint) + 2
#define MAX_THICKRES 2    /* see eHairType */
#define MAX_HAIR_SUBDIV 4 /* see hair_subdiv rna */

struct ModifierData;
struct Object;
struct ParticleHairCache;
struct ParticleSystem;

typedef struct ParticleHairFinalCache {
  /* Output of the subdivision stage: vertex buff sized to subdiv level. */
  GPUVertBuf *proc_buf;
  GPUTexture *proc_tex;

  /* Just contains a huge index buffer used to draw the final hair. */
  GPUBatch *proc_hairs[MAX_THICKRES];

  int strands_res; /* points per hair, at least 2 */
} ParticleHairFinalCache;

typedef struct ParticleHairCache {
  GPUVertBuf *pos;
  GPUIndexBuf *indices;
  GPUBatch *hairs;

  /* Hair Procedural display: Interpolation is done on the GPU. */
  GPUVertBuf *proc_point_buf; /* Input control points */
  GPUTexture *point_tex;

  /** Infos of control points strands (segment count and base index) */
  GPUVertBuf *proc_strand_buf;
  GPUTexture *strand_tex;

  GPUVertBuf *proc_strand_seg_buf;
  GPUTexture *strand_seg_tex;

  GPUVertBuf *proc_uv_buf[MAX_MTFACE];
  GPUTexture *uv_tex[MAX_MTFACE];
  char uv_layer_names[MAX_MTFACE][MAX_LAYER_NAME_CT][MAX_LAYER_NAME_LEN];

  GPUVertBuf *proc_col_buf[MAX_MCOL];
  GPUTexture *col_tex[MAX_MCOL];
  char col_layer_names[MAX_MCOL][MAX_LAYER_NAME_CT][MAX_LAYER_NAME_LEN];

  int num_uv_layers;
  int num_col_layers;

  ParticleHairFinalCache final[MAX_HAIR_SUBDIV];

  int strands_len;
  int elems_len;
  int point_len;
} ParticleHairCache;

bool particles_ensure_procedural_data(struct Object *object,
                                      struct ParticleSystem *psys,
                                      struct ModifierData *md,
                                      struct ParticleHairCache **r_hair_cache,
                                      int subdiv,
                                      int thickness_res);

#endif /* __DRAW_HAIR_PRIVATE_H__ */
