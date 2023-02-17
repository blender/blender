/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_builder.hh"

#include "BLI_hash.hh"

namespace blender::fn::multi_function {

CustomMF_GenericConstant::CustomMF_GenericConstant(const CPPType &type,
                                                   const void *value,
                                                   bool make_value_copy)
    : type_(type), owns_value_(make_value_copy)
{
  if (make_value_copy) {
    void *copied_value = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
    type.copy_construct(value, copied_value);
    value = copied_value;
  }
  value_ = value;

  SignatureBuilder builder{"Constant", signature_};
  builder.single_output("Value", type);
  this->set_signature(&signature_);
}

CustomMF_GenericConstant::~CustomMF_GenericConstant()
{
  if (owns_value_) {
    signature_.params[0].type.data_type().single_type().destruct(const_cast<void *>(value_));
    MEM_freeN(const_cast<void *>(value_));
  }
}

void CustomMF_GenericConstant::call(IndexMask mask, Params params, Context /*context*/) const
{
  GMutableSpan output = params.uninitialized_single_output(0);
  type_.fill_construct_indices(value_, output.data(), mask);
}

uint64_t CustomMF_GenericConstant::hash() const
{
  return type_.hash_or_fallback(value_, uintptr_t(this));
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

CustomMF_GenericConstantArray::CustomMF_GenericConstantArray(GSpan array) : array_(array)
{
  const CPPType &type = array.type();
  SignatureBuilder builder{"Constant Vector", signature_};
  builder.vector_output("Value", type);
  this->set_signature(&signature_);
}

void CustomMF_GenericConstantArray::call(IndexMask mask, Params params, Context /*context*/) const
{
  GVectorArray &vectors = params.vector_output(0);
  for (int64_t i : mask) {
    vectors.extend(i, array_);
  }
}

CustomMF_DefaultOutput::CustomMF_DefaultOutput(Span<DataType> input_types,
                                               Span<DataType> output_types)
    : output_amount_(output_types.size())
{
  SignatureBuilder builder{"Default Output", signature_};
  for (DataType data_type : input_types) {
    builder.input("Input", data_type);
  }
  for (DataType data_type : output_types) {
    builder.output("Output", data_type);
  }
  this->set_signature(&signature_);
}
void CustomMF_DefaultOutput::call(IndexMask mask, Params params, Context /*context*/) const
{
  for (int param_index : this->param_indices()) {
    ParamType param_type = this->param_type(param_index);
    if (!param_type.is_output()) {
      continue;
    }

    if (param_type.data_type().is_single()) {
      GMutableSpan span = params.uninitialized_single_output(param_index);
      const CPPType &type = span.type();
      type.fill_construct_indices(type.default_value(), span.data(), mask);
    }
  }
}

CustomMF_GenericCopy::CustomMF_GenericCopy(DataType data_type)
{
  SignatureBuilder builder{"Copy", signature_};
  builder.input("Input", data_type);
  builder.output("Output", data_type);
  this->set_signature(&signature_);
}

void CustomMF_GenericCopy::call(IndexMask mask, Params params, Context /*context*/) const
{
  const DataType data_type = this->param_type(0).data_type();
  switch (data_type.category()) {
    case DataType::Single: {
      const GVArray &inputs = params.readonly_single_input(0, "Input");
      GMutableSpan outputs = params.uninitialized_single_output(1, "Output");
      inputs.materialize_to_uninitialized(mask, outputs.data());
      break;
    }
    case DataType::Vector: {
      const GVVectorArray &inputs = params.readonly_vector_input(0, "Input");
      GVectorArray &outputs = params.vector_output(1, "Output");
      outputs.extend(mask, inputs);
      break;
    }
  }
}

}  // namespace blender::fn::multi_function
