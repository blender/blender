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
 * Contributor(s): Jörg Müller.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_SPEAKER_H__
#define __BKE_SPEAKER_H__

/** \file BKE_speaker.h
 *  \ingroup bke
 *  \brief General operations for speakers.
 */

struct Main;
struct Speaker;

void BKE_speaker_init(struct Speaker *spk);
void *BKE_speaker_add(struct Main *bmain, const char *name);
struct Speaker *BKE_speaker_copy(struct Main *bmain, const struct Speaker *spk);
void BKE_speaker_make_local(struct Main *bmain, struct Speaker *spk, const bool lib_local);
void BKE_speaker_free(struct Speaker *spk);

#endif
