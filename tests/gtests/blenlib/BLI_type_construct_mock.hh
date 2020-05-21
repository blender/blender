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

#ifndef __ANY_TYPE_MOCK_TEST_H__
#define __ANY_TYPE_MOCK_TEST_H__

#include "BLI_sys_types.h"

class TypeConstructMock {
 public:
  bool default_constructed = false;
  bool copy_constructed = false;
  bool move_constructed = false;
  bool copy_assigned = false;
  bool move_assigned = false;

  TypeConstructMock() : default_constructed(true)
  {
  }

  TypeConstructMock(const TypeConstructMock &other) : copy_constructed(true)
  {
  }

  TypeConstructMock(TypeConstructMock &&other) : move_constructed(true)
  {
  }

  TypeConstructMock &operator=(const TypeConstructMock &other)
  {
    if (this == &other) {
      return *this;
    }

    copy_assigned = true;
    return *this;
  }

  TypeConstructMock &operator=(TypeConstructMock &&other)
  {
    if (this == &other) {
      return *this;
    }

    move_assigned = true;
    return *this;
  }
};

#endif /* __ANY_TYPE_MOCK_TEST_H__ */
