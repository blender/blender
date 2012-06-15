/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_ViewerOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"
#include "WM_api.h"
#include "WM_types.h"
#include "PIL_time.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

extern "C" {
	#include "MEM_guardedalloc.h"
	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}


ViewerOperation::ViewerOperation() : ViewerBaseOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);

	this->imageInput = NULL;
	this->alphaInput = NULL;
}

void ViewerOperation::initExecution()
{
	// When initializing the tree during initial load the width and height can be zero.
	this->imageInput = getInputSocketReader(0);
	this->alphaInput = getInputSocketReader(1);
	ViewerBaseOperation::initExecution();
}

void ViewerOperation::deinitExecution()
{
	this->imageInput = NULL;
	this->alphaInput = NULL;
	ViewerBaseOperation::deinitExecution();
}


void ViewerOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer **memoryBuffers)
{
	float *buffer = this->outputBuffer;
	unsigned char *bufferDisplay = this->outputBufferDisplay;
	if (!buffer) return;
	const int x1 = rect->xmin;
	const int y1 = rect->ymin;
	const int x2 = rect->xmax;
	const int y2 = rect->ymax;
	const int offsetadd = (this->getWidth() - (x2 - x1)) * 4;
	int offset = (y1 * this->getWidth() + x1) * 4;
	float alpha[4], srgb[4];
	int x;
	int y;
	bool breaked = false;

	for (y = y1; y < y2 && (!breaked); y++) {
		for (x = x1; x < x2; x++) {
			imageInput->read(&(buffer[offset]), x, y, COM_PS_NEAREST, memoryBuffers);
			if (alphaInput != NULL) {
				alphaInput->read(alpha, x, y, COM_PS_NEAREST, memoryBuffers);
				buffer[offset + 3] = alpha[0];
			}
			/// @todo: linear conversion only when scene color management is selected, also check predivide.
			if (this->doColorManagement) {
				if (this->doColorPredivide) {
					linearrgb_to_srgb_predivide_v4(srgb, buffer + offset);
				}
				else {
					linearrgb_to_srgb_v4(srgb, buffer + offset);
				}
			}
			else {
				copy_v4_v4(srgb, buffer + offset);
			}

			rgba_float_to_uchar(bufferDisplay + offset, srgb);

			offset += 4;
		}
		if (isBreaked()) {
			breaked = true;
		}

		offset += offsetadd;
	}
	updateImage(rect);
}
