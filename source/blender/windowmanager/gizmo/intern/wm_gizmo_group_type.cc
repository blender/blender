/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#include <cstdio>

#include "BLI_vector_set.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_types.hh"

/* Own includes. */
#include "wm_gizmo_intern.hh"
#include "wm_gizmo_wmapi.hh"

/* -------------------------------------------------------------------- */
/** \name GizmoGroup Type Append
 *
 * \note This follows conventions from #WM_operatortype_find #WM_operatortype_append & friends.
 * \{ */

using blender::StringRef;

static auto &get_gizmo_group_type_map()
{
  struct IDNameGetter {
    StringRef operator()(const wmGizmoGroupType *value) const
    {
      return StringRef(value->idname);
    }
  };
  static blender::CustomIDVectorSet<wmGizmoGroupType *, IDNameGetter> map;
  return map;
}

wmGizmoGroupType *WM_gizmogrouptype_find(const StringRef idname, bool quiet)
{
  if (!idname.is_empty()) {
    if (wmGizmoGroupType *const *gzgt = get_gizmo_group_type_map().lookup_key_ptr_as(idname)) {
      return *gzgt;
    }

    if (!quiet) {
      printf("search for unknown gizmo group '%s'\n", std::string(idname).c_str());
    }
  }
  else {
    if (!quiet) {
      printf("search for empty gizmo group\n");
    }
  }

  return nullptr;
}

static wmGizmoGroupType *wm_gizmogrouptype_append__begin()
{
  wmGizmoGroupType *gzgt = MEM_callocN<wmGizmoGroupType>("gizmogrouptype");
  gzgt->srna = RNA_def_struct_ptr(&BLENDER_RNA, "", &RNA_GizmoGroupProperties);
#if 0
  /* Set the default i18n context now, so that opfunc can redefine it if needed! */
  RNA_def_struct_translation_context(ot->srna, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  ot->translation_context = BLT_I18NCONTEXT_OPERATOR_DEFAULT;
#endif
  return gzgt;
}
static void wm_gizmogrouptype_append__end(wmGizmoGroupType *gzgt)
{
  BLI_assert(gzgt->name != nullptr);
  BLI_assert(gzgt->idname != nullptr);

  RNA_def_struct_identifier(&BLENDER_RNA, gzgt->srna, gzgt->idname);

  gzgt->type_update_flag |= WM_GIZMOMAPTYPE_KEYMAP_INIT;

  /* If not set, use default. */
  if (gzgt->setup_keymap == nullptr) {
    if (gzgt->flag & WM_GIZMOGROUPTYPE_SELECT) {
      gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic_select;
    }
    else {
      gzgt->setup_keymap = WM_gizmogroup_setup_keymap_generic;
    }
  }

  get_gizmo_group_type_map().add(gzgt);
}

wmGizmoGroupType *WM_gizmogrouptype_append(void (*wtfunc)(wmGizmoGroupType *))
{
  wmGizmoGroupType *gzgt = wm_gizmogrouptype_append__begin();
  wtfunc(gzgt);
  wm_gizmogrouptype_append__end(gzgt);
  return gzgt;
}

wmGizmoGroupType *WM_gizmogrouptype_append_ptr(void (*wtfunc)(wmGizmoGroupType *, void *),
                                               void *userdata)
{
  wmGizmoGroupType *gzgt = wm_gizmogrouptype_append__begin();
  wtfunc(gzgt, userdata);
  wm_gizmogrouptype_append__end(gzgt);
  return gzgt;
}

wmGizmoGroupTypeRef *WM_gizmogrouptype_append_and_link(wmGizmoMapType *gzmap_type,
                                                       void (*wtfunc)(wmGizmoGroupType *))
{
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_append(wtfunc);

  gzgt->gzmap_params.spaceid = gzmap_type->spaceid;
  gzgt->gzmap_params.regionid = gzmap_type->regionid;

  return WM_gizmomaptype_group_link_ptr(gzmap_type, gzgt);
}

/**
 * Free but don't remove from the global list.
 */
static void gizmogrouptype_free(wmGizmoGroupType *gzgt)
{
  /* Python gizmo group, allocates its own string. */
  if (gzgt->rna_ext.srna) {
    MEM_freeN(gzgt->idname);
  }

  MEM_freeN(gzgt);
}

void WM_gizmo_group_type_free_ptr(wmGizmoGroupType *gzgt)
{
  BLI_assert(gzgt == WM_gizmogrouptype_find(gzgt->idname, false));

  get_gizmo_group_type_map().remove(gzgt);

  gizmogrouptype_free(gzgt);

  /* XXX, TODO: update the world! */
}

bool WM_gizmo_group_type_free(const StringRef idname)
{
  wmGizmoGroupType *const *gzgt = get_gizmo_group_type_map().lookup_key_ptr_as(idname);
  if (gzgt == nullptr) {
    return false;
  }

  WM_gizmo_group_type_free_ptr(*gzgt);

  return true;
}

void wm_gizmogrouptype_free()
{
  for (wmGizmoGroupType *gzgt : get_gizmo_group_type_map()) {
    gizmogrouptype_free(gzgt);
  }
  get_gizmo_group_type_map().clear();
}

void wm_gizmogrouptype_init()
{
  /* Reserve size is set based on blender default setup. */
  get_gizmo_group_type_map().reserve(128);
}

/** \} */
