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
  MFSignature signature_;

 public:
  virtual ~MultiFunction()
  {
  }

  virtual void call(IndexMask mask, MFParams params, MFContext context) const = 0;

  virtual uint64_t hash() const
  {
    return DefaultHash<const MultiFunction *>{}(this);
  }

  virtual bool equals(const MultiFunction &UNUSED(other)) const
  {
    return false;
  }

  int param_amount() const
  {
    return signature_.param_types.size();
  }

  IndexRange param_indices() const
  {
    return signature_.param_types.index_range();
  }

  MFParamType param_type(int param_index) const
  {
    return signature_.param_types[param_index];
  }

  StringRefNull param_name(int param_index) const
  {
    return signature_.param_names[param_index];
  }

  StringRefNull name() const
  {
    return signature_.function_name;
  }

  bool depends_on_context() const
  {
    return signature_.depends_on_context;
  }

  const MFSignature &signature() const
  {
    return signature_;
  }

 protected:
  MFSignatureBuilder get_builder(std::string function_name)
  {
    signature_.function_name = std::move(function_name);
    return MFSignatureBuilder(signature_);
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
