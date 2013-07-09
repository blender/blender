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
#include "DNA_listBase.h"
#include "BLI_utildefines.h"
#include "RNA_types.h"

#define ASSERT_STUBS 0
#if ASSERT_STUBS
#include <assert.h>
#define STUB_ASSERT(x) (assert(x))
#else
#define STUB_ASSERT(x)
#endif


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
void EDBM_selectmode_set(struct BMEditMesh *em) {STUB_ASSERT(0);}
void EDBM_mesh_load(struct Object *ob) {STUB_ASSERT(0);}
void EDBM_mesh_make(struct ToolSettings *ts, struct Scene *scene, struct Object *ob) {STUB_ASSERT(0);}
void EDBM_mesh_normals_update(struct BMEditMesh *em) {STUB_ASSERT(0);}
void *g_system;

float *RE_RenderLayerGetPass(struct RenderLayer *rl, int passtype) {STUB_ASSERT(0); return (float *) NULL;}
float RE_filter_value(int type, float x) {STUB_ASSERT(0); return 0.0f;}
struct RenderLayer *RE_GetRenderLayer(struct RenderResult *rr, const char *name) {STUB_ASSERT(0); return (struct RenderLayer *)NULL;}

/* zbuf.c stub */
void antialias_tagbuf(int xsize, int ysize, char *rectmove) {STUB_ASSERT(0);}
void RE_zbuf_accumulate_vecblur(struct NodeBlurData *nbd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect) {STUB_ASSERT(0);}

/* imagetexture.c stub */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result) {STUB_ASSERT(0);}

/* texture.c */
int multitex_thread(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres, short thread, short which_output) {STUB_ASSERT(0); return 0;}
int multitex_ext(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres) {STUB_ASSERT(0); return 0;}
int multitex_ext_safe(struct Tex *tex, float *texvec, struct TexResult *texres) {STUB_ASSERT(0); return 0;}
int multitex_nodes(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres, short thread, short which_output, struct ShadeInput *shi, struct MTex *mtex) {STUB_ASSERT(0); return 0;}

struct Material *RE_init_sample_material(struct Material *orig_mat, struct Scene *scene) {STUB_ASSERT(0); return (struct Material *)NULL;}
void RE_free_sample_material(struct Material *mat) {STUB_ASSERT(0);}
void RE_sample_material_color(struct Material *mat, float color[3], float *alpha, const float volume_co[3], const float surface_co[3],
                              int face_index, short hit_quad, struct DerivedMesh *orcoDm, struct Object *ob) {STUB_ASSERT(0);}

/* nodes */
struct RenderResult *RE_GetResult(struct Render *re) {STUB_ASSERT(0); return (struct RenderResult *) NULL;}
struct Render *RE_GetRender(const char *name) {STUB_ASSERT(0); return (struct Render *) NULL;}

/* blenkernel */
void RE_FreeRenderResult(struct RenderResult *res) {STUB_ASSERT(0);}
void RE_FreeAllRenderResults(void) {STUB_ASSERT(0);}
struct RenderResult *RE_MultilayerConvert(void *exrhandle, int rectx, int recty) {STUB_ASSERT(0); return (struct RenderResult *) NULL;}
void RE_GetResultImage(struct Render *re, struct RenderResult *rr) {STUB_ASSERT(0);}
int RE_RenderInProgress(struct Render *re) {STUB_ASSERT(0); return 0;}
struct Scene *RE_GetScene(struct Render *re) {STUB_ASSERT(0); return (struct Scene *) NULL;}
void RE_Database_Free(struct Render *re) {STUB_ASSERT(0);}
void RE_FreeRender(struct Render *re) {STUB_ASSERT(0);}
void RE_DataBase_GetView(struct Render *re, float mat[4][4]) {STUB_ASSERT(0);}
int externtex(struct MTex *mtex, float *vec, float *tin, float *tr, float *tg, float *tb, float *ta) {STUB_ASSERT(0); return 0;}
float texture_value_blend(float tex, float out, float fact, float facg, int blendtype, int flip) {STUB_ASSERT(0); return 0.0f;}
void texture_rgb_blend(float *in, float *tex, float *out, float fact, float facg, int blendtype) {STUB_ASSERT(0);}
char stipple_quarttone[1]; //GLubyte stipple_quarttone[128]
double elbeemEstimateMemreq(int res, float sx, float sy, float sz, int refine, char *retstr) {STUB_ASSERT(0); return 0.0f;}
struct Render *RE_NewRender(const char *name) {STUB_ASSERT(0); return (struct Render *) NULL;}
void RE_SwapResult(struct Render *re, struct RenderResult **rr) {STUB_ASSERT(0);}
void RE_BlenderFrame(struct Render *re, struct Scene *scene, int frame) {STUB_ASSERT(0);}
int RE_WriteEnvmapResult(struct ReportList *reports, struct Scene *scene, struct EnvMap *env, const char *relpath, const char imtype, float layout[12]) {STUB_ASSERT(0); return 0; }

/* rna */
float *give_cursor(struct Scene *scene, struct View3D *v3d) {STUB_ASSERT(0); return (float *) NULL;}
void WM_menutype_free(void) {STUB_ASSERT(0);}
void WM_menutype_freelink(struct MenuType *mt) {STUB_ASSERT(0);}
int WM_menutype_add(struct MenuType *mt) {STUB_ASSERT(0); return 0;}
int WM_operator_props_dialog_popup(struct bContext *C, struct wmOperator *op, int width, int height) {STUB_ASSERT(0); return 0;}
int WM_operator_confirm(struct bContext *C, struct wmOperator *op, const struct wmEvent *event) {STUB_ASSERT(0); return 0;}
struct MenuType *WM_menutype_find(const char *idname, int quiet) {STUB_ASSERT(0); return (struct MenuType *) NULL;}
void WM_operator_stack_clear(struct bContext *C) {STUB_ASSERT(0);}
void WM_operator_handlers_clear(struct bContext *C, struct wmOperatorType *ot) {STUB_ASSERT(0);}

