/* This program is free software; you can redistribute it and/or
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#include <string.h>

#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_screen.h"

#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_object.h"

#include "BLT_translation.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MOD_modifiertypes.h"
#include "MOD_ui_common.h" /* Self include */

static Object *get_modifier_object(const bContext *C)
{
  SpaceProperties *sbuts = CTX_wm_space_properties(C);
  if (sbuts != NULL && (sbuts->pinid != NULL) && GS(sbuts->pinid->name) == ID_OB) {
    return (Object *)sbuts->pinid;
  }
  else {
    return CTX_data_active_object(C);
  }
}

/**
 * Poll function so these modifier panels don't show for other object types with modifiers (only
 * grease pencil currently).
 */
static bool modifier_ui_poll(const bContext *C, PanelType *UNUSED(pt))
{
  Object *ob = get_modifier_object(C);

  return (ob != NULL) && (ob->type != OB_GPENCIL);
}

/* -------------------------------------------------------------------- */
/** \name Panel Drag and Drop, Expansion Saving
 * \{ */

/**
 * Move a modifier to the index it's moved to after a drag and drop.
 */
static void modifier_reorder(bContext *C, Panel *panel, int new_index)
{
  Object *ob = get_modifier_object(C);

  ModifierData *md = BLI_findlink(&ob->modifiers, panel->runtime.list_index);
  PointerRNA props_ptr;
  wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_modifier_move_to_index", false);
  WM_operator_properties_create_ptr(&props_ptr, ot);
  RNA_string_set(&props_ptr, "modifier", md->name);
  RNA_int_set(&props_ptr, "index", new_index);
  WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &props_ptr);
  WM_operator_properties_free(&props_ptr);
}

static short get_modifier_expand_flag(const bContext *C, Panel *panel)
{
  Object *ob = get_modifier_object(C);
  ModifierData *md = BLI_findlink(&ob->modifiers, panel->runtime.list_index);
  return md->ui_expand_flag;
}

