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

/** \file blender/imbuf/intern/dds/BlockDXT.cpp
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

#include <Common.h>
#include <Stream.h>
#include <ColorBlock.h>
#include <BlockDXT.h>

/*----------------------------------------------------------------------------
	BlockDXT1
----------------------------------------------------------------------------*/

uint BlockDXT1::evaluatePalette(Color32 color_array[4]) const
{
	// Does bit expansion before interpolation.
	color_array[0].b = (col0.b << 3) | (col0.b >> 2);
	color_array[0].g = (col0.g << 2) | (col0.g >> 4);
	color_array[0].r = (col0.r << 3) | (col0.r >> 2);
	color_array[0].a = 0xFF;

	// @@ Same as above, but faster?
//	Color32 c;
//	c.u = ((col0.u << 3) & 0xf8) | ((col0.u << 5) & 0xfc00) | ((col0.u << 8) & 0xf80000);
//	c.u |= (c.u >> 5) & 0x070007;
//	c.u |= (c.u >> 6) & 0x000300;
//	color_array[0].u = c.u;

	color_array[1].r = (col1.r << 3) | (col1.r >> 2);
	color_array[1].g = (col1.g << 2) | (col1.g >> 4);
	color_array[1].b = (col1.b << 3) | (col1.b >> 2);
	color_array[1].a = 0xFF;

	// @@ Same as above, but faster?
//	c.u = ((col1.u << 3) & 0xf8) | ((col1.u << 5) & 0xfc00) | ((col1.u << 8) & 0xf80000);
//	c.u |= (c.u >> 5) & 0x070007;
//	c.u |= (c.u >> 6) & 0x000300;
//	color_array[1].u = c.u;

	if ( col0.u > col1.u ) {
		// Four-color block: derive the other two colors.
		color_array[2].r = (2 * color_array[0].r + color_array[1].r) / 3;
		color_array[2].g = (2 * color_array[0].g + color_array[1].g) / 3;
		color_array[2].b = (2 * color_array[0].b + color_array[1].b) / 3;
		color_array[2].a = 0xFF;

		color_array[3].r = (2 * color_array[1].r + color_array[0].r) / 3;
		color_array[3].g = (2 * color_array[1].g + color_array[0].g) / 3;
		color_array[3].b = (2 * color_array[1].b + color_array[0].b) / 3;
		color_array[3].a = 0xFF;

		return 4;
	}
	else {
		// Three-color block: derive the other color.
		color_array[2].r = (color_array[0].r + color_array[1].r) / 2;
		color_array[2].g = (color_array[0].g + color_array[1].g) / 2;
		color_array[2].b = (color_array[0].b + color_array[1].b) / 2;
		color_array[2].a = 0xFF;

		// Set all components to 0 to match DXT specs.
		color_array[3].r = 0x00; // color_array[2].r;
		color_array[3].g = 0x00; // color_array[2].g;
		color_array[3].b = 0x00; // color_array[2].b;
		color_array[3].a = 0x00;

		return 3;
	}
}


uint BlockDXT1::evaluatePaletteNV5x(Color32 color_array[4]) const
{
	// Does bit expansion before interpolation.
	color_array[0].b = (3 * col0.b * 22) / 8;
	color_array[0].g = (col0.g << 2) | (col0.g >> 4);
	color_array[0].r = (3 * col0.r * 22) / 8;
	color_array[0].a = 0xFF;

	color_array[1].r = (3 * col1.r * 22) / 8;
	color_array[1].g = (col1.g << 2) | (col1.g >> 4);
	color_array[1].b = (3 * col1.b * 22) / 8;
	color_array[1].a = 0xFF;

	int gdiff = color_array[1].g - color_array[0].g;

	if ( col0.u > col1.u ) {
		// Four-color block: derive the other two colors.
		color_array[2].r = ((2 * col0.r + col1.r) * 22) / 8;
		color_array[2].g = (256 * color_array[0].g + gdiff / 4 + 128 + gdiff * 80) / 256;
		color_array[2].b = ((2 * col0.b + col1.b) * 22) / 8;
		color_array[2].a = 0xFF;

		color_array[3].r = ((2 * col1.r + col0.r) * 22) / 8;
		color_array[3].g = (256 * color_array[1].g - gdiff / 4 + 128 - gdiff * 80) / 256;
		color_array[3].b = ((2 * col1.b + col0.b) * 22) / 8;
		color_array[3].a = 0xFF;

		return 4;
	}
	else {
		// Three-color block: derive the other color.
		color_array[2].r = ((col0.r + col1.r) * 33) / 8;
		color_array[2].g = (256 * color_array[0].g + gdiff / 4 + 128 + gdiff * 128) / 256;
		color_array[2].b = ((col0.b + col1.b) * 33) / 8;
		color_array[2].a = 0xFF;

		// Set all components to 0 to match DXT specs.
		color_array[3].r = 0x00; // color_array[2].r;
		color_array[3].g = 0x00; // color_array[2].g;
		color_array[3].b = 0x00; // color_array[2].b;
		color_array[3].a = 0x00;

		return 3;
	}
}

