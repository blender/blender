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

#include "COM_MaskOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_scene_types.h"

#ifdef USE_RASKTER

extern "C" {
	#include "../../../../intern/raskter/raskter.h"
}

MaskOperation::MaskOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
	this->m_mask = NULL;
	this->m_maskWidth = 0;
	this->m_maskHeight = 0;
	this->m_framenumber = 0;
	this->m_rasterizedMask = NULL;
	setComplex(true);
}

void MaskOperation::initExecution()
{
	initMutex();

	this->m_rasterizedMask = NULL;
	this->m_maskLayers.first = this->m_maskLayers.last = NULL;

	if (this->m_mask) {
		BKE_mask_layer_copy_list(&this->m_maskLayers, &this->m_mask->masklayers);
	}
}

void MaskOperation::deinitExecution()
{
	BKE_mask_layer_free_list(&this->m_maskLayers);

	if (this->m_rasterizedMask) {
		MEM_freeN(this->m_rasterizedMask);
		this->m_rasterizedMask = NULL;
	}
}

void *MaskOperation::initializeTileData(rcti *rect)
{
	if (this->m_rasterizedMask)
		return this->m_rasterizedMask;

	if (!this->m_mask)
		return NULL;

	lockMutex();
	if (this->m_rasterizedMask == NULL) {
		int width = this->getWidth();
		int height = this->getHeight();
		float *buffer;

		buffer = (float *)MEM_callocN(sizeof(float) * width * height, "rasterized mask");

		BKE_mask_rasterize_layers(&this->m_maskLayers, width, height, buffer, TRUE,
		                          this->m_do_smooth, this->m_do_feather);

		if (this->m_do_smooth) {
			PLX_antialias_buffer(buffer, width, height);
		}

		this->m_rasterizedMask = buffer;
	}
	unlockMutex();
	return this->m_rasterizedMask;
}

void MaskOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	if (this->m_maskWidth == 0 || this->m_maskHeight == 0) {
		NodeOperation::determineResolution(resolution, preferredResolution);
	}
	else {
		unsigned int nr[2];

		nr[0] = this->m_maskWidth;
		nr[1] = this->m_maskHeight;

		NodeOperation::determineResolution(resolution, nr);

		resolution[0] = this->m_maskWidth;
		resolution[1] = this->m_maskHeight;
	}
}

void MaskOperation::executePixel(float *color, int x, int y, void *data)
{
	if (!data) {
		color[0] = 0.0f;
	}
	else {
		float *buffer = (float *) data;
		int index = (y * this->getWidth() + x);

		color[0] = buffer[index];
	}
}

#else /* mask rasterizer by campbell wip */

MaskOperation::MaskOperation() : NodeOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
	this->m_mask = NULL;
	this->m_maskWidth = 0;
	this->m_maskHeight = 0;
	this->m_maskWidthInv = 0.0f;
	this->m_maskHeightInv = 0.0f;
	this->m_framenumber = 0;
	this->m_rasterMaskHandleTot = 1;
	memset(this->m_rasterMaskHandles, 0, sizeof(this->m_rasterMaskHandles));
}

void MaskOperation::initExecution()
{
	if (this->m_mask && this->m_rasterMaskHandles[0] == NULL) {
		if (this->m_rasterMaskHandleTot == 1) {
			this->m_rasterMaskHandles[0] = BKE_maskrasterize_handle_new();

			BKE_maskrasterize_handle_init(this->m_rasterMaskHandles[0], this->m_mask,
			        this->m_maskWidth, this->m_maskHeight,
			        TRUE, this->m_do_smooth, this->m_do_feather);
		}
		else {
			/* make a throw away copy of the mask */
			const float frame_range = 1.0f; /* should be 1 max, could be configurable */
			const float frame = (float)this->m_framenumber - frame_range;
			const float frame_step = (frame_range * 2.0f) / this->m_rasterMaskHandleTot;
			float frame_iter = frame;

			Mask *mask_temp;

			mask_temp = BKE_mask_copy_nolib(this->m_mask);

			/* trick so we can get unkeyed edits to display */
			{
				MaskLayer *masklay;
				MaskLayerShape *masklay_shape;

				for (masklay = (MaskLayer *)mask_temp->masklayers.first;
				     masklay;
				     masklay = (MaskLayer *)masklay->next)
				{
					masklay_shape = BKE_mask_layer_shape_varify_frame(masklay, this->m_framenumber);
					BKE_mask_layer_shape_from_mask(masklay, masklay_shape);
				}
			}

			for (unsigned int i = 0; i < this->m_rasterMaskHandleTot; i++) {
				this->m_rasterMaskHandles[i] = BKE_maskrasterize_handle_new();

				/* re-eval frame info */
				BKE_mask_evaluate(mask_temp, frame_iter, TRUE);

				BKE_maskrasterize_handle_init(this->m_rasterMaskHandles[i], mask_temp,
				                              this->m_maskWidth, this->m_maskHeight,
				                              TRUE, this->m_do_smooth, this->m_do_feather);

				frame_iter += frame_step;
			}

			BKE_mask_free(mask_temp);
			MEM_freeN(mask_temp);
		}
	}
}

void MaskOperation::deinitExecution()
{
	for (unsigned int i = 0; i < this->m_rasterMaskHandleTot; i++) {
		if (this->m_rasterMaskHandles[i]) {
			BKE_maskrasterize_handle_free(this->m_rasterMaskHandles[i]);
			this->m_rasterMaskHandles[i] = NULL;
		}
	}
}

void MaskOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	if (this->m_maskWidth == 0 || this->m_maskHeight == 0) {
		NodeOperation::determineResolution(resolution, preferredResolution);
	}
	else {
		unsigned int nr[2];

		nr[0] = this->m_maskWidth;
		nr[1] = this->m_maskHeight;

		NodeOperation::determineResolution(resolution, nr);

		resolution[0] = this->m_maskWidth;
		resolution[1] = this->m_maskHeight;
	}
}

void MaskOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	const float xy[2] = {x * this->m_maskWidthInv, y * this->m_maskHeightInv};

	if (this->m_rasterMaskHandleTot == 1) {
		if (this->m_rasterMaskHandles[0]) {
			color[0] = BKE_maskrasterize_handle_sample(this->m_rasterMaskHandles[0], xy);
		}
		else {
			color[0] = 0.0f;
		}
	}
	else {
		/* incase loop below fails */
		color[0] = 0.0f;

		for (unsigned int i = 0; i < this->m_rasterMaskHandleTot; i++) {
			if (this->m_rasterMaskHandles[i]) {
				color[0] += BKE_maskrasterize_handle_sample(this->m_rasterMaskHandles[i], xy);
			}
		}

		/* until we get better falloff */
		color[0] /= this->m_rasterMaskHandleTot;
	}
}

#endif /* USE_RASKTER */
