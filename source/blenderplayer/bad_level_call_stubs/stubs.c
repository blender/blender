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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * BKE_bad_level_calls function stubs
 */

#if GAMEBLENDER == 1
#include <stdlib.h>
#include "DNA_listBase.h"
#include "RNA_types.h"

struct ARegion;
struct ARegionType;
struct Base;
struct bNodeTree;
struct CSG_FaceIteratorDescriptor;
struct CSG_VertexIteratorDescriptor;
struct ColorBand;
struct CurveMapping;
struct EditBone;
struct EditMesh;
struct ID;
struct FCurve;
struct ImBuf;
struct Image;
struct ImageUser;
struct KeyingSetInfo;
struct KeyingSet;
struct LOD_Decimation_Info;
struct MTex;
struct Main;
struct Material;
struct MenuType;
struct Mesh;
struct ModifierData;
struct NodeBlurData;
struct Object;
struct Render;
struct RenderEngine;
struct RenderLayer;
struct RenderResult;
struct ScrArea;
struct ShadeInput;
struct ShadeResult;
struct SpaceImage;
struct Tex;
struct TexResult;
struct Text;
struct bAction;
struct bArmature;
struct bConstraint;
struct bNode;
struct bPoseChannel;
struct uiLayout;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmWindowManager;
struct View3D;


/*new render funcs */
float *RE_RenderLayerGetPass(struct RenderLayer *rl, int passtype) {return (float *) NULL;}
float RE_filter_value(int type, float x) {return 0.0f;}
struct RenderLayer *RE_GetRenderLayer(struct RenderResult *rr, const char *name) {return (struct RenderLayer *)NULL;}

/* zbuf.c stub */
void antialias_tagbuf(int xsize, int ysize, char *rectmove) {}
void RE_zbuf_accumulate_vecblur(struct NodeBlurData *nbd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect) {}

/* imagetexture.c stub */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result) {}

/* texture.c */
int multitex_thread(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres, short thread, short which_output) {return 0;}
int multitex_ext(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres){return 0;}
int multitex_nodes(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres, short thread, short which_output, struct ShadeInput *shi, struct MTex *mtex) {return 0;}

/* nodes */
struct RenderResult *RE_GetResult(struct Render *re){return (struct RenderResult *) NULL;}
struct Render *RE_GetRender(const char *name){return (struct Render *) NULL;}

/* blenkernel */
char* btempdir(){return (char *) NULL;}
void RE_FreeRenderResult(struct RenderResult *res){}
char* datatoc_bmonofont_ttf(){return (char *) NULL;}
int datatoc_bmonofont_ttf_size(){return 0;}
struct RenderResult *RE_MultilayerConvert(void *exrhandle, int rectx, int recty){return (struct RenderResult *) NULL;}
void RE_GetResultImage(struct Render *re, struct RenderResult *rr){}
int RE_RenderInProgress(struct Render *re){return 0;}
struct Scene *RE_GetScene(struct Render *re){return (struct Scene *) NULL;}
void RE_Database_Free(struct Render *re){}
void RE_FreeRender(struct Render *re){}
void RE_shade_external(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr){}
void RE_DataBase_GetView(struct Render *re, float mat[][4]){}
int externtex(struct MTex *mtex, float *vec, float *tin, float *tr, float *tg, float *tb, float *ta){return 0;}
float texture_value_blend(float tex, float out, float fact, float facg, int blendtype, int flip){return 0.0f;}
void texture_rgb_blend(float *in, float *tex, float *out, float fact, float facg, int blendtype){}
char stipple_quarttone[1]; //GLubyte stipple_quarttone[128]
double elbeemEstimateMemreq(int res, float sx, float sy, float sz, int refine, char *retstr) {return 0.0f;}
struct Render *RE_NewRender(const char *name){return (struct Render*) NULL;}
void RE_SwapResult(struct Render *re, struct RenderResult **rr){}
void RE_BlenderFrame(struct Render *re, struct Scene *scene, int frame){}

