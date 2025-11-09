/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BKE_customdata.hh" /* For cd_datatransfer_interp */

struct CustomData;
struct CustomDataTransferLayerMap;
struct ListBase;
struct Object;
struct Mesh;

/**
 * Fake CD_LAYERS (those are actually 'real' data stored directly into elements' structs,
 * or otherwise not (directly) accessible to usual CDLayer system).
 */
enum {
  CD_FAKE = 1 << 8,

  /* Vertices. */
  CD_FAKE_MDEFORMVERT = CD_FAKE | CD_MDEFORMVERT, /* *sigh* due to how vgroups are stored :(. */

  /* Edges. */
  CD_FAKE_SEAM = CD_FAKE | 100, /* UV seam flag for edges. */

  /* Multiple types of mesh elements... */
  CD_FAKE_UV =
      CD_FAKE |
      CD_PROP_FLOAT2, /* UV flag, because we handle both loop's UVs and face's textures. */

  CD_FAKE_LNOR = CD_FAKE | 200,

  CD_FAKE_SHARP = CD_FAKE | 300, /* Sharp flag for edges, smooth flag for faces. */

  CD_FAKE_BWEIGHT = CD_FAKE | 400,
  CD_FAKE_CREASE = CD_FAKE | 500,
  CD_FAKE_FREESTYLE_EDGE = CD_FAKE | 600,
  CD_FAKE_FREESTYLE_FACE = CD_FAKE | 700,
};

float data_transfer_interp_float_do(int mix_mode, float val_dst, float val_src, float mix_factor);

void data_transfer_layersmapping_add_item(blender::Vector<CustomDataTransferLayerMap> *r_map,
                                          int data_type,
                                          int mix_mode,
                                          float mix_factor,
                                          const float *mix_weights,
                                          const void *data_src,
                                          void *data_dst,
                                          int data_src_n,
                                          int data_dst_n,
                                          size_t elem_size,
                                          size_t data_size,
                                          size_t data_offset,
                                          cd_datatransfer_interp interp,
                                          void *interp_data);

/* Type-specific. */

bool data_transfer_layersmapping_vgroups(blender::Vector<CustomDataTransferLayerMap> *r_map,
                                         int mix_mode,
                                         float mix_factor,
                                         const float *mix_weights,
                                         bool use_create,
                                         bool use_delete,
                                         Object *ob_src,
                                         Object *ob_dst,
                                         const Mesh &mesh_src,
                                         Mesh &mesh_dst,
                                         bool use_dupref_dst,
                                         int fromlayers,
                                         int tolayers);

/* Defined in `customdata.cc`. */

/**
 * Normals are special, we need to take care of source & destination spaces.
 */
void customdata_data_transfer_interp_normal_normals(const CustomDataTransferLayerMap *laymap,
                                                    void *data_dst,
                                                    const void **sources,
                                                    const float *weights,
                                                    int count,
                                                    float mix_factor);
