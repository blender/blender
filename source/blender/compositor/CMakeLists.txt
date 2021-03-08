# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2011, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  intern
  nodes
  operations
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../depsgraph
  ../imbuf
  ../makesdna
  ../makesrna
  ../nodes
  ../windowmanager
  ../nodes/composite
  ../nodes/intern
  ../render
  ../render/intern
  ../../../extern/clew/include
  ../../../intern/atomic
  ../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
  COM_compositor.h
  COM_defines.h

  intern/COM_CPUDevice.cc
  intern/COM_CPUDevice.h
  intern/COM_ChunkOrder.cc
  intern/COM_ChunkOrder.h
  intern/COM_ChunkOrderHotspot.cc
  intern/COM_ChunkOrderHotspot.h
  intern/COM_CompositorContext.cc
  intern/COM_CompositorContext.h
  intern/COM_Converter.cc
  intern/COM_Converter.h
  intern/COM_Debug.cc
  intern/COM_Debug.h
  intern/COM_Device.cc
  intern/COM_Device.h
  intern/COM_ExecutionGroup.cc
  intern/COM_ExecutionGroup.h
  intern/COM_ExecutionSystem.cc
  intern/COM_ExecutionSystem.h
  intern/COM_MemoryBuffer.cc
  intern/COM_MemoryBuffer.h
  intern/COM_MemoryProxy.cc
  intern/COM_MemoryProxy.h
  intern/COM_MetaData.cc
  intern/COM_MetaData.h
  intern/COM_Node.cc
  intern/COM_Node.h
  intern/COM_NodeConverter.cc
  intern/COM_NodeConverter.h
  intern/COM_NodeGraph.cc
  intern/COM_NodeGraph.h
  intern/COM_NodeOperation.cc
  intern/COM_NodeOperation.h
  intern/COM_NodeOperationBuilder.cc
  intern/COM_NodeOperationBuilder.h
  intern/COM_OpenCLDevice.cc
  intern/COM_OpenCLDevice.h
  intern/COM_SingleThreadedOperation.cc
  intern/COM_SingleThreadedOperation.h
  intern/COM_SocketReader.cc
  intern/COM_SocketReader.h
  intern/COM_WorkPackage.cc
  intern/COM_WorkPackage.h
  intern/COM_WorkScheduler.cc
  intern/COM_WorkScheduler.h
  intern/COM_compositor.cc

  operations/COM_QualityStepHelper.cc
  operations/COM_QualityStepHelper.h

  # Internal nodes
  nodes/COM_SocketProxyNode.cc
  nodes/COM_SocketProxyNode.h

  # input nodes
  nodes/COM_BokehImageNode.cc
  nodes/COM_BokehImageNode.h
  nodes/COM_ColorNode.cc
  nodes/COM_ColorNode.h
  nodes/COM_ImageNode.cc
  nodes/COM_ImageNode.h
  nodes/COM_MaskNode.cc
  nodes/COM_MaskNode.h
  nodes/COM_MovieClipNode.cc
  nodes/COM_MovieClipNode.h
  nodes/COM_OutputFileNode.cc
  nodes/COM_OutputFileNode.h
  nodes/COM_RenderLayersNode.cc
  nodes/COM_RenderLayersNode.h
  nodes/COM_SwitchNode.cc
  nodes/COM_SwitchNode.h
  nodes/COM_SwitchViewNode.cc
  nodes/COM_SwitchViewNode.h
  nodes/COM_TextureNode.cc
  nodes/COM_TextureNode.h
  nodes/COM_TimeNode.cc
  nodes/COM_TimeNode.h
  nodes/COM_ValueNode.cc
  nodes/COM_ValueNode.h

  # output nodes
  nodes/COM_CompositorNode.cc
  nodes/COM_CompositorNode.h
  nodes/COM_SplitViewerNode.cc
  nodes/COM_SplitViewerNode.h
  nodes/COM_ViewLevelsNode.cc
  nodes/COM_ViewLevelsNode.h
  nodes/COM_ViewerNode.cc
  nodes/COM_ViewerNode.h
  operations/COM_CalculateMeanOperation.cc
  operations/COM_CalculateMeanOperation.h
  operations/COM_CalculateStandardDeviationOperation.cc
  operations/COM_CalculateStandardDeviationOperation.h

  # distort nodes
  nodes/COM_FlipNode.cc
  nodes/COM_FlipNode.h
  nodes/COM_RotateNode.cc
  nodes/COM_RotateNode.h
  nodes/COM_ScaleNode.cc
  nodes/COM_ScaleNode.h
  nodes/COM_TranslateNode.cc
  nodes/COM_TranslateNode.h

  nodes/COM_DisplaceNode.cc
  nodes/COM_DisplaceNode.h
  nodes/COM_MapUVNode.cc
  nodes/COM_MapUVNode.h

  nodes/COM_ChannelMatteNode.cc
  nodes/COM_ChannelMatteNode.h
  nodes/COM_ChromaMatteNode.cc
  nodes/COM_ChromaMatteNode.h
  nodes/COM_ColorMatteNode.cc
  nodes/COM_ColorMatteNode.h
  nodes/COM_DifferenceMatteNode.cc
  nodes/COM_DifferenceMatteNode.h
  nodes/COM_DistanceMatteNode.cc
  nodes/COM_DistanceMatteNode.h
  nodes/COM_LensDistortionNode.cc
  nodes/COM_LensDistortionNode.h
  nodes/COM_LuminanceMatteNode.cc
  nodes/COM_LuminanceMatteNode.h

  nodes/COM_GlareNode.cc
  nodes/COM_GlareNode.h

  nodes/COM_SunBeamsNode.cc
  nodes/COM_SunBeamsNode.h
  operations/COM_SunBeamsOperation.cc
  operations/COM_SunBeamsOperation.h

  nodes/COM_CryptomatteNode.cc
  nodes/COM_CryptomatteNode.h
  operations/COM_CryptomatteOperation.cc
  operations/COM_CryptomatteOperation.h

  nodes/COM_CornerPinNode.cc
  nodes/COM_CornerPinNode.h
  nodes/COM_PlaneTrackDeformNode.cc
  nodes/COM_PlaneTrackDeformNode.h

  nodes/COM_CropNode.cc
  nodes/COM_CropNode.h
  operations/COM_CropOperation.cc
  operations/COM_CropOperation.h

  nodes/COM_DefocusNode.cc
  nodes/COM_DefocusNode.h
  nodes/COM_MovieDistortionNode.cc
  nodes/COM_MovieDistortionNode.h
  nodes/COM_Stabilize2dNode.cc
  nodes/COM_Stabilize2dNode.h
  nodes/COM_TransformNode.cc
  nodes/COM_TransformNode.h

  # color nodes
  nodes/COM_AlphaOverNode.cc
  nodes/COM_AlphaOverNode.h
  nodes/COM_BrightnessNode.cc
  nodes/COM_BrightnessNode.h
  nodes/COM_ColorBalanceNode.cc
  nodes/COM_ColorBalanceNode.h
  nodes/COM_ColorCorrectionNode.cc
  nodes/COM_ColorCorrectionNode.h
  nodes/COM_ColorCurveNode.cc
  nodes/COM_ColorCurveNode.h
  nodes/COM_ColorExposureNode.cc
  nodes/COM_ColorExposureNode.h
  nodes/COM_ColorRampNode.cc
  nodes/COM_ColorRampNode.h
  nodes/COM_ColorToBWNode.cc
  nodes/COM_ColorToBWNode.h
  nodes/COM_ConvertAlphaNode.cc
  nodes/COM_ConvertAlphaNode.h
  nodes/COM_GammaNode.cc
  nodes/COM_GammaNode.h
  nodes/COM_HueSaturationValueCorrectNode.cc
  nodes/COM_HueSaturationValueCorrectNode.h
  nodes/COM_HueSaturationValueNode.cc
  nodes/COM_HueSaturationValueNode.h
  nodes/COM_InvertNode.cc
  nodes/COM_InvertNode.h
  nodes/COM_MixNode.cc
  nodes/COM_MixNode.h
  nodes/COM_SetAlphaNode.cc
  nodes/COM_SetAlphaNode.h
  nodes/COM_TonemapNode.cc
  nodes/COM_TonemapNode.h
  nodes/COM_VectorCurveNode.cc
  nodes/COM_VectorCurveNode.h
  nodes/COM_ZCombineNode.cc
  nodes/COM_ZCombineNode.h
  operations/COM_TonemapOperation.cc
  operations/COM_TonemapOperation.h

  # converter nodes
  nodes/COM_CombineColorNode.cc
  nodes/COM_CombineColorNode.h
  nodes/COM_IDMaskNode.cc
  nodes/COM_IDMaskNode.h
  nodes/COM_SeparateColorNode.cc
  nodes/COM_SeparateColorNode.h

  nodes/COM_MapRangeNode.cc
  nodes/COM_MapRangeNode.h
  nodes/COM_MapValueNode.cc
  nodes/COM_MapValueNode.h
  nodes/COM_MathNode.cc
  nodes/COM_MathNode.h
  nodes/COM_NormalNode.cc
  nodes/COM_NormalNode.h
  nodes/COM_NormalizeNode.cc
  nodes/COM_NormalizeNode.h

  operations/COM_NormalizeOperation.cc
  operations/COM_NormalizeOperation.h

  nodes/COM_PixelateNode.cc
  nodes/COM_PixelateNode.h
  operations/COM_PixelateOperation.cc
  operations/COM_PixelateOperation.h

  # Filter nodes
  nodes/COM_BilateralBlurNode.cc
  nodes/COM_BilateralBlurNode.h
  operations/COM_BilateralBlurOperation.cc
  operations/COM_BilateralBlurOperation.h
  nodes/COM_VectorBlurNode.cc
  nodes/COM_VectorBlurNode.h
  operations/COM_VectorBlurOperation.cc
  operations/COM_VectorBlurOperation.h
  nodes/COM_BlurNode.cc
  nodes/COM_BlurNode.h
  nodes/COM_BokehBlurNode.cc
  nodes/COM_BokehBlurNode.h
  nodes/COM_DenoiseNode.cc
  nodes/COM_DenoiseNode.h
  nodes/COM_DespeckleNode.cc
  nodes/COM_DespeckleNode.h
  nodes/COM_DilateErodeNode.cc
  nodes/COM_DilateErodeNode.h
  nodes/COM_DirectionalBlurNode.cc
  nodes/COM_DirectionalBlurNode.h
  nodes/COM_FilterNode.cc
  nodes/COM_FilterNode.h
  nodes/COM_InpaintNode.cc
  nodes/COM_InpaintNode.h
  operations/COM_BlurBaseOperation.cc
  operations/COM_BlurBaseOperation.h
  operations/COM_BokehBlurOperation.cc
  operations/COM_BokehBlurOperation.h
  operations/COM_DirectionalBlurOperation.cc
  operations/COM_DirectionalBlurOperation.h
  operations/COM_FastGaussianBlurOperation.cc
  operations/COM_FastGaussianBlurOperation.h
  operations/COM_GammaCorrectOperation.cc
  operations/COM_GammaCorrectOperation.h
  operations/COM_GaussianAlphaXBlurOperation.cc
  operations/COM_GaussianAlphaXBlurOperation.h
  operations/COM_GaussianAlphaYBlurOperation.cc
  operations/COM_GaussianAlphaYBlurOperation.h
  operations/COM_GaussianBokehBlurOperation.cc
  operations/COM_GaussianBokehBlurOperation.h
  operations/COM_GaussianXBlurOperation.cc
  operations/COM_GaussianXBlurOperation.h
  operations/COM_GaussianYBlurOperation.cc
  operations/COM_GaussianYBlurOperation.h
  operations/COM_MovieClipAttributeOperation.cc
  operations/COM_MovieClipAttributeOperation.h
  operations/COM_MovieDistortionOperation.cc
  operations/COM_MovieDistortionOperation.h
  operations/COM_VariableSizeBokehBlurOperation.cc
  operations/COM_VariableSizeBokehBlurOperation.h

  # Matte nodes
  nodes/COM_BoxMaskNode.cc
  nodes/COM_BoxMaskNode.h
  nodes/COM_ColorSpillNode.cc
  nodes/COM_ColorSpillNode.h
  nodes/COM_DoubleEdgeMaskNode.cc
  nodes/COM_DoubleEdgeMaskNode.h
  nodes/COM_EllipseMaskNode.cc
  nodes/COM_EllipseMaskNode.h

  operations/COM_DoubleEdgeMaskOperation.cc
  operations/COM_DoubleEdgeMaskOperation.h


  nodes/COM_KeyingScreenNode.cc
  nodes/COM_KeyingScreenNode.h
  operations/COM_KeyingScreenOperation.cc
  operations/COM_KeyingScreenOperation.h

  nodes/COM_TrackPositionNode.cc
  nodes/COM_TrackPositionNode.h
  operations/COM_TrackPositionOperation.cc
  operations/COM_TrackPositionOperation.h

  nodes/COM_KeyingNode.cc
  nodes/COM_KeyingNode.h
  operations/COM_KeyingBlurOperation.cc
  operations/COM_KeyingBlurOperation.h
  operations/COM_KeyingClipOperation.cc
  operations/COM_KeyingClipOperation.h
  operations/COM_KeyingDespillOperation.cc
  operations/COM_KeyingDespillOperation.h
  operations/COM_KeyingOperation.cc
  operations/COM_KeyingOperation.h

  operations/COM_ColorSpillOperation.cc
  operations/COM_ColorSpillOperation.h
  operations/COM_RenderLayersProg.cc
  operations/COM_RenderLayersProg.h

  operations/COM_BokehImageOperation.cc
  operations/COM_BokehImageOperation.h
  operations/COM_ImageOperation.cc
  operations/COM_ImageOperation.h
  operations/COM_MultilayerImageOperation.cc
  operations/COM_MultilayerImageOperation.h
  operations/COM_TextureOperation.cc
  operations/COM_TextureOperation.h


  operations/COM_SocketProxyOperation.cc
  operations/COM_SocketProxyOperation.h

  operations/COM_CompositorOperation.cc
  operations/COM_CompositorOperation.h
  operations/COM_ConvertDepthToRadiusOperation.cc
  operations/COM_ConvertDepthToRadiusOperation.h
  operations/COM_OutputFileMultiViewOperation.cc
  operations/COM_OutputFileMultiViewOperation.h
  operations/COM_OutputFileOperation.cc
  operations/COM_OutputFileOperation.h
  operations/COM_PreviewOperation.cc
  operations/COM_PreviewOperation.h
  operations/COM_SplitOperation.cc
  operations/COM_SplitOperation.h
  operations/COM_ViewerOperation.cc
  operations/COM_ViewerOperation.h
  operations/COM_ZCombineOperation.cc
  operations/COM_ZCombineOperation.h

  operations/COM_ChangeHSVOperation.cc
  operations/COM_ChangeHSVOperation.h
  operations/COM_ChannelMatteOperation.cc
  operations/COM_ChannelMatteOperation.h
  operations/COM_ChromaMatteOperation.cc
  operations/COM_ChromaMatteOperation.h
  operations/COM_ColorCurveOperation.cc
  operations/COM_ColorCurveOperation.h
  operations/COM_ColorExposureOperation.cc
  operations/COM_ColorExposureOperation.h
  operations/COM_ColorMatteOperation.cc
  operations/COM_ColorMatteOperation.h
  operations/COM_ColorRampOperation.cc
  operations/COM_ColorRampOperation.h
  operations/COM_CurveBaseOperation.cc
  operations/COM_CurveBaseOperation.h
  operations/COM_DifferenceMatteOperation.cc
  operations/COM_DifferenceMatteOperation.h
  operations/COM_DistanceRGBMatteOperation.cc
  operations/COM_DistanceRGBMatteOperation.h
  operations/COM_DistanceYCCMatteOperation.cc
  operations/COM_DistanceYCCMatteOperation.h
  operations/COM_HueSaturationValueCorrectOperation.cc
  operations/COM_HueSaturationValueCorrectOperation.h
  operations/COM_LuminanceMatteOperation.cc
  operations/COM_LuminanceMatteOperation.h
  operations/COM_VectorCurveOperation.cc
  operations/COM_VectorCurveOperation.h

  operations/COM_BrightnessOperation.cc
  operations/COM_BrightnessOperation.h
  operations/COM_ColorCorrectionOperation.cc
  operations/COM_ColorCorrectionOperation.h
  operations/COM_GammaOperation.cc
  operations/COM_GammaOperation.h
  operations/COM_MixOperation.cc
  operations/COM_MixOperation.h
  operations/COM_ReadBufferOperation.cc
  operations/COM_ReadBufferOperation.h
  operations/COM_SetColorOperation.cc
  operations/COM_SetColorOperation.h
  operations/COM_SetValueOperation.cc
  operations/COM_SetValueOperation.h
  operations/COM_SetVectorOperation.cc
  operations/COM_SetVectorOperation.h
  operations/COM_WriteBufferOperation.cc
  operations/COM_WriteBufferOperation.h

  operations/COM_MathBaseOperation.cc
  operations/COM_MathBaseOperation.h

  operations/COM_AlphaOverKeyOperation.cc
  operations/COM_AlphaOverKeyOperation.h
  operations/COM_AlphaOverMixedOperation.cc
  operations/COM_AlphaOverMixedOperation.h
  operations/COM_AlphaOverPremultiplyOperation.cc
  operations/COM_AlphaOverPremultiplyOperation.h

  operations/COM_ColorBalanceASCCDLOperation.cc
  operations/COM_ColorBalanceASCCDLOperation.h
  operations/COM_ColorBalanceLGGOperation.cc
  operations/COM_ColorBalanceLGGOperation.h
  operations/COM_InvertOperation.cc
  operations/COM_InvertOperation.h
  operations/COM_MapRangeOperation.cc
  operations/COM_MapRangeOperation.h
  operations/COM_MapValueOperation.cc
  operations/COM_MapValueOperation.h
  operations/COM_SetAlphaMultiplyOperation.cc
  operations/COM_SetAlphaMultiplyOperation.h
  operations/COM_SetAlphaReplaceOperation.cc
  operations/COM_SetAlphaReplaceOperation.h

  # Distort operation
  operations/COM_DisplaceOperation.cc
  operations/COM_DisplaceOperation.h
  operations/COM_DisplaceSimpleOperation.cc
  operations/COM_DisplaceSimpleOperation.h
  operations/COM_FlipOperation.cc
  operations/COM_FlipOperation.h
  operations/COM_MapUVOperation.cc
  operations/COM_MapUVOperation.h
  operations/COM_PlaneCornerPinOperation.cc
  operations/COM_PlaneCornerPinOperation.h
  operations/COM_PlaneDistortCommonOperation.cc
  operations/COM_PlaneDistortCommonOperation.h
  operations/COM_PlaneTrackOperation.cc
  operations/COM_PlaneTrackOperation.h
  operations/COM_ProjectorLensDistortionOperation.cc
  operations/COM_ProjectorLensDistortionOperation.h
  operations/COM_RotateOperation.cc
  operations/COM_RotateOperation.h
  operations/COM_ScaleOperation.cc
  operations/COM_ScaleOperation.h
  operations/COM_ScreenLensDistortionOperation.cc
  operations/COM_ScreenLensDistortionOperation.h
  operations/COM_TranslateOperation.cc
  operations/COM_TranslateOperation.h
  operations/COM_WrapOperation.cc
  operations/COM_WrapOperation.h

  # Filter operations
  operations/COM_ConvolutionEdgeFilterOperation.cc
  operations/COM_ConvolutionEdgeFilterOperation.h
  operations/COM_ConvolutionFilterOperation.cc
  operations/COM_ConvolutionFilterOperation.h
  operations/COM_DenoiseOperation.cc
  operations/COM_DenoiseOperation.h
  operations/COM_DespeckleOperation.cc
  operations/COM_DespeckleOperation.h
  operations/COM_DilateErodeOperation.cc
  operations/COM_DilateErodeOperation.h
  operations/COM_GlareBaseOperation.cc
  operations/COM_GlareBaseOperation.h
  operations/COM_GlareFogGlowOperation.cc
  operations/COM_GlareFogGlowOperation.h
  operations/COM_GlareGhostOperation.cc
  operations/COM_GlareGhostOperation.h
  operations/COM_GlareSimpleStarOperation.cc
  operations/COM_GlareSimpleStarOperation.h
  operations/COM_GlareStreaksOperation.cc
  operations/COM_GlareStreaksOperation.h
  operations/COM_GlareThresholdOperation.cc
  operations/COM_GlareThresholdOperation.h
  operations/COM_InpaintOperation.cc
  operations/COM_InpaintOperation.h
  operations/COM_SetSamplerOperation.cc
  operations/COM_SetSamplerOperation.h


  # Convert operations
  operations/COM_ConvertOperation.cc
  operations/COM_ConvertOperation.h
  operations/COM_IDMaskOperation.cc
  operations/COM_IDMaskOperation.h

  operations/COM_DotproductOperation.cc
  operations/COM_DotproductOperation.h

  # Matte operation
  operations/COM_BoxMaskOperation.cc
  operations/COM_BoxMaskOperation.h
  operations/COM_EllipseMaskOperation.cc
  operations/COM_EllipseMaskOperation.h

  operations/COM_ConvertColorProfileOperation.cc
  operations/COM_ConvertColorProfileOperation.h
  operations/COM_MovieClipOperation.cc
  operations/COM_MovieClipOperation.h

  operations/COM_AntiAliasOperation.cc
  operations/COM_AntiAliasOperation.h

  operations/COM_MaskOperation.cc
  operations/COM_MaskOperation.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  extern_clew
)

list(APPEND INC
  ${CMAKE_CURRENT_BINARY_DIR}/operations
)

data_to_c(
  ${CMAKE_CURRENT_SOURCE_DIR}/operations/COM_OpenCLKernels.cl
  ${CMAKE_CURRENT_BINARY_DIR}/operations/COM_OpenCLKernels.cl.h
  SRC
)

add_definitions(-DCL_USE_DEPRECATED_OPENCL_1_1_APIS)

if(WITH_INTERNATIONAL)
  add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  add_definitions(-DOIDN_STATIC_LIB)
  list(APPEND INC_SYS
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
    ${TBB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENIMAGEDENOISE_LIBRARIES}
    ${TBB_LIBRARIES}
  )
endif()

blender_add_lib(bf_compositor "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
