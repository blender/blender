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
 */

#ifndef __RNA_ACCESS_H__
#define __RNA_ACCESS_H__

/** \file
 * \ingroup RNA
 */

#include <stdarg.h>

#include "RNA_types.h"

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct IDOverrideStatic;
struct IDOverrideStaticProperty;
struct IDOverrideStaticPropertyOperation;
struct ListBase;
struct Main;
struct ReportList;
struct Scene;
struct bContext;

/* Types */
extern BlenderRNA BLENDER_RNA;

/* Keep sorted. */
extern StructRNA RNA_Action;
extern StructRNA RNA_ActionConstraint;
extern StructRNA RNA_ActionFCurves;
extern StructRNA RNA_ActionGroup;
extern StructRNA RNA_ActionGroups;
extern StructRNA RNA_ActionPoseMarkers;
extern StructRNA RNA_Actuator;
extern StructRNA RNA_ActuatorSensor;
extern StructRNA RNA_Addon;
extern StructRNA RNA_AddonPreferences;
extern StructRNA RNA_AdjustmentSequence;
extern StructRNA RNA_AlwaysSensor;
extern StructRNA RNA_AndController;
extern StructRNA RNA_AnimData;
extern StructRNA RNA_AnimViz;
extern StructRNA RNA_AnimVizMotionPaths;
extern StructRNA RNA_AnyType;
extern StructRNA RNA_Area;
extern StructRNA RNA_AreaLight;
extern StructRNA RNA_Armature;
extern StructRNA RNA_ArmatureGpencilModifier;
extern StructRNA RNA_ArmatureModifier;
extern StructRNA RNA_ArmatureSensor;
extern StructRNA RNA_ArrayGpencilModifier;
extern StructRNA RNA_ArrayModifier;
extern StructRNA RNA_BackgroundImage;
extern StructRNA RNA_BevelModifier;
extern StructRNA RNA_BezierSplinePoint;
extern StructRNA RNA_BlendData;
extern StructRNA RNA_BlendTexture;
extern StructRNA RNA_BlenderRNA;
extern StructRNA RNA_BoidRule;
extern StructRNA RNA_BoidRuleAverageSpeed;
extern StructRNA RNA_BoidRuleAvoid;
extern StructRNA RNA_BoidRuleAvoidCollision;
extern StructRNA RNA_BoidRuleFight;
extern StructRNA RNA_BoidRuleFollowLeader;
extern StructRNA RNA_BoidRuleGoal;
extern StructRNA RNA_BoidSettings;
extern StructRNA RNA_BoidState;
extern StructRNA RNA_Bone;
extern StructRNA RNA_BoneGroup;
extern StructRNA RNA_BoolProperty;
extern StructRNA RNA_BooleanModifier;
extern StructRNA RNA_Brush;
extern StructRNA RNA_BrushCapabilitiesImagePaint;
extern StructRNA RNA_BrushCapabilitiesVertexPaint;
extern StructRNA RNA_BrushTextureSlot;
extern StructRNA RNA_BuildGpencilModifier;
extern StructRNA RNA_BuildModifier;
extern StructRNA RNA_CacheFile;
extern StructRNA RNA_Camera;
extern StructRNA RNA_CameraDOFSettings;
extern StructRNA RNA_CastModifier;
extern StructRNA RNA_ChildOfConstraint;
extern StructRNA RNA_ChildParticle;
extern StructRNA RNA_ClampToConstraint;
extern StructRNA RNA_ClothCollisionSettings;
extern StructRNA RNA_ClothModifier;
extern StructRNA RNA_ClothSettings;
extern StructRNA RNA_CloudsTexture;
extern StructRNA RNA_Collection;
extern StructRNA RNA_CollectionEngineSettings;
extern StructRNA RNA_CollectionProperty;
extern StructRNA RNA_CollisionModifier;
extern StructRNA RNA_CollisionSensor;
extern StructRNA RNA_CollisionSettings;
extern StructRNA RNA_ColorGpencilModifier;
extern StructRNA RNA_ColorManagedDisplaySettings;
extern StructRNA RNA_ColorManagedInputColorspaceSettings;
extern StructRNA RNA_ColorManagedSequencerColorspaceSettings;
extern StructRNA RNA_ColorManagedViewSettings;
extern StructRNA RNA_ColorMixSequence;
extern StructRNA RNA_ColorRamp;
extern StructRNA RNA_ColorRampElement;
extern StructRNA RNA_ColorSequence;
extern StructRNA RNA_CompositorNode;
extern StructRNA RNA_CompositorNodeAlphaOver;
extern StructRNA RNA_CompositorNodeBilateralblur;
extern StructRNA RNA_CompositorNodeBlur;
extern StructRNA RNA_CompositorNodeBrightContrast;
extern StructRNA RNA_CompositorNodeChannelMatte;
extern StructRNA RNA_CompositorNodeChromaMatte;
extern StructRNA RNA_CompositorNodeColorMatte;
extern StructRNA RNA_CompositorNodeColorSpill;
extern StructRNA RNA_CompositorNodeCombHSVA;
extern StructRNA RNA_CompositorNodeCombRGBA;
extern StructRNA RNA_CompositorNodeCombYCCA;
extern StructRNA RNA_CompositorNodeCombYUVA;
extern StructRNA RNA_CompositorNodeComposite;
extern StructRNA RNA_CompositorNodeCornerPin;
extern StructRNA RNA_CompositorNodeCrop;
extern StructRNA RNA_CompositorNodeCurveRGB;
extern StructRNA RNA_CompositorNodeCurveVec;
extern StructRNA RNA_CompositorNodeDBlur;
extern StructRNA RNA_CompositorNodeDefocus;
extern StructRNA RNA_CompositorNodeDiffMatte;
extern StructRNA RNA_CompositorNodeDilateErode;
extern StructRNA RNA_CompositorNodeDisplace;
extern StructRNA RNA_CompositorNodeDistanceMatte;
extern StructRNA RNA_CompositorNodeDoubleEdgeMask;
extern StructRNA RNA_CompositorNodeFilter;
extern StructRNA RNA_CompositorNodeFlip;
extern StructRNA RNA_CompositorNodeGamma;
extern StructRNA RNA_CompositorNodeGlare;
extern StructRNA RNA_CompositorNodeHueSat;
extern StructRNA RNA_CompositorNodeIDMask;
extern StructRNA RNA_CompositorNodeImage;
extern StructRNA RNA_CompositorNodeInpaint;
extern StructRNA RNA_CompositorNodeInvert;
extern StructRNA RNA_CompositorNodeLensdist;
extern StructRNA RNA_CompositorNodeLevels;
extern StructRNA RNA_CompositorNodeLumaMatte;
extern StructRNA RNA_CompositorNodeMapRange;
extern StructRNA RNA_CompositorNodeMapUV;
extern StructRNA RNA_CompositorNodeMapValue;
extern StructRNA RNA_CompositorNodeMask;
extern StructRNA RNA_CompositorNodeMath;
extern StructRNA RNA_CompositorNodeMixRGB;
extern StructRNA RNA_CompositorNodeNormal;
extern StructRNA RNA_CompositorNodeNormalize;
extern StructRNA RNA_CompositorNodeOutputFile;
extern StructRNA RNA_CompositorNodePremulKey;
extern StructRNA RNA_CompositorNodeRGB;
extern StructRNA RNA_CompositorNodeRGBToBW;
extern StructRNA RNA_CompositorNodeRLayers;
extern StructRNA RNA_CompositorNodeRotate;
extern StructRNA RNA_CompositorNodeScale;
extern StructRNA RNA_CompositorNodeSepHSVA;
extern StructRNA RNA_CompositorNodeSepRGBA;
extern StructRNA RNA_CompositorNodeSepYCCA;
extern StructRNA RNA_CompositorNodeSepYUVA;
extern StructRNA RNA_CompositorNodeSetAlpha;
extern StructRNA RNA_CompositorNodeSplitViewer;
extern StructRNA RNA_CompositorNodeSunBeams;
extern StructRNA RNA_CompositorNodeSwitchView;
extern StructRNA RNA_CompositorNodeTexture;
extern StructRNA RNA_CompositorNodeTime;
extern StructRNA RNA_CompositorNodeTonemap;
extern StructRNA RNA_CompositorNodeTranslate;
extern StructRNA RNA_CompositorNodeTree;
extern StructRNA RNA_CompositorNodeValToRGB;
extern StructRNA RNA_CompositorNodeValue;
extern StructRNA RNA_CompositorNodeVecBlur;
extern StructRNA RNA_CompositorNodeViewer;
extern StructRNA RNA_CompositorNodeZcombine;
extern StructRNA RNA_ConsoleLine;
extern StructRNA RNA_Constraint;
extern StructRNA RNA_ConstraintTarget;
extern StructRNA RNA_ConstraintTargetBone;
extern StructRNA RNA_Context;
extern StructRNA RNA_ControlFluidSettings;
extern StructRNA RNA_Controller;
extern StructRNA RNA_CopyLocationConstraint;
extern StructRNA RNA_CopyRotationConstraint;
extern StructRNA RNA_CopyScaleConstraint;
extern StructRNA RNA_CopyTransformsConstraint;
extern StructRNA RNA_CorrectiveSmoothModifier;
extern StructRNA RNA_Curve;
extern StructRNA RNA_CurveMap;
extern StructRNA RNA_CurveMapPoint;
extern StructRNA RNA_CurveMapping;
extern StructRNA RNA_CurveModifier;
extern StructRNA RNA_CurvePoint;
extern StructRNA RNA_DampedTrackConstraint;
extern StructRNA RNA_DataTransferModifier;
extern StructRNA RNA_DecimateModifier;
extern StructRNA RNA_DelaySensor;
extern StructRNA RNA_Depsgraph;
extern StructRNA RNA_DepsgraphObjectInstance;
extern StructRNA RNA_DepsgraphUpdate;
extern StructRNA RNA_DisplaceModifier;
extern StructRNA RNA_DisplaySafeAreas;
extern StructRNA RNA_DistortedNoiseTexture;
extern StructRNA RNA_DomainFluidSettings;
extern StructRNA RNA_DopeSheet;
extern StructRNA RNA_Driver;
extern StructRNA RNA_DriverTarget;
extern StructRNA RNA_DriverVariable;
extern StructRNA RNA_DupliGpencilModifier;
extern StructRNA RNA_DynamicPaintBrushSettings;
extern StructRNA RNA_DynamicPaintCanvasSettings;
extern StructRNA RNA_DynamicPaintModifier;
extern StructRNA RNA_DynamicPaintSurface;
extern StructRNA RNA_EdgeSplitModifier;
extern StructRNA RNA_EditBone;
extern StructRNA RNA_EffectSequence;
extern StructRNA RNA_EffectorWeights;
extern StructRNA RNA_EnumProperty;
extern StructRNA RNA_EnumPropertyItem;
extern StructRNA RNA_Event;
extern StructRNA RNA_ExplodeModifier;
extern StructRNA RNA_ExpressionController;
extern StructRNA RNA_FCurve;
extern StructRNA RNA_FCurveSample;
extern StructRNA RNA_FFmpegSettings;
extern StructRNA RNA_FModifier;
extern StructRNA RNA_FModifierCycles;
extern StructRNA RNA_FModifierEnvelope;
extern StructRNA RNA_FModifierEnvelopeControlPoint;
extern StructRNA RNA_FModifierFunctionGenerator;
extern StructRNA RNA_FModifierGenerator;
extern StructRNA RNA_FModifierLimits;
extern StructRNA RNA_FModifierNoise;
extern StructRNA RNA_FModifierPython;
extern StructRNA RNA_FModifierStepped;
extern StructRNA RNA_FaceMap;
extern StructRNA RNA_FieldSettings;
extern StructRNA RNA_FileBrowserFSMenuEntry;
extern StructRNA RNA_FileSelectParams;
extern StructRNA RNA_FloatProperty;
extern StructRNA RNA_FloorConstraint;
extern StructRNA RNA_FluidFluidSettings;
extern StructRNA RNA_FluidSettings;
extern StructRNA RNA_FluidSimulationModifier;
extern StructRNA RNA_FollowPathConstraint;
extern StructRNA RNA_FreestyleLineSet;
extern StructRNA RNA_FreestyleLineStyle;
extern StructRNA RNA_FreestyleModuleSettings;
extern StructRNA RNA_FreestyleSettings;
extern StructRNA RNA_Function;
extern StructRNA RNA_GPencilFrame;
extern StructRNA RNA_GPencilInterpolateSettings;
extern StructRNA RNA_GPencilLayer;
extern StructRNA RNA_GPencilSculptBrush;
extern StructRNA RNA_GPencilSculptGuide;
extern StructRNA RNA_GPencilSculptSettings;
extern StructRNA RNA_GPencilStroke;
extern StructRNA RNA_GPencilStrokePoint;
extern StructRNA RNA_GaussianBlurSequence;
extern StructRNA RNA_Gizmo;
extern StructRNA RNA_GizmoGroupProperties;
extern StructRNA RNA_GizmoProperties;
extern StructRNA RNA_GlowSequence;
extern StructRNA RNA_GpencilModifier;
extern StructRNA RNA_GreasePencil;
extern StructRNA RNA_Header;
extern StructRNA RNA_HemiLight;
extern StructRNA RNA_Histogram;
extern StructRNA RNA_HookGpencilModifier;
extern StructRNA RNA_HookModifier;
extern StructRNA RNA_ID;
extern StructRNA RNA_IDOverrideStatic;
extern StructRNA RNA_IDOverrideStaticProperty;
extern StructRNA RNA_IKParam;
extern StructRNA RNA_Image;
extern StructRNA RNA_ImageFormatSettings;
extern StructRNA RNA_ImagePaint;
extern StructRNA RNA_ImagePreview;
extern StructRNA RNA_ImageSequence;
extern StructRNA RNA_ImageTexture;
extern StructRNA RNA_ImageUser;
extern StructRNA RNA_InflowFluidSettings;
extern StructRNA RNA_IntProperty;
extern StructRNA RNA_Itasc;
extern StructRNA RNA_JoystickSensor;
extern StructRNA RNA_Key;
extern StructRNA RNA_KeyConfig;
extern StructRNA RNA_KeyConfigPreferences;
extern StructRNA RNA_KeyMap;
extern StructRNA RNA_KeyMapItem;
extern StructRNA RNA_KeyMapItems;
extern StructRNA RNA_KeyboardSensor;
extern StructRNA RNA_Keyframe;
extern StructRNA RNA_KeyingSet;
extern StructRNA RNA_KeyingSetInfo;
extern StructRNA RNA_KeyingSetPath;
extern StructRNA RNA_KeyingSetsAll;
extern StructRNA RNA_KinematicConstraint;
extern StructRNA RNA_LaplacianDeformModifier;
extern StructRNA RNA_LaplacianSmoothModifier;
extern StructRNA RNA_Lattice;
extern StructRNA RNA_LatticeGpencilModifier;
extern StructRNA RNA_LatticeModifier;
extern StructRNA RNA_LatticePoint;
extern StructRNA RNA_LayerCollection;
extern StructRNA RNA_LayerObjects;
extern StructRNA RNA_Library;
extern StructRNA RNA_Light;
extern StructRNA RNA_LightProbe;
extern StructRNA RNA_LightSkySettings;
extern StructRNA RNA_LightTextureSlot;
extern StructRNA RNA_LimitDistanceConstraint;
extern StructRNA RNA_LimitLocationConstraint;
extern StructRNA RNA_LimitRotationConstraint;
extern StructRNA RNA_LimitScaleConstraint;
extern StructRNA RNA_LineStyleAlphaModifier;
extern StructRNA RNA_LineStyleAlphaModifier_AlongStroke;
extern StructRNA RNA_LineStyleAlphaModifier_CreaseAngle;
extern StructRNA RNA_LineStyleAlphaModifier_Curvature_3D;
extern StructRNA RNA_LineStyleAlphaModifier_DistanceFromCamera;
extern StructRNA RNA_LineStyleAlphaModifier_DistanceFromObject;
extern StructRNA RNA_LineStyleAlphaModifier_Material;
extern StructRNA RNA_LineStyleAlphaModifier_Noise;
extern StructRNA RNA_LineStyleAlphaModifier_Tangent;
extern StructRNA RNA_LineStyleColorModifier;
extern StructRNA RNA_LineStyleColorModifier_AlongStroke;
extern StructRNA RNA_LineStyleColorModifier_CreaseAngle;
extern StructRNA RNA_LineStyleColorModifier_Curvature_3D;
extern StructRNA RNA_LineStyleColorModifier_DistanceFromCamera;
extern StructRNA RNA_LineStyleColorModifier_DistanceFromObject;
extern StructRNA RNA_LineStyleColorModifier_Material;
extern StructRNA RNA_LineStyleColorModifier_Noise;
extern StructRNA RNA_LineStyleColorModifier_Tangent;
extern StructRNA RNA_LineStyleGeometryModifier;
extern StructRNA RNA_LineStyleGeometryModifier_2DOffset;
extern StructRNA RNA_LineStyleGeometryModifier_2DTransform;
extern StructRNA RNA_LineStyleGeometryModifier_BackboneStretcher;
extern StructRNA RNA_LineStyleGeometryModifier_BezierCurve;
extern StructRNA RNA_LineStyleGeometryModifier_Blueprint;
extern StructRNA RNA_LineStyleGeometryModifier_GuidingLines;
extern StructRNA RNA_LineStyleGeometryModifier_PerlinNoise1D;
extern StructRNA RNA_LineStyleGeometryModifier_PerlinNoise2D;
extern StructRNA RNA_LineStyleGeometryModifier_Polygonalization;
extern StructRNA RNA_LineStyleGeometryModifier_Sampling;
extern StructRNA RNA_LineStyleGeometryModifier_Simplification;
extern StructRNA RNA_LineStyleGeometryModifier_SinusDisplacement;
extern StructRNA RNA_LineStyleGeometryModifier_SpatialNoise;
extern StructRNA RNA_LineStyleGeometryModifier_TipRemover;
extern StructRNA RNA_LineStyleModifier;
extern StructRNA RNA_LineStyleTextureSlot;
extern StructRNA RNA_LineStyleThicknessModifier;
extern StructRNA RNA_LineStyleThicknessModifier_AlongStroke;
extern StructRNA RNA_LineStyleThicknessModifier_Calligraphy;
extern StructRNA RNA_LineStyleThicknessModifier_CreaseAngle;
extern StructRNA RNA_LineStyleThicknessModifier_Curvature_3D;
extern StructRNA RNA_LineStyleThicknessModifier_DistanceFromCamera;
extern StructRNA RNA_LineStyleThicknessModifier_DistanceFromObject;
extern StructRNA RNA_LineStyleThicknessModifier_Material;
extern StructRNA RNA_LineStyleThicknessModifier_Noise;
extern StructRNA RNA_LineStyleThicknessModifier_Tangent;
extern StructRNA RNA_LockedTrackConstraint;
extern StructRNA RNA_Macro;
extern StructRNA RNA_MagicTexture;
extern StructRNA RNA_MarbleTexture;
extern StructRNA RNA_Mask;
extern StructRNA RNA_MaskLayer;
extern StructRNA RNA_MaskModifier;
extern StructRNA RNA_MaskSequence;
extern StructRNA RNA_Material;
extern StructRNA RNA_MaterialRaytraceMirror;
extern StructRNA RNA_MaterialSlot;
extern StructRNA RNA_Menu;
extern StructRNA RNA_Mesh;
extern StructRNA RNA_MeshCacheModifier;
extern StructRNA RNA_MeshColor;
extern StructRNA RNA_MeshColorLayer;
extern StructRNA RNA_MeshDeformModifier;
extern StructRNA RNA_MeshEdge;
extern StructRNA RNA_MeshFloatProperty;
extern StructRNA RNA_MeshFloatPropertyLayer;
extern StructRNA RNA_MeshIntProperty;
extern StructRNA RNA_MeshIntPropertyLayer;
extern StructRNA RNA_MeshLoop;
extern StructRNA RNA_MeshLoopColorLayer;
extern StructRNA RNA_MeshLoopTriangle;
extern StructRNA RNA_MeshPolygon;
extern StructRNA RNA_MeshSequenceCacheModifier;
extern StructRNA RNA_MeshSkinVertex;
extern StructRNA RNA_MeshSkinVertexLayer;
extern StructRNA RNA_MeshSticky;
extern StructRNA RNA_MeshStringProperty;
extern StructRNA RNA_MeshStringPropertyLayer;
extern StructRNA RNA_MeshTextureFace;
extern StructRNA RNA_MeshTextureFaceLayer;
extern StructRNA RNA_MeshTexturePoly;
extern StructRNA RNA_MeshTexturePolyLayer;
extern StructRNA RNA_MeshVertex;
extern StructRNA RNA_MessageSensor;
extern StructRNA RNA_MetaBall;
extern StructRNA RNA_MetaElement;
extern StructRNA RNA_MetaSequence;
extern StructRNA RNA_MirrorGpencilModifier;
extern StructRNA RNA_MirrorModifier;
extern StructRNA RNA_Modifier;
extern StructRNA RNA_MotionPath;
extern StructRNA RNA_MotionPathVert;
extern StructRNA RNA_MouseSensor;
extern StructRNA RNA_MovieClipSequence;
extern StructRNA RNA_MovieSequence;
extern StructRNA RNA_MovieTracking;
extern StructRNA RNA_MovieTrackingObject;
extern StructRNA RNA_MovieTrackingStabilization;
extern StructRNA RNA_MovieTrackingTrack;
extern StructRNA RNA_MulticamSequence;
extern StructRNA RNA_MultiresModifier;
extern StructRNA RNA_MusgraveTexture;
extern StructRNA RNA_NandController;
extern StructRNA RNA_NearSensor;
extern StructRNA RNA_NlaStrip;
extern StructRNA RNA_NlaTrack;
extern StructRNA RNA_Node;
extern StructRNA RNA_NodeInstanceHash;
extern StructRNA RNA_NodeLink;
extern StructRNA RNA_NodeOutputFileSlotFile;
extern StructRNA RNA_NodeOutputFileSlotLayer;
extern StructRNA RNA_NodeSocket;
extern StructRNA RNA_NodeSocketInterface;
extern StructRNA RNA_NodeTree;
extern StructRNA RNA_NoiseGpencilModifier;
extern StructRNA RNA_NoiseTexture;
extern StructRNA RNA_NorController;
extern StructRNA RNA_NormalEditModifier;
extern StructRNA RNA_Object;
extern StructRNA RNA_ObjectBase;
extern StructRNA RNA_ObjectDisplay;
extern StructRNA RNA_ObstacleFluidSettings;
extern StructRNA RNA_OceanModifier;
extern StructRNA RNA_OceanTexData;
extern StructRNA RNA_OceanTexture;
extern StructRNA RNA_OffsetGpencilModifier;
extern StructRNA RNA_OpacityGpencilModifier;
extern StructRNA RNA_Operator;
extern StructRNA RNA_OperatorFileListElement;
extern StructRNA RNA_OperatorMacro;
extern StructRNA RNA_OperatorMousePath;
extern StructRNA RNA_OperatorProperties;
extern StructRNA RNA_OperatorStrokeElement;
extern StructRNA RNA_OrController;
extern StructRNA RNA_OutflowFluidSettings;
extern StructRNA RNA_PackedFile;
extern StructRNA RNA_Paint;
extern StructRNA RNA_PaintCurve;
extern StructRNA RNA_PaintToolSlot;
extern StructRNA RNA_Palette;
extern StructRNA RNA_PaletteColor;
extern StructRNA RNA_Panel;
extern StructRNA RNA_Particle;
extern StructRNA RNA_ParticleBrush;
extern StructRNA RNA_ParticleDupliWeight;
extern StructRNA RNA_ParticleEdit;
extern StructRNA RNA_ParticleFluidSettings;
extern StructRNA RNA_ParticleHairKey;
extern StructRNA RNA_ParticleInstanceModifier;
extern StructRNA RNA_ParticleKey;
extern StructRNA RNA_ParticleSettings;
extern StructRNA RNA_ParticleSettingsTextureSlot;
extern StructRNA RNA_ParticleSystem;
extern StructRNA RNA_ParticleSystemModifier;
extern StructRNA RNA_ParticleTarget;
extern StructRNA RNA_PivotConstraint;
extern StructRNA RNA_PointCache;
extern StructRNA RNA_PointLight;
extern StructRNA RNA_PointerProperty;
extern StructRNA RNA_Pose;
extern StructRNA RNA_PoseBone;
extern StructRNA RNA_Preferences;
extern StructRNA RNA_PreferencesEdit;
extern StructRNA RNA_PreferencesFilePaths;
extern StructRNA RNA_PreferencesInput;
extern StructRNA RNA_PreferencesKeymap;
extern StructRNA RNA_PreferencesSystem;
extern StructRNA RNA_PreferencesView;
extern StructRNA RNA_PreferencesWalkNavigation;
extern StructRNA RNA_Property;
extern StructRNA RNA_PropertyGroup;
extern StructRNA RNA_PropertyGroupItem;
extern StructRNA RNA_PropertySensor;
extern StructRNA RNA_PythonConstraint;
extern StructRNA RNA_PythonController;
extern StructRNA RNA_RadarSensor;
extern StructRNA RNA_RandomSensor;
extern StructRNA RNA_RaySensor;
extern StructRNA RNA_Region;
extern StructRNA RNA_RenderEngine;
extern StructRNA RNA_RenderEngineSettings;
extern StructRNA RNA_RenderEngineSettingsClay;
extern StructRNA RNA_RenderLayer;
extern StructRNA RNA_RenderPass;
extern StructRNA RNA_RenderResult;
extern StructRNA RNA_RenderSettings;
extern StructRNA RNA_RigidBodyJointConstraint;
extern StructRNA RNA_RigidBodyObject;
extern StructRNA RNA_RigidBodyWorld;
extern StructRNA RNA_SPHFluidSettings;
extern StructRNA RNA_Scene;
extern StructRNA RNA_SceneDisplay;
extern StructRNA RNA_SceneEEVEE;
extern StructRNA RNA_SceneObjects;
extern StructRNA RNA_SceneRenderLayer;
extern StructRNA RNA_SceneSequence;
extern StructRNA RNA_Scopes;
extern StructRNA RNA_Screen;
extern StructRNA RNA_ScrewModifier;
extern StructRNA RNA_Sculpt;
extern StructRNA RNA_SelectedUvElement;
extern StructRNA RNA_Sensor;
extern StructRNA RNA_Sequence;
extern StructRNA RNA_SequenceColorBalance;
extern StructRNA RNA_SequenceColorBalanceData;
extern StructRNA RNA_SequenceCrop;
extern StructRNA RNA_SequenceEditor;
extern StructRNA RNA_SequenceElement;
extern StructRNA RNA_SequenceModifier;
extern StructRNA RNA_SequenceProxy;
extern StructRNA RNA_SequenceTransform;
extern StructRNA RNA_ShaderFx;
extern StructRNA RNA_ShaderFxBlur;
extern StructRNA RNA_ShaderFxColorize;
extern StructRNA RNA_ShaderFxFlip;
extern StructRNA RNA_ShaderFxGlow;
extern StructRNA RNA_ShaderFxLight;
extern StructRNA RNA_ShaderFxPixel;
extern StructRNA RNA_ShaderFxRim;
extern StructRNA RNA_ShaderFxShadow;
extern StructRNA RNA_ShaderFxSwirl;
extern StructRNA RNA_ShaderFxWave;
extern StructRNA RNA_ShaderNode;
extern StructRNA RNA_ShaderNodeCameraData;
extern StructRNA RNA_ShaderNodeCombineRGB;
extern StructRNA RNA_ShaderNodeExtendedMaterial;
extern StructRNA RNA_ShaderNodeGamma;
extern StructRNA RNA_ShaderNodeGeometry;
extern StructRNA RNA_ShaderNodeHueSaturation;
extern StructRNA RNA_ShaderNodeIESLight;
extern StructRNA RNA_ShaderNodeInvert;
extern StructRNA RNA_ShaderNodeLightData;
extern StructRNA RNA_ShaderNodeMapping;
extern StructRNA RNA_ShaderNodeMaterial;
extern StructRNA RNA_ShaderNodeMath;
extern StructRNA RNA_ShaderNodeMixRGB;
extern StructRNA RNA_ShaderNodeNormal;
extern StructRNA RNA_ShaderNodeOutput;
extern StructRNA RNA_ShaderNodeRGB;
extern StructRNA RNA_ShaderNodeRGBCurve;
extern StructRNA RNA_ShaderNodeRGBToBW;
extern StructRNA RNA_ShaderNodeScript;
extern StructRNA RNA_ShaderNodeSeparateRGB;
extern StructRNA RNA_ShaderNodeSqueeze;
extern StructRNA RNA_ShaderNodeTexture;
extern StructRNA RNA_ShaderNodeTree;
extern StructRNA RNA_ShaderNodeValToRGB;
extern StructRNA RNA_ShaderNodeValue;
extern StructRNA RNA_ShaderNodeVectorCurve;
extern StructRNA RNA_ShaderNodeVectorMath;
extern StructRNA RNA_ShapeKey;
extern StructRNA RNA_ShapeKeyBezierPoint;
extern StructRNA RNA_ShapeKeyCurvePoint;
extern StructRNA RNA_ShapeKeyPoint;
extern StructRNA RNA_ShrinkwrapConstraint;
extern StructRNA RNA_ShrinkwrapModifier;
extern StructRNA RNA_SimpleDeformModifier;
extern StructRNA RNA_SimplifyGpencilModifier;
extern StructRNA RNA_SkinModifier;
extern StructRNA RNA_SmokeCollSettings;
extern StructRNA RNA_SmokeDomainSettings;
extern StructRNA RNA_SmokeFlowSettings;
extern StructRNA RNA_SmokeModifier;
extern StructRNA RNA_SmoothGpencilModifier;
extern StructRNA RNA_SmoothModifier;
extern StructRNA RNA_SoftBodyModifier;
extern StructRNA RNA_SoftBodySettings;
extern StructRNA RNA_SolidifyModifier;
extern StructRNA RNA_Sound;
extern StructRNA RNA_SoundSequence;
extern StructRNA RNA_Space;
extern StructRNA RNA_SpaceClipEditor;
extern StructRNA RNA_SpaceConsole;
extern StructRNA RNA_SpaceDopeSheetEditor;
extern StructRNA RNA_SpaceFileBrowser;
extern StructRNA RNA_SpaceGraphEditor;
extern StructRNA RNA_SpaceImageEditor;
extern StructRNA RNA_SpaceInfo;
extern StructRNA RNA_SpaceNLA;
extern StructRNA RNA_SpaceNodeEditor;
extern StructRNA RNA_SpaceOutliner;
extern StructRNA RNA_SpacePreferences;
extern StructRNA RNA_SpaceProperties;
extern StructRNA RNA_SpaceSequenceEditor;
extern StructRNA RNA_SpaceTextEditor;
extern StructRNA RNA_SpaceUVEditor;
extern StructRNA RNA_SpaceView3D;
extern StructRNA RNA_Speaker;
extern StructRNA RNA_SpeedControlSequence;
extern StructRNA RNA_Spline;
extern StructRNA RNA_SplineIKConstraint;
extern StructRNA RNA_SplinePoint;
extern StructRNA RNA_SpotLight;
extern StructRNA RNA_Stereo3dDisplay;
extern StructRNA RNA_StretchToConstraint;
extern StructRNA RNA_StringProperty;
extern StructRNA RNA_Struct;
extern StructRNA RNA_StucciTexture;
extern StructRNA RNA_StudioLight;
extern StructRNA RNA_SubdivGpencilModifier;
extern StructRNA RNA_SubsurfModifier;
extern StructRNA RNA_SunLight;
extern StructRNA RNA_SurfaceCurve;
extern StructRNA RNA_SurfaceDeformModifier;
extern StructRNA RNA_SurfaceModifier;
extern StructRNA RNA_TexMapping;
extern StructRNA RNA_Text;
extern StructRNA RNA_TextBox;
extern StructRNA RNA_TextCharacterFormat;
extern StructRNA RNA_TextCurve;
extern StructRNA RNA_TextLine;
extern StructRNA RNA_TextSequence;
extern StructRNA RNA_Texture;
extern StructRNA RNA_TextureNode;
extern StructRNA RNA_TextureNodeBricks;
extern StructRNA RNA_TextureNodeChecker;
extern StructRNA RNA_TextureNodeCompose;
extern StructRNA RNA_TextureNodeCoordinates;
extern StructRNA RNA_TextureNodeCurveRGB;
extern StructRNA RNA_TextureNodeCurveTime;
extern StructRNA RNA_TextureNodeDecompose;
extern StructRNA RNA_TextureNodeDistance;
extern StructRNA RNA_TextureNodeHueSaturation;
extern StructRNA RNA_TextureNodeImage;
extern StructRNA RNA_TextureNodeInvert;
extern StructRNA RNA_TextureNodeMath;
extern StructRNA RNA_TextureNodeMixRGB;
extern StructRNA RNA_TextureNodeOutput;
extern StructRNA RNA_TextureNodeRGBToBW;
extern StructRNA RNA_TextureNodeRotate;
extern StructRNA RNA_TextureNodeScale;
extern StructRNA RNA_TextureNodeTexture;
extern StructRNA RNA_TextureNodeTranslate;
extern StructRNA RNA_TextureNodeTree;
extern StructRNA RNA_TextureNodeValToNor;
extern StructRNA RNA_TextureNodeValToRGB;
extern StructRNA RNA_TextureNodeViewer;
extern StructRNA RNA_TextureSlot;
extern StructRNA RNA_Theme;
extern StructRNA RNA_ThemeAudioWindow;
extern StructRNA RNA_ThemeBoneColorSet;
extern StructRNA RNA_ThemeConsole;
extern StructRNA RNA_ThemeDopeSheet;
extern StructRNA RNA_ThemeFileBrowser;
extern StructRNA RNA_ThemeFontStyle;
extern StructRNA RNA_ThemeGraphEditor;
extern StructRNA RNA_ThemeImageEditor;
extern StructRNA RNA_ThemeInfo;
extern StructRNA RNA_ThemeLogicEditor;
extern StructRNA RNA_ThemeNLAEditor;
extern StructRNA RNA_ThemeNodeEditor;
extern StructRNA RNA_ThemeOutliner;
extern StructRNA RNA_ThemePreferences;
extern StructRNA RNA_ThemeProperties;
extern StructRNA RNA_ThemeSequenceEditor;
extern StructRNA RNA_ThemeSpaceGeneric;
extern StructRNA RNA_ThemeSpaceGradient;
extern StructRNA RNA_ThemeSpaceListGeneric;
extern StructRNA RNA_ThemeStyle;
extern StructRNA RNA_ThemeTextEditor;
extern StructRNA RNA_ThemeUserInterface;
extern StructRNA RNA_ThemeView3D;
extern StructRNA RNA_ThemeWidgetColors;
extern StructRNA RNA_ThemeWidgetStateColors;
extern StructRNA RNA_ThickGpencilModifier;
extern StructRNA RNA_TimeGpencilModifier;
extern StructRNA RNA_TimelineMarker;
extern StructRNA RNA_Timer;
extern StructRNA RNA_TintGpencilModifier;
extern StructRNA RNA_ToolSettings;
extern StructRNA RNA_TrackToConstraint;
extern StructRNA RNA_TransformConstraint;
extern StructRNA RNA_TransformOrientationSlot;
extern StructRNA RNA_TransformSequence;
extern StructRNA RNA_UILayout;
extern StructRNA RNA_UIList;
extern StructRNA RNA_UIPieMenu;
extern StructRNA RNA_UIPopupMenu;
extern StructRNA RNA_UVProjectModifier;
extern StructRNA RNA_UVProjector;
extern StructRNA RNA_UVWarpModifier;
extern StructRNA RNA_UnitSettings;
extern StructRNA RNA_UnknownType;
extern StructRNA RNA_UserSolidLight;
extern StructRNA RNA_VectorFont;
extern StructRNA RNA_VertexGroup;
extern StructRNA RNA_VertexGroupElement;
extern StructRNA RNA_VertexPaint;
extern StructRNA RNA_VertexWeightEditModifier;
extern StructRNA RNA_VertexWeightMixModifier;
extern StructRNA RNA_VertexWeightProximityModifier;
extern StructRNA RNA_View3DCursor;
extern StructRNA RNA_View3DOverlay;
extern StructRNA RNA_View3DShading;
extern StructRNA RNA_ViewLayer;
extern StructRNA RNA_VoronoiTexture;
extern StructRNA RNA_WarpModifier;
extern StructRNA RNA_WaveModifier;
extern StructRNA RNA_WeightedNormalModifier;
extern StructRNA RNA_Window;
extern StructRNA RNA_WindowManager;
extern StructRNA RNA_WipeSequence;
extern StructRNA RNA_WireframeModifier;
extern StructRNA RNA_WoodTexture;
extern StructRNA RNA_WorkSpace;
extern StructRNA RNA_World;
extern StructRNA RNA_WorldAmbientOcclusion;
extern StructRNA RNA_WorldLighting;
extern StructRNA RNA_WorldMistSettings;
extern StructRNA RNA_WorldTextureSlot;
extern StructRNA RNA_XnorController;
extern StructRNA RNA_XorController;
extern StructRNA RNA_uiPopover;
extern StructRNA RNA_wmOwnerIDs;

