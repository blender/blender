/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_procedure_optimization.hh"

namespace blender::fn::multi_function::procedure_optimization {

void move_destructs_up(Procedure &procedure, Instruction &block_end_instr)
{
  /* A mapping from a variable to its destruct instruction. */
  Map<Variable *, DestructInstruction *> destruct_instructions;
  Instruction *current_instr = &block_end_instr;
  while (true) {
    InstructionType instr_type = current_instr->type();
    switch (instr_type) {
      case InstructionType::Destruct: {
        DestructInstruction &destruct_instr = static_cast<DestructInstruction &>(*current_instr);
        Variable *variable = destruct_instr.variable();
        if (variable == nullptr) {
          continue;
        }
        /* Remember this destruct instruction so that it can be moved up later on when the last use
         * of the variable is found. */
        destruct_instructions.add(variable, &destruct_instr);
        break;
      }
      case InstructionType::Call: {
        CallInstruction &call_instr = static_cast<CallInstruction &>(*current_instr);
        /* For each variable, place the corresponding remembered destruct instruction right after
         * this call instruction. */
        for (Variable *variable : call_instr.params()) {
          if (variable == nullptr) {
            continue;
          }
          DestructInstruction *destruct_instr = destruct_instructions.pop_default(variable,
                                                                                  nullptr);
          if (destruct_instr == nullptr) {
            continue;
          }

          /* Unlink destruct instruction from previous position. */
          Instruction *after_destruct_instr = destruct_instr->next();
          while (!destruct_instr->prev().is_empty()) {
            /* Do a copy of the cursor here, because `destruct_instr->prev()` changes when
             * #set_next is called below. */
            const InstructionCursor cursor = destruct_instr->prev()[0];
            cursor.set_next(procedure, after_destruct_instr);
          }

          /* Insert destruct instruction in new position. */
          Instruction *next_instr = call_instr.next();
          call_instr.set_next(destruct_instr);
          destruct_instr->set_next(next_instr);
        }
        break;
      }
      default: {
        break;
      }
    }

    const Span<InstructionCursor> prev_cursors = current_instr->prev();
    if (prev_cursors.size() != 1) {
      /* Stop when there is some branching before this instruction. */
      break;
    }
    const InstructionCursor &prev_cursor = prev_cursors[0];
    current_instr = prev_cursor.instruction();
    if (current_instr == nullptr) {
      /* Stop when there is no previous instruction. E.g. when this is the first instruction. */
      break;
    }
  }
}

}  // namespace blender::fn::multi_function::procedure_optimization
