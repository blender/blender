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

/** \file blenderplayer/bad_level_call_stubs/stubs.c
 *  \ingroup blc
 */


#ifdef WITH_GAMEENGINE
#include <stdlib.h>
#include <assert.h>
#include "DNA_listBase.h"
#include "BLI_utildefines.h"
#include "RNA_types.h"

struct ARegion;
struct ARegionType;
struct BMEditMesh;
struct Base;
struct Brush;
struct CSG_FaceIteratorDescriptor;
struct CSG_VertexIteratorDescriptor;
struct ChannelDriver;
struct ColorBand;
struct Context;
struct Curve;
struct CurveMapping;
struct DerivedMesh;
struct EditBone;
struct EnvMap;
struct FCurve;
struct Heap;
struct HeapNode;
struct ID;
#ifdef WITH_FREESTYLE
struct FreestyleConfig;
struct FreestyleLineSet;
#endif
struct ImBuf;
struct Image;
struct ImageUser;
struct KeyingSet;
struct KeyingSetInfo;
struct MCol;
struct MTex;
struct Main;
struct Mask;
struct Material;
struct MenuType;
struct Mesh;
struct MetaBall;
struct ModifierData;
struct MovieClip;
struct MultiresModifierData;
struct NodeBlurData;
struct Nurb;
struct Object;
struct PBVHNode;
struct PyObject;
struct Render;
struct RenderEngine;
struct RenderEngineType;
struct RenderLayer;
struct RenderResult;
struct Scene;
struct Scene;
#ifdef WITH_FREESTYLE
struct SceneRenderLayer;
#endif
struct ScrArea;
struct SculptSession;
struct ShadeInput;
struct ShadeResult;
struct SpaceClip;
struct SpaceImage;
struct SpaceNode;
struct Tex;
struct TexResult;
struct Text;
struct ToolSettings;
struct View2D;
struct View3D;
struct bAction;
struct bArmature;
struct bConstraint;
struct bConstraintOb;
struct bConstraintTarget;
struct bContextDataResult;
struct bNode;
struct bNodeType;
struct bNodeSocket;
struct bNodeSocketType;
struct bNodeTree;
struct bNodeTreeType;
struct bPoseChannel;
struct bPythonConstraint;
struct bTheme;
struct uiLayout;
struct wmEvent;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;
struct wmWindow;
struct wmWindowManager;

/*new render funcs */
void EDBM_selectmode_set(struct BMEditMesh *em) {assert(true);}
void EDBM_mesh_load(struct Object *ob) {assert(true);}
void EDBM_mesh_make(struct ToolSettings *ts, struct Scene *scene, struct Object *ob) {assert(true);}
void EDBM_mesh_normals_update(struct BMEditMesh *em) {assert(true);}
void *g_system;

float *RE_RenderLayerGetPass(struct RenderLayer *rl, int passtype) {assert(true); return (float *) NULL;}
float RE_filter_value(int type, float x) {assert(true); return 0.0f;}
struct RenderLayer *RE_GetRenderLayer(struct RenderResult *rr, const char *name) {assert(true); return (struct RenderLayer *)NULL;}

/* zbuf.c stub */
void antialias_tagbuf(int xsize, int ysize, char *rectmove) {assert(true);}
void RE_zbuf_accumulate_vecblur(struct NodeBlurData *nbd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect) {assert(true);}

/* imagetexture.c stub */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result) {assert(true);}

/* texture.c */
int multitex_thread(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres, short thread, short which_output) {assert(true); return 0;}
int multitex_ext(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres) {assert(true); return 0;}
int multitex_ext_safe(struct Tex *tex, float *texvec, struct TexResult *texres) {assert(true); return 0;}
int multitex_nodes(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres, short thread, short which_output, struct ShadeInput *shi, struct MTex *mtex) {assert(true); return 0;}

struct Material *RE_init_sample_material(struct Material *orig_mat, struct Scene *scene) {assert(true); return (struct Material *)NULL;}
void RE_free_sample_material(struct Material *mat) {assert(true);}
void RE_sample_material_color(struct Material *mat, float color[3], float *alpha, const float volume_co[3], const float surface_co[3],
                              int face_index, short hit_quad, struct DerivedMesh *orcoDm, struct Object *ob) {assert(true);}

/* nodes */
struct RenderResult *RE_GetResult(struct Render *re) {assert(true); return (struct RenderResult *) NULL;}
struct Render *RE_GetRender(const char *name) {assert(true); return (struct Render *) NULL;}