/* Pointer
 *
 * These functions will fill in RNA pointers, this can be done in three ways:
 * - a pointer Main is created by just passing the data pointer
 * - a pointer to a datablock can be created with the type and id data pointer
 * - a pointer to data contained in a datablock can be created with the id type
 *   and id data pointer, and the data type and pointer to the struct itself.
 *
 * There is also a way to get a pointer with the information about all structs.
 */

void RNA_main_pointer_create(struct Main *main, PointerRNA *r_ptr);
void RNA_id_pointer_create(struct ID *id, PointerRNA *r_ptr);
void RNA_pointer_create(struct ID *id, StructRNA *type, void *data, PointerRNA *r_ptr);
bool RNA_pointer_is_null(const PointerRNA *ptr);

bool RNA_path_resolved_create(PointerRNA *ptr,
                              struct PropertyRNA *prop,
                              const int prop_index,
                              PathResolvedRNA *r_anim_rna);

void RNA_blender_rna_pointer_create(PointerRNA *r_ptr);
void RNA_pointer_recast(PointerRNA *ptr, PointerRNA *r_ptr);

extern const PointerRNA PointerRNA_NULL;

/* Structs */

StructRNA *RNA_struct_find(const char *identifier);

const char *RNA_struct_identifier(const StructRNA *type);
const char *RNA_struct_ui_name(const StructRNA *type);
const char *RNA_struct_ui_name_raw(const StructRNA *type);
const char *RNA_struct_ui_description(const StructRNA *type);
const char *RNA_struct_ui_description_raw(const StructRNA *type);
const char *RNA_struct_translation_context(const StructRNA *type);
int RNA_struct_ui_icon(const StructRNA *type);

