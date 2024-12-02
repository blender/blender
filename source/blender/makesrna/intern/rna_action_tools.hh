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

#endif /* RNA_RUNTIME */