void WM_autosave_init(struct bContext *C) {STUB_ASSERT(0);}
void WM_jobs_kill_all_except(struct wmWindowManager *wm) {STUB_ASSERT(0);}

char *WM_clipboard_text_get(int selection) {STUB_ASSERT(0); return (char *)0;}
void WM_clipboard_text_set(char *buf, int selection) {STUB_ASSERT(0);}

void	WM_cursor_restore(struct wmWindow *win) {STUB_ASSERT(0);}
void	WM_cursor_time(struct wmWindow *win, int nr) {STUB_ASSERT(0);}

void                WM_uilisttype_init(void) {STUB_ASSERT(0);}
struct uiListType  *WM_uilisttype_find(const char *idname, int quiet) {STUB_ASSERT(0); return (struct uiListType *)NULL;}
int                 WM_uilisttype_add(struct uiListType *ult) {STUB_ASSERT(0); return 0;}
void                WM_uilisttype_freelink(struct uiListType *ult) {STUB_ASSERT(0);}
void                WM_uilisttype_free(void) {STUB_ASSERT(0);}

struct wmKeyMapItem *WM_keymap_item_find_id(struct wmKeyMap *keymap, int id) {STUB_ASSERT(0); return (struct wmKeyMapItem *) NULL;}
int WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, struct wmEvent *event) {STUB_ASSERT(0); return 0;}
void WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference) {STUB_ASSERT(0);}
void WM_main_add_notifier(unsigned int type, void *reference) {STUB_ASSERT(0);}
void ED_armature_bone_rename(struct bArmature *arm, char *oldnamep, char *newnamep) {STUB_ASSERT(0);}
struct wmEventHandler *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op) {STUB_ASSERT(0); return (struct wmEventHandler *)NULL;}
struct wmTimer *WM_event_add_timer(struct wmWindowManager *wm, struct wmWindow *win, int event_type, double timestep) {STUB_ASSERT(0); return (struct wmTimer *)NULL;}
void WM_event_remove_timer(struct wmWindowManager *wm, struct wmWindow *win, struct wmTimer *timer) {STUB_ASSERT(0);}
void ED_armature_edit_bone_remove(struct bArmature *arm, struct EditBone *exBone) {STUB_ASSERT(0);}
void object_test_constraints(struct Object *owner) {STUB_ASSERT(0);}
void ED_object_parent(struct Object *ob, struct Object *par, int type, const char *substr) {STUB_ASSERT(0);}
void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con) {STUB_ASSERT(0);}
void ED_node_composit_default(struct bContext *C, struct Scene *scene) {STUB_ASSERT(0);}
void *ED_region_draw_cb_activate(struct ARegionType *art, void(*draw)(const struct bContext *, struct ARegion *, void *), void *custumdata, int type) {STUB_ASSERT(0); return 0;} /* XXX this one looks weird */
void *ED_region_draw_cb_customdata(void *handle) {STUB_ASSERT(0); return 0;} /* XXX This one looks wrong also */
void ED_region_draw_cb_exit(struct ARegionType *art, void *handle) {STUB_ASSERT(0);}
void	ED_area_headerprint(struct ScrArea *sa, char *str) {STUB_ASSERT(0);}
void UI_view2d_region_to_view(struct View2D *v2d, int x, int y, float *viewx, float *viewy) {STUB_ASSERT(0);}
void UI_view2d_view_to_region(struct View2D *v2d, float x, float y, int *regionx, int *regiony) {STUB_ASSERT(0);}
void UI_view2d_to_region_no_clip(struct View2D *v2d, float x, float y, int *regionx, int *region_y) {STUB_ASSERT(0);}

struct EditBone *ED_armature_bone_get_mirrored(struct ListBase *edbo, struct EditBone *ebo) {STUB_ASSERT(0); return (struct EditBone *) NULL;}
struct EditBone *ED_armature_edit_bone_add(struct bArmature *arm, char *name) {STUB_ASSERT(0); return (struct EditBone *) NULL;}
struct ListBase *get_active_constraints (struct Object *ob) {STUB_ASSERT(0); return (struct ListBase *) NULL;}
struct ListBase *get_constraint_lb(struct Object *ob, struct bConstraint *con, struct bPoseChannel **pchan_r) {STUB_ASSERT(0); return (struct ListBase *) NULL;}
int ED_pose_channel_in_IK_chain(struct Object *ob, struct bPoseChannel *pchan) {STUB_ASSERT(0); return 0;}

int ED_space_image_show_uvedit(struct SpaceImage *sima, struct Object *obedit) {STUB_ASSERT(0); return 0;}
int ED_space_image_show_render(struct SpaceImage *sima) {STUB_ASSERT(0); return 0;}
int ED_space_image_show_paint(struct SpaceImage *sima) {STUB_ASSERT(0); return 0;}
void ED_space_image_paint_update(struct wmWindowManager *wm, struct ToolSettings *settings) {STUB_ASSERT(0);}
void ED_space_image_set(struct SpaceImage *sima, struct Scene *scene, struct Object *obedit, struct Image *ima) {STUB_ASSERT(0);}
struct ImBuf *ED_space_image_buffer(struct SpaceImage *sima) {STUB_ASSERT(0); return (struct ImBuf *) NULL;}
void ED_space_image_uv_sculpt_update(struct wmWindowManager *wm, struct ToolSettings *settings) {STUB_ASSERT(0);}