PropertyRNA *RNA_struct_name_property(const StructRNA *type);
const EnumPropertyItem *RNA_struct_property_tag_defines(const StructRNA *type);
PropertyRNA *RNA_struct_iterator_property(StructRNA *type);
StructRNA *RNA_struct_base(StructRNA *type);
const StructRNA *RNA_struct_base_child_of(const StructRNA *type, const StructRNA *parent_type);

bool RNA_struct_is_ID(const StructRNA *type);
bool RNA_struct_is_a(const StructRNA *type, const StructRNA *srna);

bool RNA_struct_undo_check(const StructRNA *type);

StructRegisterFunc RNA_struct_register(StructRNA *type);
StructUnregisterFunc RNA_struct_unregister(StructRNA *type);
void **RNA_struct_instance(PointerRNA *ptr);

void *RNA_struct_py_type_get(StructRNA *srna);
void RNA_struct_py_type_set(StructRNA *srna, void *py_type);

void *RNA_struct_blender_type_get(StructRNA *srna);
void RNA_struct_blender_type_set(StructRNA *srna, void *blender_type);

struct IDProperty *RNA_struct_idprops(PointerRNA *ptr, bool create);
bool RNA_struct_idprops_check(StructRNA *srna);
bool RNA_struct_idprops_register_check(const StructRNA *type);
bool RNA_struct_idprops_datablock_allowed(const StructRNA *type);
bool RNA_struct_idprops_contains_datablock(const StructRNA *type);
bool RNA_struct_idprops_unset(PointerRNA *ptr, const char *identifier);

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier);
bool RNA_struct_contains_property(PointerRNA *ptr, PropertyRNA *prop_test);
unsigned int RNA_struct_count_properties(StructRNA *srna);

