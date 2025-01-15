/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 */

#pragma once

struct bActionGroup;
struct BezTriple;
struct FCurve;
struct ID;
struct Main;

/**
 * Datatype for use in the keyframe copy/paste buffer.
 */
struct tAnimCopybufItem {
  tAnimCopybufItem *next, *prev;

  ID *id;            /* ID which owns the curve */
  bActionGroup *grp; /* Action Group */
  char *rna_path;    /* RNA-Path */
  int array_index;   /* array index */

  int totvert;     /* number of keyframes stored for this channel */
  BezTriple *bezt; /* keyframes in buffer */

  short id_type; /* Result of `GS(id->name)`. */
  bool is_bone;  /* special flag for armature bones */
};

void tAnimCopybufItem_free(tAnimCopybufItem *aci);

/**
 * Flip bone names in the RNA path, returning the flipped path.
 *
 * Returns empty optional if the aci is not a bone (is_bone=false or RNA path
 * has an unexpected prefix).
 */
std::optional<std::string> flip_names(const tAnimCopybufItem *aci);

/**
 * Most strict paste buffer matching method: exact matches only.
 */
bool pastebuf_match_path_full(Main *bmain,
                              const FCurve *fcu,
                              const tAnimCopybufItem *aci,
                              bool from_single,
                              bool to_single,
                              bool flip);

/**
 * Medium strict paste buffer matching method: match the property name only.
 */
bool pastebuf_match_path_property(Main *bmain,
                                  const FCurve *fcu,
                                  const tAnimCopybufItem *aci,
                                  bool from_single,
                                  bool to_single,
                                  bool flip);

/**
 * Least strict paste buffer matching method: indices only.
 */
bool pastebuf_match_index_only(Main *bmain,
                               const FCurve *fcu,
                               const tAnimCopybufItem *aci,
                               bool from_single,
                               bool to_single,
                               bool flip);
