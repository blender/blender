/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_attrs.h"
#include "DNA_listBase.h"

#include "DNA_shader_fx_types.h" /* Needed for all enum type definitions. */

#include "BKE_lib_query.hh" /* For LibraryForeachIDCallbackFlag enum. */

namespace blender {

struct ARegionType;
struct BlendDataReader;
struct BlendWriter;
struct ID;
struct IDTypeForeachColorFunctionCallback;
struct ModifierUpdateDepsgraphContext;
struct Object;
struct ShaderFxData;

#define SHADER_FX_ACTIVE(_fx, _is_render) \
  ((((_fx)->mode & eShaderFxMode_Realtime) && (_is_render == false)) || \
   (((_fx)->mode & eShaderFxMode_Render) && (_is_render == true)))
#define SHADER_FX_EDIT(_fx, _is_edit) ((((_fx)->mode & eShaderFxMode_Editmode) == 0) && (_is_edit))

enum ShaderFxTypeType {
  /* Should not be used, only for None type */
  eShaderFxType_NoneType,

  /* grease pencil effects */
  eShaderFxType_GpencilType,
};

enum ShaderFxTypeFlag {
  eShaderFxTypeFlag_SupportsEditmode = (1 << 0),

  /* For effects that support editmode this determines if the
   * effect should be enabled by default in editmode.
   */
  eShaderFxTypeFlag_EnableInEditmode = (1 << 2),

  /* max one per type */
  eShaderFxTypeFlag_Single = (1 << 4),

  /* can't be added manually by user */
  eShaderFxTypeFlag_NoUserAdd = (1 << 5),
};

typedef void (*ShaderFxIDWalkFunc)(void *user_data,
                                   Object *ob,
                                   ID **idpoin,
                                   LibraryForeachIDCallbackFlag cb_flag);
typedef void (*ShaderFxTexWalkFunc)(void *user_data,
                                    Object *ob,
                                    ShaderFxData *fx,
                                    const char *propname);

struct ShaderFxTypeInfo {
  /* The user visible name for this effect */
  char name[32];

  /* The DNA struct name for the effect data type, used to
   * write the DNA data out.
   */
  char struct_name[32];

  /* The size of the effect data type, used by allocation. */
  int struct_size;

  ShaderFxTypeType type;
  ShaderFxTypeFlag flags;

  /* Copy instance data for this effect type. Should copy all user
   * level settings to the target effect.
   */
  void (*copy_data)(const ShaderFxData *fx, ShaderFxData *target);

  /* Initialize new instance data for this effect type, this function
   * should set effect variables to their default values.
   *
   * This function is optional.
   */
  void (*init_data)(ShaderFxData *fx);

  /* Free internal effect data variables, this function should
   * not free the fx variable itself.
   *
   * This function is optional.
   */
  void (*free_data)(ShaderFxData *fx);

  /* Return a boolean value indicating if this effect is able to be
   * calculated based on the effect data. This is *not* regarding the
   * fx->flag, that is tested by the system, this is just if the data
   * validates (for example, a lattice will return false if the lattice
   * object is not defined).
   *
   * This function is optional (assumes never disabled if not present).
   */
  bool (*is_disabled)(ShaderFxData *fx, bool use_render_params);

  /* Add the appropriate relations to the dependency graph.
   *
   * This function is optional.
   */
  void (*update_depsgraph)(ShaderFxData *fx, const ModifierUpdateDepsgraphContext *ctx);

  /* Should return true if the effect needs to be recalculated on time
   * changes.
   *
   * This function is optional (assumes false if not present).
   */
  bool (*depends_on_time)(ShaderFxData *fx);

  /* Should call the given walk function with a pointer to each ID
   * pointer (i.e. each data-block pointer) that the effect data
   * stores. This is used for linking on file load and for
   * unlinking data-blocks or forwarding data-block references.
   *
   * This function is optional.
   */
  void (*foreach_ID_link)(ShaderFxData *fx, Object *ob, ShaderFxIDWalkFunc walk, void *user_data);

  /* Should iterate over every working space color. */
  void (*foreach_working_space_color)(ShaderFxData *fx,
                                      const IDTypeForeachColorFunctionCallback &func);

  /* Register the panel types for the effect's UI. */
  void (*panel_register)(ARegionType *region_type);
};

#define SHADERFX_TYPE_PANEL_PREFIX "FX_PT_"

/**
 * Initialize global data (type info and some common global storage).
 */
void BKE_shaderfx_init(void);

/**
 * Get an effect's panel type, which was defined in the #panel_register callback.
 *
 * \note ShaderFx panel types are assumed to be named with the struct name field concatenated to
 * the defined prefix.
 */
void BKE_shaderfxType_panel_id(ShaderFxType type, char *r_idname);
void BKE_shaderfx_panel_expand(ShaderFxData *fx);
const ShaderFxTypeInfo *BKE_shaderfx_get_info(ShaderFxType type);
ShaderFxData *BKE_shaderfx_new(int type);
void BKE_shaderfx_free_ex(ShaderFxData *fx, int flag);
void BKE_shaderfx_free(ShaderFxData *fx);
/**
 * Check unique name.
 */
void BKE_shaderfx_unique_name(ListBaseT<ShaderFxData> *shaders, ShaderFxData *fx);
bool BKE_shaderfx_depends_ontime(ShaderFxData *fx);
/**
 * Check whether given shaderfx is not local (i.e. from linked data) when the object is a library
 * override.
 *
 * \param shaderfx: May be NULL, in which case we consider it as a non-local shaderfx case.
 */
bool BKE_shaderfx_is_nonlocal_in_liboverride(const Object *ob, const ShaderFxData *shaderfx);
ShaderFxData *BKE_shaderfx_findby_type(Object *ob, ShaderFxType type);
ShaderFxData *BKE_shaderfx_findby_name(Object *ob, const char *name);
void BKE_shaderfx_copydata_generic(const ShaderFxData *fx_src, ShaderFxData *fx_dst);
void BKE_shaderfx_copydata(ShaderFxData *fx, ShaderFxData *target);
void BKE_shaderfx_copydata_ex(ShaderFxData *fx, ShaderFxData *target, int flag);
void BKE_shaderfx_copy(ListBaseT<ShaderFxData> *dst, const ListBaseT<ShaderFxData> *src);
void BKE_shaderfx_foreach_ID_link(Object *ob, ShaderFxIDWalkFunc walk, void *user_data);

/**
 * Check if exist grease pencil effects.
 */
bool BKE_shaderfx_has_gpencil(const Object *ob);

void BKE_shaderfx_blend_write(BlendWriter *writer, ListBaseT<ShaderFxData> *fxbase);
void BKE_shaderfx_blend_read_data(BlendDataReader *reader,
                                  ListBaseT<ShaderFxData> *lb,
                                  Object *ob);

}  // namespace blender
