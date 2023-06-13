/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
struct ReportList;

void ED_operatortypes_geometry(void);

/**
 * Convert an attribute with the given name to a new type and domain.
 * The attribute must already exist.
 *
 * \note Does not support meshes in edit mode.
 */
bool ED_geometry_attribute_convert(struct Mesh *mesh,
                                   const char *name,
                                   eCustomDataType dst_type,
                                   eAttrDomain dst_domain,
                                   ReportList *reports);
#ifdef __cplusplus
}
#endif