/* blenkernel */
void RE_FreeRenderResult(struct RenderResult *res) {assert(true);}
void RE_FreeAllRenderResults(void) {assert(true);}
struct RenderResult *RE_MultilayerConvert(void *exrhandle, int rectx, int recty) {assert(true); return (struct RenderResult *) NULL;}
void RE_GetResultImage(struct Render *re, struct RenderResult *rr) {assert(true);}
int RE_RenderInProgress(struct Render *re) {assert(true); return 0;}
struct Scene *RE_GetScene(struct Render *re) {assert(true); return (struct Scene *) NULL;}
void RE_Database_Free(struct Render *re) {assert(true);}
void RE_FreeRender(struct Render *re) {assert(true);}
void RE_DataBase_GetView(struct Render *re, float mat[4][4]) {assert(true);}
int externtex(struct MTex *mtex, float *vec, float *tin, float *tr, float *tg, float *tb, float *ta) {assert(true); return 0;}
float texture_value_blend(float tex, float out, float fact, float facg, int blendtype, int flip) {assert(true); return 0.0f;}
void texture_rgb_blend(float *in, float *tex, float *out, float fact, float facg, int blendtype) {assert(true);}
char stipple_quarttone[1]; //GLubyte stipple_quarttone[128]
double elbeemEstimateMemreq(int res, float sx, float sy, float sz, int refine, char *retstr) {assert(true); return 0.0f;}
struct Render *RE_NewRender(const char *name) {assert(true); return (struct Render *) NULL;}
void RE_SwapResult(struct Render *re, struct RenderResult **rr) {assert(true);}
void RE_BlenderFrame(struct Render *re, struct Scene *scene, int frame) {assert(true);}
int RE_WriteEnvmapResult(struct ReportList *reports, struct Scene *scene, struct EnvMap *env, const char *relpath, const char imtype, float layout[12]) {assert(true); return 0; }

/* rna */
float *give_cursor(struct Scene *scene, struct View3D *v3d) {assert(true); return (float *) NULL;}
void WM_menutype_free(void) {assert(true);}
void WM_menutype_freelink(struct MenuType *mt) {assert(true);}
int WM_menutype_add(struct MenuType *mt) {assert(true); return 0;}
int WM_operator_props_dialog_popup(struct bContext *C, struct wmOperator *op, int width, int height) {assert(true); return 0;}
int WM_operator_confirm(struct bContext *C, struct wmOperator *op, const struct wmEvent *event) {assert(true); return 0;}
struct MenuType *WM_menutype_find(const char *idname, int quiet) {assert(true); return (struct MenuType *) NULL;}
void WM_operator_stack_clear(struct bContext *C) {assert(true);}
void WM_operator_handlers_clear(struct bContext *C, struct wmOperatorType *ot) {assert(true);}

void WM_autosave_init(struct bContext *C) {assert(true);}
void WM_jobs_kill_all_except(struct wmWindowManager *wm) {assert(true);}

char *WM_clipboard_text_get(int selection) {assert(true); return (char *)0;}
void WM_clipboard_text_set(char *buf, int selection) {assert(true);}

void	WM_cursor_restore(struct wmWindow *win) {assert(true);}
void	WM_cursor_time(struct wmWindow *win, int nr) {assert(true);}

void                WM_uilisttype_init(void) {assert(true);}
struct uiListType  *WM_uilisttype_find(const char *idname, int quiet) {assert(true); return (struct uiListType *)NULL;}
int                 WM_uilisttype_add(struct uiListType *ult) {assert(true); return 0;}
void                WM_uilisttype_freelink(struct uiListType *ult) {assert(true);}
void                WM_uilisttype_free(void) {assert(true);}

struct wmKeyMapItem *WM_keymap_item_find_id(struct wmKeyMap *keymap, int id) {assert(true); return (struct wmKeyMapItem *) NULL;}
int WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event) {assert(true); return 0;}
void WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference) {assert(true);}
void WM_main_add_notifier(unsigned int type, void *reference) {assert(true);}
void ED_armature_bone_rename(struct bArmature *arm, char *oldnamep, char *newnamep) {assert(true);}
struct wmEventHandler *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op) {assert(true); return (struct wmEventHandler *)NULL;}
struct wmTimer *WM_event_add_timer(struct wmWindowManager *wm, struct wmWindow *win, int event_type, double timestep) {assert(true); return (struct wmTimer *)NULL;}
void WM_event_remove_timer(struct wmWindowManager *wm, struct wmWindow *win, struct wmTimer *timer) {assert(true);}
void ED_armature_edit_bone_remove(struct bArmature *arm, struct EditBone *exBone) {assert(true);}
void object_test_constraints(struct Object *owner) {assert(true);}
void ED_object_parent(struct Object *ob, struct Object *par, int type, const char *substr) {assert(true);}
void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con) {assert(true);}
void ED_node_composit_default(struct bContext *C, struct Scene *scene) {assert(true);}
void *ED_region_draw_cb_activate(struct ARegionType *art, void(*draw)(const struct bContext *, struct ARegion *, void *), void *custumdata, int type) {assert(true); return 0;} /* XXX this one looks weird */
void *ED_region_draw_cb_customdata(void *handle) {assert(true); return 0;} /* XXX This one looks wrong also */
void ED_region_draw_cb_exit(struct ARegionType *art, void *handle) {assert(true);}
void	ED_area_headerprint(struct ScrArea *sa, char *str) {assert(true);}
void UI_view2d_region_to_view(struct View2D *v2d, int x, int y, float *viewx, float *viewy) {assert(true);}
void UI_view2d_view_to_region(struct View2D *v2d, float x, float y, int *regionx, int *regiony) {assert(true);}
void UI_view2d_to_region_no_clip(struct View2D *v2d, float x, float y, int *regionx, int *region_y) {assert(true);}

