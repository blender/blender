/*
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RNA_ACCESS_H
#define RNA_ACCESS_H

/** \file RNA_access.h
 *  \ingroup RNA
 */

#include <stdarg.h>

#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct ID;
struct ListBase;
struct Main;
struct ReportList;
struct Scene;

/* Types */

extern BlenderRNA BLENDER_RNA;
extern LIBEXPORT StructRNA RNA_Action;
extern LIBEXPORT StructRNA RNA_ActionConstraint;
extern LIBEXPORT StructRNA RNA_ActionGroup;
extern LIBEXPORT StructRNA RNA_Actuator;
extern LIBEXPORT StructRNA RNA_ActuatorSensor;
extern LIBEXPORT StructRNA RNA_Addon;
extern LIBEXPORT StructRNA RNA_AlwaysSensor;
extern LIBEXPORT StructRNA RNA_AndController;
extern LIBEXPORT StructRNA RNA_AnimData;
extern LIBEXPORT StructRNA RNA_AnimViz;
extern LIBEXPORT StructRNA RNA_AnimVizMotionPaths;
extern LIBEXPORT StructRNA RNA_AnimVizOnionSkinning;
extern LIBEXPORT StructRNA RNA_AnyType;
extern LIBEXPORT StructRNA RNA_Area;
extern LIBEXPORT StructRNA RNA_AreaLamp;
extern LIBEXPORT StructRNA RNA_Armature;
extern LIBEXPORT StructRNA RNA_ArmatureModifier;
extern LIBEXPORT StructRNA RNA_ArmatureSensor;
extern LIBEXPORT StructRNA RNA_ArrayModifier;
extern LIBEXPORT StructRNA RNA_BackgroundImage;
extern LIBEXPORT StructRNA RNA_BevelModifier;
extern LIBEXPORT StructRNA RNA_BezierSplinePoint;
extern LIBEXPORT StructRNA RNA_BlendData;
extern LIBEXPORT StructRNA RNA_BlendTexture;
extern LIBEXPORT StructRNA RNA_BlenderRNA;
extern LIBEXPORT StructRNA RNA_BoidRule;
extern LIBEXPORT StructRNA RNA_BoidRuleAverageSpeed;
extern LIBEXPORT StructRNA RNA_BoidRuleAvoid;
extern LIBEXPORT StructRNA RNA_BoidRuleAvoidCollision;
extern LIBEXPORT StructRNA RNA_BoidRuleFight;
extern LIBEXPORT StructRNA RNA_BoidRuleFollowLeader;
extern LIBEXPORT StructRNA RNA_BoidRuleGoal;
extern LIBEXPORT StructRNA RNA_BoidSettings;
extern LIBEXPORT StructRNA RNA_BoidState;
extern LIBEXPORT StructRNA RNA_Bone;
extern LIBEXPORT StructRNA RNA_BoneGroup;
extern LIBEXPORT StructRNA RNA_BooleanModifier;
extern LIBEXPORT StructRNA RNA_BooleanProperty;
extern LIBEXPORT StructRNA RNA_Brush;
extern LIBEXPORT StructRNA RNA_BrushTextureSlot;
extern LIBEXPORT StructRNA RNA_BuildModifier;
extern LIBEXPORT StructRNA RNA_Camera;
extern LIBEXPORT StructRNA RNA_CastModifier;
extern LIBEXPORT StructRNA RNA_ChildOfConstraint;
extern LIBEXPORT StructRNA RNA_ChildParticle;
extern LIBEXPORT StructRNA RNA_ClampToConstraint;
extern LIBEXPORT StructRNA RNA_ClothCollisionSettings;
extern LIBEXPORT StructRNA RNA_ClothModifier;
extern LIBEXPORT StructRNA RNA_ClothSettings;
extern LIBEXPORT StructRNA RNA_CloudsTexture;
extern LIBEXPORT StructRNA RNA_CollectionProperty;
extern LIBEXPORT StructRNA RNA_CollisionModifier;
extern LIBEXPORT StructRNA RNA_CollisionSensor;
extern LIBEXPORT StructRNA RNA_CollisionSettings;
extern LIBEXPORT StructRNA RNA_ColorRamp;
extern LIBEXPORT StructRNA RNA_ColorRampElement;
extern LIBEXPORT StructRNA RNA_ColorSequence;
extern LIBEXPORT StructRNA RNA_CompositorNode;
extern LIBEXPORT StructRNA RNA_CompositorNodeAlphaOver;
extern LIBEXPORT StructRNA RNA_CompositorNodeBilateralblur;
extern LIBEXPORT StructRNA RNA_CompositorNodeBlur;
extern LIBEXPORT StructRNA RNA_CompositorNodeBrightContrast;
extern LIBEXPORT StructRNA RNA_CompositorNodeChannelMatte;
extern LIBEXPORT StructRNA RNA_CompositorNodeChromaMatte;
extern LIBEXPORT StructRNA RNA_CompositorNodeColorMatte;
extern LIBEXPORT StructRNA RNA_CompositorNodeColorSpill;
extern LIBEXPORT StructRNA RNA_CompositorNodeCombHSVA;
extern LIBEXPORT StructRNA RNA_CompositorNodeCombRGBA;
extern LIBEXPORT StructRNA RNA_CompositorNodeCombYCCA;
extern LIBEXPORT StructRNA RNA_CompositorNodeCombYUVA;
extern LIBEXPORT StructRNA RNA_CompositorNodeComposite;
extern LIBEXPORT StructRNA RNA_CompositorNodeCrop;
extern LIBEXPORT StructRNA RNA_CompositorNodeCurveRGB;
extern LIBEXPORT StructRNA RNA_CompositorNodeCurveVec;
extern LIBEXPORT StructRNA RNA_CompositorNodeDBlur;
extern LIBEXPORT StructRNA RNA_CompositorNodeDefocus;
extern LIBEXPORT StructRNA RNA_CompositorNodeDiffMatte;
extern LIBEXPORT StructRNA RNA_CompositorNodeDilateErode;
extern LIBEXPORT StructRNA RNA_CompositorNodeDisplace;
extern LIBEXPORT StructRNA RNA_CompositorNodeDistanceMatte;
extern LIBEXPORT StructRNA RNA_CompositorNodeFilter;
extern LIBEXPORT StructRNA RNA_CompositorNodeFlip;
extern LIBEXPORT StructRNA RNA_CompositorNodeGamma;
extern LIBEXPORT StructRNA RNA_CompositorNodeGlare;
extern LIBEXPORT StructRNA RNA_CompositorNodeHueSat;
extern LIBEXPORT StructRNA RNA_CompositorNodeIDMask;
extern LIBEXPORT StructRNA RNA_CompositorNodeImage;
extern LIBEXPORT StructRNA RNA_CompositorNodeInvert;
extern LIBEXPORT StructRNA RNA_CompositorNodeLensdist;
extern LIBEXPORT StructRNA RNA_CompositorNodeLevels;
extern LIBEXPORT StructRNA RNA_CompositorNodeLumaMatte;
extern LIBEXPORT StructRNA RNA_CompositorNodeMapUV;
extern LIBEXPORT StructRNA RNA_CompositorNodeMapValue;
extern LIBEXPORT StructRNA RNA_CompositorNodeMath;
extern LIBEXPORT StructRNA RNA_CompositorNodeMixRGB;
extern LIBEXPORT StructRNA RNA_CompositorNodeNormal;
extern LIBEXPORT StructRNA RNA_CompositorNodeNormalize;
extern LIBEXPORT StructRNA RNA_CompositorNodeOutputFile;
extern LIBEXPORT StructRNA RNA_CompositorNodePremulKey;
extern LIBEXPORT StructRNA RNA_CompositorNodeRGB;
extern LIBEXPORT StructRNA RNA_CompositorNodeRGBToBW;
extern LIBEXPORT StructRNA RNA_CompositorNodeRLayers;
extern LIBEXPORT StructRNA RNA_CompositorNodeRotate;
extern LIBEXPORT StructRNA RNA_CompositorNodeScale;
extern LIBEXPORT StructRNA RNA_CompositorNodeSepHSVA;
extern LIBEXPORT StructRNA RNA_CompositorNodeSepRGBA;
extern LIBEXPORT StructRNA RNA_CompositorNodeSepYCCA;
extern LIBEXPORT StructRNA RNA_CompositorNodeSepYUVA;
extern LIBEXPORT StructRNA RNA_CompositorNodeSetAlpha;
extern LIBEXPORT StructRNA RNA_CompositorNodeSplitViewer;
extern LIBEXPORT StructRNA RNA_CompositorNodeTexture;
extern LIBEXPORT StructRNA RNA_CompositorNodeTime;
extern LIBEXPORT StructRNA RNA_CompositorNodeTonemap;
extern LIBEXPORT StructRNA RNA_CompositorNodeTranslate;
extern LIBEXPORT StructRNA RNA_CompositorNodeTree;
extern LIBEXPORT StructRNA RNA_CompositorNodeValToRGB;
extern LIBEXPORT StructRNA RNA_CompositorNodeValue;
extern LIBEXPORT StructRNA RNA_CompositorNodeVecBlur;
extern LIBEXPORT StructRNA RNA_CompositorNodeViewer;
extern LIBEXPORT StructRNA RNA_CompositorNodeZcombine;
extern LIBEXPORT StructRNA RNA_ConsoleLine;
extern LIBEXPORT StructRNA RNA_Constraint;
extern LIBEXPORT StructRNA RNA_ConstraintTarget;
extern LIBEXPORT StructRNA RNA_Context;
extern LIBEXPORT StructRNA RNA_ControlFluidSettings;
extern LIBEXPORT StructRNA RNA_Controller;
extern LIBEXPORT StructRNA RNA_CopyLocationConstraint;
extern LIBEXPORT StructRNA RNA_CopyRotationConstraint;
extern LIBEXPORT StructRNA RNA_CopyScaleConstraint;
extern LIBEXPORT StructRNA RNA_CopyTransformsConstraint;
extern LIBEXPORT StructRNA RNA_Curve;
extern LIBEXPORT StructRNA RNA_CurveMap;
extern LIBEXPORT StructRNA RNA_CurveMapPoint;
extern LIBEXPORT StructRNA RNA_CurveMapping;
extern LIBEXPORT StructRNA RNA_CurveModifier;
extern LIBEXPORT StructRNA RNA_CurvePoint;
extern LIBEXPORT StructRNA RNA_DampedTrackConstraint;
extern LIBEXPORT StructRNA RNA_DecimateModifier;
extern LIBEXPORT StructRNA RNA_DelaySensor;
extern LIBEXPORT StructRNA RNA_DisplaceModifier;
extern LIBEXPORT StructRNA RNA_DistortedNoiseTexture;
extern LIBEXPORT StructRNA RNA_DomainFluidSettings;
extern LIBEXPORT StructRNA RNA_Driver;
extern LIBEXPORT StructRNA RNA_DriverTarget;
extern LIBEXPORT StructRNA RNA_DriverVariable;
extern LIBEXPORT StructRNA RNA_DupliObject;
extern LIBEXPORT StructRNA RNA_EdgeSplitModifier;
extern LIBEXPORT StructRNA RNA_EditBone;
extern LIBEXPORT StructRNA RNA_EffectSequence;
extern LIBEXPORT StructRNA RNA_EffectorWeights;
extern LIBEXPORT StructRNA RNA_EnumProperty;
extern LIBEXPORT StructRNA RNA_EnumPropertyItem;
extern LIBEXPORT StructRNA RNA_EnvironmentMap;
extern LIBEXPORT StructRNA RNA_EnvironmentMapTexture;
extern LIBEXPORT StructRNA RNA_Event;
extern LIBEXPORT StructRNA RNA_ExplodeModifier;
extern LIBEXPORT StructRNA RNA_ExpressionController;
extern LIBEXPORT StructRNA RNA_FCurve;
extern LIBEXPORT StructRNA RNA_FCurveSample;
extern LIBEXPORT StructRNA RNA_FModifier;
extern LIBEXPORT StructRNA RNA_FModifierCycles;
extern LIBEXPORT StructRNA RNA_FModifierEnvelope;
extern LIBEXPORT StructRNA RNA_FModifierEnvelopeControlPoint;
extern LIBEXPORT StructRNA RNA_FModifierFunctionGenerator;
extern LIBEXPORT StructRNA RNA_FModifierGenerator;
extern LIBEXPORT StructRNA RNA_FModifierLimits;
extern LIBEXPORT StructRNA RNA_FModifierNoise;
extern LIBEXPORT StructRNA RNA_FModifierPython;
extern LIBEXPORT StructRNA RNA_FModifierStepped;
extern LIBEXPORT StructRNA RNA_FieldSettings;
extern LIBEXPORT StructRNA RNA_FileSelectParams;
extern LIBEXPORT StructRNA RNA_FloatProperty;
extern LIBEXPORT StructRNA RNA_FloorConstraint;
extern LIBEXPORT StructRNA RNA_FluidFluidSettings;
extern LIBEXPORT StructRNA RNA_FluidSettings;
extern LIBEXPORT StructRNA RNA_FluidSimulationModifier;
extern LIBEXPORT StructRNA RNA_FollowPathConstraint;
extern LIBEXPORT StructRNA RNA_Function;
extern LIBEXPORT StructRNA RNA_GPencilFrame;
extern LIBEXPORT StructRNA RNA_GPencilLayer;
extern LIBEXPORT StructRNA RNA_GPencilStroke;
extern LIBEXPORT StructRNA RNA_GPencilStrokePoint;
extern LIBEXPORT StructRNA RNA_GameBooleanProperty;
extern LIBEXPORT StructRNA RNA_GameFloatProperty;
extern LIBEXPORT StructRNA RNA_GameIntProperty;
extern LIBEXPORT StructRNA RNA_GameObjectSettings;
extern LIBEXPORT StructRNA RNA_GameProperty;
extern LIBEXPORT StructRNA RNA_GameSoftBodySettings;
extern LIBEXPORT StructRNA RNA_GameStringProperty;
extern LIBEXPORT StructRNA RNA_GameTimerProperty;
extern LIBEXPORT StructRNA RNA_GlowSequence;
extern LIBEXPORT StructRNA RNA_GreasePencil;
extern LIBEXPORT StructRNA RNA_Group;
extern LIBEXPORT StructRNA RNA_Header;
extern LIBEXPORT StructRNA RNA_HemiLamp;
extern LIBEXPORT StructRNA RNA_Histogram;
extern LIBEXPORT StructRNA RNA_HookModifier;
extern LIBEXPORT StructRNA RNA_ID;
extern LIBEXPORT StructRNA RNA_IKParam;
extern LIBEXPORT StructRNA RNA_Image;
extern LIBEXPORT StructRNA RNA_ImagePaint;
extern LIBEXPORT StructRNA RNA_ImageSequence;
extern LIBEXPORT StructRNA RNA_ImageTexture;
extern LIBEXPORT StructRNA RNA_ImageUser;
extern LIBEXPORT StructRNA RNA_InflowFluidSettings;
extern LIBEXPORT StructRNA RNA_IntProperty;
extern LIBEXPORT StructRNA RNA_Itasc;
extern LIBEXPORT StructRNA RNA_JoystickSensor;
extern LIBEXPORT StructRNA RNA_Key;
extern LIBEXPORT StructRNA RNA_KeyConfig;
extern LIBEXPORT StructRNA RNA_KeyMap;
extern LIBEXPORT StructRNA RNA_KeyMapItem;
extern LIBEXPORT StructRNA RNA_KeyboardSensor;
extern LIBEXPORT StructRNA RNA_Keyframe;
extern LIBEXPORT StructRNA RNA_KeyingSet;
extern LIBEXPORT StructRNA RNA_KeyingSetInfo;
extern LIBEXPORT StructRNA RNA_KeyingSetPath;
extern LIBEXPORT StructRNA RNA_KinematicConstraint;
extern LIBEXPORT StructRNA RNA_Lamp;
extern LIBEXPORT StructRNA RNA_LampSkySettings;
extern LIBEXPORT StructRNA RNA_LampTextureSlot;
extern LIBEXPORT StructRNA RNA_Lattice;
extern LIBEXPORT StructRNA RNA_LatticeModifier;
extern LIBEXPORT StructRNA RNA_LatticePoint;
extern LIBEXPORT StructRNA RNA_Library;
extern LIBEXPORT StructRNA RNA_LimitDistanceConstraint;
extern LIBEXPORT StructRNA RNA_LimitLocationConstraint;
extern LIBEXPORT StructRNA RNA_LimitRotationConstraint;
extern LIBEXPORT StructRNA RNA_LimitScaleConstraint;
extern LIBEXPORT StructRNA RNA_LockedTrackConstraint;
extern LIBEXPORT StructRNA RNA_Macro;
extern LIBEXPORT StructRNA RNA_MagicTexture;
extern LIBEXPORT StructRNA RNA_MarbleTexture;
extern LIBEXPORT StructRNA RNA_MaskModifier;
extern LIBEXPORT StructRNA RNA_Material;
extern LIBEXPORT StructRNA RNA_MaterialHalo;
extern LIBEXPORT StructRNA RNA_MaterialPhysics;
extern LIBEXPORT StructRNA RNA_MaterialRaytraceMirror;
extern LIBEXPORT StructRNA RNA_MaterialRaytraceTransparency;
extern LIBEXPORT StructRNA RNA_MaterialSlot;
extern LIBEXPORT StructRNA RNA_MaterialStrand;
extern LIBEXPORT StructRNA RNA_MaterialSubsurfaceScattering;
extern LIBEXPORT StructRNA RNA_MaterialTextureSlot;
extern LIBEXPORT StructRNA RNA_MaterialVolume;
extern LIBEXPORT StructRNA RNA_Menu;
extern LIBEXPORT StructRNA RNA_Mesh;
extern LIBEXPORT StructRNA RNA_MeshColor;
extern LIBEXPORT StructRNA RNA_MeshColorLayer;
extern LIBEXPORT StructRNA RNA_MeshDeformModifier;
extern LIBEXPORT StructRNA RNA_MeshEdge;
extern LIBEXPORT StructRNA RNA_MeshFace;
extern LIBEXPORT StructRNA RNA_MeshFloatProperty;
extern LIBEXPORT StructRNA RNA_MeshFloatPropertyLayer;
extern LIBEXPORT StructRNA RNA_MeshIntProperty;
extern LIBEXPORT StructRNA RNA_MeshIntPropertyLayer;
extern LIBEXPORT StructRNA RNA_MeshSticky;
extern LIBEXPORT StructRNA RNA_MeshStringProperty;
extern LIBEXPORT StructRNA RNA_MeshStringPropertyLayer;
extern LIBEXPORT StructRNA RNA_MeshTextureFace;
extern LIBEXPORT StructRNA RNA_MeshTextureFaceLayer;
extern LIBEXPORT StructRNA RNA_MeshVertex;
extern LIBEXPORT StructRNA RNA_MessageSensor;
extern LIBEXPORT StructRNA RNA_MetaBall;
extern LIBEXPORT StructRNA RNA_MetaElement;
extern LIBEXPORT StructRNA RNA_MetaSequence;
extern LIBEXPORT StructRNA RNA_MirrorModifier;
extern LIBEXPORT StructRNA RNA_Modifier;
extern LIBEXPORT StructRNA RNA_MotionPath;
extern LIBEXPORT StructRNA RNA_MotionPathVert;
extern LIBEXPORT StructRNA RNA_MouseSensor;
extern LIBEXPORT StructRNA RNA_MovieSequence;
extern LIBEXPORT StructRNA RNA_MulticamSequence;
extern LIBEXPORT StructRNA RNA_MultiresModifier;
extern LIBEXPORT StructRNA RNA_MusgraveTexture;
extern LIBEXPORT StructRNA RNA_NandController;
extern LIBEXPORT StructRNA RNA_NearSensor;
extern LIBEXPORT StructRNA RNA_NlaStrip;
extern LIBEXPORT StructRNA RNA_NlaTrack;
extern LIBEXPORT StructRNA RNA_Node;
extern LIBEXPORT StructRNA RNA_NodeGroup;
extern LIBEXPORT StructRNA RNA_NodeLink;
extern LIBEXPORT StructRNA RNA_NodeSocket;
extern LIBEXPORT StructRNA RNA_NodeTree;
extern LIBEXPORT StructRNA RNA_NoiseTexture;
extern LIBEXPORT StructRNA RNA_NorController;
extern LIBEXPORT StructRNA RNA_Object;
extern LIBEXPORT StructRNA RNA_ObjectBase;
extern LIBEXPORT StructRNA RNA_ObstacleFluidSettings;
extern LIBEXPORT StructRNA RNA_Operator;
extern LIBEXPORT StructRNA RNA_OperatorFileListElement;
extern LIBEXPORT StructRNA RNA_OperatorMousePath;
extern LIBEXPORT StructRNA RNA_OperatorProperties;
extern LIBEXPORT StructRNA RNA_OperatorStrokeElement;
extern LIBEXPORT StructRNA RNA_OperatorTypeMacro;
extern LIBEXPORT StructRNA RNA_OrController;
extern LIBEXPORT StructRNA RNA_OutflowFluidSettings;
extern LIBEXPORT StructRNA RNA_PackedFile;
extern LIBEXPORT StructRNA RNA_Paint;
extern LIBEXPORT StructRNA RNA_Panel;
extern LIBEXPORT StructRNA RNA_Particle;
extern LIBEXPORT StructRNA RNA_ParticleBrush;
extern LIBEXPORT StructRNA RNA_ParticleDupliWeight;
extern LIBEXPORT StructRNA RNA_ParticleEdit;
extern LIBEXPORT StructRNA RNA_ParticleFluidSettings;
extern LIBEXPORT StructRNA RNA_ParticleHairKey;
extern LIBEXPORT StructRNA RNA_ParticleInstanceModifier;
extern LIBEXPORT StructRNA RNA_ParticleKey;
extern LIBEXPORT StructRNA RNA_ParticleSettings;
extern LIBEXPORT StructRNA RNA_ParticleSettingsTextureSlot;
extern LIBEXPORT StructRNA RNA_ParticleSystem;
extern LIBEXPORT StructRNA RNA_ParticleSystemModifier;
extern LIBEXPORT StructRNA RNA_ParticleTarget;
extern LIBEXPORT StructRNA RNA_PivotConstraint;
extern LIBEXPORT StructRNA RNA_PluginSequence;
extern LIBEXPORT StructRNA RNA_PluginTexture;
extern LIBEXPORT StructRNA RNA_PointCache;
extern LIBEXPORT StructRNA RNA_PointDensity;
extern LIBEXPORT StructRNA RNA_PointDensityTexture;
extern LIBEXPORT StructRNA RNA_PointLamp;
extern LIBEXPORT StructRNA RNA_PointerProperty;
extern LIBEXPORT StructRNA RNA_Pose;
extern LIBEXPORT StructRNA RNA_PoseBone;
extern LIBEXPORT StructRNA RNA_Property;
extern LIBEXPORT StructRNA RNA_PropertyGroup;
extern LIBEXPORT StructRNA RNA_PropertyGroupItem;
extern LIBEXPORT StructRNA RNA_PropertySensor;
extern LIBEXPORT StructRNA RNA_PythonConstraint;
extern LIBEXPORT StructRNA RNA_PythonController;
extern LIBEXPORT StructRNA RNA_RGBANodeSocket;
extern LIBEXPORT StructRNA RNA_RadarSensor;
extern LIBEXPORT StructRNA RNA_RandomSensor;
extern LIBEXPORT StructRNA RNA_RaySensor;
extern LIBEXPORT StructRNA RNA_Region;
extern LIBEXPORT StructRNA RNA_RenderEngine;
extern LIBEXPORT StructRNA RNA_RenderLayer;
extern LIBEXPORT StructRNA RNA_RenderPass;
extern LIBEXPORT StructRNA RNA_RenderResult;
extern LIBEXPORT StructRNA RNA_RenderSettings;
extern LIBEXPORT StructRNA RNA_RigidBodyJointConstraint;
extern LIBEXPORT StructRNA RNA_SPHFluidSettings;
extern LIBEXPORT StructRNA RNA_Scene;
extern LIBEXPORT StructRNA RNA_SceneGameData;
extern LIBEXPORT StructRNA RNA_SceneRenderLayer;
extern LIBEXPORT StructRNA RNA_SceneSequence;
extern LIBEXPORT StructRNA RNA_Scopes;
extern LIBEXPORT StructRNA RNA_Screen;
extern LIBEXPORT StructRNA RNA_ScrewModifier;
extern LIBEXPORT StructRNA RNA_Sculpt;
extern LIBEXPORT StructRNA RNA_Sensor;
extern LIBEXPORT StructRNA RNA_Sequence;
extern LIBEXPORT StructRNA RNA_SequenceColorBalance;
extern LIBEXPORT StructRNA RNA_SequenceCrop;
extern LIBEXPORT StructRNA RNA_SequenceEditor;
extern LIBEXPORT StructRNA RNA_SequenceElement;
extern LIBEXPORT StructRNA RNA_SequenceProxy;
extern LIBEXPORT StructRNA RNA_SequenceTransform;
extern LIBEXPORT StructRNA RNA_ShaderNode;
extern LIBEXPORT StructRNA RNA_ShaderNodeCameraData;
extern LIBEXPORT StructRNA RNA_ShaderNodeCombineRGB;
extern LIBEXPORT StructRNA RNA_ShaderNodeExtendedMaterial;
extern LIBEXPORT StructRNA RNA_ShaderNodeGeometry;
extern LIBEXPORT StructRNA RNA_ShaderNodeHueSaturation;
extern LIBEXPORT StructRNA RNA_ShaderNodeInvert;
extern LIBEXPORT StructRNA RNA_ShaderNodeMapping;
extern LIBEXPORT StructRNA RNA_ShaderNodeMaterial;
extern LIBEXPORT StructRNA RNA_ShaderNodeMath;
extern LIBEXPORT StructRNA RNA_ShaderNodeMixRGB;
extern LIBEXPORT StructRNA RNA_ShaderNodeNormal;
extern LIBEXPORT StructRNA RNA_ShaderNodeOutput;
extern LIBEXPORT StructRNA RNA_ShaderNodeRGB;
extern LIBEXPORT StructRNA RNA_ShaderNodeRGBCurve;
extern LIBEXPORT StructRNA RNA_ShaderNodeRGBToBW;
extern LIBEXPORT StructRNA RNA_ShaderNodeSeparateRGB;
extern LIBEXPORT StructRNA RNA_ShaderNodeSqueeze;
extern LIBEXPORT StructRNA RNA_ShaderNodeTexture;
extern LIBEXPORT StructRNA RNA_ShaderNodeTree;
extern LIBEXPORT StructRNA RNA_ShaderNodeValToRGB;
extern LIBEXPORT StructRNA RNA_ShaderNodeValue;
extern LIBEXPORT StructRNA RNA_ShaderNodeVectorCurve;
extern LIBEXPORT StructRNA RNA_ShaderNodeVectorMath;
extern LIBEXPORT StructRNA RNA_ShapeKey;
extern LIBEXPORT StructRNA RNA_ShapeKeyBezierPoint;
extern LIBEXPORT StructRNA RNA_ShapeKeyCurvePoint;
extern LIBEXPORT StructRNA RNA_ShapeKeyPoint;
extern LIBEXPORT StructRNA RNA_ShrinkwrapConstraint;
extern LIBEXPORT StructRNA RNA_ShrinkwrapModifier;
extern LIBEXPORT StructRNA RNA_SimpleDeformModifier;
extern LIBEXPORT StructRNA RNA_SmokeCollSettings;
extern LIBEXPORT StructRNA RNA_SmokeDomainSettings;
extern LIBEXPORT StructRNA RNA_SmokeFlowSettings;
extern LIBEXPORT StructRNA RNA_SmokeModifier;
extern LIBEXPORT StructRNA RNA_SmoothModifier;
extern LIBEXPORT StructRNA RNA_SoftBodyModifier;
extern LIBEXPORT StructRNA RNA_SoftBodySettings;
extern LIBEXPORT StructRNA RNA_SolidifyModifier;
extern LIBEXPORT StructRNA RNA_Sound;
extern LIBEXPORT StructRNA RNA_SoundSequence;
extern LIBEXPORT StructRNA RNA_Space;
extern LIBEXPORT StructRNA RNA_SpaceConsole;
extern LIBEXPORT StructRNA RNA_SpaceDopeSheetEditor;
extern LIBEXPORT StructRNA RNA_SpaceFileBrowser;
extern LIBEXPORT StructRNA RNA_SpaceGraphEditor;
extern LIBEXPORT StructRNA RNA_SpaceImageEditor;
extern LIBEXPORT StructRNA RNA_SpaceInfo;
extern LIBEXPORT StructRNA RNA_SpaceLogicEditor;
extern LIBEXPORT StructRNA RNA_SpaceNLA;
extern LIBEXPORT StructRNA RNA_SpaceNodeEditor;
extern LIBEXPORT StructRNA RNA_SpaceOutliner;
extern LIBEXPORT StructRNA RNA_SpaceProperties;
extern LIBEXPORT StructRNA RNA_SpaceSequenceEditor;
extern LIBEXPORT StructRNA RNA_SpaceTextEditor;
extern LIBEXPORT StructRNA RNA_SpaceTimeline;
extern LIBEXPORT StructRNA RNA_SpaceUVEditor;
extern LIBEXPORT StructRNA RNA_SpaceUserPreferences;
extern LIBEXPORT StructRNA RNA_SpaceView3D;
extern LIBEXPORT StructRNA RNA_SpeedControlSequence;
extern LIBEXPORT StructRNA RNA_Spline;
extern LIBEXPORT StructRNA RNA_SplineIKConstraint;
extern LIBEXPORT StructRNA RNA_SpotLamp;
extern LIBEXPORT StructRNA RNA_StretchToConstraint;
extern LIBEXPORT StructRNA RNA_StringProperty;
extern LIBEXPORT StructRNA RNA_Struct;
extern LIBEXPORT StructRNA RNA_StucciTexture;
extern LIBEXPORT StructRNA RNA_SubsurfModifier;
extern LIBEXPORT StructRNA RNA_SunLamp;
extern LIBEXPORT StructRNA RNA_SurfaceCurve;
extern LIBEXPORT StructRNA RNA_SurfaceModifier;
extern LIBEXPORT StructRNA RNA_TexMapping;
extern LIBEXPORT StructRNA RNA_Text;
extern LIBEXPORT StructRNA RNA_TextBox;
extern LIBEXPORT StructRNA RNA_TextCharacterFormat;
extern LIBEXPORT StructRNA RNA_TextCurve;
extern LIBEXPORT StructRNA RNA_TextLine;
extern LIBEXPORT StructRNA RNA_TextMarker;
extern LIBEXPORT StructRNA RNA_Texture;
extern LIBEXPORT StructRNA RNA_TextureNode;
extern LIBEXPORT StructRNA RNA_TextureNodeBricks;
extern LIBEXPORT StructRNA RNA_TextureNodeChecker;
extern LIBEXPORT StructRNA RNA_TextureNodeCompose;
extern LIBEXPORT StructRNA RNA_TextureNodeCoordinates;
extern LIBEXPORT StructRNA RNA_TextureNodeCurveRGB;
extern LIBEXPORT StructRNA RNA_TextureNodeCurveTime;
extern LIBEXPORT StructRNA RNA_TextureNodeDecompose;
extern LIBEXPORT StructRNA RNA_TextureNodeDistance;
extern LIBEXPORT StructRNA RNA_TextureNodeHueSaturation;
extern LIBEXPORT StructRNA RNA_TextureNodeImage;
extern LIBEXPORT StructRNA RNA_TextureNodeInvert;
extern LIBEXPORT StructRNA RNA_TextureNodeMath;
extern LIBEXPORT StructRNA RNA_TextureNodeMixRGB;
extern LIBEXPORT StructRNA RNA_TextureNodeOutput;
extern LIBEXPORT StructRNA RNA_TextureNodeRGBToBW;
extern LIBEXPORT StructRNA RNA_TextureNodeRotate;
extern LIBEXPORT StructRNA RNA_TextureNodeScale;
extern LIBEXPORT StructRNA RNA_TextureNodeTexture;
extern LIBEXPORT StructRNA RNA_TextureNodeTranslate;
extern LIBEXPORT StructRNA RNA_TextureNodeTree;
extern LIBEXPORT StructRNA RNA_TextureNodeValToNor;
extern LIBEXPORT StructRNA RNA_TextureNodeValToRGB;
extern LIBEXPORT StructRNA RNA_TextureNodeViewer;
extern LIBEXPORT StructRNA RNA_TextureSlot;
extern LIBEXPORT StructRNA RNA_Theme;
extern LIBEXPORT StructRNA RNA_ThemeAudioWindow;
extern LIBEXPORT StructRNA RNA_ThemeBoneColorSet;
extern LIBEXPORT StructRNA RNA_ThemeConsole;
extern LIBEXPORT StructRNA RNA_ThemeDopeSheet;
extern LIBEXPORT StructRNA RNA_ThemeFileBrowser;
extern LIBEXPORT StructRNA RNA_ThemeFontStyle;
extern LIBEXPORT StructRNA RNA_ThemeGraphEditor;
extern LIBEXPORT StructRNA RNA_ThemeImageEditor;
extern LIBEXPORT StructRNA RNA_ThemeInfo;
extern LIBEXPORT StructRNA RNA_ThemeLogicEditor;
extern LIBEXPORT StructRNA RNA_ThemeNLAEditor;
extern LIBEXPORT StructRNA RNA_ThemeNodeEditor;
extern LIBEXPORT StructRNA RNA_ThemeOutliner;
extern LIBEXPORT StructRNA RNA_ThemeProperties;
extern LIBEXPORT StructRNA RNA_ThemeSequenceEditor;
extern LIBEXPORT StructRNA RNA_ThemeStyle;
extern LIBEXPORT StructRNA RNA_ThemeTextEditor;
extern LIBEXPORT StructRNA RNA_ThemeTimeline;
extern LIBEXPORT StructRNA RNA_ThemeUserInterface;
extern LIBEXPORT StructRNA RNA_ThemeUserPreferences;
extern LIBEXPORT StructRNA RNA_ThemeView3D;
extern LIBEXPORT StructRNA RNA_ThemeWidgetColors;
extern LIBEXPORT StructRNA RNA_ThemeWidgetStateColors;
extern LIBEXPORT StructRNA RNA_TimelineMarker;
extern LIBEXPORT StructRNA RNA_Timer;
extern LIBEXPORT StructRNA RNA_ToolSettings;
extern LIBEXPORT StructRNA RNA_TouchSensor;
extern LIBEXPORT StructRNA RNA_TrackToConstraint;
extern LIBEXPORT StructRNA RNA_TransformConstraint;
extern LIBEXPORT StructRNA RNA_TransformSequence;
extern LIBEXPORT StructRNA RNA_UILayout;
extern LIBEXPORT StructRNA RNA_UIListItem;
extern LIBEXPORT StructRNA RNA_UVProjectModifier;
extern LIBEXPORT StructRNA RNA_UVProjector;
extern LIBEXPORT StructRNA RNA_UnitSettings;
extern LIBEXPORT StructRNA RNA_UnknownType;
extern LIBEXPORT StructRNA RNA_UserPreferences;
extern LIBEXPORT StructRNA RNA_UserPreferencesEdit;
extern LIBEXPORT StructRNA RNA_UserPreferencesFilePaths;
extern LIBEXPORT StructRNA RNA_UserPreferencesSystem;
extern LIBEXPORT StructRNA RNA_UserPreferencesView;
extern LIBEXPORT StructRNA RNA_UserSolidLight;
extern LIBEXPORT StructRNA RNA_ValueNodeSocket;
extern LIBEXPORT StructRNA RNA_VectorFont;
extern LIBEXPORT StructRNA RNA_VectorNodeSocket;
extern LIBEXPORT StructRNA RNA_VertexGroup;
extern LIBEXPORT StructRNA RNA_VertexGroupElement;
extern LIBEXPORT StructRNA RNA_VertexPaint;
extern LIBEXPORT StructRNA RNA_VoronoiTexture;
extern LIBEXPORT StructRNA RNA_VoxelData;
extern LIBEXPORT StructRNA RNA_VoxelDataTexture;
extern LIBEXPORT StructRNA RNA_WarpModifier;
extern LIBEXPORT StructRNA RNA_WaveModifier;
extern LIBEXPORT StructRNA RNA_Window;
extern LIBEXPORT StructRNA RNA_WindowManager;
extern LIBEXPORT StructRNA RNA_WipeSequence;
extern LIBEXPORT StructRNA RNA_WoodTexture;
extern LIBEXPORT StructRNA RNA_World;
extern LIBEXPORT StructRNA RNA_WorldAmbientOcclusion;
extern LIBEXPORT StructRNA RNA_WorldMistSettings;
extern LIBEXPORT StructRNA RNA_WorldStarsSettings;
extern LIBEXPORT StructRNA RNA_WorldTextureSlot;
extern LIBEXPORT StructRNA RNA_XnorController;
extern LIBEXPORT StructRNA RNA_XorController;

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