// Evaluate palette assuming 3 color block.
void BlockDXT1::evaluatePalette3(Color32 color_array[4]) const
{
	color_array[0].b = (col0.b << 3) | (col0.b >> 2);
	color_array[0].g = (col0.g << 2) | (col0.g >> 4);
	color_array[0].r = (col0.r << 3) | (col0.r >> 2);
	color_array[0].a = 0xFF;

	color_array[1].r = (col1.r << 3) | (col1.r >> 2);
	color_array[1].g = (col1.g << 2) | (col1.g >> 4);
	color_array[1].b = (col1.b << 3) | (col1.b >> 2);
	color_array[1].a = 0xFF;

	// Three-color block: derive the other color.
	color_array[2].r = (color_array[0].r + color_array[1].r) / 2;
	color_array[2].g = (color_array[0].g + color_array[1].g) / 2;
	color_array[2].b = (color_array[0].b + color_array[1].b) / 2;
	color_array[2].a = 0xFF;

	// Set all components to 0 to match DXT specs.
	color_array[3].r = 0x00; // color_array[2].r;
	color_array[3].g = 0x00; // color_array[2].g;
	color_array[3].b = 0x00; // color_array[2].b;
	color_array[3].a = 0x00;
}

// Evaluate palette assuming 4 color block.
void BlockDXT1::evaluatePalette4(Color32 color_array[4]) const
{
	color_array[0].b = (col0.b << 3) | (col0.b >> 2);
	color_array[0].g = (col0.g << 2) | (col0.g >> 4);
	color_array[0].r = (col0.r << 3) | (col0.r >> 2);
	color_array[0].a = 0xFF;

	color_array[1].r = (col1.r << 3) | (col1.r >> 2);
	color_array[1].g = (col1.g << 2) | (col1.g >> 4);
	color_array[1].b = (col1.b << 3) | (col1.b >> 2);
	color_array[1].a = 0xFF;

	// Four-color block: derive the other two colors.
	color_array[2].r = (2 * color_array[0].r + color_array[1].r) / 3;
	color_array[2].g = (2 * color_array[0].g + color_array[1].g) / 3;
	color_array[2].b = (2 * color_array[0].b + color_array[1].b) / 3;
	color_array[2].a = 0xFF;

	color_array[3].r = (2 * color_array[1].r + color_array[0].r) / 3;
	color_array[3].g = (2 * color_array[1].g + color_array[0].g) / 3;
	color_array[3].b = (2 * color_array[1].b + color_array[0].b) / 3;
	color_array[3].a = 0xFF;
}


void BlockDXT1::decodeBlock(ColorBlock *block) const
{
	// Decode color block.
	Color32 color_array[4];
	evaluatePalette(color_array);

	// Write color block.
	for ( uint j = 0; j < 4; j++ ) {
		for ( uint i = 0; i < 4; i++ ) {
			uint idx = (row[j] >> (2 * i)) & 3;
			block->color(i, j) = color_array[idx];
		}
	}
}

void BlockDXT1::decodeBlockNV5x(ColorBlock *block) const
{
	// Decode color block.
	Color32 color_array[4];
	evaluatePaletteNV5x(color_array);

	// Write color block.
	for ( uint j = 0; j < 4; j++ ) {
		for ( uint i = 0; i < 4; i++ ) {
			uint idx = (row[j] >> (2 * i)) & 3;
			block->color(i, j) = color_array[idx];
		}
	}
}

