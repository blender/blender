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

/** \file blender/imbuf/intern/dds/PixelFormat.h
 *  \ingroup imbdds
 */


/*
 * This file is based on a similar file from the NVIDIA texture tools
 * (http://nvidia-texture-tools.googlecode.com/)
 *
 * Original license from NVIDIA follows.
 */

// Copyright NVIDIA Corporation 2007 -- Ignacio Castano <icastano@nvidia.com>
// 
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without
// restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following
// conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

#ifndef __PIXELFORMAT_H__
#define __PIXELFORMAT_H__

#include <Common.h>

	namespace PixelFormat
	{

		// Convert component \a c having \a inbits to the returned value having \a outbits.
		inline uint convert(uint c, uint inbits, uint outbits)
		{
			if (inbits == 0)
			{
				return 0;
			}
			else if (inbits >= outbits)
			{
				// truncate
				return c >> (inbits - outbits);
			}
			else
			{
				// bitexpand
				return (c << (outbits - inbits)) | convert(c, inbits, outbits - inbits);
			}
		}

		// Get pixel component shift and size given its mask.
		inline void maskShiftAndSize(uint mask, uint * shift, uint * size)
		{
			if (!mask)
			{
				*shift = 0;
				*size = 0;
				return;
			}

			*shift = 0;
			while((mask & 1) == 0) {
				++(*shift);
				mask >>= 1;
			}
			
			*size = 0;
			while((mask & 1) == 1) {
				++(*size);
				mask >>= 1;
			}
		}

        inline float quantizeCeil(float f, int inbits, int outbits)
        {
            //uint i = f * (float(1 << inbits) - 1);
            //i = convert(i, inbits, outbits);
            //float result = float(i) / (float(1 << outbits) - 1);
            //nvCheck(result >= f);
            float result;
            int offset = 0;
            do {
                uint i = offset + uint(f * (float(1 << inbits) - 1));
                i = convert(i, inbits, outbits);
                result = float(i) / (float(1 << outbits) - 1);
                offset++;
            } while (result < f);

            return result;
        }

        /*
        inline float quantizeRound(float f, int bits)
        {
            float scale = float(1 << bits);
            return fround(f * scale) / scale;
        }

        inline float quantizeFloor(float f, int bits)
        {
            float scale = float(1 << bits);
            return floor(f * scale) / scale;
        }
        */

	} // PixelFormat namespace

#endif // _DDS_IMAGE_PIXELFORMAT_H
