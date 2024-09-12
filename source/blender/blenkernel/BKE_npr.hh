/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct bNodeTree;
struct Main;

bNodeTree *BKE_npr_tree_add(Main *bmain, const char *name);