void ED_screen_set_scene(struct bContext *C, struct Scene *scene) {STUB_ASSERT(0);}
struct MovieClip *ED_space_clip_get_clip(struct SpaceClip *sc) {STUB_ASSERT(0); return (struct MovieClip *) NULL;}
void ED_space_clip_set_clip(struct bContext *C, struct SpaceClip *sc, struct MovieClip *clip) {STUB_ASSERT(0);}
void ED_space_clip_set_mask(struct bContext *C, struct SpaceClip *sc, struct Mask *mask) {STUB_ASSERT(0);}
void ED_space_image_set_mask(struct bContext *C, struct SpaceImage *sima, struct Mask *mask) {STUB_ASSERT(0);}

void ED_area_tag_redraw_regiontype(struct ScrArea *sa, int regiontype) {STUB_ASSERT(0);}
void ED_render_engine_changed(struct Main *bmain) {STUB_ASSERT(0);}

struct PTCacheEdit *PE_get_current(struct Scene *scene, struct Object *ob) {STUB_ASSERT(0); return (struct PTCacheEdit *) NULL;}
void PE_current_changed(struct Scene *scene, struct Object *ob) {STUB_ASSERT(0);}

/* rna keymap */
struct wmKeyMap *WM_keymap_active(struct wmWindowManager *wm, struct wmKeyMap *keymap) {STUB_ASSERT(0); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_find(struct wmKeyConfig *keyconf, char *idname, int spaceid, int regionid) {STUB_ASSERT(0); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_add_item(struct wmKeyMap *keymap, char *idname, int type, int val, int modifier, int keymodifier) {STUB_ASSERT(0); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_copy_to_user(struct wmKeyMap *kemap) {STUB_ASSERT(0); return (struct wmKeyMap *) NULL;}
struct wmKeyMap *WM_keymap_list_find(struct ListBase *lb, char *idname, int spaceid, int regionid) {STUB_ASSERT(0); return (struct wmKeyMap *) NULL;}
struct wmKeyConfig *WM_keyconfig_new(struct wmWindowManager *wm, char *idname) {STUB_ASSERT(0); return (struct wmKeyConfig *) NULL;}
struct wmKeyConfig *WM_keyconfig_new_user(struct wmWindowManager *wm, char *idname) {STUB_ASSERT(0); return (struct wmKeyConfig *) NULL;}
void WM_keyconfig_remove(struct wmWindowManager *wm, char *idname) {STUB_ASSERT(0);}
void WM_keyconfig_set_active(struct wmWindowManager *wm, const char *idname) {STUB_ASSERT(0);}
void WM_keymap_remove_item(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) {STUB_ASSERT(0);}
void WM_keymap_restore_to_default(struct wmKeyMap *keymap) {STUB_ASSERT(0);}
void WM_keymap_restore_item_to_default(struct bContext *C, struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) {STUB_ASSERT(0);}
void WM_keymap_properties_reset(struct wmKeyMapItem *kmi) {STUB_ASSERT(0);}
void WM_keyconfig_update_tag(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) {STUB_ASSERT(0);}
int WM_keymap_item_compare(struct wmKeyMapItem *k1, struct wmKeyMapItem *k2) {STUB_ASSERT(0); return 0;}


/* rna editors */
struct EditMesh;

struct FCurve *verify_fcurve (struct bAction *act, const char group[], const char rna_path[], const int array_index, short add) {STUB_ASSERT(0); return (struct FCurve *) NULL;}
int insert_vert_fcurve(struct FCurve *fcu, float x, float y, short flag) {STUB_ASSERT(0); return 0;}
void delete_fcurve_key(struct FCurve *fcu, int index, short do_recalc) {STUB_ASSERT(0);}
struct KeyingSetInfo *ANIM_keyingset_info_find_name (const char name[]) {STUB_ASSERT(0); return (struct KeyingSetInfo *) NULL;}
struct KeyingSet *ANIM_scene_get_active_keyingset (struct Scene *scene) {STUB_ASSERT(0); return (struct KeyingSet *) NULL;}
int ANIM_scene_get_keyingset_index(struct Scene *scene, struct KeyingSet *ks) {STUB_ASSERT(0); return 0;}
struct ListBase builtin_keyingsets;
void ANIM_keyingset_info_register(struct KeyingSetInfo *ksi) {STUB_ASSERT(0);}
void ANIM_keyingset_info_unregister(const struct bContext *C, struct KeyingSetInfo *ksi) {STUB_ASSERT(0);}
short ANIM_validate_keyingset(struct bContext *C, struct ListBase *dsources, struct KeyingSet *ks) {STUB_ASSERT(0); return 0;}
short ANIM_add_driver(struct ID *id, const char rna_path[], int array_index, short flag, int type) {STUB_ASSERT(0); return 0;}
short ANIM_remove_driver(struct ID *id, const char rna_path[], int array_index, short flag) {STUB_ASSERT(0); return 0;}
void ED_space_image_release_buffer(struct SpaceImage *sima, void *lock) {STUB_ASSERT(0);}
struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **lock_r) {STUB_ASSERT(0); return (struct ImBuf *) NULL;}
void ED_space_image_get_zoom(struct SpaceImage *sima, struct ARegion *ar, float *zoomx, float *zoomy) {STUB_ASSERT(0);}
char *ED_info_stats_string(struct Scene *scene) {STUB_ASSERT(0); return (char *) NULL;}
void ED_area_tag_redraw(struct ScrArea *sa) {STUB_ASSERT(0);}
void ED_area_tag_refresh(struct ScrArea *sa) {STUB_ASSERT(0);}
void ED_area_newspace(struct bContext *C, struct ScrArea *sa, int type) {STUB_ASSERT(0);}
void ED_region_tag_redraw(struct ARegion *ar) {STUB_ASSERT(0);}
void WM_event_add_fileselect(struct bContext *C, struct wmOperator *op) {STUB_ASSERT(0);}
void WM_cursor_wait(int val) {STUB_ASSERT(0);}
void ED_node_texture_default(struct bContext *C, struct Tex *tx) {STUB_ASSERT(0);}
void ED_node_tag_update_id(struct ID *id) {STUB_ASSERT(0);}
void ED_node_tag_update_nodetree(struct Main *bmain, struct bNodeTree *ntree) {STUB_ASSERT(0);}
void ED_node_tree_update(const struct bContext *C) {STUB_ASSERT(0);}
void ED_node_set_tree_type(struct SpaceNode *snode, struct bNodeTreeType *typeinfo){}
void ED_init_custom_node_type(struct bNodeType *ntype){}
void ED_init_custom_node_socket_type(struct bNodeSocketType *stype){}
void ED_init_standard_node_socket_type(struct bNodeSocketType *stype) {STUB_ASSERT(0);}
void ED_init_node_socket_type_virtual(struct bNodeSocketType *stype) {STUB_ASSERT(0);}
int ED_node_tree_path_length(struct SpaceNode *snode){return 0;}
void ED_node_tree_path_get(struct SpaceNode *snode, char *value){}
void ED_node_tree_path_get_fixedbuf(struct SpaceNode *snode, char *value, int max_length){}
void ED_node_tree_start(struct SpaceNode *snode, struct bNodeTree *ntree, struct ID *id, struct ID *from){}
void ED_node_tree_push(struct SpaceNode *snode, struct bNodeTree *ntree, struct bNode *gnode){}
void ED_node_tree_pop(struct SpaceNode *snode){}
void ED_view3d_scene_layers_update(struct Main *bmain, struct Scene *scene) {STUB_ASSERT(0);}
int ED_view3d_scene_layer_set(int lay, const int *values) {STUB_ASSERT(0); return 0;}
void ED_view3d_quadview_update(struct ScrArea *sa, struct ARegion *ar) {STUB_ASSERT(0);}
void ED_view3d_from_m4(float mat[4][4], float ofs[3], float quat[4], float *dist) {STUB_ASSERT(0);}
struct BGpic *ED_view3D_background_image_new(struct View3D *v3d) {STUB_ASSERT(0); return (struct BGpic *) NULL;}
void ED_view3D_background_image_remove(struct View3D *v3d, struct BGpic *bgpic) {STUB_ASSERT(0);}
void ED_view3D_background_image_clear(struct View3D *v3d) {STUB_ASSERT(0);}
void ED_view3d_update_viewmat(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, float viewmat[4][4], float winmat[4][4]) {STUB_ASSERT(0);}
float ED_view3d_grid_scale(struct Scene *scene, struct View3D *v3d, const char **grid_unit) {STUB_ASSERT(0); return 0.0f;}
void view3d_apply_mat4(float mat[4][4], float *ofs, float *quat, float *dist) {STUB_ASSERT(0);}
int text_file_modified(struct Text *text) {STUB_ASSERT(0); return 0;}
void ED_node_shader_default(struct bContext *C, struct ID *id) {STUB_ASSERT(0);}
void ED_screen_animation_timer_update(struct bContext *C, int redraws) {STUB_ASSERT(0);}
void ED_screen_animation_playing(struct wmWindowManager *wm) {STUB_ASSERT(0);}
void ED_base_object_select(struct Base *base, short mode) {STUB_ASSERT(0);}
int ED_object_modifier_remove(struct ReportList *reports, struct Scene *scene, struct Object *ob, struct ModifierData *md) {STUB_ASSERT(0); return 0;}
int ED_object_modifier_add(struct ReportList *reports, struct Scene *scene, struct Object *ob, char *name, int type) {STUB_ASSERT(0); return 0;}
void ED_object_modifier_clear(struct Main *bmain, struct Object *ob) {STUB_ASSERT(0);}
void ED_object_editmode_enter(struct bContext *C, int flag) {STUB_ASSERT(0);}
void ED_object_editmode_exit(struct bContext *C, int flag) {STUB_ASSERT(0);}
bool ED_object_editmode_load(struct Object *obedit) {STUB_ASSERT(0); return false; }
int uiLayoutGetActive(struct uiLayout *layout) {STUB_ASSERT(0); return 0;}
int uiLayoutGetOperatorContext(struct uiLayout *layout) {STUB_ASSERT(0); return 0;}
int uiLayoutGetAlignment(struct uiLayout *layout) {STUB_ASSERT(0); return 0;}
int uiLayoutGetEnabled(struct uiLayout *layout) {STUB_ASSERT(0); return 0;}
float uiLayoutGetScaleX(struct uiLayout *layout) {STUB_ASSERT(0); return 0.0f;}
float uiLayoutGetScaleY(struct uiLayout *layout) {STUB_ASSERT(0); return 0.0f;}
void uiLayoutSetActive(struct uiLayout *layout, int active) {STUB_ASSERT(0);}
void uiLayoutSetOperatorContext(struct uiLayout *layout, int opcontext) {STUB_ASSERT(0);}
void uiLayoutSetEnabled(struct uiLayout *layout, int enabled) {STUB_ASSERT(0);}
void uiLayoutSetAlignment(struct uiLayout *layout, int alignment) {STUB_ASSERT(0);}
void uiLayoutSetScaleX(struct uiLayout *layout, float scale) {STUB_ASSERT(0);}
void uiLayoutSetScaleY(struct uiLayout *layout, float scale) {STUB_ASSERT(0);}
void uiTemplateIconView(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) {STUB_ASSERT(0);}
void ED_base_object_free_and_unlink(struct Scene *scene, struct Base *base) {STUB_ASSERT(0);}
void ED_mesh_geometry_add(struct Mesh *mesh, struct ReportList *reports, int verts, int edges, int faces) {STUB_ASSERT(0);}
void ED_mesh_material_add(struct Mesh *me, struct Material *ma) {STUB_ASSERT(0);}
void ED_mesh_transform(struct Mesh *me, float *mat) {STUB_ASSERT(0);}
void ED_mesh_update(struct Mesh *mesh, struct bContext *C) {STUB_ASSERT(0);}
void ED_mesh_vertices_add(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_edges_add(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_tessfaces_add(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_loops_add(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_polys_add(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_vertices_remove(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_edges_remove(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_faces_remove(struct Mesh *mesh, struct ReportList *reports, int count) {STUB_ASSERT(0);}
void ED_mesh_material_link(struct Mesh *mesh, struct Material *ma) {STUB_ASSERT(0);}
int ED_mesh_color_add(struct Mesh *me, const char *name, const bool active_set) {STUB_ASSERT(0); return -1; }
int ED_mesh_uv_texture_add(struct Mesh *me, const char *name, const bool active_set) {STUB_ASSERT(0); return -1; }
bool ED_mesh_color_remove_named(struct Mesh *me, const char *name) {STUB_ASSERT(0); return false; }
bool ED_mesh_uv_texture_remove_named(struct Mesh *me, const char *name) {STUB_ASSERT(0); return false; }
void ED_object_constraint_dependency_update(struct Scene *scene, struct Object *ob) {STUB_ASSERT(0);}
void ED_object_constraint_update(struct Object *ob) {STUB_ASSERT(0);}
struct bDeformGroup *ED_vgroup_add_name(struct Object *ob, char *name) {STUB_ASSERT(0); return (struct bDeformGroup *) NULL;}
void ED_vgroup_vert_add(struct Object *ob, struct bDeformGroup *dg, int vertnum, float weight, int assignmode) {STUB_ASSERT(0);}
void ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum) {STUB_ASSERT(0);}
void ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum) {STUB_ASSERT(0);}
void ED_vgroup_delete(struct Object *ob, struct bDeformGroup *defgroup) {STUB_ASSERT(0);}
void ED_vgroup_clear(struct Object *ob) {STUB_ASSERT(0);}
void ED_vgroup_object_is_edit_mode(struct Object *ob) {STUB_ASSERT(0);}
long mesh_mirrtopo_table(struct Object *ob, char mode) {STUB_ASSERT(0); return 0; }
intptr_t mesh_octree_table(struct Object *ob, struct BMEditMesh *em, float *co, char mode) {STUB_ASSERT(0); return 0; }

void ED_sequencer_update_view(struct bContext *C, int view) {STUB_ASSERT(0);}
float ED_rollBoneToVector(struct EditBone *bone, float new_up_axis[3]) {STUB_ASSERT(0); return 0.0f;}
void ED_space_image_get_size(struct SpaceImage *sima, int *width, int *height) {STUB_ASSERT(0);}
int ED_space_image_check_show_maskedit(struct Scene *scene, struct SpaceImage *sima) {STUB_ASSERT(0); return 0;}

bool ED_texture_context_check_world(struct bContext *C) {STUB_ASSERT(0); return false;}
bool ED_texture_context_check_material(struct bContext *C) {STUB_ASSERT(0); return false;}
bool ED_texture_context_check_lamp(struct bContext *C) {STUB_ASSERT(0); return false;}
bool ED_texture_context_check_particles(struct bContext *C) {STUB_ASSERT(0); return false;}
bool ED_texture_context_check_others(struct bContext *C) {STUB_ASSERT(0); return false;}

void ED_nurb_set_spline_type(struct Nurb *nu, int type) {STUB_ASSERT(0);}

void ED_mball_transform(struct MetaBall *mb, float *mat) {STUB_ASSERT(0);}

bool snapObjectsRayEx(struct Scene *scene, struct Base *base_act) {STUB_ASSERT(0); return 0;}

void make_editLatt(struct Object *obedit) {STUB_ASSERT(0);}
void load_editLatt(struct Object *obedit) {STUB_ASSERT(0);}

void load_editNurb(struct Object *obedit) {STUB_ASSERT(0);}
void make_editNurb(struct Object *obedit) {STUB_ASSERT(0);}


void uiItemR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int flag, char *name, int icon) {STUB_ASSERT(0);}

struct PointerRNA uiItemFullO(struct uiLayout *layout, char *idname, char *name, int icon, struct IDProperty *properties, int context, int flag) {struct PointerRNA a = {{0}}; STUB_ASSERT(0); return a;}
PointerRNA uiItemFullO_ptr(struct uiLayout *layout, struct wmOperatorType *ot, const char *name, int icon, struct IDProperty *properties, int context, int flag) {struct PointerRNA a = {{0}}; STUB_ASSERT(0); return a;}
struct uiLayout *uiLayoutRow(struct uiLayout *layout, bool align) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutColumn(struct uiLayout *layout, bool align) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutColumnFlow(struct uiLayout *layout, int number, bool align) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutBox(struct uiLayout *layout) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}
struct uiLayout *uiLayoutSplit(struct uiLayout *layout, float percentage, bool align) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}
int uiLayoutGetRedAlert(struct uiLayout *layout) {STUB_ASSERT(0); return 0;}
void uiLayoutSetRedAlert(struct uiLayout *layout, int redalert) {STUB_ASSERT(0);}
void uiItemsEnumR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname) {STUB_ASSERT(0);}
void uiItemMenuEnumR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *name, int icon) {STUB_ASSERT(0);}
void uiItemEnumR_string(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *value, char *name, int icon) {STUB_ASSERT(0);}
void uiItemPointerR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, struct PointerRNA *searchptr, char *searchpropname, char *name, int icon) {STUB_ASSERT(0);}
void uiItemPointerSubR(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, char *searchpropname, char *name, int icon) {STUB_ASSERT(0);}
void uiItemsEnumO(struct uiLayout *layout, char *opname, char *propname) {STUB_ASSERT(0);}
void uiItemEnumO_string(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value_str) {STUB_ASSERT(0);}
void uiItemMenuEnumO(struct uiLayout *layout, char *opname, char *propname, char *name, int icon) {STUB_ASSERT(0);}
void uiItemBooleanO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, int value) {STUB_ASSERT(0);}
void uiItemIntO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, int value) {STUB_ASSERT(0);}
void uiItemFloatO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, float value) {STUB_ASSERT(0);}
void uiItemStringO(struct uiLayout *layout, char *name, int icon, char *opname, char *propname, char *value) {STUB_ASSERT(0);}
void uiItemL(struct uiLayout *layout, const char *name, int icon) {STUB_ASSERT(0);}
void uiItemM(struct uiLayout *layout, struct bContext *C, char *menuname, char *name, int icon) {STUB_ASSERT(0);}
void uiItemS(struct uiLayout *layout) {STUB_ASSERT(0);}
void uiItemFullR(struct uiLayout *layout, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, int value, int flag, char *name, int icon) {STUB_ASSERT(0);}
void uiLayoutSetContextPointer(struct uiLayout *layout, char *name, struct PointerRNA *ptr) {STUB_ASSERT(0);}
char *uiLayoutIntrospect(struct uiLayout *layout) {STUB_ASSERT(0); return (char *)NULL;}
void UI_reinit_font(void) {STUB_ASSERT(0);}
int UI_rnaptr_icon_get(struct bContext *C, struct PointerRNA *ptr, int rnaicon, const bool big) {STUB_ASSERT(0); return 0;}
struct bTheme *UI_GetTheme(void) {STUB_ASSERT(0); return (struct bTheme *) NULL;}

