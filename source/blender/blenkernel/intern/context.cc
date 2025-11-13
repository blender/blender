/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "DEG_depsgraph.hh"

#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_sound.hh"
#include "BKE_wm_runtime.hh"
#include "BKE_workspace.hh"

#include "RE_engine.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "CLG_log.h"

/* Logging. */
CLG_LOGREF_DECLARE_GLOBAL(BKE_LOG_CONTEXT, "context");

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

using blender::Vector;

static CLG_LogRef LOG = {"context"};

/* struct */

struct bContext {
  int thread;

  /* windowmanager context */
  struct {
    wmWindowManager *manager;
    wmWindow *window;
    WorkSpace *workspace;
    bScreen *screen;
    ScrArea *area;
    ARegion *region;
    ARegion *region_popup;
    wmGizmoGroup *gizmo_group;
    const bContextStore *store;

    /* Operator poll. */
    /**
     * Store the reason the poll function fails (static string, not allocated).
     * For more advanced formatting use `operator_poll_msg_dyn_params`.
     */
    const char *operator_poll_msg;
    /**
     * Store values to dynamically to create the string (called when a tool-tip is shown).
     */
    bContextPollMsgDyn_Params operator_poll_msg_dyn_params;
  } wm;

  /* data context */
  struct {
    Main *main;
    Scene *scene;

    int recursion;
    /** True if python is initialized. */
    bool py_init;
    void *py_context;
    /**
     * If we need to remove members, do so in a copy
     * (keep this to check if the copy needs freeing).
     */
    void *py_context_orig;
    /** True if logging is enabled for context members (can be set programmatically). */
    bool log_access;
  } data;
};

/* context */

bContext *CTX_create()
{
  bContext *C = MEM_callocN<bContext>(__func__);

  return C;
}

bContext *CTX_copy(const bContext *C)
{
  bContext *newC = MEM_callocN<bContext>(__func__);
  *newC = *C;

  memset(&newC->wm.operator_poll_msg_dyn_params, 0, sizeof(newC->wm.operator_poll_msg_dyn_params));

  return newC;
}

void CTX_free(bContext *C)
{
  /* This may contain a dynamically allocated message, free. */
  CTX_wm_operator_poll_msg_clear(C);

  MEM_freeN(C);
}

/* store */

/**
 * Append a new context store to \a contexts, copying entries from the previous one if any.
 */
static bContextStore *ctx_store_extend(Vector<std::unique_ptr<bContextStore>> &contexts)
{
  /* ensure we have a context to put the entry in, if it was already used
   * we have to copy the context to ensure */
  if (contexts.is_empty()) {
    contexts.append(std::make_unique<bContextStore>());
  }
  else if (contexts.last()->used) {
    auto new_ctx = std::make_unique<bContextStore>(bContextStore{contexts.last()->entries, false});
    contexts.append(std::move(new_ctx));
  }

  return contexts.last().get();
}

bContextStore *CTX_store_add(Vector<std::unique_ptr<bContextStore>> &contexts,
                             const blender::StringRef name,
                             const PointerRNA *ptr)
{
  bContextStore *ctx = ctx_store_extend(contexts);
  ctx->entries.append(bContextStoreEntry{name, *ptr});
  return ctx;
}

bContextStore *CTX_store_add(Vector<std::unique_ptr<bContextStore>> &contexts,
                             const blender::StringRef name,
                             const blender::StringRef str)
{
  bContextStore *ctx = ctx_store_extend(contexts);
  ctx->entries.append(bContextStoreEntry{name, std::string{str}});
  return ctx;
}

bContextStore *CTX_store_add(Vector<std::unique_ptr<bContextStore>> &contexts,
                             const blender::StringRef name,
                             const int64_t value)
{
  bContextStore *ctx = ctx_store_extend(contexts);
  ctx->entries.append(bContextStoreEntry{name, value});
  return ctx;
}

bContextStore *CTX_store_add_all(Vector<std::unique_ptr<bContextStore>> &contexts,
                                 const bContextStore *context)
{
  bContextStore *ctx = ctx_store_extend(contexts);
  for (const bContextStoreEntry &src_entry : context->entries) {
    ctx->entries.append(src_entry);
  }
  return ctx;
}

const bContextStore *CTX_store_get(const bContext *C)
{
  return C->wm.store;
}

void CTX_store_set(bContext *C, const bContextStore *store)
{
  C->wm.store = store;
}

const PointerRNA *CTX_store_ptr_lookup(const bContextStore *store,
                                       const blender::StringRef name,
                                       const StructRNA *type)
{
  for (auto entry = store->entries.rbegin(); entry != store->entries.rend(); ++entry) {
    if (entry->name == name && std::holds_alternative<PointerRNA>(entry->value)) {
      const PointerRNA &ptr = std::get<PointerRNA>(entry->value);
      if (!type || RNA_struct_is_a(ptr.type, type)) {
        return &ptr;
      }
    }
  }
  return nullptr;
}

template<typename T>
const T *ctx_store_lookup_impl(const bContextStore *store, const blender::StringRef name)
{
  for (auto entry = store->entries.rbegin(); entry != store->entries.rend(); ++entry) {
    if (entry->name == name && std::holds_alternative<T>(entry->value)) {
      return &std::get<T>(entry->value);
    }
  }
  return nullptr;
}

std::optional<blender::StringRefNull> CTX_store_string_lookup(const bContextStore *store,
                                                              const blender::StringRef name)
{
  if (const std::string *value = ctx_store_lookup_impl<std::string>(store, name)) {
    return *value;
  }
  return {};
}

std::optional<int64_t> CTX_store_int_lookup(const bContextStore *store,
                                            const blender::StringRef name)
{
  if (const int64_t *value = ctx_store_lookup_impl<int64_t>(store, name)) {
    return *value;
  }
  return {};
}

/* is python initialized? */

