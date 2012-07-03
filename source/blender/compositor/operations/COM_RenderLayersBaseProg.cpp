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

#include "COM_RenderLayersBaseProg.h"

#include "BLI_listbase.h"
#include "DNA_scene_types.h"

extern "C" {
	#include "RE_pipeline.h"
	#include "RE_shader_ext.h"
	#include "RE_render_ext.h"
}

RenderLayersBaseProg::RenderLayersBaseProg(int renderpass, int elementsize) : NodeOperation()
{
	this->m_renderpass = renderpass;
	this->setScene(NULL);
	this->m_inputBuffer = NULL;
	this->m_elementsize = elementsize;
}


void RenderLayersBaseProg::initExecution()
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
			if (rl && rl->rectf) {
				this->m_inputBuffer = RE_RenderLayerGetPass(rl, this->m_renderpass);

				if (this->m_inputBuffer == NULL || this->m_renderpass == SCE_PASS_COMBINED) {
					this->m_inputBuffer = rl->rectf;
				}
			}
		}
	}
	if (re) {
		RE_ReleaseResult(re);
		re = NULL;
	}
}

void RenderLayersBaseProg::executePixel(float *output, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	int ix = x;
	int iy = y;
	
	if (this->m_inputBuffer == NULL || ix < 0 || iy < 0 || ix >= (int)this->getWidth() || iy >= (int)this->getHeight() ) {
		zero_v4(output);
	}
	else {
		unsigned int offset = (iy * this->getWidth() + ix) * this->m_elementsize;
		if (this->m_elementsize == 1) {
			output[0] = this->m_inputBuffer[offset];
			output[1] = 0.0f;
			output[2] = 0.0f;
			output[3] = 0.0f;
		}
		else if (this->m_elementsize == 3) {
			copy_v3_v3(output, &this->m_inputBuffer[offset]);
			output[3] = 1.0f;
		}
		else {
			copy_v4_v4(output, &this->m_inputBuffer[offset]);
		}
	}
}

void RenderLayersBaseProg::deinitExecution()
{
	this->m_inputBuffer = NULL;
}

void RenderLayersBaseProg::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
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
			if (rl && rl->rectf) {
				resolution[0] = rl->rectx;
				resolution[1] = rl->recty;
			}
		}
	}
	
	if (re)
		RE_ReleaseResult(re);

}

