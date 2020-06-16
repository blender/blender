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

#ifndef __FN_MULTI_FUNCTION_HH__
#define __FN_MULTI_FUNCTION_HH__

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

#include "FN_multi_function_context.hh"
#include "FN_multi_function_params.hh"

namespace blender {
namespace fn {

class MultiFunction {
 private:
  MFSignature m_signature;

 public:
  virtual ~MultiFunction()
  {
  }

  virtual void call(IndexMask mask, MFParams params, MFContext context) const = 0;

  IndexRange param_indices() const
  {
    return m_signature.param_types.index_range();
  }

  MFParamType param_type(uint param_index) const
  {
    return m_signature.param_types[param_index];
  }

  StringRefNull param_name(uint param_index) const
  {
    return m_signature.param_names[param_index];
  }

  StringRefNull name() const
  {
    return m_signature.function_name;
  }

  const MFSignature &signature() const
  {
    return m_signature;
  }

 protected:
  MFSignatureBuilder get_builder(StringRef function_name)
  {
    m_signature.function_name = function_name;
    return MFSignatureBuilder(m_signature);
  }
};

inline MFParamsBuilder::MFParamsBuilder(const class MultiFunction &fn, uint min_array_size)
    : MFParamsBuilder(fn.signature(), min_array_size)
{
}

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_HH__ */
