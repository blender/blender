/*
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
 * Copyright 2011, Blender Foundation.
 */

#include <cstring>

#include "DNA_node_types.h"

#include "BKE_node.h"

#include "COM_NodeOperation.h"
#include "COM_NodeOperationBuilder.h"

#include "COM_AlphaOverNode.h"
#include "COM_BilateralBlurNode.h"
#include "COM_BlurNode.h"
#include "COM_BokehBlurNode.h"
#include "COM_BokehImageNode.h"
#include "COM_BoxMaskNode.h"
#include "COM_BrightnessNode.h"
#include "COM_ChannelMatteNode.h"
#include "COM_ChromaMatteNode.h"
#include "COM_ColorBalanceNode.h"
#include "COM_ColorCorrectionNode.h"
#include "COM_ColorCurveNode.h"
#include "COM_ColorExposureNode.h"
#include "COM_ColorMatteNode.h"
#include "COM_ColorNode.h"
#include "COM_ColorRampNode.h"
#include "COM_ColorSpillNode.h"
#include "COM_ColorToBWNode.h"
#include "COM_CombineColorNode.h"
#include "COM_CompositorNode.h"
#include "COM_ConvertAlphaNode.h"
#include "COM_ConvertOperation.h"
#include "COM_Converter.h"
#include "COM_CornerPinNode.h"
#include "COM_CropNode.h"
#include "COM_CryptomatteNode.h"
#include "COM_DefocusNode.h"
#include "COM_DenoiseNode.h"
#include "COM_DespeckleNode.h"
#include "COM_DifferenceMatteNode.h"
#include "COM_DilateErodeNode.h"
#include "COM_DirectionalBlurNode.h"
#include "COM_DisplaceNode.h"
#include "COM_DistanceMatteNode.h"
#include "COM_DoubleEdgeMaskNode.h"
#include "COM_EllipseMaskNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_FilterNode.h"
#include "COM_FlipNode.h"
#include "COM_GammaNode.h"
#include "COM_GlareNode.h"
#include "COM_HueSaturationValueCorrectNode.h"
#include "COM_HueSaturationValueNode.h"
#include "COM_IDMaskNode.h"
#include "COM_ImageNode.h"
#include "COM_InpaintNode.h"
#include "COM_InvertNode.h"
#include "COM_KeyingNode.h"
#include "COM_KeyingScreenNode.h"
#include "COM_LensDistortionNode.h"
#include "COM_LuminanceMatteNode.h"
#include "COM_MapRangeNode.h"
#include "COM_MapUVNode.h"
#include "COM_MapValueNode.h"
#include "COM_MaskNode.h"
#include "COM_MathNode.h"
#include "COM_MixNode.h"
#include "COM_MovieClipNode.h"
#include "COM_MovieDistortionNode.h"
#include "COM_NormalNode.h"
#include "COM_NormalizeNode.h"
#include "COM_OutputFileNode.h"
#include "COM_PixelateNode.h"
#include "COM_PlaneTrackDeformNode.h"
#include "COM_RenderLayersNode.h"
#include "COM_RotateNode.h"
#include "COM_ScaleNode.h"
#include "COM_ScaleOperation.h"
#include "COM_SeparateColorNode.h"
#include "COM_SetAlphaNode.h"
#include "COM_SetValueOperation.h"
#include "COM_SplitViewerNode.h"
#include "COM_Stabilize2dNode.h"
#include "COM_SunBeamsNode.h"
#include "COM_SwitchNode.h"
#include "COM_SwitchViewNode.h"
#include "COM_TextureNode.h"
#include "COM_TimeNode.h"
#include "COM_TonemapNode.h"
#include "COM_TrackPositionNode.h"
#include "COM_TransformNode.h"
#include "COM_TranslateNode.h"
#include "COM_TranslateOperation.h"
#include "COM_ValueNode.h"
#include "COM_VectorBlurNode.h"
#include "COM_VectorCurveNode.h"
#include "COM_ViewLevelsNode.h"
#include "COM_ViewerNode.h"
#include "COM_ZCombineNode.h"

