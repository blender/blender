/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <map>
#include <unordered_map>

CCL_NAMESPACE_BEGIN

using std::map;
using std::pair;
using std::unordered_map;
using std::unordered_multimap;

template<typename T> static void map_free_memory(T &data)
{
  /* Use swap() trick to actually free all internal memory. */
  T empty_data;
  data.swap(empty_data);
}

CCL_NAMESPACE_END