/* rna */
float *give_cursor(struct Scene *scene, struct View3D *v3d){return (float *) NULL;}
void WM_menutype_free(void){}
void WM_menutype_freelink(struct MenuType* mt){}
int WM_menutype_add(struct MenuType *mt) {return 0;}
int WM_operator_props_dialog_popup (struct bContext *C, struct wmOperator *op, int width, int height){return 0;}
struct MenuType *WM_menutype_find(const char *idname, int quiet){return (struct MenuType *) NULL;}
void WM_operator_stack_clear(struct bContext *C) {}

void WM_autosave_init(struct bContext *C){}
void WM_jobs_stop_all(struct wmWindowManager *wm){}

struct wmKeyMapItem *WM_keymap_item_find_id(struct wmKeyMap *keymap, int id){return (struct wmKeyMapItem *) NULL;}
int WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event){return 0;}
void WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference){}
void WM_main_add_notifier(unsigned int type, void *reference){}
void ED_armature_bone_rename(struct bArmature *arm, char *oldnamep, char *newnamep){}
struct wmEventHandler *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op){return (struct wmEventHandler *)NULL;};
void ED_armature_edit_bone_remove(struct bArmature *arm, struct EditBone *exBone){}
void object_test_constraints (struct Object *owner){}
void ED_object_parent(struct Object *ob, struct Object *par, int type, const char *substr){}
void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con){}
void ED_node_composit_default(struct Scene *sce){}
void *ED_region_draw_cb_activate(struct ARegionType *art, void(*draw)(const struct bContext *, struct ARegion *, void *), void *custumdata, int type){return 0;} /* XXX this one looks wierd */
void *ED_region_draw_cb_customdata(void *handle){return 0;} /* XXX This one looks wrong also */
void ED_region_draw_cb_exit(struct ARegionType *art, void *handle){}

struct EditBone *ED_armature_bone_get_mirrored(struct ListBase *edbo, struct EditBone *ebo){return (struct EditBone *) NULL;}
struct EditBone *ED_armature_edit_bone_add(struct bArmature *arm, char *name){return (struct EditBone*) NULL;}
struct ListBase *get_active_constraints (struct Object *ob){return (struct ListBase *) NULL;}
struct ListBase *get_constraint_lb(struct Object *ob, struct bConstraint *con, struct bPoseChannel **pchan_r){return (struct ListBase *) NULL;}
int ED_pose_channel_in_IK_chain(struct Object *ob, struct bPoseChannel *pchan){return 0;}

int ED_space_image_show_uvedit(struct SpaceImage *sima, struct Object *obedit){return 0;}
int ED_space_image_show_render(struct SpaceImage *sima){return 0;}
int ED_space_image_show_paint(struct SpaceImage *sima){return 0;}
void ED_space_image_set(struct bContext *C, struct SpaceImage *sima, struct Scene *scene, struct Object *obedit, struct Image *ima){}
struct ImBuf *ED_space_image_buffer(struct SpaceImage *sima){return (struct ImBuf *) NULL;}
void	ED_screen_set_scene(struct bContext *C, struct Scene *scene){}

struct PTCacheEdit *PE_get_current(struct Scene *scene, struct Object *ob){return (struct PTCacheEdit *) NULL;}
void PE_current_changed(struct Scene *scene, struct Object *ob){}

