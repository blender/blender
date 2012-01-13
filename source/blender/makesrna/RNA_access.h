/*
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
extern StructRNA RNA_Action;
extern StructRNA RNA_ActionConstraint;
extern StructRNA RNA_ActionFCurves;
extern StructRNA RNA_ActionGroup;
extern StructRNA RNA_ActionGroups;
extern StructRNA RNA_ActionPoseMarkers;
extern StructRNA RNA_Actuator;
extern StructRNA RNA_ActuatorSensor;
extern StructRNA RNA_Addon;
extern StructRNA RNA_AdjustmentSequence;
extern StructRNA RNA_AlwaysSensor;
extern StructRNA RNA_AndController;
extern StructRNA RNA_AnimData;
extern StructRNA RNA_AnimViz;
extern StructRNA RNA_AnimVizMotionPaths;
extern StructRNA RNA_AnimVizOnionSkinning;
extern StructRNA RNA_AnyType;
extern StructRNA RNA_Area;
extern StructRNA RNA_AreaLamp;
extern StructRNA RNA_Armature;
extern StructRNA RNA_ArmatureModifier;
extern StructRNA RNA_ArmatureSensor;
extern StructRNA RNA_ArrayModifier;
extern StructRNA RNA_BackgroundImage;
extern StructRNA RNA_BevelModifier;
extern StructRNA RNA_SplinePoint;
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
extern StructRNA RNA_BooleanModifier;
extern StructRNA RNA_BoolProperty;
extern StructRNA RNA_Brush;
extern StructRNA RNA_BrushTextureSlot;
extern StructRNA RNA_BuildModifier;
extern StructRNA RNA_Camera;
extern StructRNA RNA_CastModifier;
extern StructRNA RNA_ChildOfConstraint;
extern StructRNA RNA_ChildParticle;
extern StructRNA RNA_ClampToConstraint;
extern StructRNA RNA_ClothCollisionSettings;
extern StructRNA RNA_ClothModifier;
extern StructRNA RNA_ClothSettings;
extern StructRNA RNA_CloudsTexture;
extern StructRNA RNA_CollectionProperty;
extern StructRNA RNA_CollisionModifier;
extern StructRNA RNA_CollisionSensor;
extern StructRNA RNA_CollisionSettings;
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
extern StructRNA RNA_CompositorNodeCrop;
extern StructRNA RNA_CompositorNodeCurveRGB;
extern StructRNA RNA_CompositorNodeCurveVec;
extern StructRNA RNA_CompositorNodeDBlur;
extern StructRNA RNA_CompositorNodeDefocus;
extern StructRNA RNA_CompositorNodeDiffMatte;
extern StructRNA RNA_CompositorNodeDilateErode;
extern StructRNA RNA_CompositorNodeDisplace;
extern StructRNA RNA_CompositorNodeDistanceMatte;
extern StructRNA RNA_CompositorNodeFilter;
extern StructRNA RNA_CompositorNodeFlip;
extern StructRNA RNA_CompositorNodeGamma;
extern StructRNA RNA_CompositorNodeGlare;
extern StructRNA RNA_CompositorNodeHueSat;
extern StructRNA RNA_CompositorNodeIDMask;
extern StructRNA RNA_CompositorNodeDoubleEdgeMask;
extern StructRNA RNA_CompositorNodeImage;
extern StructRNA RNA_CompositorNodeInvert;
extern StructRNA RNA_CompositorNodeLensdist;
extern StructRNA RNA_CompositorNodeLevels;
extern StructRNA RNA_CompositorNodeLumaMatte;
extern StructRNA RNA_CompositorNodeMapUV;
extern StructRNA RNA_CompositorNodeMapValue;
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
extern StructRNA RNA_Context;
extern StructRNA RNA_ControlFluidSettings;
extern StructRNA RNA_Controller;
extern StructRNA RNA_CopyLocationConstraint;
extern StructRNA RNA_CopyRotationConstraint;
extern StructRNA RNA_CopyScaleConstraint;
extern StructRNA RNA_CopyTransformsConstraint;
extern StructRNA RNA_Curve;
extern StructRNA RNA_CurveMap;
extern StructRNA RNA_CurveMapPoint;
extern StructRNA RNA_CurveMapping;
extern StructRNA RNA_CurveModifier;
extern StructRNA RNA_CurvePoint;
extern StructRNA RNA_DampedTrackConstraint;
extern StructRNA RNA_DecimateModifier;
extern StructRNA RNA_DelaySensor;
extern StructRNA RNA_DisplaceModifier;
extern StructRNA RNA_DistortedNoiseTexture;
extern StructRNA RNA_DomainFluidSettings;
extern StructRNA RNA_DopeSheet;
extern StructRNA RNA_Driver;
extern StructRNA RNA_DriverTarget;
extern StructRNA RNA_DriverVariable;
extern StructRNA RNA_DupliObject;
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
extern StructRNA RNA_EnvironmentMap;
extern StructRNA RNA_EnvironmentMapTexture;
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
extern StructRNA RNA_FieldSettings;
extern StructRNA RNA_FileSelectParams;
extern StructRNA RNA_FloatProperty;
extern StructRNA RNA_FloorConstraint;
extern StructRNA RNA_FluidFluidSettings;
extern StructRNA RNA_FluidSettings;
extern StructRNA RNA_FluidSimulationModifier;
extern StructRNA RNA_FollowPathConstraint;
extern StructRNA RNA_Function;
extern StructRNA RNA_GPencilFrame;
extern StructRNA RNA_GPencilLayer;
extern StructRNA RNA_GPencilStroke;
extern StructRNA RNA_GPencilStrokePoint;
extern StructRNA RNA_GameBooleanProperty;
extern StructRNA RNA_GameFloatProperty;
extern StructRNA RNA_GameIntProperty;
extern StructRNA RNA_GameObjectSettings;
extern StructRNA RNA_GameProperty;
extern StructRNA RNA_GameSoftBodySettings;
extern StructRNA RNA_GameStringProperty;
extern StructRNA RNA_GameTimerProperty;
extern StructRNA RNA_GlowSequence;
extern StructRNA RNA_GreasePencil;
extern StructRNA RNA_Group;
extern StructRNA RNA_Header;
extern StructRNA RNA_HemiLamp;
extern StructRNA RNA_Histogram;
extern StructRNA RNA_HookModifier;
extern StructRNA RNA_ID;
extern StructRNA RNA_IKParam;
extern StructRNA RNA_Image;
extern StructRNA RNA_ImageFormatSettings;
extern StructRNA RNA_ImagePaint;
extern StructRNA RNA_ImageSequence;
extern StructRNA RNA_ImageTexture;
extern StructRNA RNA_ImageUser;
extern StructRNA RNA_InflowFluidSettings;
extern StructRNA RNA_IntProperty;
extern StructRNA RNA_Itasc;
extern StructRNA RNA_JoystickSensor;
extern StructRNA RNA_Key;
extern StructRNA RNA_KeyConfig;
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
extern StructRNA RNA_Lamp;
extern StructRNA RNA_LampSkySettings;
extern StructRNA RNA_LampTextureSlot;
extern StructRNA RNA_Lattice;
extern StructRNA RNA_LatticeModifier;
extern StructRNA RNA_LatticePoint;
extern StructRNA RNA_Library;
extern StructRNA RNA_LimitDistanceConstraint;
extern StructRNA RNA_LimitLocationConstraint;
extern StructRNA RNA_LimitRotationConstraint;
extern StructRNA RNA_LimitScaleConstraint;
extern StructRNA RNA_LockedTrackConstraint;
extern StructRNA RNA_Macro;
extern StructRNA RNA_MagicTexture;
extern StructRNA RNA_MarbleTexture;
extern StructRNA RNA_MaskModifier;
extern StructRNA RNA_Material;
extern StructRNA RNA_MaterialHalo;
extern StructRNA RNA_MaterialPhysics;
extern StructRNA RNA_MaterialRaytraceMirror;
extern StructRNA RNA_MaterialRaytraceTransparency;
extern StructRNA RNA_MaterialSlot;
extern StructRNA RNA_MaterialStrand;
extern StructRNA RNA_MaterialSubsurfaceScattering;
extern StructRNA RNA_MaterialTextureSlot;
extern StructRNA RNA_MaterialVolume;
extern StructRNA RNA_Menu;
extern StructRNA RNA_Mesh;
extern StructRNA RNA_MeshColor;
extern StructRNA RNA_MeshColorLayer;
extern StructRNA RNA_MeshDeformModifier;
extern StructRNA RNA_MeshEdge;
extern StructRNA RNA_MeshFace;
extern StructRNA RNA_MeshFloatProperty;
extern StructRNA RNA_MeshFloatPropertyLayer;
extern StructRNA RNA_MeshIntProperty;
extern StructRNA RNA_MeshIntPropertyLayer;
extern StructRNA RNA_MeshSticky;
extern StructRNA RNA_MeshStringProperty;
extern StructRNA RNA_MeshStringPropertyLayer;
extern StructRNA RNA_MeshTextureFace;
extern StructRNA RNA_MeshTextureFaceLayer;
extern StructRNA RNA_MeshVertex;
extern StructRNA RNA_MessageSensor;
extern StructRNA RNA_MetaBall;
extern StructRNA RNA_MetaElement;
extern StructRNA RNA_MetaSequence;
extern StructRNA RNA_MirrorModifier;
extern StructRNA RNA_Modifier;
extern StructRNA RNA_MotionPath;
extern StructRNA RNA_MotionPathVert;
extern StructRNA RNA_MouseSensor;
extern StructRNA RNA_MovieSequence;
extern StructRNA RNA_MovieTrackingObject;
extern StructRNA RNA_MulticamSequence;
extern StructRNA RNA_MultiresModifier;
extern StructRNA RNA_MusgraveTexture;
extern StructRNA RNA_NandController;
extern StructRNA RNA_NearSensor;
extern StructRNA RNA_NlaStrip;
extern StructRNA RNA_NlaTrack;
extern StructRNA RNA_Node;
extern StructRNA RNA_NodeForLoop;
extern StructRNA RNA_NodeGroup;
extern StructRNA RNA_NodeLink;
extern StructRNA RNA_NodeSocket;
extern StructRNA RNA_NodeSocketPanel;
extern StructRNA RNA_NodeTree;
extern StructRNA RNA_NodeWhileLoop;
extern StructRNA RNA_NoiseTexture;
extern StructRNA RNA_NorController;
extern StructRNA RNA_Object;
extern StructRNA RNA_ObjectBase;
extern StructRNA RNA_ObstacleFluidSettings;
extern StructRNA RNA_OceanModifier;
extern StructRNA RNA_OceanTexData;
extern StructRNA RNA_OceanTexture;
extern StructRNA RNA_Operator;
extern StructRNA RNA_OperatorFileListElement;
extern StructRNA RNA_OperatorMousePath;
extern StructRNA RNA_OperatorProperties;
extern StructRNA RNA_OperatorStrokeElement;
extern StructRNA RNA_OperatorMacro;
extern StructRNA RNA_OrController;
extern StructRNA RNA_OutflowFluidSettings;
extern StructRNA RNA_PackedFile;
extern StructRNA RNA_Paint;
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
extern StructRNA RNA_PluginSequence;
extern StructRNA RNA_PluginTexture;
extern StructRNA RNA_PointCache;
extern StructRNA RNA_PointDensity;
extern StructRNA RNA_PointDensityTexture;
extern StructRNA RNA_PointLamp;
extern StructRNA RNA_PointerProperty;
extern StructRNA RNA_Pose;
extern StructRNA RNA_PoseBone;
extern StructRNA RNA_Property;
extern StructRNA RNA_PropertyGroup;
extern StructRNA RNA_PropertyGroupItem;
extern StructRNA RNA_PropertySensor;
extern StructRNA RNA_PythonConstraint;
extern StructRNA RNA_PythonController;
extern StructRNA RNA_QuickTimeSettings;
extern StructRNA RNA_RadarSensor;
extern StructRNA RNA_RandomSensor;
extern StructRNA RNA_RaySensor;
extern StructRNA RNA_Region;
extern StructRNA RNA_RenderEngine;
extern StructRNA RNA_RenderLayer;
extern StructRNA RNA_RenderPass;
extern StructRNA RNA_RenderResult;
extern StructRNA RNA_RenderSettings;
extern StructRNA RNA_RigidBodyJointConstraint;
extern StructRNA RNA_SPHFluidSettings;
extern StructRNA RNA_Scene;
extern StructRNA RNA_SceneGameData;
extern StructRNA RNA_SceneRenderLayer;
extern StructRNA RNA_SceneSequence;
extern StructRNA RNA_SceneObjects;
extern StructRNA RNA_Scopes;
extern StructRNA RNA_Screen;
extern StructRNA RNA_ScrewModifier;
extern StructRNA RNA_Sculpt;
extern StructRNA RNA_Sensor;
extern StructRNA RNA_Sequence;
extern StructRNA RNA_SequenceColorBalance;
extern StructRNA RNA_SequenceCrop;
extern StructRNA RNA_SequenceEditor;
extern StructRNA RNA_SequenceElement;
extern StructRNA RNA_SequenceProxy;
extern StructRNA RNA_SequenceTransform;
extern StructRNA RNA_ShaderNode;
extern StructRNA RNA_ShaderNodeCameraData;
extern StructRNA RNA_ShaderNodeCombineRGB;
extern StructRNA RNA_ShaderNodeExtendedMaterial;
extern StructRNA RNA_ShaderNodeGeometry;
extern StructRNA RNA_ShaderNodeHueSaturation;
extern StructRNA RNA_ShaderNodeInvert;
extern StructRNA RNA_ShaderNodeMapping;
extern StructRNA RNA_ShaderNodeMaterial;
extern StructRNA RNA_ShaderNodeMath;
extern StructRNA RNA_ShaderNodeMixRGB;
extern StructRNA RNA_ShaderNodeNormal;
extern StructRNA RNA_ShaderNodeOutput;
extern StructRNA RNA_ShaderNodeRGB;
extern StructRNA RNA_ShaderNodeRGBCurve;
extern StructRNA RNA_ShaderNodeRGBToBW;
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
extern StructRNA RNA_SmokeCollSettings;
extern StructRNA RNA_SmokeDomainSettings;
extern StructRNA RNA_SmokeFlowSettings;
extern StructRNA RNA_SmokeModifier;
extern StructRNA RNA_SmoothModifier;
extern StructRNA RNA_SoftBodyModifier;
extern StructRNA RNA_SoftBodySettings;
extern StructRNA RNA_SolidifyModifier;
extern StructRNA RNA_Sound;
extern StructRNA RNA_SoundSequence;
extern StructRNA RNA_Space;
extern StructRNA RNA_SpaceConsole;
extern StructRNA RNA_SpaceDopeSheetEditor;
extern StructRNA RNA_SpaceFileBrowser;
extern StructRNA RNA_SpaceGraphEditor;
extern StructRNA RNA_SpaceImageEditor;
extern StructRNA RNA_SpaceInfo;
extern StructRNA RNA_SpaceLogicEditor;
extern StructRNA RNA_SpaceNLA;
extern StructRNA RNA_SpaceNodeEditor;
extern StructRNA RNA_SpaceOutliner;
extern StructRNA RNA_SpaceProperties;
extern StructRNA RNA_SpaceSequenceEditor;
extern StructRNA RNA_SpaceTextEditor;
extern StructRNA RNA_SpaceTimeline;
extern StructRNA RNA_SpaceUVEditor;
extern StructRNA RNA_SpaceUserPreferences;
extern StructRNA RNA_SpaceView3D;
extern StructRNA RNA_SpaceClipEditor;
extern StructRNA RNA_Speaker;
extern StructRNA RNA_SpeedControlSequence;
extern StructRNA RNA_Spline;
extern StructRNA RNA_SplineIKConstraint;
extern StructRNA RNA_SpotLamp;
extern StructRNA RNA_StretchToConstraint;
extern StructRNA RNA_StringProperty;
extern StructRNA RNA_Struct;
extern StructRNA RNA_StucciTexture;
extern StructRNA RNA_SubsurfModifier;
extern StructRNA RNA_SunLamp;
extern StructRNA RNA_SurfaceCurve;
extern StructRNA RNA_SurfaceModifier;
extern StructRNA RNA_TexMapping;
extern StructRNA RNA_Text;
extern StructRNA RNA_TextBox;
extern StructRNA RNA_TextCharacterFormat;
extern StructRNA RNA_TextCurve;
extern StructRNA RNA_TextLine;
extern StructRNA RNA_TextMarker;
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
extern StructRNA RNA_ThemeProperties;
extern StructRNA RNA_ThemeSequenceEditor;
extern StructRNA RNA_ThemeStyle;
extern StructRNA RNA_ThemeTextEditor;
extern StructRNA RNA_ThemeTimeline;
extern StructRNA RNA_ThemeUserInterface;
extern StructRNA RNA_ThemeUserPreferences;
extern StructRNA RNA_ThemeView3D;
extern StructRNA RNA_ThemeWidgetColors;
extern StructRNA RNA_ThemeWidgetStateColors;
extern StructRNA RNA_TimelineMarker;
extern StructRNA RNA_Timer;
extern StructRNA RNA_ToolSettings;
extern StructRNA RNA_TouchSensor;
extern StructRNA RNA_TrackToConstraint;
extern StructRNA RNA_TransformConstraint;
extern StructRNA RNA_TransformSequence;
extern StructRNA RNA_UILayout;
extern StructRNA RNA_UIListItem;
extern StructRNA RNA_UVProjectModifier;
extern StructRNA RNA_UVProjector;
extern StructRNA RNA_UnitSettings;
extern StructRNA RNA_UnknownType;
extern StructRNA RNA_UserPreferences;
extern StructRNA RNA_UserPreferencesEdit;
extern StructRNA RNA_UserPreferencesFilePaths;
extern StructRNA RNA_UserPreferencesInput;
extern StructRNA RNA_UserPreferencesSystem;
extern StructRNA RNA_UserPreferencesView;
extern StructRNA RNA_UserSolidLight;
extern StructRNA RNA_VectorFont;
extern StructRNA RNA_VertexGroup;
extern StructRNA RNA_VertexGroupElement;
extern StructRNA RNA_VertexPaint;
extern StructRNA RNA_VoronoiTexture;
extern StructRNA RNA_VoxelData;
extern StructRNA RNA_VoxelDataTexture;
extern StructRNA RNA_WarpModifier;
extern StructRNA RNA_WaveModifier;
extern StructRNA RNA_VertexWeightEditModifier;
extern StructRNA RNA_VertexWeightMixModifier;
extern StructRNA RNA_VertexWeightProximityModifier;
extern StructRNA RNA_Window;
extern StructRNA RNA_WindowManager;
extern StructRNA RNA_WipeSequence;
extern StructRNA RNA_WoodTexture;
extern StructRNA RNA_World;
extern StructRNA RNA_WorldAmbientOcclusion;
extern StructRNA RNA_WorldLighting;
extern StructRNA RNA_WorldMistSettings;
extern StructRNA RNA_WorldStarsSettings;
extern StructRNA RNA_WorldTextureSlot;
extern StructRNA RNA_XnorController;
extern StructRNA RNA_XorController;

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

void RNA_blender_rna_pointer_create(PointerRNA *r_ptr);
void RNA_pointer_recast(PointerRNA *ptr, PointerRNA *r_ptr);

extern const PointerRNA PointerRNA_NULL;

/* Structs */

