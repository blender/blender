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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RNA_INTERNAL_H
#define RNA_INTERNAL_H

#include "UI_resources.h"

#include "rna_internal_types.h"

#define RNA_MAGIC ((int)~0)

struct ID;
struct IDProperty;
struct SDNA;

/* Data structures used during define */

typedef struct ContainerDefRNA {
	void *next, *prev;

	ContainerRNA *cont;
	ListBase properties;
} ContainerDefRNA;

typedef struct FunctionDefRNA {
	ContainerDefRNA cont;

	FunctionRNA *func;
	const char *srna;
	const char *call;
	const char *gencall;
} FunctionDefRNA;

typedef struct PropertyDefRNA {
	struct PropertyDefRNA *next, *prev;

	struct ContainerRNA *cont;
	struct PropertyRNA *prop;

	/* struct */
	const char *dnastructname;
	const char *dnastructfromname;
	const char *dnastructfromprop;

	/* property */
	const char *dnaname;
	const char *dnatype;
	int dnaarraylength;
	int dnapointerlevel;

	/* for finding length of array collections */
	const char *dnalengthstructname;
	const char *dnalengthname;
	int dnalengthfixed;

	int booleanbit, booleannegative;

	/* not to be confused with PROP_ENUM_FLAG
	 * this only allows one of the flags to be set at a time, clearing all others */
	int enumbitflags;
} PropertyDefRNA;

typedef struct StructDefRNA {
	ContainerDefRNA cont;

	struct StructRNA *srna;
	const char *filename;

	const char *dnaname;

	/* for derived structs to find data in some property */
	const char *dnafromname;
	const char *dnafromprop;

	ListBase functions;
} StructDefRNA;

typedef struct AllocDefRNA {
	struct AllocDefRNA *next, *prev;
	void *mem;
} AllocDefRNA;

typedef struct BlenderDefRNA {
	struct SDNA *sdna;
	ListBase structs;
	ListBase allocs;
	struct StructRNA *laststruct;
	int error, silent, preprocess, verify;
} BlenderDefRNA;

extern BlenderDefRNA DefRNA;

/* Define functions for all types */

extern BlenderRNA BLENDER_RNA;

void RNA_def_ID(struct BlenderRNA *brna);
void RNA_def_action(struct BlenderRNA *brna);
void RNA_def_animation(struct BlenderRNA *brna);
void RNA_def_animviz(struct BlenderRNA *brna);
void RNA_def_armature(struct BlenderRNA *brna);
void RNA_def_actuator(struct BlenderRNA *brna);
void RNA_def_boid(struct BlenderRNA *brna);
void RNA_def_brush(struct BlenderRNA *brna);
void RNA_def_brushclone(struct BlenderRNA *brna);
void RNA_def_camera(struct BlenderRNA *brna);
void RNA_def_cloth(struct BlenderRNA *brna);
void RNA_def_color(struct BlenderRNA *brna);
void RNA_def_constraint(struct BlenderRNA *brna);
void RNA_def_context(struct BlenderRNA *brna);
void RNA_def_controller(struct BlenderRNA *brna);
void RNA_def_curve(struct BlenderRNA *brna);
void RNA_def_fluidsim(struct BlenderRNA *brna);
void RNA_def_fcurve(struct BlenderRNA *brna);
void RNA_def_gameproperty(struct BlenderRNA *brna);
void RNA_def_gpencil(struct BlenderRNA *brna);
void RNA_def_group(struct BlenderRNA *brna);
void RNA_def_image(struct BlenderRNA *brna);
void RNA_def_key(struct BlenderRNA *brna);
void RNA_def_lamp(struct BlenderRNA *brna);
void RNA_def_lattice(struct BlenderRNA *brna);
void RNA_def_main(struct BlenderRNA *brna);
void RNA_def_material(struct BlenderRNA *brna);
void RNA_def_mesh(struct BlenderRNA *brna);
void RNA_def_meta(struct BlenderRNA *brna);
void RNA_def_modifier(struct BlenderRNA *brna);
void RNA_def_nla(struct BlenderRNA *brna);
void RNA_def_nodetree(struct BlenderRNA *brna);
void RNA_def_object(struct BlenderRNA *brna);
void RNA_def_object_force(struct BlenderRNA *brna);
void RNA_def_packedfile(struct BlenderRNA *brna);
void RNA_def_particle(struct BlenderRNA *brna);
void RNA_def_pose(struct BlenderRNA *brna);
void RNA_def_render(struct BlenderRNA *brna);
void RNA_def_rna(struct BlenderRNA *brna);
void RNA_def_scene(struct BlenderRNA *brna);
void RNA_def_screen(struct BlenderRNA *brna);
void RNA_def_sculpt_paint(struct BlenderRNA *brna);
void RNA_def_sensor(struct BlenderRNA *brna);
void RNA_def_sequencer(struct BlenderRNA *brna);
void RNA_def_smoke(struct BlenderRNA *brna);
void RNA_def_space(struct BlenderRNA *brna);
void RNA_def_test(struct BlenderRNA *brna);
void RNA_def_text(struct BlenderRNA *brna);
void RNA_def_texture(struct BlenderRNA *brna);
void RNA_def_timeline_marker(struct BlenderRNA *brna);
void RNA_def_sound(struct BlenderRNA *brna);
void RNA_def_ui(struct BlenderRNA *brna);
void RNA_def_userdef(struct BlenderRNA *brna);
void RNA_def_vfont(struct BlenderRNA *brna);
void RNA_def_wm(struct BlenderRNA *brna);
void RNA_def_world(struct BlenderRNA *brna);

