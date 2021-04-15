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

using fn::MFDataType;

template<typename From, typename To>
static void add_implicit_conversion(DataTypeConversions &conversions)
{
  static fn::CustomMF_Convert<From, To> function;
  conversions.add(fn::MFDataType::ForSingle<From>(), fn::MFDataType::ForSingle<To>(), function);
}

template<typename From, typename To, typename ConversionF>
static void add_implicit_conversion(DataTypeConversions &conversions,
                                    StringRef name,
                                    ConversionF conversion)
{
  static fn::CustomMF_SI_SO<From, To> function{name, conversion};
  conversions.add(fn::MFDataType::ForSingle<From>(), fn::MFDataType::ForSingle<To>(), function);
}

static DataTypeConversions create_implicit_conversions()
{
  DataTypeConversions conversions;
  add_implicit_conversion<float, float2>(conversions);
  add_implicit_conversion<float, float3>(conversions);
  add_implicit_conversion<float, int32_t>(conversions);
  add_implicit_conversion<float, bool>(
      conversions, "float to boolean", [](float a) { return a > 0.0f; });
  add_implicit_conversion<float, Color4f>(
      conversions, "float to Color4f", [](float a) { return Color4f(a, a, a, 1.0f); });

  add_implicit_conversion<float2, float3>(
      conversions, "float2 to float3", [](float2 a) { return float3(a.x, a.y, 0.0f); });
  add_implicit_conversion<float2, float>(
      conversions, "float2 to float", [](float2 a) { return (a.x + a.y) / 2.0f; });
  add_implicit_conversion<float2, int32_t>(
      conversions, "float2 to int32_t", [](float2 a) { return (int32_t)((a.x + a.y) / 2.0f); });
  add_implicit_conversion<float2, bool>(
      conversions, "float2 to bool", [](float2 a) { return !is_zero_v2(a); });
  add_implicit_conversion<float2, Color4f>(
      conversions, "float2 to Color4f", [](float2 a) { return Color4f(a.x, a.y, 0.0f, 1.0f); });

  add_implicit_conversion<float3, bool>(
      conversions, "float3 to boolean", [](float3 a) { return !is_zero_v3(a); });
  add_implicit_conversion<float3, float>(
      conversions, "float3 to float", [](float3 a) { return (a.x + a.y + a.z) / 3.0f; });
  add_implicit_conversion<float3, int32_t>(
      conversions, "float3 to int32_t", [](float3 a) { return (int)((a.x + a.y + a.z) / 3.0f); });
  add_implicit_conversion<float3, float2>(conversions);
  add_implicit_conversion<float3, Color4f>(
      conversions, "float3 to Color4f", [](float3 a) { return Color4f(a.x, a.y, a.z, 1.0f); });

  add_implicit_conversion<int32_t, bool>(
      conversions, "int32 to boolean", [](int32_t a) { return a > 0; });
  add_implicit_conversion<int32_t, float>(conversions);
  add_implicit_conversion<int32_t, float2>(
      conversions, "int32 to float2", [](int32_t a) { return float2((float)a); });
  add_implicit_conversion<int32_t, float3>(
      conversions, "int32 to float3", [](int32_t a) { return float3((float)a); });
  add_implicit_conversion<int32_t, Color4f>(conversions, "int32 to Color4f", [](int32_t a) {
    return Color4f((float)a, (float)a, (float)a, 1.0f);
  });

  add_implicit_conversion<bool, float>(conversions);
  add_implicit_conversion<bool, int32_t>(conversions);
  add_implicit_conversion<bool, float2>(
      conversions, "boolean to float2", [](bool a) { return (a) ? float2(1.0f) : float2(0.0f); });
  add_implicit_conversion<bool, float3>(
      conversions, "boolean to float3", [](bool a) { return (a) ? float3(1.0f) : float3(0.0f); });
  add_implicit_conversion<bool, Color4f>(conversions, "boolean to Color4f", [](bool a) {
    return (a) ? Color4f(1.0f, 1.0f, 1.0f, 1.0f) : Color4f(0.0f, 0.0f, 0.0f, 1.0f);
  });

  add_implicit_conversion<Color4f, bool>(
      conversions, "Color4f to boolean", [](Color4f a) { return rgb_to_grayscale(a) > 0.0f; });
  add_implicit_conversion<Color4f, float>(
      conversions, "Color4f to float", [](Color4f a) { return rgb_to_grayscale(a); });
  add_implicit_conversion<Color4f, float2>(
      conversions, "Color4f to float2", [](Color4f a) { return float2(a.r, a.g); });
  add_implicit_conversion<Color4f, float3>(
      conversions, "Color4f to float3", [](Color4f a) { return float3(a.r, a.g, a.b); });

  return conversions;
}

const DataTypeConversions &get_implicit_type_conversions()
{
  static const DataTypeConversions conversions = create_implicit_conversions();
  return conversions;
}

void DataTypeConversions::convert(const CPPType &from_type,
                                  const CPPType &to_type,
                                  const void *from_value,
                                  void *to_value) const
{
  const fn::MultiFunction *fn = this->get_conversion(MFDataType::ForSingle(from_type),
                                                     MFDataType::ForSingle(to_type));
  BLI_assert(fn != nullptr);

  fn::MFContextBuilder context;
  fn::MFParamsBuilder params{*fn, 1};
  params.add_readonly_single_input(fn::GSpan(from_type, from_value, 1));
  params.add_uninitialized_single_output(fn::GMutableSpan(to_type, to_value, 1));
  fn->call({0}, params, context);
}

}  // namespace blender::nodes
