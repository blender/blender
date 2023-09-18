/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

#include "RNA_prototypes.h"
#include "RNA_types.hh"
#include <stdio.h>

struct ID;
struct bContext;
struct wmMsg;
/* Opaque (don't expose outside `wm_message_bus.cc`). */
struct wmMsgBus;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;
struct wmMsgSubscribeValueLink;

using wmMsgNotifyFn = void (*)(bContext *C,
                               wmMsgSubscribeKey *msg_key,
                               wmMsgSubscribeValue *msg_val);
using wmMsgSubscribeValueFreeDataFn = void (*)(wmMsgSubscribeKey *msg_key,
                                               wmMsgSubscribeValue *msg_val);

/* Exactly what arguments here is not obvious. */
using wmMsgSubscribeValueUpdateIdFn =
    void (*)(bContext *C, wmMsgBus *mbus, ID *id_src, ID *id_dst, wmMsgSubscribeValue *msg_val);
enum {
  WM_MSG_TYPE_RNA = 0,
  WM_MSG_TYPE_STATIC = 1,
};
#define WM_MSG_TYPE_NUM 2

struct wmMsgTypeInfo {
  struct {
    unsigned int (*hash_fn)(const void *msg);
    bool (*cmp_fn)(const void *a, const void *b);
    void (*key_free_fn)(void *key);
  } gset;

  void (*update_by_id)(wmMsgBus *mbus, ID *id_src, ID *id_dst);
  void (*remove_by_id)(wmMsgBus *mbus, const ID *id);
  void (*repr)(FILE *stream, const wmMsgSubscribeKey *msg_key);

  /* sizeof(wmMsgSubscribeKey_*) */
  uint msg_key_size;
};

struct wmMsg {
  unsigned int type;
  // #ifdef DEBUG
  /* For debugging: '__func__:__LINE__'. */
  const char *id;
  // #endif
};

struct wmMsgSubscribeKey {
  /** Linked list for predictable ordering, otherwise we would depend on #GHash bucketing. */
  wmMsgSubscribeKey *next, *prev;
  ListBase values;
  /* Over-allocate, eg: #wmMsgSubscribeKey_RNA */
  /* Last member will be 'wmMsg_*' */
};

/** One of many in #wmMsgSubscribeKey.values */
struct wmMsgSubscribeValue {
  wmMsgSubscribeValue *next, *prev;

  /** Handle, used to iterate and clear. */
  void *owner;
  /** User data, can be whatever we like, free using the 'free_data' callback if it's owned. */
  void *user_data;

  /** Callbacks */
  wmMsgNotifyFn notify;
  wmMsgSubscribeValueUpdateIdFn update_id;
  wmMsgSubscribeValueFreeDataFn free_data;

  /** Keep this subscriber if possible. */
  uint is_persistent : 1;
  /* tag to run when handling events,
   * we may want option for immediate execution. */
  uint tag : 1;
};

/** One of many in #wmMsgSubscribeKey.values */
struct wmMsgSubscribeValueLink {
  wmMsgSubscribeValueLink *next, *prev;
  wmMsgSubscribeValue params;
};

void WM_msgbus_types_init();

wmMsgBus *WM_msgbus_create();
void WM_msgbus_destroy(wmMsgBus *mbus);

void WM_msgbus_clear_by_owner(wmMsgBus *mbus, void *owner);

void WM_msg_dump(wmMsgBus *mbus, const char *info);
void WM_msgbus_handle(wmMsgBus *mbus, bContext *C);

void WM_msg_publish_with_key(wmMsgBus *mbus, wmMsgSubscribeKey *msg_key);
/**
 * \param msg_key_test: Needs following #wmMsgSubscribeKey fields filled in:
 * - `msg.params`
 * - `msg.head.type`
 * - `msg.head.id`
 * .. other values should be zeroed.
 *
 * \return The key for this subscription.
 * note that this is only needed in rare cases when the key needs further manipulation.
 */
wmMsgSubscribeKey *WM_msg_subscribe_with_key(wmMsgBus *mbus,
                                             const wmMsgSubscribeKey *msg_key_test,
                                             const wmMsgSubscribeValue *msg_val_params);

void WM_msg_id_update(wmMsgBus *mbus, ID *id_src, ID *id_dst);
void WM_msg_id_remove(wmMsgBus *mbus, const ID *id);

/* -------------------------------------------------------------------------- */
/* `wm_message_bus_static.cc` */

enum {
  /* generic window redraw */
  WM_MSG_STATICTYPE_WINDOW_DRAW = 0,
  WM_MSG_STATICTYPE_SCREEN_EDIT = 1,
  WM_MSG_STATICTYPE_FILE_READ = 2,
};

struct wmMsgParams_Static {
  int event;
};

struct wmMsg_Static {
  wmMsg head; /* keep first */
  wmMsgParams_Static params;
};

