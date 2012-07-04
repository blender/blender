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

#include "COM_SplitViewerOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"
#include "BLI_utildefines.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

extern "C" {
	#include "MEM_guardedalloc.h"
	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}


SplitViewerOperation::SplitViewerOperation() : ViewerBaseOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->m_image1Input = NULL;
	this->m_image2Input = NULL;
}

void SplitViewerOperation::initExecution()
{
	// When initializing the tree during initial load the width and height can be zero.
	this->m_image1Input = getInputSocketReader(0);
	this->m_image2Input = getInputSocketReader(1);
	ViewerBaseOperation::initExecution();
}

void SplitViewerOperation::deinitExecution()
{
	this->m_image1Input = NULL;
	this->m_image2Input = NULL;
	ViewerBaseOperation::deinitExecution();
}


void SplitViewerOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer **memoryBuffers)
{
	float *buffer = this->m_outputBuffer;
	unsigned char *bufferDisplay = this->m_outputBufferDisplay;
	
	if (!buffer) return;
	int x1 = rect->xmin;
	int y1 = rect->ymin;
	int x2 = rect->xmax;
	int y2 = rect->ymax;
	int offset = (y1 * this->getWidth() + x1) * 4;
	int x;
	int y;
	int perc = this->m_xSplit ? this->m_splitPercentage * this->getWidth() / 100.0f : this->m_splitPercentage * this->getHeight() / 100.0f;
	for (y = y1; y < y2; y++) {
		for (x = x1; x < x2; x++) {
			bool image1;
			float srgb[4];
			image1 = this->m_xSplit ? x > perc : y > perc;
			if (image1) {
				this->m_image1Input->read(&(buffer[offset]), x, y, COM_PS_NEAREST, memoryBuffers);
			}
			else {
				this->m_image2Input->read(&(buffer[offset]), x, y, COM_PS_NEAREST, memoryBuffers);
			}
			/// @todo: linear conversion only when scene color management is selected, also check predivide.
			if (this->m_doColorManagement) {
				if (this->m_doColorPredivide) {
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
		offset += (this->getWidth() - (x2 - x1)) * 4;
	}
	updateImage(rect);
}

