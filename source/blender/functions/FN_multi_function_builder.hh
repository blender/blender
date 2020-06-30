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

#ifndef __FN_MULTI_FUNCTION_BUILDER_HH__
#define __FN_MULTI_FUNCTION_BUILDER_HH__

/** \file
 * \ingroup fn
 *
 * This file contains several utilities to create multi-functions with less redundant code.
 */

#include <functional>

#include "FN_multi_function.hh"

namespace blender {
namespace fn {

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
  FunctionT m_function;

 public:
  CustomMF_SI_SO(StringRef name, FunctionT function) : m_function(std::move(function))
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
      mask.foreach_index([&](uint i) { new ((void *)&out1[i]) Out1(element_fn(in1[i])); });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<In1> in1 = params.readonly_single_input<In1>(0);
    MutableSpan<Out1> out1 = params.uninitialized_single_output<Out1>(1);
    m_function(mask, in1, out1);
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
  FunctionT m_function;

 public:
  CustomMF_SI_SI_SO(StringRef name, FunctionT function) : m_function(std::move(function))
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
      mask.foreach_index([&](uint i) { new ((void *)&out1[i]) Out1(element_fn(in1[i], in2[i])); });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<In1> in1 = params.readonly_single_input<In1>(0);
    VSpan<In2> in2 = params.readonly_single_input<In2>(1);
    MutableSpan<Out1> out1 = params.uninitialized_single_output<Out1>(2);
    m_function(mask, in1, in2, out1);
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
  FunctionT m_function;

 public:
  CustomMF_SI_SI_SI_SO(StringRef name, FunctionT function) : m_function(std::move(function))
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
      mask.foreach_index(
          [&](uint i) { new ((void *)&out1[i]) Out1(element_fn(in1[i], in2[i], in3[i])); });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    VSpan<In1> in1 = params.readonly_single_input<In1>(0);
    VSpan<In2> in2 = params.readonly_single_input<In2>(1);
    VSpan<In3> in3 = params.readonly_single_input<In3>(2);
    MutableSpan<Out1> out1 = params.uninitialized_single_output<Out1>(3);
    m_function(mask, in1, in2, in3, out1);
  }
};

/**
 * Generates a multi-function with the following parameters:
 * 1. single mutable (SM) of type Mut1
 */
template<typename Mut1> class CustomMF_SM : public MultiFunction {
 private:
  using FunctionT = std::function<void(IndexMask, MutableSpan<Mut1>)>;
  FunctionT m_function;

 public:
  CustomMF_SM(StringRef name, FunctionT function) : m_function(std::move(function))
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
      mask.foreach_index([&](uint i) { element_fn(mut1[i]); });
    };
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<Mut1> mut1 = params.single_mutable<Mut1>(0);
    m_function(mask, mut1);
  }
};

/**
 * Generates a multi-function that outputs a constant value.
 */
template<typename T> class CustomMF_Constant : public MultiFunction {
 private:
  T m_value;

 public:
  template<typename U> CustomMF_Constant(U &&value) : m_value(std::forward<U>(value))
  {
    MFSignatureBuilder signature = this->get_builder("Constant");
    std::stringstream ss;
    ss << m_value;
    signature.single_output<T>(ss.str());
  }

  void call(IndexMask mask, MFParams params, MFContext UNUSED(context)) const override
  {
    MutableSpan<T> output = params.uninitialized_single_output<T>(0);
    mask.foreach_index([&](uint i) { new (&output[i]) T(m_value); });
  }
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_BUILDER_HH__ */
