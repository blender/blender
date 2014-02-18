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

#include "COM_BlurBaseOperation.h"
#include "BLI_math.h"
#include "MEM_guardedalloc.h"

extern "C" {
#  include "RE_pipeline.h"
}

BlurBaseOperation::BlurBaseOperation(DataType data_type) : NodeOperation()
{
	/* data_type is almost always COM_DT_COLOR except for alpha-blur */
	this->addInputSocket(data_type);
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(data_type);
	this->setComplex(true);
	this->m_inputProgram = NULL;
	this->m_data = NULL;
	this->m_size = 1.0f;
	this->m_deleteData = false;
	this->m_sizeavailable = false;
}
void BlurBaseOperation::initExecution()
{
	this->m_inputProgram = this->getInputSocketReader(0);
	this->m_inputSize = this->getInputSocketReader(1);
	this->m_data->image_in_width = this->getWidth();
	this->m_data->image_in_height = this->getHeight();
	if (this->m_data->relative) {
		switch (this->m_data->aspect) {
			case CMP_NODE_BLUR_ASPECT_NONE:
				this->m_data->sizex = (int)(this->m_data->percentx * 0.01f * this->m_data->image_in_width);
				this->m_data->sizey = (int)(this->m_data->percenty * 0.01f * this->m_data->image_in_height);
				break;
			case CMP_NODE_BLUR_ASPECT_Y:
				this->m_data->sizex = (int)(this->m_data->percentx * 0.01f * this->m_data->image_in_width);
				this->m_data->sizey = (int)(this->m_data->percenty * 0.01f * this->m_data->image_in_width);
				break;
			case CMP_NODE_BLUR_ASPECT_X:
				this->m_data->sizex = (int)(this->m_data->percentx * 0.01f * this->m_data->image_in_height);
				this->m_data->sizey = (int)(this->m_data->percenty * 0.01f * this->m_data->image_in_height);
				break;
		}
	}

	QualityStepHelper::initExecution(COM_QH_MULTIPLY);

}

float *BlurBaseOperation::make_gausstab(float rad, int size)
{
	float *gausstab, sum, val;
	int i, n;

	n = 2 * size + 1;

	gausstab = (float *)MEM_mallocN(sizeof(float) * n, __func__);

	sum = 0.0f;
	float fac = (rad > 0.0f ? 1.0f / rad : 0.0f);
	for (i = -size; i <= size; i++) {
		val = RE_filter_value(this->m_data->filtertype, (float)i * fac);
		sum += val;
		gausstab[i + size] = val;
	}

	sum = 1.0f / sum;
	for (i = 0; i < n; i++)
		gausstab[i] *= sum;

	return gausstab;
}

/* normalized distance from the current (inverted so 1.0 is close and 0.0 is far)
 * 'ease' is applied after, looks nicer */
float *BlurBaseOperation::make_dist_fac_inverse(float rad, int size, int falloff)
{
	float *dist_fac_invert, val;
	int i, n;

	n = 2 * size + 1;

	dist_fac_invert = (float *)MEM_mallocN(sizeof(float) * n, __func__);

	float fac = (rad > 0.0f ? 1.0f / rad : 0.0f);
	for (i = -size; i <= size; i++) {
		val = 1.0f - fabsf((float)i * fac);

		/* keep in sync with proportional_falloff_curve_only_items */
		switch (falloff) {
			case PROP_SMOOTH:
				/* ease - gives less hard lines for dilate/erode feather */
				val = (3.0f * val * val - 2.0f * val * val * val);
				break;
			case PROP_SPHERE:
				val = sqrtf(2.0f * val - val * val);
				break;
			case PROP_ROOT:
				val = sqrtf(val);
				break;
			case PROP_SHARP:
				val = val * val;
				break;
			case PROP_LIN:
				/* fall-through */
#ifndef NDEBUG
			case -1:
				/* uninitialized! */
				BLI_assert(0);
				break;
#endif
			default:
				/* nothing */
				break;
		}
		dist_fac_invert[i + size] = val;
	}

	return dist_fac_invert;
}

void BlurBaseOperation::deinitExecution()
{
	this->m_inputProgram = NULL;
	this->m_inputSize = NULL;
	if (this->m_deleteData) {
		delete this->m_data;
	}
	this->m_data = NULL;
}

void BlurBaseOperation::updateSize()
{
	if (!this->m_sizeavailable) {
		float result[4];
		this->getInputSocketReader(1)->readSampled(result, 0, 0, COM_PS_NEAREST);
		this->m_size = result[0];
		this->m_sizeavailable = true;
	}
}
