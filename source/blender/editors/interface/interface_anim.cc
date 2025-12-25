/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DNA_anim_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_fcurve_driver.h"
#include "BKE_global.hh"
#include "BKE_nla.hh"

#include "DEG_depsgraph_build.hh"

#include "ED_keyframing.hh"

#include "ANIM_fcurve.hh"
#include "ANIM_keyframing.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"

namespace blender::ui {

static FCurve *ui_but_get_fcurve(
    Button *but, AnimData **adt, bAction **action, bool *r_driven, bool *r_special)
{
  /* for entire array buttons we check the first component, it's not perfect
   * but works well enough in typical cases */
  const int rnaindex = (but->rnaindex == -1) ? 0 : but->rnaindex;

  return BKE_fcurve_find_by_rna_context_ui(static_cast<bContext *>(but->block->evil_C),
                                           &but->rnapoin,
                                           but->rnaprop,
                                           rnaindex,
                                           adt,
                                           action,
                                           r_driven,
                                           r_special);
}

void button_anim_flag(Button *but, const AnimationEvalContext *anim_eval_context)
{
  /* Clear the flags that this function might set. */
  but->flag &= ~(BUT_ANIMATED | BUT_ANIMATED_KEY | BUT_DRIVEN);
  but->drawflag &= ~BUT_ANIMATED_CHANGED;

  /* NOTE: "special" is reserved for special F-Curves stored on the animation data
   *        itself (which are used to animate properties of the animation data).
   *        We count those as "animated" too for now
   */
  AnimData *adt;
  bAction *act;
  bool driven;
  bool special;
  FCurve *fcu = ui_but_get_fcurve(but, &adt, &act, &driven, &special);

  if (!fcu) {
    return;
  }
  if (driven) {
    but->flag |= BUT_DRIVEN;
    return;
  }

  /* Empty curves are ignored by the animation evaluation system. */
  if (BKE_fcurve_is_empty(fcu)) {
    return;
  }

  but->flag |= BUT_ANIMATED;

  /* #41525 - When the active action is a NLA strip being edited,
   * we need to correct the frame number to "look inside" the
   * remapped action
   */
  float cfra = anim_eval_context->eval_time;
  if (adt) {
    cfra = BKE_nla_tweakedit_remap(adt, cfra, NLATIME_CONVERT_UNMAP);
  }

  if (animrig::fcurve_frame_has_keyframe(fcu, cfra)) {
    but->flag |= BUT_ANIMATED_KEY;
  }

  /* This feature is not implemented at all for the NLA. However, if the NLA just consists of
   * stashed (i.e. deactivated) Actions, it doesn't do anything, and we can treat it as
   * non-existent here. Note that this is mostly to play nice with stashed Actions, and doesn't
   * fully look at all the track & strip flags. */
  if (adt) {
    for (NlaTrack &nla_track : adt->nla_tracks) {
      if (!(nla_track.flag & NLATRACK_MUTED)) {
        /* Found a non-muted track, so this NLA is not purely for stashing Actions. */
        return;
      }
    }
  }

  const AnimationEvalContext remapped_context = BKE_animsys_eval_context_construct_at(
      anim_eval_context, cfra);
  if (fcurve_is_changed(but->rnapoin, but->rnaprop, fcu, &remapped_context)) {
    but->drawflag |= BUT_ANIMATED_CHANGED;
  }
}

static Button *ui_but_anim_decorate_find_attached_button(ButtonDecorator *but)
{
  Button *but_iter = nullptr;

  BLI_assert(button_is_decorator(but));
  BLI_assert(but->decorated_rnapoin.data && but->decorated_rnaprop);
  if (but->block->buttons.is_empty()) {
    return nullptr;
  }
  int i = but->block->but_index(but);
  i = i > 0 ? i - 1 : but->block->buttons.size() - 1;
  const int start = i;
  do {
    but_iter = but->block->buttons[i].get();
    if (but_iter != but &&
        button_rna_equals_ex(
            but_iter, &but->decorated_rnapoin, but->decorated_rnaprop, but->decorated_rnaindex))
    {
      return but_iter;
    }
    i = i > 0 ? i - 1 : but->block->buttons.size() - 1;
  } while (i != start);

  return nullptr;
}

void button_anim_decorate_update_from_flag(ButtonDecorator *but)
{
  if (!but->decorated_rnapoin.data || !but->decorated_rnaprop) {
    /* Nothing to do. */
    return;
  }

  const Button *but_anim = ui_but_anim_decorate_find_attached_button(but);

  if (!but_anim) {
    printf("Could not find button with matching property to decorate (%s.%s)\n",
           RNA_struct_identifier(but->decorated_rnapoin.type),
           RNA_property_identifier(but->decorated_rnaprop));
    return;
  }

  const int flag = but_anim->flag;

  if (flag & BUT_DRIVEN) {
    but->icon = ICON_DECORATE_DRIVER;
    but->toggle_keyframe_on_click = false;
  }
  else if (flag & BUT_ANIMATED_KEY) {
    but->icon = ICON_DECORATE_KEYFRAME;
    but->toggle_keyframe_on_click = true;
  }
  else if (flag & BUT_ANIMATED) {
    but->icon = ICON_DECORATE_ANIMATE;
    but->toggle_keyframe_on_click = true;
  }
  else if (flag & BUT_OVERRIDDEN) {
    but->icon = ICON_DECORATE_OVERRIDE;
    but->toggle_keyframe_on_click = false;
  }
  else {
    but->icon = ICON_DECORATE;
    but->toggle_keyframe_on_click = true;
  }

  const int flag_copy = (BUT_DISABLED | BUT_INACTIVE);
  but->flag = (but->flag & ~flag_copy) | (flag & flag_copy);
}

bool button_anim_expression_get(Button *but, char *str, size_t str_maxncpy)
{
  FCurve *fcu;
  ChannelDriver *driver;
  bool driven, special;

  fcu = ui_but_get_fcurve(but, nullptr, nullptr, &driven, &special);

  if (fcu && driven) {
    driver = fcu->driver;

    if (driver && driver->type == DRIVER_TYPE_PYTHON) {
      if (str) {
        BLI_strncpy_utf8(str, driver->expression, str_maxncpy);
      }
      return true;
    }
  }

  return false;
}

bool button_anim_expression_set(Button *but, const char *str)
{
  FCurve *fcu;
  ChannelDriver *driver;
  bool driven, special;

  fcu = ui_but_get_fcurve(but, nullptr, nullptr, &driven, &special);

  if (fcu && driven) {
    driver = fcu->driver;

    if (driver && (driver->type == DRIVER_TYPE_PYTHON)) {
      bContext *C = static_cast<bContext *>(but->block->evil_C);

      STRNCPY_UTF8(driver->expression, str);

      /* tag driver as needing to be recompiled */
      BKE_driver_invalidate_expression(driver, true, false);

      /* clear invalid flags which may prevent this from working */
      driver->flag &= ~DRIVER_FLAG_INVALID;
      fcu->flag &= ~FCURVE_DISABLED;

      /* this notifier should update the Graph Editor and trigger depsgraph refresh? */
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME, nullptr);

      DEG_relations_tag_update(CTX_data_main(C));

      return true;
    }
  }

