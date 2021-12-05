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
