/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup spbuttons
 */

#pragma once

#include "BLI_bitmap.h"
#include "DNA_listBase.h"
#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct ID;
struct SpaceProperties;
struct Tex;
struct bContext;
struct bContextDataResult;
struct bNode;
struct bNodeSocket;
struct bNodeTree;
struct wmOperatorType;

struct SpaceProperties_Runtime {
  /** For filtering properties displayed in the space. */
  char search_string[UI_MAX_NAME_STR];
  /**
   * Bit-field (in the same order as the tabs) for whether each tab has properties
   * that match the search filter. Only valid when #search_string is set.
   */
  BLI_bitmap *tab_search_results;
};

/* context data */

typedef struct ButsContextPath {
  PointerRNA ptr[8];
  int len;
  int flag;
  int collection_ctx;
} ButsContextPath;

typedef struct ButsTextureUser {
  struct ButsTextureUser *next, *prev;

  struct ID *id;

  PointerRNA ptr;
  PropertyRNA *prop;

  struct bNodeTree *ntree;
  struct bNode *node;
  struct bNodeSocket *socket;

  const char *category;
  int icon;
  const char *name;

  int index;
} ButsTextureUser;

typedef struct ButsContextTexture {
  ListBase users;

  struct Tex *texture;

  struct ButsTextureUser *user;
  int index;
} ButsContextTexture;

/* internal exports only */

/* buttons_context.c */

void buttons_context_compute(const struct bContext *C, struct SpaceProperties *sbuts);
int buttons_context(const struct bContext *C,
                    const char *member,
                    struct bContextDataResult *result);
void buttons_context_register(struct ARegionType *art);
struct ID *buttons_context_id_path(const struct bContext *C);

extern const char *buttons_context_dir[]; /* doc access */

/* buttons_texture.c */

void buttons_texture_context_compute(const struct bContext *C, struct SpaceProperties *sbuts);

/* buttons_ops.c */

void BUTTONS_OT_start_filter(struct wmOperatorType *ot);
void BUTTONS_OT_clear_filter(struct wmOperatorType *ot);
void BUTTONS_OT_toggle_pin(struct wmOperatorType *ot);
void BUTTONS_OT_file_browse(struct wmOperatorType *ot);
/**
 * Second operator, only difference from #BUTTONS_OT_file_browse is #WM_FILESEL_DIRECTORY.
 */
void BUTTONS_OT_directory_browse(struct wmOperatorType *ot);
void BUTTONS_OT_context_menu(struct wmOperatorType *ot);

#ifdef __cplusplus
}
#endif
