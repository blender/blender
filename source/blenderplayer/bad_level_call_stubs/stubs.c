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

#define ASSERT_STUBS 0
#if ASSERT_STUBS
#  include <assert.h>
#  define STUB_ASSERT(x) (assert(x))
#else
#  define STUB_ASSERT(x)
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
struct Lattice;
struct ModifierData;
struct MovieClip;
struct MultiresModifierData;
struct HookModifierData;
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


/* -------------------------------------------------------------------- */
/* Declarations */

/* may cause troubles... enable for now so args match for certain */
#if 1
#if defined(__GNUC__)
#  pragma GCC diagnostic error "-Wmissing-prototypes"
#endif

#include "../../intern/cycles/blender/CCL_api.h"
#include "../../intern/dualcon/dualcon.h"
#include "../../intern/elbeem/extern/elbeem.h"
#include "../blender/blenkernel/BKE_modifier.h"
#include "../blender/collada/collada.h"
#include "../blender/compositor/COM_compositor.h"
#include "../blender/editors/include/ED_armature.h"
#include "../blender/editors/include/ED_buttons.h"
#include "../blender/editors/include/ED_clip.h"
#include "../blender/editors/include/ED_curve.h"
#include "../blender/editors/include/ED_fileselect.h"
#include "../blender/editors/include/ED_image.h"
#include "../blender/editors/include/ED_info.h"
#include "../blender/editors/include/ED_keyframes_edit.h"
#include "../blender/editors/include/ED_keyframing.h"
#include "../blender/editors/include/ED_lattice.h"
#include "../blender/editors/include/ED_mball.h"
#include "../blender/editors/include/ED_mesh.h"
#include "../blender/editors/include/ED_node.h"
#include "../blender/editors/include/ED_object.h"
#include "../blender/editors/include/ED_particle.h"
#include "../blender/editors/include/ED_render.h"
#include "../blender/editors/include/ED_screen.h"
#include "../blender/editors/include/ED_space_api.h"
#include "../blender/editors/include/ED_transform.h"
#include "../blender/editors/include/ED_view3d.h"
#include "../blender/editors/include/UI_interface.h"
#include "../blender/editors/include/UI_interface_icons.h"
#include "../blender/editors/include/UI_resources.h"
#include "../blender/editors/include/UI_view2d.h"
#include "../blender/python/BPY_extern.h"
#include "../blender/render/extern/include/RE_engine.h"
#include "../blender/render/extern/include/RE_pipeline.h"
#include "../blender/render/extern/include/RE_render_ext.h"
#include "../blender/render/extern/include/RE_shader_ext.h"
#include "../blender/windowmanager/WM_api.h"


/* -------------------------------------------------------------------- */
/* Externs
 * (ideally we wouldn't have _any_ but we can't include all directly)
 */

/* bpy_operator_wrap.h */
extern void macro_wrapper(struct wmOperatorType *ot, void *userdata);
extern void operator_wrapper(struct wmOperatorType *ot, void *userdata);
/* bpy_rna.h */
extern bool pyrna_id_FromPyObject(struct PyObject *obj, struct ID **id);
extern const char *BPY_app_translations_py_pgettext(const char *msgctxt, const char *msgid);
extern const char *BPY_app_translations_py_pgettext(const char *msgctxt, const char *msgid);
extern struct PyObject *pyrna_id_CreatePyObject(struct ID *id);

#endif
/* end declarations */


/* -------------------------------------------------------------------- */
/* Return Macro's */

#include <string.h>  /* memset */
#define RET_NULL {STUB_ASSERT(0); return (void *) NULL;}
#define RET_ZERO {STUB_ASSERT(0); return 0;}
#define RET_MINUSONE {STUB_ASSERT(0); return -1;}
#define RET_STRUCT(t) {struct t v; STUB_ASSERT(0); memset(&v, 0, sizeof(v)); return v;}
#define RET_ARG(arg) {STUB_ASSERT(0); return arg; }
#define RET_NONE {STUB_ASSERT(0);}


/* -------------------------------------------------------------------- */
/* Stubs */

/*new render funcs */
void EDBM_selectmode_set(struct BMEditMesh *em) RET_NONE
void EDBM_mesh_load(struct Object *ob) RET_NONE
void EDBM_mesh_make(struct ToolSettings *ts, struct Object *ob) RET_NONE
void EDBM_mesh_normals_update(struct BMEditMesh *em) RET_NONE
void *g_system;

float *RE_RenderLayerGetPass(struct RenderLayer *rl, int passtype) RET_NULL
float RE_filter_value(int type, float x) RET_ZERO
struct RenderLayer *RE_GetRenderLayer(struct RenderResult *rr, const char *name) RET_NULL

/* zbuf.c stub */
void antialias_tagbuf(int xsize, int ysize, char *rectmove) RET_NONE
void RE_zbuf_accumulate_vecblur(struct NodeBlurData *nbd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect) RET_NONE

/* imagetexture.c stub */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result) RET_NONE

/* texture.c */
int multitex_ext(struct Tex *tex, float texvec[3], float dxt[3], float dyt[3], int osatex, struct TexResult *texres, struct ImagePool *pool, bool scene_color_manage) RET_ZERO
int multitex_ext_safe(struct Tex *tex, float texvec[3], struct TexResult *texres, struct ImagePool *pool, bool scene_color_manage) RET_ZERO
int multitex_nodes(struct Tex *tex, float texvec[3], float dxt[3], float dyt[3], int osatex, struct TexResult *texres, const short thread, short which_output, struct ShadeInput *shi, struct MTex *mtex, struct ImagePool *pool) RET_ZERO

