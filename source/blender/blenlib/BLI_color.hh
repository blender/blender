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

#ifndef __BLI_COLOR_HH__
#define __BLI_COLOR_HH__

#include <iostream>

#include "BLI_math_color.h"

namespace blender {

struct Color4f {
  float r, g, b, a;

  Color4f() = default;

  Color4f(float r, float g, float b, float a) : r(r), g(g), b(b), a(a)
  {
  }

  operator float *()
  {
    return &r;
  }

  operator const float *() const
  {
    return &r;
  }

  friend std::ostream &operator<<(std::ostream &stream, Color4f c)
  {
    stream << "(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
    return stream;
  }
};

struct Color4b {
  uint8_t r, g, b, a;

  Color4b() = default;

  Color4b(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a)
  {
  }

  Color4b(Color4f other)
  {
    rgba_float_to_uchar(*this, other);
  }

  operator Color4f() const
  {
    Color4f result;
    rgba_uchar_to_float(result, *this);
    return result;
  }

  operator uint8_t *()
  {
    return &r;
  }

  operator const uint8_t *() const
  {
    return &r;
  }

  friend std::ostream &operator<<(std::ostream &stream, Color4b c)
  {
    stream << "(" << c.r << ", " << c.g << ", " << c.b << ", " << c.a << ")";
    return stream;
  }
};

}  // namespace blender

#endif /* __BLI_COLOR_HH__ */
