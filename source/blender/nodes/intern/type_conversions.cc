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

#include "NOD_type_conversions.hh"

#include "FN_multi_function_builder.hh"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"

namespace blender::nodes {

using fn::GVArrayPtr;
using fn::GVMutableArray;
using fn::GVMutableArrayPtr;
using fn::MFDataType;

template<typename From, typename To, To (*ConversionF)(const From &)>
static void add_implicit_conversion(DataTypeConversions &conversions)
{
  const CPPType &from_type = CPPType::get<From>();
  const CPPType &to_type = CPPType::get<To>();
  const std::string conversion_name = from_type.name() + " to " + to_type.name();

  static fn::CustomMF_SI_SO<From, To> multi_function{conversion_name, ConversionF};
  static auto convert_single_to_initialized = [](const void *src, void *dst) {
    *(To *)dst = ConversionF(*(const From *)src);
  };
  static auto convert_single_to_uninitialized = [](const void *src, void *dst) {
    new (dst) To(ConversionF(*(const From *)src));
  };
  conversions.add(fn::MFDataType::ForSingle<From>(),
                  fn::MFDataType::ForSingle<To>(),
                  multi_function,
                  convert_single_to_initialized,
                  convert_single_to_uninitialized);
}

static float2 float_to_float2(const float &a)
{
  return float2(a);
}
static float3 float_to_float3(const float &a)
{
  return float3(a);
}
static int32_t float_to_int(const float &a)
{
  return (int32_t)a;
}
static bool float_to_bool(const float &a)
{
  return a > 0.0f;
}
static Color4f float_to_color(const float &a)
{
  return Color4f(a, a, a, 1.0f);
}

static float3 float2_to_float3(const float2 &a)
{
  return float3(a.x, a.y, 0.0f);
}
static float float2_to_float(const float2 &a)
{
  return (a.x + a.y) / 2.0f;
}
static int float2_to_int(const float2 &a)
{
  return (int32_t)((a.x + a.y) / 2.0f);
}
static bool float2_to_bool(const float2 &a)
{
  return !is_zero_v2(a);
}
static Color4f float2_to_color(const float2 &a)
{
  return Color4f(a.x, a.y, 0.0f, 1.0f);
}

static bool float3_to_bool(const float3 &a)
{
  return !is_zero_v3(a);
}
static float float3_to_float(const float3 &a)
{
  return (a.x + a.y + a.z) / 3.0f;
}
static int float3_to_int(const float3 &a)
{
  return (int)((a.x + a.y + a.z) / 3.0f);
}
static float2 float3_to_float2(const float3 &a)
{
  return float2(a);
}
static Color4f float3_to_color(const float3 &a)
{
  return Color4f(a.x, a.y, a.z, 1.0f);
}

static bool int_to_bool(const int32_t &a)
{
  return a > 0;
}
static float int_to_float(const int32_t &a)
{
  return (float)a;
}
static float2 int_to_float2(const int32_t &a)
{
  return float2((float)a);
}
static float3 int_to_float3(const int32_t &a)
{
  return float3((float)a);
}
static Color4f int_to_color(const int32_t &a)
{
  return Color4f((float)a, (float)a, (float)a, 1.0f);
}

static float bool_to_float(const bool &a)
{
  return (bool)a;
}
static int32_t bool_to_int(const bool &a)
{
  return (int32_t)a;
}
static float2 bool_to_float2(const bool &a)
{
  return (a) ? float2(1.0f) : float2(0.0f);
}
static float3 bool_to_float3(const bool &a)
{
  return (a) ? float3(1.0f) : float3(0.0f);
}
static Color4f bool_to_color(const bool &a)
{
  return (a) ? Color4f(1.0f, 1.0f, 1.0f, 1.0f) : Color4f(0.0f, 0.0f, 0.0f, 1.0f);
}

static bool color_to_bool(const Color4f &a)
{
  return rgb_to_grayscale(a) > 0.0f;
}
static float color_to_float(const Color4f &a)
{
  return rgb_to_grayscale(a);
}
static int32_t color_to_int(const Color4f &a)
{
  return (int)rgb_to_grayscale(a);
}
static float2 color_to_float2(const Color4f &a)
{
  return float2(a.r, a.g);
}
static float3 color_to_float3(const Color4f &a)
{
  return float3(a.r, a.g, a.b);
}

static DataTypeConversions create_implicit_conversions()
{
  DataTypeConversions conversions;

  add_implicit_conversion<float, float2, float_to_float2>(conversions);
  add_implicit_conversion<float, float3, float_to_float3>(conversions);
  add_implicit_conversion<float, int32_t, float_to_int>(conversions);
  add_implicit_conversion<float, bool, float_to_bool>(conversions);
  add_implicit_conversion<float, Color4f, float_to_color>(conversions);

  add_implicit_conversion<float2, float3, float2_to_float3>(conversions);
  add_implicit_conversion<float2, float, float2_to_float>(conversions);
  add_implicit_conversion<float2, int32_t, float2_to_int>(conversions);
  add_implicit_conversion<float2, bool, float2_to_bool>(conversions);
  add_implicit_conversion<float2, Color4f, float2_to_color>(conversions);

  add_implicit_conversion<float3, bool, float3_to_bool>(conversions);
  add_implicit_conversion<float3, float, float3_to_float>(conversions);
  add_implicit_conversion<float3, int32_t, float3_to_int>(conversions);
  add_implicit_conversion<float3, float2, float3_to_float2>(conversions);
  add_implicit_conversion<float3, Color4f, float3_to_color>(conversions);

  add_implicit_conversion<int32_t, bool, int_to_bool>(conversions);
  add_implicit_conversion<int32_t, float, int_to_float>(conversions);
  add_implicit_conversion<int32_t, float2, int_to_float2>(conversions);
  add_implicit_conversion<int32_t, float3, int_to_float3>(conversions);
  add_implicit_conversion<int32_t, Color4f, int_to_color>(conversions);

  add_implicit_conversion<bool, float, bool_to_float>(conversions);
  add_implicit_conversion<bool, int32_t, bool_to_int>(conversions);
  add_implicit_conversion<bool, float2, bool_to_float2>(conversions);
  add_implicit_conversion<bool, float3, bool_to_float3>(conversions);
  add_implicit_conversion<bool, Color4f, bool_to_color>(conversions);

  add_implicit_conversion<Color4f, bool, color_to_bool>(conversions);
  add_implicit_conversion<Color4f, float, color_to_float>(conversions);
  add_implicit_conversion<Color4f, int32_t, color_to_int>(conversions);
  add_implicit_conversion<Color4f, float2, color_to_float2>(conversions);
  add_implicit_conversion<Color4f, float3, color_to_float3>(conversions);

  return conversions;
}

const DataTypeConversions &get_implicit_type_conversions()
{
  static const DataTypeConversions conversions = create_implicit_conversions();
  return conversions;
}

void DataTypeConversions::convert_to_uninitialized(const CPPType &from_type,
                                                   const CPPType &to_type,
                                                   const void *from_value,
                                                   void *to_value) const
{
  if (from_type == to_type) {
    from_type.copy_to_uninitialized(from_value, to_value);
    return;
  }

  const ConversionFunctions *functions = this->get_conversion_functions(
      MFDataType::ForSingle(from_type), MFDataType::ForSingle(to_type));
  BLI_assert(functions != nullptr);

  functions->convert_single_to_uninitialized(from_value, to_value);
}

class GVArray_For_ConvertedGVArray : public GVArray {
 private:
  GVArrayPtr varray_;
  const CPPType &from_type_;
  ConversionFunctions old_to_new_conversions_;