bool CTX_py_init_get(const bContext *C)
{
  return C->data.py_init;
}
void CTX_py_init_set(bContext *C, bool value)
{
  C->data.py_init = value;
}

void *CTX_py_dict_get(const bContext *C)
{
  return C->data.py_context;
}
void *CTX_py_dict_get_orig(const bContext *C)
{
  return C->data.py_context_orig;
}

void CTX_py_state_push(bContext *C, bContext_PyState *pystate, void *value)
{
  pystate->py_context = C->data.py_context;
  pystate->py_context_orig = C->data.py_context_orig;

  C->data.py_context = value;
  C->data.py_context_orig = value;
}
void CTX_py_state_pop(bContext *C, bContext_PyState *pystate)
{
  C->data.py_context = pystate->py_context;
  C->data.py_context_orig = pystate->py_context_orig;
}

/* data context utility functions */

struct bContextDataResult {
  PointerRNA ptr;
  Vector<PointerRNA> list;
  PropertyRNA *prop;
  int index;
  blender::StringRefNull str;
  std::optional<int64_t> int_value;
  const char **dir;
  ContextDataType type;
};

/** Create a brief string representation of a context data result. */
static std::string ctx_result_brief_repr(const bContextDataResult &result)
{
  switch (result.type) {
    case ContextDataType::Pointer:
      if (result.ptr.data) {
        const char *rna_type_name = result.ptr.type ? RNA_struct_identifier(result.ptr.type) :
                                                      "Unknown";
        /* Try to get the name property if it exists. */
        std::string member_name;
        if (result.ptr.type) {
          PropertyRNA *name_prop = RNA_struct_name_property(result.ptr.type);
          if (name_prop) {
            char name_buf[256];
            PointerRNA ptr_copy = result.ptr; /* Make a non-const copy. */
            char *name = RNA_property_string_get_alloc(
                &ptr_copy, name_prop, name_buf, sizeof(name_buf), nullptr);
            if (name && name[0] != '\0') {
              member_name = name;
              if (name != name_buf) {
                MEM_freeN(name);
              }
            }
          }
        }
        /* Format like PyRNA: '<Type("name") at 0xAddress>' or '<Type at 0xAddress>'. */
        if (!member_name.empty()) {
          return fmt::format("<{}(\"{}\") at 0x{:x}>",
                             rna_type_name,
                             member_name,
                             reinterpret_cast<uintptr_t>(result.ptr.data));
        }
        else {
          return fmt::format(
              "<{} at 0x{:x}>", rna_type_name, reinterpret_cast<uintptr_t>(result.ptr.data));
        }
      }
      else {
        return "None";
      }

    case ContextDataType::Collection:
      return fmt::format("[{} item(s)]", result.list.size());

    case ContextDataType::String:
      if (!result.str.is_empty()) {
        return "\"" + result.str + "\"";
      }
      else {
        return "\"\"";
      }

    case ContextDataType::Property:
      if (result.prop && result.ptr.data) {
        const char *prop_name = RNA_property_identifier(result.prop);
        const char *rna_type_name = result.ptr.type ? RNA_struct_identifier(result.ptr.type) :
                                                      "Unknown";
        if (result.index >= 0) {
          return fmt::format("<Property({}.{}[{}])>", rna_type_name, prop_name, result.index);
        }
        else {
          return fmt::format("<Property({}.{})>", rna_type_name, prop_name);
        }
      }
      else {
        return "<Property(None)>";
      }

    case ContextDataType::Int64:
      if (result.int_value.has_value()) {
        return std::to_string(result.int_value.value());
      }
      else {
        return "None";
      }
  }
  /* Unhandled context type. Update if new types are added. */
  BLI_assert_unreachable();
  return "<UNKNOWN>";
}

/** Simple logging for context data results. */
static void ctx_member_log_access(const bContext *C,
                                  const char *member,
                                  const bContextDataResult &result)
{
  const bool use_logging = CLOG_CHECK(BKE_LOG_CONTEXT, CLG_LEVEL_TRACE) ||
                           (C && CTX_member_logging_get(C));

  if (!use_logging) {
    return;
  }

  std::string value_repr = ctx_result_brief_repr(result);
  const char *value_desc = value_repr.c_str();

#ifdef WITH_PYTHON
  /* Get current Python location if available and Python is properly initialized. */
  std::optional<std::string> python_location;
  if (C && CTX_py_init_get(C)) {
    python_location = BPY_python_current_file_and_line();
  }
  const char *location = python_location ? python_location->c_str() : "unknown:0";
#else
  const char *location = "unknown:0";
#endif

  /* Use TRACE level when available, otherwise force output when Python logging is enabled. */
  const char *format = "%s: %s=%s";
  if (CLOG_CHECK(BKE_LOG_CONTEXT, CLG_LEVEL_TRACE)) {
    CLOG_TRACE(BKE_LOG_CONTEXT, format, location, member, value_desc);
  }
  else if (C && CTX_member_logging_get(C)) {
    /* Force output at TRACE level even if not enabled via command line. */
    CLOG_AT_LEVEL_NOCHECK(BKE_LOG_CONTEXT, CLG_LEVEL_TRACE, format, location, member, value_desc);
  }
}