struct wmMsgSubscribeKey_Static {
  wmMsgSubscribeKey head;
  wmMsg_Static msg;
};

void WM_msgtypeinfo_init_static(wmMsgTypeInfo *msgtype_info);

wmMsgSubscribeKey_Static *WM_msg_lookup_static(wmMsgBus *mbus,
                                               const wmMsgParams_Static *msg_key_params);
void WM_msg_publish_static_params(wmMsgBus *mbus, const wmMsgParams_Static *msg_key_params);
void WM_msg_publish_static(wmMsgBus *mbus,
                           /* wmMsgParams_Static (expanded) */
                           int event);
void WM_msg_subscribe_static_params(wmMsgBus *mbus,
                                    const wmMsgParams_Static *msg_key_params,
                                    const wmMsgSubscribeValue *msg_val_params,
                                    const char *id_repr);
void WM_msg_subscribe_static(wmMsgBus *mbus,
                             int event,
                             const wmMsgSubscribeValue *msg_val_params,
                             const char *id_repr);

/* -------------------------------------------------------------------------- */
/* `wm_message_bus_rna.cc` */

struct wmMsgParams_RNA {
  /** when #PointerRNA.data & owner_id are NULL. match against all. */
  PointerRNA ptr;
  /** when NULL, match against any property. */
  const PropertyRNA *prop;

  /**
   * Optional RNA data path for persistent RNA properties, ignore if NULL.
   * otherwise it's allocated.
   */
  char *data_path;
};

struct wmMsg_RNA {
  wmMsg head; /* keep first */
  wmMsgParams_RNA params;
};

struct wmMsgSubscribeKey_RNA {
  wmMsgSubscribeKey head;
  wmMsg_RNA msg;
};

void WM_msgtypeinfo_init_rna(wmMsgTypeInfo *msgtype_info);

wmMsgSubscribeKey_RNA *WM_msg_lookup_rna(wmMsgBus *mbus, const wmMsgParams_RNA *msg_key_params);
void WM_msg_publish_rna_params(wmMsgBus *mbus, const wmMsgParams_RNA *msg_key_params);
void WM_msg_publish_rna(wmMsgBus *mbus,
                        /* wmMsgParams_RNA (expanded) */
                        PointerRNA *ptr,
                        PropertyRNA *prop);
void WM_msg_subscribe_rna_params(wmMsgBus *mbus,
                                 const wmMsgParams_RNA *msg_key_params,
                                 const wmMsgSubscribeValue *msg_val_params,
                                 const char *id_repr);
void WM_msg_subscribe_rna(wmMsgBus *mbus,
                          PointerRNA *ptr,
                          const PropertyRNA *prop,
                          const wmMsgSubscribeValue *msg_val_params,
                          const char *id_repr);

/* ID variants */
void WM_msg_subscribe_ID(wmMsgBus *mbus,
                         ID *id,
                         const wmMsgSubscribeValue *msg_val_params,
                         const char *id_repr);
void WM_msg_publish_ID(wmMsgBus *mbus, ID *id);

#define WM_msg_publish_rna_prop(mbus, id_, data_, type_, prop_) \
  { \
    wmMsgParams_RNA msg_key_params_ = {{0}}; \
    msg_key_params_.ptr = RNA_pointer_create(id_, &RNA_##type_, data_); \
    msg_key_params_.prop = &rna_##type_##_##prop_; \
    WM_msg_publish_rna_params(mbus, &msg_key_params_); \
  } \
  ((void)0)
#define WM_msg_subscribe_rna_prop(mbus, id_, data_, type_, prop_, value) \
  { \
    wmMsgParams_RNA msg_key_params_ = {{0}}; \
    msg_key_params_.ptr = RNA_pointer_create(id_, &RNA_##type_, data_); \
    msg_key_params_.prop = &rna_##type_##_##prop_; \
    WM_msg_subscribe_rna_params(mbus, &msg_key_params_, value, __func__); \
  } \
  ((void)0)

/* Anonymous variants (for convenience) */
#define WM_msg_subscribe_rna_anon_type(mbus, type_, value) \
  { \
    PointerRNA msg_ptr_ = {0, &RNA_##type_}; \
    wmMsgParams_RNA msg_key_params_ = {{0}}; \
    msg_key_params_.ptr = msg_ptr_; \
\
    WM_msg_subscribe_rna_params(mbus, &msg_key_params_, value, __func__); \
  } \
  ((void)0)
#define WM_msg_subscribe_rna_anon_prop(mbus, type_, prop_, value) \
  { \
    PointerRNA msg_ptr_ = {0, &RNA_##type_}; \
    wmMsgParams_RNA msg_key_params_ = {{0}}; \
    msg_key_params_.ptr = msg_ptr_; \
    msg_key_params_.prop = &rna_##type_##_##prop_; \
\
    WM_msg_subscribe_rna_params(mbus, &msg_key_params_, value, __func__); \
  } \
  ((void)0)
