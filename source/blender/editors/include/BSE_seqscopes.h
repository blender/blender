/**
 * $Id: BSE_seqscopes.h 6983 2006-03-07 20:01:12Z schlaile $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * Author: Peter Schlaile < peter [at] schlaile [dot] de >
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef BSE_SEQUENCE_SCOPES_H
#define BSE_SEQUENCE_SCOPES_H

struct ImBuf;

struct ImBuf *make_waveform_view_from_ibuf(struct ImBuf * ibuf);
struct ImBuf *make_vectorscope_view_from_ibuf(struct ImBuf * ibuf);

#endif