LIBEXPORT void RNA_main_pointer_create(struct Main *main, PointerRNA *r_ptr);
LIBEXPORT void RNA_id_pointer_create(struct ID *id, PointerRNA *r_ptr);
LIBEXPORT void RNA_pointer_create(struct ID *id, StructRNA *type, void *data, PointerRNA *r_ptr);

LIBEXPORT void RNA_blender_rna_pointer_create(PointerRNA *r_ptr);
LIBEXPORT void RNA_pointer_recast(PointerRNA *ptr, PointerRNA *r_ptr);

extern LIBEXPORT const PointerRNA PointerRNA_NULL;

/* Structs */

LIBEXPORT const char *RNA_struct_identifier(StructRNA *type);
LIBEXPORT const char *RNA_struct_ui_name(StructRNA *type);
LIBEXPORT const char *RNA_struct_ui_description(StructRNA *type);
LIBEXPORT int RNA_struct_ui_icon(StructRNA *type);

LIBEXPORT PropertyRNA *RNA_struct_name_property(StructRNA *type);
LIBEXPORT PropertyRNA *RNA_struct_iterator_property(StructRNA *type);
LIBEXPORT StructRNA *RNA_struct_base(StructRNA *type);

LIBEXPORT int RNA_struct_is_ID(StructRNA *type);
LIBEXPORT int RNA_struct_is_a(StructRNA *type, StructRNA *srna);

