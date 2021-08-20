/* Apache License, Version 2.0 */

#include "FN_multi_function.hh"

namespace blender::fn::tests {

class AddPrefixFunction : public MultiFunction {
 public:
  AddPrefixFunction()
  {
    static MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static MFSignature create_signature()
  {
    MFSignatureBuilder signature{"Add Prefix"};
    signature.single_input<std::string>("Prefix");
    signature.single_mutable<std::string>("Strings");
    return signature.build();
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
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
    static MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static MFSignature create_signature()
  {
    MFSignatureBuilder signature{"Create Range"};
    signature.single_input<int>("Size");
    signature.vector_output<int>("Range");
    return signature.build();
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
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
  MFSignature signature_;

 public:
  GenericAppendFunction(const CPPType &type)
  {
    MFSignatureBuilder signature{"Append"};
    signature.vector_mutable("Vector", type);
    signature.single_input("Value", type);
    signature_ = signature.build();
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
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
    static MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static MFSignature create_signature()
  {
    MFSignatureBuilder signature{"Concat Vectors"};
    signature.vector_mutable<int>("A");
    signature.vector_input<int>("B");
    return signature.build();
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
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
    static MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static MFSignature create_signature()
  {
    MFSignatureBuilder signature{"Append"};
    signature.vector_mutable<int>("Vector");
    signature.single_input<int>("Value");
    return signature.build();
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
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
    static MFSignature signature = create_signature();
    this->set_signature(&signature);
  }

  static MFSignature create_signature()
  {
    MFSignatureBuilder signature{"Sum Vectors"};
    signature.vector_input<int>("Vector");
    signature.single_output<int>("Sum");
    return signature.build();
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
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

}  // namespace blender::fn::tests