/* rna template */
void uiTemplateAnyID(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *text) {STUB_ASSERT(0);}
void uiTemplatePathBuilder(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *root_ptr, char *text) {STUB_ASSERT(0);}
void uiTemplateHeader(struct uiLayout *layout, struct bContext *C, int menus) {STUB_ASSERT(0);}
void uiTemplateID(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *newop, char *unlinkop) {STUB_ASSERT(0);}
struct uiLayout *uiTemplateModifier(struct uiLayout *layout, struct PointerRNA *ptr) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}
struct uiLayout *uiTemplateConstraint(struct uiLayout *layout, struct PointerRNA *ptr) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}
void uiTemplatePreview(struct uiLayout *layout, struct ID *id, int show_buttons, struct ID *parent, struct MTex *slot) {STUB_ASSERT(0);}
void uiTemplateIDPreview(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, char *newop, char *openop, char *unlinkop, int rows, int cols) {STUB_ASSERT(0);}
void uiTemplateCurveMapping(struct uiLayout *layout, struct CurveMapping *cumap, int type, int compact) {STUB_ASSERT(0);}
void uiTemplateColorRamp(struct uiLayout *layout, struct ColorBand *coba, int expand) {STUB_ASSERT(0);}
void uiTemplateLayers(struct uiLayout *layout, struct PointerRNA *ptr, char *propname) {STUB_ASSERT(0);}
void uiTemplateImageLayers(struct uiLayout *layout, struct bContext *C, struct Image *ima, struct ImageUser *iuser) {STUB_ASSERT(0);}
void uiTemplateList(struct uiLayout *layout, struct bContext *C, const char *listtype_name, const char *list_id,
                    PointerRNA *dataptr, const char *propname, PointerRNA *active_dataptr,
                    const char *active_propname, int rows, int maxrows, int layout_type) {STUB_ASSERT(0);}