LIBEXPORT StructRegisterFunc RNA_struct_register(StructRNA *type);
LIBEXPORT StructUnregisterFunc RNA_struct_unregister(StructRNA *type);
LIBEXPORT void **RNA_struct_instance(PointerRNA *ptr);

LIBEXPORT void *RNA_struct_py_type_get(StructRNA *srna);
LIBEXPORT void RNA_struct_py_type_set(StructRNA *srna, void *py_type);

LIBEXPORT void *RNA_struct_blender_type_get(StructRNA *srna);
LIBEXPORT void RNA_struct_blender_type_set(StructRNA *srna, void *blender_type);

LIBEXPORT struct IDProperty *RNA_struct_idprops(PointerRNA *ptr, int create);
LIBEXPORT int RNA_struct_idprops_check(StructRNA *srna);
LIBEXPORT int RNA_struct_idprops_register_check(StructRNA *type);
int RNA_struct_idprops_unset(PointerRNA *ptr, const char *identifier);

LIBEXPORT PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier);
LIBEXPORT int RNA_struct_contains_property(PointerRNA *ptr, PropertyRNA *prop_test);

/* lower level functions for access to type properties */
LIBEXPORT const struct ListBase *RNA_struct_type_properties(StructRNA *srna);
LIBEXPORT PropertyRNA *RNA_struct_type_find_property(StructRNA *srna, const char *identifier);

