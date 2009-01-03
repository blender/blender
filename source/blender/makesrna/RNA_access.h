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

#include "RNA_types.h"

struct bContext;
struct ID;
struct Main;

/* Types */

extern BlenderRNA BLENDER_RNA;

extern StructRNA RNA_Actuator;
extern StructRNA RNA_ActuatorSensor;
extern StructRNA RNA_AlwaysSensor;
extern StructRNA RNA_AndController;
extern StructRNA RNA_Armature;
extern StructRNA RNA_ArmatureModifier;
extern StructRNA RNA_ArrayModifier;
extern StructRNA RNA_BPoint;
extern StructRNA RNA_BevelModifier;
extern StructRNA RNA_BezTriple;
extern StructRNA RNA_BlenderRNA;
extern StructRNA RNA_Bone;
extern StructRNA RNA_BooleanModifier;
extern StructRNA RNA_BooleanProperty;
extern StructRNA RNA_Brush;
extern StructRNA RNA_BuildModifier;
extern StructRNA RNA_Camera;
extern StructRNA RNA_CastModifier;
extern StructRNA RNA_CharInfo;
extern StructRNA RNA_ClothModifier;
extern StructRNA RNA_CollectionProperty;
extern StructRNA RNA_CollisionModifier;
extern StructRNA RNA_CollisionSensor;
extern StructRNA RNA_CollisionSettings;
extern StructRNA RNA_ColorSequence;
extern StructRNA RNA_Constraint;
extern StructRNA RNA_ControlFluidSettings;
extern StructRNA RNA_Controller;
extern StructRNA RNA_Curve;
extern StructRNA RNA_CurveMap;
extern StructRNA RNA_CurveMapPoint;
extern StructRNA RNA_CurveMapping;
extern StructRNA RNA_CurveModifier;
extern StructRNA RNA_DecimateModifier;
extern StructRNA RNA_DelaySensor;
extern StructRNA RNA_DisplaceModifier;
extern StructRNA RNA_DomainFluidSettings;
extern StructRNA RNA_EdgeSplitModifier;
extern StructRNA RNA_EffectSequence;
extern StructRNA RNA_EnumProperty;
extern StructRNA RNA_EnumPropertyItem;
extern StructRNA RNA_EnvironmentMap;
extern StructRNA RNA_ExplodeModifier;
extern StructRNA RNA_ExpressionController;
extern StructRNA RNA_FieldSettings;
extern StructRNA RNA_FloatProperty;
extern StructRNA RNA_FluidFluidSettings;
extern StructRNA RNA_FluidSettings;
extern StructRNA RNA_FluidSimulationModifier;
extern StructRNA RNA_GameBooleanProperty;
extern StructRNA RNA_GameFloatProperty;
extern StructRNA RNA_GameIntProperty;
extern StructRNA RNA_GameProperty;
extern StructRNA RNA_GameSoftBodySettings;
extern StructRNA RNA_GameStringProperty;
extern StructRNA RNA_GameTimeProperty;
extern StructRNA RNA_GlowSequence;
extern StructRNA RNA_Group;
extern StructRNA RNA_HookModifier;
extern StructRNA RNA_ID;
extern StructRNA RNA_IDProperty;
extern StructRNA RNA_IDPropertyGroup;
extern StructRNA RNA_Image;
extern StructRNA RNA_ImageSequence;
extern StructRNA RNA_ImageUser;
extern StructRNA RNA_InflowFluidSettings;
extern StructRNA RNA_IntProperty;
extern StructRNA RNA_Ipo;
extern StructRNA RNA_IpoCurve;
extern StructRNA RNA_IpoDriver;
extern StructRNA RNA_JoystickSensor;
extern StructRNA RNA_Key;
extern StructRNA RNA_KeyboardSensor;
extern StructRNA RNA_Lamp;
extern StructRNA RNA_Lattice;
extern StructRNA RNA_LatticeModifier;
extern StructRNA RNA_LatticePoint;
extern StructRNA RNA_Library;
extern StructRNA RNA_MCol;
extern StructRNA RNA_MColLayer;
extern StructRNA RNA_MFloatProperty;
extern StructRNA RNA_MFloatPropertyLayer;
extern StructRNA RNA_MIntProperty;
extern StructRNA RNA_MIntPropertyLayer;
extern StructRNA RNA_MSticky;
extern StructRNA RNA_MStringProperty;
extern StructRNA RNA_MStringPropertyLayer;
extern StructRNA RNA_Main;
extern StructRNA RNA_MaskModifier;
extern StructRNA RNA_Material;
extern StructRNA RNA_Mesh;
extern StructRNA RNA_MeshDeformModifier;
extern StructRNA RNA_MeshEdge;
extern StructRNA RNA_MeshFace;
extern StructRNA RNA_MeshMultires;
extern StructRNA RNA_MeshTextureFace;
extern StructRNA RNA_MeshTextureFaceLayer;
extern StructRNA RNA_MeshVertex;
extern StructRNA RNA_MeshVertexGroup;
extern StructRNA RNA_MessageSensor;
extern StructRNA RNA_MetaBall;
extern StructRNA RNA_MetaElement;
extern StructRNA RNA_MetaSequence;
extern StructRNA RNA_MirrorModifier;
extern StructRNA RNA_Modifier;
extern StructRNA RNA_MouseSensor;
extern StructRNA RNA_MovieSequence;
extern StructRNA RNA_NandController;
extern StructRNA RNA_NearSensor;
extern StructRNA RNA_Node;
extern StructRNA RNA_NodeTree;
extern StructRNA RNA_NorController;
extern StructRNA RNA_Object;
extern StructRNA RNA_ObjectGameSettings;
extern StructRNA RNA_ObstacleFluidSettings;
extern StructRNA RNA_Operator;
extern StructRNA RNA_OperatorMousePath;
extern StructRNA RNA_OperatorProperties;
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
extern StructRNA RNA_PointCache;
extern StructRNA RNA_PointerProperty;
extern StructRNA RNA_Property;
extern StructRNA RNA_PropertySensor;
extern StructRNA RNA_PythonController;
extern StructRNA RNA_RadarSensor;
extern StructRNA RNA_Radiosity;
extern StructRNA RNA_RandomSensor;
extern StructRNA RNA_RaySensor;
extern StructRNA RNA_Region;
extern StructRNA RNA_Scene;
extern StructRNA RNA_SceneSequence;
extern StructRNA RNA_ScrArea;
extern StructRNA RNA_ScrEdge;
extern StructRNA RNA_ScrVert;
extern StructRNA RNA_Screen;
extern StructRNA RNA_ScriptLink;
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
extern StructRNA RNA_SoundListener;
extern StructRNA RNA_SoundSequence;
extern StructRNA RNA_SpeedControlSequence;
extern StructRNA RNA_StringProperty;
extern StructRNA RNA_Struct;
extern StructRNA RNA_SubsurfModifier;
extern StructRNA RNA_Text;
extern StructRNA RNA_TextBox;
extern StructRNA RNA_TextLine;
extern StructRNA RNA_TextMarker;
extern StructRNA RNA_Texture;
extern StructRNA RNA_TouchSensor;
extern StructRNA RNA_TransformSequence;
extern StructRNA RNA_UVProjectModifier;
extern StructRNA RNA_UnknownType;
extern StructRNA RNA_UserPreferences;
extern StructRNA RNA_VectorFont;
extern StructRNA RNA_VertexGroup;
extern StructRNA RNA_WaveModifier;
extern StructRNA RNA_WindowManager;
extern StructRNA RNA_WipeSequence;
extern StructRNA RNA_World;
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
void RNA_id_pointer_create(StructRNA *idtype, struct ID *id, PointerRNA *r_ptr);
void RNA_pointer_create(StructRNA *idtype, struct ID *id, StructRNA *type, void *data, PointerRNA *r_ptr);

