/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * This file only contains the memory management functions for the Animation
 * data-block. For all other functionality, see `source/blender/animrig`.
 */

#pragma once

struct Animation;
struct Main;

Animation *BKE_animation_add(Main *bmain, const char name[]);
