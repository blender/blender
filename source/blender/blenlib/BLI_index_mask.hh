/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __BLI_INDEX_MASK_HH__
#define __BLI_INDEX_MASK_HH__

/** \file
 * \ingroup bli
 *
 * An IndexMask references an array of unsigned integers with the following property:
 *   The integers must be in ascending order and there must not be duplicates.
 *
 * Remember that the array is only referenced and not owned by an IndexMask instance.
 *
 * In most cases the integers in the array represent some indices into another array. So they
 * "select" or "mask" a some elements in that array. Hence the name IndexMask.
 *
 * The invariant stated above has the nice property that it makes it easy to check if an integer
 * array is an IndexRange, i.e. no indices are skipped. That allows functions to implement two code
 * paths: One where it iterates over the index array and one where it iterates over the index
 * range. The latter one is more efficient due to less memory reads and potential usage of SIMD
 * instructions.
 *
 * The IndexMask.foreach_index method helps writing code that implements both code paths at the
 * same time.
 */

#include "BLI_array_ref.hh"
#include "BLI_index_range.hh"

namespace blender {

class IndexMask {
 private:
  /* The underlying reference to sorted integers. */
  ArrayRef<uint> m_indices;

 public:
  /* Creates an IndexMask that contains no indices. */
  IndexMask() = default;

  /**
   * Create an IndexMask using the given integer array.
   * This constructor asserts that the given integers are in ascending order and that there are no
   * duplicates.
   */
  IndexMask(ArrayRef<uint> indices) : m_indices(indices)
  {
#ifdef DEBUG
    for (uint i = 1; i < indices.size(); i++) {
      BLI_assert(indices[i - 1] < indices[i]);
    }
#endif
  }

  /**
   * Use this method when you know that no indices are skipped. It is more efficient than preparing
   * an integer array all the time.
   */
  IndexMask(IndexRange range) : m_indices(range.as_array_ref())
  {
  }

  /**
   * Construct an IndexMask from a sorted list of indices. Note, the created IndexMask is only
   * valid as long as the initializer_list is valid.
   *
   * Don't do this:
   *   IndexMask mask = {3, 4, 5};
   *
   * Do this:
   *   do_something_with_an_index_mask({3, 4, 5});
   */
  IndexMask(const std::initializer_list<uint> &indices) : IndexMask(ArrayRef<uint>(indices))
  {
  }

  /**
   * Creates an IndexMask that references the indices [0, n-1].
   */
  explicit IndexMask(uint n) : IndexMask(IndexRange(n))
  {
  }

  operator ArrayRef<uint>() const
  {
    return m_indices;
  }

  const uint *begin() const
  {
    return m_indices.begin();
  }

  const uint *end() const
  {
    return m_indices.end();
  }

  /**
   * Returns the n-th index referenced by this IndexMask. The `index_mask` method returns an
   * IndexRange containing all indices that can be used as parameter here.
   */
  uint operator[](uint n) const
  {
    return m_indices[n];
  }

  /**
   * Returns the minimum size an array has to have, if the integers in this IndexMask are going to
   * be used as indices in that array.
   */
  uint min_array_size() const
  {
    if (m_indices.size() == 0) {
      return 0;
    }
    else {
      return m_indices.last() + 1;
    }
  }

  ArrayRef<uint> indices() const
  {
    return m_indices;
  }

  /**
   * Returns true if this IndexMask does not skip any indices. This check requires O(1) time.
   */
  bool is_range() const
  {
    return m_indices.size() > 0 && m_indices.last() - m_indices.first() == m_indices.size() - 1;
  }

  /**
   * Returns the IndexRange referenced by this IndexMask. This method should only be called after
   * the caller made sure that this IndexMask is actually a range.
   */
  IndexRange as_range() const
  {
    BLI_assert(this->is_range());
    return IndexRange{m_indices.first(), m_indices.size()};
  }

  /**
   * Calls the given callback for every referenced index. The callback has to take one unsigned
   * integer as parameter.
   *
   * This method implements different code paths for the cases when the IndexMask represents a
   * range or not.
   */
  template<typename CallbackT> void foreach_index(const CallbackT &callback) const
  {
    if (this->is_range()) {
      IndexRange range = this->as_range();
      for (uint i : range) {
        callback(i);
      }
    }
    else {
      for (uint i : m_indices) {
        callback(i);
      }
    }
  }

  /**
   * Returns an IndexRange that can be used to index this IndexMask.
   *
   * The range is [0, number of indices - 1].
   *
   * This is not to be confused with the `as_range` method.
   */
  IndexRange index_range() const
  {
    return m_indices.index_range();
  }

  /**
   * Returns the largest index that is referenced by this IndexMask.
   */
  uint last() const
  {
    return m_indices.last();
  }

  /**
   * Returns the number of indices referenced by this IndexMask.
   */
  uint size() const
  {
    return m_indices.size();
  }
};

}  // namespace blender

#endif /* __BLI_INDEX_MASK_HH__ */
