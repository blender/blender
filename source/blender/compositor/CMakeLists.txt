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
#
# The Original Code is: all of this file.
#
# Contributor(s): Jeroen Bakker, Monique Dewanchand, Blender Developers Fund.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	intern
	nodes
	operations
	../blenkernel
	../blenlib
	../imbuf
	../makesdna
	../makesrna
	../windowmanager
	../nodes
	../nodes/composite
	../nodes/intern
	../render/extern/include
	../render/intern/include
	../../../intern/opencl
	../../../intern/guardedalloc
)

set(INC_SYS

)

set(SRC
	COM_compositor.h
	COM_defines.h

	intern/COM_compositor.cpp
	intern/COM_ExecutionSystem.cpp
	intern/COM_ExecutionSystem.h
	intern/COM_ExecutionSystemHelper.cpp
	intern/COM_ExecutionSystemHelper.h
	intern/COM_Converter.cpp
	intern/COM_Converter.h
	intern/COM_ExecutionGroup.cpp
	intern/COM_ExecutionGroup.h
	intern/COM_Node.cpp
	intern/COM_Node.h
	intern/COM_NodeBase.cpp
	intern/COM_NodeBase.h
	intern/COM_NodeOperation.cpp
	intern/COM_NodeOperation.h
	intern/COM_Socket.cpp
	intern/COM_Socket.h
	intern/COM_SocketReader.cpp
	intern/COM_SocketReader.h
	intern/COM_InputSocket.cpp
	intern/COM_InputSocket.h
	intern/COM_OutputSocket.cpp
	intern/COM_OutputSocket.h
	intern/COM_SocketConnection.cpp
	intern/COM_SocketConnection.h
	intern/COM_MemoryProxy.cpp
	intern/COM_MemoryProxy.h
	intern/COM_MemoryBuffer.cpp
	intern/COM_MemoryBuffer.h
	intern/COM_WorkScheduler.cpp
	intern/COM_WorkScheduler.h
	intern/COM_WorkPackage.cpp
	intern/COM_WorkPackage.h
	intern/COM_ChunkOrder.cpp
	intern/COM_ChunkOrder.h
	intern/COM_ChunkOrderHotspot.cpp
	intern/COM_ChunkOrderHotspot.h
	intern/COM_Device.cpp
	intern/COM_Device.h
	intern/COM_CPUDevice.cpp
	intern/COM_CPUDevice.h
	intern/COM_OpenCLDevice.cpp
	intern/COM_OpenCLDevice.h
	intern/COM_CompositorContext.cpp
	intern/COM_CompositorContext.h
	intern/COM_ChannelInfo.cpp
	intern/COM_ChannelInfo.h
	intern/COM_SingleThreadedNodeOperation.cpp
	intern/COM_SingleThreadedNodeOperation.h
	intern/COM_Debug.cpp
	intern/COM_Debug.h

	operations/COM_QualityStepHelper.h
	operations/COM_QualityStepHelper.cpp

	# Internal nodes
	nodes/COM_MuteNode.cpp
	nodes/COM_MuteNode.h
	nodes/COM_GroupNode.cpp
	nodes/COM_GroupNode.h
	nodes/COM_SocketProxyNode.cpp
	nodes/COM_SocketProxyNode.h

	# input nodes
	nodes/COM_RenderLayersNode.cpp
	nodes/COM_RenderLayersNode.h
	nodes/COM_ImageNode.cpp
	nodes/COM_ImageNode.h
	nodes/COM_TextureNode.cpp
	nodes/COM_TextureNode.h
	nodes/COM_BokehImageNode.cpp
	nodes/COM_BokehImageNode.h
	nodes/COM_ColorNode.cpp
	nodes/COM_ColorNode.h
	nodes/COM_ValueNode.cpp
	nodes/COM_ValueNode.h
	nodes/COM_TimeNode.cpp
	nodes/COM_TimeNode.h
	nodes/COM_SwitchNode.cpp
	nodes/COM_SwitchNode.h
	nodes/COM_MovieClipNode.cpp
	nodes/COM_MovieClipNode.h
	nodes/COM_OutputFileNode.cpp
	nodes/COM_OutputFileNode.h
	nodes/COM_MaskNode.cpp
	nodes/COM_MaskNode.h

	# output nodes
	nodes/COM_CompositorNode.cpp
	nodes/COM_CompositorNode.h
	nodes/COM_ViewerNode.cpp
	nodes/COM_ViewerNode.h
	nodes/COM_SplitViewerNode.cpp
	nodes/COM_SplitViewerNode.h
	nodes/COM_ViewLevelsNode.cpp
	nodes/COM_ViewLevelsNode.h
	operations/COM_CalculateStandardDeviationOperation.cpp
	operations/COM_CalculateStandardDeviationOperation.h
	operations/COM_CalculateMeanOperation.cpp
	operations/COM_CalculateMeanOperation.h

	# distort nodes
	nodes/COM_TranslateNode.cpp
	nodes/COM_TranslateNode.h
	nodes/COM_ScaleNode.cpp
	nodes/COM_ScaleNode.h
	nodes/COM_RotateNode.cpp
	nodes/COM_RotateNode.h
	nodes/COM_FlipNode.cpp
	nodes/COM_FlipNode.h

	nodes/COM_MapUVNode.cpp
	nodes/COM_MapUVNode.h
	nodes/COM_DisplaceNode.cpp
	nodes/COM_DisplaceNode.h

	nodes/COM_DifferenceMatteNode.cpp
	nodes/COM_DifferenceMatteNode.h
	nodes/COM_LuminanceMatteNode.cpp
	nodes/COM_LuminanceMatteNode.h
	nodes/COM_DistanceMatteNode.cpp
	nodes/COM_DistanceMatteNode.h
	nodes/COM_ChromaMatteNode.cpp
	nodes/COM_ChromaMatteNode.h
	nodes/COM_ColorMatteNode.cpp
	nodes/COM_ColorMatteNode.h
	nodes/COM_ChannelMatteNode.cpp
	nodes/COM_ChannelMatteNode.h
	nodes/COM_LensDistortionNode.cpp
	nodes/COM_LensDistortionNode.h

	nodes/COM_GlareNode.cpp
	nodes/COM_GlareNode.h

	nodes/COM_PlaneTrackDeformNode.cpp
	nodes/COM_PlaneTrackDeformNode.h

	nodes/COM_CropNode.cpp
	nodes/COM_CropNode.h
	operations/COM_CropOperation.cpp
	operations/COM_CropOperation.h

	nodes/COM_TransformNode.cpp
	nodes/COM_TransformNode.h
	nodes/COM_Stabilize2dNode.cpp
	nodes/COM_Stabilize2dNode.h
	nodes/COM_MovieDistortionNode.cpp
	nodes/COM_MovieDistortionNode.h
	nodes/COM_DefocusNode.cpp
	nodes/COM_DefocusNode.h

	# color nodes
	nodes/COM_VectorCurveNode.cpp
	nodes/COM_VectorCurveNode.h
	nodes/COM_ColorCurveNode.cpp
	nodes/COM_ColorCurveNode.h
	nodes/COM_ColorToBWNode.cpp
	nodes/COM_ColorToBWNode.h
	nodes/COM_ColorRampNode.cpp
	nodes/COM_ColorRampNode.h
	nodes/COM_MixNode.cpp
	nodes/COM_MixNode.h
	nodes/COM_AlphaOverNode.cpp
	nodes/COM_AlphaOverNode.h
	nodes/COM_ZCombineNode.cpp
	nodes/COM_ZCombineNode.h
	nodes/COM_BrightnessNode.cpp
	nodes/COM_BrightnessNode.h
	nodes/COM_ColorBalanceNode.cpp
	nodes/COM_ColorBalanceNode.h
	nodes/COM_InvertNode.cpp
	nodes/COM_InvertNode.h
	nodes/COM_GammaNode.cpp
	nodes/COM_GammaNode.h
	nodes/COM_SetAlphaNode.cpp
	nodes/COM_SetAlphaNode.h
	nodes/COM_ConvertAlphaNode.cpp
	nodes/COM_ConvertAlphaNode.h
	nodes/COM_HueSaturationValueNode.cpp
	nodes/COM_HueSaturationValueNode.h
	nodes/COM_HueSaturationValueCorrectNode.cpp
	nodes/COM_HueSaturationValueCorrectNode.h
	nodes/COM_ColorCorrectionNode.cpp
	nodes/COM_ColorCorrectionNode.h
	nodes/COM_TonemapNode.cpp
	nodes/COM_TonemapNode.h
	operations/COM_TonemapOperation.cpp
	operations/COM_TonemapOperation.h

	# converter nodes
	nodes/COM_IDMaskNode.cpp
	nodes/COM_IDMaskNode.h
	nodes/COM_SeparateRGBANode.cpp
	nodes/COM_SeparateRGBANode.h
	nodes/COM_CombineRGBANode.cpp
	nodes/COM_CombineRGBANode.h
	nodes/COM_SeparateHSVANode.cpp
	nodes/COM_SeparateHSVANode.h
	nodes/COM_CombineHSVANode.cpp
	nodes/COM_CombineHSVANode.h
	nodes/COM_SeparateYUVANode.cpp
	nodes/COM_SeparateYUVANode.h
	nodes/COM_CombineYUVANode.cpp
	nodes/COM_CombineYUVANode.h
	nodes/COM_SeparateYCCANode.cpp
	nodes/COM_SeparateYCCANode.h
	nodes/COM_CombineYCCANode.cpp
	nodes/COM_CombineYCCANode.h

	nodes/COM_NormalNode.cpp
	nodes/COM_NormalNode.h
	nodes/COM_NormalizeNode.cpp
	nodes/COM_NormalizeNode.h
	nodes/COM_MathNode.cpp
	nodes/COM_MathNode.h
	nodes/COM_MapValueNode.cpp
	nodes/COM_MapValueNode.h
	nodes/COM_MapRangeNode.cpp
	nodes/COM_MapRangeNode.h

	operations/COM_NormalizeOperation.cpp
	operations/COM_NormalizeOperation.h

	nodes/COM_PixelateNode.cpp
	nodes/COM_PixelateNode.h
	operations/COM_PixelateOperation.cpp
	operations/COM_PixelateOperation.h

	# Filter nodes
	nodes/COM_BilateralBlurNode.cpp
	nodes/COM_BilateralBlurNode.h
	operations/COM_BilateralBlurOperation.cpp
	operations/COM_BilateralBlurOperation.h
	nodes/COM_VectorBlurNode.cpp
	nodes/COM_VectorBlurNode.h
	operations/COM_VectorBlurOperation.cpp
	operations/COM_VectorBlurOperation.h

	nodes/COM_FilterNode.cpp
	nodes/COM_FilterNode.h
	nodes/COM_DespeckleNode.cpp
	nodes/COM_DespeckleNode.h
	nodes/COM_DilateErodeNode.cpp
	nodes/COM_DilateErodeNode.h
	nodes/COM_InpaintNode.cpp
	nodes/COM_InpaintNode.h
	nodes/COM_BlurNode.cpp
	nodes/COM_BlurNode.h
	nodes/COM_BokehBlurNode.cpp
	nodes/COM_BokehBlurNode.h
	nodes/COM_DirectionalBlurNode.cpp
	nodes/COM_DirectionalBlurNode.h
	operations/COM_GaussianAlphaXBlurOperation.cpp
	operations/COM_GaussianAlphaXBlurOperation.h
	operations/COM_GaussianAlphaYBlurOperation.cpp
	operations/COM_GaussianAlphaYBlurOperation.h
	operations/COM_GaussianXBlurOperation.cpp
	operations/COM_GaussianXBlurOperation.h
	operations/COM_GaussianYBlurOperation.cpp
	operations/COM_GaussianYBlurOperation.h
	operations/COM_GaussianBokehBlurOperation.cpp
	operations/COM_GaussianBokehBlurOperation.h
	operations/COM_BokehBlurOperation.cpp
	operations/COM_BokehBlurOperation.h
	operations/COM_VariableSizeBokehBlurOperation.cpp
	operations/COM_VariableSizeBokehBlurOperation.h
	operations/COM_FastGaussianBlurOperation.cpp
	operations/COM_FastGaussianBlurOperation.h
	operations/COM_BlurBaseOperation.cpp
	operations/COM_BlurBaseOperation.h
	operations/COM_DirectionalBlurOperation.cpp
	operations/COM_DirectionalBlurOperation.h
	operations/COM_MovieClipAttributeOperation.cpp
	operations/COM_MovieClipAttributeOperation.h
	operations/COM_MovieDistortionOperation.cpp
	operations/COM_MovieDistortionOperation.h
	operations/COM_GammaCorrectOperation.h
	operations/COM_GammaCorrectOperation.cpp

	# Matte nodes
	nodes/COM_BoxMaskNode.cpp
	nodes/COM_BoxMaskNode.h
	nodes/COM_EllipseMaskNode.cpp
	nodes/COM_EllipseMaskNode.h
	nodes/COM_ColorSpillNode.cpp
	nodes/COM_ColorSpillNode.h
	nodes/COM_DoubleEdgeMaskNode.cpp
	nodes/COM_DoubleEdgeMaskNode.h

	operations/COM_DoubleEdgeMaskOperation.cpp
	operations/COM_DoubleEdgeMaskOperation.h


	nodes/COM_KeyingScreenNode.cpp
	nodes/COM_KeyingScreenNode.h
	operations/COM_KeyingScreenOperation.cpp
	operations/COM_KeyingScreenOperation.h

	nodes/COM_TrackPositionNode.cpp
	nodes/COM_TrackPositionNode.h
	operations/COM_TrackPositionOperation.cpp
	operations/COM_TrackPositionOperation.h

	nodes/COM_KeyingNode.cpp
	nodes/COM_KeyingNode.h
	operations/COM_KeyingOperation.cpp
	operations/COM_KeyingOperation.h
	operations/COM_KeyingBlurOperation.cpp
	operations/COM_KeyingBlurOperation.h
	operations/COM_KeyingDespillOperation.cpp
	operations/COM_KeyingDespillOperation.h
	operations/COM_KeyingClipOperation.cpp
	operations/COM_KeyingClipOperation.h

	operations/COM_ColorSpillOperation.cpp
	operations/COM_ColorSpillOperation.h
	operations/COM_RenderLayersProg.cpp
	operations/COM_RenderLayersProg.h

	operations/COM_ImageOperation.cpp
	operations/COM_ImageOperation.h
	operations/COM_MultilayerImageOperation.cpp
	operations/COM_MultilayerImageOperation.h
	operations/COM_TextureOperation.cpp
	operations/COM_TextureOperation.h
	operations/COM_BokehImageOperation.cpp
	operations/COM_BokehImageOperation.h


	operations/COM_SocketProxyOperation.h
	operations/COM_SocketProxyOperation.cpp

	operations/COM_CompositorOperation.h
	operations/COM_CompositorOperation.cpp
	operations/COM_OutputFileOperation.h
	operations/COM_OutputFileOperation.cpp
	operations/COM_ViewerOperation.h
	operations/COM_ViewerOperation.cpp
	operations/COM_PreviewOperation.h
	operations/COM_PreviewOperation.cpp
	operations/COM_SplitOperation.h
	operations/COM_SplitOperation.cpp
	operations/COM_ConvertDepthToRadiusOperation.h
	operations/COM_ConvertDepthToRadiusOperation.cpp
	operations/COM_ZCombineOperation.cpp
	operations/COM_ZCombineOperation.h

	operations/COM_ChangeHSVOperation.h
	operations/COM_ChangeHSVOperation.cpp
	operations/COM_ColorCurveOperation.h
	operations/COM_ColorCurveOperation.cpp
	operations/COM_ColorRampOperation.h
	operations/COM_ColorRampOperation.cpp
	operations/COM_VectorCurveOperation.h
	operations/COM_VectorCurveOperation.cpp
	operations/COM_CurveBaseOperation.h
	operations/COM_CurveBaseOperation.cpp
	operations/COM_HueSaturationValueCorrectOperation.cpp
	operations/COM_HueSaturationValueCorrectOperation.h
	operations/COM_DifferenceMatteOperation.cpp
	operations/COM_DifferenceMatteOperation.h
	operations/COM_LuminanceMatteOperation.cpp
	operations/COM_LuminanceMatteOperation.h
	operations/COM_DistanceRGBMatteOperation.cpp
	operations/COM_DistanceRGBMatteOperation.h
	operations/COM_DistanceYCCMatteOperation.cpp
	operations/COM_DistanceYCCMatteOperation.h
	operations/COM_ChromaMatteOperation.cpp
	operations/COM_ChromaMatteOperation.h
	operations/COM_ColorMatteOperation.cpp
	operations/COM_ColorMatteOperation.h
	operations/COM_ChannelMatteOperation.cpp
	operations/COM_ChannelMatteOperation.h

	operations/COM_ReadBufferOperation.cpp
	operations/COM_ReadBufferOperation.h
	operations/COM_WriteBufferOperation.cpp
	operations/COM_WriteBufferOperation.h
	operations/COM_MixOperation.h
	operations/COM_MixOperation.cpp
	operations/COM_BrightnessOperation.cpp
	operations/COM_BrightnessOperation.h
	operations/COM_GammaOperation.cpp
	operations/COM_GammaOperation.h
	operations/COM_ColorCorrectionOperation.cpp
	operations/COM_ColorCorrectionOperation.h
	operations/COM_SetValueOperation.h
	operations/COM_SetValueOperation.cpp
	operations/COM_SetColorOperation.h
	operations/COM_SetColorOperation.cpp
	operations/COM_SetVectorOperation.h
	operations/COM_SetVectorOperation.cpp

	operations/COM_MathBaseOperation.h
	operations/COM_MathBaseOperation.cpp

	operations/COM_AlphaOverMixedOperation.h
	operations/COM_AlphaOverMixedOperation.cpp
	operations/COM_AlphaOverPremultiplyOperation.h
	operations/COM_AlphaOverPremultiplyOperation.cpp
	operations/COM_AlphaOverKeyOperation.h
	operations/COM_AlphaOverKeyOperation.cpp

	operations/COM_ColorBalanceLGGOperation.h
	operations/COM_ColorBalanceLGGOperation.cpp
	operations/COM_ColorBalanceASCCDLOperation.h
	operations/COM_ColorBalanceASCCDLOperation.cpp
	operations/COM_InvertOperation.cpp
	operations/COM_InvertOperation.h
	operations/COM_SetAlphaOperation.cpp
	operations/COM_SetAlphaOperation.h
	operations/COM_MapValueOperation.cpp
	operations/COM_MapValueOperation.h
	operations/COM_MapRangeOperation.cpp
	operations/COM_MapRangeOperation.h

	# Distort operation
	operations/COM_TranslateOperation.h
	operations/COM_TranslateOperation.cpp
	operations/COM_WrapOperation.h
	operations/COM_WrapOperation.cpp
	operations/COM_RotateOperation.h
	operations/COM_RotateOperation.cpp
	operations/COM_ScaleOperation.h
	operations/COM_ScaleOperation.cpp
	operations/COM_MapUVOperation.h
	operations/COM_MapUVOperation.cpp
	operations/COM_DisplaceOperation.h
	operations/COM_DisplaceOperation.cpp
	operations/COM_DisplaceSimpleOperation.h
	operations/COM_DisplaceSimpleOperation.cpp
	operations/COM_FlipOperation.h
	operations/COM_FlipOperation.cpp
	operations/COM_ProjectorLensDistortionOperation.cpp
	operations/COM_ProjectorLensDistortionOperation.h
	operations/COM_ScreenLensDistortionOperation.cpp
	operations/COM_ScreenLensDistortionOperation.h
	operations/COM_PlaneTrackCommonOperation.cpp
	operations/COM_PlaneTrackCommonOperation.h
	operations/COM_PlaneTrackMaskOperation.cpp
	operations/COM_PlaneTrackMaskOperation.h
	operations/COM_PlaneTrackWarpImageOperation.cpp
	operations/COM_PlaneTrackWarpImageOperation.h

	#Filter operations
	operations/COM_ConvolutionFilterOperation.h
	operations/COM_ConvolutionFilterOperation.cpp
	operations/COM_ConvolutionEdgeFilterOperation.h
	operations/COM_ConvolutionEdgeFilterOperation.cpp
	operations/COM_DespeckleOperation.cpp
	operations/COM_DespeckleOperation.h
	operations/COM_DilateErodeOperation.cpp
	operations/COM_DilateErodeOperation.h
	operations/COM_InpaintOperation.cpp
	operations/COM_InpaintOperation.h
	operations/COM_GlareThresholdOperation.cpp
	operations/COM_GlareThresholdOperation.h
	operations/COM_GlareBaseOperation.cpp
	operations/COM_GlareBaseOperation.h
	operations/COM_GlareSimpleStarOperation.cpp
	operations/COM_GlareSimpleStarOperation.h
	operations/COM_GlareStreaksOperation.cpp
	operations/COM_GlareStreaksOperation.h
	operations/COM_GlareGhostOperation.cpp
	operations/COM_GlareGhostOperation.h
	operations/COM_GlareFogGlowOperation.cpp
	operations/COM_GlareFogGlowOperation.h
	operations/COM_SetSamplerOperation.cpp
	operations/COM_SetSamplerOperation.h


	#Convert operations
	operations/COM_IDMaskOperation.cpp
	operations/COM_IDMaskOperation.h
	operations/COM_ConvertOperation.cpp
	operations/COM_ConvertOperation.h

	operations/COM_DotproductOperation.cpp
	operations/COM_DotproductOperation.h

	# Matte operation
	operations/COM_BoxMaskOperation.h
	operations/COM_BoxMaskOperation.cpp
	operations/COM_EllipseMaskOperation.h
	operations/COM_EllipseMaskOperation.cpp

	operations/COM_MovieClipOperation.cpp
	operations/COM_MovieClipOperation.h
	operations/COM_ConvertColorProfileOperation.cpp
	operations/COM_ConvertColorProfileOperation.h

	operations/COM_AntiAliasOperation.cpp
	operations/COM_AntiAliasOperation.h

	operations/COM_MaskOperation.cpp
	operations/COM_MaskOperation.h
)

list(APPEND INC
	${CMAKE_CURRENT_BINARY_DIR}/operations
)
data_to_c(${CMAKE_CURRENT_SOURCE_DIR}/operations/COM_OpenCLKernels.cl
          ${CMAKE_CURRENT_BINARY_DIR}/operations/COM_OpenCLKernels.cl.h SRC)

blender_add_lib(bf_compositor "${SRC}" "${INC}" "${INC_SYS}")
