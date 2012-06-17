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


#ifndef _COM_ImageOperation_h
#define _COM_ImageOperation_h

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "DNA_movieclip_types.h"
#include "BLI_listbase.h"
#include "IMB_imbuf_types.h"

/**
 * Base class for all renderlayeroperations
 *
 * @todo: rename to operation.
 */
class MovieClipOperation : public NodeOperation {
protected:
	MovieClip *movieClip;
	MovieClipUser *movieClipUser;
	ImBuf *movieClipBuffer;
	int movieClipheight;
	int movieClipwidth;
	int framenumber;
	
	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

public:
	MovieClipOperation();
	
	void initExecution();
	void deinitExecution();
	void setMovieClip(MovieClip *image) { this->movieClip = image; }
	void setMovieClipUser(MovieClipUser *imageuser) { this->movieClipUser = imageuser; }

	void setFramenumber(int framenumber) { this->framenumber = framenumber; }
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
};

#endif
