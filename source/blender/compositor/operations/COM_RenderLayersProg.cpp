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

#include "COM_RenderLayersProg.h"

#include "BLI_listbase.h"
#include "BKE_scene.h"
#include "DNA_scene_types.h"

extern "C" {
#  include "RE_pipeline.h"
#  include "RE_shader_ext.h"
#  include "RE_render_ext.h"
}

/* ******** Render Layers Base Prog ******** */

RenderLayersProg::RenderLayersProg(const char *passName, DataType type, int elementsize) : NodeOperation(), m_passName(passName)
{
	this->setScene(NULL);
	this->m_inputBuffer = NULL;
	this->m_elementsize = elementsize;
	this->m_rd = NULL;

	this->addOutputSocket(type);
}


void RenderLayersProg::initExecution()
{
	Scene *scene = this->getScene();
	Render *re = (scene) ? RE_GetRender(scene->id.name) : NULL;
	RenderResult *rr = NULL;
	
	if (re)
		rr = RE_AcquireResultRead(re);
	
	if (rr) {
		SceneRenderLayer *srl = (SceneRenderLayer *)BLI_findlink(&scene->r.layers, getLayerId());
		if (srl) {

			RenderLayer *rl = RE_GetRenderLayer(rr, srl->name);
			if (rl) {
				this->m_inputBuffer = RE_RenderLayerGetPass(rl, this->m_passName.c_str(), this->m_viewName);
			}
		}
	}
	if (re) {
		RE_ReleaseResult(re);
		re = NULL;
	}
}

void RenderLayersProg::doInterpolation(float output[4], float x, float y, PixelSampler sampler)
{
	unsigned int offset;
	int width = this->getWidth(), height = this->getHeight();

	int ix = x, iy = y;
	if (ix < 0 || iy < 0 || ix >= width || iy >= height) {
		if (this->m_elementsize == 1)
			output[0] = 0.0f;
		else if (this->m_elementsize == 3)
			zero_v3(output);
		else
			zero_v4(output);
		return;
	}

	switch (sampler) {
		case COM_PS_NEAREST: {
			offset = (iy * width + ix) * this->m_elementsize;

			if (this->m_elementsize == 1)
				output[0] = this->m_inputBuffer[offset];
			else if (this->m_elementsize == 3)
				copy_v3_v3(output, &this->m_inputBuffer[offset]);
			else
				copy_v4_v4(output, &this->m_inputBuffer[offset]);
			break;
		}

		case COM_PS_BILINEAR:
			BLI_bilinear_interpolation_fl(this->m_inputBuffer, output, width, height, this->m_elementsize, x, y);
			break;

		case COM_PS_BICUBIC:
			BLI_bicubic_interpolation_fl(this->m_inputBuffer, output, width, height, this->m_elementsize, x, y);
			break;
	}
}

void RenderLayersProg::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
#if 0
	const RenderData *rd = this->m_rd;

	int dx = 0, dy = 0;

	if (rd->mode & R_BORDER && rd->mode & R_CROP) {
		/* see comment in executeRegion describing coordinate mapping,
		 * here it simply goes other way around
		 */
		int full_width  = rd->xsch * rd->size / 100;
		int full_height = rd->ysch * rd->size / 100;

		dx = rd->border.xmin * full_width - (full_width - this->getWidth()) / 2.0f;
		dy = rd->border.ymin * full_height - (full_height - this->getHeight()) / 2.0f;
	}

	int ix = x - dx;
	int iy = y - dy;
#endif

#ifndef NDEBUG
	{
		const DataType data_type = this->getOutputSocket()->getDataType();
		int actual_element_size = this->m_elementsize;
		int expected_element_size;
		if (data_type == COM_DT_VALUE) {
			expected_element_size = 1;
		}
		else if (data_type == COM_DT_VECTOR) {
			expected_element_size = 3;
		}
		else if (data_type == COM_DT_COLOR) {
			expected_element_size = 4;
		}
		else {
			expected_element_size = 0;
			BLI_assert(!"Something horribly wrong just happened");
		}
		BLI_assert(expected_element_size == actual_element_size);
	}
#endif

	if (this->m_inputBuffer == NULL) {
		int elemsize = this->m_elementsize;
		if (elemsize == 1) {
			output[0] = 0.0f;
		}
		else if (elemsize == 3) {
			zero_v3(output);
		}
		else {
			BLI_assert(elemsize == 4);
			zero_v4(output);
		}
	}
	else {
		doInterpolation(output, x, y, sampler);
	}
}

void RenderLayersProg::deinitExecution()
{
	this->m_inputBuffer = NULL;
}

void RenderLayersProg::determineResolution(unsigned int resolution[2], unsigned int /*preferredResolution*/[2])
{
	Scene *sce = this->getScene();
	Render *re = (sce) ? RE_GetRender(sce->id.name) : NULL;
	RenderResult *rr = NULL;
	
	resolution[0] = 0;
	resolution[1] = 0;
	
	if (re)
		rr = RE_AcquireResultRead(re);
	
	if (rr) {
		SceneRenderLayer *srl   = (SceneRenderLayer *)BLI_findlink(&sce->r.layers, getLayerId());
		if (srl) {
			RenderLayer *rl = RE_GetRenderLayer(rr, srl->name);
			if (rl) {
				resolution[0] = rl->rectx;
				resolution[1] = rl->recty;
			}
		}
	}
	
	if (re)
		RE_ReleaseResult(re);

}

/* ******** Render Layers AO Operation ******** */
void RenderLayersAOOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float *inputBuffer = this->getInputBuffer();
	if (inputBuffer == NULL) {
		zero_v3(output);
	}
	else {
		doInterpolation(output, x, y, sampler);
	}
	output[3] = 1.0f;
}

/* ******** Render Layers Alpha Operation ******** */
void RenderLayersAlphaProg::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	float *inputBuffer = this->getInputBuffer();

	if (inputBuffer == NULL) {
		output[0] = 0.0f;
	}
	else {
		float temp[4];
		doInterpolation(temp, x, y, sampler);
		output[0] = temp[3];
	}
}

/* ******** Render Layers Depth Operation ******** */
void RenderLayersDepthProg::executePixelSampled(float output[4], float x, float y, PixelSampler /*sampler*/)
{
	int ix = x;
	int iy = y;
	float *inputBuffer = this->getInputBuffer();

	if (inputBuffer == NULL || ix < 0 || iy < 0 || ix >= (int)this->getWidth() || iy >= (int)this->getHeight() ) {
		output[0] = 10e10f;
	}
	else {
		unsigned int offset = (iy * this->getWidth() + ix);
		output[0] = inputBuffer[offset];
	}
}
