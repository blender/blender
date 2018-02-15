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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_HASH_H__
#define __BLI_HASH_H__

/** \file BLI_hash.h
 *  \ingroup bli
 */

BLI_INLINE unsigned int BLI_hash_int_2d(unsigned int kx, unsigned int ky)
{
#define rot(x, k) (((x) << (k)) | ((x) >> (32 - (k))))

	unsigned int a, b, c;

	a = b = c = 0xdeadbeef + (2 << 2) + 13;
	a += kx;
	b += ky;

	c ^= b; c -= rot(b, 14);
	a ^= c; a -= rot(c, 11);
	b ^= a; b -= rot(a, 25);
	c ^= b; c -= rot(b, 16);
	a ^= c; a -= rot(c, 4);
	b ^= a; b -= rot(a, 14);
	c ^= b; c -= rot(b, 24);

	return c;

#undef rot
}

BLI_INLINE unsigned int BLI_hash_string(const char *str)
{
	unsigned int i = 0, c;

	while ((c = *str++)) {
		i = i * 37 + c;
	}
	return i;
}

BLI_INLINE unsigned int BLI_hash_int(unsigned int k)
{
	return BLI_hash_int_2d(k, 0);
}

BLI_INLINE float BLI_hash_int_01(unsigned int k)
{
	return (float)BLI_hash_int(k) * (1.0f / (float)0xFFFFFFFF);
}

#endif // __BLI_HASH_H__
