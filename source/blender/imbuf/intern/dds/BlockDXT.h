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

/** \file blender/imbuf/intern/dds/BlockDXT.h
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

#ifndef __BLOCKDXT_H__
#define __BLOCKDXT_H__

#include <Common.h>
#include <Color.h>
#include <ColorBlock.h>
#include <Stream.h>

/// DXT1 block.
struct BlockDXT1
{
	Color16 col0;
	Color16 col1;
	union {
		uint8 row[4];
		uint indices;
	};

	bool isFourColorMode() const;

	uint evaluatePalette(Color32 color_array[4]) const;
	uint evaluatePaletteNV5x(Color32 color_array[4]) const;

	void evaluatePalette3(Color32 color_array[4]) const;
	void evaluatePalette4(Color32 color_array[4]) const;
	
	void decodeBlock(ColorBlock * block) const;
	void decodeBlockNV5x(ColorBlock * block) const;
	
	void setIndices(int * idx);

	void flip4();
	void flip2();
};

/// Return true if the block uses four color mode, false otherwise.
inline bool BlockDXT1::isFourColorMode() const
{
	return col0.u > col1.u;
}


/// DXT3 alpha block with explicit alpha.
struct AlphaBlockDXT3
{
	union {
		struct {
			uint alpha0 : 4;
			uint alpha1 : 4;
			uint alpha2 : 4;
			uint alpha3 : 4;
			uint alpha4 : 4;
			uint alpha5 : 4;
			uint alpha6 : 4;
			uint alpha7 : 4;
			uint alpha8 : 4;
			uint alpha9 : 4;
			uint alphaA : 4;
			uint alphaB : 4;
			uint alphaC : 4;
			uint alphaD : 4;
			uint alphaE : 4;
			uint alphaF : 4;
		};
		uint16 row[4];
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
	void decodeBlockNV5x(ColorBlock * block) const;
	
	void flip4();
	void flip2();
};


/// DXT5 alpha block.
struct AlphaBlockDXT5
{
	// uint64 unions do not compile on all platforms
#if 0
	union {
		struct {
			uint64 alpha0 : 8;	// 8
			uint64 alpha1 : 8;	// 16
			uint64 bits0 : 3;		// 3 - 19
			uint64 bits1 : 3; 	// 6 - 22
			uint64 bits2 : 3; 	// 9 - 25
			uint64 bits3 : 3;		// 12 - 28
			uint64 bits4 : 3;		// 15 - 31
			uint64 bits5 : 3;		// 18 - 34
			uint64 bits6 : 3;		// 21 - 37
			uint64 bits7 : 3;		// 24 - 40
			uint64 bits8 : 3;		// 27 - 43
			uint64 bits9 : 3; 	// 30 - 46
			uint64 bitsA : 3; 	// 33 - 49
			uint64 bitsB : 3;		// 36 - 52
			uint64 bitsC : 3;		// 39 - 55
			uint64 bitsD : 3;		// 42 - 58
			uint64 bitsE : 3;		// 45 - 61
			uint64 bitsF : 3;		// 48 - 64
		};
		uint64 u;
	};
#endif
	uint64 u;
	uint8 alpha0() const { return u & 0xffLL; }
	uint8 alpha1() const { return (u >> 8) & 0xffLL; }
	uint8 bits0() const { return (u >> 16) & 0x7LL; }
	uint8 bits1() const { return (u >> 19) & 0x7LL; }
	uint8 bits2() const { return (u >> 22) & 0x7LL; }
	uint8 bits3() const { return (u >> 25) & 0x7LL; }
	uint8 bits4() const { return (u >> 28) & 0x7LL; }
	uint8 bits5() const { return (u >> 31) & 0x7LL; }
	uint8 bits6() const { return (u >> 34) & 0x7LL; }
	uint8 bits7() const { return (u >> 37) & 0x7LL; }
	uint8 bits8() const { return (u >> 40) & 0x7LL; }
	uint8 bits9() const { return (u >> 43) & 0x7LL; }
	uint8 bitsA() const { return (u >> 46) & 0x7LL; }
	uint8 bitsB() const { return (u >> 49) & 0x7LL; }
	uint8 bitsC() const { return (u >> 52) & 0x7LL; }
	uint8 bitsD() const { return (u >> 55) & 0x7LL; }
	uint8 bitsE() const { return (u >> 58) & 0x7LL; }
	uint8 bitsF() const { return (u >> 61) & 0x7LL; }
	
	void evaluatePalette(uint8 alpha[8]) const;
	void evaluatePalette8(uint8 alpha[8]) const;
	void evaluatePalette6(uint8 alpha[8]) const;
	void indices(uint8 index_array[16]) const;

	uint index(uint index) const;
	void setIndex(uint index, uint value);
	
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
	void decodeBlockNV5x(ColorBlock * block) const;
	
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

/// CTX1 block.
struct BlockCTX1
{
	uint8 col0[2];
	uint8 col1[2];
	union {
		uint8 row[4];
		uint indices;
	};

	void evaluatePalette(Color32 color_array[4]) const;
	void setIndices(int * idx);

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
void mem_read(Stream & mem, BlockCTX1 & block);

#endif  /* __BLOCKDXT_H__ */