struct Material *RE_init_sample_material(struct Material *orig_mat, struct Scene *scene) RET_NULL
void RE_free_sample_material(struct Material *mat) RET_NONE
void RE_sample_material_color(struct Material *mat, float color[3], float *alpha, const float volume_co[3], const float surface_co[3],
                              int face_index, short hit_quad, struct DerivedMesh *orcoDm, struct Object *ob) RET_NONE

/* skin modifier*/
void modifier_skin_customdata_ensure(struct Object *ob) RET_NONE

/* nodes */
struct Render *RE_GetRender(const char *name) RET_NULL
float RE_lamp_get_data(struct ShadeInput *shi, struct Object *lamp_obj, float col[4], float lv[3], float *dist, float shadow[4]) RET_ZERO

/* blenkernel */
void RE_FreeRenderResult(struct RenderResult *res) RET_NONE
void RE_FreeAllRenderResults(void) RET_NONE
struct RenderResult *RE_MultilayerConvert(void *exrhandle, const char *colorspace, int predivide, int rectx, int recty) RET_NULL
struct Scene *RE_GetScene(struct Render *re) RET_NULL
void RE_Database_Free(struct Render *re) RET_NONE
void RE_FreeRender(struct Render *re) RET_NONE
void RE_DataBase_GetView(struct Render *re, float mat[4][4]) RET_NONE
int externtex(struct MTex *mtex, const float vec[3], float *tin, float *tr, float *tg, float *tb, float *ta, const int thread, struct ImagePool *poold) RET_ZERO
float texture_value_blend(float tex, float out, float fact, float facg, int blendtype) RET_ZERO
void texture_rgb_blend(float in[3], const float tex[3], const float out[3], float fact, float facg, int blendtype) RET_NONE
const unsigned char stipple_quarttone[128];
double elbeemEstimateMemreq(int res, float sx, float sy, float sz, int refine, char *retstr) RET_ZERO
struct Render *RE_NewRender(const char *name) RET_NULL
void RE_SwapResult(struct Render *re, struct RenderResult **rr) RET_NONE
void RE_BlenderFrame(struct Render *re, struct Main *bmain, struct Scene *scene, struct SceneRenderLayer *srl, struct Object *camera_override, unsigned int lay_override, int frame, const short write_still) RET_NONE
int RE_WriteEnvmapResult(struct ReportList *reports, struct Scene *scene, struct EnvMap *env, const char *relpath, const char imtype, float layout[12]) RET_ZERO

/* rna */
float *ED_view3d_cursor3d_get(struct Scene *scene, struct View3D *v3d) RET_NULL
void WM_menutype_free(void) RET_NONE
void WM_menutype_freelink(struct MenuType *mt) RET_NONE
bool WM_menutype_add(struct MenuType *mt) RET_ZERO
int WM_operator_props_dialog_popup(struct bContext *C, struct wmOperator *op, int width, int height) RET_ZERO
int WM_operator_confirm(struct bContext *C, struct wmOperator *op, const struct wmEvent *event) RET_ZERO
struct MenuType *WM_menutype_find(const char *idname, bool quiet) RET_NULL
void WM_operator_stack_clear(struct wmWindowManager *wm) RET_NONE
void WM_operator_handlers_clear(wmWindowManager *wm, struct wmOperatorType *ot) RET_NONE

void WM_autosave_init(wmWindowManager *wm) RET_NONE
void WM_jobs_kill_all_except(struct wmWindowManager *wm, void *owner) RET_NONE

char *WM_clipboard_text_get(bool selection, int *r_len) RET_NULL
char *WM_clipboard_text_get_firstline(bool selection, int *r_len) RET_NULL
void WM_clipboard_text_set(const char *buf, bool selection) RET_NONE

void WM_cursor_set(struct wmWindow *win, int curor) RET_NONE
void WM_cursor_modal_set(struct wmWindow *win, int curor) RET_NONE
void WM_cursor_modal_restore(struct wmWindow *win) RET_NONE
void WM_cursor_time(struct wmWindow *win, int nr) RET_NONE
void WM_cursor_warp(struct wmWindow *win, int x, int y) RET_NONE

void                WM_uilisttype_init(void) RET_NONE
struct uiListType  *WM_uilisttype_find(const char *idname, bool quiet) RET_NULL
bool                WM_uilisttype_add(struct uiListType *ult) RET_ZERO
void                WM_uilisttype_freelink(struct uiListType *ult) RET_NONE
void                WM_uilisttype_free(void) RET_NONE

