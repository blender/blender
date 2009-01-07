/**
 * sound.h (mar-2001 nzc)
 *	
 * $Id$ 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef BKE_SOUND_H
#define BKE_SOUND_H

struct PackedFile;
struct bSound;
struct bSample;
struct ListBase;

/* bad bad global... */
extern struct ListBase *samples;

void sound_free_all_samples(void);

/*  void *sound_get_listener(void); implemented in src!also declared there now  */

void sound_set_packedfile(struct bSample* sample, struct PackedFile* pf);
struct PackedFile* sound_find_packedfile(struct bSound* sound);
void sound_free_sample(struct bSample* sample);
void sound_free_sound(struct bSound* sound);

#endif