struct EditBone *ED_armature_bone_get_mirrored(struct ListBase *edbo, struct EditBone *ebo) {assert(true); return (struct EditBone *) NULL;}
struct EditBone *ED_armature_edit_bone_add(struct bArmature *arm, char *name) {assert(true); return (struct EditBone *) NULL;}
struct ListBase *get_active_constraints (struct Object *ob) {assert(true); return (struct ListBase *) NULL;}
struct ListBase *get_constraint_lb(struct Object *ob, struct bConstraint *con, struct bPoseChannel **pchan_r) {assert(true); return (struct ListBase *) NULL;}
int ED_pose_channel_in_IK_chain(struct Object *ob, struct bPoseChannel *pchan) {assert(true); return 0;}

int ED_space_image_show_uvedit(struct SpaceImage *sima, struct Object *obedit) {assert(true); return 0;}
int ED_space_image_show_render(struct SpaceImage *sima) {assert(true); return 0;}
int ED_space_image_show_paint(struct SpaceImage *sima) {assert(true); return 0;}
void ED_space_image_paint_update(struct wmWindowManager *wm, struct ToolSettings *settings) {assert(true);}
void ED_space_image_set(struct SpaceImage *sima, struct Scene *scene, struct Object *obedit, struct Image *ima) {assert(true);}
struct ImBuf *ED_space_image_buffer(struct SpaceImage *sima) {assert(true); return (struct ImBuf *) NULL;}
void ED_space_image_uv_sculpt_update(struct wmWindowManager *wm, struct ToolSettings *settings) {assert(true);}

void ED_screen_set_scene(struct bContext *C, struct Scene *scene) {assert(true);}
struct MovieClip *ED_space_clip_get_clip(struct SpaceClip *sc) {assert(true); return (struct MovieClip *) NULL;}
void ED_space_clip_set_clip(struct bContext *C, struct SpaceClip *sc, struct MovieClip *clip) {assert(true);}
void ED_space_clip_set_mask(struct bContext *C, struct SpaceClip *sc, struct Mask *mask) {assert(true);}
void ED_space_image_set_mask(struct bContext *C, struct SpaceImage *sima, struct Mask *mask) {assert(true);}

void ED_area_tag_redraw_regiontype(struct ScrArea *sa, int regiontype) {assert(true);}
void ED_render_engine_changed(struct Main *bmain) {assert(true);}

struct PTCacheEdit *PE_get_current(struct Scene *scene, struct Object *ob) {assert(true); return (struct PTCacheEdit *) NULL;}
void PE_current_changed(struct Scene *scene, struct Object *ob) {assert(true);}

