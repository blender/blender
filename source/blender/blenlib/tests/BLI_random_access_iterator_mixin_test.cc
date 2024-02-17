/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <array>

#include "testing/testing.h"

#include "BLI_random_access_iterator_mixin.hh"
#include "BLI_vector.hh"

namespace blender::iterator::tests {

template<typename T>
struct DoublingIterator : public RandomAccessIteratorMixin<DoublingIterator<T>> {
 private:
  const T *data_;

 public:
  DoublingIterator(const T *data) : data_(data) {}

  T operator*() const
  {
    return *data_ * 2;
  }

  const T *const &iter_prop() const
  {
    return data_;
  }
};

TEST(random_access_iterator_mixin, DoublingIterator)
{
  std::array<int, 4> my_array = {3, 6, 1, 2};

  const DoublingIterator<int> begin = DoublingIterator<int>(&*my_array.begin());
  const DoublingIterator<int> end = begin + my_array.size();

  Vector<int> values;
  for (DoublingIterator<int> it = begin; it != end; ++it) {
    values.append(*it);
  }

  EXPECT_EQ(values.size(), 4);
  EXPECT_EQ(values[0], 6);
  EXPECT_EQ(values[1], 12);
  EXPECT_EQ(values[2], 2);
  EXPECT_EQ(values[3], 4);
}

}  // namespace blender::iterator::tests
