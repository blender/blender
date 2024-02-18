/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup datatoc
 */

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

/* #define VERBOSE */

#define STRPREFIX(a, b) (strncmp((a), (b), strlen(b)) == 0)

static char *arg_basename(char *string)
{
  char *lfslash, *lbslash;

  lfslash = strrchr(string, '/');
  lbslash = strrchr(string, '\\');
  if (lbslash) {
    lbslash++;
  }
  if (lfslash) {
    lfslash++;
  }

  return std::max({string, lfslash, lbslash});
}

/**
 * Detect leading C-style comments and seek the file to it's end,
 * returning the number of bytes skipped and setting `r_newlines`
 * or return zero and seek to the file start.
 *
 * The number of newlines is used so the number of lines matches
 * the generated data, so any errors provide useful line numbers
 * (this could be made optional, as there may be cases where it's not helpful).
 */
static int strip_leading_c_comment(FILE *fpin, int size, int *r_newlines)
{
  *r_newlines = 0;
  if (size < 4) {
    return 0;
  }

  enum {
    IS_SPACE = 1,
    IS_COMMENT = 2,
    IS_COMMENT_MAYBE_BEG = 3,
    IS_COMMENT_MAYBE_END = 4,
  } context = IS_SPACE;

  /* Last known valid offset. */
  int offset_checkpoint = 0;
  int newlines_checkpoint = 0;

  int offset = 0;
  int newlines = 0;

  while (offset < size) {
    const char c_curr = getc(fpin);
    offset += 1;

    if (context == IS_SPACE) {
      if (c_curr == ' ' || c_curr == '\t' || c_curr == '\n') {
        /* Pass. */
      }
      else if (c_curr == '/') {
        context = IS_COMMENT_MAYBE_BEG;
      }
      else {
        /* Non-space and non-comment, exit. */
        break;
      }
    }
    else if (context == IS_COMMENT) {
      if (c_curr == '*') {
        context = IS_COMMENT_MAYBE_END;
      }
    }
    else if (context == IS_COMMENT_MAYBE_BEG) {
      if (c_curr == '*') {
        context = IS_COMMENT;
      }
      else {
        /* Non-comment text, exit. */
        break;
      }
    }
    else if (context == IS_COMMENT_MAYBE_END) {
      if (c_curr == '/') {
        context = IS_SPACE;
      }
      else if (c_curr == '*') {
        /* Pass. */
      }
      else {
        context = IS_COMMENT;
      }
    }

    if (c_curr == '\n') {
      newlines += 1;
    }

    if (context == IS_SPACE) {
      offset_checkpoint = offset;
      newlines_checkpoint = newlines;
    }
  }

  if (offset != offset_checkpoint) {
    fseek(fpin, offset_checkpoint, SEEK_SET);
  }
  *r_newlines = newlines_checkpoint;
  return offset_checkpoint;
}

int main(int argc, char **argv)
{
  FILE *fpin, *fpout;
  long size;
  int i;
  int argv_len;

  bool strip_leading_c_comments_test = false;
  int leading_newlines = 0;

  if (argc < 2) {
    printf(
        "Usage: "
        "datatoc <data_file_from> <data_file_to> [--options=strip_leading_c_comments]\n");
    exit(1);
  }

  if (argc > 3) {
    const char *arg_extra = argv[3];
    const char *arg_transform = "--options=";
    if (STRPREFIX(arg_extra, arg_transform)) {
      /* We may want to have other options in the future. */
      const char *options = arg_extra + strlen(arg_transform);
      if (strcmp(options, "strip_leading_c_comments") == 0) {
        strip_leading_c_comments_test = true;
      }
      else {
        printf("Unknown --options=<%s>\n", options);
        exit(1);
      }
    }
    else {
      printf("Unknown argument <%s>, expected --options=[...] or none.\n", arg_extra);
      exit(1);
    }
  }

  fpin = fopen(argv[1], "rb");
  if (!fpin) {
    printf("Unable to open input <%s>\n", argv[1]);
    exit(1);
  }

  argv[1] = arg_basename(argv[1]);

  fseek(fpin, 0L, SEEK_END);
  size = ftell(fpin);
  fseek(fpin, 0L, SEEK_SET);

  if (strip_leading_c_comments_test) {
    const int size_offset = strip_leading_c_comment(fpin, size, &leading_newlines);
    size -= size_offset; /* The comment is skipped, */
  }

  if (argv[1][0] == '.') {
    argv[1]++;
  }

#ifdef VERBOSE
  printf("Making C file <%s>\n", argv[2]);
#endif

  argv_len = int(strlen(argv[1]));
  for (i = 0; i < argv_len; i++) {
    if (argv[1][i] == '.') {
      argv[1][i] = '_';
    }
  }

  fpout = fopen(argv[2], "w");
  if (!fpout) {
    fprintf(stderr, "Unable to open output <%s>\n", argv[2]);
    exit(1);
  }

  fprintf(fpout, "/* DataToC output of file <%s> */\n\n", argv[1]);

  /* Quiet 'missing-variable-declarations' warning. */
  fprintf(fpout, "extern const int datatoc_%s_size;\n", argv[1]);
  fprintf(fpout, "extern const char datatoc_%s[];\n\n", argv[1]);

  fprintf(fpout, "const int datatoc_%s_size = %d;\n", argv[1], int(leading_newlines + size));
  fprintf(fpout, "const char datatoc_%s[] = {\n", argv[1]);

  if (leading_newlines) {
    while (leading_newlines--) {
      if (leading_newlines % 32 == 31) {
        fprintf(fpout, "\n");
      }
      fprintf(fpout, "%3d,", '\n');
    }
    fprintf(fpout, "\n");
  }

  while (size--) {
    /* Even though this file is generated and doesn't need new-lines,
     * these files may be loaded by developers when looking up symbols.
     * Avoid a very long single line that may lock-up some editors. */
    if (size % 32 == 31) {
      fprintf(fpout, "\n");
    }

    // fprintf(fpout, "\\x%02x", getc(fpin));
    fprintf(fpout, "%3d,", getc(fpin));
  }

  /* Trailing nullptr terminator, this isn't needed in some cases and
   * won't be taken into account by the size variable, but its useful when dealing with
   * nullptr terminated string data */
  fprintf(fpout, "0\n};\n\n");

  fclose(fpin);
  fclose(fpout);
  return 0;
}
