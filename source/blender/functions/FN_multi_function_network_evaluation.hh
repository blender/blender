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

#ifndef __FN_MULTI_FUNCTION_NETWORK_EVALUATION_HH__
#define __FN_MULTI_FUNCTION_NETWORK_EVALUATION_HH__

/** \file
 * \ingroup fn
 */

#include "FN_multi_function_network.hh"

namespace blender {
namespace fn {

class MFNetworkEvaluationStorage;

class MFNetworkEvaluator : public MultiFunction {
 private:
  Vector<const MFOutputSocket *> m_inputs;
  Vector<const MFInputSocket *> m_outputs;

 public:
  MFNetworkEvaluator(Vector<const MFOutputSocket *> inputs, Vector<const MFInputSocket *> outputs);

  void call(IndexMask mask, MFParams params, MFContext context) const override;

 private:
  using Storage = MFNetworkEvaluationStorage;

  void copy_inputs_to_storage(MFParams params, Storage &storage) const;
  void copy_outputs_to_storage(
      MFParams params,
      Storage &storage,
      Vector<const MFInputSocket *> &outputs_to_initialize_in_the_end) const;

  void evaluate_network_to_compute_outputs(MFContext &global_context, Storage &storage) const;

  void evaluate_function(MFContext &global_context,
                         const MFFunctionNode &function_node,
                         Storage &storage) const;

  bool can_do_single_value_evaluation(const MFFunctionNode &function_node, Storage &storage) const;

  void initialize_remaining_outputs(MFParams params,
                                    Storage &storage,
                                    Span<const MFInputSocket *> remaining_outputs) const;
};

}  // namespace fn
}  // namespace blender

#endif /* __FN_MULTI_FUNCTION_NETWORK_EVALUATION_HH__ */