void BlockDXT1::setIndices(int *idx)
{
	indices = 0;
	for (uint i = 0; i < 16; i++) {
		indices |= (idx[i] & 3) << (2 * i);
	}
}


/// Flip DXT1 block vertically.
inline void BlockDXT1::flip4()
{
	swap(row[0], row[3]);
	swap(row[1], row[2]);
}

/// Flip half DXT1 block vertically.
inline void BlockDXT1::flip2()
{
	swap(row[0], row[1]);
}


/*----------------------------------------------------------------------------
	BlockDXT3
----------------------------------------------------------------------------*/

void BlockDXT3::decodeBlock(ColorBlock *block) const
{
	// Decode color.
	color.decodeBlock(block);

	// Decode alpha.
	alpha.decodeBlock(block);
}

void BlockDXT3::decodeBlockNV5x(ColorBlock *block) const
{
	color.decodeBlockNV5x(block);
	alpha.decodeBlock(block);
}

void AlphaBlockDXT3::decodeBlock(ColorBlock *block) const
{
	block->color(0x0).a = (alpha0 << 4) | alpha0;
	block->color(0x1).a = (alpha1 << 4) | alpha1;
	block->color(0x2).a = (alpha2 << 4) | alpha2;
	block->color(0x3).a = (alpha3 << 4) | alpha3;
	block->color(0x4).a = (alpha4 << 4) | alpha4;
	block->color(0x5).a = (alpha5 << 4) | alpha5;
	block->color(0x6).a = (alpha6 << 4) | alpha6;
	block->color(0x7).a = (alpha7 << 4) | alpha7;
	block->color(0x8).a = (alpha8 << 4) | alpha8;
	block->color(0x9).a = (alpha9 << 4) | alpha9;
	block->color(0xA).a = (alphaA << 4) | alphaA;
	block->color(0xB).a = (alphaB << 4) | alphaB;
	block->color(0xC).a = (alphaC << 4) | alphaC;
	block->color(0xD).a = (alphaD << 4) | alphaD;
	block->color(0xE).a = (alphaE << 4) | alphaE;
	block->color(0xF).a = (alphaF << 4) | alphaF;
}

/// Flip DXT3 alpha block vertically.
void AlphaBlockDXT3::flip4()
{
	swap(row[0], row[3]);
	swap(row[1], row[2]);
}

/// Flip half DXT3 alpha block vertically.
void AlphaBlockDXT3::flip2()
{
	swap(row[0], row[1]);
}

/// Flip DXT3 block vertically.
void BlockDXT3::flip4()
{
	alpha.flip4();
	color.flip4();
}

/// Flip half DXT3 block vertically.
void BlockDXT3::flip2()
{
	alpha.flip2();
	color.flip2();
}


/*----------------------------------------------------------------------------
	BlockDXT5
----------------------------------------------------------------------------*/

void AlphaBlockDXT5::evaluatePalette(uint8 alpha[8]) const
{
	if (alpha0() > alpha1()) {
		evaluatePalette8(alpha);
	}
	else {
		evaluatePalette6(alpha);
	}
}

void AlphaBlockDXT5::evaluatePalette8(uint8 alpha[8]) const
{
	// 8-alpha block:  derive the other six alphas.
	// Bit code 000 = alpha0, 001 = alpha1, others are interpolated.
	alpha[0] = alpha0();
	alpha[1] = alpha1();
	alpha[2] = (6 * alpha[0] + 1 * alpha[1]) / 7;	// bit code 010
	alpha[3] = (5 * alpha[0] + 2 * alpha[1]) / 7;	// bit code 011
	alpha[4] = (4 * alpha[0] + 3 * alpha[1]) / 7;	// bit code 100
	alpha[5] = (3 * alpha[0] + 4 * alpha[1]) / 7;	// bit code 101
	alpha[6] = (2 * alpha[0] + 5 * alpha[1]) / 7;	// bit code 110
	alpha[7] = (1 * alpha[0] + 6 * alpha[1]) / 7;	// bit code 111
}