StructRNA *RNA_struct_find(const char *identifier);

const char *RNA_struct_identifier(StructRNA *type);
const char *RNA_struct_ui_name(StructRNA *type);
const char *RNA_struct_ui_description(StructRNA *type);
int RNA_struct_ui_icon(StructRNA *type);

PropertyRNA *RNA_struct_name_property(StructRNA *type);
PropertyRNA *RNA_struct_iterator_property(StructRNA *type);
StructRNA *RNA_struct_base(StructRNA *type);

int RNA_struct_is_ID(StructRNA *type);
int RNA_struct_is_a(StructRNA *type, StructRNA *srna);

int RNA_struct_undo_check(StructRNA *type);

StructRegisterFunc RNA_struct_register(StructRNA *type);
StructUnregisterFunc RNA_struct_unregister(StructRNA *type);
void **RNA_struct_instance(PointerRNA *ptr);

void *RNA_struct_py_type_get(StructRNA *srna);
void RNA_struct_py_type_set(StructRNA *srna, void *py_type);

void *RNA_struct_blender_type_get(StructRNA *srna);
void RNA_struct_blender_type_set(StructRNA *srna, void *blender_type);

struct IDProperty *RNA_struct_idprops(PointerRNA *ptr, int create);
int RNA_struct_idprops_check(StructRNA *srna);
int RNA_struct_idprops_register_check(StructRNA *type);
int RNA_struct_idprops_unset(PointerRNA *ptr, const char *identifier);

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier);
int RNA_struct_contains_property(PointerRNA *ptr, PropertyRNA *prop_test);

