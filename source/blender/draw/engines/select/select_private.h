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
 * Copyright 2019, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#ifndef __SELECT_PRIVATE_H__
#define __SELECT_PRIVATE_H__

#include "DRW_render.h"

/* GPUViewport.storage
 * Is freed everytime the viewport engine changes */
typedef struct SELECTID_StorageList {
  struct SELECTID_PrivateData *g_data;
} SELECTID_StorageList;

typedef struct SELECTID_PassList {
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
  DRWShadingGroup *shgrp_face_unif;
  DRWShadingGroup *shgrp_face_flat;
  DRWShadingGroup *shgrp_edge;
  DRWShadingGroup *shgrp_vert;

  DRWView *view_faces;
  DRWView *view_edges;
  DRWView *view_verts;
} SELECTID_PrivateData; /* Transient data */

struct BaseOffset {
  /* For convenience only. */
  union {
    uint offset;
    uint face_start;
  };
  union {
    uint face;
    uint edge_start;
  };
  union {
    uint edge;
    uint vert_start;
  };
  uint vert;
};

void select_id_draw_object(void *vedata,
                           View3D *v3d,
                           Object *ob,
                           short select_mode,
                           uint initial_offset,
                           uint *r_vert_offset,
                           uint *r_edge_offset,
                           uint *r_face_offset);

#endif /* __SELECT_PRIVATE_H__ */
