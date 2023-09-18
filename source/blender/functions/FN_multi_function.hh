/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 * - `Params`: This references the input and output arrays that the function works with. The
 *      arrays are not owned by Params.
 * - `IndexMask`: An array of indices indicating which indices in the provided arrays should be
 *      touched/processed.
 * - `Context`: Further information for the called function.
 *
 * A new multi-function is generally implemented as follows:
 * 1. Create a new subclass of MultiFunction.
 * 2. Implement a constructor that initialized the signature of the function.
 * 3. Override the `call` function.
 */

#include "BLI_hash.hh"

#include "FN_multi_function_context.hh"
#include "FN_multi_function_params.hh"

namespace blender::fn::multi_function {

class MultiFunction : NonCopyable, NonMovable {
 private:
  const Signature *signature_ref_ = nullptr;

 public:
  virtual ~MultiFunction() {}

  /**
   * The result is the same as using #call directly but this method has some additional features.
   * - Automatic multi-threading when possible and appropriate.
   * - Automatic index mask offsetting to avoid large temporary intermediate arrays that are mostly
   *   unused.
   */
  void call_auto(const IndexMask &mask, Params params, Context context) const;
  virtual void call(const IndexMask &mask, Params params, Context context) const = 0;

  virtual uint64_t hash() const
  {
    return get_default_hash(this);
  }

  virtual bool equals(const MultiFunction & /*other*/) const
  {
    return false;
  }

  int param_amount() const
  {
    return signature_ref_->params.size();
  }

  IndexRange param_indices() const
  {
    return signature_ref_->params.index_range();
  }

  ParamType param_type(int param_index) const
  {
    return signature_ref_->params[param_index].type;
  }

  StringRefNull param_name(int param_index) const
  {
    return signature_ref_->params[param_index].name;
  }

  StringRefNull name() const
  {
    return signature_ref_->function_name;
  }

  virtual std::string debug_name() const;

  const Signature &signature() const
  {
    BLI_assert(signature_ref_ != nullptr);
    return *signature_ref_;
  }

  /**
   * Information about how the multi-function behaves that help a caller to execute it efficiently.
   */
  struct ExecutionHints {
    /**
     * Suggested minimum workload under which multi-threading does not really help.
     * This should be lowered when the multi-function is doing something computationally expensive.
     */
    int64_t min_grain_size = 10000;
    /**
     * Indicates that the multi-function will allocate an array large enough to hold all indices
     * passed in as mask. This tells the caller that it would be preferable to pass in smaller
     * indices. Also maybe the full mask should be split up into smaller segments to decrease peak
     * memory usage.
     */
    bool allocates_array = false;
    /**
     * Tells the caller that every execution takes about the same time. This helps making a more
     * educated guess about a good grain size.
     */
    bool uniform_execution_time = true;
  };

  ExecutionHints execution_hints() const;

 protected:
  /* Make the function use the given signature. This should be called once in the constructor of
   * child classes. No copy of the signature is made, so the caller has to make sure that the
   * signature lives as long as the multi function. It is ok to embed the signature into the child
   * class. */
  void set_signature(const Signature *signature)
  {
    /* Take a pointer as argument, so that it is more obvious that no copy is created. */
    BLI_assert(signature != nullptr);
    signature_ref_ = signature;
  }

  virtual ExecutionHints get_execution_hints() const;
};

inline ParamsBuilder::ParamsBuilder(const MultiFunction &fn, const IndexMask *mask)
    : ParamsBuilder(fn.signature(), *mask)
{
}

}  // namespace blender::fn::multi_function

namespace blender {
namespace mf = fn::multi_function;
}