/* lower level functions for access to type properties */
const struct ListBase *RNA_struct_type_properties(StructRNA *srna);
PropertyRNA *RNA_struct_type_find_property(StructRNA *srna, const char *identifier);

FunctionRNA *RNA_struct_find_function(PointerRNA *ptr, const char *identifier);
const struct ListBase *RNA_struct_type_functions(StructRNA *srna);

char *RNA_struct_name_get_alloc(PointerRNA *ptr, char *fixedbuf, int fixedlen, int *r_len);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

const char *RNA_property_identifier(PropertyRNA *prop);
const char *RNA_property_description(PropertyRNA *prop);

PropertyType RNA_property_type(PropertyRNA *prop);
PropertySubType RNA_property_subtype(PropertyRNA *prop);
PropertyUnit RNA_property_unit(PropertyRNA *prop);
int RNA_property_flag(PropertyRNA *prop);
void *RNA_property_py_data_get(PropertyRNA *prop);

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_array_check(PropertyRNA *prop);
int RNA_property_multi_array_length(PointerRNA *ptr, PropertyRNA *prop, int dimension);
int RNA_property_array_dimension(PointerRNA *ptr, PropertyRNA *prop, int length[]);
char RNA_property_array_item_char(PropertyRNA *prop, int index);
int RNA_property_array_item_index(PropertyRNA *prop, char name);