/* rna keymap */
struct wmKeyMap *WM_keymap_active(struct wmWindowManager *wm, struct wmKeyMap *keymap) {assert(true); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_find(struct wmKeyConfig *keyconf, char *idname, int spaceid, int regionid) {assert(true); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_add_item(struct wmKeyMap *keymap, char *idname, int type, int val, int modifier, int keymodifier) {assert(true); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_copy_to_user(struct wmKeyMap *kemap) {assert(true); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_list_find(struct ListBase *lb, char *idname, int spaceid, int regionid) {assert(true); return (struct wmKeyMap *) NULL;}
struct wmKeyConfig *WM_keyconfig_new(struct wmWindowManager *wm, char *idname) {assert(true); return (struct wmKeyConfig *) NULL;}
struct wmKeyConfig *WM_keyconfig_new_user(struct wmWindowManager *wm, char *idname) {assert(true); return (struct wmKeyConfig *) NULL;}
void WM_keyconfig_remove(struct wmWindowManager *wm, char *idname) {assert(true);}
void WM_keyconfig_set_active(struct wmWindowManager *wm, const char *idname) {assert(true);}
void WM_keymap_remove_item(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) {assert(true);}
void WM_keymap_restore_to_default(struct wmKeyMap *keymap) {assert(true);}
void WM_keymap_restore_item_to_default(struct bContext *C, struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) {assert(true);}
void WM_keymap_properties_reset(struct wmKeyMapItem *kmi) {assert(true);}
void WM_keyconfig_update_tag(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) {assert(true);}
int WM_keymap_item_compare(struct wmKeyMapItem *k1, struct wmKeyMapItem *k2) {assert(true); return 0;}


/* rna editors */
struct EditMesh;

struct FCurve *verify_fcurve (struct bAction *act, const char group[], const char rna_path[], const int array_index, short add) {assert(true); return (struct FCurve *) NULL;}
int insert_vert_fcurve(struct FCurve *fcu, float x, float y, short flag) {assert(true); return 0;}
void delete_fcurve_key(struct FCurve *fcu, int index, short do_recalc) {assert(true);}
struct KeyingSetInfo *ANIM_keyingset_info_find_name (const char name[]) {assert(true); return (struct KeyingSetInfo *) NULL;}
struct KeyingSet *ANIM_scene_get_active_keyingset (struct Scene *scene) {assert(true); return (struct KeyingSet *) NULL;}
int ANIM_scene_get_keyingset_index(struct Scene *scene, struct KeyingSet *ks) {assert(true); return 0;}
struct ListBase builtin_keyingsets;
void ANIM_keyingset_info_register(struct KeyingSetInfo *ksi) {assert(true);}
void ANIM_keyingset_info_unregister(const struct bContext *C, struct KeyingSetInfo *ksi) {assert(true);}
short ANIM_validate_keyingset(struct bContext *C, struct ListBase *dsources, struct KeyingSet *ks) {assert(true); return 0;}
short ANIM_add_driver(struct ID *id, const char rna_path[], int array_index, short flag, int type) {assert(true); return 0;}
short ANIM_remove_driver(struct ID *id, const char rna_path[], int array_index, short flag) {assert(true); return 0;}
void ED_space_image_release_buffer(struct SpaceImage *sima, void *lock) {assert(true);}
struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **lock_r) {assert(true); return (struct ImBuf *) NULL;}
void ED_space_image_get_zoom(struct SpaceImage *sima, struct ARegion *ar, float *zoomx, float *zoomy) {assert(true);}
char *ED_info_stats_string(struct Scene *scene) {assert(true); return (char *) NULL;}
void ED_area_tag_redraw(struct ScrArea *sa) {assert(true);}
void ED_area_tag_refresh(struct ScrArea *sa) {assert(true);}
void ED_area_newspace(struct bContext *C, struct ScrArea *sa, int type) {assert(true);}
void ED_region_tag_redraw(struct ARegion *ar) {assert(true);}
void WM_event_add_fileselect(struct bContext *C, struct wmOperator *op) {assert(true);}
void WM_cursor_wait(int val) {assert(true);}
void ED_node_texture_default(struct bContext *C, struct Tex *tx) {assert(true);}
void ED_node_tag_update_id(struct ID *id) {assert(true);}
void ED_node_tag_update_nodetree(struct Main *bmain, struct bNodeTree *ntree) {assert(true);}
void ED_node_tree_update(const struct bContext *C) {assert(true);}
void ED_node_set_tree_type(struct SpaceNode *snode, struct bNodeTreeType *typeinfo){}
void ED_init_custom_node_type(struct bNodeType *ntype){}
void ED_init_custom_node_socket_type(struct bNodeSocketType *stype){}
void ED_init_standard_node_socket_type(struct bNodeSocketType *stype) {assert(true);}
void ED_init_node_socket_type_virtual(struct bNodeSocketType *stype) {assert(true);}
int ED_node_tree_path_length(struct SpaceNode *snode){return 0;}
void ED_node_tree_path_get(struct SpaceNode *snode, char *value){}
void ED_node_tree_path_get_fixedbuf(struct SpaceNode *snode, char *value, int max_length){}
void ED_node_tree_start(struct SpaceNode *snode, struct bNodeTree *ntree, struct ID *id, struct ID *from){}
void ED_node_tree_push(struct SpaceNode *snode, struct bNodeTree *ntree, struct bNode *gnode){}
void ED_node_tree_pop(struct SpaceNode *snode){}
void ED_view3d_scene_layers_update(struct Main *bmain, struct Scene *scene) {assert(true);}
int ED_view3d_scene_layer_set(int lay, const int *values) {assert(true); return 0;}
void ED_view3d_quadview_update(struct ScrArea *sa, struct ARegion *ar) {assert(true);}
void ED_view3d_from_m4(float mat[4][4], float ofs[3], float quat[4], float *dist) {assert(true);}
struct BGpic *ED_view3D_background_image_new(struct View3D *v3d) {assert(true); return (struct BGpic *) NULL;}
void ED_view3D_background_image_remove(struct View3D *v3d, struct BGpic *bgpic) {assert(true);}
void ED_view3D_background_image_clear(struct View3D *v3d) {assert(true);}
void ED_view3d_update_viewmat(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, float viewmat[4][4], float winmat[4][4]) {assert(true);}
float ED_view3d_grid_scale(struct Scene *scene, struct View3D *v3d, const char **grid_unit) {assert(true); return 0.0f;}
void view3d_apply_mat4(float mat[4][4], float *ofs, float *quat, float *dist) {assert(true);}
int text_file_modified(struct Text *text) {assert(true); return 0;}
void ED_node_shader_default(struct bContext *C, struct ID *id) {assert(true);}
void ED_screen_animation_timer_update(struct bContext *C, int redraws) {assert(true);}
void ED_screen_animation_playing(struct wmWindowManager *wm) {assert(true);}
void ED_base_object_select(struct Base *base, short mode) {assert(true);}
int ED_object_modifier_remove(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md) {assert(true); return 0;}
int ED_object_modifier_add(struct ReportList *reports, struct Scene *scene, struct Object *ob, char *name, int type) {assert(true); return 0;}
void ED_object_modifier_clear(struct Main *bmain, struct Object *ob) {assert(true);}
void ED_object_editmode_enter(struct bContext *C, int flag) {assert(true);}
void ED_object_editmode_exit(struct bContext *C, int flag) {assert(true);}
bool ED_object_editmode_load(struct Object *obedit) {assert(true); return false; }
int uiLayoutGetActive(struct uiLayout *layout) {assert(true); return 0;}
int uiLayoutGetOperatorContext(struct uiLayout *layout) {assert(true); return 0;}
int uiLayoutGetAlignment(struct uiLayout *layout) {assert(true); return 0;}
int uiLayoutGetEnabled(struct uiLayout *layout) {assert(true); return 0;}
float uiLayoutGetScaleX(struct uiLayout *layout) {assert(true); return 0.0f;}
float uiLayoutGetScaleY(struct uiLayout *layout) {assert(true); return 0.0f;}
void uiLayoutSetActive(struct uiLayout *layout, int active) {assert(true);}
void uiLayoutSetOperatorContext(struct uiLayout *layout, int opcontext) {assert(true);}
void uiLayoutSetEnabled(struct uiLayout *layout, int enabled) {assert(true);}
void uiLayoutSetAlignment(struct uiLayout *layout, int alignment) {assert(true);}
void uiLayoutSetScaleX(struct uiLayout *layout, float scale) {assert(true);}
void uiLayoutSetScaleY(struct uiLayout *layout, float scale) {assert(true);}
void uiTemplateIconView(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) {assert(true);}
void ED_base_object_free_and_unlink(struct Scene *scene, struct Base *base) {assert(true);}
void ED_mesh_calc_normals(struct Mesh *me) {assert(true);}
void ED_mesh_geometry_add(struct Mesh *mesh, struct ReportList *reports, int verts, int edges, int faces) {assert(true);}
void ED_mesh_material_add(struct Mesh *me, struct Material *ma) {assert(true);}
void ED_mesh_transform(struct Mesh *me, float *mat) {assert(true);}
void ED_mesh_update(struct Mesh *mesh, struct bContext *C) {assert(true);}
void ED_mesh_vertices_add(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_edges_add(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_tessfaces_add(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_loops_add(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_polys_add(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_vertices_remove(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_edges_remove(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_faces_remove(struct Mesh *mesh, struct ReportList *reports, int count) {assert(true);}
void ED_mesh_material_link(struct Mesh *mesh, struct Material *ma) {assert(true);}
int ED_mesh_color_add(struct Mesh *me, const char *name, const bool active_set) {assert(true); return -1; }
int ED_mesh_uv_texture_add(struct Mesh *me, const char *name, const bool active_set) {assert(true); return -1; }
bool ED_mesh_color_remove_named(struct Mesh *me, const char *name) {assert(true); return false; }
bool ED_mesh_uv_texture_remove_named(struct Mesh *me, const char *name) {assert(true); return false; }
void ED_object_constraint_dependency_update(struct Scene *scene, struct Object *ob) {assert(true);}
void ED_object_constraint_update(struct Object *ob) {assert(true);}
struct bDeformGroup *ED_vgroup_add_name(struct Object *ob, char *name) {assert(true); return (struct bDeformGroup *) NULL;}
void ED_vgroup_vert_add(struct Object *ob, struct bDeformGroup *dg, int vertnum, float weight, int assignmode) {assert(true);}
void ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum) {assert(true);}
void ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum) {assert(true);}
void ED_vgroup_delete(struct Object *ob, struct bDeformGroup *defgroup) {assert(true);}
void ED_vgroup_clear(struct Object *ob) {assert(true);}
void ED_vgroup_object_is_edit_mode(struct Object *ob) {assert(true);}
long mesh_mirrtopo_table(struct Object *ob, char mode) {assert(true); return 0; }
intptr_t mesh_octree_table(struct Object *ob, struct BMEditMesh *em, float *co, char mode) {assert(true); return 0; }

void ED_sequencer_update_view(struct bContext *C, int view) {assert(true);}
float ED_rollBoneToVector(struct EditBone *bone, float new_up_axis[3]) {assert(true); return 0.0f;}
void ED_space_image_get_size(struct SpaceImage *sima, int *width, int *height) {assert(true);}
int ED_space_image_check_show_maskedit(struct Scene *scene, struct SpaceImage *sima) {assert(true); return 0;}

void ED_nurb_set_spline_type(struct Nurb *nu, int type) {assert(true);}

void ED_mball_transform(struct MetaBall *mb, float *mat) {assert(true);}

bool snapObjectsRayEx(struct Scene *scene, struct Base *base_act) {assert(true); return 0;}

void make_editLatt(struct Object *obedit) {assert(true);}
void load_editLatt(struct Object *obedit) {assert(true);}

void load_editNurb(struct Object *obedit) {assert(true);}
void make_editNurb(struct Object *obedit) {assert(true);}


void uiItemR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int flag, char *name, int icon) {assert(true);}

struct PointerRNA uiItemFullO(struct uiLayout *layout, char *idname, char *name, int icon, struct IDProperty *properties, int context, int flag) {assert(true); struct PointerRNA a = {{0}}; return a;}
PointerRNA uiItemFullO_ptr(struct uiLayout *layout, struct wmOperatorType *ot, const char *name, int icon, struct IDProperty *properties, int context, int flag) {assert(true); struct PointerRNA a = {{0}}; return a;}
struct uiLayout *uiLayoutRow(struct uiLayout *layout, bool align) {assert(true); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutColumn(struct uiLayout *layout, bool align) {assert(true); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutColumnFlow(struct uiLayout *layout, int number, bool align) {assert(true); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutBox(struct uiLayout *layout) {assert(true); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutSplit(struct uiLayout *layout, float percentage, bool align) {assert(true); return (struct uiLayout *) NULL;}
int uiLayoutGetRedAlert(struct uiLayout *layout) {assert(true); return 0;}
void uiLayoutSetRedAlert(struct uiLayout *layout, int redalert) {assert(true);}
void uiItemsEnumR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname) {assert(true);}
void uiItemMenuEnumR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *name, int icon) {assert(true);}
void uiItemEnumR_string(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *value, char *name, int icon) {assert(true);}
void uiItemPointerR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, struct PointerRNA *searchptr, char *searchpropname, char *name, int icon) {assert(true);}
void uiItemPointerSubR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *searchpropname, char *name, int icon) {assert(true);}
void uiItemsEnumO(struct uiLayout *layout, char *opname, char *propname) {assert(true);}
void uiItemEnumO_string(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value_str) {assert(true);}
void uiItemMenuEnumO(struct uiLayout *layout, char *opname, char *propname, char *name, int icon) {assert(true);}
void uiItemBooleanO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, int value) {assert(true);}
void uiItemIntO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, int value) {assert(true);}
void uiItemFloatO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, float value) {assert(true);}
void uiItemStringO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value) {assert(true);}
void uiItemL(struct uiLayout *layout, const char *name, int icon) {assert(true);}
void uiItemM(struct uiLayout *layout, struct bContext *C, char *menuname, char *name, int icon) {assert(true);}
void uiItemS(struct uiLayout *layout) {assert(true);}
void uiItemFullR(struct uiLayout *layout, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, int value, int flag, char *name, int icon) {assert(true);}
void uiLayoutSetContextPointer(struct uiLayout *layout, char *name, struct PointerRNA *ptr) {assert(true);}
char *uiLayoutIntrospect(struct uiLayout *layout) {assert(true); return (char *)NULL;}
void UI_reinit_font(void) {assert(true);}
int UI_rnaptr_icon_get(struct bContext *C, struct PointerRNA *ptr, int rnaicon, const bool big) {assert(true); return 0;}
struct bTheme *UI_GetTheme(void) {assert(true); return (struct bTheme *) NULL;}

/* rna template */
void uiTemplateAnyID(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *text) {assert(true);}
void uiTemplatePathBuilder(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *root_ptr, char *text) {assert(true);}
void uiTemplateHeader(struct uiLayout *layout, struct bContext *C, int menus) {assert(true);}
void uiTemplateID(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *newop, char *unlinkop) {assert(true);}
struct uiLayout *uiTemplateModifier(struct uiLayout *layout, struct PointerRNA *ptr) {assert(true); return (struct uiLayout *) NULL;}
struct uiLayout *uiTemplateConstraint(struct uiLayout *layout, struct PointerRNA *ptr) {assert(true); return (struct uiLayout *) NULL;}
void uiTemplatePreview(struct uiLayout *layout, struct ID *id, int show_buttons, struct ID *parent, struct MTex *slot) {assert(true);}
void uiTemplateIDPreview(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop, int rows, int cols) {assert(true);}
void uiTemplateCurveMapping(struct uiLayout *layout, struct CurveMapping *cumap, int type, int compact) {assert(true);}
void uiTemplateColorRamp(struct uiLayout *layout, struct ColorBand *coba, int expand) {assert(true);}
void uiTemplateLayers(struct uiLayout *layout, struct PointerRNA *ptr, char *propname) {assert(true);}
void uiTemplateImageLayers(struct uiLayout *layout, struct bContext *C, struct Image *ima, struct ImageUser *iuser) {assert(true);}
void uiTemplateList(struct uiLayout *layout, struct bContext *C, const char *listtype_name, const char *list_id,
                    PointerRNA *dataptr, const char *propname, PointerRNA *active_dataptr,
                    const char *active_propname, int rows, int maxrows, int layout_type) {assert(true);}
void uiTemplateRunningJobs(struct uiLayout *layout, struct bContext *C) {assert(true);}
void uiTemplateOperatorSearch(struct uiLayout *layout) {assert(true);}
void uiTemplateHeader3D(struct uiLayout *layout, struct bContext *C) {assert(true);}
void uiTemplateEditModeSelection(struct uiLayout *layout, struct bContext *C) {assert(true);}
void uiTemplateImage(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *userptr, int compact) {assert(true);}
void uiTemplateColorPicker(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int value_slider) {assert(true);}
void uiTemplateHistogram(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int expand) {assert(true);}
void uiTemplateReportsBanner(struct uiLayout *layout, struct bContext *C, struct wmOperator *op) {assert(true);}
void uiTemplateWaveform(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int expand) {assert(true);}
void uiTemplateVectorscope(struct uiLayout *_self, struct PointerRNA *data, char *property, int expand) {assert(true);}
void uiTemplateNodeLink(struct uiLayout *layout, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input) {assert(true);}
void uiTemplateNodeView(struct uiLayout *layout, struct bContext *C, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input) {assert(true);}
void uiTemplateTextureUser(struct uiLayout *layout, struct bContext *C) {assert(true);}
void uiTemplateTextureShow(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop) {assert(true);}
void uiTemplateKeymapItemProperties(struct uiLayout *layout, struct PointerRNA *ptr) {assert(true);}
void uiTemplateMovieClip(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, int compact) {assert(true);}
void uiTemplateMovieclipInformation(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *userptr) {assert(true);}
void uiTemplateTrack(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) {assert(true);}
void uiTemplateMarker(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, PointerRNA *userptr, PointerRNA *trackptr, int compact) {assert(true);}
void uiTemplateImageSettings(struct uiLayout *layout, struct PointerRNA *imfptr) {assert(true);}
void uiTemplateColorspaceSettings(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) {assert(true);}
void uiTemplateColormanagedViewSettings(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, int show_global_settings) {assert(true);}
void uiTemplateComponentMenu(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name){}
void uiTemplateNodeSocket(struct uiLayout *layout, struct bContext *C, float *color) {assert(true);}

/* rna render */
struct RenderResult *RE_engine_begin_result(struct RenderEngine *engine, int x, int y, int w, int h) {assert(true); return (struct RenderResult *) NULL;}
struct RenderResult *RE_AcquireResultRead(struct Render *re) {assert(true); return (struct RenderResult *) NULL;}
struct RenderResult *RE_AcquireResultWrite(struct Render *re) {assert(true); return (struct RenderResult *) NULL;}
struct RenderStats *RE_GetStats(struct Render *re) {assert(true); return (struct RenderStats *) NULL;}
struct RenderData *RE_engine_get_render_data(struct Render *re) {assert(true); return (struct RenderData *) NULL;}
void RE_engine_update_result(struct RenderEngine *engine, struct RenderResult *result) {assert(true);}
void RE_engine_update_progress(struct RenderEngine *engine, float progress) {assert(true);}
void RE_engine_end_result(struct RenderEngine *engine, struct RenderResult *result) {assert(true);}
void RE_engine_update_stats(struct RenderEngine *engine, char *stats, char *info) {assert(true);}
void RE_layer_load_from_file(struct RenderLayer *layer, struct ReportList *reports, char *filename) {assert(true);}
void RE_result_load_from_file(struct RenderResult *result, struct ReportList *reports, char *filename) {assert(true);}
void RE_AcquireResultImage(struct Render *re, struct RenderResult *rr) {assert(true);}
void RE_ReleaseResult(struct Render *re) {assert(true);}
void RE_ReleaseResultImage(struct Render *re) {assert(true);}
int RE_engine_test_break(struct RenderEngine *engine) {assert(true); return 0;}
void RE_engines_init() {assert(true);}
void RE_engines_exit() {assert(true);}
void RE_engine_report(struct RenderEngine *engine, int type, const char *msg) {assert(true);}
ListBase R_engines = {NULL, NULL};
void RE_engine_free(struct RenderEngine *engine) {assert(true);}
struct RenderEngineType *RE_engines_find(const char *idname) {assert(true); return NULL; }
void RE_engine_update_memory_stats(struct RenderEngine *engine, float mem_used, float mem_peak) {assert(true);}
struct RenderEngine *RE_engine_create(struct RenderEngineType *type) {assert(true); return NULL; }
void RE_FreePersistentData(void) {assert(true);}

/* python */
struct wmOperatorType *WM_operatortype_find(const char *idname, int quiet) {assert(true); return (struct wmOperatorType *) NULL;}
struct GHashIterator *WM_operatortype_iter() {assert(true); return (struct GHashIterator *) NULL;}
struct wmOperatorType *WM_operatortype_exists(const char *idname) {assert(true); return (struct wmOperatorType *) NULL;}
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot, const char *idname) {assert(true); return (struct wmOperatorTypeMacro *) NULL;}
int WM_operator_call_py(struct bContext *C, struct wmOperatorType *ot, short context, short is_undo, struct PointerRNA *properties, struct ReportList *reports) {assert(true); return 0;}
int WM_operatortype_remove(const char *idname) {assert(true); return 0;}
int WM_operator_poll(struct bContext *C, struct wmOperatorType *ot) {assert(true); return 0;}
int WM_operator_poll_context(struct bContext *C, struct wmOperatorType *ot, int context) {assert(true); return 0;}
int WM_operator_props_popup(struct bContext *C, struct wmOperator *op, struct wmEvent *event) {assert(true); return 0;}
void WM_operator_properties_free(struct PointerRNA *ptr) {assert(true);}
void WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring) {assert(true);}
void WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot) {assert(true);}
void WM_operator_properties_sanitize(struct PointerRNA *ptr, const short no_context) {assert(true);}
void WM_operatortype_append_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata) {assert(true);}
void WM_operatortype_append_macro_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata) {assert(true);}
void WM_operator_bl_idname(char *to, const char *from) {assert(true);}
void WM_operator_py_idname(char *to, const char *from) {assert(true);}
void WM_operator_ui_popup(struct bContext *C, struct wmOperator *op, int width, int height) {assert(true);}
short insert_keyframe(struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag) {assert(true); return 0;}
short delete_keyframe(struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag) {assert(true); return 0;}
char *WM_operator_pystring(struct bContext *C, struct wmOperatorType *ot, struct PointerRNA *opptr, int all_args) {assert(true); return (char *)NULL;}
struct wmKeyMapItem *WM_modalkeymap_add_item(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value) {assert(true); return (struct wmKeyMapItem *)NULL;}
struct wmKeyMapItem *WM_modalkeymap_add_item_str(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, const char *value) {assert(true); return (struct wmKeyMapItem *)NULL;}
struct wmKeyMap *WM_modalkeymap_add(struct wmKeyConfig *keyconf, char *idname, EnumPropertyItem *items) {assert(true); return (struct wmKeyMap *) NULL;}