/* rna keymap */
struct wmKeyMap *WM_keymap_active(struct wmWindowManager *wm, struct wmKeyMap *keymap){return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_find(struct wmKeyConfig *keyconf, char *idname, int spaceid, int regionid){return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_add_item(struct wmKeyMap *keymap, char *idname, int type, int val, int modifier, int keymodifier){return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_copy_to_user(struct wmKeyMap *kemap){return (struct wmKeyMap *) NULL;} 
struct wmKeyMap *WM_keymap_list_find(struct ListBase *lb, char *idname, int spaceid, int regionid){return (struct wmKeyMap *) NULL;}
struct wmKeyConfig *WM_keyconfig_add(struct wmWindowManager *wm, char *idname){return (struct wmKeyConfig *) NULL;}
struct wmKeyConfig *WM_keyconfig_add_user(struct wmWindowManager *wm, char *idname){return (struct wmKeyConfig *) NULL;}
void WM_keyconfig_remove(struct wmWindowManager *wm, char *idname){}
void WM_keymap_remove_item(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi){}
void WM_keymap_restore_to_default(struct wmKeyMap *keymap){}
void WM_keymap_restore_item_to_default(struct bContext *C, struct wmKeyMap *keymap, struct wmKeyMapItem *kmi){}
void WM_keymap_properties_reset(struct wmKeyMapItem *kmi){}
int WM_keymap_user_init(struct wmWindowManager *wm, struct wmKeyMap *keymap) {return 0;}
int WM_keymap_item_compare(struct wmKeyMapItem *k1, struct wmKeyMapItem *k2){return 0;}


/* rna editors */
int insert_vert_fcurve(struct FCurve *fcu, float x, float y, short flag){return 0;}
void delete_fcurve_key(struct FCurve *fcu, int index, short do_recalc){}
struct KeyingSetInfo *ANIM_keyingset_info_find_named (const char name[]){return (struct KeyingSetInfo *) NULL;}
struct KeyingSet *ANIM_scene_get_active_keyingset (struct Scene *scene){return (struct KeyingSet *) NULL;}
int ANIM_scene_get_keyingset_index(struct Scene *scene, struct KeyingSet *ks){return 0;}
struct ListBase builtin_keyingsets;
void ANIM_keyingset_info_register (const struct bContext *C, struct KeyingSetInfo *ksi){}
void ANIM_keyingset_info_unregister (const struct bContext *C, struct KeyingSetInfo *ksi){}
short ANIM_add_driver(struct ID *id, const char rna_path[], int array_index, short flag, int type){return 0;}
void ED_space_image_release_buffer(struct SpaceImage *sima, void *lock){}
struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **lock_r){return (struct ImBuf *) NULL;}
char *ED_info_stats_string(struct Scene *scene){return (char *) NULL;}
void ED_area_tag_redraw(struct ScrArea *sa){}
void ED_area_tag_refresh(struct ScrArea *sa){}
void ED_area_newspace(struct bContext *C, struct ScrArea *sa, int type){} 
void WM_event_add_fileselect(struct bContext *C, struct wmOperator *op){}
void WM_cursor_wait (int val) {}
void ED_node_texture_default(struct Tex *tx){}
void ED_node_changed_update(struct bContext *C, struct bNode *node){}
void ED_node_generic_update(struct Main *bmain, struct Scene *scene, struct bNodeTree *ntree, struct bNode *node){}
void ED_view3d_scene_layers_update(struct Main *bmain, struct Scene *scene){}
int ED_view3d_scene_layer_set(int lay, const int *values){return 0;}
void ED_view3d_quadview_update(struct ScrArea *sa, struct ARegion *ar){}
int text_file_modified(struct Text *text){return 0;}
void ED_node_shader_default(struct Material *ma){}
void ED_screen_animation_timer_update(struct bContext *C, int redraws){}
void ED_base_object_select(struct Base *base, short mode){}
int ED_object_modifier_remove(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md){return 0;}
int ED_object_modifier_add(struct ReportList *reports, struct Scene *scene, struct Object *ob, char *name, int type){return 0;}
void ED_object_enter_editmode(struct bContext *C, int flag){}
void ED_object_exit_editmode(struct bContext *C, int flag){}
int uiLayoutGetActive(struct uiLayout *layout){return 0;}
int uiLayoutGetOperatorContext(struct uiLayout *layout){return 0;}
int uiLayoutGetAlignment(struct uiLayout *layout){return 0;}
int uiLayoutGetEnabled(struct uiLayout *layout){return 0;}
float uiLayoutGetScaleX(struct uiLayout *layout){return 0.0f;}
float uiLayoutGetScaleY(struct uiLayout *layout){return 0.0f;}
void uiLayoutSetActive(struct uiLayout *layout, int active){}
void uiLayoutSetOperatorContext(struct uiLayout *layout, int opcontext){}
void uiLayoutSetEnabled(struct uiLayout *layout, int enabled){}
void uiLayoutSetAlignment(struct uiLayout *layout, int alignment){}
void uiLayoutSetScaleX(struct uiLayout *layout, float scale){}
void uiLayoutSetScaleY(struct uiLayout *layout, float scale){}
void ED_base_object_free_and_unlink(struct Scene *scene, struct Base *base){}
void ED_mesh_calc_normals(struct Mesh *me){}
void ED_mesh_geometry_add(struct Mesh *mesh, struct ReportList *reports, int verts, int edges, int faces){}
void ED_mesh_material_add(struct Mesh *me, struct Material *ma){}
void ED_mesh_transform(struct Mesh *me, float *mat){}
void ED_mesh_update(struct Mesh *mesh, struct bContext *C){}
int ED_mesh_color_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me){return 0;}
int ED_mesh_uv_texture_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me){return 0;}
void ED_object_constraint_dependency_update(struct Scene *scene, struct Object *ob){}
void ED_object_constraint_update(struct Object *ob){}
struct bDeformGroup *ED_vgroup_add_name(struct Object *ob, char *name){return (struct bDeformGroup *) NULL;}
void ED_vgroup_vert_add(struct Object *ob, struct bDeformGroup *dg, int vertnum, float weight, int assignmode){}
void ED_sequencer_update_view(struct bContext *C, int view){}
float ED_rollBoneToVector(struct EditBone *bone, float new_up_axis[3]){return 0.0f;}
void ED_space_image_size(struct SpaceImage *sima, int *width, int *height){}