static void set_modifier_expand_flag(const bContext *C, Panel *panel, short expand_flag)
{
  Object *ob = get_modifier_object(C);
  ModifierData *md = BLI_findlink(&ob->modifiers, panel->runtime.list_index);
  md->ui_expand_flag = expand_flag;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Panel Layouts
 * \{ */

/**
 * Draw modifier error message.
 */
void modifier_panel_end(uiLayout *layout, PointerRNA *ptr)
{
  ModifierData *md = ptr->data;
  if (md->error) {
    uiLayout *row = uiLayoutRow(layout, false);
    uiItemL(row, IFACE_(md->error), ICON_ERROR);
  }
}

/**
 * Gets RNA pointers for the active object and the panel's modifier data. Also locks
 * the layout if the modifer is from a linked object, and sets the context pointer.
 */
#define ERROR_LIBDATA_MESSAGE TIP_("External library data")
void modifier_panel_get_property_pointers(const bContext *C,
                                          Panel *panel,
                                          PointerRNA *r_ob_ptr,
                                          PointerRNA *r_md_ptr)
{
  Object *ob = get_modifier_object(C);

  ModifierData *md = BLI_findlink(&ob->modifiers, panel->runtime.list_index);

  RNA_pointer_create(&ob->id, &RNA_Modifier, md, r_md_ptr);

  if (r_ob_ptr != NULL) {
    RNA_pointer_create(&ob->id, &RNA_Object, ob, r_ob_ptr);
  }

  uiBlock *block = uiLayoutGetBlock(panel->layout);
  UI_block_lock_set(block, ID_IS_LINKED(ob), ERROR_LIBDATA_MESSAGE);

  uiLayoutSetContextPointer(panel->layout, "modifier", r_md_ptr);
}

/**
 * Helper function for modifier layouts to draw vertex group settings.
 */
void modifier_vgroup_ui(uiLayout *layout,
                        PointerRNA *ptr,
                        PointerRNA *ob_ptr,
                        const char *vgroup_prop,
                        const char *invert_vgroup_prop,
                        const char *text)
{
  bool has_vertex_group = RNA_string_length(ptr, vgroup_prop) != 0;

  uiLayout *row = uiLayoutRow(layout, true);
  uiItemPointerR(row, ptr, vgroup_prop, ob_ptr, "vertex_groups", text, ICON_NONE);
  if (invert_vgroup_prop != NULL) {
    uiLayout *sub = uiLayoutRow(row, true);
    uiLayoutSetActive(sub, has_vertex_group);
    uiLayoutSetPropDecorate(sub, false);
    uiItemR(sub, ptr, invert_vgroup_prop, 0, "", ICON_ARROW_LEFTRIGHT);
  }
}

/**
 * Check whether Modifier is a simulation or not. Used for switching to the
 * physics/particles context tab.
 */
static int modifier_is_simulation(ModifierData *md)
{
  /* Physic Tab */
  if (ELEM(md->type,
           eModifierType_Cloth,
           eModifierType_Collision,
           eModifierType_Fluidsim,
           eModifierType_Fluid,
           eModifierType_Softbody,
           eModifierType_Surface,
           eModifierType_DynamicPaint)) {
    return 1;
  }
  /* Particle Tab */
  else if (md->type == eModifierType_ParticleSystem) {
    return 2;
  }
  else {
    return 0;
  }
}

static bool modifier_can_delete(ModifierData *md)
{
  /* fluid particle modifier can't be deleted here */
  if (md->type == eModifierType_ParticleSystem) {
    short particle_type = ((ParticleSystemModifierData *)md)->psys->part->type;
    if (ELEM(particle_type,
             PART_FLUID,
             PART_FLUID_FLIP,
             PART_FLUID_FOAM,
             PART_FLUID_SPRAY,
             PART_FLUID_BUBBLE,
             PART_FLUID_TRACER,
             PART_FLUID_SPRAYFOAM,
             PART_FLUID_SPRAYBUBBLE,
             PART_FLUID_FOAMBUBBLE,
             PART_FLUID_SPRAYFOAMBUBBLE)) {
      return false;
    }
  }
  return true;
}

static void modifier_ops_extra_draw(bContext *C, uiLayout *layout, void *md_v)
{
  PointerRNA op_ptr;
  uiLayout *row;
  ModifierData *md = (ModifierData *)md_v;

  PointerRNA ptr;
  Object *ob = get_modifier_object(C);
  RNA_pointer_create(&ob->id, &RNA_Modifier, md, &ptr);
  uiLayoutSetContextPointer(layout, "modifier", &ptr);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  uiLayoutSetUnitsX(layout, 4.0f);

  /* Apply. */
  uiItemEnumO(layout,
              "OBJECT_OT_modifier_apply",
              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply"),
              ICON_CHECKMARK,
              "apply_as",
              MODIFIER_APPLY_DATA);

  /* Apply as shapekey. */
  if (BKE_modifier_is_same_topology(md) && !BKE_modifier_is_non_geometrical(md)) {
    uiItemEnumO(layout,
                "OBJECT_OT_modifier_apply",
                CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Apply As Shapekey"),
                ICON_SHAPEKEY_DATA,
                "apply_as",
                MODIFIER_APPLY_SHAPE);
  }

  /* Duplicate. */
  if (!ELEM(md->type,
            eModifierType_Fluidsim,
            eModifierType_Softbody,
            eModifierType_ParticleSystem,
            eModifierType_Cloth,
            eModifierType_Fluid)) {
    uiItemO(layout,
            CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Duplicate"),
            ICON_DUPLICATE,
            "OBJECT_OT_modifier_copy");
  }

  uiItemS(layout);

  /* Move to first. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_modifier_move_to_index",
              IFACE_("Move to First"),
              ICON_TRIA_UP,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", 0);
  if (!md->prev) {
    uiLayoutSetEnabled(row, false);
  }

  /* Move to last. */
  row = uiLayoutColumn(layout, false);
  uiItemFullO(row,
              "OBJECT_OT_modifier_move_to_index",
              IFACE_("Move to Last"),
              ICON_TRIA_DOWN,
              NULL,
              WM_OP_INVOKE_DEFAULT,
              0,
              &op_ptr);
  RNA_int_set(&op_ptr, "index", BLI_listbase_count(&ob->modifiers) - 1);
  if (!md->next) {
    uiLayoutSetEnabled(row, false);
  }
}

static void modifier_panel_header(const bContext *C, Panel *panel)
{
  uiLayout *row, *sub, *name_row;
  uiLayout *layout = panel->layout;

  PointerRNA ptr;
  Object *ob = get_modifier_object(C);

  /* Don't use #modifier_panel_get_property_pointers, we don't want to lock the header. */
  ModifierData *md = BLI_findlink(&ob->modifiers, panel->runtime.list_index);
  RNA_pointer_create(&ob->id, &RNA_Modifier, md, &ptr);
  uiLayoutSetContextPointer(panel->layout, "modifier", &ptr);

  const ModifierTypeInfo *mti = BKE_modifier_get_info(md->type);
  Scene *scene = CTX_data_scene(C);
  int index = panel->runtime.list_index;

  /* Modifier Icon. */
  sub = uiLayoutRow(layout, true);
  if (mti->isDisabled && mti->isDisabled(scene, md, 0)) {
    uiLayoutSetRedAlert(sub, true);
  }
  uiItemL(sub, "", RNA_struct_ui_icon(ptr.type));

  row = uiLayoutRow(layout, true);

  /* Modifier Name.
   * Count how many buttons are added to the header to check if there is enough space. */
  int buttons_number = 0;
  name_row = uiLayoutRow(row, true);

  /* Display mode switching buttons. */
  if (ob->type == OB_MESH) {
    int last_cage_index;
    int cage_index = BKE_modifiers_get_cage_index(scene, ob, &last_cage_index, 0);
    if (BKE_modifier_supports_cage(scene, md) && (index <= last_cage_index)) {
      sub = uiLayoutRow(row, true);
      if (index < cage_index || !BKE_modifier_couldbe_cage(scene, md)) {
        uiLayoutSetActive(sub, false);
      }
      uiItemR(sub, &ptr, "show_on_cage", 0, "", ICON_NONE);
      buttons_number++;
    }
  } /* Tessellation point for curve-typed objects. */
  else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
    if (mti->type != eModifierTypeType_Constructive) {
      /* Constructive modifiers tessellates curve before applying. */
      uiItemR(row, &ptr, "use_apply_on_spline", 0, "", ICON_NONE);
      buttons_number++;
    }
  }
  /* Collision and Surface are always enabled, hide buttons. */
  if (((md->type != eModifierType_Collision) || !(ob->pd && ob->pd->deflect)) &&
      (md->type != eModifierType_Surface)) {
    if (mti->flags & eModifierTypeFlag_SupportsEditmode) {
      sub = uiLayoutRow(row, true);
      uiLayoutSetActive(sub, (md->mode & eModifierMode_Realtime));
      uiItemR(sub, &ptr, "show_in_editmode", 0, "", ICON_NONE);
      buttons_number++;
    }
    uiItemR(row, &ptr, "show_viewport", 0, "", ICON_NONE);
    uiItemR(row, &ptr, "show_render", 0, "", ICON_NONE);
    buttons_number += 2;
  }

  /* Extra operators menu. */
  uiItemMenuF(row, "", ICON_DOWNARROW_HLT, modifier_ops_extra_draw, md);

  /* Delete button. */
  if (modifier_can_delete(md) && !modifier_is_simulation(md)) {
    sub = uiLayoutRow(row, false);
    uiLayoutSetEmboss(sub, UI_EMBOSS_NONE);
    uiItemO(sub, "", ICON_X, "OBJECT_OT_modifier_remove");
    buttons_number++;
  }

  /* Switch context buttons. */
  if (modifier_is_simulation(md) == 1) {
    uiItemStringO(
        row, "", ICON_PROPERTIES, "WM_OT_properties_context_change", "context", "PHYSICS");
    buttons_number++;
  }
  else if (modifier_is_simulation(md) == 2) {
    uiItemStringO(
        row, "", ICON_PROPERTIES, "WM_OT_properties_context_change", "context", "PARTICLES");
    buttons_number++;
  }

  bool display_name = (panel->sizex / UI_UNIT_X - buttons_number > 5) || (panel->sizex == 0);
  if (display_name) {
    uiItemR(name_row, &ptr, "name", 0, "", ICON_NONE);
  }
  else {
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
  }

  /* Extra padding for delete button. */
  uiItemS(layout);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modifier Registration Helpers
 * \{ */

/**
 * Create a panel in the context's region
 */
PanelType *modifier_panel_register(ARegionType *region_type, ModifierType type, PanelDrawFn draw)
{
  /* Get the name for the modifier's panel. */
  char panel_idname[BKE_ST_MAXNAME];
  BKE_modifier_type_panel_id(type, panel_idname);

  PanelType *panel_type = MEM_callocN(sizeof(PanelType), panel_idname);

  strcpy(panel_type->idname, panel_idname);
  strcpy(panel_type->label, "");
  strcpy(panel_type->context, "modifier");
  strcpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = modifier_panel_header;
  panel_type->draw = draw;
  panel_type->poll = modifier_ui_poll;

  /* Give the panel the special flag that says it was built here and corresponds to a
   * modifer rather than a PanelType. */
  panel_type->flag = PNL_LAYOUT_HEADER_EXPAND | PNL_DRAW_BOX | PNL_INSTANCED;
  panel_type->reorder = modifier_reorder;
  panel_type->get_list_data_expand_flag = get_modifier_expand_flag;
  panel_type->set_list_data_expand_flag = set_modifier_expand_flag;

  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/**
 * Add a child panel to the parent.
 *
 * \note To create the panel type's idname, it appends the \a name argument to the \a parent's
 * idname.
 */
PanelType *modifier_subpanel_register(ARegionType *region_type,
                                      const char *name,
                                      const char *label,
                                      PanelDrawFn draw_header,
                                      PanelDrawFn draw,
                                      PanelType *parent)
{
  /* Create the subpanel's ID name. */
  char panel_idname[BKE_ST_MAXNAME];
  strcpy(panel_idname, parent->idname);
  strcat(panel_idname, "_");
  strcat(panel_idname, name);

  PanelType *panel_type = MEM_callocN(sizeof(PanelType), panel_idname);

  strcpy(panel_type->idname, panel_idname);
  strcpy(panel_type->label, label);
  strcpy(panel_type->context, "modifier");
  strcpy(panel_type->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);

  panel_type->draw_header = draw_header;
  panel_type->draw = draw;
  panel_type->poll = modifier_ui_poll;
  panel_type->flag = (PNL_DEFAULT_CLOSED | PNL_DRAW_BOX);

  BLI_assert(parent != NULL);
  strcpy(panel_type->parent_id, parent->idname);
  panel_type->parent = parent;
  BLI_addtail(&parent->children, BLI_genericNodeN(panel_type));
  BLI_addtail(&region_type->paneltypes, panel_type);

  return panel_type;
}

/** \} */
