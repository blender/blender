/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <algorithm>

#include "BLI_utildefines.h"

namespace blender::binary_search {

/**
 * Find the index of the first element where the predicate is true. The predicate must also be
 * true for all following elements. If the predicate is false for all elements, the size of the
 * range is returned.
 */
template<typename Iterator, typename Predicate>
static int64_t first_if(Iterator begin, Iterator end, Predicate &&predicate)
{
  return std::lower_bound(begin,
                          end,
                          nullptr,
                          [&](const auto &value, void * /*dummy*/) { return !predicate(value); }) -
         begin;
}

/**
 * Find the index of the last element where the predicate is true. The predicate must also be
 * true for all previous elements. If the predicate is false for all elements, the -1 is returned.
 */
template<typename Iterator, typename Predicate>
static int64_t last_if(Iterator begin, Iterator end, Predicate &&predicate)
{
  return std::upper_bound(begin,
                          end,
                          nullptr,
                          [&](void * /*dummy*/, const auto &value) { return !predicate(value); }) -
         begin - 1;
}

template<typename Range, typename Predicate>
int64_t first_if(const Range &range, Predicate &&predicate)
{
  return first_if(range.begin(), range.end(), predicate);
}

template<typename Range, typename Predicate>
int64_t last_if(const Range &range, Predicate &&predicate)
{
  return last_if(range.begin(), range.end(), predicate);
}

}  // namespace blender::binary_search
