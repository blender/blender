/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 *
 * Common utility functions for Action Slot access.
 *
 * The implementation of these functions can be found in rna_animation.cc.
 */

#pragma once

#ifdef RNA_RUNTIME

/**
 * Get the Action slot, given this slot handle.
 */
PointerRNA rna_generic_action_slot_get(bAction *dna_action,
                                       const blender::animrig::slot_handle_t slot_handle);

/**
 * Set the Action slot.
 *
 * Requires that it is a slot on the already-assigned Action.
 */
void rna_generic_action_slot_set(PointerRNA rna_slot_to_assign,
                                 ID &animated_id,
                                 bAction *&action_ptr_ref,
                                 blender::animrig::slot_handle_t &slot_handle_ref,
                                 char *slot_name,
                                 ReportList *reports);

/**
 * Set the Action slot by its handle.
 *
 * Requires that there is an Action already assigned, unless the slot to assign
 * is Slot::unassigned (that always works).
 */
void rna_generic_action_slot_handle_set(blender::animrig::slot_handle_t slot_handle_to_assign,
                                        ID &animated_id,
                                        bAction *&action_ptr_ref,
                                        blender::animrig::slot_handle_t &slot_handle_ref,
                                        char *slot_name);

/**
 * Generic iterator for Action slots that are suitable for use by the owner of the RNA property.
 *
 * Use these functions to complete the array property: "rna_iterator_array_next",
 * "rna_iterator_array_end", "rna_iterator_array_dereference_get".
 */
void rna_iterator_generic_action_suitable_slots_begin(CollectionPropertyIterator *iter,
                                                      bAction *assigned_action);

/**
 * Generic function for handling library overrides on Action slot handle properties.
 *
 * This is used for `id.animation_data.action_slot_handle`, and similar properties. These
 * properties determine which Action Slot is assigned. The reason this needs special code is that
 * the assigned slot is determined by two properties: the assigned Action, and the slot handle. So
 * even when the slot handle itself is numerically identical in the library file and the override,
 * if the Action assignment is overridden, that number indicates a different, unrelated slot.
 *
 * In the above case, when the library overrides get applied, first the new Action is assigned.
 * This will make Blender auto-select a slot, which may fail, resulting in having no slot assigned.
 * To ensure that the intended slot is assigned after this, this function will emit a library
 * override operation for the slot handle as well. That way, after the Action is assigned, an
 * explicit slot will be assigned.
 */
void rna_generic_action_slot_handle_override_diff(Main *bmain,
                                                  RNAPropertyOverrideDiffContext &rnadiff_ctx,
                                                  const bAction *action_a,
                                                  const bAction *action_b);

#endif /* RNA_RUNTIME */