void EM_selectmode_set(struct EditMesh *em){}
void make_editMesh(struct Scene *scene, struct Object *ob){}
void load_editMesh(struct Scene *scene, struct Object *ob){}

void uiItemR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int flag, char *name, int icon){}

struct PointerRNA uiItemFullO(struct uiLayout *layout, char *idname, char *name, int icon, struct IDProperty *properties, int context, int flag){struct PointerRNA a; return a;}
struct uiLayout *uiLayoutRow(struct uiLayout *layout, int align){return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutColumn(struct uiLayout *layout, int align){return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutColumnFlow(struct uiLayout *layout, int number, int align){return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutBox(struct uiLayout *layout){return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutSplit(struct uiLayout *layout, float percentage, int align){return (struct uiLayout *) NULL;}
void uiItemsEnumR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname){}
void uiItemMenuEnumR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *name, int icon){}
void uiItemEnumR_string(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *value, char *name, int icon){}
void uiItemPointerR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, struct PointerRNA *searchptr, char *searchpropname, char *name, int icon){}
void uiItemsEnumO(struct uiLayout *layout, char *opname, char *propname){}
void uiItemEnumO_string(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value_str){}
void uiItemMenuEnumO(struct uiLayout *layout, char *opname, char *propname, char *name, int icon){}
void uiItemBooleanO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, int value){}
void uiItemIntO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, int value){}
void uiItemFloatO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, float value){}
void uiItemStringO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value){}
void uiItemL(struct uiLayout *layout, char *name, int icon){}
void uiItemM(struct uiLayout *layout, struct bContext *C, char *menuname, char *name, int icon){}
void uiItemS(struct uiLayout *layout){}
void uiItemFullR(struct uiLayout *layout, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, int value, int flag, char *name, int icon){}
void uiLayoutSetContextPointer(struct uiLayout *layout, char *name, struct PointerRNA *ptr){}
char *uiLayoutIntrospect(struct uiLayout *layout){return (char *)NULL;}

