/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#define USE_CAGE_OCCLUSION

#include "draw_manager.hh"
#include "draw_pass.hh"

#include "DRW_render.hh"

using namespace blender::draw;

/* GPUViewport.storage
 * Is freed every time the viewport engine changes. */
struct SELECTID_StorageList {
  struct SELECTID_PrivateData *g_data;
};

struct SELECTID_Instance {
  PassSimple depth_only_ps = {"depth_only_ps"};
  PassSimple::Sub *depth_only = nullptr;
  PassSimple::Sub *depth_occlude = nullptr;

  PassSimple select_edge_ps = {"select_id_edge_ps"};
  PassSimple::Sub *select_edge = nullptr;

  PassSimple select_id_vert_ps = {"select_id_vert_ps"};
  PassSimple::Sub *select_vert = nullptr;

  PassSimple select_face_ps = {"select_id_face_ps"};
  PassSimple::Sub *select_face_uniform = nullptr;
  PassSimple::Sub *select_face_flat = nullptr;

  View view_faces = {"view_faces"};
  View view_edges = {"view_edges"};
  View view_verts = {"view_verts"};
};

struct SELECTID_Data {
  void *engine_type;
  DRWViewportEmptyList *fbl;
  DRWViewportEmptyList *txl;
  DRWViewportEmptyList *psl;
  DRWViewportEmptyList *stl;
  SELECTID_Instance *instance;

  char info[GPU_INFO_SIZE];
};

struct SELECTID_Shaders {
  /* Depth Pre Pass */
  GPUShader *select_id_flat;
  GPUShader *select_id_uniform;
};

/* `select_draw_utils.cc` */

short select_id_get_object_select_mode(Scene *scene, Object *ob);
void select_id_draw_object(SELECTID_Instance &inst,
                           View3D *v3d,
                           Object *ob,
                           ResourceHandle res_handle,
                           short select_mode,
                           uint initial_offset,
                           uint *r_vert_offset,
                           uint *r_edge_offset,
                           uint *r_face_offset);