/* lower level functions for access to type properties */
const struct ListBase *RNA_struct_type_properties(StructRNA *srna);
PropertyRNA *RNA_struct_type_find_property(StructRNA *srna, const char *identifier);

FunctionRNA *RNA_struct_find_function(StructRNA *srna, const char *identifier);
const struct ListBase *RNA_struct_type_functions(StructRNA *srna);

char *RNA_struct_name_get_alloc(PointerRNA *ptr, char *fixedbuf, int fixedlen, int *r_len);

bool RNA_struct_available_or_report(struct ReportList *reports, const char *identifier);
bool RNA_struct_bl_idname_ok_or_report(struct ReportList *reports,
                                       const char *identifier,
                                       const char *sep);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

const char *RNA_property_identifier(const PropertyRNA *prop);
const char *RNA_property_description(PropertyRNA *prop);

PropertyType RNA_property_type(PropertyRNA *prop);
PropertySubType RNA_property_subtype(PropertyRNA *prop);
PropertyUnit RNA_property_unit(PropertyRNA *prop);
int RNA_property_flag(PropertyRNA *prop);
int RNA_property_override_flag(PropertyRNA *prop);
int RNA_property_tags(PropertyRNA *prop);
bool RNA_property_builtin(PropertyRNA *prop);
void *RNA_property_py_data_get(PropertyRNA *prop);

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_array_check(PropertyRNA *prop);
int RNA_property_multi_array_length(PointerRNA *ptr, PropertyRNA *prop, int dimension);
int RNA_property_array_dimension(PointerRNA *ptr, PropertyRNA *prop, int length[]);
char RNA_property_array_item_char(PropertyRNA *prop, int index);
int RNA_property_array_item_index(PropertyRNA *prop, char name);

