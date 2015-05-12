/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* == OpCodes for OperationDepsNodes ==
 * This file defines all the "operation codes" (opcodes) used to identify
 * common operation node types. The intention of these defines is to have
 * a fast and reliable way of identifying the relevant nodes within a component
 * without having to use fragile dynamic strings.
 *
 * This file is meant to be used like UI_icons.h. That is, before including
 * the file, the host file must define the DEG_OPCODE(_label) macro, which
 * is responsible for converting the define into whatever form is suitable.
 * Therefore, it intentionally doesn't have header guards.
 */


/* Example macro define: */
/* #define DEF_DEG_OPCODE(label) DEG_OPCODE_##label, */

/* Generic Operations ------------------------------ */

/* Placeholder for operations which don't need special mention */
DEF_DEG_OPCODE(OPERATION)

// XXX: Placeholder while porting depsgraph code
DEF_DEG_OPCODE(PLACEHOLDER)

DEF_DEG_OPCODE(NOOP)

/* Animation, Drivers, etc. ------------------------ */

/* NLA + Action */
DEF_DEG_OPCODE(ANIMATION)

/* Driver */
DEF_DEG_OPCODE(DRIVER)

/* Proxy Inherit? */
//DEF_DEG_OPCODE(PROXY)

/* Transform --------------------------------------- */

/* Transform entry point - local transforms only */
DEF_DEG_OPCODE(TRANSFORM_LOCAL)

/* Parenting */
DEF_DEG_OPCODE(TRANSFORM_PARENT)

/* Constraints */
DEF_DEG_OPCODE(TRANSFORM_CONSTRAINTS)
//DEF_DEG_OPCODE(TRANSFORM_CONSTRAINTS_INIT)
//DEF_DEG_OPCODE(TRANSFORM_CONSTRAINT)
//DEF_DEG_OPCODE(TRANSFORM_CONSTRAINTS_DONE)

/* Rigidbody Sim - Perform Sim */
DEF_DEG_OPCODE(RIGIDBODY_REBUILD)
DEF_DEG_OPCODE(RIGIDBODY_SIM)

/* Rigidbody Sim - Copy Results to Object */
DEF_DEG_OPCODE(TRANSFORM_RIGIDBODY)

/* Transform exitpoint */
DEF_DEG_OPCODE(TRANSFORM_FINAL)

/* XXX: ubereval is for temporary porting purposes only */
DEF_DEG_OPCODE(OBJECT_UBEREVAL)

/* Geometry ---------------------------------------- */

/* XXX: Placeholder - UberEval */
DEF_DEG_OPCODE(GEOMETRY_UBEREVAL)

/* Modifier */
DEF_DEG_OPCODE(GEOMETRY_MODIFIER)

/* Curve Objects - Path Calculation (used for path-following tools) */
DEF_DEG_OPCODE(GEOMETRY_PATH)

/* Pose -------------------------------------------- */

/* Init IK Trees, etc. */
DEF_DEG_OPCODE(POSE_INIT)

/* Free IK Trees + Compute Deform Matrices */
DEF_DEG_OPCODE(POSE_DONE)

/* IK/Spline Solvers */
DEF_DEG_OPCODE(POSE_IK_SOLVER)
DEF_DEG_OPCODE(POSE_SPLINE_IK_SOLVER)

/* Bone -------------------------------------------- */

/* Bone local transforms - Entrypoint */
DEF_DEG_OPCODE(BONE_LOCAL)

/* Pose-space conversion (includes parent + restpose) */
DEF_DEG_OPCODE(BONE_POSE_PARENT)

/* Constraints */
DEF_DEG_OPCODE(BONE_CONSTRAINTS)
//DEF_DEG_OPCODE(BONE_CONSTRAINTS_INIT)
//DEF_DEG_OPCODE(BONE_CONSTRAINT)
//DEF_DEG_OPCODE(BONE_CONSTRAINTS_DONE)

/* Bone transforms are ready
 * - "READY"             This (internal) noop is used to signal that all pre-IK operations are done.
 *                       Its role is to help mediate situations where cyclic relations may otherwise form
 *                       (i.e. one bone in chain targetting another in same chain)
 * - "DONE"              This noop is used to signal that the bone's final pose transform can be read by others
 */
// TODO: deform mats could get calculated in the final_transform ops...
DEF_DEG_OPCODE(BONE_READY)
DEF_DEG_OPCODE(BONE_DONE)

/* Particles --------------------------------------- */

/* XXX: placeholder - Particle System eval */
DEF_DEG_OPCODE(PSYS_EVAL)
