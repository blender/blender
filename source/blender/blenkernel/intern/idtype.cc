/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <array>
#include <cstring>

#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_collection_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"

#include "BKE_node.hh"

#include "BKE_idtype.hh"

// static CLG_LogRef LOG = {"lib.idtype"};

uint BKE_idtype_cache_key_hash(const void *key_v)
{
  const IDCacheKey *key = static_cast<const IDCacheKey *>(key_v);
  size_t hash = BLI_ghashutil_uinthash(key->id_session_uid);
  hash = BLI_ghashutil_combine_hash(hash, BLI_ghashutil_uinthash(uint(key->identifier)));
  return uint(hash);
}

bool BKE_idtype_cache_key_cmp(const void *key_a_v, const void *key_b_v)
{
  const IDCacheKey *key_a = static_cast<const IDCacheKey *>(key_a_v);
  const IDCacheKey *key_b = static_cast<const IDCacheKey *>(key_b_v);
  return (key_a->id_session_uid != key_b->id_session_uid) ||
         (key_a->identifier != key_b->identifier);
}

static std::array<IDTypeInfo *, INDEX_ID_MAX> id_types;

static void id_type_init()
{
  int init_types_num = 0;

#define INIT_TYPE(_id_code) \
  { \
    BLI_assert(IDType_##_id_code.main_listbase_index == INDEX_##_id_code); \
    id_types[INDEX_##_id_code] = &IDType_##_id_code; \
    init_types_num++; \
  } \
  (void)0

  INIT_TYPE(ID_SCE);
  INIT_TYPE(ID_LI);
  INIT_TYPE(ID_OB);
  INIT_TYPE(ID_ME);
  INIT_TYPE(ID_CU_LEGACY);
  INIT_TYPE(ID_MB);
  INIT_TYPE(ID_MA);
  INIT_TYPE(ID_TE);
  INIT_TYPE(ID_IM);
  INIT_TYPE(ID_LT);
  INIT_TYPE(ID_LA);
  INIT_TYPE(ID_CA);
  INIT_TYPE(ID_KE);
  INIT_TYPE(ID_WO);
  INIT_TYPE(ID_SCR);
  INIT_TYPE(ID_VF);
  INIT_TYPE(ID_TXT);
  INIT_TYPE(ID_SPK);
  INIT_TYPE(ID_SO);
  INIT_TYPE(ID_GR);
  INIT_TYPE(ID_AR);
  INIT_TYPE(ID_AC);
  INIT_TYPE(ID_NT);
  INIT_TYPE(ID_BR);
  INIT_TYPE(ID_PA);
  INIT_TYPE(ID_GD_LEGACY);
  INIT_TYPE(ID_WM);
  INIT_TYPE(ID_MC);
  INIT_TYPE(ID_MSK);
  INIT_TYPE(ID_LS);
  INIT_TYPE(ID_PAL);
  INIT_TYPE(ID_PC);
  INIT_TYPE(ID_CF);
  INIT_TYPE(ID_WS);
  INIT_TYPE(ID_LP);
  INIT_TYPE(ID_CV);
  INIT_TYPE(ID_PT);
  INIT_TYPE(ID_VO);
  INIT_TYPE(ID_GP);

  /* Special case. */
  BLI_assert(IDType_ID_LINK_PLACEHOLDER.main_listbase_index == INDEX_ID_NULL);
  id_types[INDEX_ID_NULL] = &IDType_ID_LINK_PLACEHOLDER;
  init_types_num++;

  BLI_assert_msg(init_types_num == INDEX_ID_MAX, "Some IDTypeInfo initialization is missing");
  UNUSED_VARS_NDEBUG(init_types_num);

  { /* Inspect which ID types can be animated, so that IDType_ID_AC.dependencies_id_types can be
     * set to include those. The runtime ID* cache of #animrig::Slot will point to any
     * ID that is animated by it, and thus can point to any animatable ID type. */
    IDType_ID_AC.dependencies_id_types = 0;
    for (const IDTypeInfo *id_type : id_types) {
      const bool is_animatable = (id_type->flags & IDTYPE_FLAGS_NO_ANIMDATA) == 0;
      if (is_animatable) {
        IDType_ID_AC.dependencies_id_types |= id_type->id_filter;
      }
    }
  }

#undef INIT_TYPE
}

void BKE_idtype_init()
{
  /* Initialize data-block types. */
  id_type_init();
}

const IDTypeInfo *BKE_idtype_get_info_from_idtype_index(const int idtype_index)
{
  if (idtype_index >= 0 && idtype_index < int(id_types.size())) {
    const IDTypeInfo *id_type = id_types[size_t(idtype_index)];
    if (id_type && id_type->name[0] != '\0') {
      BLI_assert_msg(BKE_idtype_idcode_to_index(id_type->id_code) == idtype_index,
                     "Critical inconsistency in ID type information");
      return id_type;
    }
  }

  return nullptr;
}

const IDTypeInfo *BKE_idtype_get_info_from_idcode(const short id_code)
{
  return BKE_idtype_get_info_from_idtype_index(BKE_idtype_idcode_to_index(id_code));
}

const IDTypeInfo *BKE_idtype_get_info_from_id(const ID *id)
{
  return BKE_idtype_get_info_from_idcode(GS(id->name));
}

static const IDTypeInfo *idtype_get_info_from_name(const char *idtype_name)
{
  for (const IDTypeInfo *id_type : id_types) {
    if (id_type && STREQ(idtype_name, id_type->name)) {
      return id_type;
    }
  }

  return nullptr;
}

/* Various helpers/wrappers around #IDTypeInfo structure. */

const char *BKE_idtype_idcode_to_name(const short idcode)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(idcode);
  BLI_assert(id_type != nullptr);
  return id_type != nullptr ? id_type->name : nullptr;
}

const char *BKE_idtype_idcode_to_name_plural(const short idcode)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(idcode);
  BLI_assert(id_type != nullptr);
  return id_type != nullptr ? id_type->name_plural : nullptr;
}