LIBEXPORT FunctionRNA *RNA_struct_find_function(PointerRNA *ptr, const char *identifier);
LIBEXPORT const struct ListBase *RNA_struct_type_functions(StructRNA *srna);

LIBEXPORT char *RNA_struct_name_get_alloc(PointerRNA *ptr, char *fixedbuf, int fixedlen);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

LIBEXPORT const char *RNA_property_identifier(PropertyRNA *prop);
LIBEXPORT const char *RNA_property_description(PropertyRNA *prop);

LIBEXPORT PropertyType RNA_property_type(PropertyRNA *prop);
LIBEXPORT PropertySubType RNA_property_subtype(PropertyRNA *prop);
LIBEXPORT PropertyUnit RNA_property_unit(PropertyRNA *prop);
LIBEXPORT int RNA_property_flag(PropertyRNA *prop);

LIBEXPORT int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_array_check(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_multi_array_length(PointerRNA *ptr, PropertyRNA *prop, int dimension);
LIBEXPORT int RNA_property_array_dimension(PointerRNA *ptr, PropertyRNA *prop, int length[]);
LIBEXPORT char RNA_property_array_item_char(PropertyRNA *prop, int index);
LIBEXPORT int RNA_property_array_item_index(PropertyRNA *prop, char name);

LIBEXPORT int RNA_property_string_maxlength(PropertyRNA *prop);

LIBEXPORT const char *RNA_property_ui_name(PropertyRNA *prop);
LIBEXPORT const char *RNA_property_ui_description(PropertyRNA *prop);
LIBEXPORT int RNA_property_ui_icon(PropertyRNA *prop);

/* Dynamic Property Information */

LIBEXPORT void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax);
LIBEXPORT void RNA_property_int_ui_range(PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step);