void RNA_blender_rna_pointer_create(PointerRNA *r_ptr);

/* Structs */

const char *RNA_struct_identifier(PointerRNA *ptr);
const char *RNA_struct_ui_name(PointerRNA *ptr);
const char *RNA_struct_ui_description(PointerRNA *ptr);

PropertyRNA *RNA_struct_name_property(PointerRNA *ptr);
PropertyRNA *RNA_struct_iterator_property(PointerRNA *ptr);

int RNA_struct_is_ID(PointerRNA *ptr);

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier);
const struct ListBase *RNA_struct_defined_properties(StructRNA *srna);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

const char *RNA_property_identifier(PointerRNA *ptr, PropertyRNA *prop);
PropertyType RNA_property_type(PointerRNA *ptr, PropertyRNA *prop);
PropertySubType RNA_property_subtype(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PointerRNA *ptr, PropertyRNA *prop, float *softmin, float *softmax, float *step, float *precision);

int RNA_property_string_maxlength(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_enum_items(PointerRNA *ptr, PropertyRNA *prop, const EnumPropertyItem **item, int *totitem);
int RNA_property_enum_value(PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *value);
int RNA_property_enum_identifier(PointerRNA *ptr, PropertyRNA *prop, const int value, const char **identifier);


const char *RNA_property_ui_name(PointerRNA *ptr, PropertyRNA *prop);
const char *RNA_property_ui_description(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_editable(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_evaluated(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_update(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop);

/* Property Data */

int RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, int value);
int RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, int index, int value);

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value);
int RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, int index, int value);

float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value);
float RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, int index, float value);

void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen);
int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value);

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value);

void RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);
void RNA_property_pointer_set(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *ptr_value);

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

void RNA_string_get(PointerRNA *ptr, const char *name, char *value);
char *RNA_string_get_alloc(PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen);
int RNA_string_length(PointerRNA *ptr, const char *name);
void RNA_string_set(PointerRNA *ptr, const char *name, const char *value);

void RNA_pointer_get(PointerRNA *ptr, const char *name, PointerRNA *r_value);
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

#endif /* RNA_ACCESS */



