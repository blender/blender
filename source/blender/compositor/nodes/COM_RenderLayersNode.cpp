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

#include "COM_RenderLayersNode.h"
#include "COM_RenderLayersProg.h"
#include "COM_TranslateOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"
#include "COM_SetValueOperation.h"

RenderLayersNode::RenderLayersNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

void RenderLayersNode::testSocketLink(NodeConverter &converter, const CompositorContext &context,
                                      NodeOutput *output, RenderLayersProg *operation,
                                      Scene *scene, int layerId, bool is_preview) const
{
	operation->setScene(scene);
	operation->setLayerId(layerId);
	operation->setRenderData(context.getRenderData());
	operation->setViewName(context.getViewName());

	converter.mapOutputSocket(output, operation->getOutputSocket());
	converter.addOperation(operation);
	
	if (is_preview) /* only for image socket */
		converter.addPreview(operation->getOutputSocket());
}

void RenderLayersNode::testRenderLink(NodeConverter &converter,
                                      const CompositorContext &context,
                                      Render *re) const
{
	Scene *scene = (Scene *)this->getbNode()->id;
	const short layerId = this->getbNode()->custom1;
	RenderResult *rr = RE_AcquireResultRead(re);
	if (rr == NULL) {
		return;
	}
	SceneRenderLayer *srl = (SceneRenderLayer *)BLI_findlink(&scene->r.layers, layerId);
	if (srl == NULL) {
		return;
	}
	RenderLayer *rl = RE_GetRenderLayer(rr, srl->name);
	if (rl == NULL) {
		return;
	}
	const int numberOfOutputs = this->getNumberOfOutputSockets();
	for (int i = 0; i < numberOfOutputs; i++) {
		NodeOutput *output = this->getOutputSocket(i);
		NodeImageLayer *storage = (NodeImageLayer*) output->getbNodeSocket()->storage;
		RenderPass *rpass = (RenderPass*) BLI_findstring(
		        &rl->passes,
		        storage->pass_name,
		        offsetof(RenderPass, name));
		if (rpass == NULL) {
			continue;
		}
		RenderLayersProg *operation;
		bool is_preview;
		if (STREQ(rpass->name, RE_PASSNAME_COMBINED) &&
		    STREQ(output->getbNodeSocket()->name, "Alpha"))
		{
			operation = new RenderLayersAlphaProg(rpass->name,
			                                      COM_DT_VALUE,
			                                      rpass->channels);
			is_preview = false;
		}
		else if (STREQ(rpass->name, RE_PASSNAME_Z)) {
			operation = new RenderLayersDepthProg(rpass->name,
			                                      COM_DT_VALUE,
			                                      rpass->channels);
			is_preview = false;
		}
		else {
			DataType type = ((rpass->channels == 4)? COM_DT_COLOR : ((rpass->channels == 3)? COM_DT_VECTOR : COM_DT_VALUE));
			operation = new RenderLayersProg(rpass->name,
			                                 type,
			                                 rpass->channels);
			is_preview = STREQ(output->getbNodeSocket()->name, "Image");
		}
		testSocketLink(converter,
		               context,
		               output,
		               operation,
		               scene,
		               layerId,
		               is_preview);
	}
}

void RenderLayersNode::convertToOperations(NodeConverter &converter,
                                           const CompositorContext &context) const
{
	Scene *scene = (Scene *)this->getbNode()->id;
	Render *re = (scene) ? RE_GetRender(scene->id.name) : NULL;

	if (re != NULL) {
		testRenderLink(converter, context, re);
		RE_ReleaseResult(re);
	}
}