LIBEXPORT void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax);
LIBEXPORT void RNA_property_float_ui_range(PointerRNA *ptr, PropertyRNA *prop, float *softmin, float *softmax, float *step, float *precision);

LIBEXPORT int RNA_property_float_clamp(PointerRNA *ptr, PropertyRNA *prop, float *value);
LIBEXPORT int RNA_property_int_clamp(PointerRNA *ptr, PropertyRNA *prop, int *value);

LIBEXPORT int RNA_enum_identifier(EnumPropertyItem *item, const int value, const char **identifier);
LIBEXPORT int RNA_enum_bitflag_identifiers(EnumPropertyItem *item, const int value, const char **identifier);
LIBEXPORT int RNA_enum_name(EnumPropertyItem *item, const int value, const char **name);
LIBEXPORT int RNA_enum_description(EnumPropertyItem *item, const int value, const char **description);

LIBEXPORT void RNA_property_enum_items(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, EnumPropertyItem **item, int *totitem, int *free);
LIBEXPORT int RNA_property_enum_value(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *value);
LIBEXPORT int RNA_property_enum_identifier(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **identifier);
LIBEXPORT int RNA_property_enum_name(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **name);
LIBEXPORT int RNA_property_enum_bitflag_identifiers(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **identifier);

LIBEXPORT StructRNA *RNA_property_pointer_type(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_pointer_poll(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *value);

LIBEXPORT int RNA_property_editable(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_editable_index(PointerRNA *ptr, PropertyRNA *prop, int index);
LIBEXPORT int RNA_property_editable_flag(PointerRNA *ptr, PropertyRNA *prop); /* without lib check, only checks the flag */
LIBEXPORT int RNA_property_animateable(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_animated(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_path_from_ID_check(PointerRNA *ptr, PropertyRNA *prop); /* slow, use with care */

LIBEXPORT void RNA_property_update(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_update_main(struct Main *bmain, struct Scene *scene, PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_update_check(struct PropertyRNA *prop);

/* Property Data */

LIBEXPORT int RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, int value);
LIBEXPORT void RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
LIBEXPORT int RNA_property_boolean_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
LIBEXPORT void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
LIBEXPORT void RNA_property_boolean_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);
LIBEXPORT int RNA_property_boolean_get_default(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_boolean_get_default_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
LIBEXPORT int RNA_property_boolean_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

LIBEXPORT int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value);
LIBEXPORT void RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
LIBEXPORT int RNA_property_int_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
LIBEXPORT void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
LIBEXPORT void RNA_property_int_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);
LIBEXPORT int RNA_property_int_get_default(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_int_get_default_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
LIBEXPORT int RNA_property_int_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

LIBEXPORT float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value);
LIBEXPORT void RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
LIBEXPORT float RNA_property_float_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
LIBEXPORT void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, const float *values);
LIBEXPORT void RNA_property_float_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, float value);
LIBEXPORT float RNA_property_float_get_default(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_float_get_default_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
LIBEXPORT float RNA_property_float_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

LIBEXPORT void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value);
LIBEXPORT char *RNA_property_string_get_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen);
LIBEXPORT void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value);
LIBEXPORT int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_string_get_default(PointerRNA *ptr, PropertyRNA *prop, char *value);
LIBEXPORT char *RNA_property_string_get_default_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen);
LIBEXPORT int RNA_property_string_default_length(PointerRNA *ptr, PropertyRNA *prop);

