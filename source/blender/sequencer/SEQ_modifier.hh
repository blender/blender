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
struct Strip;
struct SequenceModifierData;

namespace blender::seq {

struct StripScreenQuad;
struct RenderData;

struct StripModifierTypeInfo {
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

const StripModifierTypeInfo *modifier_type_info_get(int type);
SequenceModifierData *modifier_new(Strip *strip, const char *name, int type);
bool modifier_remove(Strip *strip, SequenceModifierData *smd);
void modifier_clear(Strip *strip);
void modifier_free(SequenceModifierData *smd);
void modifier_unique_name(Strip *strip, SequenceModifierData *smd);
SequenceModifierData *modifier_find_by_name(Strip *strip, const char *name);
void modifier_apply_stack(const RenderData *context,
                          const Strip *strip,
                          ImBuf *ibuf,
                          int timeline_frame);
void modifier_list_copy(Strip *seqn, Strip *strip);
int sequence_supports_modifiers(Strip *strip);

void modifier_blend_write(BlendWriter *writer, ListBase *modbase);
void modifier_blend_read_data(BlendDataReader *reader, ListBase *lb);

}  // namespace blender::seq