struct wmKeyMapItem *WM_keymap_item_find_id(struct wmKeyMap *keymap, int id) RET_NULL
int WM_enum_search_invoke(struct bContext *C, struct wmOperator *op, const struct wmEvent *event) RET_ZERO
void WM_event_add_notifier(const struct bContext *C, unsigned int type, void *reference) RET_NONE
void WM_main_add_notifier(unsigned int type, void *reference) RET_NONE
void ED_armature_bone_rename(struct bArmature *arm, const char *oldnamep, const char *newnamep) RET_NONE
void ED_armature_transform(struct bArmature *arm, float mat[4][4]) RET_NONE
struct wmEventHandler *WM_event_add_modal_handler(struct bContext *C, struct wmOperator *op) RET_NULL
struct wmTimer *WM_event_add_timer(struct wmWindowManager *wm, struct wmWindow *win, int event_type, double timestep) RET_NULL
void WM_event_remove_timer(struct wmWindowManager *wm, struct wmWindow *win, struct wmTimer *timer) RET_NONE
void ED_armature_edit_bone_remove(struct bArmature *arm, struct EditBone *exBone) RET_NONE
void object_test_constraints(struct Object *owner) RET_NONE
void ED_armature_ebone_to_mat4(struct EditBone *ebone, float mat[4][4]) RET_NONE
void ED_object_parent(struct Object *ob, struct Object *par, int type, const char *substr) RET_NONE
void ED_object_constraint_set_active(struct Object *ob, struct bConstraint *con) RET_NONE
void ED_node_composit_default(const struct bContext *C, struct Scene *scene) RET_NONE
void *ED_region_draw_cb_activate(struct ARegionType *art, void(*draw)(const struct bContext *, struct ARegion *, void *), void *custumdata, int type) RET_ZERO /* XXX this one looks weird */
void *ED_region_draw_cb_customdata(void *handle) RET_ZERO /* XXX This one looks wrong also */
void ED_region_draw_cb_exit(struct ARegionType *art, void *handle) RET_NONE
void ED_area_headerprint(struct ScrArea *sa, const char *str) RET_NONE
void UI_view2d_region_to_view(struct View2D *v2d, float x, float y, float *viewx, float *viewy) RET_NONE
void UI_view2d_view_to_region(struct View2D *v2d, float x, float y, int *regionx, int *regiony) RET_NONE
void UI_view2d_to_region_no_clip(struct View2D *v2d, float x, float y, int *regionx, int *region_y) RET_NONE

struct EditBone *ED_armature_bone_get_mirrored(const struct ListBase *edbo, EditBone *ebo) RET_NULL
struct EditBone *ED_armature_edit_bone_add(struct bArmature *arm, const char *name) RET_NULL
struct ListBase *get_active_constraints (struct Object *ob) RET_NULL
struct ListBase *get_constraint_lb(struct Object *ob, struct bConstraint *con, struct bPoseChannel **pchan_r) RET_NULL

bool ED_space_image_show_uvedit(struct SpaceImage *sima, struct Object *obedit) RET_ZERO
bool ED_space_image_show_render(struct SpaceImage *sima) RET_ZERO
bool ED_space_image_show_paint(struct SpaceImage *sima) RET_ZERO
void ED_space_image_paint_update(struct wmWindowManager *wm, struct ToolSettings *settings) RET_NONE
void ED_space_image_set(struct SpaceImage *sima, struct Scene *scene, struct Object *obedit, struct Image *ima) RET_NONE
void ED_space_image_uv_sculpt_update(struct wmWindowManager *wm, struct ToolSettings *settings) RET_NONE

void ED_screen_set_scene(struct bContext *C, struct bScreen *screen, struct Scene *scene) RET_NONE
struct MovieClip *ED_space_clip_get_clip(struct SpaceClip *sc) RET_NULL
void ED_space_clip_set_clip(struct bContext *C, struct bScreen *screen, struct SpaceClip *sc, struct MovieClip *clip) RET_NONE
void ED_space_clip_set_mask(struct bContext *C, struct SpaceClip *sc, struct Mask *mask) RET_NONE
void ED_space_image_set_mask(struct bContext *C, struct SpaceImage *sima, struct Mask *mask) RET_NONE

void ED_area_tag_redraw_regiontype(struct ScrArea *sa, int regiontype) RET_NONE
void ED_render_engine_changed(struct Main *bmain) RET_NONE

void ED_file_read_bookmarks(void) RET_NONE

struct PTCacheEdit *PE_get_current(struct Scene *scene, struct Object *ob) RET_NULL
void PE_current_changed(struct Scene *scene, struct Object *ob) RET_NONE

/* rna keymap */
struct wmKeyMap *WM_keymap_active(struct wmWindowManager *wm, struct wmKeyMap *keymap) RET_NULL
struct wmKeyMap *WM_keymap_find(struct wmKeyConfig *keyconf, const char *idname, int spaceid, int regionid) RET_NULL
struct wmKeyMapItem *WM_keymap_add_item(struct wmKeyMap *keymap, const char *idname, int type,  int val, int modifier, int keymodifier) RET_NULL
struct wmKeyMap *WM_keymap_list_find(ListBase *lb, const char *idname, int spaceid, int regionid) RET_NULL
struct wmKeyConfig *WM_keyconfig_new(struct wmWindowManager *wm, const char *idname) RET_NULL
struct wmKeyConfig *WM_keyconfig_new_user(struct wmWindowManager *wm, const char *idname) RET_NULL
bool WM_keyconfig_remove(struct wmWindowManager *wm, struct wmKeyConfig *keyconf) RET_ZERO
bool WM_keymap_remove(struct wmKeyConfig *keyconfig, struct wmKeyMap *keymap) RET_ZERO
void WM_keyconfig_set_active(struct wmWindowManager *wm, const char *idname) RET_NONE
bool WM_keymap_remove_item(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) RET_ZERO
void WM_keymap_restore_to_default(struct wmKeyMap *keymap, struct bContext *C) RET_NONE
void WM_keymap_restore_item_to_default(struct bContext *C, struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) RET_NONE
void WM_keymap_properties_reset(struct wmKeyMapItem *kmi, struct IDProperty *properties) RET_NONE
void WM_keyconfig_update_tag(struct wmKeyMap *keymap, struct wmKeyMapItem *kmi) RET_NONE
int WM_keymap_item_compare(struct wmKeyMapItem *k1, struct wmKeyMapItem *k2) RET_ZERO
int	WM_keymap_map_type_get(struct wmKeyMapItem *kmi) RET_ZERO