int RNA_property_string_maxlength(PropertyRNA *prop);

const char *RNA_property_ui_name(PropertyRNA *prop);
const char *RNA_property_ui_description(PropertyRNA *prop);
int RNA_property_ui_icon(PropertyRNA *prop);

/* Dynamic Property Information */

void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PointerRNA *ptr, PropertyRNA *prop, float *softmin, float *softmax, float *step, float *precision);

int RNA_property_float_clamp(PointerRNA *ptr, PropertyRNA *prop, float *value);
int RNA_property_int_clamp(PointerRNA *ptr, PropertyRNA *prop, int *value);

int RNA_enum_identifier(EnumPropertyItem *item, const int value, const char **identifier);
int RNA_enum_bitflag_identifiers(EnumPropertyItem *item, const int value, const char **identifier);
int RNA_enum_name(EnumPropertyItem *item, const int value, const char **name);
int RNA_enum_description(EnumPropertyItem *item, const int value, const char **description);

void RNA_property_enum_items(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, EnumPropertyItem **item, int *totitem, int *free);
void RNA_property_enum_items_gettexted(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, EnumPropertyItem **item, int *totitem, int *free);
int RNA_property_enum_value(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *value);
int RNA_property_enum_identifier(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **identifier);
int RNA_property_enum_name(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **name);
int RNA_property_enum_bitflag_identifiers(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, const int value, const char **identifier);

