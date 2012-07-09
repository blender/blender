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

#include "COM_ConvertDepthToRadiusOperation.h"
#include "BLI_math.h"
#include "DNA_camera_types.h"

ConvertDepthToRadiusOperation::ConvertDepthToRadiusOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_inputOperation = NULL;
	this->m_fStop = 128.0f;
	this->m_cameraObject = NULL;
	this->m_maxRadius = 32.0f;
	this->m_blurPostOperation = NULL;
}

float ConvertDepthToRadiusOperation::determineFocalDistance()
{

	if (this->m_cameraObject == NULL || this->m_cameraObject->type != OB_CAMERA) {
		return 10.0f;
	}
	else {
		Camera *camera = (Camera *)this->m_cameraObject->data;
		this->m_cam_lens = camera->lens;
		if (camera->dof_ob) {
			/* too simple, better to return the distance on the view axis only
			 * return len_v3v3(ob->obmat[3], cam->dof_ob->obmat[3]); */
			float mat[4][4], imat[4][4], obmat[4][4];

			copy_m4_m4(obmat, this->m_cameraObject->obmat);
			normalize_m4(obmat);
			invert_m4_m4(imat, obmat);
			mult_m4_m4m4(mat, imat, camera->dof_ob->obmat);
			return (float)fabs(mat[3][2]);
		}
		return camera->YF_dofdist;
	}
}

void ConvertDepthToRadiusOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
	float focalDistance = determineFocalDistance();
	if (focalDistance == 0.0f) focalDistance = 1e10f;  /* if the dof is 0.0 then set it be be far away */
	this->m_inverseFocalDistance = 1.f / focalDistance;
	this->m_aspect = (this->getWidth() > this->getHeight()) ? (this->getHeight() / (float)this->getWidth()) : (this->getWidth() / (float)this->getHeight());
	this->m_aperture = 0.5f * (this->m_cam_lens / (this->m_aspect * 32.0f)) / this->m_fStop;
	float minsz = MIN2(getWidth(), getHeight());
	this->m_dof_sp = (float)minsz / (16.f / this->m_cam_lens);    // <- == aspect * MIN2(img->x, img->y) / tan(0.5f * fov);
	
	if (this->m_blurPostOperation) {
		m_blurPostOperation->setSigma(m_aperture*128.0f);
	}
}

void ConvertDepthToRadiusOperation::executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	float inputValue[4];
	float z;
	float radius;
	this->m_inputOperation->read(inputValue, x, y, sampler, inputBuffers);
	z = inputValue[0];
	if (z != 0.f) {
		float iZ = (1.f / z);
		
		// bug #6656 part 2b, do not rescale
#if 0
		bcrad = 0.5f*fabs(aperture*(dof_sp*(cam_invfdist - iZ) - 1.f));
		// scale crad back to original maximum and blend
		crad->rect[px] = bcrad + wts->rect[px]*(scf*crad->rect[px] - bcrad);
#endif
		radius = 0.5f * fabsf(this->m_aperture * (this->m_dof_sp * (this->m_inverseFocalDistance - iZ) - 1.f));
		// 'bug' #6615, limit minimum radius to 1 pixel, not really a solution, but somewhat mitigates the problem
		if (radius < 0.0f) radius = 0.0f;
		if (radius > this->m_maxRadius) {
			radius = this->m_maxRadius;
		}
		outputValue[0] = radius;
	}
	else outputValue[0] = 0.0f;
}

void ConvertDepthToRadiusOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}
