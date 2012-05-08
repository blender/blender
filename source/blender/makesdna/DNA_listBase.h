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
 *
 *
 */

/** \file DNA_listBase.h
 *  \ingroup DNA
 *  \brief These structs are the foundation for all linked lists in the
 *         library system.
 */

#ifndef __DNA_LISTBASE_H__
#define __DNA_LISTBASE_H__

#ifdef __cplusplus
extern "C" {
#endif

/* generic - all structs which are used in linked-lists used this */
typedef struct Link
{
	struct Link *next,*prev;
} Link;


/* use this when it is not worth defining a custom one... */
typedef struct LinkData
{
	struct LinkData *next, *prev;
	void *data;
} LinkData;

/* never change the size of this! genfile.c detects pointerlen with it */
typedef struct ListBase 
{
	void *first, *last;
} ListBase;

/* 8 byte alignment! */

#ifdef __cplusplus
}
#endif

#endif

