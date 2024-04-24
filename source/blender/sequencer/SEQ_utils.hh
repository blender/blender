/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_scene_types.h"

struct ListBase;
struct Mask;
struct Scene;
struct Sequence;
struct StripElem;

void SEQ_sequence_base_unique_name_recursive(Scene *scene, ListBase *seqbasep, Sequence *seq);
const char *SEQ_sequence_give_name(const Sequence *seq);
ListBase *SEQ_get_seqbase_from_sequence(Sequence *seq, ListBase **channels, int *r_offset);
const Sequence *SEQ_get_topmost_sequence(const Scene *scene, int frame);
/**
 * In cases where we don't know the sequence's listbase.
 */
ListBase *SEQ_get_seqbase_by_seq(const Scene *scene, Sequence *seq);
/**
 * Only use as last resort when the StripElem is available but no the Sequence.
 * (needed for RNA)
 */
Sequence *SEQ_sequence_from_strip_elem(ListBase *seqbase, StripElem *se);
Sequence *SEQ_get_sequence_by_name(ListBase *seqbase, const char *name, bool recursive);
Mask *SEQ_active_mask_get(Scene *scene);
void SEQ_alpha_mode_from_file_extension(Sequence *seq);
bool SEQ_sequence_has_source(const Sequence *seq);
void SEQ_set_scale_to_fit(const Sequence *seq,
                          int image_width,
                          int image_height,
                          int preview_width,
                          int preview_height,
                          eSeqImageFitMethod fit_method);
/**
 * Ensure, that provided Sequence has unique name. If animation data exists for this Sequence, it
 * will be duplicated and mapped onto new name
 *
 * \param seq: Sequence which name will be ensured to be unique
 * \param scene: Scene in which name must be unique
 */
void SEQ_ensure_unique_name(Sequence *seq, Scene *scene);