int RNA_property_string_maxlength(PropertyRNA *prop);

const char *RNA_property_ui_name(PropertyRNA *prop);
const char *RNA_property_ui_name_raw(PropertyRNA *prop);
const char *RNA_property_ui_description(PropertyRNA *prop);
const char *RNA_property_ui_description_raw(PropertyRNA *prop);
const char *RNA_property_translation_context(PropertyRNA *prop);
int RNA_property_ui_icon(PropertyRNA *prop);

/* Dynamic Property Information */

void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(
    PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PointerRNA *ptr,
                                 PropertyRNA *prop,
                                 float *softmin,
                                 float *softmax,
                                 float *step,
                                 float *precision);

int RNA_property_float_clamp(PointerRNA *ptr, PropertyRNA *prop, float *value);
int RNA_property_int_clamp(PointerRNA *ptr, PropertyRNA *prop, int *value);

bool RNA_enum_identifier(const EnumPropertyItem *item, const int value, const char **identifier);
int RNA_enum_bitflag_identifiers(const EnumPropertyItem *item,
                                 const int value,
                                 const char **identifier);
bool RNA_enum_name(const EnumPropertyItem *item, const int value, const char **r_name);
bool RNA_enum_description(const EnumPropertyItem *item, const int value, const char **description);
int RNA_enum_from_value(const EnumPropertyItem *item, const int value);
int RNA_enum_from_identifier(const EnumPropertyItem *item, const char *identifier);
int RNA_enum_from_name(const EnumPropertyItem *item, const char *name);
unsigned int RNA_enum_items_count(const EnumPropertyItem *item);

