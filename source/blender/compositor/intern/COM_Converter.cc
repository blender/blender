/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#include <cstring>

#include "DNA_node_types.h"

#include "BKE_node.hh"

#include "COM_NodeOperationBuilder.h"

#include "COM_AlphaOverNode.h"
#include "COM_AntiAliasingNode.h"
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
#include "COM_CombineColorNodeLegacy.h"
#include "COM_CombineXYZNode.h"
#include "COM_CompositorNode.h"
#include "COM_ConvertAlphaNode.h"
#include "COM_ConvertColorSpaceNode.h"
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
#include "COM_PosterizeNode.h"
#include "COM_RenderLayersNode.h"
#include "COM_RotateNode.h"
#include "COM_ScaleNode.h"
#include "COM_ScaleOperation.h"
#include "COM_SceneTimeNode.h"
#include "COM_SeparateColorNode.h"
#include "COM_SeparateColorNodeLegacy.h"
#include "COM_SeparateXYZNode.h"
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

namespace blender::compositor {

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
  if (blender::bke::node_type_is_undefined(b_node)) {
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
    case CMP_NODE_SEPARATE_COLOR:
      node = new SeparateColorNode(b_node);
      break;
    case CMP_NODE_COMBINE_COLOR:
      node = new CombineColorNode(b_node);
      break;
    case CMP_NODE_SEPRGBA_LEGACY:
      node = new SeparateRGBANode(b_node);
      break;
    case CMP_NODE_COMBRGBA_LEGACY:
      node = new CombineRGBANode(b_node);
      break;
    case CMP_NODE_SEPHSVA_LEGACY:
      node = new SeparateHSVANode(b_node);
      break;
    case CMP_NODE_COMBHSVA_LEGACY:
      node = new CombineHSVANode(b_node);
      break;
    case CMP_NODE_SEPYUVA_LEGACY:
      node = new SeparateYUVANode(b_node);
      break;
    case CMP_NODE_COMBYUVA_LEGACY:
      node = new CombineYUVANode(b_node);
      break;
    case CMP_NODE_SEPYCCA_LEGACY:
      node = new SeparateYCCANode(b_node);
      break;
    case CMP_NODE_COMBYCCA_LEGACY:
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
    case CMP_NODE_SCENE_TIME:
      node = new SceneTimeNode(b_node);
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
    case CMP_NODE_CRYPTOMATTE_LEGACY:
      node = new CryptomatteLegacyNode(b_node);
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
    case CMP_NODE_ANTIALIASING:
      node = new AntiAliasingNode(b_node);
      break;
    case CMP_NODE_POSTERIZE:
      node = new PosterizeNode(b_node);
      break;
    case CMP_NODE_CONVERT_COLOR_SPACE:
      node = new ConvertColorSpaceNode(b_node);
      break;
    case CMP_NODE_SEPARATE_XYZ:
      node = new SeparateXYZNode(b_node);
      break;
    case CMP_NODE_COMBINE_XYZ:
      node = new CombineXYZNode(b_node);
      break;
  }
  return node;
}

/* TODO(jbakker): make this an std::optional<NodeOperation>. */
NodeOperation *COM_convert_data_type(const NodeOperationOutput &from, const NodeOperationInput &to)
{
  const DataType src_data_type = from.get_data_type();
  const DataType dst_data_type = to.get_data_type();

  if (src_data_type == DataType::Value && dst_data_type == DataType::Color) {
    return new ConvertValueToColorOperation();
  }
  if (src_data_type == DataType::Value && dst_data_type == DataType::Vector) {
    return new ConvertValueToVectorOperation();
  }
  if (src_data_type == DataType::Color && dst_data_type == DataType::Value) {
    return new ConvertColorToValueOperation();
  }
  if (src_data_type == DataType::Color && dst_data_type == DataType::Vector) {
    return new ConvertColorToVectorOperation();
  }
  if (src_data_type == DataType::Vector && dst_data_type == DataType::Value) {
    return new ConvertVectorToValueOperation();
  }
  if (src_data_type == DataType::Vector && dst_data_type == DataType::Color) {
    return new ConvertVectorToColorOperation();
  }

  return nullptr;
}

