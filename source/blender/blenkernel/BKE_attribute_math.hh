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

#include "BLI_array.hh"
#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"

#include "DNA_customdata_types.h"

namespace blender::attribute_math {

/**
 * Utility function that simplifies calling a templated function based on a custom data type.
 */
template<typename Func>
void convert_to_static_type(const CustomDataType data_type, const Func &func)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      func(float());
      break;
    case CD_PROP_FLOAT2:
      func(float2());
      break;
    case CD_PROP_FLOAT3:
      func(float3());
      break;
    case CD_PROP_INT32:
      func(int());
      break;
    case CD_PROP_BOOL:
      func(bool());
      break;
    case CD_PROP_COLOR:
      func(Color4f());
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

/* -------------------------------------------------------------------- */
/** \name Mix three values of the same type.
 *
 * This is typically used to interpolate values within a triangle.
 * \{ */

template<typename T> T mix3(const float3 &weights, const T &v0, const T &v1, const T &v2);

template<> inline bool mix3(const float3 &weights, const bool &v0, const bool &v1, const bool &v2)
{
  return (weights.x * v0 + weights.y * v1 + weights.z * v2) >= 0.5f;
}

template<> inline int mix3(const float3 &weights, const int &v0, const int &v1, const int &v2)
{
  return static_cast<int>(weights.x * v0 + weights.y * v1 + weights.z * v2);
}

template<>
inline float mix3(const float3 &weights, const float &v0, const float &v1, const float &v2)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2;
}

template<>
inline float2 mix3(const float3 &weights, const float2 &v0, const float2 &v1, const float2 &v2)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2;
}

template<>
inline float3 mix3(const float3 &weights, const float3 &v0, const float3 &v1, const float3 &v2)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2;
}

template<>
inline Color4f mix3(const float3 &weights, const Color4f &v0, const Color4f &v1, const Color4f &v2)
{
  Color4f result;
  interp_v4_v4v4v4(result, v0, v1, v2, weights);
  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mix a dynamic amount of values with weights for many elements.
 *
 * This section provides an abstraction for "mixers". The abstraction encapsulates details about
 * how different types should be mixed. Usually #DefaultMixer<T> should be used to get a mixer for
 * a specific type.
 * \{ */

template<typename T> class SimpleMixer {
 private:
  MutableSpan<T> buffer_;
  T default_value_;
  Array<float> total_weights_;

 public:
  /**
   * \param buffer: Span where the interpolated values should be stored.
   * \param default_value: Output value for an element that has not been affected by a #mix_in.
   */
  SimpleMixer(MutableSpan<T> buffer, T default_value = {})
      : buffer_(buffer), default_value_(default_value), total_weights_(buffer.size(), 0.0f)
  {
    BLI_STATIC_ASSERT(std::is_trivial_v<T>, "");
    memset(buffer_.data(), 0, sizeof(T) * buffer_.size());
  }

  /**
   * Mix a #value into the element with the given #index.
   */
  void mix_in(const int64_t index, const T &value, const float weight = 1.0f)
  {
    BLI_assert(weight >= 0.0f);
    buffer_[index] += value * weight;
    total_weights_[index] += weight;
  }

  /**
   * Has to be called before the buffer provided in the constructor is used.
   */
  void finalize()
  {
    for (const int64_t i : buffer_.index_range()) {
      const float weight = total_weights_[i];
      if (weight > 0.0f) {
        buffer_[i] *= 1.0f / weight;
      }
      else {
        buffer_[i] = default_value_;
      }
    }
  }
};

/** This mixer accumulates values in a type that is different from the one that is mixed. Some
 * types cannot encode the floating point weights in their values (e.g. int and bool). */
template<typename T, typename AccumulationT, T (*ConvertToT)(const AccumulationT &value)>
class SimpleMixerWithAccumulationType {
 private:
  struct Item {
    /* Store both values together, because they are accessed together. */
    AccumulationT value = {0};
    float weight = 0.0f;
  };

  MutableSpan<T> buffer_;
  T default_value_;
  Array<Item> accumulation_buffer_;

 public:
  SimpleMixerWithAccumulationType(MutableSpan<T> buffer, T default_value = {})
      : buffer_(buffer), default_value_(default_value), accumulation_buffer_(buffer.size())
  {
  }

  void mix_in(const int64_t index, const T &value, const float weight = 1.0f)
  {
    const AccumulationT converted_value = static_cast<AccumulationT>(value);
    Item &item = accumulation_buffer_[index];
    item.value += converted_value * weight;
    item.weight += weight;
  }

  void finalize()
  {
    for (const int64_t i : buffer_.index_range()) {
      const Item &item = accumulation_buffer_[i];
      if (item.weight > 0.0f) {
        const float weight_inv = 1.0f / item.weight;
        const T converted_value = ConvertToT(item.value * weight_inv);
        buffer_[i] = converted_value;
      }
      else {
        buffer_[i] = default_value_;
      }
    }
  }
};

class Color4fMixer {
 private:
  MutableSpan<Color4f> buffer_;
  Color4f default_color_;
  Array<float> total_weights_;

 public:
  Color4fMixer(MutableSpan<Color4f> buffer, Color4f default_color = {0, 0, 0, 1});
  void mix_in(const int64_t index, const Color4f &color, const float weight = 1.0f);
  void finalize();
};

template<typename T> struct DefaultMixerStruct {
  /* Use void by default. This can be check for in `if constexpr` statements. */
  using type = void;
};
template<> struct DefaultMixerStruct<float> {
  using type = SimpleMixer<float>;
};
template<> struct DefaultMixerStruct<float2> {
  using type = SimpleMixer<float2>;
};
template<> struct DefaultMixerStruct<float3> {
  using type = SimpleMixer<float3>;
};
template<> struct DefaultMixerStruct<Color4f> {
  /* Use a special mixer for colors. Color4f can't be added/multiplied, because this is not
   * something one should usually do with colors.  */
  using type = Color4fMixer;
};
template<> struct DefaultMixerStruct<int> {
  static int double_to_int(const double &value)
  {
    return static_cast<int>(value);
  }
  /* Store interpolated ints in a double temporarily, so that weights are handled correctly. It
   * uses double instead of float so that it is accurate for all 32 bit integers. */
  using type = SimpleMixerWithAccumulationType<int, double, double_to_int>;
};
template<> struct DefaultMixerStruct<bool> {
  static bool float_to_bool(const float &value)
  {
    return value >= 0.5f;
  }
  /* Store interpolated booleans in a float temporary.
   * Otherwise information provided by weights is easily rounded away. */
  using type = SimpleMixerWithAccumulationType<bool, float, float_to_bool>;
};

/* Utility to get a good default mixer for a given type. This is `void` when there is no default
 * mixer for the given type. */
template<typename T> using DefaultMixer = typename DefaultMixerStruct<T>::type;

/** \} */

}  // namespace blender::attribute_math