/* rna template */
void uiTemplateAnyID(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *text){}
void uiTemplatePathBuilder(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *root_ptr, char *text){}
void uiTemplateHeader(struct uiLayout *layout, struct bContext *C, int menus){}
void uiTemplateID(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *newop, char *unlinkop){}
struct uiLayout *uiTemplateModifier(struct uiLayout *layout, struct PointerRNA *ptr){return (struct uiLayout *) NULL;}
struct uiLayout *uiTemplateConstraint(struct uiLayout *layout, struct PointerRNA *ptr){return (struct uiLayout *) NULL;}
void uiTemplatePreview(struct uiLayout *layout, struct ID *id, struct ID *parent, struct MTex *slot){}
void uiTemplateIDPreview(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop, int rows, int cols){}
void uiTemplateCurveMapping(struct uiLayout *layout, struct CurveMapping *cumap, int type, int compact){}
void uiTemplateColorRamp(struct uiLayout *layout, struct ColorBand *coba, int expand){}
void uiTemplateLayers(struct uiLayout *layout, struct PointerRNA *ptr, char *propname){}
void uiTemplateTriColorSet(struct uiLayout *layout, struct PointerRNA *ptr, char *propname){}
void uiTemplateImageLayers(struct uiLayout *layout, struct bContext *C, struct Image *ima, struct ImageUser *iuser){}
ListBase uiTemplateList(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *activeptr, char *activepropname, int rows, int listtype){struct ListBase b = {0,0}; return b;}
void uiTemplateRunningJobs(struct uiLayout *layout, struct bContext *C){}
void uiTemplateOperatorSearch(struct uiLayout *layout){}
void uiTemplateHeader3D(struct uiLayout *layout, struct bContext *C){}
void uiTemplateTextureImage(struct uiLayout *layout, struct bContext *C, struct Tex *tex){}
void uiTemplateImage(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *userptr, int compact){}
void uiTemplateDopeSheetFilter(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr){}
void uiTemplateColorWheel(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int value_slider){}
void uiTemplateHistogram(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int expand){}
void uiTemplateReportsBanner(struct uiLayout *layout, struct bContext *C, struct wmOperator *op){}
void uiTemplateWaveform(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int expand){}
void uiTemplateVectorscope(struct uiLayout *_self, struct PointerRNA *data, char* property, int expand){}

/* rna render */
struct RenderResult *RE_engine_begin_result(struct RenderEngine *engine, int x, int y, int w, int h){return (struct RenderResult *) NULL;}
struct RenderResult *RE_AcquireResultRead(struct Render *re){return (struct RenderResult *) NULL;}
struct RenderResult *RE_AcquireResultWrite(struct Render *re){return (struct RenderResult *) NULL;}
struct RenderStats *RE_GetStats(struct Render *re){return (struct RenderStats *) NULL;}
void RE_engine_update_result(struct RenderEngine *engine, struct RenderResult *result){}
void RE_engine_end_result(struct RenderEngine *engine, struct RenderResult *result){}
void RE_engine_update_stats(struct RenderEngine *engine, char *stats, char *info){}
void RE_layer_load_from_file(struct RenderLayer *layer, struct ReportList *reports, char *filename){}
void RE_result_load_from_file(struct RenderResult *result, struct ReportList *reports, char *filename){}
void RE_AcquireResultImage(struct Render *re, struct RenderResult *rr){}
void RE_ReleaseResult(struct Render *re){}
void RE_ReleaseResultImage(struct Render *re){}
int RE_engine_test_break(struct RenderEngine *engine){return 0;}

/* python */
struct wmOperatorType *WM_operatortype_find(const char *idname, int quiet){return (struct wmOperatorType *) NULL;}
struct wmOperatorType *WM_operatortype_first(){return (struct wmOperatorType *) NULL;}
struct wmOperatorType *WM_operatortype_exists(const char *idname){return (struct wmOperatorType *) NULL;}
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot, const char *idname){return (struct wmOperatorTypeMacro *) NULL;}
int WM_operator_call_py(struct bContext *C, struct wmOperatorType *ot, int context, struct PointerRNA *properties, struct ReportList *reports){return 0;}
int WM_operatortype_remove(const char *idname){return 0;}
int WM_operator_poll(struct bContext *C, struct wmOperatorType *ot){return 0;}
int WM_operator_props_popup(struct bContext *C, struct wmOperator *op, struct wmEvent *event){return 0;}
void WM_operator_properties_free(struct PointerRNA *ptr){}
void WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring){}
void WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot){}
void WM_operatortype_append_ptr(void (*opfunc)(struct wmOperatorType*, void*), void *userdata){}
void WM_operatortype_append_macro_ptr(void (*opfunc)(struct wmOperatorType*, void*), void *userdata){}
void WM_operator_bl_idname(char *to, const char *from){}
void WM_operator_py_idname(char *to, const char *from){}
void WM_operator_ui_popup(struct bContext *C, struct wmOperator *op, int width, int height){}
short insert_keyframe (struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag){return 0;}
short delete_keyframe(struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag){return 0;};
char *WM_operator_pystring(struct bContext *C, struct wmOperatorType *ot, struct PointerRNA *opptr, int all_args){return (char *)NULL;}
struct wmKeyMapItem *WM_modalkeymap_add_item(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value){return (struct wmKeyMapItem *)NULL;}
struct wmKeyMap *WM_modalkeymap_add(struct wmKeyConfig *keyconf, char *idname, EnumPropertyItem *items){return (struct wmKeyMap *) NULL;}