 public:
  GVArray_For_ConvertedGVArray(GVArrayPtr varray,
                               const CPPType &to_type,
                               const DataTypeConversions &conversions)
      : GVArray(to_type, varray->size()), varray_(std::move(varray)), from_type_(varray_->type())
  {
    old_to_new_conversions_ = *conversions.get_conversion_functions(from_type_, to_type);
  }

 private:
  void get_impl(const int64_t index, void *r_value) const override
  {
    BUFFER_FOR_CPP_TYPE_VALUE(from_type_, buffer);
    varray_->get(index, buffer);
    old_to_new_conversions_.convert_single_to_initialized(buffer, r_value);
    from_type_.destruct(buffer);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    BUFFER_FOR_CPP_TYPE_VALUE(from_type_, buffer);
    varray_->get(index, buffer);
    old_to_new_conversions_.convert_single_to_uninitialized(buffer, r_value);
    from_type_.destruct(buffer);
  }
};

class GVMutableArray_For_ConvertedGVMutableArray : public GVMutableArray {
 private:
  GVMutableArrayPtr varray_;
  const CPPType &from_type_;
  ConversionFunctions old_to_new_conversions_;
  ConversionFunctions new_to_old_conversions_;

 public:
  GVMutableArray_For_ConvertedGVMutableArray(GVMutableArrayPtr varray,
                                             const CPPType &to_type,
                                             const DataTypeConversions &conversions)
      : GVMutableArray(to_type, varray->size()),
        varray_(std::move(varray)),
        from_type_(varray_->type())
  {
    old_to_new_conversions_ = *conversions.get_conversion_functions(from_type_, to_type);
    new_to_old_conversions_ = *conversions.get_conversion_functions(to_type, from_type_);
  }

 private:
  void get_impl(const int64_t index, void *r_value) const override
  {
    BUFFER_FOR_CPP_TYPE_VALUE(from_type_, buffer);
    varray_->get(index, buffer);
    old_to_new_conversions_.convert_single_to_initialized(buffer, r_value);
    from_type_.destruct(buffer);
  }

  void get_to_uninitialized_impl(const int64_t index, void *r_value) const override
  {
    BUFFER_FOR_CPP_TYPE_VALUE(from_type_, buffer);
    varray_->get(index, buffer);
    old_to_new_conversions_.convert_single_to_uninitialized(buffer, r_value);
    from_type_.destruct(buffer);
  }

  void set_by_move_impl(const int64_t index, void *value) override
  {
    BUFFER_FOR_CPP_TYPE_VALUE(from_type_, buffer);
    new_to_old_conversions_.convert_single_to_uninitialized(value, buffer);
    varray_->set_by_relocate(index, buffer);
  }
};

fn::GVArrayPtr DataTypeConversions::try_convert(fn::GVArrayPtr varray,
                                                const CPPType &to_type) const
{
  const CPPType &from_type = varray->type();
  if (from_type == to_type) {
    return varray;
  }
  if (!this->is_convertible(from_type, to_type)) {
    return {};
  }
  return std::make_unique<GVArray_For_ConvertedGVArray>(std::move(varray), to_type, *this);
}

fn::GVMutableArrayPtr DataTypeConversions::try_convert(fn::GVMutableArrayPtr varray,
                                                       const CPPType &to_type) const
{
  const CPPType &from_type = varray->type();
  if (from_type == to_type) {
    return varray;
  }
  if (!this->is_convertible(from_type, to_type)) {
    return {};
  }
  return std::make_unique<GVMutableArray_For_ConvertedGVMutableArray>(
      std::move(varray), to_type, *this);
}

}  // namespace blender::nodes
