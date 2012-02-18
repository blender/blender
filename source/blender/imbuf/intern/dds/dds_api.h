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
 * Contributors: Amorilia (amorilia@users.sourceforge.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/dds/dds_api.h
 *  \ingroup imbdds
 */


#ifndef __DDS_API_H__
#define __DDS_API_H__

#ifdef __cplusplus
extern "C" {
#endif

int  	      imb_save_dds(struct ImBuf *ibuf, const char *name, int flags);
int           imb_is_a_dds(unsigned char *mem); /* use only first 32 bytes of mem */
struct ImBuf *imb_load_dds(unsigned char *mem, size_t size, int flags);

#ifdef __cplusplus
}
#endif

#endif /* __DDS_API_H */
