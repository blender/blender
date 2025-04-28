/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include <cstdio>

#include "BLI_listbase.h"
#include "BLI_vector_set.hh"

#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_types.hh"

#include "ED_screen.hh"

/* Own includes. */
#include "wm_gizmo_intern.hh"
#include "wm_gizmo_wmapi.hh"

/* -------------------------------------------------------------------- */
/** \name Gizmo Type Append
 *
 * \note This follows conventions from #WM_operatortype_find #WM_operatortype_append & friends.
 * \{ */

using blender::StringRef;

static auto &get_gizmo_type_map()
{
  struct IDNameGetter {
    StringRef operator()(const wmGizmoType *value) const
    {
      return StringRef(value->idname);
    }
  };
  static blender::CustomIDVectorSet<wmGizmoType *, IDNameGetter> map;
  return map;
}

const wmGizmoType *WM_gizmotype_find(const StringRef idname, bool quiet)
{
  if (!idname.is_empty()) {
    if (wmGizmoType *const *gzt = get_gizmo_type_map().lookup_key_ptr_as(idname)) {
      return *gzt;
    }

    if (!quiet) {
      printf("search for unknown gizmo '%s'\n", std::string(idname).c_str());
    }
  }
  else {
    if (!quiet) {
      printf("search for empty gizmo\n");
    }
  }

  return nullptr;
}

static wmGizmoType *wm_gizmotype_append__begin()
{
  wmGizmoType *gzt = MEM_callocN<wmGizmoType>("gizmotype");
  gzt->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_GizmoProperties);
#if 0
  /* Set the default i18n context now, so that opfunc can redefine it if needed! */
  RNA_def_struct_translation_context(ot->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  ot->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;
#endif
  return gzt;
}
static void wm_gizmotype_append__end(wmGizmoType *gzt)
{
  BLI_assert(gzt->struct_size >= sizeof(wmGizmo));

  RNA_def_struct_identifier(&BLENDER_RNA, gzt->srna, gzt->idname);

  get_gizmo_type_map().add(gzt);
}

void WM_gizmotype_append(void (*gtfunc)(wmGizmoType *))
{
  wmGizmoType *gzt = wm_gizmotype_append__begin();
  gtfunc(gzt);
  wm_gizmotype_append__end(gzt);
}

void WM_gizmotype_append_ptr(void (*gtfunc)(wmGizmoType *, void *), void *userdata)
{
  wmGizmoType *mt = wm_gizmotype_append__begin();
  gtfunc(mt, userdata);
  wm_gizmotype_append__end(mt);
}

void WM_gizmotype_free_ptr(wmGizmoType *gzt)
{
  /* Python gizmo, allocates its own string. */
  if (gzt->rna_ext.srna) {
    MEM_freeN(gzt->idname);
  }

  BLI_freelistN(&gzt->target_property_defs);
  MEM_freeN(gzt);
}

/**
 * \param C: May be nullptr.
 */
static void gizmotype_unlink(bContext *C, Main *bmain, wmGizmoType *gzt)
{
  /* Free instances. */
  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        ListBase *lb = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
        LISTBASE_FOREACH (ARegion *, region, lb) {
          wmGizmoMap *gzmap = region->runtime->gizmo_map;
          if (gzmap) {
            LISTBASE_FOREACH (wmGizmoGroup *, gzgroup, &gzmap->groups) {
              for (wmGizmo *gz = static_cast<wmGizmo *>(gzgroup->gizmos.first), *gz_next; gz;
                   gz = gz_next)
              {
                gz_next = gz->next;
                BLI_assert(gzgroup->parent_gzmap == gzmap);
                if (gz->type == gzt) {
                  WM_gizmo_unlink(&gzgroup->gizmos, gzgroup->parent_gzmap, gz, C);
                  ED_region_tag_redraw_editor_overlays(region);
                }
              }
            }
          }
        }
      }
    }
  }
}

void WM_gizmotype_remove_ptr(bContext *C, Main *bmain, wmGizmoType *gzt)
{
  BLI_assert(gzt == WM_gizmotype_find(gzt->idname, false));

  get_gizmo_type_map().remove(gzt);

  gizmotype_unlink(C, bmain, gzt);
}

bool WM_gizmotype_remove(bContext *C, Main *bmain, const StringRef idname)
{
  wmGizmoType *const *gzt = get_gizmo_type_map().lookup_key_ptr_as(idname);
  if (gzt == nullptr) {
    return false;
  }

  WM_gizmotype_remove_ptr(C, bmain, *gzt);

  return true;
}

void wm_gizmotype_free()
{
  for (wmGizmoType *gzt : get_gizmo_type_map()) {
    WM_gizmotype_free_ptr(gzt);
  }
  get_gizmo_type_map().clear();
}

void wm_gizmotype_init()
{
  /* Reserve size is set based on blender default setup. */
  get_gizmo_type_map().reserve(128);
}

/** \} */
