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
 * A `MultiFunction` encapsulates a function that is optimized for throughput (instead of latency).
 * The throughput is optimized by always processing many elements at once, instead of each element
 * separately. This is ideal for functions that are evaluated often (e.g. for every particle).
 *
 * By processing a lot of data at once, individual functions become easier to optimize for humans
 * and for the compiler. Furthermore, performance profiles become easier to understand and show
 * better where bottlenecks are.
 *
 * Every multi-function has a name and an ordered list of parameters. Parameters are used for input
 * and output. In fact, there are three kinds of parameters: inputs, outputs and mutable (which is
 * combination of input and output).
 *
 * To call a multi-function, one has to provide three things:
 * - `MFParams`: This references the input and output arrays that the function works with. The
 *      arrays are not owned by MFParams.
 * - `IndexMask`: An array of indices indicating which indices in the provided arrays should be
 *      touched/processed.
 * - `MFContext`: Further information for the called function.
 *
 * A new multi-function is generally implemented as follows:
 * 1. Create a new subclass of MultiFunction.
 * 2. Implement a constructor that initialized the signature of the function.
 * 3. Override the `call` function.
 */

#include "BLI_hash.hh"

#include "FN_multi_function_context.hh"
#include "FN_multi_function_params.hh"

namespace blender::fn {

class MultiFunction {
 private:
  const MFSignature *signature_ref_ = nullptr;

 public:
  virtual ~MultiFunction()
  {
  }

  virtual void call(IndexMask mask, MFParams params, MFContext context) const = 0;

  virtual uint64_t hash() const
  {
    return get_default_hash(this);
  }

  virtual bool equals(const MultiFunction &UNUSED(other)) const
  {
    return false;
  }

  int param_amount() const
  {
    return signature_ref_->param_types.size();
  }

  IndexRange param_indices() const
  {
    return signature_ref_->param_types.index_range();
  }

  MFParamType param_type(int param_index) const
  {
    return signature_ref_->param_types[param_index];
  }

  StringRefNull param_name(int param_index) const
  {
    return signature_ref_->param_names[param_index];
  }

  StringRefNull name() const
  {
    return signature_ref_->function_name;
  }

  bool depends_on_context() const
  {
    return signature_ref_->depends_on_context;
  }

  const MFSignature &signature() const
  {
    BLI_assert(signature_ref_ != nullptr);
    return *signature_ref_;
  }

 protected:
  /* Make the function use the given signature. This should be called once in the constructor of
   * child classes. No copy of the signature is made, so the caller has to make sure that the
   * signature lives as long as the multi function. It is ok to embed the signature into the child
   * class. */
  void set_signature(const MFSignature *signature)
  {
    /* Take a pointer as argument, so that it is more obvious that no copy is created. */
    BLI_assert(signature != nullptr);
    signature_ref_ = signature;
  }
};

inline MFParamsBuilder::MFParamsBuilder(const class MultiFunction &fn, int64_t min_array_size)
    : MFParamsBuilder(fn.signature(), min_array_size)
{
}

extern const MultiFunction &dummy_multi_function;

namespace multi_function_types {
using fn::CPPType;
using fn::GMutableSpan;
using fn::GSpan;
using fn::MFContext;
using fn::MFContextBuilder;
using fn::MFDataType;
using fn::MFParams;
using fn::MFParamsBuilder;
using fn::MFParamType;
using fn::MultiFunction;
}  // namespace multi_function_types

}  // namespace blender::fn
