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

#include "COM_TextureOperation.h"

#include "BLI_listbase.h"
#include "DNA_scene_types.h"

TextureBaseOperation::TextureBaseOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VECTOR); //offset
	this->addInputSocket(COM_DT_VECTOR); //size
	this->m_texture = NULL;
	this->m_inputSize = NULL;
	this->m_inputOffset = NULL;
	this->m_rd = NULL;
}
TextureOperation::TextureOperation() : TextureBaseOperation()
{
	this->addOutputSocket(COM_DT_COLOR);
}
TextureAlphaOperation::TextureAlphaOperation() : TextureBaseOperation()
{
	this->addOutputSocket(COM_DT_VALUE);
}

void TextureBaseOperation::initExecution()
{
	this->m_inputOffset = getInputSocketReader(0);
	this->m_inputSize = getInputSocketReader(1);
}
void TextureBaseOperation::deinitExecution()
{
	this->m_inputSize = NULL;
	this->m_inputOffset = NULL;
}

void TextureBaseOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[])
{
	if (preferredResolution[0] == 0 || preferredResolution[1] == 0) {
		int width = this->m_rd->xsch * this->m_rd->size / 100;
		int height = this->m_rd->ysch * this->m_rd->size / 100;
		resolution[0] = width;
		resolution[1] = height;
	}
	else {
		resolution[0] = preferredResolution[0];
		resolution[1] = preferredResolution[1];
	}
}

void TextureAlphaOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	TextureBaseOperation::executePixel(color, x, y, sampler);
	color[0] = color[3];
	color[1] = 0.0f;
	color[2] = 0.0f;
	color[3] = 0.0f;
}

void TextureBaseOperation::executePixel(float *color, float x, float y, PixelSampler sampler)
{
	TexResult texres = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float textureSize[4];
	float textureOffset[4];
	float vec[3];
	int retval;
	const float cx = this->getWidth() / 2;
	const float cy = this->getHeight() / 2;
	const float u = (cx - x) / this->getWidth() * 2;
	const float v = (cy - y) / this->getHeight() * 2;

	this->m_inputSize->read(textureSize, x, y, sampler);
	this->m_inputOffset->read(textureOffset, x, y, sampler);

	vec[0] = textureSize[0] * (u + textureOffset[0]);
	vec[1] = textureSize[1] * (v + textureOffset[1]);
	vec[2] = textureSize[2] * textureOffset[2];

	retval = multitex_ext(this->m_texture, vec, NULL, NULL, 0, &texres);

	if (texres.talpha)
		color[3] = texres.ta;
	else
		color[3] = texres.tin;

	if ((retval & TEX_RGB)) {
		color[0] = texres.tr;
		color[1] = texres.tg;
		color[2] = texres.tb;
	}
	else color[0] = color[1] = color[2] = color[3];
}