/* intern/decimation */
int LOD_FreeDecimationData(struct LOD_Decimation_Info *info){return 0;}
int LOD_CollapseEdge(struct LOD_Decimation_Info *info){return 0;}
int LOD_PreprocessMesh(struct LOD_Decimation_Info *info){return 0;}
int LOD_LoadMesh(struct LOD_Decimation_Info *info){return 0;}

/* smoke */
void lzo1x_1_compress(void) {return;}
void LzmaCompress(void) { return; }
void lzo1x_decompress(void) {return;}
void LzmaUncompress(void) {return;}
/* smoke is included anyway
void smoke_export(void) {return;}
void smoke_init(void) {return;}
void smoke_turbulence_init(void) {return;}
void smoke_turbulence_initBlenderRNA(void) {return;}
void smoke_initBlenderRNA(void) {return;}
void smoke_free(void) {return;}
void smoke_turbulence_free(void) {return;}
void smoke_turbulence_step(void) {return;}
void smoke_dissolve(void) {return;}
void smoke_get_density(void) {return;}
void smoke_get_heat(void) {return;}
void smoke_get_velocity_x(void) {return;}
void smoke_get_velocity_y(void) {return;}
void smoke_get_velocity_z(void) {return;}
void smoke_get_obstacle(void) {return;}
void smoke_get_index(void) {return;}
void smoke_step(void) {return;}
*/

char blender_path[] = "";

/* CSG */
struct CSG_BooleanOperation * CSG_NewBooleanFunction( void ){return (struct CSG_BooleanOperation *) NULL;}
void CSG_FreeBooleanOperation(struct CSG_BooleanOperation *operation){return;}
void CSG_FreeFaceDescriptor(struct CSG_FaceIteratorDescriptor * f_descriptor){return;}
void CSG_FreeVertexDescriptor(struct CSG_VertexIteratorDescriptor * v_descriptor){return;}	
int CSG_OutputFaceDescriptor(struct CSG_BooleanOperation * operation, struct CSG_FaceIteratorDescriptor * output){return 0;}
int CSG_OutputVertexDescriptor(struct CSG_BooleanOperation * operation, struct CSG_VertexIteratorDescriptor *output){return 0;}

typedef struct CSG_VertexIteratorDescriptor {int a;} CSG_VertexIteratorDescriptor; //workaround to build CSG_PerformanceBoolean Operation
typedef struct CSG_FaceIteratorDescriptor {int a;} CSG_FaceIteratorDescriptor; //workaround to build CSG_PerformanceBoolean Operation
typedef struct CSG_OperationType {int a;} CSG_OperationType; //workaround to build CSG_PerformanceBoolean Operation

int CSG_PerformBooleanOperation(
	struct CSG_BooleanOperation			*operation,
	CSG_OperationType				op_type,
	CSG_FaceIteratorDescriptor		obAFaces,
	CSG_VertexIteratorDescriptor	obAVertices,
	CSG_FaceIteratorDescriptor		obBFaces,
	CSG_VertexIteratorDescriptor	obBVertices)
	{ return 0;}

#endif // GAMEBLENDER == 1