/* rna editors */
struct EditMesh;

struct FCurve *verify_fcurve(struct bAction *act, const char group[], struct PointerRNA *ptr, const char rna_path[], const int array_index, short add) RET_NULL
int insert_vert_fcurve(struct FCurve *fcu, float x, float y, short flag) RET_ZERO
void delete_fcurve_key(struct FCurve *fcu, int index, bool do_recalc) RET_NONE
struct KeyingSetInfo *ANIM_keyingset_info_find_name (const char name[]) RET_NULL
struct KeyingSet *ANIM_scene_get_active_keyingset (struct Scene *scene) RET_NULL
int ANIM_scene_get_keyingset_index(struct Scene *scene, struct KeyingSet *ks) RET_ZERO
struct ListBase builtin_keyingsets;
void ANIM_keyingset_info_register(struct KeyingSetInfo *ksi) RET_NONE
void ANIM_keyingset_info_unregister(struct Main *bmain, KeyingSetInfo *ksi) RET_NONE
short ANIM_validate_keyingset(struct bContext *C, struct ListBase *dsources, struct KeyingSet *ks) RET_ZERO
short ANIM_add_driver(struct ReportList *reports, struct ID *id, const char rna_path[], int array_index, short flag, int type) RET_ZERO
short ANIM_remove_driver(struct ReportList *reports, struct ID *id, const char rna_path[], int array_index, short flag) RET_ZERO
void ED_space_image_release_buffer(struct SpaceImage *sima, struct ImBuf *ibuf, void *lock) RET_NONE
struct ImBuf *ED_space_image_acquire_buffer(struct SpaceImage *sima, void **lock_r) RET_NULL
void ED_space_image_get_zoom(struct SpaceImage *sima, struct ARegion *ar, float *zoomx, float *zoomy) RET_NONE
const char *ED_info_stats_string(struct Scene *scene) RET_NULL
void ED_area_tag_redraw(struct ScrArea *sa) RET_NONE
void ED_area_tag_refresh(struct ScrArea *sa) RET_NONE
void ED_area_newspace(struct bContext *C, struct ScrArea *sa, int type) RET_NONE
void ED_region_tag_redraw(struct ARegion *ar) RET_NONE
void ED_curve_transform(struct Curve *cv, float mat[4][4]) RET_NONE
void WM_event_add_fileselect(struct bContext *C, struct wmOperator *op) RET_NONE
void WM_cursor_wait(bool val) RET_NONE
void ED_node_texture_default(const struct bContext *C, struct Tex *tex) RET_NONE
void ED_node_tag_update_id(struct ID *id) RET_NONE
void ED_node_tag_update_nodetree(struct Main *bmain, struct bNodeTree *ntree) RET_NONE
void ED_node_tree_update(const struct bContext *C) RET_NONE
void ED_node_set_tree_type(struct SpaceNode *snode, struct bNodeTreeType *typeinfo){}
void ED_init_custom_node_type(struct bNodeType *ntype){}
void ED_init_custom_node_socket_type(struct bNodeSocketType *stype){}
void ED_init_standard_node_socket_type(struct bNodeSocketType *stype) RET_NONE
void ED_init_node_socket_type_virtual(struct bNodeSocketType *stype) RET_NONE
int ED_node_tree_path_length(struct SpaceNode *snode){return 0;}
void ED_node_tree_path_get(struct SpaceNode *snode, char *value){}
void ED_node_tree_path_get_fixedbuf(struct SpaceNode *snode, char *value, int max_length){}
void ED_node_tree_start(struct SpaceNode *snode, struct bNodeTree *ntree, struct ID *id, struct ID *from){}
void ED_node_tree_push(struct SpaceNode *snode, struct bNodeTree *ntree, struct bNode *gnode){}
void ED_node_tree_pop(struct SpaceNode *snode){}
int ED_view3d_scene_layer_set(int lay, const int *values, int *active) RET_ZERO
void ED_view3d_quadview_update(struct ScrArea *sa, struct ARegion *ar, bool do_clip) RET_NONE
void ED_view3d_from_m4(float mat[4][4], float ofs[3], float quat[4], float *dist) RET_NONE
struct BGpic *ED_view3D_background_image_new(struct View3D *v3d) RET_NULL
void ED_view3D_background_image_remove(struct View3D *v3d, struct BGpic *bgpic) RET_NONE
void ED_view3D_background_image_clear(struct View3D *v3d) RET_NONE
void ED_view3d_update_viewmat(struct Scene *scene, struct View3D *v3d, struct ARegion *ar, float viewmat[4][4], float winmat[4][4]) RET_NONE
float ED_view3d_grid_scale(struct Scene *scene, struct View3D *v3d, const char **grid_unit) RET_ZERO
void ED_view3d_shade_update(struct Main *bmain, struct View3D *v3d, struct ScrArea *sa) RET_NONE
void ED_node_shader_default(const struct bContext *C, struct ID *id) RET_NONE
void ED_screen_animation_timer_update(struct bScreen *screen, int redraws, int refresh) RET_NONE
struct bScreen *ED_screen_animation_playing(const struct wmWindowManager *wm) RET_NULL
void ED_base_object_select(struct Base *base, short mode) RET_NONE
bool ED_object_modifier_remove(struct ReportList *reports, struct Main *bmain, struct Object *ob, struct ModifierData *md) RET_ZERO
struct ModifierData *ED_object_modifier_add(struct ReportList *reports, struct Main *bmain, struct Scene *scene, struct Object *ob, const char *name, int type) RET_ZERO
void ED_object_modifier_clear(struct Main *bmain, struct Object *ob) RET_NONE
void ED_object_editmode_enter(struct bContext *C, int flag) RET_NONE
void ED_object_editmode_exit(struct bContext *C, int flag) RET_NONE
bool ED_object_editmode_load(struct Object *obedit) RET_ZERO
void ED_object_check_force_modifiers(struct Main *bmain, struct Scene *scene, struct Object *object) RET_NONE
bool uiLayoutGetActive(struct uiLayout *layout) RET_ZERO
int uiLayoutGetOperatorContext(struct uiLayout *layout) RET_ZERO
int uiLayoutGetAlignment(struct uiLayout *layout) RET_ZERO
bool uiLayoutGetEnabled(struct uiLayout *layout) RET_ZERO
float uiLayoutGetScaleX(struct uiLayout *layout) RET_ZERO
float uiLayoutGetScaleY(struct uiLayout *layout) RET_ZERO
void uiLayoutSetActive(struct uiLayout *layout, bool active) RET_NONE
void uiLayoutSetOperatorContext(struct uiLayout *layout, int opcontext) RET_NONE
void uiLayoutSetEnabled(uiLayout *layout, bool enabled) RET_NONE
void uiLayoutSetAlignment(uiLayout *layout, char alignment) RET_NONE
void uiLayoutSetScaleX(struct uiLayout *layout, float scale) RET_NONE
void uiLayoutSetScaleY(struct uiLayout *layout, float scale) RET_NONE
void uiTemplateIconView(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) RET_NONE
void ED_base_object_free_and_unlink(struct Main *bmain, struct Scene *scene, struct Base *base) RET_NONE
void ED_mesh_transform(struct Mesh *me, float mat[4][4]) RET_NONE
void ED_mesh_update(struct Mesh *mesh, struct bContext *C, int calc_edges, int calc_tessface) RET_NONE
void ED_mesh_vertices_add(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
void ED_mesh_edges_add(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
void ED_mesh_tessfaces_add(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
void ED_mesh_loops_add(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
void ED_mesh_polys_add(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
void ED_mesh_vertices_remove(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
void ED_mesh_edges_remove(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
void ED_mesh_faces_remove(struct Mesh *mesh, struct ReportList *reports, int count) RET_NONE
int ED_mesh_color_add(struct Mesh *me, const char *name, const bool active_set) RET_MINUSONE
int ED_mesh_uv_texture_add(struct Mesh *me, const char *name, const bool active_set) RET_MINUSONE
bool ED_mesh_color_remove_named(struct Mesh *me, const char *name) RET_ZERO
bool ED_mesh_uv_texture_remove_named(struct Mesh *me, const char *name) RET_ZERO
void ED_object_constraint_dependency_update(struct Main *bmain, struct Object *ob) RET_NONE
void ED_object_constraint_update(struct Object *ob) RET_NONE
struct bDeformGroup *ED_vgroup_add_name(struct Object *ob, const char *name) RET_NULL
void ED_vgroup_vert_add(struct Object *ob, struct bDeformGroup *dg, int vertnum, float weight, int assignmode) RET_NONE
void ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum) RET_NONE
float ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum) RET_ZERO
void ED_vgroup_delete(struct Object *ob, struct bDeformGroup *defgroup) RET_NONE
void ED_vgroup_clear(struct Object *ob) RET_NONE
bool ED_vgroup_object_is_edit_mode(struct Object *ob) RET_ZERO
int mesh_mirrtopo_table(struct Object *ob, char mode) RET_ZERO
intptr_t mesh_octree_table(struct Object *ob, struct BMEditMesh *em, const float co[3], char mode) RET_ZERO

float ED_rollBoneToVector(EditBone *bone, const float new_up_axis[3], const short axis_only) RET_ZERO
void ED_space_image_get_size(struct SpaceImage *sima, int *width, int *height) RET_NONE
bool ED_space_image_check_show_maskedit(struct Scene *scene, struct SpaceImage *sima) RET_ZERO

bool ED_texture_context_check_world(const struct bContext *C) RET_ZERO
bool ED_texture_context_check_material(const struct bContext *C) RET_ZERO
bool ED_texture_context_check_lamp(const struct bContext *C) RET_ZERO
bool ED_texture_context_check_particles(const struct bContext *C) RET_ZERO
bool ED_texture_context_check_others(const struct bContext *C) RET_ZERO

void ED_mball_transform(struct MetaBall *mb, float mat[4][4]) RET_NONE

bool snapObjectsRayEx(struct Scene *scene, struct Base *base_act, struct View3D *v3d, struct ARegion *ar, struct Object *obedit, short snap_mode,
                      struct Object **r_ob, float r_obmat[4][4],
                      const float ray_start[3], const float ray_normal[3], float *r_ray_dist,
                      const float mval[2], float *r_dist_px, float r_loc[3], float r_no[3], SnapMode mode) RET_ZERO

void make_editLatt(struct Object *obedit) RET_NONE
void load_editLatt(struct Object *obedit) RET_NONE
void ED_lattice_transform(struct Lattice *lt, float mat[4][4]) RET_NONE

void load_editNurb(struct Object *obedit) RET_NONE
void make_editNurb(struct Object *obedit) RET_NONE


void uiItemR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int flag, const char *name, int icon) RET_NONE

struct PointerRNA uiItemFullO(uiLayout *layout, const char *idname, const char *name, int icon, struct IDProperty *properties, int context, int flag) RET_STRUCT(PointerRNA)
PointerRNA uiItemFullO_ptr(struct uiLayout *layout, struct wmOperatorType *ot, const char *name, int icon, struct IDProperty *properties, int context, int flag) RET_STRUCT(PointerRNA)
struct uiLayout *uiLayoutRow(uiLayout *layout, int align) RET_NULL
struct uiLayout *uiLayoutColumn(uiLayout *layout, int align) RET_NULL
struct uiLayout *uiLayoutColumnFlow(uiLayout *layout, int number, int align) RET_NULL
struct uiLayout *uiLayoutBox(struct uiLayout *layout) RET_NULL
struct uiLayout *uiLayoutSplit(uiLayout *layout, float percentage, int align) RET_NULL
bool uiLayoutGetRedAlert(struct uiLayout *layout) RET_ZERO
void uiLayoutSetRedAlert(uiLayout *layout, bool redalert) RET_NONE
void uiItemsEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname) RET_NONE
void uiItemMenuEnumR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name, int icon) RET_NONE
void uiItemEnumR_string(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *value, const char *name, int icon) RET_NONE
void uiItemPointerR(uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *searchptr, const char *searchpropname, const char *name, int icon) RET_NONE
void uiItemsEnumO(uiLayout *layout, const char *opname, const char *propname) RET_NONE
void uiItemEnumO_string(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value) RET_NONE
void uiItemMenuEnumO(uiLayout *layout, struct bContext *C, const char *opname, const char *propname, const char *name, int icon) RET_NONE
void uiItemBooleanO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value) RET_NONE
void uiItemIntO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, int value) RET_NONE
void uiItemFloatO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, float value) RET_NONE
void uiItemStringO(uiLayout *layout, const char *name, int icon, const char *opname, const char *propname, const char *value) RET_NONE
void uiItemL(struct uiLayout *layout, const char *name, int icon) RET_NONE
void uiItemM(uiLayout *layout, struct bContext *C, const char *menuname, const char *name, int icon) RET_NONE
void uiItemS(struct uiLayout *layout) RET_NONE
void uiItemFullR(uiLayout *layout, struct PointerRNA *ptr, struct PropertyRNA *prop, int index, int value, int flag, const char *name, int icon) RET_NONE
void uiLayoutSetContextPointer(uiLayout *layout, const char *name, struct PointerRNA *ptr) RET_NONE
const char *uiLayoutIntrospect(uiLayout *layout) RET_NULL
void UI_reinit_font(void) RET_NONE
int UI_rnaptr_icon_get(struct bContext *C, struct PointerRNA *ptr, int rnaicon, const bool big) RET_ZERO
struct bTheme *UI_GetTheme(void) RET_NULL