/* Common Define functions */

void rna_def_animdata_common(struct StructRNA *srna);

void rna_def_animviz_common(struct StructRNA *srna);
void rna_def_motionpath_common(struct StructRNA *srna);

void rna_def_texmat_common(struct StructRNA *srna, const char *texspace_editable);
void rna_def_mtex_common(struct StructRNA *srna, const char *begin, const char *activeget, const char *activeset, const char *structname, const char *update);
void rna_def_render_layer_common(struct StructRNA *srna, int scene);

void rna_ID_name_get(struct PointerRNA *ptr, char *value);
int rna_ID_name_length(struct PointerRNA *ptr);
void rna_ID_name_set(struct PointerRNA *ptr, const char *value);
struct StructRNA *rna_ID_refine(struct PointerRNA *ptr);
struct IDProperty *rna_ID_idproperties(struct PointerRNA *ptr, int create);
void rna_ID_fake_user_set(struct PointerRNA *ptr, int value);
struct IDProperty *rna_IDPropertyGroup_idproperties(struct PointerRNA *ptr, int create);
void rna_IDPropertyGroup_unregister(const struct bContext *C, struct StructRNA *type);
struct StructRNA *rna_IDPropertyGroup_register(const struct bContext *C, struct ReportList *reports, void *data, const char *identifier, StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free);
struct StructRNA* rna_IDPropertyGroup_refine(struct PointerRNA *ptr);

void rna_object_vgroup_name_index_get(struct PointerRNA *ptr, char *value, int index);
int rna_object_vgroup_name_index_length(struct PointerRNA *ptr, int index);
void rna_object_vgroup_name_index_set(struct PointerRNA *ptr, const char *value, short *index);
void rna_object_vgroup_name_set(struct PointerRNA *ptr, const char *value, char *result, int maxlen);
void rna_object_uvlayer_name_set(struct PointerRNA *ptr, const char *value, char *result, int maxlen);
void rna_object_vcollayer_name_set(struct PointerRNA *ptr, const char *value, char *result, int maxlen);
PointerRNA rna_object_shapekey_index_get(struct ID *id, int value);
int rna_object_shapekey_index_set(struct ID *id, PointerRNA value, int current);

void rna_Object_update(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);
void rna_Object_update_data(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);
void rna_Mesh_update_draw(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);
void rna_TextureSlot_update(struct Main *bmain, struct Scene *scene, struct PointerRNA *ptr);

char *rna_TextureSlot_path(struct PointerRNA *ptr);

/* API functions */

void RNA_api_action(StructRNA *srna);
void RNA_api_armature_edit_bone(StructRNA *srna);
void RNA_api_drivers(StructRNA *srna);
void RNA_api_image(struct StructRNA *srna);
void RNA_api_operator(struct StructRNA *srna);
void RNA_api_macro(struct StructRNA *srna);
void RNA_api_keyconfig(struct StructRNA *srna);
void RNA_api_keyingset(struct StructRNA *srna);
void RNA_api_keymap(struct StructRNA *srna);
void RNA_api_keymapitem(struct StructRNA *srna);
void RNA_api_main(struct StructRNA *srna);
void RNA_api_material(StructRNA *srna);
void RNA_api_mesh(struct StructRNA *srna);
void RNA_api_object(struct StructRNA *srna);
void RNA_api_object_base(struct StructRNA *srna);
void RNA_api_pose_channel(struct StructRNA *srna);
void RNA_api_scene(struct StructRNA *srna);
void RNA_api_scene_render(struct StructRNA *srna);
void RNA_api_text(struct StructRNA *srna);
void RNA_api_ui_layout(struct StructRNA *srna);
void RNA_api_wm(struct StructRNA *srna);

