/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 *
 * Text blocks used for Python-Scripts, OpenShadingLanguage
 * and arbitrary text data to store in blend files.
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_listBase.h"

/** #Text.flags */
enum {
  /** Set if the file in run-time differs from the file on disk, or if there is no file on disk. */
  TXT_ISDIRTY = 1 << 0,
  /** When the text hasn't been written to a file. #Text.filepath may be NULL or invalid. */
  TXT_ISMEM = 1 << 2,
  /** Should always be set if the Text is not to be written into the `.blend`. */
  TXT_ISEXT = 1 << 3,
  /** Load the script as a Python module when loading the `.blend` file. */
  TXT_ISSCRIPT = 1 << 4,

  TXT_FLAG_UNUSED_8 = 1 << 8, /* cleared */
  TXT_FLAG_UNUSED_9 = 1 << 9, /* cleared */

  /** Use space instead of tabs. */
  TXT_TABSTOSPACES = 1 << 10,
};

struct TextLine {
  struct TextLine *next = nullptr, *prev = nullptr;

  char *line = nullptr;
  /** May be NULL if syntax is off or not yet formatted. */
  char *format = nullptr;
  int len = 0;
  char _pad0[4] = {};
};

struct Text {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_TXT;
#endif

  ID id;

  void *_pad1 = nullptr;

  /**
   * Optional file path, when NULL text is considered internal.
   * Otherwise this path will be used when saving/reloading.
   *
   * When set this is where the file will or has been saved.
   */
  char *filepath = nullptr;

  /**
   * Python code object for this text (cached result of #Py_CompileStringObject).
   */
  void *compiled = nullptr;

  int flags = 0;
  char _pad0[4] = {};

  ListBaseT<TextLine> lines = {nullptr, nullptr};
  TextLine *curl = nullptr, *sell = nullptr;
  int curc = 0, selc = 0;

  double mtime = 0;
};

#define TXT_TABSIZE 4
