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

#pragma once

/** \file
 * \ingroup fn
 *
 * This file contains several utilities to create multi-functions with less redundant code.
 */

#include <functional>

#include "FN_multi_function.hh"

namespace blender::fn {

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single output (SO) of type Out1
 *
 * This example creates a function that adds 10 to the incoming values:
 *  CustomMF_SI_SO<int, int> fn("add 10", [](int value) { return value + 10; });
 */
template<typename In1, typename Out1> class CustomMF_SI_SO : public MultiFunction {
 private:
  using FunctionT = std::function<void(IndexMask, VSpan<In1>, MutableSpan<Out1>)>;
  FunctionT function_;

 public:
  CustomMF_SI_SO(StringRef name, FunctionT function) : function_(std::move(function))
  {
    MFSignatureBuilder signature = this->get_builder(name);
    signature.single_input<In1>("In1");
    signature.single_output<Out1>("Out1");
  }

  template<typename ElementFuncT>
  CustomMF_SI_SO(StringRef name, ElementFuncT element_fn)
      : CustomMF_SI_SO(name, CustomMF_SI_SO::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask, VSpan<In1> in1, MutableSpan<Out1> out1) {
      mask.foreach_index(
          [&](int i) { new (static_cast<void *>(&out1[i])) Out1(element_fn(in1[i])); });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<In1> in1 = params.readonly_single_input<In1>(0);
    MutableSpan<Out1> out1 = params.uninitialized_single_output<Out1>(1);
    function_(mask, in1, out1);
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename Out1>
class CustomMF_SI_SI_SO : public MultiFunction {
 private:
  using FunctionT = std::function<void(IndexMask, VSpan<In1>, VSpan<In2>, MutableSpan<Out1>)>;
  FunctionT function_;

 public:
  CustomMF_SI_SI_SO(StringRef name, FunctionT function) : function_(std::move(function))
  {
    MFSignatureBuilder signature = this->get_builder(name);
    signature.single_input<In1>("In1");
    signature.single_input<In2>("In2");
    signature.single_output<Out1>("Out1");
  }

  template<typename ElementFuncT>
  CustomMF_SI_SI_SO(StringRef name, ElementFuncT element_fn)
      : CustomMF_SI_SI_SO(name, CustomMF_SI_SI_SO::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask, VSpan<In1> in1, VSpan<In2> in2, MutableSpan<Out1> out1) {
      mask.foreach_index(
          [&](int i) { new (static_cast<void *>(&out1[i])) Out1(element_fn(in1[i], in2[i])); });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<In1> in1 = params.readonly_single_input<In1>(0);
    VSpan<In2> in2 = params.readonly_single_input<In2>(1);
    MutableSpan<Out1> out1 = params.uninitialized_single_output<Out1>(2);
    function_(mask, in1, in2, out1);
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single input (SI) of type In3
 * 4. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename In3, typename Out1>
class CustomMF_SI_SI_SI_SO : public MultiFunction {
 private:
  using FunctionT =
      std::function<void(IndexMask, VSpan<In1>, VSpan<In2>, VSpan<In3>, MutableSpan<Out1>)>;
  FunctionT function_;

 public:
  CustomMF_SI_SI_SI_SO(StringRef name, FunctionT function) : function_(std::move(function))
  {
    MFSignatureBuilder signature = this->get_builder(name);
    signature.single_input<In1>("In1");
    signature.single_input<In2>("In2");
    signature.single_input<In3>("In3");
    signature.single_output<Out1>("Out1");
  }

  template<typename ElementFuncT>
  CustomMF_SI_SI_SI_SO(StringRef name, ElementFuncT element_fn)
      : CustomMF_SI_SI_SI_SO(name, CustomMF_SI_SI_SI_SO::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask,
               VSpan<In1> in1,
               VSpan<In2> in2,
               VSpan<In3> in3,
               MutableSpan<Out1> out1) {
      mask.foreach_index([&](int i) {
        new (static_cast<void *>(&out1[i])) Out1(element_fn(in1[i], in2[i], in3[i]));
      });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<In1> in1 = params.readonly_single_input<In1>(0);
    VSpan<In2> in2 = params.readonly_single_input<In2>(1);
    VSpan<In3> in3 = params.readonly_single_input<In3>(2);
    MutableSpan<Out1> out1 = params.uninitialized_single_output<Out1>(3);
    function_(mask, in1, in2, in3, out1);
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single input (SI) of type In1
 * 2. single input (SI) of type In2
 * 3. single input (SI) of type In3
 * 4. single input (SI) of type In4
 * 5. single output (SO) of type Out1
 */
template<typename In1, typename In2, typename In3, typename In4, typename Out1>
class CustomMF_SI_SI_SI_SI_SO : public MultiFunction {
 private:
  using FunctionT = std::function<void(
      IndexMask, VSpan<In1>, VSpan<In2>, VSpan<In3>, VSpan<In4>, MutableSpan<Out1>)>;
  FunctionT function_;

 public:
  CustomMF_SI_SI_SI_SI_SO(StringRef name, FunctionT function) : function_(std::move(function))
  {
    MFSignatureBuilder signature = this->get_builder(name);
    signature.single_input<In1>("In1");
    signature.single_input<In2>("In2");
    signature.single_input<In3>("In3");
    signature.single_input<In4>("In4");
    signature.single_output<Out1>("Out1");
  }

  template<typename ElementFuncT>
  CustomMF_SI_SI_SI_SI_SO(StringRef name, ElementFuncT element_fn)
      : CustomMF_SI_SI_SI_SI_SO(name, CustomMF_SI_SI_SI_SI_SO::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask,
               VSpan<In1> in1,
               VSpan<In2> in2,
               VSpan<In3> in3,
               VSpan<In4> in4,
               MutableSpan<Out1> out1) {
      mask.foreach_index([&](int i) {
        new (static_cast<void *>(&out1[i])) Out1(element_fn(in1[i], in2[i], in3[i], in4[i]));
      });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<In1> in1 = params.readonly_single_input<In1>(0);
    VSpan<In2> in2 = params.readonly_single_input<In2>(1);
    VSpan<In3> in3 = params.readonly_single_input<In3>(2);
    VSpan<In4> in4 = params.readonly_single_input<In4>(3);
    MutableSpan<Out1> out1 = params.uninitialized_single_output<Out1>(4);
    function_(mask, in1, in2, in3, in4, out1);
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single mutable (SM) of type Mut1
 */
template<typename Mut1> class CustomMF_SM : public MultiFunction {
 private:
  using FunctionT = std::function<void(IndexMask, MutableSpan<Mut1>)>;
  FunctionT function_;

 public:
  CustomMF_SM(StringRef name, FunctionT function) : function_(std::move(function))
  {
    MFSignatureBuilder signature = this->get_builder(name);
    signature.single_mutable<Mut1>("Mut1");
  }

  template<typename ElementFuncT>
  CustomMF_SM(StringRef name, ElementFuncT element_fn)
      : CustomMF_SM(name, CustomMF_SM::create_function(element_fn))
  {
  }

  template<typename ElementFuncT> static FunctionT create_function(ElementFuncT element_fn)
  {
    return [=](IndexMask mask, MutableSpan<Mut1> mut1) {
      mask.foreach_index([&](int i) { element_fn(mut1[i]); });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<Mut1> mut1 = params.single_mutable<Mut1>(0);
    function_(mask, mut1);
  }
};

/**
 * Generates a multi-function that converts between two types.
 */
template<typename From, typename To> class CustomMF_Convert : public MultiFunction {
 public:
  CustomMF_Convert()
  {
    std::string name = CPPType::get<From>().name() + " to " + CPPType::get<To>().name();
    MFSignatureBuilder signature = this->get_builder(std::move(name));
    signature.single_input<From>("Input");
    signature.single_output<To>("Output");
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<From> inputs = params.readonly_single_input<From>(0);
    MutableSpan<To> outputs = params.uninitialized_single_output<To>(1);

    for (int64_t i : mask) {
      new (static_cast<void *>(&outputs[i])) To(inputs[i]);
    }
  }
};

/**
 * A multi-function that outputs the same value every time. The value is not owned by an instance
 * of this function. The caller is responsible for destructing and freeing the value.
 */
class CustomMF_GenericConstant : public MultiFunction {
 private:
  const CPPType &type_;
  const void *value_;

  template<typename T> friend class CustomMF_Constant;

 public:
  CustomMF_GenericConstant(const CPPType &type, const void *value);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
  uint64_t hash() const override;
  bool equals(const MultiFunction &other) const override;
};

/**
 * A multi-function that outputs the same array every time. The array is not owned by in instance
 * of this function. The caller is responsible for destructing and freeing the values.
 */
class CustomMF_GenericConstantArray : public MultiFunction {
 private:
  GSpan array_;

 public:
  CustomMF_GenericConstantArray(GSpan array);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

/**
 * Generates a multi-function that outputs a constant value.
 */
template<typename T> class CustomMF_Constant : public MultiFunction {
 private:
  T value_;

 public:
  template<typename U> CustomMF_Constant(U &&value) : value_(std::forward<U>(value))
  {
    MFSignatureBuilder signature = this->get_builder("Constant");
    std::stringstream ss;
    ss << value_;
    signature.single_output<T>(ss.str());
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<T> output = params.uninitialized_single_output<T>(0);
    mask.foreach_index([&](int i) { new (&output[i]) T(value_); });
  }

  uint64_t hash() const override
  {
    return DefaultHash<T>{}(value_);
  }

  bool equals(const MultiFunction &other) const override
  {
    const CustomMF_Constant *other1 = dynamic_cast<const CustomMF_Constant *>(&other);
    if (other1 != nullptr) {
      return value_ == other1->value_;
    }
    const CustomMF_GenericConstant *other2 = dynamic_cast<const CustomMF_GenericConstant *>(
        &other);
    if (other2 != nullptr) {
      const CPPType &type = CPPType::get<T>();
      if (type == other2->type_) {
        return type.is_equal(static_cast<const void *>(&value_), other2->value_);
      }
    }
    return false;
  }
};

class CustomMF_DefaultOutput : public MultiFunction {
 private:
  int output_amount_;

 public:
  CustomMF_DefaultOutput(StringRef name,
                         Span<MFDataType> input_types,
                         Span<MFDataType> output_types);
  void call(IndexMask mask, MFParams params, MFContext context) const override;
};

}  // namespace blender::fn