/* rna template */
void uiTemplateAnyID(uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *proptypename, const char *text) RET_NONE
void uiTemplatePathBuilder(uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *root_ptr, const char *text) RET_NONE
void uiTemplateHeader(struct uiLayout *layout, struct bContext *C) RET_NONE
void uiTemplateID(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, const char *newop, const char *openop, const char *unlinkop) RET_NONE
struct uiLayout *uiTemplateModifier(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr) RET_NULL
struct uiLayout *uiTemplateConstraint(struct uiLayout *layout, struct PointerRNA *ptr) RET_NULL
void uiTemplatePreview(struct uiLayout *layout, struct ID *id, int show_buttons, struct ID *parent, struct MTex *slot) RET_NONE
void uiTemplateIDPreview(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, const char *newop, const char *openop, const char *unlinkop, int rows, int cols) RET_NONE
void uiTemplateCurveMapping(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int type, int levels, int brush) RET_NONE
void uiTemplateColorRamp(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int expand) RET_NONE
void uiTemplateLayers(uiLayout *layout, struct PointerRNA *ptr, const char *propname, PointerRNA *used_ptr, const char *used_propname, int active_layer) RET_NONE
void uiTemplateImageLayers(struct uiLayout *layout, struct bContext *C, struct Image *ima, struct ImageUser *iuser) RET_NONE
void uiTemplateList(struct uiLayout *layout, struct bContext *C, const char *listtype_name, const char *list_id,
                    PointerRNA *dataptr, const char *propname, PointerRNA *active_dataptr, const char *active_propname,
                    int rows, int maxrows, int layout_type, int columns) RET_NONE
