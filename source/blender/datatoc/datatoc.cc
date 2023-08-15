/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup datatoc
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* #define VERBOSE */

#define MAX2(x, y) ((x) > (y) ? (x) : (y))
#define MAX3(x, y, z) MAX2(MAX2((x), (y)), (z))

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

  return MAX3(string, lfslash, lbslash);
}

int main(int argc, char **argv)
{
  FILE *fpin, *fpout;
  long size;
  int i;
  int argv_len;

  if (argc < 2) {
    printf("Usage: datatoc <data_file_from> <data_file_to>\n");
    exit(1);
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

  fprintf(fpout, "const int datatoc_%s_size = %d;\n", argv[1], int(size));
  fprintf(fpout, "const char datatoc_%s[] = {\n", argv[1]);
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
