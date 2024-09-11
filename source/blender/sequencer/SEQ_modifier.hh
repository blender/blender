/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

struct BlendDataReader;
struct BlendWriter;
struct ImBuf;
struct ListBase;
struct SeqRenderData;
struct Sequence;
struct SequenceModifierData;
struct StripScreenQuad;

struct SequenceModifierTypeInfo {
  /* default name for the modifier */
  char name[64]; /* MAX_NAME */

  /* DNA structure name used on load/save filed */
  char struct_name[64]; /* MAX_NAME */

  /* size of modifier data structure, used by allocation */
  int struct_size;

  /* data initialization */
  void (*init_data)(SequenceModifierData *smd);

  /* free data used by modifier,
   * only modifier-specific data should be freed, modifier descriptor would
   * be freed outside of this callback
   */
  void (*free_data)(SequenceModifierData *smd);

  /* copy data from one modifier to another */
  void (*copy_data)(SequenceModifierData *smd, SequenceModifierData *target);

  /* Apply modifier on an image buffer.
   * quad contains four corners of the (pre-transform) strip rectangle in pixel space. */
  void (*apply)(const StripScreenQuad &quad, SequenceModifierData *smd, ImBuf *ibuf, ImBuf *mask);
};

const SequenceModifierTypeInfo *SEQ_modifier_type_info_get(int type);
SequenceModifierData *SEQ_modifier_new(Sequence *seq, const char *name, int type);
bool SEQ_modifier_remove(Sequence *seq, SequenceModifierData *smd);
void SEQ_modifier_clear(Sequence *seq);
void SEQ_modifier_free(SequenceModifierData *smd);
void SEQ_modifier_unique_name(Sequence *seq, SequenceModifierData *smd);
SequenceModifierData *SEQ_modifier_find_by_name(Sequence *seq, const char *name);
void SEQ_modifier_apply_stack(const SeqRenderData *context,
                              const Sequence *seq,
                              ImBuf *ibuf,
                              int timeline_frame);
void SEQ_modifier_list_copy(Sequence *seqn, Sequence *seq);
int SEQ_sequence_supports_modifiers(Sequence *seq);

void SEQ_modifier_blend_write(BlendWriter *writer, ListBase *modbase);
void SEQ_modifier_blend_read_data(BlendDataReader *reader, ListBase *lb);
