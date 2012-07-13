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
	MovieClip *m_movieClip;
	MovieClipUser *m_movieClipUser;
	ImBuf *m_movieClipBuffer;
	int m_movieClipheight;
	int m_movieClipwidth;
	int m_framenumber;
	bool m_cacheFrame;
	
	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

public:
	MovieClipOperation();
	
	void initExecution();
	void deinitExecution();
	void setMovieClip(MovieClip *image) { this->m_movieClip = image; }
	void setMovieClipUser(MovieClipUser *imageuser) { this->m_movieClipUser = imageuser; }
	void setCacheFrame(bool value) { this->m_cacheFrame = value; }

	void setFramenumber(int framenumber) { this->m_framenumber = framenumber; }
	void executePixel(float *color, float x, float y, PixelSampler sampler);
};

#endif