void AlphaBlockDXT5::evaluatePalette6(uint8 alpha[8]) const
{
	// 6-alpha block.
	// Bit code 000 = alpha0, 001 = alpha1, others are interpolated.
	alpha[0] = alpha0();
	alpha[1] = alpha1();
	alpha[2] = (4 * alpha[0] + 1 * alpha[1]) / 5;	// Bit code 010
	alpha[3] = (3 * alpha[0] + 2 * alpha[1]) / 5;	// Bit code 011
	alpha[4] = (2 * alpha[0] + 3 * alpha[1]) / 5;	// Bit code 100
	alpha[5] = (1 * alpha[0] + 4 * alpha[1]) / 5;	// Bit code 101
	alpha[6] = 0x00;							// Bit code 110
	alpha[7] = 0xFF;							// Bit code 111
}

void AlphaBlockDXT5::indices(uint8 index_array[16]) const
{
	index_array[0x0] = bits0();
	index_array[0x1] = bits1();
	index_array[0x2] = bits2();
	index_array[0x3] = bits3();
	index_array[0x4] = bits4();
	index_array[0x5] = bits5();
	index_array[0x6] = bits6();
	index_array[0x7] = bits7();
	index_array[0x8] = bits8();
	index_array[0x9] = bits9();
	index_array[0xA] = bitsA();
	index_array[0xB] = bitsB();
	index_array[0xC] = bitsC();
	index_array[0xD] = bitsD();
	index_array[0xE] = bitsE();
	index_array[0xF] = bitsF();
}

uint AlphaBlockDXT5::index(uint index) const
{
	int offset = (3 * index + 16);
	return uint((this->u >> offset) & 0x7);
}

void AlphaBlockDXT5::setIndex(uint index, uint value)
{
	int offset = (3 * index + 16);
	uint64 mask = uint64(0x7) << offset;
	this->u = (this->u & ~mask) | (uint64(value) << offset);
}

void AlphaBlockDXT5::decodeBlock(ColorBlock *block) const
{
	uint8 alpha_array[8];
	evaluatePalette(alpha_array);

	uint8 index_array[16];
	indices(index_array);

	for (uint i = 0; i < 16; i++) {
		block->color(i).a = alpha_array[index_array[i]];
	}
}

void AlphaBlockDXT5::flip4()
{
	uint64 *b = (uint64 *)this;

	// @@ The masks might have to be byte swapped.
	uint64 tmp = (*b & (uint64)(0x000000000000FFFFLL));
	tmp |= (*b & (uint64)(0x000000000FFF0000LL)) << 36;
	tmp |= (*b & (uint64)(0x000000FFF0000000LL)) << 12;
	tmp |= (*b & (uint64)(0x000FFF0000000000LL)) >> 12;
	tmp |= (*b & (uint64)(0xFFF0000000000000LL)) >> 36;

	*b = tmp;
}

void AlphaBlockDXT5::flip2()
{
	uint *b = (uint *)this;

	// @@ The masks might have to be byte swapped.
	uint tmp = (*b & 0xFF000000);
	tmp |=  (*b & 0x00000FFF) << 12;
	tmp |= (*b & 0x00FFF000) >> 12;

	*b = tmp;
}

void BlockDXT5::decodeBlock(ColorBlock *block) const
{
	// Decode color.
	color.decodeBlock(block);

	// Decode alpha.
	alpha.decodeBlock(block);
}

void BlockDXT5::decodeBlockNV5x(ColorBlock *block) const
{
	// Decode color.
	color.decodeBlockNV5x(block);

	// Decode alpha.
	alpha.decodeBlock(block);
}

/// Flip DXT5 block vertically.
void BlockDXT5::flip4()
{
	alpha.flip4();
	color.flip4();
}

/// Flip half DXT5 block vertically.
void BlockDXT5::flip2()
{
	alpha.flip2();
	color.flip2();
}


/// Decode ATI1 block.
void BlockATI1::decodeBlock(ColorBlock *block) const
{
	uint8 alpha_array[8];
	alpha.evaluatePalette(alpha_array);

	uint8 index_array[16];
	alpha.indices(index_array);

	for (uint i = 0; i < 16; i++) {
		Color32 & c = block->color(i);
		c.b = c.g = c.r = alpha_array[index_array[i]];
		c.a = 255;
	}
}

/// Flip ATI1 block vertically.
void BlockATI1::flip4()
{
	alpha.flip4();
}

