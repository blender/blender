/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief General operations for speakers.
 */

struct Main;
struct Speaker;

struct Speaker *BKE_speaker_add(struct Main *bmain, const char *name);
