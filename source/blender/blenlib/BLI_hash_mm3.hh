/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_sys_types.h"

uint32_t BLI_hash_mm3(const unsigned char *data, size_t len, uint32_t seed);
