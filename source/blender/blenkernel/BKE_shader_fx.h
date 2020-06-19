/*
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
 */
#ifndef __BKE_SHADER_FX_H__
#define __BKE_SHADER_FX_H__

/** \file
 * \ingroup bke
 */

#include "BKE_customdata.h"
#include "BLI_compiler_attrs.h"
#include "DNA_shader_fx_types.h" /* needed for all enum typdefs */

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct ID;
struct ListBase;
struct ModifierUpdateDepsgraphContext;
struct Object;
struct ShaderFxData;

#define SHADER_FX_ACTIVE(_fx, _is_render) \
  ((((_fx)->mode & eShaderFxMode_Realtime) && (_is_render == false)) || \
   (((_fx)->mode & eShaderFxMode_Render) && (_is_render == true)))
#define SHADER_FX_EDIT(_fx, _is_edit) ((((_fx)->mode & eShaderFxMode_Editmode) == 0) && (_is_edit))

typedef enum {
  /* Should not be used, only for None type */
  eShaderFxType_NoneType,

  /* grease pencil effects */
  eShaderFxType_GpencilType,
} ShaderFxTypeType;

typedef enum {
  eShaderFxTypeFlag_SupportsEditmode = (1 << 0),

  /* For effects that support editmode this determines if the
   * effect should be enabled by default in editmode.
   */
  eShaderFxTypeFlag_EnableInEditmode = (1 << 2),

  /* max one per type */
  eShaderFxTypeFlag_Single = (1 << 4),

  /* can't be added manually by user */
  eShaderFxTypeFlag_NoUserAdd = (1 << 5),
} ShaderFxTypeFlag;

/* IMPORTANT! Keep ObjectWalkFunc and IDWalkFunc signatures compatible. */
typedef void (*ShaderFxObjectWalkFunc)(void *userData,
                                       struct Object *ob,
                                       struct Object **obpoin,
                                       int cb_flag);
typedef void (*ShaderFxIDWalkFunc)(void *userData,
                                   struct Object *ob,
                                   struct ID **idpoin,
                                   int cb_flag);
typedef void (*ShaderFxTexWalkFunc)(void *userData,
                                    struct Object *ob,
                                    struct ShaderFxData *fx,
                                    const char *propname);

typedef struct ShaderFxTypeInfo {
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
  void (*copyData)(const struct ShaderFxData *fx, struct ShaderFxData *target);

  /* Initialize new instance data for this effect type, this function
   * should set effect variables to their default values.
   *
   * This function is optional.
   */
  void (*initData)(struct ShaderFxData *fx);

  /* Free internal effect data variables, this function should
   * not free the fx variable itself.
   *
   * This function is optional.
   */
  void (*freeData)(struct ShaderFxData *fx);

  /* Return a boolean value indicating if this effect is able to be
   * calculated based on the effect data. This is *not* regarding the
   * fx->flag, that is tested by the system, this is just if the data
   * validates (for example, a lattice will return false if the lattice
   * object is not defined).
   *
   * This function is optional (assumes never disabled if not present).
   */
  bool (*isDisabled)(struct ShaderFxData *fx, int userRenderParams);

  /* Add the appropriate relations to the dependency graph.
   *
   * This function is optional.
   */
  void (*updateDepsgraph)(struct ShaderFxData *fx,
                          const struct ModifierUpdateDepsgraphContext *ctx);

  /* Should return true if the effect needs to be recalculated on time
   * changes.
   *
   * This function is optional (assumes false if not present).
   */
  bool (*dependsOnTime)(struct ShaderFxData *fx);

  /* Should call the given walk function on with a pointer to each Object
   * pointer that the effect data stores. This is used for linking on file
   * load and for unlinking objects or forwarding object references.
   *
   * This function is optional.
   */
  void (*foreachObjectLink)(struct ShaderFxData *fx,
                            struct Object *ob,
                            ShaderFxObjectWalkFunc walk,
                            void *userData);

  /* Should call the given walk function with a pointer to each ID
   * pointer (i.e. each data-block pointer) that the effect data
   * stores. This is used for linking on file load and for
   * unlinking data-blocks or forwarding data-block references.
   *
   * This function is optional. If it is not present, foreachObjectLink
   * will be used.
   */
  void (*foreachIDLink)(struct ShaderFxData *fx,
                        struct Object *ob,
                        ShaderFxIDWalkFunc walk,
                        void *userData);

  /* Register the panel types for the effect's UI. */
  void (*panelRegister)(struct ARegionType *region_type);
} ShaderFxTypeInfo;

#define SHADERFX_TYPE_PANEL_PREFIX "FX_PT_"

/* Initialize  global data (type info and some common global storages). */
void BKE_shaderfx_init(void);

void BKE_shaderfxType_panel_id(ShaderFxType type, char *panel_id);
const ShaderFxTypeInfo *BKE_shaderfx_get_info(ShaderFxType type);
struct ShaderFxData *BKE_shaderfx_new(int type);
void BKE_shaderfx_free_ex(struct ShaderFxData *fx, const int flag);
void BKE_shaderfx_free(struct ShaderFxData *fx);
bool BKE_shaderfx_unique_name(struct ListBase *shaderfx, struct ShaderFxData *fx);
bool BKE_shaderfx_depends_ontime(struct ShaderFxData *fx);
struct ShaderFxData *BKE_shaderfx_findby_type(struct Object *ob, ShaderFxType type);
struct ShaderFxData *BKE_shaderfx_findby_name(struct Object *ob, const char *name);
void BKE_shaderfx_copydata_generic(const struct ShaderFxData *fx_src, struct ShaderFxData *fx_dst);
void BKE_shaderfx_copydata(struct ShaderFxData *fx, struct ShaderFxData *target);
void BKE_shaderfx_copydata_ex(struct ShaderFxData *fx,
                              struct ShaderFxData *target,
                              const int flag);
void BKE_shaderfx_foreach_ID_link(struct Object *ob, ShaderFxIDWalkFunc walk, void *userData);

bool BKE_shaderfx_has_gpencil(struct Object *ob);

#ifdef __cplusplus
}
#endif

#endif /* __BKE_SHADER_FX_H__ */
