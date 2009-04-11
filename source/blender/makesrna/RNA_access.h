/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RNA_ACCESS
#define RNA_ACCESS

#include <stdarg.h>

#include "DNA_listBase.h"
#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bContext;
struct ID;
struct Main;

/* Types */

extern BlenderRNA BLENDER_RNA;

extern StructRNA RNA_Action;
extern StructRNA RNA_ActionGroup;
extern StructRNA RNA_Actuator;
extern StructRNA RNA_ActuatorSensor;
extern StructRNA RNA_AlwaysSensor;
extern StructRNA RNA_AndController;
extern StructRNA RNA_AnimData;
extern StructRNA RNA_AnyType;
extern StructRNA RNA_Area;
extern StructRNA RNA_AreaLamp;
extern StructRNA RNA_Armature;
extern StructRNA RNA_ArmatureModifier;
extern StructRNA RNA_ArrayModifier;
extern StructRNA RNA_BevelModifier;
extern StructRNA RNA_BezierCurvePoint;
extern StructRNA RNA_BlenderRNA;
extern StructRNA RNA_BlendTexture;
extern StructRNA RNA_Bone;
extern StructRNA RNA_BooleanModifier;
extern StructRNA RNA_BooleanProperty;
extern StructRNA RNA_Brush;
extern StructRNA RNA_BuildModifier;
extern StructRNA RNA_Camera;
extern StructRNA RNA_CastModifier;
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
extern StructRNA RNA_Constraint;
extern StructRNA RNA_Context;
extern StructRNA RNA_ControlFluidSettings;
extern StructRNA RNA_Controller;
extern StructRNA RNA_Curve;
extern StructRNA RNA_CurveMap;
extern StructRNA RNA_CurveMapPoint;
extern StructRNA RNA_CurveMapping;
extern StructRNA RNA_CurveModifier;
extern StructRNA RNA_CurvePoint;
extern StructRNA RNA_DecimateModifier;
extern StructRNA RNA_DelaySensor;
extern StructRNA RNA_DisplaceModifier;
extern StructRNA RNA_DistortedNoiseTexture;
extern StructRNA RNA_DomainFluidSettings;
extern StructRNA RNA_Driver;
extern StructRNA RNA_EdgeSplitModifier;
extern StructRNA RNA_EffectSequence;
extern StructRNA RNA_EnumProperty;
extern StructRNA RNA_EnumPropertyItem;
extern StructRNA RNA_EnvironmentMap;
extern StructRNA RNA_EnvironmentMapTexture;
extern StructRNA RNA_ExplodeModifier;
extern StructRNA RNA_ExpressionController;
extern StructRNA RNA_FCurve;
extern StructRNA RNA_FieldSettings;
extern StructRNA RNA_FloatProperty;
extern StructRNA RNA_FluidFluidSettings;
extern StructRNA RNA_FluidSettings;
extern StructRNA RNA_FluidSimulationModifier;
extern StructRNA RNA_Function;
extern StructRNA RNA_GameBooleanProperty;
extern StructRNA RNA_GameFloatProperty;
extern StructRNA RNA_GameIntProperty;
extern StructRNA RNA_GameObjectSettings;
extern StructRNA RNA_GameProperty;
extern StructRNA RNA_GameSoftBodySettings;
extern StructRNA RNA_GameStringProperty;
extern StructRNA RNA_GameTimerProperty;
extern StructRNA RNA_GlowSequence;
extern StructRNA RNA_Group;
extern StructRNA RNA_HemiLamp;
extern StructRNA RNA_HookModifier;
extern StructRNA RNA_ID;
extern StructRNA RNA_IDProperty;
extern StructRNA RNA_IDPropertyGroup;
extern StructRNA RNA_Image;
extern StructRNA RNA_ImageSequence;
extern StructRNA RNA_ImageTexture;
extern StructRNA RNA_ImageUser;
extern StructRNA RNA_InflowFluidSettings;
extern StructRNA RNA_IntProperty;
extern StructRNA RNA_JoystickSensor;
extern StructRNA RNA_Key;
extern StructRNA RNA_KeyboardSensor;
extern StructRNA RNA_KeyingSet;
extern StructRNA RNA_KeyingSetPath;
extern StructRNA RNA_Lamp;
extern StructRNA RNA_LampSkySettings;
extern StructRNA RNA_LampTextureSlot;
extern StructRNA RNA_Lattice;
extern StructRNA RNA_LatticeModifier;
extern StructRNA RNA_LatticePoint;
extern StructRNA RNA_Library;
extern StructRNA RNA_LocalLamp;
extern StructRNA RNA_MagicTexture;
extern StructRNA RNA_Main;
extern StructRNA RNA_MarbleTexture;
extern StructRNA RNA_MaskModifier;
extern StructRNA RNA_Material;
extern StructRNA RNA_MaterialHalo;
extern StructRNA RNA_MaterialRaytraceMirror;
extern StructRNA RNA_MaterialRaytraceTransparency;
extern StructRNA RNA_MaterialStrand;
extern StructRNA RNA_MaterialSubsurfaceScattering;
extern StructRNA RNA_MaterialTextureSlot;
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
extern StructRNA RNA_MeshMultires;
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
extern StructRNA RNA_MouseSensor;
extern StructRNA RNA_MovieSequence;
extern StructRNA RNA_MultiresModifier;
extern StructRNA RNA_MusgraveTexture;
extern StructRNA RNA_NandController;
extern StructRNA RNA_NearSensor;
extern StructRNA RNA_Node;
extern StructRNA RNA_NodeTree;
extern StructRNA RNA_NoiseTexture;
extern StructRNA RNA_NorController;
extern StructRNA RNA_Object;
extern StructRNA RNA_ObstacleFluidSettings;
extern StructRNA RNA_Operator;
extern StructRNA RNA_OperatorMousePath;
extern StructRNA RNA_OperatorProperties;
extern StructRNA RNA_OperatorStrokeElement;
extern StructRNA RNA_OrController;
extern StructRNA RNA_OutflowFluidSettings;
extern StructRNA RNA_PackedFile;
extern StructRNA RNA_Panel;
extern StructRNA RNA_ParticleFluidSettings;
extern StructRNA RNA_ParticleInstanceModifier;
extern StructRNA RNA_ParticleSettings;
extern StructRNA RNA_ParticleSystem;
extern StructRNA RNA_ParticleSystemModifier;
extern StructRNA RNA_PluginSequence;
extern StructRNA RNA_PluginTexture;
extern StructRNA RNA_PointCache;
extern StructRNA RNA_PointerProperty;
extern StructRNA RNA_Pose;
extern StructRNA RNA_PoseChannel;
extern StructRNA RNA_Property;
extern StructRNA RNA_PropertySensor;
extern StructRNA RNA_PythonController;
extern StructRNA RNA_RadarSensor;
extern StructRNA RNA_Radiosity;
extern StructRNA RNA_RandomSensor;
extern StructRNA RNA_RaySensor;
extern StructRNA RNA_Region;
extern StructRNA RNA_Scene;
extern StructRNA RNA_SceneRenderData;
extern StructRNA RNA_SceneSequence;
extern StructRNA RNA_Screen;
extern StructRNA RNA_ScriptLink;
extern StructRNA RNA_Sculpt;
extern StructRNA RNA_Sensor;
extern StructRNA RNA_Sequence;
extern StructRNA RNA_SequenceColorBalance;
extern StructRNA RNA_SequenceCrop;
extern StructRNA RNA_SequenceEditor;
extern StructRNA RNA_SequenceElement;
extern StructRNA RNA_SequenceProxy;
extern StructRNA RNA_SequenceTransform;
extern StructRNA RNA_ShapeKey;
extern StructRNA RNA_ShapeKeyBezierPoint;
extern StructRNA RNA_ShapeKeyCurvePoint;
extern StructRNA RNA_ShapeKeyPoint;
extern StructRNA RNA_ShrinkwrapModifier;
extern StructRNA RNA_SimpleDeformModifier;
extern StructRNA RNA_SmoothModifier;
extern StructRNA RNA_SoftBodySettings;
extern StructRNA RNA_SoftbodyModifier;
extern StructRNA RNA_Sound;
extern StructRNA RNA_SoundSequence;
extern StructRNA RNA_Space;
extern StructRNA RNA_SpaceImageEditor;
extern StructRNA RNA_SpaceUVEditor;
extern StructRNA RNA_SpaceTextEditor;
extern StructRNA RNA_SpeedControlSequence;
extern StructRNA RNA_SpotLamp;
extern StructRNA RNA_StringProperty;
extern StructRNA RNA_Struct;
extern StructRNA RNA_StucciTexture;
extern StructRNA RNA_SubsurfModifier;
extern StructRNA RNA_SunLamp;
extern StructRNA RNA_Text;
extern StructRNA RNA_TextBox;
extern StructRNA RNA_TextCharacterFormat;
extern StructRNA RNA_TextLine;
extern StructRNA RNA_TextMarker;
extern StructRNA RNA_Texture;
extern StructRNA RNA_TextureSlot;
extern StructRNA RNA_Theme;
extern StructRNA RNA_ThemeDopeSheetEditor;
extern StructRNA RNA_ThemeAudioWindow;
extern StructRNA RNA_ThemeBoneColorSet;
extern StructRNA RNA_ThemeButtonsWindow;
extern StructRNA RNA_ThemeFileBrowser;
extern StructRNA RNA_ThemeImageEditor;
extern StructRNA RNA_ThemeGraphEditor;
extern StructRNA RNA_ThemeNLAEditor;
extern StructRNA RNA_ThemeNodeEditor;
extern StructRNA RNA_ThemeOutliner;
extern StructRNA RNA_ThemeSequenceEditor;
extern StructRNA RNA_ThemeTextEditor;
extern StructRNA RNA_ThemeTimeline;
extern StructRNA RNA_ThemeUserInterface;
extern StructRNA RNA_ThemeUserPreferences;
extern StructRNA RNA_ThemeView3D;
extern StructRNA RNA_TimelineMarker;
extern StructRNA RNA_ToolSettings;
extern StructRNA RNA_TouchSensor;
extern StructRNA RNA_TransformSequence;
extern StructRNA RNA_UVProjectModifier;
extern StructRNA RNA_UnknownType;
extern StructRNA RNA_UserPreferences;
extern StructRNA RNA_UserSolidLight;
extern StructRNA RNA_VectorFont;
extern StructRNA RNA_VertexGroup;
extern StructRNA RNA_VoronoiTexture;
extern StructRNA RNA_VPaint;
extern StructRNA RNA_VertexGroupElement;
extern StructRNA RNA_WaveModifier;
extern StructRNA RNA_WindowManager;
extern StructRNA RNA_WipeSequence;
extern StructRNA RNA_WoodTexture;
extern StructRNA RNA_World;
extern StructRNA RNA_WorldAmbientOcclusion;
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

