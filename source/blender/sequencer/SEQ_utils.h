/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2004 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "DNA_scene_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct Mask;
struct Scene;
struct Sequence;
struct StripElem;

void SEQ_sequence_base_unique_name_recursive(struct Scene *scene,
                                             struct ListBase *seqbasep,
                                             struct Sequence *seq);
const char *SEQ_sequence_give_name(struct Sequence *seq);
struct ListBase *SEQ_get_seqbase_from_sequence(struct Sequence *seq,
                                               struct ListBase **channels,
                                               int *r_offset);
const struct Sequence *SEQ_get_topmost_sequence(const struct Scene *scene, int frame);
/**
 * In cases where we don't know the sequence's listbase.
 */
struct ListBase *SEQ_get_seqbase_by_seq(const struct Scene *scene, struct Sequence *seq);
/**
 * Only use as last resort when the StripElem is available but no the Sequence.
 * (needed for RNA)
 */
struct Sequence *SEQ_sequence_from_strip_elem(struct ListBase *seqbase, struct StripElem *se);
struct Sequence *SEQ_get_sequence_by_name(struct ListBase *seqbase,
                                          const char *name,
                                          bool recursive);
struct Mask *SEQ_active_mask_get(struct Scene *scene);
void SEQ_alpha_mode_from_file_extension(struct Sequence *seq);
bool SEQ_sequence_has_source(const struct Sequence *seq);
void SEQ_set_scale_to_fit(const struct Sequence *seq,
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
void SEQ_ensure_unique_name(struct Sequence *seq, struct Scene *scene);
#ifdef __cplusplus
}
#endif
