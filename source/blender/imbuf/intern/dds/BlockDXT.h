/**
 * $Id$
 *
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
 * Contributors: Amorilia (amorilia@gamebox.net)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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

#ifndef _DDS_BLOCKDXT_H
#define _DDS_BLOCKDXT_H

#include <Color.h>
#include <ColorBlock.h>
#include <Stream.h>

/// DXT1 block.
struct BlockDXT1
{
	Color16 col0;
	Color16 col1;
	union {
		unsigned char row[4];
		unsigned int indices;
	};

	bool isFourColorMode() const;

	unsigned int evaluatePalette(Color32 color_array[4]) const;
	unsigned int evaluatePaletteFast(Color32 color_array[4]) const;
	void evaluatePalette3(Color32 color_array[4]) const;
	void evaluatePalette4(Color32 color_array[4]) const;
	
	void decodeBlock(ColorBlock * block) const;
	
	void setIndices(int * idx);

	void flip4();
	void flip2();
};

/// Return true if the block uses four color mode, false otherwise.
inline bool BlockDXT1::isFourColorMode() const
{
	return col0.u >= col1.u;	// @@ > or >= ?
}


/// DXT3 alpha block with explicit alpha.
struct AlphaBlockDXT3
{
	union {
		struct {
			unsigned int alpha0 : 4;
			unsigned int alpha1 : 4;
			unsigned int alpha2 : 4;
			unsigned int alpha3 : 4;
			unsigned int alpha4 : 4;
			unsigned int alpha5 : 4;
			unsigned int alpha6 : 4;
			unsigned int alpha7 : 4;
			unsigned int alpha8 : 4;
			unsigned int alpha9 : 4;
			unsigned int alphaA : 4;
			unsigned int alphaB : 4;
			unsigned int alphaC : 4;
			unsigned int alphaD : 4;
			unsigned int alphaE : 4;
			unsigned int alphaF : 4;
		};
		unsigned short row[4];
	};
	
	void decodeBlock(ColorBlock * block) const;
	
	void flip4();
	void flip2();
};


/// DXT3 block.
struct BlockDXT3
{
	AlphaBlockDXT3 alpha;
	BlockDXT1 color;
	
	void decodeBlock(ColorBlock * block) const;
	
	void flip4();
	void flip2();
};


/// DXT5 alpha block.
struct AlphaBlockDXT5
{
	union {
		struct {
			unsigned long long alpha0 : 8;	// 8
			unsigned long long alpha1 : 8;	// 16
			unsigned long long bits0 : 3;	// 3 - 19
			unsigned long long bits1 : 3; 	// 6 - 22
			unsigned long long bits2 : 3; 	// 9 - 25
			unsigned long long bits3 : 3;	// 12 - 28
			unsigned long long bits4 : 3;	// 15 - 31
			unsigned long long bits5 : 3;	// 18 - 34
			unsigned long long bits6 : 3;	// 21 - 37
			unsigned long long bits7 : 3;	// 24 - 40
			unsigned long long bits8 : 3;	// 27 - 43
			unsigned long long bits9 : 3; 	// 30 - 46
			unsigned long long bitsA : 3; 	// 33 - 49
			unsigned long long bitsB : 3;	// 36 - 52
			unsigned long long bitsC : 3;	// 39 - 55
			unsigned long long bitsD : 3;	// 42 - 58
			unsigned long long bitsE : 3;	// 45 - 61
			unsigned long long bitsF : 3;	// 48 - 64
		};
		unsigned long long u;
	};
	
	void evaluatePalette(unsigned char alpha[8]) const;
	void evaluatePalette8(unsigned char alpha[8]) const;
	void evaluatePalette6(unsigned char alpha[8]) const;
	void indices(unsigned char index_array[16]) const;

	unsigned int index(unsigned int index) const;
	void setIndex(unsigned int index, unsigned int value);
	
	void decodeBlock(ColorBlock * block) const;
	
	void flip4();
	void flip2();
};


/// DXT5 block.
struct BlockDXT5
{
	AlphaBlockDXT5 alpha;
	BlockDXT1 color;
	
	void decodeBlock(ColorBlock * block) const;
	
	void flip4();
	void flip2();
};

/// ATI1 block.
struct BlockATI1
{
	AlphaBlockDXT5 alpha;
	
	void decodeBlock(ColorBlock * block) const;
	
	void flip4();
	void flip2();
};

/// ATI2 block.
struct BlockATI2
{
	AlphaBlockDXT5 x;
	AlphaBlockDXT5 y;
	
	void decodeBlock(ColorBlock * block) const;
	
	void flip4();
	void flip2();
};

void mem_read(Stream & mem, BlockDXT1 & block);
void mem_read(Stream & mem, AlphaBlockDXT3 & block);
void mem_read(Stream & mem, BlockDXT3 & block);
void mem_read(Stream & mem, AlphaBlockDXT5 & block);
void mem_read(Stream & mem, BlockDXT5 & block);
void mem_read(Stream & mem, BlockATI1 & block);
void mem_read(Stream & mem, BlockATI2 & block);

#endif // _DDS_BLOCKDXT_H
