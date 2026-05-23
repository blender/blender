/* SPDX-FileCopyrightText: 2023-2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "IMB_imbuf_enums.h"

namespace blender {

struct MovieReader;

MovieReader *movie_open_proxy(MovieReader *anim, IMB_Proxy_Size preview_size);

}  // namespace blender
