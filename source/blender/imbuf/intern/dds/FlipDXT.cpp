/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file comes from the chromium project, adapted to Blender to add DDS
// flipping to OpenGL convention for Blender

#include "IMB_imbuf_types.h"

#include <string.h>

#include <Common.h>
#include <Stream.h>
#include <ColorBlock.h>
#include <BlockDXT.h>
#include <FlipDXT.h>

// A function that flips a DXTC block.
typedef void (*FlipBlockFunction)(uint8_t *block);

// Flips a full DXT1 block in the y direction.
static void FlipDXT1BlockFull(uint8_t *block)
{
	// A DXT1 block layout is:
	// [0-1] color0.
	// [2-3] color1.
	// [4-7] color bitmap, 2 bits per pixel.
	// So each of the 4-7 bytes represents one line, flipping a block is just
	// flipping those bytes.
	uint8_t tmp = block[4];
	block[4] = block[7];
	block[7] = tmp;
	tmp = block[5];
	block[5] = block[6];
	block[6] = tmp;
}

// Flips the first 2 lines of a DXT1 block in the y direction.
static void FlipDXT1BlockHalf(uint8_t *block)
{
	// See layout above.
	uint8_t tmp = block[4];
	block[4] = block[5];
	block[5] = tmp;
}

// Flips a full DXT3 block in the y direction.
static void FlipDXT3BlockFull(uint8_t *block)
{
	// A DXT3 block layout is:
	// [0-7]	alpha bitmap, 4 bits per pixel.
	// [8-15] a DXT1 block.

	// We can flip the alpha bits at the byte level (2 bytes per line).
	uint8_t tmp = block[0];

	block[0] = block[6];
	block[6] = tmp;
	tmp = block[1];
	block[1] = block[7];
	block[7] = tmp;
	tmp = block[2];
	block[2] = block[4];
	block[4] = tmp;
	tmp = block[3];
	block[3] = block[5];
	block[5] = tmp;

	// And flip the DXT1 block using the above function.
	FlipDXT1BlockFull(block + 8);
}

// Flips the first 2 lines of a DXT3 block in the y direction.
static void FlipDXT3BlockHalf(uint8_t *block)
{
	// See layout above.
	uint8_t tmp = block[0];

	block[0] = block[2];
	block[2] = tmp;
	tmp = block[1];
	block[1] = block[3];
	block[3] = tmp;
	FlipDXT1BlockHalf(block + 8);
}

// Flips a full DXT5 block in the y direction.
static void FlipDXT5BlockFull(uint8_t *block)
{
	// A DXT5 block layout is:
	// [0]		alpha0.
	// [1]		alpha1.
	// [2-7]	alpha bitmap, 3 bits per pixel.
	// [8-15] a DXT1 block.

	// The alpha bitmap doesn't easily map lines to bytes, so we have to
	// interpret it correctly.	Extracted from
	// http://www.opengl.org/registry/specs/EXT/texture_compression_s3tc.txt :
	//
	//	 The 6 "bits" bytes of the block are decoded into one 48-bit integer:
	//
	//		 bits = bits_0 + 256 * (bits_1 + 256 * (bits_2 + 256 * (bits_3 +
	//									 256 * (bits_4 + 256 * bits_5))))
	//
	//	 bits is a 48-bit unsigned integer, from which a three-bit control code
	//	 is extracted for a texel at location (x,y) in the block using:
	//
	//			 code(x,y) = bits[3*(4*y+x)+1..3*(4*y+x)+0]
	//
	//	 where bit 47 is the most significant and bit 0 is the least
	//	 significant bit.
	unsigned int line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
	unsigned int line_2_3 = block[5] + 256 * (block[6] + 256 * block[7]);
	// swap lines 0 and 1 in line_0_1.
	unsigned int line_1_0 = ((line_0_1 & 0x000fff) << 12) |
	                        ((line_0_1 & 0xfff000) >> 12);
	// swap lines 2 and 3 in line_2_3.
	unsigned int line_3_2 = ((line_2_3 & 0x000fff) << 12) |
	                        ((line_2_3 & 0xfff000) >> 12);

	block[2] = line_3_2 & 0xff;
	block[3] = (line_3_2 & 0xff00) >> 8;
	block[4] = (line_3_2 & 0xff0000) >> 16;
	block[5] = line_1_0 & 0xff;
	block[6] = (line_1_0 & 0xff00) >> 8;
	block[7] = (line_1_0 & 0xff0000) >> 16;

	// And flip the DXT1 block using the above function.
	FlipDXT1BlockFull(block + 8);
}

