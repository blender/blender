/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Each material can have a different UBO layout for its material data.
 * This layout is runtime generated. This file is just a placeholder for the include system.
 */

#pragma once

/* This file must replaced at runtime. The following content is only a possible implementation. */
#pragma runtime_generated

struct NodeTree {
  float crypto_hash;
  float _pad0;
  float _pad1;
  float _pad2;
};
