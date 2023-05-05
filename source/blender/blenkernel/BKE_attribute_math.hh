/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_color.hh"
#include "BLI_cpp_type.hh"
#include "BLI_math_color.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "BKE_customdata.h"

namespace blender::bke::attribute_math {

/**
 * Utility function that simplifies calling a templated function based on a run-time data type.
 */
template<typename Func>
inline void convert_to_static_type(const CPPType &cpp_type, const Func &func)
{
  cpp_type.to_static_type_tag<float,
                              float2,
                              float3,
                              int,
                              int2,
                              bool,
                              int8_t,
                              ColorGeometry4f,
                              ColorGeometry4b>([&](auto type_tag) {
    using T = typename decltype(type_tag)::type;
    if constexpr (std::is_same_v<T, void>) {
      /* It's expected that the given cpp type is one of the supported ones. */
      BLI_assert_unreachable();
    }
    else {
      func(T());
    }
  });
}

template<typename Func>
inline void convert_to_static_type(const eCustomDataType data_type, const Func &func)
{
  const CPPType &cpp_type = *bke::custom_data_type_to_cpp_type(data_type);
  convert_to_static_type(cpp_type, func);
}

/* -------------------------------------------------------------------- */
/** \name Mix two values of the same type.
 *
 * This is just basic linear interpolation.
 * \{ */

template<typename T> T mix2(float factor, const T &a, const T &b);

template<> inline bool mix2(const float factor, const bool &a, const bool &b)
{
  return ((1.0f - factor) * a + factor * b) >= 0.5f;
}

template<> inline int8_t mix2(const float factor, const int8_t &a, const int8_t &b)
{
  return int8_t(std::round((1.0f - factor) * a + factor * b));
}

template<> inline int mix2(const float factor, const int &a, const int &b)
{
  return int(std::round((1.0f - factor) * a + factor * b));
}

template<> inline int2 mix2(const float factor, const int2 &a, const int2 &b)
{
  return math::interpolate(a, b, factor);
}

template<> inline float mix2(const float factor, const float &a, const float &b)
{
  return (1.0f - factor) * a + factor * b;
}

template<> inline float2 mix2(const float factor, const float2 &a, const float2 &b)
{
  return math::interpolate(a, b, factor);
}

template<> inline float3 mix2(const float factor, const float3 &a, const float3 &b)
{
  return math::interpolate(a, b, factor);
}

template<>
inline ColorGeometry4f mix2(const float factor, const ColorGeometry4f &a, const ColorGeometry4f &b)
{
  return math::interpolate(a, b, factor);
}

template<>
inline ColorGeometry4b mix2(const float factor, const ColorGeometry4b &a, const ColorGeometry4b &b)
{
  return math::interpolate(a, b, factor);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mix three values of the same type.
 *
 * This is typically used to interpolate values within a triangle.
 * \{ */

template<typename T> T mix3(const float3 &weights, const T &v0, const T &v1, const T &v2);

template<>
inline int8_t mix3(const float3 &weights, const int8_t &v0, const int8_t &v1, const int8_t &v2)
{
  return int8_t(std::round(weights.x * v0 + weights.y * v1 + weights.z * v2));
}

template<> inline bool mix3(const float3 &weights, const bool &v0, const bool &v1, const bool &v2)
{
  return (weights.x * v0 + weights.y * v1 + weights.z * v2) >= 0.5f;
}

template<> inline int mix3(const float3 &weights, const int &v0, const int &v1, const int &v2)
{
  return int(std::round(weights.x * v0 + weights.y * v1 + weights.z * v2));
}

template<> inline int2 mix3(const float3 &weights, const int2 &v0, const int2 &v1, const int2 &v2)
{
  return int2(weights.x * float2(v0) + weights.y * float2(v1) + weights.z * float2(v2));
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
inline ColorGeometry4f mix3(const float3 &weights,
                            const ColorGeometry4f &v0,
                            const ColorGeometry4f &v1,
                            const ColorGeometry4f &v2)
{
  ColorGeometry4f result;
  interp_v4_v4v4v4(result, v0, v1, v2, weights);
  return result;
}

template<>
inline ColorGeometry4b mix3(const float3 &weights,
                            const ColorGeometry4b &v0,
                            const ColorGeometry4b &v1,
                            const ColorGeometry4b &v2)
{
  const float4 v0_f{&v0.r};
  const float4 v1_f{&v1.r};
  const float4 v2_f{&v2.r};
  const float4 mixed = v0_f * weights[0] + v1_f * weights[1] + v2_f * weights[2];
  return ColorGeometry4b{
      uint8_t(mixed[0]), uint8_t(mixed[1]), uint8_t(mixed[2]), uint8_t(mixed[3])};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mix four values of the same type.
 *
 * \{ */

template<typename T>
T mix4(const float4 &weights, const T &v0, const T &v1, const T &v2, const T &v3);

template<>
inline int8_t mix4(
    const float4 &weights, const int8_t &v0, const int8_t &v1, const int8_t &v2, const int8_t &v3)
{
  return int8_t(std::round(weights.x * v0 + weights.y * v1 + weights.z * v2 + weights.w * v3));
}

template<>
inline bool mix4(
    const float4 &weights, const bool &v0, const bool &v1, const bool &v2, const bool &v3)
{
  return (weights.x * v0 + weights.y * v1 + weights.z * v2 + weights.w * v3) >= 0.5f;
}

template<>
inline int mix4(const float4 &weights, const int &v0, const int &v1, const int &v2, const int &v3)
{
  return int(std::round(weights.x * v0 + weights.y * v1 + weights.z * v2 + weights.w * v3));
}

template<>
inline int2 mix4(
    const float4 &weights, const int2 &v0, const int2 &v1, const int2 &v2, const int2 &v3)
{
  return int2(weights.x * float2(v0) + weights.y * float2(v1) + weights.z * float2(v2) +
              weights.w * float2(v3));
}

template<>
inline float mix4(
    const float4 &weights, const float &v0, const float &v1, const float &v2, const float &v3)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2 + weights.w * v3;
}

template<>
inline float2 mix4(
    const float4 &weights, const float2 &v0, const float2 &v1, const float2 &v2, const float2 &v3)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2 + weights.w * v3;
}

template<>
inline float3 mix4(
    const float4 &weights, const float3 &v0, const float3 &v1, const float3 &v2, const float3 &v3)
{
  return weights.x * v0 + weights.y * v1 + weights.z * v2 + weights.w * v3;
}

template<>
inline ColorGeometry4f mix4(const float4 &weights,
                            const ColorGeometry4f &v0,
                            const ColorGeometry4f &v1,
                            const ColorGeometry4f &v2,
                            const ColorGeometry4f &v3)
{
  ColorGeometry4f result;
  interp_v4_v4v4v4v4(result, v0, v1, v2, v3, weights);
  return result;
}

template<>
inline ColorGeometry4b mix4(const float4 &weights,
                            const ColorGeometry4b &v0,
                            const ColorGeometry4b &v1,
                            const ColorGeometry4b &v2,
                            const ColorGeometry4b &v3)
{
  const float4 v0_f{&v0.r};
  const float4 v1_f{&v1.r};
  const float4 v2_f{&v2.r};
  const float4 v3_f{&v3.r};
  float4 mixed;
  interp_v4_v4v4v4v4(mixed, v0_f, v1_f, v2_f, v3_f, weights);
  return ColorGeometry4b{
      uint8_t(mixed[0]), uint8_t(mixed[1]), uint8_t(mixed[2]), uint8_t(mixed[3])};
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
      : SimpleMixer(buffer, buffer.index_range(), default_value)
  {
  }

  /**
   * \param mask: Only initialize these indices. Other indices in the buffer will be invalid.
   */
  SimpleMixer(MutableSpan<T> buffer, const IndexMask mask, T default_value = {})
      : buffer_(buffer), default_value_(default_value), total_weights_(buffer.size(), 0.0f)
  {
    BLI_STATIC_ASSERT(std::is_trivial_v<T>, "");
    mask.foreach_index([&](const int64_t i) { buffer_[i] = default_value_; });
  }

  /**
   * Set a #value into the element with the given #index.
   */
  void set(const int64_t index, const T &value, const float weight = 1.0f)
  {
    buffer_[index] = value * weight;
    total_weights_[index] = weight;
  }

  /**
   * Mix a #value into the element with the given #index.
   */
  void mix_in(const int64_t index, const T &value, const float weight = 1.0f)
  {
    buffer_[index] += value * weight;
    total_weights_[index] += weight;
  }

  /**
   * Has to be called before the buffer provided in the constructor is used.
   */
  void finalize()
  {
    this->finalize(IndexMask(buffer_.size()));
  }

  void finalize(const IndexMask mask)
  {
    mask.foreach_index([&](const int64_t i) {
      const float weight = total_weights_[i];
      if (weight > 0.0f) {
        buffer_[i] *= 1.0f / weight;
      }
      else {
        buffer_[i] = default_value_;
      }
    });
  }
};

/**
 * Mixes together booleans with "or" while fitting the same interface as the other
 * mixers in order to be simpler to use. This mixing method has a few benefits:
 *  - An "average" for selections is relatively meaningless.
 *  - Predictable selection propagation is very super important.
 *  - It's generally  easier to remove an element from a selection that is slightly too large than
 *    the opposite.
 */
class BooleanPropagationMixer {
 private:
  MutableSpan<bool> buffer_;

 public:
  /**
   * \param buffer: Span where the interpolated values should be stored.
   */
  BooleanPropagationMixer(MutableSpan<bool> buffer)
      : BooleanPropagationMixer(buffer, buffer.index_range())
  {
  }

  /**
   * \param mask: Only initialize these indices. Other indices in the buffer will be invalid.
   */
  BooleanPropagationMixer(MutableSpan<bool> buffer, const IndexMask mask) : buffer_(buffer)
  {
    mask.foreach_index([&](const int64_t i) { buffer_[i] = false; });
  }

  /**
   * Set a #value into the element with the given #index.
   */
  void set(const int64_t index, const bool value, [[maybe_unused]] const float weight = 1.0f)
  {
    buffer_[index] = value;
  }

  /**
   * Mix a #value into the element with the given #index.
   */
  void mix_in(const int64_t index, const bool value, [[maybe_unused]] const float weight = 1.0f)
  {
    buffer_[index] |= value;
  }

  /**
   * Does not do anything, since the mixing is trivial.
   */
  void finalize() {}

  void finalize(const IndexMask /*mask*/) {}
};

/**
 * This mixer accumulates values in a type that is different from the one that is mixed.
 * Some types cannot encode the floating point weights in their values (e.g. int and bool).
 */
template<typename T, typename AccumulationT, T (*ConvertToT)(const AccumulationT &value)>
class SimpleMixerWithAccumulationType {
 private:
  struct Item {
    /* Store both values together, because they are accessed together. */
    AccumulationT value = AccumulationT(0);
    float weight = 0.0f;
  };

  MutableSpan<T> buffer_;
  T default_value_;
  Array<Item> accumulation_buffer_;

 public:
  SimpleMixerWithAccumulationType(MutableSpan<T> buffer, T default_value = {})
      : SimpleMixerWithAccumulationType(buffer, buffer.index_range(), default_value)
  {
  }

  /**
   * \param mask: Only initialize these indices. Other indices in the buffer will be invalid.
   */
  SimpleMixerWithAccumulationType(MutableSpan<T> buffer,
                                  const IndexMask mask,
                                  T default_value = {})
      : buffer_(buffer), default_value_(default_value), accumulation_buffer_(buffer.size())
  {
    mask.foreach_index([&](const int64_t index) { buffer_[index] = default_value_; });
  }

  void set(const int64_t index, const T &value, const float weight = 1.0f)
  {
    const AccumulationT converted_value = static_cast<AccumulationT>(value);
    Item &item = accumulation_buffer_[index];
    item.value = converted_value * weight;
    item.weight = weight;
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
    this->finalize(buffer_.index_range());
  }

  void finalize(const IndexMask mask)
  {
    mask.foreach_index([&](const int64_t i) {
      const Item &item = accumulation_buffer_[i];
      if (item.weight > 0.0f) {
        const float weight_inv = 1.0f / item.weight;
        const T converted_value = ConvertToT(item.value * weight_inv);
        buffer_[i] = converted_value;
      }
      else {
        buffer_[i] = default_value_;
      }
    });
  }
};

class ColorGeometry4fMixer {
 private:
  MutableSpan<ColorGeometry4f> buffer_;
  ColorGeometry4f default_color_;
  Array<float> total_weights_;

 public:
  ColorGeometry4fMixer(MutableSpan<ColorGeometry4f> buffer,
                       ColorGeometry4f default_color = ColorGeometry4f(0.0f, 0.0f, 0.0f, 1.0f));
  /**
   * \param mask: Only initialize these indices. Other indices in the buffer will be invalid.
   */
  ColorGeometry4fMixer(MutableSpan<ColorGeometry4f> buffer,
                       IndexMask mask,
                       ColorGeometry4f default_color = ColorGeometry4f(0.0f, 0.0f, 0.0f, 1.0f));
  void set(int64_t index, const ColorGeometry4f &color, float weight = 1.0f);
  void mix_in(int64_t index, const ColorGeometry4f &color, float weight = 1.0f);
  void finalize();
  void finalize(IndexMask mask);
};

class ColorGeometry4bMixer {
 private:
  MutableSpan<ColorGeometry4b> buffer_;
  ColorGeometry4b default_color_;
  Array<float> total_weights_;
  Array<float4> accumulation_buffer_;

 public:
  ColorGeometry4bMixer(MutableSpan<ColorGeometry4b> buffer,
                       ColorGeometry4b default_color = ColorGeometry4b(0, 0, 0, 255));
  /**
   * \param mask: Only initialize these indices. Other indices in the buffer will be invalid.
   */
  ColorGeometry4bMixer(MutableSpan<ColorGeometry4b> buffer,
                       IndexMask mask,
                       ColorGeometry4b default_color = ColorGeometry4b(0, 0, 0, 255));
  void set(int64_t index, const ColorGeometry4b &color, float weight = 1.0f);
  void mix_in(int64_t index, const ColorGeometry4b &color, float weight = 1.0f);
  void finalize();
  void finalize(IndexMask mask);
};

template<typename T> struct DefaultMixerStruct {
  /* Use void by default. This can be checked for in `if constexpr` statements. */
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
template<> struct DefaultMixerStruct<ColorGeometry4f> {
  /* Use a special mixer for colors. ColorGeometry4f can't be added/multiplied, because this is not
   * something one should usually do with colors. */
  using type = ColorGeometry4fMixer;
};
template<> struct DefaultMixerStruct<ColorGeometry4b> {
  using type = ColorGeometry4bMixer;
};
template<> struct DefaultMixerStruct<int> {
  static int double_to_int(const double &value)
  {
    return int(std::round(value));
  }
  /* Store interpolated ints in a double temporarily, so that weights are handled correctly. It
   * uses double instead of float so that it is accurate for all 32 bit integers. */
  using type = SimpleMixerWithAccumulationType<int, double, double_to_int>;
};
template<> struct DefaultMixerStruct<int2> {
  static int2 double_to_int(const double2 &value)
  {
    return int2(math::round(value));
  }
  /* Store interpolated ints in a double temporarily, so that weights are handled correctly. It
   * uses double instead of float so that it is accurate for all 32 bit integers. */
  using type = SimpleMixerWithAccumulationType<int2, double2, double_to_int>;
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

template<> struct DefaultMixerStruct<int8_t> {
  static int8_t float_to_int8_t(const float &value)
  {
    return int8_t(std::round(value));
  }
  /* Store interpolated 8 bit integers in a float temporarily to increase accuracy. */
  using type = SimpleMixerWithAccumulationType<int8_t, float, float_to_int8_t>;
};

template<typename T> struct DefaultPropagationMixerStruct {
  /* Use void by default. This can be checked for in `if constexpr` statements. */
  using type = typename DefaultMixerStruct<T>::type;
};

template<> struct DefaultPropagationMixerStruct<bool> {
  using type = BooleanPropagationMixer;
};

/**
 * This mixer is meant for propagating attributes when creating new geometry. A key difference
 * with the default mixer is that booleans are mixed with "or" instead of "at least half"
 * (the default mixing for booleans).
 */
template<typename T>
using DefaultPropagationMixer = typename DefaultPropagationMixerStruct<T>::type;

/* Utility to get a good default mixer for a given type. This is `void` when there is no default
 * mixer for the given type. */
template<typename T> using DefaultMixer = typename DefaultMixerStruct<T>::type;

/** \} */

}  // namespace blender::bke::attribute_math
