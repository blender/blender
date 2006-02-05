/**
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2003 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * 
 */

#ifndef BSE_SEQAUDIO_H
#define BSE_SEQAUDIO_H

#include "SDL.h"
/* muha, we don't init (no SDL_main)! */
#ifdef main
#	undef main
#endif

#include "DNA_sound_types.h"

void audio_mixdown();
void audio_makestream(bSound *sound);
void audiostream_play(Uint32 startframe, Uint32 duration, int mixdown);
void audiostream_fill(Uint8* mixdown, int len);
void audiostream_start(Uint32 frame);
void audiostream_scrub(Uint32 frame);
void audiostream_stop(void);
int audiostream_pos(void);

#endif

