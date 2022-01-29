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

#include "FN_multi_function_procedure_optimization.hh"

namespace blender::fn::procedure_optimization {

void move_destructs_up(MFProcedure &procedure, MFInstruction &block_end_instr)
{
  /* A mapping from a variable to its destruct instruction. */
  Map<MFVariable *, MFDestructInstruction *> destruct_instructions;
  MFInstruction *current_instr = &block_end_instr;
  while (true) {
    MFInstructionType instr_type = current_instr->type();
    switch (instr_type) {
      case MFInstructionType::Destruct: {
        MFDestructInstruction &destruct_instr = static_cast<MFDestructInstruction &>(
            *current_instr);
        MFVariable *variable = destruct_instr.variable();
        if (variable == nullptr) {
          continue;
        }
        /* Remember this destruct instruction so that it can be moved up later on when the last use
         * of the variable is found. */
        destruct_instructions.add(variable, &destruct_instr);
        break;
      }
      case MFInstructionType::Call: {
        MFCallInstruction &call_instr = static_cast<MFCallInstruction &>(*current_instr);
        /* For each variable, place the corresponding remembered destruct instruction right after
         * this call instruction. */
        for (MFVariable *variable : call_instr.params()) {
          if (variable == nullptr) {
            continue;
          }
          MFDestructInstruction *destruct_instr = destruct_instructions.pop_default(variable,
                                                                                    nullptr);
          if (destruct_instr == nullptr) {
            continue;
          }

          /* Unlink destruct instruction from previous position. */
          MFInstruction *after_destruct_instr = destruct_instr->next();
          while (!destruct_instr->prev().is_empty()) {
            /* Do a copy of the cursor here, because `destruct_instr->prev()` changes when
             * #set_next is called below. */
            const MFInstructionCursor cursor = destruct_instr->prev()[0];
            cursor.set_next(procedure, after_destruct_instr);
          }

          /* Insert destruct instruction in new position. */
          MFInstruction *next_instr = call_instr.next();
          call_instr.set_next(destruct_instr);
          destruct_instr->set_next(next_instr);
        }
        break;
      }
      default: {
        break;
      }
    }

    const Span<MFInstructionCursor> prev_cursors = current_instr->prev();
    if (prev_cursors.size() != 1) {
      /* Stop when there is some branching before this instruction. */
      break;
    }
    const MFInstructionCursor &prev_cursor = prev_cursors[0];
    current_instr = prev_cursor.instruction();
    if (current_instr == nullptr) {
      /* Stop when there is no previous instruction. E.g. when this is the first instruction. */
      break;
    }
  }
}

}  // namespace blender::fn::procedure_optimization