void uiTemplateRunningJobs(struct uiLayout *layout, struct bContext *C) RET_NONE
void uiTemplateOperatorSearch(struct uiLayout *layout) RET_NONE
void uiTemplateHeader3D(struct uiLayout *layout, struct bContext *C) RET_NONE
void uiTemplateEditModeSelection(struct uiLayout *layout, struct bContext *C) RET_NONE
void uiTemplateImage(uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, struct PointerRNA *userptr, int compact) RET_NONE
void uiTemplateColorPicker(uiLayout *layout, struct PointerRNA *ptr, const char *propname, int value_slider, int lock, int lock_luminosity, int cubic) RET_NONE
void uiTemplateHistogram(uiLayout *layout, struct PointerRNA *ptr, const char *propname) RET_NONE
void uiTemplateReportsBanner(uiLayout *layout, struct bContext *C) RET_NONE
void uiTemplateWaveform(uiLayout *layout, struct PointerRNA *ptr, const char *propname) RET_NONE
void uiTemplateVectorscope(uiLayout *layout, struct PointerRNA *ptr, const char *propname) RET_NONE
void uiTemplateNodeLink(struct uiLayout *layout, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input) RET_NONE
void uiTemplateNodeView(struct uiLayout *layout, struct bContext *C, struct bNodeTree *ntree, struct bNode *node, struct bNodeSocket *input) RET_NONE
void uiTemplateTextureUser(struct uiLayout *layout, struct bContext *C) RET_NONE
void uiTemplateTextureShow(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop) RET_NONE
void uiTemplateKeymapItemProperties(struct uiLayout *layout, struct PointerRNA *ptr) RET_NONE
void uiTemplateMovieClip(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname, int compact) RET_NONE
void uiTemplateMovieclipInformation(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, struct PointerRNA *userptr) RET_NONE
void uiTemplateTrack(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) RET_NONE
void uiTemplateMarker(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, PointerRNA *userptr, PointerRNA *trackptr, int compact) RET_NONE
void uiTemplateImageSettings(uiLayout *layout, struct PointerRNA *imfptr, int color_management) RET_NONE
void uiTemplateColorspaceSettings(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname) RET_NONE
void uiTemplateColormanagedViewSettings(struct uiLayout *layout, struct bContext *C, struct PointerRNA *ptr, const char *propname) RET_NONE
void uiTemplateComponentMenu(struct uiLayout *layout, struct PointerRNA *ptr, const char *propname, const char *name) RET_NONE
void uiTemplateNodeSocket(struct uiLayout *layout, struct bContext *C, float *color) RET_NONE

