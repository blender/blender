/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations for speakers.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;

void *BKE_speaker_add(struct Main *bmain, const char *name);

#ifdef __cplusplus
}
#endif