void COM_convert_canvas(NodeOperationBuilder &builder,
                        NodeOperationOutput *from_socket,
                        NodeOperationInput *to_socket)
{
  /* Data type conversions are executed before resolutions to ensure convert operations have
   * resolution. This method have to ensure same datatypes are linked for new operations. */
  BLI_assert(from_socket->get_data_type() == to_socket->get_data_type());

  ResizeMode mode = to_socket->get_resize_mode();
  BLI_assert(mode != ResizeMode::None);

  NodeOperation *to_operation = &to_socket->get_operation();
  const float to_width = to_operation->get_width();
  const float to_height = to_operation->get_height();
  NodeOperation *from_operation = &from_socket->get_operation();
  const float from_width = from_operation->get_width();
  const float from_height = from_operation->get_height();
  bool do_center = false;
  bool do_scale = false;
  float scaleX = 0;
  float scaleY = 0;

  switch (mode) {
    case ResizeMode::None:
    case ResizeMode::Align:
      break;
    case ResizeMode::Center:
      do_center = true;
      break;
    case ResizeMode::FitWidth:
      do_center = true;
      do_scale = true;
      scaleX = scaleY = to_width / from_width;
      break;
    case ResizeMode::FitHeight:
      do_center = true;
      do_scale = true;
      scaleX = scaleY = to_height / from_height;
      break;
    case ResizeMode::FitAny:
      do_center = true;
      do_scale = true;
      scaleX = to_width / from_width;
      scaleY = to_height / from_height;
      if (scaleX < scaleY) {
        scaleX = scaleY;
      }
      else {
        scaleY = scaleX;
      }
      break;
    case ResizeMode::Stretch:
      do_center = true;
      do_scale = true;
      scaleX = to_width / from_width;
      scaleY = to_height / from_height;
      break;
  }

  float addX = do_center ? (to_width - from_width) / 2.0f : 0.0f;
  float addY = do_center ? (to_height - from_height) / 2.0f : 0.0f;
  NodeOperation *first = nullptr;
  ScaleOperation *scale_operation = nullptr;
  if (do_scale) {
    scale_operation = new ScaleRelativeOperation(from_socket->get_data_type());
    scale_operation->get_input_socket(1)->set_resize_mode(ResizeMode::None);
    scale_operation->get_input_socket(2)->set_resize_mode(ResizeMode::None);
    first = scale_operation;
    SetValueOperation *sxop = new SetValueOperation();
    sxop->set_value(scaleX);
    builder.add_link(sxop->get_output_socket(), scale_operation->get_input_socket(1));
    SetValueOperation *syop = new SetValueOperation();
    syop->set_value(scaleY);
    builder.add_link(syop->get_output_socket(), scale_operation->get_input_socket(2));
    builder.add_operation(sxop);
    builder.add_operation(syop);

    rcti scale_canvas = from_operation->get_canvas();
    if (builder.context().get_execution_model() == eExecutionModel::FullFrame) {
      ScaleOperation::scale_area(scale_canvas, scaleX, scaleY);
      scale_canvas.xmax = scale_canvas.xmin + to_operation->get_width();
      scale_canvas.ymax = scale_canvas.ymin + to_operation->get_height();
      addX = 0;
      addY = 0;
    }
    scale_operation->set_canvas(scale_canvas);
    sxop->set_canvas(scale_canvas);
    syop->set_canvas(scale_canvas);
    builder.add_operation(scale_operation);
  }

  TranslateOperation *translate_operation = new TranslateOperation(to_socket->get_data_type());
  translate_operation->get_input_socket(1)->set_resize_mode(ResizeMode::None);
  translate_operation->get_input_socket(2)->set_resize_mode(ResizeMode::None);
  if (!first) {
    first = translate_operation;
  }
  SetValueOperation *xop = new SetValueOperation();
  xop->set_value(addX);
  builder.add_link(xop->get_output_socket(), translate_operation->get_input_socket(1));
  SetValueOperation *yop = new SetValueOperation();
  yop->set_value(addY);
  builder.add_link(yop->get_output_socket(), translate_operation->get_input_socket(2));
  builder.add_operation(xop);
  builder.add_operation(yop);

  rcti translate_canvas = to_operation->get_canvas();
  if (mode == ResizeMode::Align) {
    translate_canvas.xmax = translate_canvas.xmin + from_width;
    translate_canvas.ymax = translate_canvas.ymin + from_height;
  }
  translate_operation->set_canvas(translate_canvas);
  xop->set_canvas(translate_canvas);
  yop->set_canvas(translate_canvas);
  builder.add_operation(translate_operation);

  if (do_scale) {
    translate_operation->get_input_socket(0)->set_resize_mode(ResizeMode::None);
    builder.add_link(scale_operation->get_output_socket(),
                     translate_operation->get_input_socket(0));
  }

  /* remove previous link and replace */
  builder.remove_input_link(to_socket);
  first->get_input_socket(0)->set_resize_mode(ResizeMode::None);
  to_socket->set_resize_mode(ResizeMode::None);
  builder.add_link(from_socket, first->get_input_socket(0));
  builder.add_link(translate_operation->get_output_socket(), to_socket);
}

}  // namespace blender::compositor
