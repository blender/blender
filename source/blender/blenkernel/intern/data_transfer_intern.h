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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 */

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
                                         struct CustomData *cd_src,
                                         struct CustomData *cd_dst,
                                         bool use_dupref_dst,
                                         int fromlayers,
                                         int tolayers);

/* Defined in customdata.c */

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
