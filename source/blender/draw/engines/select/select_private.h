/* SPDX-FileCopyrightText: 2019 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#define USE_CAGE_OCCLUSION

#include "DRW_render.h"

/* GPUViewport.storage
 * Is freed every time the viewport engine changes. */
typedef struct SELECTID_StorageList {
  struct SELECTID_PrivateData *g_data;
} SELECTID_StorageList;

typedef struct SELECTID_PassList {
  struct DRWPass *depth_only_pass;
  struct DRWPass *select_id_face_pass;
  struct DRWPass *select_id_edge_pass;
  struct DRWPass *select_id_vert_pass;
} SELECTID_PassList;

typedef struct SELECTID_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  SELECTID_PassList *psl;
  SELECTID_StorageList *stl;
} SELECTID_Data;

typedef struct SELECTID_Shaders {
  /* Depth Pre Pass */
  struct GPUShader *select_id_flat;
  struct GPUShader *select_id_uniform;
} SELECTID_Shaders;

typedef struct SELECTID_PrivateData {
  DRWShadingGroup *shgrp_depth_only;
  DRWShadingGroup *shgrp_occlude;
  DRWShadingGroup *shgrp_face_unif;
  DRWShadingGroup *shgrp_face_flat;
  DRWShadingGroup *shgrp_edge;
  DRWShadingGroup *shgrp_vert;

  DRWView *view_subregion;
  DRWView *view_faces;
  DRWView *view_edges;
  DRWView *view_verts;
} SELECTID_PrivateData; /* Transient data */

/* select_draw_utils.c */

void select_id_object_min_max(struct Object *obj, float r_min[3], float r_max[3]);
short select_id_get_object_select_mode(Scene *scene, Object *ob);
void select_id_draw_object(void *vedata,
                           View3D *v3d,
                           Object *ob,
                           short select_mode,
                           uint initial_offset,
                           uint *r_vert_offset,
                           uint *r_edge_offset,
                           uint *r_face_offset);