  return false;
}

bool button_anim_expression_create(Button *but, const char *str)
{
  bContext *C = static_cast<bContext *>(but->block->evil_C);
  ID *id;
  FCurve *fcu;
  bool ok = false;

  /* button must have RNA-pointer to a numeric-capable property */
  if (ELEM(nullptr, but->rnapoin.data, but->rnaprop)) {
    if (G.debug & G_DEBUG) {
      printf("ERROR: create expression failed - button has no RNA info attached\n");
    }
    return false;
  }

  if (RNA_property_array_check(but->rnaprop) != 0) {
    if (but->rnaindex == -1) {
      if (G.debug & G_DEBUG) {
        printf("ERROR: create expression failed - cannot create expression for entire array\n");
      }
      return false;
    }
  }

  /* Make sure we have animation-data for this. */
  /* FIXME: until materials can be handled by depsgraph,
   * don't allow drivers to be created for them. */
  id = but->rnapoin.owner_id;
  if ((id == nullptr) || (GS(id->name) == ID_MA) || (GS(id->name) == ID_TE)) {
    if (G.debug & G_DEBUG) {
      printf("ERROR: create expression failed - invalid data-block for adding drivers (%p)\n", id);
    }
    return false;
  }

  /* get path */
  const std::optional<std::string> path = RNA_path_from_ID_to_property(&but->rnapoin,
                                                                       but->rnaprop);
  if (!path) {
    return false;
  }

  /* create driver */
  fcu = verify_driver_fcurve(id, path->c_str(), but->rnaindex, DRIVER_FCURVE_KEYFRAMES);
  if (fcu) {
    ChannelDriver *driver = fcu->driver;

    if (driver) {
      /* set type of driver */
      driver->type = DRIVER_TYPE_PYTHON;

      /* set the expression */
      /* TODO: need some way of identifying variables used */
      STRNCPY_UTF8(driver->expression, str);

      /* updates */
      BKE_driver_invalidate_expression(driver, true, false);
      DEG_relations_tag_update(CTX_data_main(C));
      WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME, nullptr);
      ok = true;
    }
  }

  return ok;
}