LIBEXPORT int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value);
LIBEXPORT int RNA_property_enum_get_default(PointerRNA *ptr, PropertyRNA *prop);

LIBEXPORT PointerRNA RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_pointer_set(PointerRNA *ptr, PropertyRNA *prop, PointerRNA ptr_value);
LIBEXPORT PointerRNA RNA_property_pointer_get_default(PointerRNA *ptr, PropertyRNA *prop);

LIBEXPORT void RNA_property_collection_begin(PointerRNA *ptr, PropertyRNA *prop, CollectionPropertyIterator *iter);
LIBEXPORT void RNA_property_collection_next(CollectionPropertyIterator *iter);
LIBEXPORT void RNA_property_collection_end(CollectionPropertyIterator *iter);
LIBEXPORT int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_collection_lookup_index(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *t_ptr);
LIBEXPORT int RNA_property_collection_lookup_int(PointerRNA *ptr, PropertyRNA *prop, int key, PointerRNA *r_ptr);
LIBEXPORT int RNA_property_collection_lookup_string(PointerRNA *ptr, PropertyRNA *prop, const char *key, PointerRNA *r_ptr);
LIBEXPORT int RNA_property_collection_type_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);

/* efficient functions to set properties for arrays */
LIBEXPORT int RNA_property_collection_raw_array(PointerRNA *ptr, PropertyRNA *prop, PropertyRNA *itemprop, RawArray *array);
LIBEXPORT int RNA_property_collection_raw_get(struct ReportList *reports, PointerRNA *ptr, PropertyRNA *prop, const char *propname, void *array, RawPropertyType type, int len);
LIBEXPORT int RNA_property_collection_raw_set(struct ReportList *reports, PointerRNA *ptr, PropertyRNA *prop, const char *propname, void *array, RawPropertyType type, int len);
LIBEXPORT int RNA_raw_type_sizeof(RawPropertyType type);
RawPropertyType RNA_property_raw_type(PropertyRNA *prop);