void uiTemplateRunningJobs(struct uiLayout *layout, struct bContext *C) {STUB_ASSERT(0);}
void uiTemplateOperatorSearch(struct uiLayout *layout) {STUB_ASSERT(0);}
void uiTemplateHeader3D(struct uiLayout *layout, struct bContext *C) {STUB_ASSERT(0);}
void uiTemplateEditModeSelection(struct uiLayout *layout, struct bContext *C) {STUB_ASSERT(0);}
void uiTemplateImage(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, char *propname, struct PointerRNA *userptr, int compact) {STUB_ASSERT(0);}
void uiTemplateColorPicker(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int value_slider) {STUB_ASSERT(0);}
void uiTemplateHistogram(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int expand) {STUB_ASSERT(0);}
void uiTemplateReportsBanner(struct uiLayout *layout, struct bContext *C, struct wmOperator *op) {STUB_ASSERT(0);}
void uiTemplateWaveform(struct uiLayout *layout, struct PointerRNA *ptr, char *propname, int expand) {STUB_ASSERT(0);}
void uiTemplateVectorscope(struct uiLayout *_self, struct PointerRNA *data, char *property, int expand) {STUB_ASSERT(0);}
void uiTemplateNodeLink(struct uiLayout *layout, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input) {STUB_ASSERT(0);}
void uiTemplateNodeView(struct uiLayout *layout, struct bContext *C, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input) {STUB_ASSERT(0);}
void uiTemplateTextureUser(struct uiLayout *layout, struct bContext *C) {STUB_ASSERT(0);}
void uiTemplateTextureShow(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop) {STUB_ASSERT(0);}
void uiTemplateKeymapItemProperties(struct uiLayout *layout, struct PointerRNA *ptr) {STUB_ASSERT(0);}
void uiTemplateMovieClip(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, int compact) {STUB_ASSERT(0);}
void uiTemplateMovieclipInformation(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *userptr) {STUB_ASSERT(0);}
void uiTemplateTrack(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) {STUB_ASSERT(0);}
void uiTemplateMarker(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, PointerRNA *userptr, PointerRNA *trackptr, int compact) {STUB_ASSERT(0);}
void uiTemplateImageSettings(struct uiLayout *layout, struct PointerRNA *imfptr) {STUB_ASSERT(0);}
void uiTemplateColorspaceSettings(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) {STUB_ASSERT(0);}
void uiTemplateColormanagedViewSettings(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, int show_global_settings) {STUB_ASSERT(0);}
void uiTemplateComponentMenu(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name){}
void uiTemplateNodeSocket(struct uiLayout *layout, struct bContext *C, float *color) {STUB_ASSERT(0);}

