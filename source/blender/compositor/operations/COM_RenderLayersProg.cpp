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
#include "DNA_scene_types.h"

extern "C" {
#  include "RE_pipeline.h"
#  include "RE_shader_ext.h"
#  include "RE_render_ext.h"
}

/* ******** Render Layers Base Prog ******** */

RenderLayersBaseProg::RenderLayersBaseProg(int renderpass, int elementsize) : NodeOperation()
{
	this->m_renderpass = renderpass;
	this->setScene(NULL);
	this->m_inputBuffer = NULL;
	this->m_elementsize = elementsize;
	this->m_rd = NULL;
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

				if (this->m_inputBuffer == NULL && this->m_renderpass == SCE_PASS_COMBINED) {
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

void RenderLayersBaseProg::doInterpolation(float output[4], float x, float y, PixelSampler sampler)
{
	unsigned int offset;
	int width = this->getWidth(), height = this->getHeight();

	switch (sampler) {
		case COM_PS_NEAREST: {
			int ix = x;
			int iy = y;
			if (ix < 0 || iy < 0 || ix >= width || iy >= height) {
				if (this->m_elementsize == 1)
					output[0] = 0.0f;
				else if (this->m_elementsize == 3)
					zero_v3(output);
				else
					zero_v4(output);
				break;
				
			}

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
			BLI_bilinear_interpolation_fl(this->m_inputBuffer, output, width, height, this->m_elementsize, x - 0.5f, y - 0.5f);
			break;

		case COM_PS_BICUBIC:
			BLI_bicubic_interpolation_fl(this->m_inputBuffer, output, width, height, this->m_elementsize, x - 0.5f, y - 0.5f);
			break;
	}
}

void RenderLayersBaseProg::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
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

void RenderLayersBaseProg::deinitExecution()
{
	this->m_inputBuffer = NULL;
}

void RenderLayersBaseProg::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
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

/* ******** Render Layers AO Operation ******** */

RenderLayersAOOperation::RenderLayersAOOperation() : RenderLayersBaseProg(SCE_PASS_AO, 3)
{
	this->addOutputSocket(COM_DT_COLOR);
}

/* ******** Render Layers Alpha Operation ******** */

RenderLayersAlphaProg::RenderLayersAlphaProg() : RenderLayersBaseProg(SCE_PASS_COMBINED, 4)
{
	this->addOutputSocket(COM_DT_VALUE);
}

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

/* ******** Render Layers Color Operation ******** */

RenderLayersColorOperation::RenderLayersColorOperation() : RenderLayersBaseProg(SCE_PASS_RGBA, 4)
{
	this->addOutputSocket(COM_DT_COLOR);
}

/* ******** Render Layers Cycles Operation ******** */

RenderLayersCyclesOperation::RenderLayersCyclesOperation(int pass) : RenderLayersBaseProg(pass, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Depth Operation ******** */

RenderLayersDepthProg::RenderLayersDepthProg() : RenderLayersBaseProg(SCE_PASS_Z, 1)
{
	this->addOutputSocket(COM_DT_VALUE);
}

void RenderLayersDepthProg::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	int ix = x;
	int iy = y;
	float *inputBuffer = this->getInputBuffer();

	if (inputBuffer == NULL || ix < 0 || iy < 0 || ix >= (int)this->getWidth() || iy >= (int)this->getHeight() ) {
		output[0] = 0.0f;
	}
	else {
		unsigned int offset = (iy * this->getWidth() + ix);
		output[0] = inputBuffer[offset];
	}
}

/* ******** Render Layers Diffuse Operation ******** */

RenderLayersDiffuseOperation::RenderLayersDiffuseOperation() : RenderLayersBaseProg(SCE_PASS_DIFFUSE, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Emit Operation ******** */

RenderLayersEmitOperation::RenderLayersEmitOperation() : RenderLayersBaseProg(SCE_PASS_EMIT, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Environment Operation ******** */

RenderLayersEnvironmentOperation::RenderLayersEnvironmentOperation() : RenderLayersBaseProg(SCE_PASS_ENVIRONMENT, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Image Operation ******** */

RenderLayersColorProg::RenderLayersColorProg() : RenderLayersBaseProg(SCE_PASS_COMBINED, 4)
{
	this->addOutputSocket(COM_DT_COLOR);
}

/* ******** Render Layers Indirect Operation ******** */

RenderLayersIndirectOperation::RenderLayersIndirectOperation() : RenderLayersBaseProg(SCE_PASS_INDIRECT, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Material Index Operation ******** */

RenderLayersMaterialIndexOperation::RenderLayersMaterialIndexOperation() : RenderLayersBaseProg(SCE_PASS_INDEXMA, 1)
{
	this->addOutputSocket(COM_DT_VALUE);
}

/* ******** Render Layers Mist Operation ******** */

RenderLayersMistOperation::RenderLayersMistOperation() : RenderLayersBaseProg(SCE_PASS_MIST, 1)
{
	this->addOutputSocket(COM_DT_VALUE);
}

/* ******** Render Layers Normal Operation ******** */

RenderLayersNormalOperation::RenderLayersNormalOperation() : RenderLayersBaseProg(SCE_PASS_NORMAL, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Object Index Operation ******** */

RenderLayersObjectIndexOperation::RenderLayersObjectIndexOperation() : RenderLayersBaseProg(SCE_PASS_INDEXOB, 1)
{
	this->addOutputSocket(COM_DT_VALUE);
}

/* ******** Render Layers Reflection Operation ******** */

RenderLayersReflectionOperation::RenderLayersReflectionOperation() : RenderLayersBaseProg(SCE_PASS_REFLECT, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Refraction Operation ******** */

RenderLayersRefractionOperation::RenderLayersRefractionOperation() : RenderLayersBaseProg(SCE_PASS_REFRACT, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Shadow Operation ******** */

RenderLayersShadowOperation::RenderLayersShadowOperation() : RenderLayersBaseProg(SCE_PASS_SHADOW, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Specular Operation ******** */

RenderLayersSpecularOperation::RenderLayersSpecularOperation() : RenderLayersBaseProg(SCE_PASS_SPEC, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}

/* ******** Render Layers Speed Operation ******** */

RenderLayersSpeedOperation::RenderLayersSpeedOperation() : RenderLayersBaseProg(SCE_PASS_VECTOR, 4)
{
	this->addOutputSocket(COM_DT_COLOR);
}

/* ******** Render Layers UV Operation ******** */

RenderLayersUVOperation::RenderLayersUVOperation() : RenderLayersBaseProg(SCE_PASS_UV, 3)
{
	this->addOutputSocket(COM_DT_VECTOR);
}
