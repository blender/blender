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
 * The Original Code is Copyright (C) 2009 Blender Foundation, Joshua Leung
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#ifndef __NLA_PRIVATE_H__
#define __NLA_PRIVATE_H__

struct Depsgraph;

#include "RNA_types.h"
#include "BLI_bitmap.h"
#include "BLI_ghash.h"

/* --------------- NLA Evaluation DataTypes ----------------------- */

/* used for list of strips to accumulate at current time */
typedef struct NlaEvalStrip {
  struct NlaEvalStrip *next, *prev;

  NlaTrack *track; /* track that this strip belongs to */
  NlaStrip *strip; /* strip that's being used */

  short track_index; /* the index of the track within the list */
  short strip_mode;  /* which end of the strip are we looking at */

  float strip_time; /* time at which which strip is being evaluated */
} NlaEvalStrip;

/* NlaEvalStrip->strip_mode */
enum eNlaEvalStrip_StripMode {
  /* standard evaluation */
  NES_TIME_BEFORE = -1,
  NES_TIME_WITHIN,
  NES_TIME_AFTER,

  /* transition-strip evaluations */
  NES_TIME_TRANSITION_START,
  NES_TIME_TRANSITION_END,
};

struct NlaEvalChannel;
struct NlaEvalData;

/* Unique channel key for GHash. */
typedef struct NlaEvalChannelKey {
  struct PointerRNA ptr;
  struct PropertyRNA *prop;
} NlaEvalChannelKey;

/* Bitmask of array indices touched by actions. */
typedef struct NlaValidMask {
  BLI_bitmap *ptr;
  BLI_bitmap buffer[sizeof(uint64_t) / sizeof(BLI_bitmap)];
} NlaValidMask;

/* Set of property values for blending. */
typedef struct NlaEvalChannelSnapshot {
  struct NlaEvalChannel *channel;

  int length;   /* Number of values in the property. */
  bool is_base; /* Base snapshot of the channel. */

  float values[]; /* Item values. */
  /* Memory over-allocated to provide space for values. */
} NlaEvalChannelSnapshot;

/* NlaEvalChannel->mix_mode */
enum eNlaEvalChannel_MixMode {
  NEC_MIX_ADD,
  NEC_MIX_MULTIPLY,
  NEC_MIX_QUATERNION,
  NEC_MIX_AXIS_ANGLE,
};

/* Temp channel for accumulating data from NLA for a single property.
 * Handles array properties as a unit to allow intelligent blending. */
typedef struct NlaEvalChannel {
  struct NlaEvalChannel *next, *prev;
  struct NlaEvalData *owner;

  /* Original RNA path string and property key. */
  const char *rna_path;
  NlaEvalChannelKey key;

  int index;
  bool is_array;
  bool in_blend;
  char mix_mode;

  struct NlaEvalChannel *next_blend;
  NlaEvalChannelSnapshot *blend_snapshot;

  /* Mask of array items controlled by NLA. */
  NlaValidMask valid;

  /* Base set of values. */
  NlaEvalChannelSnapshot base_snapshot;
  /* Memory over-allocated to provide space for base_snapshot.values. */
} NlaEvalChannel;

/* Set of values for all channels. */
typedef struct NlaEvalSnapshot {
  /* Snapshot this one defaults to. */
  struct NlaEvalSnapshot *base;

  int size;
  NlaEvalChannelSnapshot **channels;
} NlaEvalSnapshot;

/* Set of all channels covered by NLA. */
typedef struct NlaEvalData {
  ListBase channels;

  /* Mapping of paths and NlaEvalChannelKeys to channels. */
  GHash *path_hash;
  GHash *key_hash;

  /* Base snapshot. */
  int num_channels;
  NlaEvalSnapshot base_snapshot;

  /* Evaluation result shapshot. */
  NlaEvalSnapshot eval_snapshot;
} NlaEvalData;

/* Information about the currently edited strip and ones below it for keyframing. */
typedef struct NlaKeyframingContext {
  struct NlaKeyframingContext *next, *prev;

  /* AnimData for which this context was built. */
  struct AnimData *adt;

  /* Data of the currently edited strip (copy, or fake strip for the main action). */
  NlaStrip strip;
  NlaEvalStrip *eval_strip;

  /* Evaluated NLA stack below the current strip. */
  NlaEvalData nla_channels;
} NlaKeyframingContext;

/* --------------- NLA Functions (not to be used as a proper API) ----------------------- */

/* convert from strip time <-> global time */
float nlastrip_get_frame(NlaStrip *strip, float cframe, short mode);

/* --------------- NLA Evaluation (very-private stuff) ----------------------- */
/* these functions are only defined here to avoid problems with the order
 * in which they get defined. */

NlaEvalStrip *nlastrips_ctime_get_strip(
    struct Depsgraph *depsgraph, ListBase *list, ListBase *strips, short index, float ctime);
void nlastrip_evaluate(struct Depsgraph *depsgraph,
                       PointerRNA *ptr,
                       NlaEvalData *channels,
                       ListBase *modifiers,
                       NlaEvalStrip *nes,
                       NlaEvalSnapshot *snapshot);
void nladata_flush_channels(struct Depsgraph *depsgraph,
                            PointerRNA *ptr,
                            NlaEvalData *channels,
                            NlaEvalSnapshot *snapshot);

#endif /* __NLA_PRIVATE_H__ */
