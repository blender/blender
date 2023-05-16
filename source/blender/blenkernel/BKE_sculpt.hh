/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 by Nicholas Bishop. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_attribute.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BLI_compiler_compat.h"

/*
 * Stroke ID API.  This API is used to detect if
 * an element has already been processed for some task
 * inside a given stroke.
 */

struct StrokeID {
  short id;
  short userflag;
};

enum StrokeIDUser {
  STROKEID_USER_AUTOMASKING = 1 << 0,
  STROKEID_USER_BOUNDARY = 1 << 1,
  STROKEID_USER_SCULPTVERT = 1 << 2,
  STROKEID_USER_PREV_COLOR = 1 << 3,
  STROKEID_USER_SMOOTH = 1 << 4,
  STROKEID_USER_OCCLUSION = 1 << 5,
  STROKEID_USER_LAYER_BRUSH = 1 << 6,
  STROKEID_USER_ORIGINAL = 1 << 7,
};
ENUM_OPERATORS(StrokeIDUser, STROKEID_USER_LAYER_BRUSH);

void BKE_sculpt_reproject_cdata(SculptSession *ss,
                                PBVHVertRef vertex,
                                float startco[3],
                                float startno[3]);

namespace blender::bke::sculpt {
BLI_INLINE bool stroke_id_clear(SculptSession *ss, PBVHVertRef vertex, StrokeIDUser user)
{
  StrokeID *id = blender::bke::paint::vertex_attr_ptr<StrokeID>(vertex, ss->attrs.stroke_id);

  bool retval = id->userflag & user;
  id->userflag &= ~user;

  return retval;
}

BLI_INLINE bool stroke_id_test(SculptSession *ss, PBVHVertRef vertex, StrokeIDUser user)
{
  StrokeID *id = blender::bke::paint::vertex_attr_ptr<StrokeID>(vertex, ss->attrs.stroke_id);
  bool ret;

  if (id->id != ss->stroke_id) {
    id->id = ss->stroke_id;
    id->userflag = 0;
    ret = true;
  }
  else {
    ret = !(id->userflag & (int)user);
  }

  id->userflag |= (int)user;

  return ret;
}

BLI_INLINE bool stroke_id_test_no_update(SculptSession *ss, PBVHVertRef vertex, StrokeIDUser user)
{
  StrokeID *id = blender::bke::paint::vertex_attr_ptr<StrokeID>(vertex, ss->attrs.stroke_id);

  if (id->id != ss->stroke_id) {
    return true;
  }

  return !(id->userflag & (int)user);
}

BLI_INLINE void add_sculpt_flag(SculptSession *ss, PBVHVertRef vertex, uint8_t flag)
{
  *blender::bke::paint::vertex_attr_ptr<uint8_t>(vertex, ss->attrs.flags) |= flag;
}
BLI_INLINE void clear_sculpt_flag(SculptSession *ss, PBVHVertRef vertex, uint8_t flag)
{
  *blender::bke::paint::vertex_attr_ptr<uint8_t>(vertex, ss->attrs.flags) &= ~flag;
}
BLI_INLINE bool test_sculpt_flag(SculptSession *ss, PBVHVertRef vertex, uint8_t flag)
{
  return blender::bke::paint::vertex_attr_get<uint8_t>(vertex, ss->attrs.flags) & flag;
}
}  // namespace blender::bke::sculpt