StructRNA *RNA_property_pointer_type(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_pointer_poll(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *value);

int RNA_property_editable(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_editable_index(PointerRNA *ptr, PropertyRNA *prop, int index);
int RNA_property_editable_flag(PointerRNA *ptr, PropertyRNA *prop); /* without lib check, only checks the flag */
int RNA_property_animateable(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_animated(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_path_from_ID_check(PointerRNA *ptr, PropertyRNA *prop); /* slow, use with care */

void RNA_property_update(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_update_main(struct Main *bmain, struct Scene *scene, PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_update_check(struct PropertyRNA *prop);

void RNA_property_update_cache_add(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_update_cache_flush(struct Main *bmain, struct Scene *scene);
void RNA_property_update_cache_free(void);

/* Property Data */

int RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, int value);
void RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
int RNA_property_boolean_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
void RNA_property_boolean_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);
int RNA_property_boolean_get_default(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_get_default_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
int RNA_property_boolean_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value);
void RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
void RNA_property_int_get_array_range(PointerRNA *ptr, PropertyRNA *prop, int values[2]);
int RNA_property_int_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
void RNA_property_int_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);
int RNA_property_int_get_default(PointerRNA *ptr, PropertyRNA *prop);
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
void RNA_property_float_get_default_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
float RNA_property_float_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen, int *r_len);
void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value);
int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_string_get_default(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_default_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen);
int RNA_property_string_default_length(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value);
int RNA_property_enum_get_default(PointerRNA *ptr, PropertyRNA *prop);
void *RNA_property_enum_py_data_get(PropertyRNA *prop);

PointerRNA RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_pointer_set(PointerRNA *ptr, PropertyRNA *prop, PointerRNA ptr_value);
PointerRNA RNA_property_pointer_get_default(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_collection_begin(PointerRNA *ptr, PropertyRNA *prop, CollectionPropertyIterator *iter);
void RNA_property_collection_next(CollectionPropertyIterator *iter);
void RNA_property_collection_end(CollectionPropertyIterator *iter);
int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_collection_lookup_index(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *t_ptr);
int RNA_property_collection_lookup_int(PointerRNA *ptr, PropertyRNA *prop, int key, PointerRNA *r_ptr);
int RNA_property_collection_lookup_string(PointerRNA *ptr, PropertyRNA *prop, const char *key, PointerRNA *r_ptr);
int RNA_property_collection_assign_int(PointerRNA *ptr, PropertyRNA *prop, const int key, const PointerRNA *assign_ptr);
int RNA_property_collection_type_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);

/* efficient functions to set properties for arrays */
int RNA_property_collection_raw_array(PointerRNA *ptr, PropertyRNA *prop, PropertyRNA *itemprop, RawArray *array);
int RNA_property_collection_raw_get(struct ReportList *reports, PointerRNA *ptr, PropertyRNA *prop, const char *propname, void *array, RawPropertyType type, int len);
int RNA_property_collection_raw_set(struct ReportList *reports, PointerRNA *ptr, PropertyRNA *prop, const char *propname, void *array, RawPropertyType type, int len);
int RNA_raw_type_sizeof(RawPropertyType type);
RawPropertyType RNA_property_raw_type(PropertyRNA *prop);


/* to create ID property groups */
void RNA_property_pointer_add(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_pointer_remove(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_collection_add(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);
int RNA_property_collection_remove(PointerRNA *ptr, PropertyRNA *prop, int key);
void RNA_property_collection_clear(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_collection_move(PointerRNA *ptr, PropertyRNA *prop, int key, int pos);

/* copy/reset */
int RNA_property_copy(PointerRNA *ptr, PointerRNA *fromptr, PropertyRNA *prop, int index);
int RNA_property_reset(PointerRNA *ptr, PropertyRNA *prop, int index);

/* Path
 *
 * Experimental method to refer to structs and properties with a string,
 * using a syntax like: scenes[0].objects["Cube"].data.verts[7].co
 *
 * This provides a way to refer to RNA data while being detached from any
 * particular pointers, which is useful in a number of applications, like
 * UI code or Actions, though efficiency is a concern. */

char *RNA_path_append(const char *path, PointerRNA *ptr, PropertyRNA *prop,
	int intkey, const char *strkey);
char *RNA_path_back(const char *path);

int RNA_path_resolve(PointerRNA *ptr, const char *path,
		PointerRNA *r_ptr, PropertyRNA **r_prop);

int RNA_path_resolve_full(PointerRNA *ptr, const char *path,
		PointerRNA *r_ptr, PropertyRNA **r_prop, int *index);

char *RNA_path_from_ID_to_struct(PointerRNA *ptr);
char *RNA_path_from_ID_to_property(PointerRNA *ptr, PropertyRNA *prop);

/* Quick name based property access
 *
 * These are just an easier way to access property values without having to
 * call RNA_struct_find_property. The names have to exist as RNA properties
 * for the type in the pointer, if they do not exist an error will be printed.
 *
 * There is no support for pointers and collections here yet, these can be 
 * added when ID properties support them. */

int RNA_boolean_get(PointerRNA *ptr, const char *name);
void RNA_boolean_set(PointerRNA *ptr, const char *name, int value);
void RNA_boolean_get_array(PointerRNA *ptr, const char *name, int *values);
void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const int *values);

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
void RNA_enum_set_identifier(PointerRNA *ptr, const char *name, const char *id);
int RNA_enum_is_equal(struct bContext *C, PointerRNA *ptr, const char *name, const char *enumname);

/* lower level functions that don't use a PointerRNA */
int RNA_enum_value_from_id(EnumPropertyItem *item, const char *identifier, int *value);
int RNA_enum_id_from_value(EnumPropertyItem *item, int value, const char **identifier);
int RNA_enum_icon_from_value(EnumPropertyItem *item, int value, int *icon);

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

#define RNA_BEGIN(sptr, itemptr, propname)                                    \
	{                                                                         \
		CollectionPropertyIterator rna_macro_iter;                            \
		for(RNA_collection_begin(sptr, propname, &rna_macro_iter);            \
		    rna_macro_iter.valid;                                             \
		    RNA_property_collection_next(&rna_macro_iter))                    \
		{                                                                     \
			PointerRNA itemptr= rna_macro_iter.ptr;

#define RNA_END                                                               \
		}                                                                     \
		RNA_property_collection_end(&rna_macro_iter);                         \
	}

#define RNA_PROP_BEGIN(sptr, itemptr, prop)                                   \
	{                                                                         \
		CollectionPropertyIterator rna_macro_iter;                            \
		for(RNA_property_collection_begin(sptr, prop, &rna_macro_iter);       \
			rna_macro_iter.valid;                                             \
			RNA_property_collection_next(&rna_macro_iter))                    \
		{                                                                     \
			PointerRNA itemptr= rna_macro_iter.ptr;

#define RNA_PROP_END                                                          \
		}                                                                     \
		RNA_property_collection_end(&rna_macro_iter);                         \
	}

#define RNA_STRUCT_BEGIN(sptr, prop)                                          \
	{                                                                         \
		CollectionPropertyIterator rna_macro_iter;                            \
		for(RNA_property_collection_begin(                                    \
					sptr,                                                     \
					RNA_struct_iterator_property(sptr->type),                 \
					&rna_macro_iter);                                         \
			rna_macro_iter.valid;                                             \
			RNA_property_collection_next(&rna_macro_iter))                    \
		{                                                                     \
			PropertyRNA *prop= rna_macro_iter.ptr.data;

#define RNA_STRUCT_END                                                        \
		}                                                                     \
		RNA_property_collection_end(&rna_macro_iter);                         \
	}

/* check if the idproperty exists, for operators */
int RNA_property_is_set(PointerRNA *ptr, PropertyRNA *prop);
int RNA_struct_property_is_set(PointerRNA *ptr, const char *identifier);
int RNA_property_is_idprop(PropertyRNA *prop);

/* python compatible string representation of this property, (must be freed!) */
char *RNA_property_as_string(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop);
char *RNA_pointer_as_string(struct bContext *C, PointerRNA *ptr);

/* Function */

const char *RNA_function_identifier(FunctionRNA *func);
const char *RNA_function_ui_description(FunctionRNA *func);
int RNA_function_flag(FunctionRNA *func);
int RNA_function_defined(FunctionRNA *func);

PropertyRNA *RNA_function_get_parameter(PointerRNA *ptr, FunctionRNA *func, int index);
PropertyRNA *RNA_function_find_parameter(PointerRNA *ptr, FunctionRNA *func, const char *identifier);
const struct ListBase *RNA_function_defined_parameters(FunctionRNA *func);

/* Utility */

ParameterList *RNA_parameter_list_create(ParameterList *parms, PointerRNA *ptr, FunctionRNA *func);
void RNA_parameter_list_free(ParameterList *parms);
int  RNA_parameter_list_size(ParameterList *parms);
int  RNA_parameter_list_arg_count(ParameterList *parms);
int  RNA_parameter_list_ret_count(ParameterList *parms);

void RNA_parameter_list_begin(ParameterList *parms, ParameterIterator *iter);
void RNA_parameter_list_next(ParameterIterator *iter);
void RNA_parameter_list_end(ParameterIterator *iter);

void RNA_parameter_get(ParameterList *parms, PropertyRNA *parm, void **value);
void RNA_parameter_get_lookup(ParameterList *parms, const char *identifier, void **value);
void RNA_parameter_set(ParameterList *parms, PropertyRNA *parm, const void *value);
void RNA_parameter_set_lookup(ParameterList *parms, const char *identifier, const void *value);
int RNA_parameter_length_get(ParameterList *parms, PropertyRNA *parm);
int RNA_parameter_length_get_data(ParameterList *parms, PropertyRNA *parm, void *data);
void RNA_parameter_length_set(ParameterList *parms, PropertyRNA *parm, int length);
void RNA_parameter_length_set_data(ParameterList *parms, PropertyRNA *parm, void *data, int length);

int RNA_function_call(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms);
int RNA_function_call_lookup(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, const char *identifier, ParameterList *parms);

int RNA_function_call_direct(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, FunctionRNA *func, const char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 5, 6)))
#endif
;
int RNA_function_call_direct_lookup(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, const char *identifier, const char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 5, 6)))
#endif
;
int RNA_function_call_direct_va(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, FunctionRNA *func, const char *format, va_list args);
int RNA_function_call_direct_va_lookup(struct bContext *C, struct ReportList *reports, PointerRNA *ptr, const char *identifier, const char *format, va_list args);

/* ID */

short RNA_type_to_ID_code(StructRNA *type);
StructRNA *ID_code_to_RNA_type(short idcode);


/* macro which inserts the function name */
#if defined __GNUC__ || defined __sun
#  define RNA_warning(format, args...) _RNA_warning("%s: " format "\n", __func__, ##args)
#else
#  define RNA_warning(format, ...) _RNA_warning("%s: " format "\n", __FUNCTION__, __VA_ARGS__)
#endif

void _RNA_warning(const char *format, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;

#ifdef __cplusplus
}
#endif

#endif /* RNA_ACCESS_H */