/// Flip half ATI1 block vertically.
void BlockATI1::flip2()
{
	alpha.flip2();
}


/// Decode ATI2 block.
void BlockATI2::decodeBlock(ColorBlock *block) const
{
	uint8 alpha_array[8];
	uint8 index_array[16];

	x.evaluatePalette(alpha_array);
	x.indices(index_array);

	for (uint i = 0; i < 16; i++) {
		Color32 & c = block->color(i);
		c.r = alpha_array[index_array[i]];
	}

	y.evaluatePalette(alpha_array);
	y.indices(index_array);

	for (uint i = 0; i < 16; i++) {
		Color32 & c = block->color(i);
		c.g = alpha_array[index_array[i]];
		c.b = 0;
		c.a = 255;
	}
}

/// Flip ATI2 block vertically.
void BlockATI2::flip4()
{
	x.flip4();
	y.flip4();
}

/// Flip half ATI2 block vertically.
void BlockATI2::flip2()
{
	x.flip2();
	y.flip2();
}


void BlockCTX1::evaluatePalette(Color32 color_array[4]) const
{
	// Does bit expansion before interpolation.
	color_array[0].b = 0x00;
	color_array[0].g = col0[1];
	color_array[0].r = col0[0];
	color_array[0].a = 0xFF;

	color_array[1].r = 0x00;
	color_array[1].g = col0[1];
	color_array[1].b = col1[0];
	color_array[1].a = 0xFF;

	color_array[2].r = 0x00;
	color_array[2].g = (2 * color_array[0].g + color_array[1].g) / 3;
	color_array[2].b = (2 * color_array[0].b + color_array[1].b) / 3;
	color_array[2].a = 0xFF;

	color_array[3].r = 0x00;
	color_array[3].g = (2 * color_array[1].g + color_array[0].g) / 3;
	color_array[3].b = (2 * color_array[1].b + color_array[0].b) / 3;
	color_array[3].a = 0xFF;
}

void BlockCTX1::decodeBlock(ColorBlock *block) const
{
	// Decode color block.
	Color32 color_array[4];
	evaluatePalette(color_array);

	// Write color block.
	for ( uint j = 0; j < 4; j++ ) {
		for ( uint i = 0; i < 4; i++ ) {
			uint idx = (row[j] >> (2 * i)) & 3;
			block->color(i, j) = color_array[idx];
		}
	}
}

void BlockCTX1::setIndices(int *idx)
{
	indices = 0;
	for (uint i = 0; i < 16; i++) {
		indices |= (idx[i] & 3) << (2 * i);
	}
}


/// Flip CTX1 block vertically.
inline void BlockCTX1::flip4()
{
	swap(row[0], row[3]);
	swap(row[1], row[2]);
}

/// Flip half CTX1 block vertically.
inline void BlockCTX1::flip2()
{
	swap(row[0], row[1]);
}

void mem_read(Stream & mem, BlockDXT1 & block)
{
	mem_read(mem, block.col0.u);
	mem_read(mem, block.col1.u);
	mem_read(mem, block.indices);
}

void mem_read(Stream & mem, AlphaBlockDXT3 & block)
{
	for (unsigned int i = 0; i < 4; i++) mem_read(mem, block.row[i]);
}

void mem_read(Stream & mem, BlockDXT3 & block)
{
	mem_read(mem, block.alpha);
	mem_read(mem, block.color);
}

void mem_read(Stream & mem, AlphaBlockDXT5 & block)
{
	mem_read(mem, block.u);
}

void mem_read(Stream & mem, BlockDXT5 & block)
{
	mem_read(mem, block.alpha);
	mem_read(mem, block.color);
}

void mem_read(Stream & mem, BlockATI1 & block)
{
	mem_read(mem, block.alpha);
}

void mem_read(Stream & mem, BlockATI2 & block)
{
	mem_read(mem, block.x);
	mem_read(mem, block.y);
}

void mem_read(Stream & mem, BlockCTX1 & block)
{
	mem_read(mem, block.col0[0]);
	mem_read(mem, block.col0[1]);
	mem_read(mem, block.col1[0]);
	mem_read(mem, block.col1[1]);
	mem_read(mem, block.indices);
}
