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

#include "COM_DefocusNode.h"
#include "DNA_scene_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_ConvertDepthToRadiusOperation.h"
#include "COM_VariableSizeBokehBlurOperation.h"
#include "COM_BokehImageOperation.h"
#include "COM_MathBaseOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_GammaCorrectOperation.h"
#include "COM_FastGaussianBlurOperation.h"

DefocusNode::DefocusNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void DefocusNode::convertToOperations(NodeConverter &converter, const CompositorContext &context) const
{
	bNode *node = this->getbNode();
	NodeDefocus *data = (NodeDefocus *)node->storage;
	Scene *scene = node->id ? (Scene *)node->id : context.getScene();
	Object *camob = scene ? scene->camera : NULL;

	NodeOperation *radiusOperation;
	if (data->no_zbuf) {
		MathMultiplyOperation *multiply = new MathMultiplyOperation();
		SetValueOperation *multiplier = new SetValueOperation();
		multiplier->setValue(data->scale);
		SetValueOperation *maxRadius = new SetValueOperation();
		maxRadius->setValue(data->maxblur);
		MathMinimumOperation *minimize = new MathMinimumOperation();
		
		converter.addOperation(multiply);
		converter.addOperation(multiplier);
		converter.addOperation(maxRadius);
		converter.addOperation(minimize);
		
		converter.mapInputSocket(getInputSocket(1), multiply->getInputSocket(0));
		converter.addLink(multiplier->getOutputSocket(), multiply->getInputSocket(1));
		converter.addLink(multiply->getOutputSocket(), minimize->getInputSocket(0));
		converter.addLink(maxRadius->getOutputSocket(), minimize->getInputSocket(1));
		
		radiusOperation = minimize;
	}
	else {
		ConvertDepthToRadiusOperation *radius_op = new ConvertDepthToRadiusOperation();
		radius_op->setCameraObject(camob);
		radius_op->setfStop(data->fstop);
		radius_op->setMaxRadius(data->maxblur);
		converter.addOperation(radius_op);
		
		converter.mapInputSocket(getInputSocket(1), radius_op->getInputSocket(0));
		
		FastGaussianBlurValueOperation *blur = new FastGaussianBlurValueOperation();
		/* maintain close pixels so far Z values don't bleed into the foreground */
		blur->setOverlay(FAST_GAUSS_OVERLAY_MIN);
		converter.addOperation(blur);
		
		converter.addLink(radius_op->getOutputSocket(0), blur->getInputSocket(0));
		radius_op->setPostBlur(blur);
		
		radiusOperation = blur;
	}
	
	NodeBokehImage *bokehdata = new NodeBokehImage();
	bokehdata->angle = data->rotation;
	bokehdata->rounding = 0.0f;
	bokehdata->flaps = data->bktype;
	if (data->bktype < 3) {
		bokehdata->flaps = 5;
		bokehdata->rounding = 1.0f;
	}
	bokehdata->catadioptric = 0.0f;
	bokehdata->lensshift = 0.0f;
	
	BokehImageOperation *bokeh = new BokehImageOperation();
	bokeh->setData(bokehdata);
	bokeh->deleteDataOnFinish();
	converter.addOperation(bokeh);
	
#ifdef COM_DEFOCUS_SEARCH
	InverseSearchRadiusOperation *search = new InverseSearchRadiusOperation();
	search->setMaxBlur(data->maxblur);
	converter.addOperation(search);
	
	converter.addLink(radiusOperation->getOutputSocket(0), search->getInputSocket(0));
#endif
	
	VariableSizeBokehBlurOperation *operation = new VariableSizeBokehBlurOperation();
	if (data->preview)
		operation->setQuality(COM_QUALITY_LOW);
	else
		operation->setQuality(context.getQuality());
	operation->setMaxBlur(data->maxblur);
	operation->setThreshold(data->bthresh);
	converter.addOperation(operation);
	
	converter.addLink(bokeh->getOutputSocket(), operation->getInputSocket(1));
	converter.addLink(radiusOperation->getOutputSocket(), operation->getInputSocket(2));
#ifdef COM_DEFOCUS_SEARCH
	converter.addLink(search->getOutputSocket(), operation->getInputSocket(3));
#endif
	
	if (data->gamco) {
		GammaCorrectOperation *correct = new GammaCorrectOperation();
		converter.addOperation(correct);
		GammaUncorrectOperation *inverse = new GammaUncorrectOperation();
		converter.addOperation(inverse);
		
		converter.mapInputSocket(getInputSocket(0), correct->getInputSocket(0));
		converter.addLink(correct->getOutputSocket(), operation->getInputSocket(0));
		converter.addLink(operation->getOutputSocket(), inverse->getInputSocket(0));
		converter.mapOutputSocket(getOutputSocket(), inverse->getOutputSocket());
	}
	else {
		converter.mapInputSocket(getInputSocket(0), operation->getInputSocket(0));
		converter.mapOutputSocket(getOutputSocket(), operation->getOutputSocket());
	}
}
