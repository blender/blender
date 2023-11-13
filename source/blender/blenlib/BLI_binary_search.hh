/* SPDX-FileCopyrightText: 2023 Blender Authors
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
int64_t find_predicate_begin(Iterator begin, Iterator end, Predicate &&predicate)
{
  return std::lower_bound(begin,
                          end,
                          nullptr,
                          [&](const auto &value, void * /*dummy*/) { return !predicate(value); }) -
         begin;
}

template<typename Range, typename Predicate>
int64_t find_predicate_begin(const Range &range, Predicate &&predicate)
{
  return find_predicate_begin(range.begin(), range.end(), predicate);
}

}  // namespace blender::binary_search
