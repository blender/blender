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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* only include from header */
#ifndef __BLI_ENDIAN_SWITCH_H__
#  error "this file isnt to be directly included"
#endif

#ifndef __BLI_ENDIAN_SWITCH_INLINE_H__
#define __BLI_ENDIAN_SWITCH_INLINE_H__

/** \file blender/blenlib/BLI_endian_switch_inline.h
 *  \ingroup bli
 */


BLI_INLINE void BLI_endian_switch_int16(short *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i    = p_i[0];
	p_i[0] = p_i[1];
	p_i[1] = s_i;
}

BLI_INLINE void BLI_endian_switch_uint16(unsigned short *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i    = p_i[0];
	p_i[0] = p_i[1];
	p_i[1] = s_i;
}

BLI_INLINE void BLI_endian_switch_int32(int *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i = p_i[0]; p_i[0] = p_i[3]; p_i[3] = s_i;
	s_i = p_i[1]; p_i[1] = p_i[2]; p_i[2] = s_i;
}

BLI_INLINE void BLI_endian_switch_uint32(unsigned int *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i = p_i[0]; p_i[0] = p_i[3]; p_i[3] = s_i;
	s_i = p_i[1]; p_i[1] = p_i[2]; p_i[2] = s_i;
}

BLI_INLINE void BLI_endian_switch_float(float *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i = p_i[0]; p_i[0] = p_i[3]; p_i[3] = s_i;
	s_i = p_i[1]; p_i[1] = p_i[2]; p_i[2] = s_i;
}

BLI_INLINE void BLI_endian_switch_int64(int64_t *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i = p_i[0]; p_i[0] = p_i[7]; p_i[7] = s_i;
	s_i = p_i[1]; p_i[1] = p_i[6]; p_i[6] = s_i;
	s_i = p_i[2]; p_i[2] = p_i[5]; p_i[5] = s_i;
	s_i = p_i[3]; p_i[3] = p_i[4]; p_i[4] = s_i;
}

BLI_INLINE void BLI_endian_switch_uint64(uint64_t *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i = p_i[0]; p_i[0] = p_i[7]; p_i[7] = s_i;
	s_i = p_i[1]; p_i[1] = p_i[6]; p_i[6] = s_i;
	s_i = p_i[2]; p_i[2] = p_i[5]; p_i[5] = s_i;
	s_i = p_i[3]; p_i[3] = p_i[4]; p_i[4] = s_i;
}

BLI_INLINE void BLI_endian_switch_double(double *val)
{
	char *p_i = (char *)val;
	char s_i;

	s_i = p_i[0]; p_i[0] = p_i[7]; p_i[7] = s_i;
	s_i = p_i[1]; p_i[1] = p_i[6]; p_i[6] = s_i;
	s_i = p_i[2]; p_i[2] = p_i[5]; p_i[5] = s_i;
	s_i = p_i[3]; p_i[3] = p_i[4]; p_i[4] = s_i;
}

#endif  /* __BLI_ENDIAN_SWITCH_INLINE_H__ */
