/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 * \brief Header file for allocimbuf.c
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ImBuf;

void imb_refcounter_lock_init(void);
void imb_refcounter_lock_exit(void);

#ifndef WIN32
void imb_mmap_lock_init(void);
void imb_mmap_lock_exit(void);
void imb_mmap_lock(void);
void imb_mmap_unlock(void);
#else
#  define imb_mmap_lock_init()
#  define imb_mmap_lock_exit()
#  define imb_mmap_lock()
#  define imb_mmap_unlock()
#endif

bool imb_addencodedbufferImBuf(struct ImBuf *ibuf);
bool imb_enlargeencodedbufferImBuf(struct ImBuf *ibuf);

#ifdef __cplusplus
}
#endif