/* rna render */
struct RenderResult *RE_engine_begin_result(struct RenderEngine *engine, int x, int y, int w, int h) {STUB_ASSERT(0); return (struct RenderResult *) NULL;}
struct RenderResult *RE_AcquireResultRead(struct Render *re) {STUB_ASSERT(0); return (struct RenderResult *) NULL;}
struct RenderResult *RE_AcquireResultWrite(struct Render *re) {STUB_ASSERT(0); return (struct RenderResult *) NULL;}
struct RenderStats *RE_GetStats(struct Render *re) {STUB_ASSERT(0); return (struct RenderStats *) NULL;}
struct RenderData *RE_engine_get_render_data(struct Render *re) {STUB_ASSERT(0); return (struct RenderData *) NULL;}
void RE_engine_update_result(struct RenderEngine *engine, struct RenderResult *result) {STUB_ASSERT(0);}
void RE_engine_update_progress(struct RenderEngine *engine, float progress) {STUB_ASSERT(0);}
void RE_engine_end_result(struct RenderEngine *engine, struct RenderResult *result) {STUB_ASSERT(0);}
void RE_engine_update_stats(struct RenderEngine *engine, char *stats, char *info) {STUB_ASSERT(0);}
void RE_layer_load_from_file(struct RenderLayer *layer, struct ReportList *reports, char *filename) {STUB_ASSERT(0);}
void RE_result_load_from_file(struct RenderResult *result, struct ReportList *reports, char *filename) {STUB_ASSERT(0);}
void RE_AcquireResultImage(struct Render *re, struct RenderResult *rr) {STUB_ASSERT(0);}
void RE_ReleaseResult(struct Render *re) {STUB_ASSERT(0);}
void RE_ReleaseResultImage(struct Render *re) {STUB_ASSERT(0);}
int RE_engine_test_break(struct RenderEngine *engine) {STUB_ASSERT(0); return 0;}
void RE_engines_init() {STUB_ASSERT(0);}
void RE_engines_exit() {STUB_ASSERT(0);}
void RE_engine_report(struct RenderEngine *engine, int type, const char *msg) {STUB_ASSERT(0);}
ListBase R_engines = {NULL, NULL};
void RE_engine_free(struct RenderEngine *engine) {STUB_ASSERT(0);}
struct RenderEngineType *RE_engines_find(const char *idname) {STUB_ASSERT(0); return NULL; }
void RE_engine_update_memory_stats(struct RenderEngine *engine, float mem_used, float mem_peak) {STUB_ASSERT(0);}
struct RenderEngine *RE_engine_create(struct RenderEngineType *type) {STUB_ASSERT(0); return NULL; }
void RE_FreePersistentData(void) {STUB_ASSERT(0);}