const char *BKE_idtype_idcode_to_translation_context(const short idcode)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(idcode);
  BLI_assert(id_type != nullptr);
  return id_type != nullptr ? id_type->translation_context : BLT_I18NCONTEXT_DEFAULT;
}

short BKE_idtype_idcode_from_name(const char *idtype_name)
{
  const IDTypeInfo *id_type = idtype_get_info_from_name(idtype_name);
  BLI_assert(id_type);
  return id_type != nullptr ? id_type->id_code : 0;
}

bool BKE_idtype_idcode_is_valid(const short idcode)
{
  return BKE_idtype_get_info_from_idcode(idcode) != nullptr ? true : false;
}

bool BKE_idtype_idcode_is_linkable(const short idcode)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(idcode);
  BLI_assert(id_type != nullptr);
  return id_type != nullptr ? (id_type->flags & IDTYPE_FLAGS_NO_LIBLINKING) == 0 : false;
}

bool BKE_idtype_idcode_is_only_appendable(const short idcode)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(idcode);
  BLI_assert(id_type != nullptr);
  if (id_type != nullptr && (id_type->flags & IDTYPE_FLAGS_ONLY_APPEND) != 0) {
    /* Only appendable ID types should also always be linkable. */
    BLI_assert((id_type->flags & IDTYPE_FLAGS_NO_LIBLINKING) == 0);
    return true;
  }
  return false;
}

bool BKE_idtype_idcode_append_is_reusable(const short idcode)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idcode(idcode);
  BLI_assert(id_type != nullptr);
  if (id_type != nullptr && (id_type->flags & IDTYPE_FLAGS_APPEND_IS_REUSABLE) != 0) {
    /* All appendable ID types should also always be linkable. */
    BLI_assert((id_type->flags & IDTYPE_FLAGS_NO_LIBLINKING) == 0);
    return true;
  }
  return false;
}

int BKE_idtype_idcode_to_index(const short idcode)
{
#define CASE_IDINDEX(_id) \
  case ID_##_id: \
    return INDEX_ID_##_id

  switch ((ID_Type)idcode) {
    CASE_IDINDEX(AC);
    CASE_IDINDEX(AR);
    CASE_IDINDEX(BR);
    CASE_IDINDEX(CA);
    CASE_IDINDEX(CF);
    CASE_IDINDEX(CU_LEGACY);
    CASE_IDINDEX(GD_LEGACY);
    CASE_IDINDEX(GP);
    CASE_IDINDEX(GR);
    CASE_IDINDEX(CV);
    CASE_IDINDEX(IM);
    CASE_IDINDEX(KE);
    CASE_IDINDEX(LA);
    CASE_IDINDEX(LI);
    CASE_IDINDEX(LS);
    CASE_IDINDEX(LT);
    CASE_IDINDEX(MA);
    CASE_IDINDEX(MB);
    CASE_IDINDEX(MC);
    CASE_IDINDEX(ME);
    CASE_IDINDEX(MSK);
    CASE_IDINDEX(NT);
    CASE_IDINDEX(OB);
    CASE_IDINDEX(PA);
    CASE_IDINDEX(PAL);
    CASE_IDINDEX(PC);
    CASE_IDINDEX(PT);
    CASE_IDINDEX(LP);
    CASE_IDINDEX(SCE);
    CASE_IDINDEX(SCR);
    CASE_IDINDEX(SPK);
    CASE_IDINDEX(SO);
    CASE_IDINDEX(TE);
    CASE_IDINDEX(TXT);
    CASE_IDINDEX(VF);
    CASE_IDINDEX(VO);
    CASE_IDINDEX(WM);
    CASE_IDINDEX(WO);
    CASE_IDINDEX(WS);
  }

  /* Special naughty boy... */
  if (idcode == ID_LINK_PLACEHOLDER) {
    return INDEX_ID_NULL;
  }

  return -1;

#undef CASE_IDINDEX
}

