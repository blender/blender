/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * A #Procedure optimization pass takes an existing procedure and changes it in a way that
 * improves its performance when executed.
 *
 * Oftentimes it would also be possible to implement a specific optimization directly during
 * construction of the initial #Procedure. There is a trade-off between doing that or just
 * building a "simple" procedure and then optimizing it uses separate optimization passes.
 * - Doing optimizations directly during construction is typically faster than doing it as a
 *   separate pass. However, it would be much harder to turn the optimization off when it is not
 *   necessary, making the construction potentially slower in those cases.
 * - Doing optimizations directly would also make code more complex, because it mixes the logic
 *   that generates the procedure from some other data with optimization decisions.
 * - Having a separate pass allows us to use it in different places when necessary.
 * - Having a separate pass allows us to enable and disable it easily to better understand its
 *   impact on performance.
 */

#include "FN_multi_function_procedure.hh"

namespace blender::fn::multi_function::procedure_optimization {

/**
 * When generating a procedure, destruct instructions (#DestructInstruction) have to be inserted
 * for all variables that are not outputs. Often the simplest approach is to add these instructions
 * at the very end. However, when the procedure is executed this is not optimal, because many more
 * variables are initialized at the same time than necessary. This inhibits the reuse of memory
 * buffers which decreases performance and increases memory use.
 *
 * This optimization pass moves destruct instructions up in the procedure. The goal is to destruct
 * each variable right after its last use.
 *
 * For simplicity, and because this is the most common use case, this optimization currently only
 * works on a single chain of instructions. Destruct instructions are not moved across branches.
 *
 * \param procedure: The procedure that should be optimized.
 * \param block_end_instr: The instruction that points to the last instruction within a linear
 * chain of instructions. The algorithm moves instructions backward starting at this instruction.
 */
void move_destructs_up(Procedure &procedure, Instruction &block_end_instr);

}  // namespace blender::fn::multi_function::procedure_optimization