/* python */
struct wmOperatorType *WM_operatortype_find(const char *idname, int quiet) {STUB_ASSERT(0); return (struct wmOperatorType *) NULL;}
struct GHashIterator *WM_operatortype_iter() {STUB_ASSERT(0); return (struct GHashIterator *) NULL;}
struct wmOperatorType *WM_operatortype_exists(const char *idname) {STUB_ASSERT(0); return (struct wmOperatorType *) NULL;}
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot, const char *idname) {STUB_ASSERT(0); return (struct wmOperatorTypeMacro *) NULL;}
int WM_operator_call_py(struct bContext *C, struct wmOperatorType *ot, short context, short is_undo, struct PointerRNA *properties, struct ReportList *reports) {STUB_ASSERT(0); return 0;}
int WM_operatortype_remove(const char *idname) {STUB_ASSERT(0); return 0;}
int WM_operator_poll(struct bContext *C, struct wmOperatorType *ot) {STUB_ASSERT(0); return 0;}
int WM_operator_poll_context(struct bContext *C, struct wmOperatorType *ot, int context) {STUB_ASSERT(0); return 0;}
int WM_operator_props_popup(struct bContext *C, struct wmOperator *op, struct wmEvent *event) {STUB_ASSERT(0); return 0;}
void WM_operator_properties_free(struct PointerRNA *ptr) {STUB_ASSERT(0);}
void WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring) {STUB_ASSERT(0);}
void WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot) {STUB_ASSERT(0);}
void WM_operator_properties_sanitize(struct PointerRNA *ptr, const short no_context) {STUB_ASSERT(0);}
void WM_operatortype_append_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata) {STUB_ASSERT(0);}
void WM_operatortype_append_macro_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata) {STUB_ASSERT(0);}
void WM_operator_bl_idname(char *to, const char *from) {STUB_ASSERT(0);}
void WM_operator_py_idname(char *to, const char *from) {STUB_ASSERT(0);}
void WM_operator_ui_popup(struct bContext *C, struct wmOperator *op, int width, int height) {STUB_ASSERT(0);}
short insert_keyframe(struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag) {STUB_ASSERT(0); return 0;}
short delete_keyframe(struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag) {STUB_ASSERT(0); return 0;}
char *WM_operator_pystring(struct bContext *C, struct wmOperatorType *ot, struct PointerRNA *opptr, int all_args) {STUB_ASSERT(0); return (char *)NULL;}
struct wmKeyMapItem *WM_modalkeymap_add_item(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value) {STUB_ASSERT(0); return (struct wmKeyMapItem *)NULL;}
struct wmKeyMapItem *WM_modalkeymap_add_item_str(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, const char *value) {STUB_ASSERT(0); return (struct wmKeyMapItem *)NULL;}
struct wmKeyMap *WM_modalkeymap_add(struct wmKeyConfig *keyconf, char *idname, EnumPropertyItem *items) {STUB_ASSERT(0); return (struct wmKeyMap *) NULL;}
struct uiPopupMenu *uiPupMenuBegin(struct bContext *C, const char *title, int icon) {STUB_ASSERT(0); return (struct uiPopupMenu *) NULL;}
void uiPupMenuEnd(struct bContext *C, struct uiPopupMenu *head) {STUB_ASSERT(0);}
struct uiLayout *uiPupMenuLayout(struct uiPopupMenu *head) {STUB_ASSERT(0); return (struct uiLayout *) NULL;}