static void *ctx_wm_python_context_get(const bContext *C,
                                       const char *member,
                                       const StructRNA *member_type,
                                       void *fall_through)
{
  void *return_data = nullptr;
  bool found_member = false;

#ifdef WITH_PYTHON
  if (UNLIKELY(C && CTX_py_dict_get(C))) {
    bContextDataResult result{};
    if (BPY_context_member_get((bContext *)C, member, &result)) {
      found_member = true;

      if (result.ptr.data) {
        if (RNA_struct_is_a(result.ptr.type, member_type)) {
          return_data = result.ptr.data;
        }
        else {
          CLOG_WARN(&LOG,
                    "PyContext '%s' is a '%s', expected a '%s'",
                    member,
                    RNA_struct_identifier(result.ptr.type),
                    RNA_struct_identifier(member_type));
        }
      }

      /* Log context member access directly without storing a copy. */
      ctx_member_log_access(C, member, result);
    }
  }
#else
  UNUSED_VARS(C, member, member_type);
#endif

  /* If no member was found, use the fallback value and create a simple result for logging. */
  if (!found_member) {
    bContextDataResult fallback_result{};
    fallback_result.ptr.data = fall_through;
    fallback_result.ptr.type = const_cast<StructRNA *>(
        member_type); /* Use the expected RNA type */
    fallback_result.type = ContextDataType::Pointer;
    return_data = fall_through;

    /* Log fallback context member access. */
    ctx_member_log_access(C, member, fallback_result);
  }

  /* Don't allow UI context access from non-main threads. */
  if (!BLI_thread_is_main()) {
    return nullptr;
  }

  return return_data;
}

static eContextResult ctx_data_get(bContext *C, const char *member, bContextDataResult *result)
{
  bScreen *screen;
  ScrArea *area;
  ARegion *region;
  int done = 0, recursion = C->data.recursion;
  int ret = 0;

  *result = {};

  /* NOTE: We'll log access when we have actual results. */

#ifdef WITH_PYTHON
  if (CTX_py_dict_get(C)) {
    if (BPY_context_member_get(C, member, result)) {
      /* Log the Python context result if we're in a temp_override. */
      ctx_member_log_access(C, member, *result);
      return CTX_RESULT_OK;
    }
  }
#endif

  /* Don't allow UI context access from non-main threads. */
  if (!BLI_thread_is_main()) {
    return CTX_RESULT_MEMBER_NOT_FOUND;
  }

  /* we check recursion to ensure that we do not get infinite
   * loops requesting data from ourselves in a context callback */

  /* Ok, this looks evil...
   * if (ret) done = -(-ret | -done);
   *
   * Values in order of importance
   * (0, -1, 1) - Where 1 is highest priority
   */
  if (done != 1 && recursion < 1 && C->wm.store) {
    C->data.recursion = 1;

    if (const PointerRNA *ptr = CTX_store_ptr_lookup(C->wm.store, member, nullptr)) {
      result->ptr = *ptr;
      done = 1;
    }
    else if (std::optional<blender::StringRefNull> str = CTX_store_string_lookup(C->wm.store,
                                                                                 member))
    {
      result->str = *str;
      result->type = ContextDataType::String;
      done = 1;
    }
    else if (std::optional<int64_t> int_value = CTX_store_int_lookup(C->wm.store, member)) {
      result->int_value = int_value;
      result->type = ContextDataType::Int64;
      done = 1;
    }
  }
  if (done != 1 && recursion < 2 && (region = CTX_wm_region(C))) {
    C->data.recursion = 2;
    if (region->runtime->type && region->runtime->type->context) {
      ret = region->runtime->type->context(C, member, result);
      if (ret) {
        done = -(-ret | -done);
      }
    }
  }
  if (done != 1 && recursion < 3 && (area = CTX_wm_area(C))) {
    C->data.recursion = 3;
    if (area->type && area->type->context) {
      ret = area->type->context(C, member, result);
      if (ret) {
        done = -(-ret | -done);
      }
    }
  }

  if (done != 1 && recursion < 4 && (screen = CTX_wm_screen(C))) {
    bContextDataCallback cb = reinterpret_cast<bContextDataCallback>(screen->context);
    C->data.recursion = 4;
    if (cb) {
      ret = cb(C, member, result);
      if (ret) {
        done = -(-ret | -done);
      }
    }
  }

  C->data.recursion = recursion;

  eContextResult final_result = eContextResult(done);

  /* Log context result if we're in a temp_override and we got a successful or no-data result. */
  if (ELEM(final_result, CTX_RESULT_OK, CTX_RESULT_NO_DATA)) {
    ctx_member_log_access(C, member, *result);
  }

  return final_result;
}

static void *ctx_data_pointer_get(const bContext *C, const char *member)
{
  bContextDataResult result;
  if (C && ctx_data_get((bContext *)C, member, &result) == CTX_RESULT_OK) {
    BLI_assert(result.type == ContextDataType::Pointer);
    return result.ptr.data;
  }

  return nullptr;
}

static bool ctx_data_pointer_verify(const bContext *C, const char *member, void **pointer)
{
  /* if context is nullptr, pointer must be nullptr too and that is a valid return */
  if (C == nullptr) {
    *pointer = nullptr;
    return true;
  }

  bContextDataResult result;
  if (ctx_data_get((bContext *)C, member, &result) == CTX_RESULT_OK) {
    BLI_assert(result.type == ContextDataType::Pointer);
    *pointer = result.ptr.data;
    return true;
  }

  *pointer = nullptr;
  return false;
}

static bool ctx_data_collection_get(const bContext *C,
                                    const char *member,
                                    Vector<PointerRNA> *list)
{
  bContextDataResult result;
  if (ctx_data_get((bContext *)C, member, &result) == CTX_RESULT_OK) {
    BLI_assert(result.type == ContextDataType::Collection);
    *list = std::move(result.list);
    return true;
  }

  list->clear();
  return false;
}

static bool ctx_data_base_collection_get(const bContext *C,
                                         const char *member,
                                         Vector<PointerRNA> *list)
{
  Vector<PointerRNA> ctx_object_list;
  if ((ctx_data_collection_get(C, member, &ctx_object_list) == false) ||
      ctx_object_list.is_empty())
  {
    list->clear();
    return false;
  }

  bContextDataResult result{};

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);

  bool ok = false;

  for (PointerRNA &ctx_object : ctx_object_list) {
    Object *ob = static_cast<Object *>(ctx_object.data);
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base != nullptr) {
      CTX_data_list_add(&result, &scene->id, &RNA_ObjectBase, base);
      ok = true;
    }
  }
  CTX_data_type_set(&result, ContextDataType::Collection);

  *list = std::move(result.list);
  return ok;
}