/* RNA COLLADA dependency */
int collada_export(struct Scene *sce, const char *filepath) {assert(true); return 0; }

int sculpt_get_brush_size(struct Brush *brush) {assert(true); return 0;}
void sculpt_set_brush_size(struct Brush *brush, int size) {assert(true);}
int sculpt_get_lock_brush_size(struct Brush *brush) {assert(true); return 0;}
float sculpt_get_brush_unprojected_radius(struct Brush *brush) {assert(true); return 0.0f;}
void sculpt_set_brush_unprojected_radius(struct Brush *brush, float unprojected_radius) {assert(true);}
float sculpt_get_brush_alpha(struct Brush *brush) {assert(true); return 0.0f;}
void sculpt_set_brush_alpha(struct Brush *brush, float alpha) {assert(true);}
void ED_sculpt_modifiers_changed(struct Object *ob) {assert(true);}
void ED_mesh_calc_tessface(struct Mesh *mesh) {assert(true);}

/* bpy/python internal api */
void operator_wrapper(struct wmOperatorType *ot, void *userdata) {assert(true);}
void BPY_text_free_code(struct Text *text) {assert(true);}
void BPY_id_release(struct Text *text) {assert(true);}
int BPY_context_member_get(struct Context *C, const char *member, struct bContextDataResult *result) {assert(true); return 0; }
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct) {assert(true);}
float BPY_driver_exec(struct ChannelDriver *driver, const float evaltime) {assert(true); return 0.0f;} /* might need this one! */
void BPY_DECREF(void *pyob_ptr) {assert(true);}
void BPY_pyconstraint_exec(struct bPythonConstraint *con, struct bConstraintOb *cob, struct ListBase *targets) {assert(true);}
void macro_wrapper(struct wmOperatorType *ot, void *userdata) {assert(true);}
int pyrna_id_FromPyObject(struct PyObject *obj, struct ID **id) {assert(true); return 0; }
struct PyObject *pyrna_id_CreatePyObject(struct ID *id) {assert(true); return NULL; }
void BPY_context_update(struct bContext *C) {assert(true);};
const char *BPY_app_translations_py_pgettext(const char *msgctxt, const char *msgid) {assert(true); return msgid; }

