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

#include "COM_CompositorOperation.h"
#include "BLI_listbase.h"
#include "BKE_image.h"

extern "C" {
#  include "BLI_threads.h"
#  include "RE_pipeline.h"
#  include "RE_shader_ext.h"
#  include "RE_render_ext.h"
#  include "MEM_guardedalloc.h"
#  include "render_types.h"
}
#include "PIL_time.h"


CompositorOperation::CompositorOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_VALUE);

	this->setRenderData(NULL);
	this->m_outputBuffer = NULL;
	this->m_depthBuffer = NULL;
	this->m_imageInput = NULL;
	this->m_alphaInput = NULL;
	this->m_depthInput = NULL;

	this->m_useAlphaInput = false;
	this->m_active = false;

	this->m_sceneName[0] = '\0';
}

void CompositorOperation::initExecution()
{
	if (!this->m_active)
		return;

	// When initializing the tree during initial load the width and height can be zero.
	this->m_imageInput = getInputSocketReader(0);
	this->m_alphaInput = getInputSocketReader(1);
	this->m_depthInput = getInputSocketReader(2);
	if (this->getWidth() * this->getHeight() != 0) {
		this->m_outputBuffer = (float *) MEM_callocN(this->getWidth() * this->getHeight() * 4 * sizeof(float), "CompositorOperation");
	}
	if (this->m_depthInput != NULL) {
		this->m_depthBuffer = (float *) MEM_callocN(this->getWidth() * this->getHeight() * sizeof(float), "CompositorOperation");
	}
}

void CompositorOperation::deinitExecution()
{
	if (!this->m_active)
		return;

	if (!isBreaked()) {
		Render *re = RE_GetRender(this->m_sceneName);
		RenderResult *rr = RE_AcquireResultWrite(re);

		if (rr) {
			if (rr->rectf != NULL) {
				MEM_freeN(rr->rectf);
			}
			rr->rectf = this->m_outputBuffer;
			if (rr->rectz != NULL) {
				MEM_freeN(rr->rectz);
			}
			rr->rectz = this->m_depthBuffer;
		}
		else {
			if (this->m_outputBuffer) {
				MEM_freeN(this->m_outputBuffer);
			}
			if (this->m_depthBuffer) {
				MEM_freeN(this->m_depthBuffer);
			}
		}

		if (re) {
			RE_ReleaseResult(re);
			re = NULL;
		}

		BLI_lock_thread(LOCK_DRAW_IMAGE);
		BKE_image_signal(BKE_image_verify_viewer(IMA_TYPE_R_RESULT, "Render Result"), NULL, IMA_SIGNAL_FREE);
		BLI_unlock_thread(LOCK_DRAW_IMAGE);
	}
	else {
		if (this->m_outputBuffer) {
			MEM_freeN(this->m_outputBuffer);
		}
		if (this->m_depthBuffer) {
			MEM_freeN(this->m_depthBuffer);
		}
	}

	this->m_outputBuffer = NULL;
	this->m_depthBuffer = NULL;
	this->m_imageInput = NULL;
	this->m_alphaInput = NULL;
	this->m_depthInput = NULL;
}


void CompositorOperation::executeRegion(rcti *rect, unsigned int tileNumber)
{
	float color[8]; // 7 is enough
	float *buffer = this->m_outputBuffer;
	float *zbuffer = this->m_depthBuffer;

	if (!buffer) return;
	int x1 = rect->xmin;
	int y1 = rect->ymin;
	int x2 = rect->xmax;
	int y2 = rect->ymax;
	int offset = (y1 * this->getWidth() + x1);
	int add = (this->getWidth() - (x2 - x1));
	int offset4 = offset * COM_NUMBER_OF_CHANNELS;
	int x;
	int y;
	bool breaked = false;
	int dx = 0, dy = 0;

#if 0
	const RenderData *rd = this->m_rd;

	if (rd->mode & R_BORDER && rd->mode & R_CROP) {
	/*!
	   When using cropped render result, need to re-position area of interest,
	   so it'll natch bounds of render border within frame. By default, canvas
	   will be centered between full frame and cropped frame, so we use such
	   scheme to map cropped coordinates to full-frame coordinates

		   ^ Y
		   |                      Width
		   +------------------------------------------------+
		   |                                                |
		   |                                                |
		   |  Centered canvas, we map coordinate from it    |
		   |              +------------------+              |
		   |              |                  |              |  H
		   |              |                  |              |  e
		   |  +------------------+ . Center  |              |  i
		   |  |           |      |           |              |  g
		   |  |           |      |           |              |  h
		   |  |....dx.... +------|-----------+              |  t
		   |  |           . dy   |                          |
		   |  +------------------+                          |
		   |  Render border, we map coordinates to it       |
		   |                                                |    X
		   +------------------------------------------------+---->
		                        Full frame
		 */

		int full_width  = rd->xsch * rd->size / 100;
		int full_height = rd->ysch * rd->size / 100;

		dx = rd->border.xmin * full_width - (full_width - this->getWidth()) / 2.0f;
		dy = rd->border.ymin * full_height - (full_height - this->getHeight()) / 2.0f;
	}
#endif

	for (y = y1; y < y2 && (!breaked); y++) {
		for (x = x1; x < x2 && (!breaked); x++) {
			int input_x = x + dx, input_y = y + dy;

			this->m_imageInput->readSampled(color, input_x, input_y, COM_PS_NEAREST);
			if (this->m_useAlphaInput) {
				this->m_alphaInput->readSampled(&(color[3]), input_x, input_y, COM_PS_NEAREST);
			}

			copy_v4_v4(buffer + offset4, color);

			this->m_depthInput->readSampled(color, input_x, input_y, COM_PS_NEAREST);
			zbuffer[offset] = color[0];
			offset4 += COM_NUMBER_OF_CHANNELS;
			offset++;
			if (isBreaked()) {
				breaked = true;
			}
		}
		offset += add;
		offset4 += add * COM_NUMBER_OF_CHANNELS;
	}
}

void CompositorOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	int width = this->m_rd->xsch * this->m_rd->size / 100;
	int height = this->m_rd->ysch * this->m_rd->size / 100;

	// check actual render resolution with cropping it may differ with cropped border.rendering
	// FIX for: [31777] Border Crop gives black (easy)
	Render *re = RE_GetRender(this->m_sceneName);
	if (re) {
		RenderResult *rr = RE_AcquireResultRead(re);
		if (rr) {
			width = rr->rectx;
			height = rr->recty;
		}
		RE_ReleaseResult(re);
	}

	preferredResolution[0] = width;
	preferredResolution[1] = height;

	NodeOperation::determineResolution(resolution, preferredResolution);

	resolution[0] = width;
	resolution[1] = height;
}