/* to create ID property groups */
LIBEXPORT void RNA_property_pointer_add(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_pointer_remove(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT void RNA_property_collection_add(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);
LIBEXPORT int RNA_property_collection_remove(PointerRNA *ptr, PropertyRNA *prop, int key);
LIBEXPORT void RNA_property_collection_clear(PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT int RNA_property_collection_move(PointerRNA *ptr, PropertyRNA *prop, int key, int pos);

/* copy/reset */
LIBEXPORT int RNA_property_copy(PointerRNA *ptr, PointerRNA *fromptr, PropertyRNA *prop, int index);
LIBEXPORT int RNA_property_reset(PointerRNA *ptr, PropertyRNA *prop, int index);

/* Path
 *
 * Experimental method to refer to structs and properties with a string,
 * using a syntax like: scenes[0].objects["Cube"].data.verts[7].co
 *
 * This provides a way to refer to RNA data while being detached from any
 * particular pointers, which is useful in a number of applications, like
 * UI code or Actions, though efficiency is a concern. */

LIBEXPORT char *RNA_path_append(const char *path, PointerRNA *ptr, PropertyRNA *prop,
	int intkey, const char *strkey);
LIBEXPORT char *RNA_path_back(const char *path);

LIBEXPORT int RNA_path_resolve(PointerRNA *ptr, const char *path,
		PointerRNA *r_ptr, PropertyRNA **r_prop);

LIBEXPORT int RNA_path_resolve_full(PointerRNA *ptr, const char *path,
		PointerRNA *r_ptr, PropertyRNA **r_prop, int *index);

LIBEXPORT char *RNA_path_from_ID_to_struct(PointerRNA *ptr);
LIBEXPORT char *RNA_path_from_ID_to_property(PointerRNA *ptr, PropertyRNA *prop);

/* Quick name based property access
 *
 * These are just an easier way to access property values without having to
 * call RNA_struct_find_property. The names have to exist as RNA properties
 * for the type in the pointer, if they do not exist an error will be printed.
 *
 * There is no support for pointers and collections here yet, these can be 
 * added when ID properties support them. */

LIBEXPORT int RNA_boolean_get(PointerRNA *ptr, const char *name);
LIBEXPORT void RNA_boolean_set(PointerRNA *ptr, const char *name, int value);
LIBEXPORT void RNA_boolean_get_array(PointerRNA *ptr, const char *name, int *values);
LIBEXPORT void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const int *values);

LIBEXPORT int RNA_int_get(PointerRNA *ptr, const char *name);
LIBEXPORT void RNA_int_set(PointerRNA *ptr, const char *name, int value);
LIBEXPORT void RNA_int_get_array(PointerRNA *ptr, const char *name, int *values);
LIBEXPORT void RNA_int_set_array(PointerRNA *ptr, const char *name, const int *values);

LIBEXPORT float RNA_float_get(PointerRNA *ptr, const char *name);
LIBEXPORT void RNA_float_set(PointerRNA *ptr, const char *name, float value);
LIBEXPORT void RNA_float_get_array(PointerRNA *ptr, const char *name, float *values);
LIBEXPORT void RNA_float_set_array(PointerRNA *ptr, const char *name, const float *values);

LIBEXPORT int RNA_enum_get(PointerRNA *ptr, const char *name);
LIBEXPORT void RNA_enum_set(PointerRNA *ptr, const char *name, int value);
LIBEXPORT void RNA_enum_set_identifier(PointerRNA *ptr, const char *name, const char *id);
LIBEXPORT int RNA_enum_is_equal(struct bContext *C, PointerRNA *ptr, const char *name, const char *enumname);

/* lower level functions that don't use a PointerRNA */
LIBEXPORT int RNA_enum_value_from_id(EnumPropertyItem *item, const char *identifier, int *value);
LIBEXPORT int RNA_enum_id_from_value(EnumPropertyItem *item, int value, const char **identifier);
LIBEXPORT int RNA_enum_icon_from_value(EnumPropertyItem *item, int value, int *icon);

LIBEXPORT void RNA_string_get(PointerRNA *ptr, const char *name, char *value);
LIBEXPORT char *RNA_string_get_alloc(PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen);
LIBEXPORT int RNA_string_length(PointerRNA *ptr, const char *name);
LIBEXPORT void RNA_string_set(PointerRNA *ptr, const char *name, const char *value);

/**
 * Retrieve the named property from PointerRNA.
 */
LIBEXPORT PointerRNA RNA_pointer_get(PointerRNA *ptr, const char *name);
/* Set the property name of PointerRNA ptr to ptr_value */
LIBEXPORT void RNA_pointer_set(PointerRNA *ptr, const char *name, PointerRNA ptr_value);
LIBEXPORT void RNA_pointer_add(PointerRNA *ptr, const char *name);

LIBEXPORT void RNA_collection_begin(PointerRNA *ptr, const char *name, CollectionPropertyIterator *iter);
LIBEXPORT int RNA_collection_length(PointerRNA *ptr, const char *name);
LIBEXPORT void RNA_collection_add(PointerRNA *ptr, const char *name, PointerRNA *r_value);
LIBEXPORT void RNA_collection_clear(PointerRNA *ptr, const char *name);

#define RNA_BEGIN(sptr, itemptr, propname) \
	{ \
		CollectionPropertyIterator rna_macro_iter; \
		for(RNA_collection_begin(sptr, propname, &rna_macro_iter); rna_macro_iter.valid; RNA_property_collection_next(&rna_macro_iter)) { \
			PointerRNA itemptr= rna_macro_iter.ptr;

#define RNA_END \
		} \
		RNA_property_collection_end(&rna_macro_iter); \
	}

#define RNA_PROP_BEGIN(sptr, itemptr, prop) \
	{ \
		CollectionPropertyIterator rna_macro_iter; \
		for(RNA_property_collection_begin(sptr, prop, &rna_macro_iter); rna_macro_iter.valid; RNA_property_collection_next(&rna_macro_iter)) { \
			PointerRNA itemptr= rna_macro_iter.ptr;

#define RNA_PROP_END \
		} \
		RNA_property_collection_end(&rna_macro_iter); \
	}

#define RNA_STRUCT_BEGIN(sptr, prop) \
	{ \
		CollectionPropertyIterator rna_macro_iter; \
		for(RNA_property_collection_begin(sptr, RNA_struct_iterator_property(sptr->type), &rna_macro_iter); rna_macro_iter.valid; RNA_property_collection_next(&rna_macro_iter)) { \
			PropertyRNA *prop= rna_macro_iter.ptr.data;

#define RNA_STRUCT_END \
		} \
		RNA_property_collection_end(&rna_macro_iter); \
	}

/* check if the idproperty exists, for operators */
LIBEXPORT int RNA_property_is_set(PointerRNA *ptr, const char *name);
LIBEXPORT int RNA_property_is_idprop(PropertyRNA *prop);

/* python compatible string representation of this property, (must be freed!) */
LIBEXPORT char *RNA_property_as_string(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop);
LIBEXPORT char *RNA_pointer_as_string(PointerRNA *ptr);

/* Function */

LIBEXPORT const char *RNA_function_identifier(FunctionRNA *func);
LIBEXPORT const char *RNA_function_ui_description(FunctionRNA *func);
LIBEXPORT int RNA_function_flag(FunctionRNA *func);
LIBEXPORT int RNA_function_defined(FunctionRNA *func);

LIBEXPORT PropertyRNA *RNA_function_get_parameter(PointerRNA *ptr, FunctionRNA *func, int index);
LIBEXPORT PropertyRNA *RNA_function_find_parameter(PointerRNA *ptr, FunctionRNA *func, const char *identifier);
LIBEXPORT const struct ListBase *RNA_function_defined_parameters(FunctionRNA *func);

/* Utility */

LIBEXPORT ParameterList *RNA_parameter_list_create(ParameterList *parms, PointerRNA *ptr, FunctionRNA *func);
LIBEXPORT void RNA_parameter_list_free(ParameterList *parms);
LIBEXPORT int  RNA_parameter_list_size(ParameterList *parms);
LIBEXPORT int  RNA_parameter_list_arg_count(ParameterList *parms);
LIBEXPORT int  RNA_parameter_list_ret_count(ParameterList *parms);

LIBEXPORT void RNA_parameter_list_begin(ParameterList *parms, ParameterIterator *iter);
LIBEXPORT void RNA_parameter_list_next(ParameterIterator *iter);
LIBEXPORT void RNA_parameter_list_end(ParameterIterator *iter);

LIBEXPORT void RNA_parameter_get(ParameterList *parms, PropertyRNA *parm, void **value);
LIBEXPORT void RNA_parameter_get_lookup(ParameterList *parms, const char *identifier, void **value);
LIBEXPORT void RNA_parameter_set(ParameterList *parms, PropertyRNA *parm, const void *value);
LIBEXPORT void RNA_parameter_set_lookup(ParameterList *parms, const char *identifier, const void *value);
LIBEXPORT int RNA_parameter_length_get(ParameterList *parms, PropertyRNA *parm);
LIBEXPORT int RNA_parameter_length_get_data(ParameterList *parms, PropertyRNA *parm, void *data);
LIBEXPORT void RNA_parameter_length_set(ParameterList *parms, PropertyRNA *parm, int length);
LIBEXPORT void RNA_parameter_length_set_data(ParameterList *parms, PropertyRNA *parm, void *data, int length);

LIBEXPORT int RNA_function_call(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms);
LIBEXPORT int RNA_function_call_lookup(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, const char *identifier, ParameterList *parms);

LIBEXPORT int RNA_function_call_direct(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, FunctionRNA *func, const char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 5, 6)))
#endif
;
LIBEXPORT int RNA_function_call_direct_lookup(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, const char *identifier, const char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 5, 6)))
#endif
;
LIBEXPORT int RNA_function_call_direct_va(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, FunctionRNA *func, const char *format, va_list args);
LIBEXPORT int RNA_function_call_direct_va_lookup(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, const char *identifier, const char *format, va_list args);

/* ID */

LIBEXPORT short RNA_type_to_ID_code(StructRNA *type);
LIBEXPORT StructRNA *ID_code_to_RNA_type(short idcode);

void RNA_warning(const char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;

#ifdef __cplusplus
}
#endif

#endif /* RNA_ACCESS_H */