/* Structs */

const char *RNA_struct_identifier(PointerRNA *ptr);
const char *RNA_struct_ui_name(PointerRNA *ptr);
const char *RNA_struct_ui_description(PointerRNA *ptr);

PropertyRNA *RNA_struct_name_property(PointerRNA *ptr);
PropertyRNA *RNA_struct_iterator_property(PointerRNA *ptr);

int RNA_struct_is_ID(PointerRNA *ptr);
int RNA_struct_is_a(PointerRNA *ptr, StructRNA *srna);

void *RNA_struct_py_type_get(StructRNA *srna);
void RNA_struct_py_type_set(StructRNA *srna, void *py_type);

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier);
const struct ListBase *RNA_struct_defined_properties(StructRNA *srna);

FunctionRNA *RNA_struct_find_function(PointerRNA *ptr, const char *identifier);
const struct ListBase *RNA_struct_defined_functions(StructRNA *srna);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

const char *RNA_property_identifier(PointerRNA *ptr, PropertyRNA *prop);
PropertyType RNA_property_type(PointerRNA *ptr, PropertyRNA *prop);
PropertySubType RNA_property_subtype(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_flag(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PointerRNA *ptr, PropertyRNA *prop, float *softmin, float *softmax, float *step, float *precision);

int RNA_property_string_maxlength(PointerRNA *ptr, PropertyRNA *prop);
StructRNA *RNA_property_pointer_type(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_enum_items(PointerRNA *ptr, PropertyRNA *prop, const EnumPropertyItem **item, int *totitem);
int RNA_property_enum_value(PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *value);
int RNA_property_enum_identifier(PointerRNA *ptr, PropertyRNA *prop, const int value, const char **identifier);

const char *RNA_property_ui_name(PointerRNA *ptr, PropertyRNA *prop);
const char *RNA_property_ui_description(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_editable(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_animateable(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_animated(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_update(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop);

/* Property Data */

int RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, int value);
void RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
int RNA_property_boolean_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
void RNA_property_boolean_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value);
void RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
int RNA_property_int_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
void RNA_property_int_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);

float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value);
void RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
float RNA_property_float_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, const float *values);
void RNA_property_float_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, float value);