bool COM_bnode_is_fast_node(const bNode &b_node)
{
  return !ELEM(b_node.type,
               CMP_NODE_BLUR,
               CMP_NODE_VECBLUR,
               CMP_NODE_BILATERALBLUR,
               CMP_NODE_DEFOCUS,
               CMP_NODE_BOKEHBLUR,
               CMP_NODE_GLARE,
               CMP_NODE_DBLUR,
               CMP_NODE_MOVIEDISTORTION,
               CMP_NODE_LENSDIST,
               CMP_NODE_DOUBLEEDGEMASK,
               CMP_NODE_DILATEERODE,
               CMP_NODE_DENOISE);
}

Node *COM_convert_bnode(bNode *b_node)
{
  Node *node = nullptr;

  /* ignore undefined nodes with missing or invalid node data */
  if (nodeTypeUndefined(b_node)) {
    return nullptr;
  }

  switch (b_node->type) {
    case CMP_NODE_COMPOSITE:
      node = new CompositorNode(b_node);
      break;
    case CMP_NODE_R_LAYERS:
      node = new RenderLayersNode(b_node);
      break;
    case CMP_NODE_TEXTURE:
      node = new TextureNode(b_node);
      break;
    case CMP_NODE_RGBTOBW:
      node = new ColorToBWNode(b_node);
      break;
    case CMP_NODE_MIX_RGB:
      node = new MixNode(b_node);
      break;
    case CMP_NODE_TRANSLATE:
      node = new TranslateNode(b_node);
      break;
    case CMP_NODE_SCALE:
      node = new ScaleNode(b_node);
      break;
    case CMP_NODE_ROTATE:
      node = new RotateNode(b_node);
      break;
    case CMP_NODE_FLIP:
      node = new FlipNode(b_node);
      break;
    case CMP_NODE_FILTER:
      node = new FilterNode(b_node);
      break;
    case CMP_NODE_ID_MASK:
      node = new IDMaskNode(b_node);
      break;
    case CMP_NODE_BRIGHTCONTRAST:
      node = new BrightnessNode(b_node);
      break;
    case CMP_NODE_SEPRGBA:
      node = new SeparateRGBANode(b_node);
      break;
    case CMP_NODE_COMBRGBA:
      node = new CombineRGBANode(b_node);
      break;
    case CMP_NODE_SEPHSVA:
      node = new SeparateHSVANode(b_node);
      break;
    case CMP_NODE_COMBHSVA:
      node = new CombineHSVANode(b_node);
      break;
    case CMP_NODE_SEPYUVA:
      node = new SeparateYUVANode(b_node);
      break;
    case CMP_NODE_COMBYUVA:
      node = new CombineYUVANode(b_node);
      break;
    case CMP_NODE_SEPYCCA:
      node = new SeparateYCCANode(b_node);
      break;
    case CMP_NODE_COMBYCCA:
      node = new CombineYCCANode(b_node);
      break;
    case CMP_NODE_ALPHAOVER:
      node = new AlphaOverNode(b_node);
      break;
    case CMP_NODE_COLORBALANCE:
      node = new ColorBalanceNode(b_node);
      break;
    case CMP_NODE_VIEWER:
      node = new ViewerNode(b_node);
      break;
    case CMP_NODE_SPLITVIEWER:
      node = new SplitViewerNode(b_node);
      break;
    case CMP_NODE_INVERT:
      node = new InvertNode(b_node);
      break;
    case NODE_GROUP:
    case NODE_GROUP_INPUT:
    case NODE_GROUP_OUTPUT:
      /* handled in NodeCompiler */
      break;
    case CMP_NODE_NORMAL:
      node = new NormalNode(b_node);
      break;
    case CMP_NODE_NORMALIZE:
      node = new NormalizeNode(b_node);
      break;
    case CMP_NODE_IMAGE:
      node = new ImageNode(b_node);
      break;
    case CMP_NODE_SETALPHA:
      node = new SetAlphaNode(b_node);
      break;
    case CMP_NODE_PREMULKEY:
      node = new ConvertAlphaNode(b_node);
      break;
    case CMP_NODE_MATH:
      node = new MathNode(b_node);
      break;
    case CMP_NODE_HUE_SAT:
      node = new HueSaturationValueNode(b_node);
      break;
    case CMP_NODE_COLORCORRECTION:
      node = new ColorCorrectionNode(b_node);
      break;
    case CMP_NODE_MASK_BOX:
      node = new BoxMaskNode(b_node);
      break;
    case CMP_NODE_MASK_ELLIPSE:
      node = new EllipseMaskNode(b_node);
      break;
    case CMP_NODE_GAMMA:
      node = new GammaNode(b_node);
      break;
    case CMP_NODE_CURVE_RGB:
      node = new ColorCurveNode(b_node);
      break;
    case CMP_NODE_CURVE_VEC:
      node = new VectorCurveNode(b_node);
      break;
    case CMP_NODE_HUECORRECT:
      node = new HueSaturationValueCorrectNode(b_node);
      break;
    case CMP_NODE_MAP_UV:
      node = new MapUVNode(b_node);
      break;
    case CMP_NODE_DISPLACE:
      node = new DisplaceNode(b_node);
      break;
    case CMP_NODE_VALTORGB:
      node = new ColorRampNode(b_node);
      break;
    case CMP_NODE_DIFF_MATTE:
      node = new DifferenceMatteNode(b_node);
      break;
    case CMP_NODE_LUMA_MATTE:
      node = new LuminanceMatteNode(b_node);
      break;
    case CMP_NODE_DIST_MATTE:
      node = new DistanceMatteNode(b_node);
      break;
    case CMP_NODE_CHROMA_MATTE:
      node = new ChromaMatteNode(b_node);
      break;
    case CMP_NODE_COLOR_MATTE:
      node = new ColorMatteNode(b_node);
      break;
    case CMP_NODE_CHANNEL_MATTE:
      node = new ChannelMatteNode(b_node);
      break;
    case CMP_NODE_BLUR:
      node = new BlurNode(b_node);
      break;
    case CMP_NODE_BOKEHIMAGE:
      node = new BokehImageNode(b_node);
      break;
    case CMP_NODE_BOKEHBLUR:
      node = new BokehBlurNode(b_node);
      break;
    case CMP_NODE_DILATEERODE:
      node = new DilateErodeNode(b_node);
      break;
    case CMP_NODE_INPAINT:
      node = new InpaintNode(b_node);
      break;
    case CMP_NODE_DESPECKLE:
      node = new DespeckleNode(b_node);
      break;
    case CMP_NODE_LENSDIST:
      node = new LensDistortionNode(b_node);
      break;
    case CMP_NODE_RGB:
      node = new ColorNode(b_node);
      break;
    case CMP_NODE_VALUE:
      node = new ValueNode(b_node);
      break;
    case CMP_NODE_TIME:
      node = new TimeNode(b_node);
      break;
    case CMP_NODE_DBLUR:
      node = new DirectionalBlurNode(b_node);
      break;
    case CMP_NODE_ZCOMBINE:
      node = new ZCombineNode(b_node);
      break;
    case CMP_NODE_TONEMAP:
      node = new TonemapNode(b_node);
      break;
    case CMP_NODE_SWITCH:
      node = new SwitchNode(b_node);
      break;
    case CMP_NODE_SWITCH_VIEW:
      node = new SwitchViewNode(b_node);
      break;
    case CMP_NODE_GLARE:
      node = new GlareNode(b_node);
      break;
    case CMP_NODE_MOVIECLIP:
      node = new MovieClipNode(b_node);
      break;
    case CMP_NODE_COLOR_SPILL:
      node = new ColorSpillNode(b_node);
      break;
    case CMP_NODE_OUTPUT_FILE:
      node = new OutputFileNode(b_node);
      break;
    case CMP_NODE_MAP_VALUE:
      node = new MapValueNode(b_node);
      break;
    case CMP_NODE_MAP_RANGE:
      node = new MapRangeNode(b_node);
      break;
    case CMP_NODE_TRANSFORM:
      node = new TransformNode(b_node);
      break;
    case CMP_NODE_STABILIZE2D:
      node = new Stabilize2dNode(b_node);
      break;
    case CMP_NODE_BILATERALBLUR:
      node = new BilateralBlurNode(b_node);
      break;
    case CMP_NODE_VECBLUR:
      node = new VectorBlurNode(b_node);
      break;
    case CMP_NODE_MOVIEDISTORTION:
      node = new MovieDistortionNode(b_node);
      break;
    case CMP_NODE_VIEW_LEVELS:
      node = new ViewLevelsNode(b_node);
      break;
    case CMP_NODE_DEFOCUS:
      node = new DefocusNode(b_node);
      break;
    case CMP_NODE_DOUBLEEDGEMASK:
      node = new DoubleEdgeMaskNode(b_node);
      break;
    case CMP_NODE_CROP:
      node = new CropNode(b_node);
      break;
    case CMP_NODE_MASK:
      node = new MaskNode(b_node);
      break;
    case CMP_NODE_KEYINGSCREEN:
      node = new KeyingScreenNode(b_node);
      break;
    case CMP_NODE_KEYING:
      node = new KeyingNode(b_node);
      break;
    case CMP_NODE_TRACKPOS:
      node = new TrackPositionNode(b_node);
      break;
    /* not implemented yet */
    case CMP_NODE_PIXELATE:
      node = new PixelateNode(b_node);
      break;
    case CMP_NODE_PLANETRACKDEFORM:
      node = new PlaneTrackDeformNode(b_node);
      break;
    case CMP_NODE_CORNERPIN:
      node = new CornerPinNode(b_node);
      break;
    case CMP_NODE_SUNBEAMS:
      node = new SunBeamsNode(b_node);
      break;
    case CMP_NODE_CRYPTOMATTE:
      node = new CryptomatteNode(b_node);
      break;
    case CMP_NODE_DENOISE:
      node = new DenoiseNode(b_node);
      break;
    case CMP_NODE_EXPOSURE:
      node = new ExposureNode(b_node);
      break;
  }
  return node;
}

