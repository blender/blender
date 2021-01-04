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
 *
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct ImBuf;
struct ListBase;
struct Scene;
struct SeqRenderData;
struct Sequence;
struct SequenceModifierData;

typedef struct SequenceModifierTypeInfo {
  /* default name for the modifier */
  char name[64]; /* MAX_NAME */

  /* DNA structure name used on load/save filed */
  char struct_name[64]; /* MAX_NAME */

  /* size of modifier data structure, used by allocation */
  int struct_size;

  /* data initialization */
  void (*init_data)(struct SequenceModifierData *smd);

  /* free data used by modifier,
   * only modifier-specific data should be freed, modifier descriptor would
   * be freed outside of this callback
   */
  void (*free_data)(struct SequenceModifierData *smd);

  /* copy data from one modifier to another */
  void (*copy_data)(struct SequenceModifierData *smd, struct SequenceModifierData *target);

  /* apply modifier on a given image buffer */
  void (*apply)(struct SequenceModifierData *smd, struct ImBuf *ibuf, struct ImBuf *mask);
} SequenceModifierTypeInfo;

const struct SequenceModifierTypeInfo *SEQ_modifier_type_info_get(int type);
struct SequenceModifierData *SEQ_modifier_new(struct Sequence *seq, const char *name, int type);
bool SEQ_modifier_remove(struct Sequence *seq, struct SequenceModifierData *smd);
void SEQ_modifier_clear(struct Sequence *seq);
void SEQ_modifier_free(struct SequenceModifierData *smd);
void SEQ_modifier_unique_name(struct Sequence *seq, struct SequenceModifierData *smd);
struct SequenceModifierData *SEQ_modifier_find_by_name(struct Sequence *seq, const char *name);
struct ImBuf *SEQ_modifier_apply_stack(const struct SeqRenderData *context,
                                       struct Sequence *seq,
                                       struct ImBuf *ibuf,
                                       int timeline_frame);
void SEQ_modifier_list_copy(struct Sequence *seqn, struct Sequence *seq);
int SEQ_sequence_supports_modifiers(struct Sequence *seq);

void SEQ_modifier_blend_write(struct BlendWriter *writer, struct ListBase *modbase);
void SEQ_modifier_blend_read_data(struct BlendDataReader *reader, struct ListBase *lb);
void SEQ_modifier_blend_read_lib(struct BlendLibReader *reader,
                                 struct Scene *scene,
                                 struct ListBase *lb);

#ifdef __cplusplus
}
#endif