// Flips the first 2 lines of a DXT5 block in the y direction.
static void FlipDXT5BlockHalf(uint8_t *block)
{
	// See layout above.
	unsigned int line_0_1 = block[2] + 256 * (block[3] + 256 * block[4]);
	unsigned int line_1_0 = ((line_0_1 & 0x000fff) << 12) |
	                        ((line_0_1 & 0xfff000) >> 12);
	block[2] = line_1_0 & 0xff;
	block[3] = (line_1_0 & 0xff00) >> 8;
	block[4] = (line_1_0 & 0xff0000) >> 16;
	FlipDXT1BlockHalf(block + 8);
}

// Flips a DXTC image, by flipping and swapping DXTC blocks as appropriate.
int FlipDXTCImage(unsigned int width, unsigned int height, unsigned int levels, int fourcc, uint8_t *data)
{
	// must have valid dimensions
	if (width == 0 || height == 0)
		return 0;
	// height must be a power-of-two
	if ((height & (height - 1)) != 0)
		return 0;

	FlipBlockFunction full_block_function;
	FlipBlockFunction half_block_function;
	unsigned int block_bytes = 0;

	switch (fourcc) {
		case FOURCC_DXT1:
			full_block_function = FlipDXT1BlockFull;
			half_block_function = FlipDXT1BlockHalf;
			block_bytes = 8;
			break;
		case FOURCC_DXT3:
			full_block_function = FlipDXT3BlockFull;
			half_block_function = FlipDXT3BlockHalf;
			block_bytes = 16;
			break;
		case FOURCC_DXT5:
			full_block_function = FlipDXT5BlockFull;
			half_block_function = FlipDXT5BlockHalf;
			block_bytes = 16;
			break;
		default:
			return 0;
	}

	unsigned int mip_width = width;
	unsigned int mip_height = height;

	for (unsigned int i = 0; i < levels; ++i) {
		unsigned int blocks_per_row = (mip_width + 3) / 4;
		unsigned int blocks_per_col = (mip_height + 3) / 4;
		unsigned int blocks = blocks_per_row * blocks_per_col;

		if (mip_height == 1) {
			// no flip to do, and we're done.
			break;
		}
		else if (mip_height == 2) {
			// flip the first 2 lines in each block.
			for (unsigned int i = 0; i < blocks_per_row; ++i) {
				half_block_function(data + i * block_bytes);
			}
		}
		else {
			// flip each block.
			for (unsigned int i = 0; i < blocks; ++i)
				full_block_function(data + i * block_bytes);

			// swap each block line in the first half of the image with the
			// corresponding one in the second half.
			// note that this is a no-op if mip_height is 4.
			unsigned int row_bytes = block_bytes * blocks_per_row;
			uint8_t *temp_line = new uint8_t[row_bytes];

			for (unsigned int y = 0; y < blocks_per_col / 2; ++y) {
				uint8_t *line1 = data + y * row_bytes;
				uint8_t *line2 = data + (blocks_per_col - y - 1) * row_bytes;

				memcpy(temp_line, line1, row_bytes);
				memcpy(line1, line2, row_bytes);
				memcpy(line2, temp_line, row_bytes);
			}

			delete[] temp_line;
		}

		// mip levels are contiguous.
		data += block_bytes * blocks;
		mip_width = max(1U, mip_width >> 1);
		mip_height = max(1U, mip_height >> 1);
	}

	return 1;
}

