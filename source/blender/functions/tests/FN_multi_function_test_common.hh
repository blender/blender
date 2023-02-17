/* SPDX-License-Identifier: Apache-2.0 */

#include "FN_multi_function.hh"

namespace blender::fn::multi_function::tests {

class AddPrefixFunction : public MultiFunction {
 public:
  AddPrefixFunction()
  {
    static const Signature signature = []() {
      Signature signature;
      SignatureBuilder builder{"Add Prefix", signature};
      builder.single_input<std::string>("Prefix");
      builder.single_mutable<std::string>("Strings");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    const VArray<std::string> &prefixes = params.readonly_single_input<std::string>(0, "Prefix");
    MutableSpan<std::string> strings = params.single_mutable<std::string>(1, "Strings");

    for (int64_t i : mask) {
      strings[i] = prefixes[i] + strings[i];
    }
  }
};

class CreateRangeFunction : public MultiFunction {
 public:
  CreateRangeFunction()
  {
    static const Signature signature = []() {
      Signature signature;
      SignatureBuilder builder{"Create Range", signature};
      builder.single_input<int>("Size");
      builder.vector_output<int>("Range");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    const VArray<int> &sizes = params.readonly_single_input<int>(0, "Size");
    GVectorArray &ranges = params.vector_output(1, "Range");

    for (int64_t i : mask) {
      int size = sizes[i];
      for (int j : IndexRange(size)) {
        ranges.append(i, &j);
      }
    }
  }
};

class GenericAppendFunction : public MultiFunction {
 private:
  Signature signature_;

 public:
  GenericAppendFunction(const CPPType &type)
  {
    SignatureBuilder builder{"Append", signature_};
    builder.vector_mutable("Vector", type);
    builder.single_input("Value", type);
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    GVectorArray &vectors = params.vector_mutable(0, "Vector");
    const GVArray &values = params.readonly_single_input(1, "Value");

    for (int64_t i : mask) {
      BUFFER_FOR_CPP_TYPE_VALUE(values.type(), buffer);
      values.get(i, buffer);
      vectors.append(i, buffer);
      values.type().destruct(buffer);
    }
  }
};

class ConcatVectorsFunction : public MultiFunction {
 public:
  ConcatVectorsFunction()
  {
    static const Signature signature = []() {
      Signature signature;
      SignatureBuilder builder{"Concat Vectors", signature};
      builder.vector_mutable<int>("A");
      builder.vector_input<int>("B");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    GVectorArray &a = params.vector_mutable(0);
    const GVVectorArray &b = params.readonly_vector_input(1);
    a.extend(mask, b);
  }
};

class AppendFunction : public MultiFunction {
 public:
  AppendFunction()
  {
    static const Signature signature = []() {
      Signature signature;
      SignatureBuilder builder{"Append", signature};
      builder.vector_mutable<int>("Vector");
      builder.single_input<int>("Value");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    GVectorArray_TypedMutableRef<int> vectors = params.vector_mutable<int>(0);
    const VArray<int> &values = params.readonly_single_input<int>(1);

    for (int64_t i : mask) {
      vectors.append(i, values[i]);
    }
  }
};

class SumVectorFunction : public MultiFunction {
 public:
  SumVectorFunction()
  {
    static const Signature signature = []() {
      Signature signature;
      SignatureBuilder builder{"Sum Vectors", signature};
      builder.vector_input<int>("Vector");
      builder.single_output<int>("Sum");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    const VVectorArray<int> &vectors = params.readonly_vector_input<int>(0);
    MutableSpan<int> sums = params.uninitialized_single_output<int>(1);

    for (int64_t i : mask) {
      int sum = 0;
      for (int j : IndexRange(vectors.get_vector_size(i))) {
        sum += vectors.get_vector_element(i, j);
      }
      sums[i] = sum;
    }
  }
};

class OptionalOutputsFunction : public MultiFunction {
 public:
  OptionalOutputsFunction()
  {
    static const Signature signature = []() {
      Signature signature;
      SignatureBuilder builder{"Optional Outputs", signature};
      builder.single_output<int>("Out 1");
      builder.single_output<std::string>("Out 2");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(IndexMask mask, Params params, Context /*context*/) const override
  {
    if (params.single_output_is_required(0, "Out 1")) {
      MutableSpan<int> values = params.uninitialized_single_output<int>(0, "Out 1");
      values.fill_indices(mask, 5);
    }
    MutableSpan<std::string> values = params.uninitialized_single_output<std::string>(1, "Out 2");
    for (const int i : mask) {
      new (&values[i]) std::string("hello, this is a long string");
    }
  }
};

}  // namespace blender::fn::multi_function::tests
