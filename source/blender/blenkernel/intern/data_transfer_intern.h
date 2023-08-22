/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#pragma once

#include "BKE_customdata.h" /* For cd_datatransfer_interp */

#ifdef __cplusplus
extern "C" {
#endif

struct CustomData;
struct CustomDataTransferLayerMap;
struct ListBase;

float data_transfer_interp_float_do(int mix_mode, float val_dst, float val_src, float mix_factor);

void data_transfer_layersmapping_add_item(struct ListBase *r_map,
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
                                          uint64_t data_flag,
                                          cd_datatransfer_interp interp,
                                          void *interp_data);

/* Type-specific. */

bool data_transfer_layersmapping_vgroups(struct ListBase *r_map,
                                         int mix_mode,
                                         float mix_factor,
                                         const float *mix_weights,
                                         int num_elem_dst,
                                         bool use_create,
                                         bool use_delete,
                                         struct Object *ob_src,
                                         struct Object *ob_dst,
                                         const struct CustomData *cd_src,
                                         struct CustomData *cd_dst,
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

#ifdef __cplusplus
}
#endif
