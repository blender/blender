/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Contains management of ID's and libraries
 * allocate and free of all library data
 */

#include "DNA_ID.h"
#include "DNA_mesh_types.h"

#include "BLI_utildefines.h"

#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"

void BKE_id_eval_properties_copy(ID *id_cow, ID *id)
{
  const ID_Type id_type = GS(id->name);
  BLI_assert((id_cow->tag & LIB_TAG_COPIED_ON_EVAL) && !(id->tag & LIB_TAG_COPIED_ON_EVAL));
  BLI_assert(ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW(id_type));
  if (id_type == ID_ME) {
    BKE_mesh_copy_parameters((Mesh *)id_cow, (const Mesh *)id);
  }
  else {
    BLI_assert_unreachable();
  }
}
