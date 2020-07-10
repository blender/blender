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

#include "FN_multi_function_builder.hh"

#include "BLI_hash.hh"

namespace blender::fn {

CustomMF_GenericConstant::CustomMF_GenericConstant(const CPPType &type, const void *value)
    : type_(type), value_(value)
{
  MFSignatureBuilder signature = this->get_builder("Constant " + type.name());
  std::stringstream ss;
  type.debug_print(value, ss);
  signature.single_output(ss.str(), type);
}

void CustomMF_GenericConstant::call(IndexMask mask,
                                    MFParams params,
                                    MFContext UNUSED(context)) const
{
  GMutableSpan output = params.uninitialized_single_output(0);
  type_.fill_uninitialized_indices(value_, output.buffer(), mask);
}

uint CustomMF_GenericConstant::hash() const
{
  return type_.hash(value_);
}

bool CustomMF_GenericConstant::equals(const MultiFunction &other) const
{
  const CustomMF_GenericConstant *_other = dynamic_cast<const CustomMF_GenericConstant *>(&other);
  if (_other == nullptr) {
    return false;
  }
  if (type_ != _other->type_) {
    return false;
  }
  return type_.is_equal(value_, _other->value_);
}

static std::string gspan_to_string(GSpan array)
{
  std::stringstream ss;
  ss << "[";
  uint max_amount = 5;
  for (uint i : IndexRange(std::min(max_amount, array.size()))) {
    array.type().debug_print(array[i], ss);
    ss << ", ";
  }
  if (max_amount < array.size()) {
    ss << "...";
  }
  ss << "]";
  return ss.str();
}

CustomMF_GenericConstantArray::CustomMF_GenericConstantArray(GSpan array) : array_(array)
{
  const CPPType &type = array.type();
  MFSignatureBuilder signature = this->get_builder("Constant " + type.name() + " Vector");
  signature.vector_output(gspan_to_string(array), type);
}

void CustomMF_GenericConstantArray::call(IndexMask mask,
                                         MFParams params,
                                         MFContext UNUSED(context)) const
{
  GVectorArray &vectors = params.vector_output(0);
  for (uint i : mask) {
    vectors.extend(i, array_);
  }
}

}  // namespace blender::fn