PointerRNA CTX_data_pointer_get(const bContext *C, const char *member)
{
  bContextDataResult result;
  if (ctx_data_get((bContext *)C, member, &result) == CTX_RESULT_OK) {
    BLI_assert(result.type == ContextDataType::Pointer);
    return result.ptr;
  }

  return PointerRNA_NULL;
}

PointerRNA CTX_data_pointer_get_type(const bContext *C, const char *member, StructRNA *type)
{
  PointerRNA ptr = CTX_data_pointer_get(C, member);

  if (ptr.data) {
    if (RNA_struct_is_a(ptr.type, type)) {
      return ptr;
    }

    CLOG_WARN(&LOG,
              "member '%s' is '%s', not '%s'",
              member,
              RNA_struct_identifier(ptr.type),
              RNA_struct_identifier(type));
  }

  return PointerRNA_NULL;
}

PointerRNA CTX_data_pointer_get_type_silent(const bContext *C, const char *member, StructRNA *type)
{
  PointerRNA ptr = CTX_data_pointer_get(C, member);

  if (ptr.data && RNA_struct_is_a(ptr.type, type)) {
    return ptr;
  }

  return PointerRNA_NULL;
}

Vector<PointerRNA> CTX_data_collection_get(const bContext *C, const char *member)
{
  bContextDataResult result;
  if (ctx_data_get((bContext *)C, member, &result) == CTX_RESULT_OK) {
    BLI_assert(result.type == ContextDataType::Collection);
    return result.list;
  }
  return {};
}

void CTX_data_collection_remap_property(blender::MutableSpan<PointerRNA> collection_pointers,
                                        const char *propname)
{
  for (PointerRNA &ptr : collection_pointers) {
    ptr = RNA_pointer_get(&ptr, propname);
  }
}

std::optional<blender::StringRefNull> CTX_data_string_get(const bContext *C, const char *member)
{
  bContextDataResult result;
  if (ctx_data_get((bContext *)C, member, &result) == CTX_RESULT_OK) {
    BLI_assert(result.type == ContextDataType::String);
    return result.str;
  }

  return {};
}

std::optional<int64_t> CTX_data_int_get(const bContext *C, const char *member)
{
  bContextDataResult result;
  if (ctx_data_get((bContext *)C, member, &result) == CTX_RESULT_OK) {
    BLI_assert(result.type == ContextDataType::Int64);
    return result.int_value;
  }

  return {};
}

int /*eContextResult*/ CTX_data_get(const bContext *C,
                                    const char *member,
                                    PointerRNA *r_ptr,
                                    Vector<PointerRNA> *r_lb,
                                    PropertyRNA **r_prop,
                                    int *r_index,
                                    blender::StringRef *r_str,
                                    std::optional<int64_t> *r_int_value,
                                    ContextDataType *r_type)
{
  bContextDataResult result;
  eContextResult ret = ctx_data_get((bContext *)C, member, &result);

  if (ret == CTX_RESULT_OK) {
    *r_ptr = result.ptr;
    *r_lb = result.list;
    *r_prop = result.prop;
    *r_index = result.index;
    *r_str = result.str;
    *r_int_value = result.int_value;
    *r_type = result.type;
  }
  else {
    *r_ptr = {};
    r_lb->clear();
    *r_str = "";
    *r_int_value = {};
    *r_type = ContextDataType::Pointer;
  }

  return ret;
}

static void data_dir_add(ListBase *lb, const char *member, const bool use_all)
{
  LinkData *link;

  if ((use_all == false) && STREQ(member, "scene")) { /* exception */
    return;
  }

  if (BLI_findstring(lb, member, offsetof(LinkData, data))) {
    return;
  }

  link = MEM_callocN<LinkData>(__func__);
  link->data = (void *)member;
  BLI_addtail(lb, link);
}

ListBase CTX_data_dir_get_ex(const bContext *C,
                             const bool use_store,
                             const bool use_rna,
                             const bool use_all)
{
  bContextDataResult result{};
  ListBase lb;
  bScreen *screen;
  ScrArea *area;
  ARegion *region;
  int a;

  memset(&lb, 0, sizeof(lb));

  if (use_rna) {
    char name_buf[256], *name;
    int namelen;

    PropertyRNA *iterprop;
    PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, (void *)C);

    iterprop = RNA_struct_iterator_property(ctx_ptr.type);

    RNA_PROP_BEGIN (&ctx_ptr, itemptr, iterprop) {
      name = RNA_struct_name_get_alloc(&itemptr, name_buf, sizeof(name_buf), &namelen);
      data_dir_add(&lb, name, use_all);
      if (name != name_buf) {
        MEM_freeN(name);
      }
    }
    RNA_PROP_END;
  }
  if (use_store && C->wm.store) {
    for (const bContextStoreEntry &entry : C->wm.store->entries) {
      data_dir_add(&lb, entry.name.c_str(), use_all);
    }
  }
  if ((region = CTX_wm_region(C)) && region->runtime->type && region->runtime->type->context) {
    region->runtime->type->context(C, "", &result);

    if (result.dir) {
      for (a = 0; result.dir[a]; a++) {
        data_dir_add(&lb, result.dir[a], use_all);
      }
    }
  }
  if ((area = CTX_wm_area(C)) && area->type && area->type->context) {
    area->type->context(C, "", &result);

    if (result.dir) {
      for (a = 0; result.dir[a]; a++) {
        data_dir_add(&lb, result.dir[a], use_all);
      }
    }
  }
  if ((screen = CTX_wm_screen(C)) && screen->context) {
    bContextDataCallback cb = reinterpret_cast<bContextDataCallback>(screen->context);
    cb(C, "", &result);

    if (result.dir) {
      for (a = 0; result.dir[a]; a++) {
        data_dir_add(&lb, result.dir[a], use_all);
      }
    }
  }

  return lb;
}