/* TODO(jbakker): make this an std::optional<NodeOperation>. */
NodeOperation *COM_convert_data_type(const NodeOperationOutput &from, const NodeOperationInput &to)
{
  const DataType src_data_type = from.getDataType();
  const DataType dst_data_type = to.getDataType();

  if (src_data_type == COM_DT_VALUE && dst_data_type == COM_DT_COLOR) {
    return new ConvertValueToColorOperation();
  }
  if (src_data_type == COM_DT_VALUE && dst_data_type == COM_DT_VECTOR) {
    return new ConvertValueToVectorOperation();
  }
  if (src_data_type == COM_DT_COLOR && dst_data_type == COM_DT_VALUE) {
    return new ConvertColorToValueOperation();
  }
  if (src_data_type == COM_DT_COLOR && dst_data_type == COM_DT_VECTOR) {
    return new ConvertColorToVectorOperation();
  }
  if (src_data_type == COM_DT_VECTOR && dst_data_type == COM_DT_VALUE) {
    return new ConvertVectorToValueOperation();
  }
  if (src_data_type == COM_DT_VECTOR && dst_data_type == COM_DT_COLOR) {
    return new ConvertVectorToColorOperation();
  }

  return nullptr;
}

void COM_convert_resolution(NodeOperationBuilder &builder,
                            NodeOperationOutput *fromSocket,
                            NodeOperationInput *toSocket)
{
  InputResizeMode mode = toSocket->getResizeMode();

  NodeOperation *toOperation = &toSocket->getOperation();
  const float toWidth = toOperation->getWidth();
  const float toHeight = toOperation->getHeight();
  NodeOperation *fromOperation = &fromSocket->getOperation();
  const float fromWidth = fromOperation->getWidth();
  const float fromHeight = fromOperation->getHeight();
  bool doCenter = false;
  bool doScale = false;
  float addX = (toWidth - fromWidth) / 2.0f;
  float addY = (toHeight - fromHeight) / 2.0f;
  float scaleX = 0;
  float scaleY = 0;

  switch (mode) {
    case COM_SC_NO_RESIZE:
      break;
    case COM_SC_CENTER:
      doCenter = true;
      break;
    case COM_SC_FIT_WIDTH:
      doCenter = true;
      doScale = true;
      scaleX = scaleY = toWidth / fromWidth;
      break;
    case COM_SC_FIT_HEIGHT:
      doCenter = true;
      doScale = true;
      scaleX = scaleY = toHeight / fromHeight;
      break;
    case COM_SC_FIT:
      doCenter = true;
      doScale = true;
      scaleX = toWidth / fromWidth;
      scaleY = toHeight / fromHeight;
      if (scaleX < scaleY) {
        scaleX = scaleY;
      }
      else {
        scaleY = scaleX;
      }
      break;
    case COM_SC_STRETCH:
      doCenter = true;
      doScale = true;
      scaleX = toWidth / fromWidth;
      scaleY = toHeight / fromHeight;
      break;
  }

  if (doCenter) {
    NodeOperation *first = nullptr;
    ScaleOperation *scaleOperation = nullptr;
    if (doScale) {
      scaleOperation = new ScaleOperation();
      scaleOperation->getInputSocket(1)->setResizeMode(COM_SC_NO_RESIZE);
      scaleOperation->getInputSocket(2)->setResizeMode(COM_SC_NO_RESIZE);
      first = scaleOperation;
      SetValueOperation *sxop = new SetValueOperation();
      sxop->setValue(scaleX);
      builder.addLink(sxop->getOutputSocket(), scaleOperation->getInputSocket(1));
      SetValueOperation *syop = new SetValueOperation();
      syop->setValue(scaleY);
      builder.addLink(syop->getOutputSocket(), scaleOperation->getInputSocket(2));
      builder.addOperation(sxop);
      builder.addOperation(syop);

      unsigned int resolution[2] = {fromOperation->getWidth(), fromOperation->getHeight()};
      scaleOperation->setResolution(resolution);
      sxop->setResolution(resolution);
      syop->setResolution(resolution);
      builder.addOperation(scaleOperation);
    }

    TranslateOperation *translateOperation = new TranslateOperation();
    translateOperation->getInputSocket(1)->setResizeMode(COM_SC_NO_RESIZE);
    translateOperation->getInputSocket(2)->setResizeMode(COM_SC_NO_RESIZE);
    if (!first) {
      first = translateOperation;
    }
    SetValueOperation *xop = new SetValueOperation();
    xop->setValue(addX);
    builder.addLink(xop->getOutputSocket(), translateOperation->getInputSocket(1));
    SetValueOperation *yop = new SetValueOperation();
    yop->setValue(addY);
    builder.addLink(yop->getOutputSocket(), translateOperation->getInputSocket(2));
    builder.addOperation(xop);
    builder.addOperation(yop);

    unsigned int resolution[2] = {toOperation->getWidth(), toOperation->getHeight()};
    translateOperation->setResolution(resolution);
    xop->setResolution(resolution);
    yop->setResolution(resolution);
    builder.addOperation(translateOperation);

    if (doScale) {
      translateOperation->getInputSocket(0)->setResizeMode(COM_SC_NO_RESIZE);
      builder.addLink(scaleOperation->getOutputSocket(), translateOperation->getInputSocket(0));
    }

    /* remove previous link and replace */
    builder.removeInputLink(toSocket);
    first->getInputSocket(0)->setResizeMode(COM_SC_NO_RESIZE);
    toSocket->setResizeMode(COM_SC_NO_RESIZE);
    builder.addLink(fromSocket, first->getInputSocket(0));
    builder.addLink(translateOperation->getOutputSocket(), toSocket);
  }
}
