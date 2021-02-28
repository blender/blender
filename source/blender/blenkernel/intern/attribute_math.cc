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

#include "BKE_attribute_math.hh"

namespace blender::attribute_math {

Color4fMixer::Color4fMixer(MutableSpan<Color4f> output_buffer, Color4f default_color)
    : buffer_(output_buffer),
      default_color_(default_color),
      total_weights_(output_buffer.size(), 0.0f)
{
  buffer_.fill(Color4f(0, 0, 0, 0));
}

void Color4fMixer::mix_in(const int64_t index, const Color4f &color, const float weight)
{
  BLI_assert(weight >= 0.0f);
  Color4f &output_color = buffer_[index];
  output_color.r += color.r * weight;
  output_color.g += color.g * weight;
  output_color.b += color.b * weight;
  output_color.a += color.a * weight;
  total_weights_[index] += weight;
}

void Color4fMixer::finalize()
{
  for (const int64_t i : buffer_.index_range()) {
    const float weight = total_weights_[i];
    Color4f &output_color = buffer_[i];
    if (weight > 0.0f) {
      const float weight_inv = 1.0f / weight;
      output_color.r *= weight_inv;
      output_color.g *= weight_inv;
      output_color.b *= weight_inv;
      output_color.a *= weight_inv;
    }
    else {
      output_color = default_color_;
    }
  }
}

}  // namespace blender::attribute_math
