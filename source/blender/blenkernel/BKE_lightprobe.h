/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations for probes.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct LightProbe;
struct Main;

void BKE_lightprobe_type_set(struct LightProbe *probe, short lightprobe_type);
void *BKE_lightprobe_add(struct Main *bmain, const char *name);

#ifdef __cplusplus
}
#endif