/* main collection functions */
void RNA_def_main_cameras(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_scenes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_objects(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_materials(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_node_groups(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_meshes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_lamps(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_libraries(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_screens(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_window_managers(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_images(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_lattices(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_curves(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_metaballs(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_fonts(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_textures(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_brushes(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_worlds(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_groups(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_texts(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_sounds(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_armatures(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_actions(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_particles(BlenderRNA *brna, PropertyRNA *cprop);
void RNA_def_main_gpencil(BlenderRNA *brna, PropertyRNA *cprop);

/* ID Properties */

extern StringPropertyRNA rna_IDProperty_string;
extern IntPropertyRNA rna_IDProperty_int;
extern IntPropertyRNA rna_IDProperty_int_array;
extern FloatPropertyRNA rna_IDProperty_float;
extern FloatPropertyRNA rna_IDProperty_float_array;
extern PointerPropertyRNA rna_IDProperty_group;
extern CollectionPropertyRNA rna_IDProperty_collection;
extern FloatPropertyRNA rna_IDProperty_double;
extern FloatPropertyRNA rna_IDProperty_double_array;

extern StructRNA RNA_IDProperty;
extern StructRNA RNA_IDPropertyGroup;

struct IDProperty *rna_idproperty_check(struct PropertyRNA **prop, struct PointerRNA *ptr);

/* Builtin Property Callbacks */

void rna_builtin_properties_begin(struct CollectionPropertyIterator *iter, struct PointerRNA *ptr);
void rna_builtin_properties_next(struct CollectionPropertyIterator *iter);
PointerRNA rna_builtin_properties_get(struct CollectionPropertyIterator *iter);
PointerRNA rna_builtin_type_get(struct PointerRNA *ptr);
PointerRNA rna_builtin_properties_lookup_string(PointerRNA *ptr, const char *key);

/* Iterators */

typedef int (*IteratorSkipFunc)(struct CollectionPropertyIterator *iter, void *data);

typedef struct ListBaseIterator {
	Link *link;
	int flag;
	IteratorSkipFunc skip;
} ListBaseIterator;

void rna_iterator_listbase_begin(struct CollectionPropertyIterator *iter, struct ListBase *lb, IteratorSkipFunc skip);
void rna_iterator_listbase_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_listbase_get(struct CollectionPropertyIterator *iter);
void rna_iterator_listbase_end(struct CollectionPropertyIterator *iter);
PointerRNA rna_listbase_lookup_int(PointerRNA *ptr, StructRNA *type, struct ListBase *lb, int index);

typedef struct ArrayIterator {
	char *ptr;
	char *endptr;
	void *free_ptr; /* will be free'd if set */
	int itemsize;
	IteratorSkipFunc skip;
} ArrayIterator;

void rna_iterator_array_begin(struct CollectionPropertyIterator *iter, void *ptr, int itemsize, int length, int free_ptr, IteratorSkipFunc skip);
void rna_iterator_array_next(struct CollectionPropertyIterator *iter);
void *rna_iterator_array_get(struct CollectionPropertyIterator *iter);
void *rna_iterator_array_dereference_get(struct CollectionPropertyIterator *iter);
void rna_iterator_array_end(struct CollectionPropertyIterator *iter);
PointerRNA rna_array_lookup_int(PointerRNA *ptr, StructRNA *type, void *data, int itemsize, int length, int index);

/* Duplicated code since we can't link in blenlib */

void rna_addtail(struct ListBase *listbase, void *vlink);
void rna_freelinkN(struct ListBase *listbase, void *vlink);
void rna_freelistN(struct ListBase *listbase);
PropertyDefRNA *rna_findlink(ListBase *listbase, const char *identifier);

StructDefRNA *rna_find_struct_def(StructRNA *srna);
FunctionDefRNA *rna_find_function_def(FunctionRNA *func);
PropertyDefRNA *rna_find_parameter_def(PropertyRNA *parm);
PropertyDefRNA *rna_find_struct_property_def(StructRNA *srna, PropertyRNA *prop);

/* Pointer Handling */

PointerRNA rna_pointer_inherit_refine(struct PointerRNA *ptr, struct StructRNA *type, void *data);

/* Functions */

int rna_parameter_size(struct PropertyRNA *parm);
int rna_parameter_size_alloc(struct PropertyRNA *parm);

#endif /* RNA_INTERNAL_H */


