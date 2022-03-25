/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_multi_function_procedure.hh"

namespace blender::fn {

/** A multi-function that executes a procedure internally. */
class MFProcedureExecutor : public MultiFunction {
 private:
  MFSignature signature_;
  const MFProcedure &procedure_;

 public:
  MFProcedureExecutor(const MFProcedure &procedure);

  void call(IndexMask mask, MFParams params, MFContext context) const override;

 private:
  ExecutionHints get_execution_hints() const override;
};

}  // namespace blender::fn
