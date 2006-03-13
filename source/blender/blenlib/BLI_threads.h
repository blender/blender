/*
 *
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_THREADS_H
#define BLI_THREADS_H 

/* default lock is to protect MEM_ module calls, one custom lock available now. van be extended */
#define LOCK_MALLOC		0
#define LOCK_CUSTOM1	1


void	BLI_init_threads	(ListBase *threadbase, void *(*do_thread)(void *), int tot);
int		BLI_available_threads(ListBase *threadbase);
int		BLI_available_thread_index(ListBase *threadbase);
void	BLI_insert_thread	(ListBase *threadbase, void *callerdata);
void	BLI_remove_thread	(ListBase *threadbase, void *callerdata);
void	BLI_end_threads		(ListBase *threadbase);

void	BLI_lock_thread		(int type);
void	BLI_unlock_thread	(int type);

		/* threadsafe version of MEM_malloc and friends */
void	*MEM_mallocT(int len, char *name);
void	*MEM_callocT(int len, char *name);
void	*MEM_mapallocT(int len, char *name);
void	MEM_freeT(void *poin);

#endif