ListBase CTX_data_dir_get(const bContext *C)
{
  return CTX_data_dir_get_ex(C, true, false, false);
}

bool CTX_data_equals(const char *member, const char *str)
{
  return STREQ(member, str);
}

bool CTX_data_dir(const char *member)
{
  return member[0] == '\0';
}

void CTX_data_id_pointer_set(bContextDataResult *result, ID *id)
{
  result->ptr = RNA_id_pointer_create(id);
}

void CTX_data_pointer_set(bContextDataResult *result, ID *id, StructRNA *type, void *data)
{
  result->ptr = RNA_pointer_create_discrete(id, type, data);
}

void CTX_data_pointer_set_ptr(bContextDataResult *result, const PointerRNA *ptr)
{
  result->ptr = *ptr;
}

void CTX_data_id_list_add(bContextDataResult *result, ID *id)
{
  result->list.append(RNA_id_pointer_create(id));
}

void CTX_data_list_add(bContextDataResult *result, ID *id, StructRNA *type, void *data)
{
  result->list.append(RNA_pointer_create_discrete(id, type, data));
}

void CTX_data_list_add_ptr(bContextDataResult *result, const PointerRNA *ptr)
{
  result->list.append(*ptr);
}

int ctx_data_list_count(const bContext *C,
                        bool (*func)(const bContext *, blender::Vector<PointerRNA> *))
{
  blender::Vector<PointerRNA> list;
  if (func(C, &list)) {
    return list.size();
  }
  return 0;
}

void CTX_data_prop_set(bContextDataResult *result, PropertyRNA *prop, int index)
{
  result->prop = prop;
  result->index = index;
}

void CTX_data_dir_set(bContextDataResult *result, const char **dir)
{
  result->dir = dir;
}

void CTX_data_type_set(bContextDataResult *result, ContextDataType type)
{
  result->type = type;
}

ContextDataType CTX_data_type_get(bContextDataResult *result)
{
  return result->type;
}

/* window manager context */

wmWindowManager *CTX_wm_manager(const bContext *C)
{
  return C->wm.manager;
}

bool CTX_wm_interface_locked(const bContext *C)
{
  return C->wm.manager->runtime->is_interface_locked;
}

wmWindow *CTX_wm_window(const bContext *C)
{
  return static_cast<wmWindow *>(
      ctx_wm_python_context_get(C, "window", &RNA_Window, C->wm.window));
}

WorkSpace *CTX_wm_workspace(const bContext *C)
{
  return static_cast<WorkSpace *>(
      ctx_wm_python_context_get(C, "workspace", &RNA_WorkSpace, C->wm.workspace));
}

bScreen *CTX_wm_screen(const bContext *C)
{
  return static_cast<bScreen *>(ctx_wm_python_context_get(C, "screen", &RNA_Screen, C->wm.screen));
}

ScrArea *CTX_wm_area(const bContext *C)
{
  return static_cast<ScrArea *>(ctx_wm_python_context_get(C, "area", &RNA_Area, C->wm.area));
}

SpaceLink *CTX_wm_space_data(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  return (area) ? static_cast<SpaceLink *>(area->spacedata.first) : nullptr;
}

ARegion *CTX_wm_region(const bContext *C)
{
  return static_cast<ARegion *>(ctx_wm_python_context_get(C, "region", &RNA_Region, C->wm.region));
}

void *CTX_wm_region_data(const bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  return (region) ? region->regiondata : nullptr;
}

ARegion *CTX_wm_region_popup(const bContext *C)
{
  return C->wm.region_popup;
}

wmGizmoGroup *CTX_wm_gizmo_group(const bContext *C)
{
  return C->wm.gizmo_group;
}

wmMsgBus *CTX_wm_message_bus(const bContext *C)
{
  return C->wm.manager ? C->wm.manager->runtime->message_bus : nullptr;
}

ReportList *CTX_wm_reports(const bContext *C)
{
  if (C->wm.manager) {
    return &(C->wm.manager->runtime->reports);
  }

  return nullptr;
}

View3D *CTX_wm_view3d(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_VIEW3D) {
    return static_cast<View3D *>(area->spacedata.first);
  }
  return nullptr;
}

RegionView3D *CTX_wm_region_view3d(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  ARegion *region = CTX_wm_region(C);

  if (area && area->spacetype == SPACE_VIEW3D) {
    if (region && region->regiontype == RGN_TYPE_WINDOW) {
      return static_cast<RegionView3D *>(region->regiondata);
    }
  }
  return nullptr;
}

