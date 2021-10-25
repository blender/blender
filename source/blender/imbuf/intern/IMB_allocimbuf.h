/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**
 * \file IMB_allocimbuf.h
 * \ingroup imbuf
 * \brief Header file for allocimbuf.c
 */
#ifndef __IMB_ALLOCIMBUF_H__
#define __IMB_ALLOCIMBUF_H__

struct ImBuf;

void imb_refcounter_lock_init(void);
void imb_refcounter_lock_exit(void);

#ifdef WIN32
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

#endif

