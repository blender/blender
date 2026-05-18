/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef WITH_TBB
#  include <tbb/parallel_sort.h>
#else
#  include <algorithm>
#endif

namespace blender {

#ifdef WITH_TBB
using tbb::parallel_sort;
#else
template<typename RandomAccessIterator>
void parallel_sort(RandomAccessIterator begin, RandomAccessIterator end)
{
  std::sort<RandomAccessIterator>(begin, end);
}
template<typename RandomAccessIterator, typename Compare>
void parallel_sort(RandomAccessIterator begin, RandomAccessIterator end, const Compare &comp)
{
  std::sort<RandomAccessIterator, Compare>(begin, end, comp);
}
template<typename Range> void parallel_sort(Range &range)
{
  std::sort(range.begin(), range.end());
}
template<typename Range, typename Compare> void parallel_sort(Range &range, const Compare &comp)
{
  std::sort(range.begin(), range.end(), comp);
}
#endif /* !WITH_TBB */

}  // namespace blender
