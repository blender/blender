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

/** \file
 * \ingroup bli
 */

#ifndef __BLI_UTILITY_MIXINS_HH__
#define __BLI_UTILITY_MIXINS_HH__

namespace BLI {

/**
 * A type that inherits from NonCopyable cannot be copied anymore.
 */
class NonCopyable {
 public:
  /* Disable copy construction and assignment. */
  NonCopyable(const NonCopyable &other) = delete;
  NonCopyable &operator=(const NonCopyable &other) = delete;

  /* Explicitly enable default construction, move construction and move assignment. */
  NonCopyable() = default;
  NonCopyable(NonCopyable &&other) = default;
  NonCopyable &operator=(NonCopyable &&other) = default;
};

/**
 * A type that inherits from NonMovable cannot be moved anymore.
 */
class NonMovable {
 public:
  /* Disable move construction and assignment. */
  NonMovable(NonMovable &&other) = delete;
  NonMovable &operator=(NonMovable &&other) = delete;

  /* Explicitly enable default construction, copy construction and copy assignment. */
  NonMovable() = default;
  NonMovable(const NonMovable &other) = default;
  NonMovable &operator=(const NonMovable &other) = default;
};

}  // namespace BLI

#endif /* __BLI_UTILITY_MIXINS_HH__ */