void RNA_property_enum_items_ex(struct bContext *C,
                                PointerRNA *ptr,
                                PropertyRNA *prop,
                                const bool use_static,
                                const EnumPropertyItem **r_item,
                                int *r_totitem,
                                bool *r_free);
void RNA_property_enum_items(struct bContext *C,
                             PointerRNA *ptr,
                             PropertyRNA *prop,
                             const EnumPropertyItem **r_item,
                             int *r_totitem,
                             bool *r_free);
void RNA_property_enum_items_gettexted(struct bContext *C,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const EnumPropertyItem **r_item,
                                       int *r_totitem,
                                       bool *r_free);
void RNA_property_enum_items_gettexted_all(struct bContext *C,
                                           PointerRNA *ptr,
                                           PropertyRNA *prop,
                                           const EnumPropertyItem **r_item,
                                           int *r_totitem,
                                           bool *r_free);
bool RNA_property_enum_value(
    struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *r_value);
bool RNA_property_enum_identifier(struct bContext *C,
                                  PointerRNA *ptr,
                                  PropertyRNA *prop,
                                  const int value,
                                  const char **identifier);
bool RNA_property_enum_name(
    struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **name);
bool RNA_property_enum_name_gettexted(
    struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **name);

bool RNA_property_enum_item_from_value(struct bContext *C,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const int value,
                                       EnumPropertyItem *r_item);
bool RNA_property_enum_item_from_value_gettexted(struct bContext *C,
                                                 PointerRNA *ptr,
                                                 PropertyRNA *prop,
                                                 const int value,
                                                 EnumPropertyItem *r_item);

int RNA_property_enum_bitflag_identifiers(struct bContext *C,
                                          PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          const int value,
                                          const char **identifier);

