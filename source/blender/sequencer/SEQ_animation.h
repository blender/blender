/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct GSet;
struct ListBase;
struct Scene;
struct Sequence;

void SEQ_free_animdata(struct Scene *scene, struct Sequence *seq);
void SEQ_offset_animdata(struct Scene *scene, struct Sequence *seq, int ofs);
struct GSet *SEQ_fcurves_by_strip_get(const struct Sequence *seq, struct ListBase *fcurve_base);

#ifdef __cplusplus
}
#endif