/* rna render */
struct RenderResult *RE_engine_begin_result(RenderEngine *engine, int x, int y, int w, int h, const char *layername) RET_NULL
struct RenderResult *RE_AcquireResultRead(struct Render *re) RET_NULL
struct RenderResult *RE_AcquireResultWrite(struct Render *re) RET_NULL
struct RenderStats *RE_GetStats(struct Render *re) RET_NULL
struct RenderData *RE_engine_get_render_data(struct Render *re) RET_NULL
void RE_engine_update_result(struct RenderEngine *engine, struct RenderResult *result) RET_NONE
void RE_engine_update_progress(struct RenderEngine *engine, float progress) RET_NONE
void RE_engine_end_result(RenderEngine *engine, struct RenderResult *result, int cancel, int merge_results) RET_NONE
void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info) RET_NONE
void RE_layer_load_from_file(struct RenderLayer *layer, struct ReportList *reports, const char *filename, int x, int y) RET_NONE
void RE_result_load_from_file(struct RenderResult *result, struct ReportList *reports, const char *filename) RET_NONE
void RE_AcquireResultImage(struct Render *re, struct RenderResult *rr) RET_NONE
void RE_ReleaseResult(struct Render *re) RET_NONE
void RE_ReleaseResultImage(struct Render *re) RET_NONE
int RE_engine_test_break(struct RenderEngine *engine) RET_ZERO
void RE_engines_init() RET_NONE
void RE_engines_exit() RET_NONE
void RE_engine_report(struct RenderEngine *engine, int type, const char *msg) RET_NONE
ListBase R_engines = {NULL, NULL};
void RE_engine_free(struct RenderEngine *engine) RET_NONE
struct RenderEngineType *RE_engines_find(const char *idname) RET_NULL
void RE_engine_update_memory_stats(struct RenderEngine *engine, float mem_used, float mem_peak) RET_NONE
struct RenderEngine *RE_engine_create(struct RenderEngineType *type) RET_NULL
void RE_FreePersistentData(void) RET_NONE

