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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */
/** \file
 * \ingroup DNA
 *
 * Text blocks used for Python-Scripts, OpenShadingLanguage
 * and arbitrary text data to store in blend files.
 */

#ifndef __DNA_TEXT_TYPES_H__
#define __DNA_TEXT_TYPES_H__

#include "DNA_ID.h"
#include "DNA_listBase.h"

typedef struct TextLine {
  struct TextLine *next, *prev;

  char *line;
  /** May be NULL if syntax is off or not yet formatted. */
  char *format;
  /** Blen unused. */
  int len;
  char _pad0[4];
} TextLine;

typedef struct Text {
  ID id;

  /**
   * Optional file path, when NULL text is considered internal.
   * Otherwise this path will be used when saving/reloading.
   *
   * When set this is where the file will or has been saved.
   */
  char *filepath;

  /**
   * Python code object for this text (cached result of #Py_CompileStringObject).
   */
  void *compiled;

  int flags;
  char _pad0[4];

  ListBase lines;
  TextLine *curl, *sell;
  int curc, selc;

  double mtime;
} Text;

#define TXT_TABSIZE 4

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

#endif /* __DNA_TEXT_TYPES_H__ */
