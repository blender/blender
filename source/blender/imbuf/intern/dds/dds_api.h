/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbdds
 */

#pragma once

#include "../../IMB_imbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

bool imb_is_a_dds(const unsigned char *mem, size_t size);
bool imb_save_dds(struct ImBuf *ibuf, const char *filepath, int flags);
struct ImBuf *imb_load_dds(const unsigned char *mem,
                           size_t size,
                           int flags,
                           char colorspace[IM_MAX_SPACE]);

#ifdef __cplusplus
}
#endif