void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen);
int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value);

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value);

PointerRNA RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_pointer_set(PointerRNA *ptr, PropertyRNA *prop, PointerRNA ptr_value);

void RNA_property_collection_begin(PointerRNA *ptr, PropertyRNA *prop, CollectionPropertyIterator *iter);
void RNA_property_collection_next(CollectionPropertyIterator *iter);
void RNA_property_collection_end(CollectionPropertyIterator *iter);
int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_collection_lookup_int(PointerRNA *ptr, PropertyRNA *prop, int key, PointerRNA *r_ptr);
int RNA_property_collection_lookup_string(PointerRNA *ptr, PropertyRNA *prop, const char *key, PointerRNA *r_ptr);

/* to create ID property groups */
void RNA_property_pointer_add(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_collection_add(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);
void RNA_property_collection_clear(PointerRNA *ptr, PropertyRNA *prop);

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

char *RNA_path_from_ID_to_property(PointerRNA *ptr, PropertyRNA *prop);

#if 0
/* Dependency
 *
 * Experimental code that will generate callbacks for each dependency
 * between ID types. This may end up being useful for UI
 * and evaluation code that needs to know such dependencies for correct
 * redraws and re-evaluations. */

typedef void (*PropDependencyCallback)(void *udata, PointerRNA *from, PointerRNA *to);
void RNA_test_dependencies_cb(void *udata, PointerRNA *from, PointerRNA *to);

void RNA_generate_dependencies(PointerRNA *mainptr, void *udata, PropDependencyCallback cb);
#endif

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
int RNA_enum_is_equal(PointerRNA *ptr, const char *name, const char *enumname);

/* lower level functions that donr use a PointerRNA */
int	RNA_enum_value_from_id(EnumPropertyItem *item, const char *identifier, int *value);
int	RNA_enum_id_from_value(EnumPropertyItem *item, int value, const char **identifier);

void RNA_string_get(PointerRNA *ptr, const char *name, char *value);
char *RNA_string_get_alloc(PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen);
int RNA_string_length(PointerRNA *ptr, const char *name);
void RNA_string_set(PointerRNA *ptr, const char *name, const char *value);

PointerRNA RNA_pointer_get(PointerRNA *ptr, const char *name);
void RNA_pointer_add(PointerRNA *ptr, const char *name);

void RNA_collection_begin(PointerRNA *ptr, const char *name, CollectionPropertyIterator *iter);
int RNA_collection_length(PointerRNA *ptr, const char *name);
void RNA_collection_add(PointerRNA *ptr, const char *name, PointerRNA *r_value);
void RNA_collection_clear(PointerRNA *ptr, const char *name);

#define RNA_BEGIN(sptr, itemptr, propname) \
	{ \
		CollectionPropertyIterator rna_macro_iter; \
		for(RNA_collection_begin(sptr, propname, &rna_macro_iter); rna_macro_iter.valid; RNA_property_collection_next(&rna_macro_iter)) { \
			PointerRNA itemptr= rna_macro_iter.ptr;

#define RNA_END \
		} \
		RNA_property_collection_end(&rna_macro_iter); \
	}

/* check if the idproperty exists, for operators */
int RNA_property_is_set(PointerRNA *ptr, const char *name);

/* python compatible string representation of this property, (must be freed!) */
char *RNA_property_as_string(PointerRNA *ptr, PropertyRNA *prop);

/* Function */

const char *RNA_function_identifier(PointerRNA *ptr, FunctionRNA *func);
PropertyRNA *RNA_function_return(PointerRNA *ptr, FunctionRNA *func);
const char *RNA_function_ui_description(PointerRNA *ptr, FunctionRNA *func);

PropertyRNA *RNA_function_get_parameter(PointerRNA *ptr, FunctionRNA *func, int index);
PropertyRNA *RNA_function_find_parameter(PointerRNA *ptr, FunctionRNA *func, const char *identifier);
const struct ListBase *RNA_function_defined_parameters(PointerRNA *ptr, FunctionRNA *func);

/* Utility */

ParameterList *RNA_parameter_list_create(PointerRNA *ptr, FunctionRNA *func);
void RNA_parameter_list_free(ParameterList *parms);

void RNA_parameter_list_begin(ParameterList *parms, ParameterIterator *iter);
void RNA_parameter_list_next(ParameterIterator *iter);
void RNA_parameter_list_end(ParameterIterator *iter);

void RNA_parameter_get(ParameterList *parms, PropertyRNA *parm, void **value);
void RNA_parameter_get_lookup(ParameterList *parms, const char *identifier, void **value);
void RNA_parameter_set(ParameterList *parms, PropertyRNA *parm, void *value);
void RNA_parameter_set_lookup(ParameterList *parms, const char *identifier, void *value);

int RNA_function_call(PointerRNA *ptr, FunctionRNA *func, ParameterList *parms);
int RNA_function_call_lookup(PointerRNA *ptr, const char *identifier, ParameterList *parms);

int RNA_function_call_direct(PointerRNA *ptr, FunctionRNA *func, const char *format, ...);
int RNA_function_call_direct_lookup(PointerRNA *ptr, const char *identifier, const char *format, ...);
int RNA_function_call_direct_va(PointerRNA *ptr, FunctionRNA *func, const char *format, va_list args);
int RNA_function_call_direct_va_lookup(PointerRNA *ptr, const char *identifier, const char *format, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* RNA_ACCESS */