SpaceText *CTX_wm_space_text(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_TEXT) {
    return static_cast<SpaceText *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceConsole *CTX_wm_space_console(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_CONSOLE) {
    return static_cast<SpaceConsole *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceImage *CTX_wm_space_image(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_IMAGE) {
    return static_cast<SpaceImage *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceProperties *CTX_wm_space_properties(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_PROPERTIES) {
    return static_cast<SpaceProperties *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceFile *CTX_wm_space_file(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_FILE) {
    return static_cast<SpaceFile *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceSeq *CTX_wm_space_seq(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_SEQ) {
    return static_cast<SpaceSeq *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceOutliner *CTX_wm_space_outliner(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_OUTLINER) {
    return static_cast<SpaceOutliner *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceNla *CTX_wm_space_nla(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_NLA) {
    return static_cast<SpaceNla *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceNode *CTX_wm_space_node(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_NODE) {
    return static_cast<SpaceNode *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceGraph *CTX_wm_space_graph(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_GRAPH) {
    return static_cast<SpaceGraph *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceAction *CTX_wm_space_action(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_ACTION) {
    return static_cast<SpaceAction *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceInfo *CTX_wm_space_info(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_INFO) {
    return static_cast<SpaceInfo *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceUserPref *CTX_wm_space_userpref(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_USERPREF) {
    return static_cast<SpaceUserPref *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceClip *CTX_wm_space_clip(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_CLIP) {
    return static_cast<SpaceClip *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceTopBar *CTX_wm_space_topbar(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_TOPBAR) {
    return static_cast<SpaceTopBar *>(area->spacedata.first);
  }
  return nullptr;
}

SpaceSpreadsheet *CTX_wm_space_spreadsheet(const bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area && area->spacetype == SPACE_SPREADSHEET) {
    return static_cast<SpaceSpreadsheet *>(area->spacedata.first);
  }
  return nullptr;
}

void CTX_wm_manager_set(bContext *C, wmWindowManager *wm)
{
  C->wm.manager = wm;
  C->wm.window = nullptr;
  C->wm.screen = nullptr;
  C->wm.area = nullptr;
  C->wm.region = nullptr;
}

#ifdef WITH_PYTHON
#  define PYCTX_REGION_MEMBERS "region", "region_data"
#  define PYCTX_AREA_MEMBERS "area", "space_data", PYCTX_REGION_MEMBERS
#  define PYCTX_SCREEN_MEMBERS "screen", PYCTX_AREA_MEMBERS
#  define PYCTX_WINDOW_MEMBERS "window", "scene", "workspace", PYCTX_SCREEN_MEMBERS
#endif

void CTX_wm_window_set(bContext *C, wmWindow *win)
{
  C->wm.window = win;
  if (win) {
    C->data.scene = win->scene;
  }
  C->wm.workspace = (win) ? BKE_workspace_active_get(win->workspace_hook) : nullptr;
  C->wm.screen = (win) ? BKE_workspace_active_screen_get(win->workspace_hook) : nullptr;
  C->wm.area = nullptr;
  C->wm.region = nullptr;

#ifdef WITH_PYTHON
  if (C->data.py_context != nullptr) {
    const char *members[] = {PYCTX_WINDOW_MEMBERS};
    BPY_context_dict_clear_members_array(
        &C->data.py_context, C->data.py_context_orig, members, ARRAY_SIZE(members));
  }
#endif
}

void CTX_wm_screen_set(bContext *C, bScreen *screen)
{
  C->wm.screen = screen;
  C->wm.area = nullptr;
  C->wm.region = nullptr;

#ifdef WITH_PYTHON
  if (C->data.py_context != nullptr) {
    const char *members[] = {PYCTX_SCREEN_MEMBERS};
    BPY_context_dict_clear_members_array(
        &C->data.py_context, C->data.py_context_orig, members, ARRAY_SIZE(members));
  }
#endif
}

void CTX_wm_area_set(bContext *C, ScrArea *area)
{
  C->wm.area = area;
  C->wm.region = nullptr;

#ifdef WITH_PYTHON
  if (C->data.py_context != nullptr) {
    const char *members[] = {PYCTX_AREA_MEMBERS};
    BPY_context_dict_clear_members_array(
        &C->data.py_context, C->data.py_context_orig, members, ARRAY_SIZE(members));
  }
#endif
}

void CTX_wm_region_set(bContext *C, ARegion *region)
{
  C->wm.region = region;

#ifdef WITH_PYTHON
  if (C->data.py_context != nullptr) {
    const char *members[] = {PYCTX_REGION_MEMBERS};
    BPY_context_dict_clear_members_array(
        &C->data.py_context, C->data.py_context_orig, members, ARRAY_SIZE(members));
  }
#endif
}

void CTX_wm_region_popup_set(bContext *C, ARegion *region_popup)
{
  BLI_assert(region_popup == nullptr || region_popup->regiontype == RGN_TYPE_TEMPORARY);
  C->wm.region_popup = region_popup;
}

void CTX_wm_gizmo_group_set(bContext *C, wmGizmoGroup *gzgroup)
{
  C->wm.gizmo_group = gzgroup;
}

void CTX_wm_operator_poll_msg_clear(bContext *C)
{
  bContextPollMsgDyn_Params *params = &C->wm.operator_poll_msg_dyn_params;
  if (params->free_fn != nullptr) {
    params->free_fn(C, params->user_data);
  }
  params->get_fn = nullptr;
  params->free_fn = nullptr;
  params->user_data = nullptr;

  C->wm.operator_poll_msg = nullptr;
}
void CTX_wm_operator_poll_msg_set(bContext *C, const char *msg)
{
  CTX_wm_operator_poll_msg_clear(C);

  C->wm.operator_poll_msg = msg;
}

void CTX_wm_operator_poll_msg_set_dynamic(bContext *C, const bContextPollMsgDyn_Params *params)
{
  CTX_wm_operator_poll_msg_clear(C);

  C->wm.operator_poll_msg_dyn_params = *params;
}

const char *CTX_wm_operator_poll_msg_get(bContext *C, bool *r_free)
{
  bContextPollMsgDyn_Params *params = &C->wm.operator_poll_msg_dyn_params;
  if (params->get_fn != nullptr) {
    char *msg = params->get_fn(C, params->user_data);
    if (msg != nullptr) {
      *r_free = true;
    }
    return msg;
  }

  *r_free = false;
  return IFACE_(C->wm.operator_poll_msg);
}

/* data context */

Main *CTX_data_main(const bContext *C)
{
  Main *bmain;
  if (ctx_data_pointer_verify(C, "blend_data", (void **)&bmain)) {
    return bmain;
  }

  return C->data.main;
}

void CTX_data_main_set(bContext *C, Main *bmain)
{
  C->data.main = bmain;
  BKE_sound_refresh_callback_bmain(bmain);
}

Scene *CTX_data_scene(const bContext *C)
{
  Scene *scene;
  if (ctx_data_pointer_verify(C, "scene", (void **)&scene)) {
    return scene;
  }

  return C->data.scene;
}

Scene *CTX_data_sequencer_scene(const bContext *C)
{
  Scene *scene;
  if (ctx_data_pointer_verify(C, "sequencer_scene", (void **)&scene)) {
    return scene;
  }
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (workspace) {
    return workspace->sequencer_scene;
  }
  return nullptr;
}

ViewLayer *CTX_data_view_layer(const bContext *C)
{
  ViewLayer *view_layer;

  if (ctx_data_pointer_verify(C, "view_layer", (void **)&view_layer)) {
    return view_layer;
  }

  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);
  if (win) {
    view_layer = BKE_view_layer_find(scene, win->view_layer_name);
    if (view_layer) {
      return view_layer;
    }
  }

  return BKE_view_layer_default_view(scene);
}

RenderEngineType *CTX_data_engine_type(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  return RE_engines_find(scene->r.engine);
}

LayerCollection *CTX_data_layer_collection(const bContext *C)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  LayerCollection *layer_collection;

  if (ctx_data_pointer_verify(C, "layer_collection", (void **)&layer_collection)) {
    if (BKE_view_layer_has_collection(view_layer, layer_collection->collection)) {
      return layer_collection;
    }
  }

  /* fallback */
  return BKE_layer_collection_get_active(view_layer);
}

Collection *CTX_data_collection(const bContext *C)
{
  Collection *collection;
  if (ctx_data_pointer_verify(C, "collection", (void **)&collection)) {
    return collection;
  }

  LayerCollection *layer_collection = CTX_data_layer_collection(C);
  if (layer_collection) {
    return layer_collection->collection;
  }

  /* fallback */
  Scene *scene = CTX_data_scene(C);
  return scene->master_collection;
}

enum eContextObjectMode CTX_data_mode_enum_ex(const Object *obedit,
                                              const Object *ob,
                                              const eObjectMode object_mode)
{
  // Object *obedit = CTX_data_edit_object(C);
  if (obedit) {
    switch (obedit->type) {
      case OB_MESH:
        return CTX_MODE_EDIT_MESH;
      case OB_CURVES_LEGACY:
        return CTX_MODE_EDIT_CURVE;
      case OB_SURF:
        return CTX_MODE_EDIT_SURFACE;
      case OB_FONT:
        return CTX_MODE_EDIT_TEXT;
      case OB_ARMATURE:
        return CTX_MODE_EDIT_ARMATURE;
      case OB_MBALL:
        return CTX_MODE_EDIT_METABALL;
      case OB_LATTICE:
        return CTX_MODE_EDIT_LATTICE;
      case OB_CURVES:
        return CTX_MODE_EDIT_CURVES;
      case OB_GREASE_PENCIL:
        return CTX_MODE_EDIT_GREASE_PENCIL;
      case OB_POINTCLOUD:
        return CTX_MODE_EDIT_POINTCLOUD;
    }
  }
  else {
    // Object *ob = CTX_data_active_object(C);
    if (ob) {
      if (object_mode & OB_MODE_POSE) {
        return CTX_MODE_POSE;
      }
      if (object_mode & OB_MODE_SCULPT) {
        return CTX_MODE_SCULPT;
      }
      if (object_mode & OB_MODE_WEIGHT_PAINT) {
        return CTX_MODE_PAINT_WEIGHT;
      }
      if (object_mode & OB_MODE_VERTEX_PAINT) {
        return CTX_MODE_PAINT_VERTEX;
      }
      if (object_mode & OB_MODE_TEXTURE_PAINT) {
        return CTX_MODE_PAINT_TEXTURE;
      }
      if (object_mode & OB_MODE_PARTICLE_EDIT) {
        return CTX_MODE_PARTICLE;
      }
      if (object_mode & OB_MODE_PAINT_GREASE_PENCIL) {
        if (ob->type == OB_GREASE_PENCIL) {
          return CTX_MODE_PAINT_GREASE_PENCIL;
        }
      }
      if (object_mode & OB_MODE_EDIT_GPENCIL_LEGACY) {
        return CTX_MODE_EDIT_GPENCIL_LEGACY;
      }
      if (object_mode & OB_MODE_SCULPT_GREASE_PENCIL) {
        if (ob->type == OB_GREASE_PENCIL) {
          return CTX_MODE_SCULPT_GREASE_PENCIL;
        }
      }
      if (object_mode & OB_MODE_WEIGHT_GREASE_PENCIL) {
        if (ob->type == OB_GREASE_PENCIL) {
          return CTX_MODE_WEIGHT_GREASE_PENCIL;
        }
      }
      if (object_mode & OB_MODE_VERTEX_GREASE_PENCIL) {
        if (ob->type == OB_GREASE_PENCIL) {
          return CTX_MODE_VERTEX_GREASE_PENCIL;
        }
      }
      if (object_mode & OB_MODE_SCULPT_CURVES) {
        return CTX_MODE_SCULPT_CURVES;
      }
    }
  }

  return CTX_MODE_OBJECT;
}

enum eContextObjectMode CTX_data_mode_enum(const bContext *C)
{
  Object *obedit = CTX_data_edit_object(C);
  Object *obact = obedit ? nullptr : CTX_data_active_object(C);
  return CTX_data_mode_enum_ex(obedit, obact, obact ? eObjectMode(obact->mode) : OB_MODE_OBJECT);
}

/**
 * Would prefer if we can use the enum version below over this one - Campbell.
 *
 * \note Must be aligned with above enum.
 */
static const char *data_mode_strings[] = {
    "mesh_edit",
    "curve_edit",
    "surface_edit",
    "text_edit",
    "armature_edit",
    "mball_edit",
    "lattice_edit",
    "curves_edit",
    "grease_pencil_edit",
    "pointcloud_edit",
    "posemode",
    "sculpt_mode",
    "weightpaint",
    "vertexpaint",
    "imagepaint",
    "particlemode",
    "objectmode",
    "greasepencil_paint",
    "greasepencil_edit",
    "greasepencil_sculpt",
    "greasepencil_weight",
    "greasepencil_vertex",
    "curves_sculpt",
    "grease_pencil_paint",
    "grease_pencil_sculpt",
    "grease_pencil_weight",
    "grease_pencil_vertex",
    nullptr,
};
BLI_STATIC_ASSERT(ARRAY_SIZE(data_mode_strings) == CTX_MODE_NUM + 1,
                  "Must have a string for each context mode")
const char *CTX_data_mode_string(const bContext *C)
{
  return data_mode_strings[CTX_data_mode_enum(C)];
}

void CTX_data_scene_set(bContext *C, Scene *scene)
{
  C->data.scene = scene;

#ifdef WITH_PYTHON
  if (C->data.py_context != nullptr) {
    const char *members[] = {"scene"};
    BPY_context_dict_clear_members_array(&C->data.py_context, C->data.py_context_orig, members, 1);
  }
#endif
}

ToolSettings *CTX_data_tool_settings(const bContext *C)
{
  ToolSettings *toolsettings;
  if (ctx_data_pointer_verify(C, "tool_settings", (void **)&toolsettings)) {
    return toolsettings;
  }

  Scene *scene = CTX_data_scene(C);
  if (scene) {
    return scene->toolsettings;
  }

  return nullptr;
}

bool CTX_data_selected_ids(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_ids", list);
}

bool CTX_data_selected_nodes(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_nodes", list);
}

bool CTX_data_selected_editable_objects(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_editable_objects", list);
}

bool CTX_data_selected_editable_bases(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_base_collection_get(C, "selected_editable_objects", list);
}

bool CTX_data_editable_objects(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "editable_objects", list);
}

bool CTX_data_editable_bases(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_base_collection_get(C, "editable_objects", list);
}

bool CTX_data_selected_objects(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_objects", list);
}

bool CTX_data_selected_bases(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_base_collection_get(C, "selected_objects", list);
}

bool CTX_data_visible_objects(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "visible_objects", list);
}

bool CTX_data_visible_bases(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_base_collection_get(C, "visible_objects", list);
}

bool CTX_data_selectable_objects(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selectable_objects", list);
}

bool CTX_data_selectable_bases(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_base_collection_get(C, "selectable_objects", list);
}

Object *CTX_data_active_object(const bContext *C)
{
  return static_cast<Object *>(ctx_data_pointer_get(C, "active_object"));
}

Base *CTX_data_active_base(const bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob == nullptr) {
    return nullptr;
  }
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  return BKE_view_layer_base_find(view_layer, ob);
}

Object *CTX_data_edit_object(const bContext *C)
{
  return static_cast<Object *>(ctx_data_pointer_get(C, "edit_object"));
}

Image *CTX_data_edit_image(const bContext *C)
{
  return static_cast<Image *>(ctx_data_pointer_get(C, "edit_image"));
}

Text *CTX_data_edit_text(const bContext *C)
{
  return static_cast<Text *>(ctx_data_pointer_get(C, "edit_text"));
}

MovieClip *CTX_data_edit_movieclip(const bContext *C)
{
  return static_cast<MovieClip *>(ctx_data_pointer_get(C, "edit_movieclip"));
}

Mask *CTX_data_edit_mask(const bContext *C)
{
  return static_cast<Mask *>(ctx_data_pointer_get(C, "edit_mask"));
}

EditBone *CTX_data_active_bone(const bContext *C)
{
  return static_cast<EditBone *>(ctx_data_pointer_get(C, "active_bone"));
}

CacheFile *CTX_data_edit_cachefile(const bContext *C)
{
  return static_cast<CacheFile *>(ctx_data_pointer_get(C, "edit_cachefile"));
}

bool CTX_data_selected_bones(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_bones", list);
}

bool CTX_data_selected_editable_bones(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_editable_bones", list);
}

bool CTX_data_visible_bones(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "visible_bones", list);
}

bool CTX_data_editable_bones(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "editable_bones", list);
}

bPoseChannel *CTX_data_active_pose_bone(const bContext *C)
{
  return static_cast<bPoseChannel *>(ctx_data_pointer_get(C, "active_pose_bone"));
}

bool CTX_data_selected_pose_bones(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_pose_bones", list);
}

bool CTX_data_selected_pose_bones_from_active_object(const bContext *C,
                                                     blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "selected_pose_bones_from_active_object", list);
}

bool CTX_data_visible_pose_bones(const bContext *C, blender::Vector<PointerRNA> *list)
{
  return ctx_data_collection_get(C, "visible_pose_bones", list);
}

const AssetLibraryReference *CTX_wm_asset_library_ref(const bContext *C)
{
  return static_cast<AssetLibraryReference *>(ctx_data_pointer_get(C, "asset_library_reference"));
}

blender::asset_system::AssetRepresentation *CTX_wm_asset(const bContext *C)
{
  return static_cast<blender::asset_system::AssetRepresentation *>(
      ctx_data_pointer_get(C, "asset"));
}

Depsgraph *CTX_data_depsgraph_pointer(const bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
  /* Dependency graph might have been just allocated, and hence it will not be marked.
   * This confuses redo system due to the lack of flushing changes back to the original data.
   * In the future we would need to check whether the CTX_wm_window(C)  is in editing mode (as an
   * opposite of playback-preview-only) and set active flag based on that. */
  DEG_make_active(depsgraph);
  return depsgraph;
}

Depsgraph *CTX_data_expect_evaluated_depsgraph(const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  /* TODO(sergey): Assert that the dependency graph is fully evaluated.
   * Note that first the depsgraph and scene post-evaluation hooks needs to run extra round of
   * updates first to make check here really reliable. */
  return depsgraph;
}

Depsgraph *CTX_data_ensure_evaluated_depsgraph(const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Main *bmain = CTX_data_main(C);
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  return depsgraph;
}

Depsgraph *CTX_data_depsgraph_on_load(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  return BKE_scene_get_depsgraph(scene, view_layer);
}

void CTX_member_logging_set(bContext *C, bool enable)
{
  C->data.log_access = enable;
}

bool CTX_member_logging_get(const bContext *C)
{
  return C->data.log_access;
}
