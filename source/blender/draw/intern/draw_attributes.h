/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup draw
 *
 * \brief Utilities for rendering attributes.
 */

#pragma once

#include "DNA_customdata_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"

#include "BLI_sys_types.h"
#include "BLI_threads.h"

#include "GPU_shader.h"
#include "GPU_vertex_format.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DRW_AttributeRequest {
  eCustomDataType cd_type;
  int layer_index;
  eAttrDomain domain;
  char attribute_name[64];
} DRW_AttributeRequest;

typedef struct DRW_Attributes {
  DRW_AttributeRequest requests[GPU_MAX_ATTR];
  int num_requests;
} DRW_Attributes;

void drw_attributes_clear(DRW_Attributes *attributes);

void drw_attributes_merge(DRW_Attributes *dst,
                          const DRW_Attributes *src,
                          ThreadMutex *render_mutex);

/* Return true if all requests in b are in a. */
bool drw_attributes_overlap(const DRW_Attributes *a, const DRW_Attributes *b);

DRW_AttributeRequest *drw_attributes_add_request(DRW_Attributes *attrs,
                                                 eCustomDataType type,
                                                 int layer,
                                                 eAttrDomain domain);

bool drw_custom_data_match_attribute(const CustomData *custom_data,
                                     const char *name,
                                     int *r_layer_index,
                                     eCustomDataType *r_type);

#ifdef __cplusplus
}
#endif