/* RNA COLLADA dependency */
int collada_export(struct Scene *sce, const char *filepath) {STUB_ASSERT(0); return 0; }

int sculpt_get_brush_size(struct Brush *brush) {STUB_ASSERT(0); return 0;}
void sculpt_set_brush_size(struct Brush *brush, int size) {STUB_ASSERT(0);}
int sculpt_get_lock_brush_size(struct Brush *brush) {STUB_ASSERT(0); return 0;}
float sculpt_get_brush_unprojected_radius(struct Brush *brush) {STUB_ASSERT(0); return 0.0f;}
void sculpt_set_brush_unprojected_radius(struct Brush *brush, float unprojected_radius) {STUB_ASSERT(0);}
float sculpt_get_brush_alpha(struct Brush *brush) {STUB_ASSERT(0); return 0.0f;}
void sculpt_set_brush_alpha(struct Brush *brush, float alpha) {STUB_ASSERT(0);}
void ED_sculpt_modifiers_changed(struct Object *ob) {STUB_ASSERT(0);}
void ED_mesh_calc_tessface(struct Mesh *mesh) {STUB_ASSERT(0);}

/* bpy/python internal api */
void operator_wrapper(struct wmOperatorType *ot, void *userdata) {STUB_ASSERT(0);}
void BPY_text_free_code(struct Text *text) {STUB_ASSERT(0);}
void BPY_id_release(struct Text *text) {STUB_ASSERT(0);}
int BPY_context_member_get(struct Context *C, const char *member, struct bContextDataResult *result) {STUB_ASSERT(0); return 0; }
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct) {STUB_ASSERT(0);}
float BPY_driver_exec(struct ChannelDriver *driver, const float evaltime) {STUB_ASSERT(0); return 0.0f;} /* might need this one! */
void BPY_DECREF(void *pyob_ptr) {STUB_ASSERT(0);}
void BPY_pyconstraint_exec(struct bPythonConstraint *con, struct bConstraintOb *cob, struct ListBase *targets) {STUB_ASSERT(0);}
void macro_wrapper(struct wmOperatorType *ot, void *userdata) {STUB_ASSERT(0);}
int pyrna_id_FromPyObject(struct PyObject *obj, struct ID **id) {STUB_ASSERT(0); return 0; }
struct PyObject *pyrna_id_CreatePyObject(struct ID *id) {STUB_ASSERT(0); return NULL; }
void BPY_context_update(struct bContext *C) {STUB_ASSERT(0);};
const char *BPY_app_translations_py_pgettext(const char *msgctxt, const char *msgid) {STUB_ASSERT(0); return msgid; }

#ifdef WITH_FREESTYLE
/* Freestyle */
void FRS_init_freestyle_config(struct FreestyleConfig *config) {STUB_ASSERT(0);}
void FRS_free_freestyle_config(struct FreestyleConfig *config) {STUB_ASSERT(0);}
void FRS_copy_freestyle_config(struct FreestyleConfig *new_config, struct FreestyleConfig *config) {STUB_ASSERT(0);}
struct FreestyleLineSet *FRS_get_active_lineset(struct FreestyleConfig *config) {STUB_ASSERT(0); return NULL; }
short FRS_get_active_lineset_index(struct FreestyleConfig *config) {STUB_ASSERT(0); return 0; }
void FRS_set_active_lineset_index(struct FreestyleConfig *config, short index) {STUB_ASSERT(0);}
void FRS_unlink_target_object(struct FreestyleConfig *config, struct Object *ob) {STUB_ASSERT(0);}
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
                            int depth) {STUB_ASSERT(0); return 0; }

/* intern/cycles */
struct CCLDeviceInfo;
struct CCLDeviceInfo *CCL_compute_device_list(int opencl) {STUB_ASSERT(0); return NULL; }

/* compositor */
void COM_execute(struct bNodeTree *editingtree, int rendering) {STUB_ASSERT(0);}

char blender_path[] = "";

#endif // WITH_GAMEENGINE
