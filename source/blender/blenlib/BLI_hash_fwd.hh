/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>

namespace blender {

template<typename T, typename... Args>
inline uint64_t get_default_hash(const T &v, const Args &...args);

}