StructRNA *RNA_property_pointer_type(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_pointer_poll(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *value);

bool RNA_property_editable(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_editable_info(PointerRNA *ptr, PropertyRNA *prop, const char **r_info);
bool RNA_property_editable_index(PointerRNA *ptr, PropertyRNA *prop, int index);
bool RNA_property_editable_flag(PointerRNA *ptr,
                                PropertyRNA *prop); /* without lib check, only checks the flag */
bool RNA_property_animateable(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_animated(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_overridable_get(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_overridable_static_set(PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         const bool is_overridable);
bool RNA_property_overridden(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_comparable(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_path_from_ID_check(PointerRNA *ptr, PropertyRNA *prop); /* slow, use with care */

void RNA_property_update(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_update_main(struct Main *bmain,
                              struct Scene *scene,
                              PointerRNA *ptr,
                              PropertyRNA *prop);
bool RNA_property_update_check(struct PropertyRNA *prop);

void RNA_property_update_cache_add(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_update_cache_flush(struct Main *bmain, struct Scene *scene);
void RNA_property_update_cache_free(void);

/* Property Data */

bool RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, bool value);
void RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, bool *values);
bool RNA_property_boolean_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, const bool *values);
void RNA_property_boolean_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, bool value);
bool RNA_property_boolean_get_default(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_get_default_array(PointerRNA *ptr, PropertyRNA *prop, bool *values);
bool RNA_property_boolean_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value);
void RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
void RNA_property_int_get_array_range(PointerRNA *ptr, PropertyRNA *prop, int values[2]);
int RNA_property_int_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
void RNA_property_int_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);
int RNA_property_int_get_default(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_int_set_default(PointerRNA *ptr, PropertyRNA *prop, int value);
void RNA_property_int_get_default_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
int RNA_property_int_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value);
void RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
void RNA_property_float_get_array_range(PointerRNA *ptr, PropertyRNA *prop, float values[2]);
float RNA_property_float_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, const float *values);
void RNA_property_float_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, float value);
float RNA_property_float_get_default(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_float_set_default(PointerRNA *ptr, PropertyRNA *prop, float value);
void RNA_property_float_get_default_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
float RNA_property_float_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_alloc(
    PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen, int *r_len);
void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value);
void RNA_property_string_set_bytes(PointerRNA *ptr, PropertyRNA *prop, const char *value, int len);
int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_string_get_default(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_default_alloc(PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            char *fixedbuf,
                                            int fixedlen);
int RNA_property_string_default_length(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value);
int RNA_property_enum_get_default(PointerRNA *ptr, PropertyRNA *prop);
void *RNA_property_enum_py_data_get(PropertyRNA *prop);
int RNA_property_enum_step(
    const struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, int from_value, int step);

PointerRNA RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_pointer_set(struct ReportList *reports,
                              PointerRNA *ptr,
                              PropertyRNA *prop,
                              PointerRNA ptr_value);
PointerRNA RNA_property_pointer_get_default(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_collection_begin(PointerRNA *ptr,
                                   PropertyRNA *prop,
                                   CollectionPropertyIterator *iter);
void RNA_property_collection_next(CollectionPropertyIterator *iter);
void RNA_property_collection_skip(CollectionPropertyIterator *iter, int num);
void RNA_property_collection_end(CollectionPropertyIterator *iter);
int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_collection_lookup_index(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *t_ptr);
int RNA_property_collection_lookup_int(PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       int key,
                                       PointerRNA *r_ptr);
int RNA_property_collection_lookup_string(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          const char *key,
                                          PointerRNA *r_ptr);
int RNA_property_collection_assign_int(PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const int key,
                                       const PointerRNA *assign_ptr);
bool RNA_property_collection_type_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);

/* efficient functions to set properties for arrays */
int RNA_property_collection_raw_array(PointerRNA *ptr,
                                      PropertyRNA *prop,
                                      PropertyRNA *itemprop,
                                      RawArray *array);
int RNA_property_collection_raw_get(struct ReportList *reports,
                                    PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    const char *propname,
                                    void *array,
                                    RawPropertyType type,
                                    int len);
int RNA_property_collection_raw_set(struct ReportList *reports,
                                    PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    const char *propname,
                                    void *array,
                                    RawPropertyType type,
                                    int len);
int RNA_raw_type_sizeof(RawPropertyType type);
RawPropertyType RNA_property_raw_type(PropertyRNA *prop);

/* to create ID property groups */
void RNA_property_pointer_add(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_pointer_remove(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_collection_add(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);
bool RNA_property_collection_remove(PointerRNA *ptr, PropertyRNA *prop, int key);
void RNA_property_collection_clear(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_collection_move(PointerRNA *ptr, PropertyRNA *prop, int key, int pos);

/* copy/reset */
bool RNA_property_copy(
    struct Main *bmain, PointerRNA *ptr, PointerRNA *fromptr, PropertyRNA *prop, int index);
bool RNA_property_reset(PointerRNA *ptr, PropertyRNA *prop, int index);
bool RNA_property_assign_default(PointerRNA *ptr, PropertyRNA *prop);

/* Path
 *
 * Experimental method to refer to structs and properties with a string,
 * using a syntax like: scenes[0].objects["Cube"].data.verts[7].co
 *
 * This provides a way to refer to RNA data while being detached from any
 * particular pointers, which is useful in a number of applications, like
 * UI code or Actions, though efficiency is a concern. */

char *RNA_path_append(
    const char *path, PointerRNA *ptr, PropertyRNA *prop, int intkey, const char *strkey);
char *RNA_path_back(const char *path);

/* path_resolve() variants only ensure that a valid pointer (and optionally property) exist */
bool RNA_path_resolve(PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop);

bool RNA_path_resolve_full(
    PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index);

/* path_resolve_property() variants ensure that pointer + property both exist */
bool RNA_path_resolve_property(PointerRNA *ptr,
                               const char *path,
                               PointerRNA *r_ptr,
                               PropertyRNA **r_prop);

bool RNA_path_resolve_property_full(
    PointerRNA *ptr, const char *path, PointerRNA *r_ptr, PropertyRNA **r_prop, int *r_index);

/* path_resolve_property_and_item_pointer() variants ensure that pointer + property both exist,
 * and resolve last Pointer value if possible (Pointer prop or item of a Collection prop). */
bool RNA_path_resolve_property_and_item_pointer(PointerRNA *ptr,
                                                const char *path,
                                                PointerRNA *r_ptr,
                                                PropertyRNA **r_prop,
                                                PointerRNA *r_item_ptr);

bool RNA_path_resolve_property_and_item_pointer_full(PointerRNA *ptr,
                                                     const char *path,
                                                     PointerRNA *r_ptr,
                                                     PropertyRNA **r_prop,
                                                     int *r_index,
                                                     PointerRNA *r_item_ptr);

typedef struct PropertyElemRNA PropertyElemRNA;
struct PropertyElemRNA {
  PropertyElemRNA *next, *prev;
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
};
bool RNA_path_resolve_elements(PointerRNA *ptr, const char *path, struct ListBase *r_elements);

char *RNA_path_from_ID_to_struct(PointerRNA *ptr);
char *RNA_path_from_ID_to_property(PointerRNA *ptr, PropertyRNA *prop);
char *RNA_path_from_ID_to_property_index(PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         int array_dim,
                                         int index);

char *RNA_path_resolve_from_type_to_property(struct PointerRNA *ptr,
                                             struct PropertyRNA *prop,
                                             const struct StructRNA *type);

char *RNA_path_full_ID_py(struct ID *id);
char *RNA_path_full_struct_py(struct PointerRNA *ptr);
char *RNA_path_full_property_py_ex(PointerRNA *ptr,
                                   PropertyRNA *prop,
                                   int index,
                                   bool use_fallback);
char *RNA_path_full_property_py(struct PointerRNA *ptr, struct PropertyRNA *prop, int index);
char *RNA_path_struct_property_py(struct PointerRNA *ptr, struct PropertyRNA *prop, int index);
char *RNA_path_property_py(struct PointerRNA *ptr, struct PropertyRNA *prop, int index);

/* Quick name based property access
 *
 * These are just an easier way to access property values without having to
 * call RNA_struct_find_property. The names have to exist as RNA properties
 * for the type in the pointer, if they do not exist an error will be printed.
 *
 * There is no support for pointers and collections here yet, these can be
 * added when ID properties support them. */

bool RNA_boolean_get(PointerRNA *ptr, const char *name);
void RNA_boolean_set(PointerRNA *ptr, const char *name, bool value);
void RNA_boolean_get_array(PointerRNA *ptr, const char *name, bool *values);
void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const bool *values);

int RNA_int_get(PointerRNA *ptr, const char *name);
void RNA_int_set(PointerRNA *ptr, const char *name, int value);
void RNA_int_get_array(PointerRNA *ptr, const char *name, int *values);
void RNA_int_set_array(PointerRNA *ptr, const char *name, const int *values);

float RNA_float_get(PointerRNA *ptr, const char *name);
void RNA_float_set(PointerRNA *ptr, const char *name, float value);
void RNA_float_get_array(PointerRNA *ptr, const char *name, float *values);
void RNA_float_set_array(PointerRNA *ptr, const char *name, const float *values);

int RNA_enum_get(PointerRNA *ptr, const char *name);
void RNA_enum_set(PointerRNA *ptr, const char *name, int value);
void RNA_enum_set_identifier(struct bContext *C,
                             PointerRNA *ptr,
                             const char *name,
                             const char *id);
bool RNA_enum_is_equal(struct bContext *C,
                       PointerRNA *ptr,
                       const char *name,
                       const char *enumname);

/* lower level functions that don't use a PointerRNA */
bool RNA_enum_value_from_id(const EnumPropertyItem *item, const char *identifier, int *r_value);
bool RNA_enum_id_from_value(const EnumPropertyItem *item, int value, const char **r_identifier);
bool RNA_enum_icon_from_value(const EnumPropertyItem *item, int value, int *r_icon);
bool RNA_enum_name_from_value(const EnumPropertyItem *item, int value, const char **r_name);

void RNA_string_get(PointerRNA *ptr, const char *name, char *value);
char *RNA_string_get_alloc(PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen);
int RNA_string_length(PointerRNA *ptr, const char *name);
void RNA_string_set(PointerRNA *ptr, const char *name, const char *value);

/**
 * Retrieve the named property from PointerRNA.
 */
PointerRNA RNA_pointer_get(PointerRNA *ptr, const char *name);
/* Set the property name of PointerRNA ptr to ptr_value */
void RNA_pointer_set(PointerRNA *ptr, const char *name, PointerRNA ptr_value);
void RNA_pointer_add(PointerRNA *ptr, const char *name);

void RNA_collection_begin(PointerRNA *ptr, const char *name, CollectionPropertyIterator *iter);
int RNA_collection_length(PointerRNA *ptr, const char *name);
void RNA_collection_add(PointerRNA *ptr, const char *name, PointerRNA *r_value);
void RNA_collection_clear(PointerRNA *ptr, const char *name);

#define RNA_BEGIN(sptr, itemptr, propname) \
  { \
    CollectionPropertyIterator rna_macro_iter; \
    for (RNA_collection_begin(sptr, propname, &rna_macro_iter); rna_macro_iter.valid; \
         RNA_property_collection_next(&rna_macro_iter)) { \
      PointerRNA itemptr = rna_macro_iter.ptr;

#define RNA_END \
  } \
  RNA_property_collection_end(&rna_macro_iter); \
  } \
  ((void)0)

#define RNA_PROP_BEGIN(sptr, itemptr, prop) \
  { \
    CollectionPropertyIterator rna_macro_iter; \
    for (RNA_property_collection_begin(sptr, prop, &rna_macro_iter); rna_macro_iter.valid; \
         RNA_property_collection_next(&rna_macro_iter)) { \
      PointerRNA itemptr = rna_macro_iter.ptr;

#define RNA_PROP_END \
  } \
  RNA_property_collection_end(&rna_macro_iter); \
  } \
  ((void)0)

#define RNA_STRUCT_BEGIN(sptr, prop) \
  { \
    CollectionPropertyIterator rna_macro_iter; \
    for (RNA_property_collection_begin( \
             sptr, RNA_struct_iterator_property((sptr)->type), &rna_macro_iter); \
         rna_macro_iter.valid; \
         RNA_property_collection_next(&rna_macro_iter)) { \
      PropertyRNA *prop = (PropertyRNA *)rna_macro_iter.ptr.data;

#define RNA_STRUCT_END \
  } \
  RNA_property_collection_end(&rna_macro_iter); \
  } \
  ((void)0)

/* check if the idproperty exists, for operators */
bool RNA_property_is_set_ex(PointerRNA *ptr, PropertyRNA *prop, bool use_ghost);
bool RNA_property_is_set(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_unset(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_struct_property_is_set_ex(PointerRNA *ptr, const char *identifier, bool use_ghost);
bool RNA_struct_property_is_set(PointerRNA *ptr, const char *identifier);
bool RNA_property_is_idprop(const PropertyRNA *prop);
bool RNA_property_is_unlink(PropertyRNA *prop);
void RNA_struct_property_unset(PointerRNA *ptr, const char *identifier);

/* python compatible string representation of this property, (must be freed!) */
char *RNA_property_as_string(
    struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index, int max_prop_length);
char *RNA_pointer_as_string_id(struct bContext *C, PointerRNA *ptr);
char *RNA_pointer_as_string(struct bContext *C,
                            PointerRNA *ptr,
                            PropertyRNA *prop_ptr,
                            PointerRNA *ptr_prop);
char *RNA_pointer_as_string_keywords_ex(struct bContext *C,
                                        PointerRNA *ptr,
                                        const bool skip_optional_value,
                                        const bool all_args,
                                        const bool nested_args,
                                        const int max_prop_length,
                                        PropertyRNA *iterprop);
char *RNA_pointer_as_string_keywords(struct bContext *C,
                                     PointerRNA *ptr,
                                     const bool skip_optional_value,
                                     const bool all_args,
                                     const bool nested_args,
                                     const int max_prop_length);
char *RNA_function_as_string_keywords(struct bContext *C,
                                      FunctionRNA *func,
                                      const bool as_function,
                                      const bool all_args,
                                      const int max_prop_length);

/* Function */

const char *RNA_function_identifier(FunctionRNA *func);
const char *RNA_function_ui_description(FunctionRNA *func);
const char *RNA_function_ui_description_raw(FunctionRNA *func);
int RNA_function_flag(FunctionRNA *func);
int RNA_function_defined(FunctionRNA *func);

PropertyRNA *RNA_function_get_parameter(PointerRNA *ptr, FunctionRNA *func, int index);
PropertyRNA *RNA_function_find_parameter(PointerRNA *ptr,
                                         FunctionRNA *func,
                                         const char *identifier);
const struct ListBase *RNA_function_defined_parameters(FunctionRNA *func);

/* Utility */

int RNA_parameter_flag(PropertyRNA *prop);

ParameterList *RNA_parameter_list_create(ParameterList *parms, PointerRNA *ptr, FunctionRNA *func);
void RNA_parameter_list_free(ParameterList *parms);
int RNA_parameter_list_size(ParameterList *parms);
int RNA_parameter_list_arg_count(ParameterList *parms);
int RNA_parameter_list_ret_count(ParameterList *parms);

void RNA_parameter_list_begin(ParameterList *parms, ParameterIterator *iter);
void RNA_parameter_list_next(ParameterIterator *iter);
void RNA_parameter_list_end(ParameterIterator *iter);

void RNA_parameter_get(ParameterList *parms, PropertyRNA *parm, void **value);
void RNA_parameter_get_lookup(ParameterList *parms, const char *identifier, void **value);
void RNA_parameter_set(ParameterList *parms, PropertyRNA *parm, const void *value);
void RNA_parameter_set_lookup(ParameterList *parms, const char *identifier, const void *value);
/* Only for PROP_DYNAMIC properties! */
int RNA_parameter_dynamic_length_get(ParameterList *parms, PropertyRNA *parm);
int RNA_parameter_dynamic_length_get_data(ParameterList *parms, PropertyRNA *parm, void *data);
void RNA_parameter_dynamic_length_set(ParameterList *parms, PropertyRNA *parm, int length);
void RNA_parameter_dynamic_length_set_data(ParameterList *parms,
                                           PropertyRNA *parm,
                                           void *data,
                                           int length);

int RNA_function_call(struct bContext *C,
                      struct ReportList *reports,
                      PointerRNA *ptr,
                      FunctionRNA *func,
                      ParameterList *parms);
int RNA_function_call_lookup(struct bContext *C,
                             struct ReportList *reports,
                             PointerRNA *ptr,
                             const char *identifier,
                             ParameterList *parms);

int RNA_function_call_direct(struct bContext *C,
                             struct ReportList *reports,
                             PointerRNA *ptr,
                             FunctionRNA *func,
                             const char *format,
                             ...) ATTR_PRINTF_FORMAT(5, 6);
int RNA_function_call_direct_lookup(struct bContext *C,
                                    struct ReportList *reports,
                                    PointerRNA *ptr,
                                    const char *identifier,
                                    const char *format,
                                    ...) ATTR_PRINTF_FORMAT(5, 6);
int RNA_function_call_direct_va(struct bContext *C,
                                struct ReportList *reports,
                                PointerRNA *ptr,
                                FunctionRNA *func,
                                const char *format,
                                va_list args);
int RNA_function_call_direct_va_lookup(struct bContext *C,
                                       struct ReportList *reports,
                                       PointerRNA *ptr,
                                       const char *identifier,
                                       const char *format,
                                       va_list args);

const char *RNA_translate_ui_text(const char *text,
                                  const char *text_ctxt,
                                  struct StructRNA *type,
                                  struct PropertyRNA *prop,
                                  int translate);

/* ID */

short RNA_type_to_ID_code(const StructRNA *type);
StructRNA *ID_code_to_RNA_type(short idcode);

#define RNA_POINTER_INVALIDATE(ptr) \
  { \
    /* this is checked for validity */ \
    (ptr)->type = /* should not be needed but prevent bad pointer access, just in case */ \
        (ptr)->id.data = NULL; \
  } \
  (void)0

/* macro which inserts the function name */
#if defined __GNUC__
#  define RNA_warning(format, args...) _RNA_warning("%s: " format "\n", __func__, ##args)
#else
#  define RNA_warning(format, ...) _RNA_warning("%s: " format "\n", __FUNCTION__, __VA_ARGS__)
#endif

void _RNA_warning(const char *format, ...) ATTR_PRINTF_FORMAT(1, 2);

/* Equals test. */

/* Note: In practice, EQ_STRICT and EQ_COMPARE have same behavior currently,
 * and will yield same result. */
typedef enum eRNACompareMode {
  /* Only care about equality, not full comparison. */
  RNA_EQ_STRICT,           /* set/unset ignored */
  RNA_EQ_UNSET_MATCH_ANY,  /* unset property matches anything */
  RNA_EQ_UNSET_MATCH_NONE, /* unset property never matches set property */
  /* Full comparison. */
  RNA_EQ_COMPARE,
} eRNACompareMode;

bool RNA_property_equals(struct Main *bmain,
                         struct PointerRNA *ptr_a,
                         struct PointerRNA *ptr_b,
                         struct PropertyRNA *prop,
                         eRNACompareMode mode);
bool RNA_struct_equals(struct Main *bmain,
                       struct PointerRNA *ptr_a,
                       struct PointerRNA *ptr_b,
                       eRNACompareMode mode);

/* Override. */

/* flags for RNA_struct_override_matches. */
typedef enum eRNAOverrideMatch {
  /* Do not compare properties that are not overridable. */
  RNA_OVERRIDE_COMPARE_IGNORE_NON_OVERRIDABLE = 1 << 0,
  /* Do not compare properties that are already overridden. */
  RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN = 1 << 1,

  /* Create new property override if needed and possible. */
  RNA_OVERRIDE_COMPARE_CREATE = 1 << 16,
  /* Restore property's value(s) to reference ones if needed and possible. */
  RNA_OVERRIDE_COMPARE_RESTORE = 1 << 17,
} eRNAOverrideMatch;

typedef enum eRNAOverrideMatchResult {
  /* Some new property overrides were created to take into account
   * differences between local and reference. */
  RNA_OVERRIDE_MATCH_RESULT_CREATED = 1 << 0,
  /* Some properties were reset to reference values. */
  RNA_OVERRIDE_MATCH_RESULT_RESTORED = 1 << 1,
} eRNAOverrideMatchResult;

typedef enum eRNAOverrideStatus {
  RNA_OVERRIDE_STATUS_OVERRIDABLE = 1 << 0, /* The property is overridable. */
  RNA_OVERRIDE_STATUS_OVERRIDDEN = 1 << 1,  /* The property is overridden. */
  RNA_OVERRIDE_STATUS_MANDATORY =
      1 << 2, /* Overriding this property is mandatory when creating an override. */
  RNA_OVERRIDE_STATUS_LOCKED = 1 << 3, /* The override status of this property is locked. */
} eRNAOverrideStatus;

bool RNA_struct_override_matches(struct Main *bmain,
                                 struct PointerRNA *ptr_local,
                                 struct PointerRNA *ptr_reference,
                                 const char *root_path,
                                 struct IDOverrideStatic *override,
                                 const eRNAOverrideMatch flags,
                                 eRNAOverrideMatchResult *r_report_flags);

bool RNA_struct_override_store(struct Main *bmain,
                               struct PointerRNA *ptr_local,
                               struct PointerRNA *ptr_reference,
                               PointerRNA *ptr_storage,
                               struct IDOverrideStatic *override);

void RNA_struct_override_apply(struct Main *bmain,
                               struct PointerRNA *ptr_local,
                               struct PointerRNA *ptr_override,
                               struct PointerRNA *ptr_storage,
                               struct IDOverrideStatic *override);

struct IDOverrideStaticProperty *RNA_property_override_property_find(PointerRNA *ptr,
                                                                     PropertyRNA *prop);
struct IDOverrideStaticProperty *RNA_property_override_property_get(PointerRNA *ptr,
                                                                    PropertyRNA *prop,
                                                                    bool *r_created);

struct IDOverrideStaticPropertyOperation *RNA_property_override_property_operation_find(
    PointerRNA *ptr, PropertyRNA *prop, const int index, const bool strict, bool *r_strict);
struct IDOverrideStaticPropertyOperation *RNA_property_override_property_operation_get(
    PointerRNA *ptr,
    PropertyRNA *prop,
    const short operation,
    const int index,
    const bool strict,
    bool *r_strict,
    bool *r_created);

eRNAOverrideStatus RNA_property_static_override_status(PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       const int index);

void RNA_struct_state_owner_set(const char *name);
const char *RNA_struct_state_owner_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __RNA_ACCESS_H__ */