int BKE_idtype_idfilter_to_index(const uint64_t id_filter)
{
#define CASE_IDINDEX(_id) \
  case FILTER_ID_##_id: \
    return INDEX_ID_##_id

  switch (id_filter) {
    CASE_IDINDEX(AC);
    CASE_IDINDEX(AR);
    CASE_IDINDEX(BR);
    CASE_IDINDEX(CA);
    CASE_IDINDEX(CF);
    CASE_IDINDEX(CU_LEGACY);
    CASE_IDINDEX(GD_LEGACY);
    CASE_IDINDEX(GP);
    CASE_IDINDEX(GR);
    CASE_IDINDEX(CV);
    CASE_IDINDEX(IM);
    CASE_IDINDEX(KE);
    CASE_IDINDEX(LA);
    CASE_IDINDEX(LI);
    CASE_IDINDEX(LS);
    CASE_IDINDEX(LT);
    CASE_IDINDEX(MA);
    CASE_IDINDEX(MB);
    CASE_IDINDEX(MC);
    CASE_IDINDEX(ME);
    CASE_IDINDEX(MSK);
    CASE_IDINDEX(NT);
    CASE_IDINDEX(OB);
    CASE_IDINDEX(PA);
    CASE_IDINDEX(PAL);
    CASE_IDINDEX(PC);
    CASE_IDINDEX(PT);
    CASE_IDINDEX(LP);
    CASE_IDINDEX(SCE);
    CASE_IDINDEX(SCR);
    CASE_IDINDEX(SPK);
    CASE_IDINDEX(SO);
    CASE_IDINDEX(TE);
    CASE_IDINDEX(TXT);
    CASE_IDINDEX(VF);
    CASE_IDINDEX(VO);
    CASE_IDINDEX(WM);
    CASE_IDINDEX(WO);
    CASE_IDINDEX(WS);
  }

  /* No handling of #ID_LINK_PLACEHOLDER or #INDEX_ID_NULL here. */

  return -1;

#undef CASE_IDINDEX
}

short BKE_idtype_index_to_idcode(const int idtype_index)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idtype_index(idtype_index);
  if (id_type) {
    return id_type->id_code;
  }

  BLI_assert_unreachable();
  return -1;
}

uint64_t BKE_idtype_index_to_idfilter(const int idtype_index)
{
  const IDTypeInfo *id_type = BKE_idtype_get_info_from_idtype_index(idtype_index);
  if (id_type) {
    return id_type->id_filter;
  }

  BLI_assert_unreachable();
  return 0;
}

uint64_t BKE_idtype_idcode_to_idfilter(const short idcode)
{
  return BKE_idtype_index_to_idfilter(BKE_idtype_idcode_to_index(idcode));
}

short BKE_idtype_idfilter_to_idcode(const uint64_t idfilter)
{
  return BKE_idtype_index_to_idcode(BKE_idtype_idfilter_to_index(idfilter));
}

short BKE_idtype_idcode_iter_step(int *idtype_index)
{
  return (*idtype_index < int(id_types.size())) ? BKE_idtype_index_to_idcode((*idtype_index)++) :
                                                  0;
}

void BKE_idtype_id_foreach_cache(ID *id,
                                 IDTypeForeachCacheFunctionCallback function_callback,
                                 void *user_data)
{
  const IDTypeInfo *type_info = BKE_idtype_get_info_from_id(id);
  if (type_info->foreach_cache != nullptr) {
    type_info->foreach_cache(id, function_callback, user_data);
  }

  /* Handle 'private IDs'. */
  bNodeTree *nodetree = blender::bke::node_tree_from_id(id);
  if (nodetree != nullptr) {
    type_info = BKE_idtype_get_info_from_id(&nodetree->id);
    if (type_info == nullptr) {
      /* Very old .blend file seem to have empty names for their embedded node trees, see
       * `blo_do_versions_250()`. Assume those are node-trees then. */
      type_info = BKE_idtype_get_info_from_idcode(ID_NT);
    }
    if (type_info->foreach_cache != nullptr) {
      type_info->foreach_cache(&nodetree->id, function_callback, user_data);
    }
  }

  if (GS(id->name) == ID_SCE) {
    Scene *scene = (Scene *)id;
    if (scene->master_collection != nullptr) {
      type_info = BKE_idtype_get_info_from_id(&scene->master_collection->id);
      if (type_info->foreach_cache != nullptr) {
        type_info->foreach_cache(&scene->master_collection->id, function_callback, user_data);
      }
    }
  }
}
