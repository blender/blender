/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __SUBD_PATCH_TABLE_H__
#define __SUBD_PATCH_TABLE_H__

#include "util/array.h"
#include "util/types.h"

#ifdef WITH_OPENSUBDIV
#  ifdef _MSC_VER
#    include "iso646.h"
#  endif

#  include <opensubdiv/far/patchTable.h>
#endif

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENSUBDIV
using namespace OpenSubdiv;
#else
/* forward declare for when OpenSubdiv is unavailable */
namespace Far {
struct PatchTable;
}
#endif

#define PATCH_ARRAY_SIZE 4
#define PATCH_PARAM_SIZE 2
#define PATCH_HANDLE_SIZE 3
#define PATCH_NODE_SIZE 1

struct PackedPatchTable {
  array<uint> table;

  size_t num_arrays;
  size_t num_indices;
  size_t num_patches;
  size_t num_nodes;

  /* calculated size from num_* members */
  size_t total_size();

  void pack(Far::PatchTable *patch_table, int offset = 0);
  void copy_adjusting_offsets(uint *dest, int doffset);
};

CCL_NAMESPACE_END

#endif /* __SUBD_PATCH_TABLE_H__ */
