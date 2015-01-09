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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Bastien Montagne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/data_transfer_intern.h
 *  \ingroup bke
 */

#ifndef __DATA_TRANSFER_INTERN_H__
#define __DATA_TRANSFER_INTERN_H__

struct CustomDataTransferLayerMap;
struct CustomData;
struct ListBase;

float data_transfer_interp_float_do(
        const int mix_mode, const float val_dst, const float val_src, const float mix_factor);

/* Copied from BKE_customdata.h :( */
typedef void (*cd_datatransfer_interp)(const struct CustomDataTransferLayerMap *laymap, void *dest,
                                       void **sources, const float *weights, const int count, const float mix_factor);

void data_transfer_layersmapping_add_item(
        struct ListBase *r_map, const int data_type, const int mix_mode,
        const float mix_factor, const float *mix_weights,
        void *data_src, void *data_dst, const int data_src_n, const int data_dst_n,
        const size_t elem_size, const size_t data_size, const size_t data_offset, const uint64_t data_flag,
        cd_datatransfer_interp interp);

/* Type-specific. */

bool data_transfer_layersmapping_vgroups(
        struct ListBase *r_map, const int mix_mode, const float mix_factor, const float *mix_weights,
        const int num_elem_dst, const bool use_create, const bool use_delete,
        struct Object *ob_src, struct Object *ob_dst, struct CustomData *cd_src, struct CustomData *cd_dst,
        const bool use_dupref_dst, const int fromlayers, const int tolayers);

#endif  /* __DATA_TRANSFER_INTERN_H__ */