#ifdef WITH_FREESTYLE
/* Freestyle */
void FRS_init_freestyle_config(struct FreestyleConfig *config) {assert(true);}
void FRS_free_freestyle_config(struct FreestyleConfig *config) {assert(true);}
void FRS_copy_freestyle_config(struct FreestyleConfig *new_config, struct FreestyleConfig *config) {assert(true);}
struct FreestyleLineSet *FRS_get_active_lineset(struct FreestyleConfig *config) {assert(true); return NULL; }
short FRS_get_active_lineset_index(struct FreestyleConfig *config) {assert(true); return 0; }
void FRS_set_active_lineset_index(struct FreestyleConfig *config, short index) {assert(true);}
void FRS_unlink_target_object(struct FreestyleConfig *config, struct Object *ob) {assert(true);}
#endif
/* intern/dualcon */
struct DualConMesh;
struct DualConMesh *dualcon(const struct DualConMesh *input_mesh,
                            void *create_mesh,
                            int flags,
                            int mode,
                            float threshold,
                            float hermite_num,
                            float scale,
                            int depth) {assert(true); return 0; }

/* intern/cycles */
struct CCLDeviceInfo;
struct CCLDeviceInfo *CCL_compute_device_list(int opencl) {assert(true); return NULL; }

/* compositor */
void COM_execute(struct bNodeTree *editingtree, int rendering) {assert(true);}

char blender_path[] = "";

#endif // WITH_GAMEENGINE