void button_anim_autokey(bContext *C, Button *but, Scene *scene, float cfra)
{
  animrig::autokeyframe_property(C, scene, &but->rnapoin, but->rnaprop, but->rnaindex, cfra, true);
}

void button_anim_copy_driver(bContext *C)
{
  /* this operator calls context_active_but_prop_get */
  WM_operator_name_call(
      C, "ANIM_OT_copy_driver_button", wm::OpCallContext::InvokeDefault, nullptr, nullptr);
}

void button_anim_paste_driver(bContext *C)
{
  /* this operator calls context_active_but_prop_get */
  WM_operator_name_call(
      C, "ANIM_OT_paste_driver_button", wm::OpCallContext::InvokeDefault, nullptr, nullptr);
}

void button_anim_decorate_cb(bContext *C, void *arg_but, void * /*arg_dummy*/)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  auto *but_decorate = static_cast<ButtonDecorator *>(arg_but);
  if (!but_decorate->toggle_keyframe_on_click) {
    return;
  }

  Button *but_anim = ui_but_anim_decorate_find_attached_button(but_decorate);
  if (!but_anim) {
    return;
  }

  /* While click drag the active button may not be `but_decorate`, instead is the but where the
   * drag started, temporarily override `but_anim` as active. */
  but_anim->flag |= BUT_ACTIVE_OVERRIDE;
  wm->op_undo_depth++;

  if (but_anim->flag & BUT_ANIMATED_KEY) {
    wmOperatorType *ot = WM_operatortype_find("ANIM_OT_keyframe_delete_button", false);
    PointerRNA props_ptr = WM_operator_properties_create_ptr(ot);
    RNA_boolean_set(&props_ptr, "all", but_anim->rnaindex == -1);
    WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &props_ptr, nullptr);
    WM_operator_properties_free(&props_ptr);
  }
  else {
    wmOperatorType *ot = WM_operatortype_find("ANIM_OT_keyframe_insert_button", false);
    PointerRNA props_ptr = WM_operator_properties_create_ptr(ot);
    RNA_boolean_set(&props_ptr, "all", but_anim->rnaindex == -1);
    WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &props_ptr, nullptr);
    WM_operator_properties_free(&props_ptr);
  }

  but_anim->flag &= ~BUT_ACTIVE_OVERRIDE;
  wm->op_undo_depth--;
}

}  // namespace blender::ui