/* python */
struct wmOperatorType *WM_operatortype_find(const char *idname, bool quiet) RET_NULL
struct GHashIterator *WM_operatortype_iter() RET_NULL
struct wmOperatorTypeMacro *WM_operatortype_macro_define(struct wmOperatorType *ot, const char *idname) RET_NULL
int WM_operator_call_py(struct bContext *C, struct wmOperatorType *ot, short context, struct PointerRNA *properties, struct ReportList *reports, const bool is_undo) RET_ZERO
void WM_operatortype_remove_ptr(struct wmOperatorType *ot) RET_NONE
bool WM_operatortype_remove(const char *idname) RET_ZERO
int WM_operator_poll(struct bContext *C, struct wmOperatorType *ot) RET_ZERO
int WM_operator_poll_context(struct bContext *C, struct wmOperatorType *ot, short context) RET_ZERO
int WM_operator_props_popup(struct bContext *C, struct wmOperator *op, const struct wmEvent *event) RET_ZERO
void WM_operator_properties_free(struct PointerRNA *ptr) RET_NONE
void WM_operator_properties_create(struct PointerRNA *ptr, const char *opstring) RET_NONE
void WM_operator_properties_create_ptr(struct PointerRNA *ptr, struct wmOperatorType *ot) RET_NONE
void WM_operator_properties_sanitize(struct PointerRNA *ptr, const bool no_context) RET_NONE
void WM_operatortype_append_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata) RET_NONE
void WM_operatortype_append_macro_ptr(void (*opfunc)(struct wmOperatorType *, void *), void *userdata) RET_NONE
void WM_operator_bl_idname(char *to, const char *from) RET_NONE
void WM_operator_py_idname(char *to, const char *from) RET_NONE
int WM_operator_ui_popup(struct bContext *C, struct wmOperator *op, int width, int height) RET_ZERO
void update_autoflags_fcurve(struct FCurve *fcu, struct bContext *C, struct ReportList *reports, struct PointerRNA *ptr) RET_NONE
short insert_keyframe(struct ReportList *reports, struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag) RET_ZERO
short delete_keyframe(struct ReportList *reports, struct ID *id, struct bAction *act, const char group[], const char rna_path[], int array_index, float cfra, short flag) RET_ZERO
char *WM_operator_pystring_ex(struct bContext *C, struct wmOperator *op, const bool all_args, const bool macro_args, struct wmOperatorType *ot, struct PointerRNA *opptr) RET_NULL
char *WM_operator_pystring(struct bContext *C, struct wmOperator *op, const bool all_args, const bool macro_args) RET_NULL
struct wmKeyMapItem *WM_modalkeymap_add_item(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, int value) RET_NULL
struct wmKeyMapItem *WM_modalkeymap_add_item_str(struct wmKeyMap *km, int type, int val, int modifier, int keymodifier, const char *value) RET_NULL
struct wmKeyMap *WM_modalkeymap_add(struct wmKeyConfig *keyconf, const char *idname, struct EnumPropertyItem *items) RET_NULL
struct uiPopupMenu *uiPupMenuBegin(struct bContext *C, const char *title, int icon) RET_NULL
void uiPupMenuEnd(struct bContext *C, struct uiPopupMenu *head) RET_NONE
struct uiLayout *uiPupMenuLayout(struct uiPopupMenu *head) RET_NULL

/* RNA COLLADA dependency */
int collada_export(struct Scene *sce,
                   const char *filepath,
                   int apply_modifiers,
                   BC_export_mesh_type export_mesh_type,

                   int selected,
                   int include_children,
                   int include_armatures,
                   int include_shapekeys,
                   int deform_bones_only,

                   int active_uv_only,
                   int include_uv_textures,
                   int include_material_textures,
                   int use_texture_copies,

                   int triangulate,
                   int use_object_instantiation,
                   int sort_by_name,
                   BC_export_transformation_type export_transformation_type,
                   int open_sim) RET_ZERO

void ED_mesh_calc_tessface(struct Mesh *mesh) RET_NONE

/* bpy/python internal api */
void operator_wrapper(struct wmOperatorType *ot, void *userdata) RET_NONE
void BPY_text_free_code(struct Text *text) RET_NONE
void BPY_id_release(struct ID *id) RET_NONE
int BPY_context_member_get(struct bContext *C, const char *member, struct bContextDataResult *result) RET_ZERO
void BPY_pyconstraint_target(struct bPythonConstraint *con, struct bConstraintTarget *ct) RET_NONE
float BPY_driver_exec(struct ChannelDriver *driver, const float evaltime) RET_ZERO /* might need this one! */
void BPY_DECREF(void *pyob_ptr) RET_NONE
void BPY_pyconstraint_exec(struct bPythonConstraint *con, struct bConstraintOb *cob, struct ListBase *targets) RET_NONE
void macro_wrapper(struct wmOperatorType *ot, void *userdata) RET_NONE
bool pyrna_id_FromPyObject(struct PyObject *obj, struct ID **id) RET_ZERO
struct PyObject *pyrna_id_CreatePyObject(struct ID *id) RET_NULL
void BPY_context_update(struct bContext *C) RET_NONE;
const char *BPY_app_translations_py_pgettext(const char *msgctxt, const char *msgid) RET_ARG(msgid)

/* intern/dualcon */

void *dualcon(const DualConInput *input_mesh,
              /* callbacks for output */
              DualConAllocOutput alloc_output,
              DualConAddVert add_vert,
              DualConAddQuad add_quad,

              DualConFlags flags,
              DualConMode mode,
              float threshold,
              float hermite_num,
              float scale,
              int depth) RET_ZERO

/* intern/cycles */
struct CCLDeviceInfo;
struct CCLDeviceInfo *CCL_compute_device_list(int opencl) RET_NULL

/* compositor */
void COM_execute(RenderData *rd, Scene *scene, bNodeTree *editingtree, int rendering,
                 const ColorManagedViewSettings *viewSettings, const ColorManagedDisplaySettings *displaySettings) RET_NONE

#endif // WITH_GAMEENGINE
