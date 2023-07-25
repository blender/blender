/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Evaluated mesh info printing function, to help track down differences output.
 *
 * Output from these functions can be evaluated as Python literals.
 * See `bmesh_debug.c` for the equivalent #BMesh functionality.
 */

#ifndef NDEBUG

#  include <cstdio>

#  include "MEM_guardedalloc.h"

#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_object_types.h"

#  include "BLI_utildefines.h"

#  include "BKE_customdata.h"

#  include "BKE_mesh.hh"

#  include "BLI_dynstr.h"

char *BKE_mesh_debug_info(const Mesh *me)
{
  DynStr *dynstr = BLI_dynstr_new();
  char *ret;

  const char *indent8 = "        ";

  BLI_dynstr_append(dynstr, "{\n");
  BLI_dynstr_appendf(dynstr, "    'ptr': '%p',\n", (void *)me);
  BLI_dynstr_appendf(dynstr, "    'totvert': %d,\n", me->totvert);
  BLI_dynstr_appendf(dynstr, "    'totedge': %d,\n", me->totedge);
  BLI_dynstr_appendf(dynstr, "    'totface': %d,\n", me->totface_legacy);
  BLI_dynstr_appendf(dynstr, "    'faces_num': %d,\n", me->faces_num);

  BLI_dynstr_appendf(dynstr, "    'runtime.deformed_only': %d,\n", me->runtime->deformed_only);
  BLI_dynstr_appendf(
      dynstr, "    'runtime->is_original_bmesh': %d,\n", me->runtime->is_original_bmesh);

  BLI_dynstr_append(dynstr, "    'vert_layers': (\n");
  CustomData_debug_info_from_layers(&me->vdata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'edge_layers': (\n");
  CustomData_debug_info_from_layers(&me->edata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'loop_layers': (\n");
  CustomData_debug_info_from_layers(&me->ldata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'poly_layers': (\n");
  CustomData_debug_info_from_layers(&me->pdata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'tessface_layers': (\n");
  CustomData_debug_info_from_layers(&me->fdata_legacy, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "}\n");

  ret = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);
  return ret;
}

void BKE_mesh_debug_print(const Mesh *me)
{
  char *str = BKE_mesh_debug_info(me);
  puts(str);
  fflush(stdout);
  MEM_freeN(str);
}

#endif /* NDEBUG */
