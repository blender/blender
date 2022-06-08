/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BKE_attribute.h"
#include "DNA_customdata_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;

void ED_operatortypes_geometry(void);
bool ED_geometry_attribute_convert(struct Mesh *mesh,
                                   const char *layer_name,
                                   eCustomDataType old_type,
                                   eAttrDomain old_domain,
                                   eCustomDataType new_type,
                                   eAttrDomain new_domain);
#ifdef __cplusplus
}
#endif
