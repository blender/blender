/*
 * Copyright 2012, Blender Foundation.
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
 *		Sergey Sharybin
 */


#ifndef _COM_MaskOperation_h
#define _COM_MaskOperation_h

/* XXX, remove when the USE_RASKTER option is also removed */
extern "C" {
	#include "BKE_mask.h"
}

#include "COM_NodeOperation.h"
#include "DNA_scene_types.h"
#include "DNA_mask_types.h"
#include "BLI_listbase.h"
#include "IMB_imbuf_types.h"

#ifdef __PLX_RASKTER_MT__
#include "../../../../intern/raskter/raskter.h"
#endif

/**
 * Class with implementation of mask rasterization
 */
class MaskOperation : public NodeOperation {
protected:
	Mask *m_mask;

	/* note, these are used more like aspect,
	 * but they _do_ impact on mask detail */
	int   m_maskWidth;
	int   m_maskHeight;
	float m_maskWidthInv;  /* 1 / m_maskWidth  */
	float m_maskHeightInv; /* 1 / m_maskHeight */

	int m_framenumber;
	bool m_do_smooth;
	bool m_do_feather;

#ifdef USE_RASKTER
	float *m_rasterizedMask;
	
	ListBase m_maskLayers;

#else /* USE_RASKTER */
	struct MaskRasterHandle *m_rasterMaskHandle;
#endif /* USE_RASKTER */

	/**
	 * Determine the output resolution. The resolution is retrieved from the Renderer
	 */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

public:
	MaskOperation();

	void initExecution();
	void deinitExecution();


	void setMask(Mask *mask) { this->m_mask = mask; }
	void setMaskWidth(int width)
	{
		this->m_maskWidth    = width;
		this->m_maskWidthInv = 1.0f / (float)width;
	}
	void setMaskHeight(int height)
	{
		this->m_maskHeight = height;
		this->m_maskHeightInv = 1.0f / (float)height;
	}
	void setFramenumber(int framenumber) { this->m_framenumber = framenumber; }
	void setSmooth(bool smooth) { this->m_do_smooth = smooth; }
	void setFeather(bool feather) { this->m_do_feather = feather; }

#ifdef USE_RASKTER
	void *initializeTileData(rcti *rect);
	void executePixel(float *color, int x, int y, void *data);
#else /* USE_RASKTER */
	void executePixel(float *color, float x, float y, PixelSampler sampler);
#endif /* USE_RASKTER */
};

#endif
