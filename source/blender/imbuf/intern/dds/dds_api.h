/**
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
 * Contributors: Amorilia (amorilia@gamebox.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef _DDS_API_H
#define _DDS_API_H

#ifdef __cplusplus
extern "C" {
#endif

short	      imb_save_dds(struct ImBuf *ibuf, char *name, int flags);
int           imb_is_a_dds(unsigned char *mem); /* use only first 32 bytes of mem */
struct ImBuf *imb_load_dds(unsigned char *mem, int size, int flags);

#ifdef __cplusplus
}
#endif

#endif /* __DDS_API_H */
