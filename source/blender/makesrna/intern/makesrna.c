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

/** \file
 * \ingroup RNA
 */

#include <float.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef _WIN32
#  ifndef snprintf
#    define snprintf _snprintf
#  endif
#endif

#include "CLG_log.h"

static CLG_LogRef LOG = {"makesrna"};

/**
 * Variable to control debug output of makesrna.
 * debugSRNA:
 * - 0 = no output, except errors
 * - 1 = detail actions
 */
static int debugSRNA = 0;

/* stub for BLI_abort() */
#ifndef NDEBUG
void BLI_system_backtrace(FILE *fp)
{
  (void)fp;
}
#endif

/* Replace if different */
#define TMP_EXT ".tmp"

/* copied from BLI_file_older */
#include <sys/stat.h>
static int file_older(const char *file1, const char *file2)
{
  struct stat st1, st2;
  if (debugSRNA > 0) {
    printf("compare: %s %s\n", file1, file2);
  }

  if (stat(file1, &st1)) {
    return 0;
  }
  if (stat(file2, &st2)) {
    return 0;
  }

  return (st1.st_mtime < st2.st_mtime);
}
static const char *makesrna_path = NULL;

/* forward declarations */
static void rna_generate_static_parameter_prototypes(FILE *f,
                                                     StructRNA *srna,
                                                     FunctionDefRNA *dfunc,
                                                     const char *name_override,
                                                     int close_prototype);

/* helpers */
#define WRITE_COMMA \
  { \
    if (!first) \
      fprintf(f, ", "); \
    first = 0; \
  } \
  (void)0

#define WRITE_PARAM(param) \
  { \
    WRITE_COMMA; \
    fprintf(f, param); \
  } \
  (void)0

static int replace_if_different(const char *tmpfile, const char *dep_files[])
{
  /* return 0;  */ /* use for testing had edited rna */

#define REN_IF_DIFF \
  { \
    FILE *file_test = fopen(orgfile, "rb"); \
    if (file_test) { \
      fclose(file_test); \
      if (fp_org) \
        fclose(fp_org); \
      if (fp_new) \
        fclose(fp_new); \
      if (remove(orgfile) != 0) { \
        CLOG_ERROR(&LOG, "remove error (%s): \"%s\"", strerror(errno), orgfile); \
        return -1; \
      } \
    } \
  } \
  if (rename(tmpfile, orgfile) != 0) { \
    CLOG_ERROR(&LOG, "rename error (%s): \"%s\" -> \"%s\"", strerror(errno), tmpfile, orgfile); \
    return -1; \
  } \
  remove(tmpfile); \
  return 1

  /* end REN_IF_DIFF */

  FILE *fp_new = NULL, *fp_org = NULL;
  int len_new, len_org;
  char *arr_new, *arr_org;
  int cmp;

  char orgfile[4096];

  strcpy(orgfile, tmpfile);
  orgfile[strlen(orgfile) - strlen(TMP_EXT)] = '\0'; /* strip '.tmp' */

  fp_org = fopen(orgfile, "rb");

  if (fp_org == NULL) {
    REN_IF_DIFF;
  }

  /* XXX, trick to work around dependency problem
   * assumes dep_files is in the same dir as makesrna.c, which is true for now. */

  if (1) {
    /* first check if makesrna.c is newer then generated files
     * for development on makesrna.c you may want to disable this */
    if (file_older(orgfile, __FILE__)) {
      REN_IF_DIFF;
    }

    if (file_older(orgfile, makesrna_path)) {
      REN_IF_DIFF;
    }

    /* now check if any files we depend on are newer then any generated files */
    if (dep_files) {
      int pass;
      for (pass = 0; dep_files[pass]; pass++) {
        char from_path[4096] = __FILE__;
        char *p1, *p2;

        /* dir only */
        p1 = strrchr(from_path, '/');
        p2 = strrchr(from_path, '\\');
        strcpy((p1 > p2 ? p1 : p2) + 1, dep_files[pass]);
        /* account for build deps, if makesrna.c (this file) is newer */
        if (file_older(orgfile, from_path)) {
          REN_IF_DIFF;
        }
      }
    }
  }
  /* XXX end dep trick */

  fp_new = fopen(tmpfile, "rb");

  if (fp_new == NULL) {
    /* shouldn't happen, just to be safe */
    CLOG_ERROR(&LOG, "open error: \"%s\"", tmpfile);
    fclose(fp_org);
    return -1;
  }

  fseek(fp_new, 0L, SEEK_END);
  len_new = ftell(fp_new);
  fseek(fp_new, 0L, SEEK_SET);
  fseek(fp_org, 0L, SEEK_END);
  len_org = ftell(fp_org);
  fseek(fp_org, 0L, SEEK_SET);

  if (len_new != len_org) {
    fclose(fp_new);
    fp_new = NULL;
    fclose(fp_org);
    fp_org = NULL;
    REN_IF_DIFF;
  }

  /* now compare the files... */
  arr_new = MEM_mallocN(sizeof(char) * len_new, "rna_cmp_file_new");
  arr_org = MEM_mallocN(sizeof(char) * len_org, "rna_cmp_file_org");

  if (fread(arr_new, sizeof(char), len_new, fp_new) != len_new) {
    CLOG_ERROR(&LOG, "unable to read file %s for comparison.", tmpfile);
  }
  if (fread(arr_org, sizeof(char), len_org, fp_org) != len_org) {
    CLOG_ERROR(&LOG, "unable to read file %s for comparison.", orgfile);
  }

  fclose(fp_new);
  fp_new = NULL;
  fclose(fp_org);
  fp_org = NULL;

  cmp = memcmp(arr_new, arr_org, len_new);

  MEM_freeN(arr_new);
  MEM_freeN(arr_org);

  if (cmp) {
    REN_IF_DIFF;
  }
  else {
    remove(tmpfile);
    return 0;
  }

#undef REN_IF_DIFF
}

/* Helper to solve keyword problems with C/C++ */

static const char *rna_safe_id(const char *id)
{
  if (STREQ(id, "default")) {
    return "default_value";
  }
  else if (STREQ(id, "operator")) {
    return "operator_value";
  }
  else if (STREQ(id, "new")) {
    return "create";
  }
  else if (STREQ(id, "co_return")) {
    /* MSVC2015, C++ uses for coroutines */
    return "coord_return";
  }

  return id;
}

/* Sorting */

static int cmp_struct(const void *a, const void *b)
{
  const StructRNA *structa = *(const StructRNA **)a;
  const StructRNA *structb = *(const StructRNA **)b;

  return strcmp(structa->identifier, structb->identifier);
}

static int cmp_property(const void *a, const void *b)
{
  const PropertyRNA *propa = *(const PropertyRNA **)a;
  const PropertyRNA *propb = *(const PropertyRNA **)b;

  if (STREQ(propa->identifier, "rna_type")) {
    return -1;
  }
  else if (STREQ(propb->identifier, "rna_type")) {
    return 1;
  }

  if (STREQ(propa->identifier, "name")) {
    return -1;
  }
  else if (STREQ(propb->identifier, "name")) {
    return 1;
  }

  return strcmp(propa->name, propb->name);
}

static int cmp_def_struct(const void *a, const void *b)
{
  const StructDefRNA *dsa = *(const StructDefRNA **)a;
  const StructDefRNA *dsb = *(const StructDefRNA **)b;

  return cmp_struct(&dsa->srna, &dsb->srna);
}

static int cmp_def_property(const void *a, const void *b)
{
  const PropertyDefRNA *dpa = *(const PropertyDefRNA **)a;
  const PropertyDefRNA *dpb = *(const PropertyDefRNA **)b;

  return cmp_property(&dpa->prop, &dpb->prop);
}

static void rna_sortlist(ListBase *listbase, int (*cmp)(const void *, const void *))
{
  Link *link;
  void **array;
  int a, size;

  if (listbase->first == listbase->last) {
    return;
  }

  for (size = 0, link = listbase->first; link; link = link->next) {
    size++;
  }

  array = MEM_mallocN(sizeof(void *) * size, "rna_sortlist");
  for (a = 0, link = listbase->first; link; link = link->next, a++) {
    array[a] = link;
  }

  qsort(array, size, sizeof(void *), cmp);

  listbase->first = listbase->last = NULL;
  for (a = 0; a < size; a++) {
    link = array[a];
    link->next = link->prev = NULL;
    rna_addtail(listbase, link);
  }

  MEM_freeN(array);
}

/* Preprocessing */

static void rna_print_c_string(FILE *f, const char *str)
{
  static const char *escape[] = {
      "\''", "\"\"", "\??", "\\\\", "\aa", "\bb", "\ff", "\nn", "\rr", "\tt", "\vv", NULL};
  int i, j;

  if (!str) {
    fprintf(f, "NULL");
    return;
  }

  fprintf(f, "\"");
  for (i = 0; str[i]; i++) {
    for (j = 0; escape[j]; j++) {
      if (str[i] == escape[j][0]) {
        break;
      }
    }

    if (escape[j]) {
      fprintf(f, "\\%c", escape[j][1]);
    }
    else {
      fprintf(f, "%c", str[i]);
    }
  }
  fprintf(f, "\"");
}

static void rna_print_data_get(FILE *f, PropertyDefRNA *dp)
{
  if (dp->dnastructfromname && dp->dnastructfromprop) {
    fprintf(f,
            "    %s *data = (%s *)(((%s *)ptr->data)->%s);\n",
            dp->dnastructname,
            dp->dnastructname,
            dp->dnastructfromname,
            dp->dnastructfromprop);
  }
  else {
    fprintf(f, "    %s *data = (%s *)(ptr->data);\n", dp->dnastructname, dp->dnastructname);
  }
}

static void rna_print_id_get(FILE *f, PropertyDefRNA *UNUSED(dp))
{
  fprintf(f, "    ID *id = ptr->id.data;\n");
}

static void rna_construct_function_name(
    char *buffer, int size, const char *structname, const char *propname, const char *type)
{
  snprintf(buffer, size, "%s_%s_%s", structname, propname, type);
}

static void rna_construct_wrapper_function_name(
    char *buffer, int size, const char *structname, const char *propname, const char *type)
{
  if (type == NULL || type[0] == '\0') {
    snprintf(buffer, size, "%s_%s", structname, propname);
  }
  else {
    snprintf(buffer, size, "%s_%s_%s", structname, propname, type);
  }
}

static char *rna_alloc_function_name(const char *structname,
                                     const char *propname,
                                     const char *type)
{
  AllocDefRNA *alloc;
  char buffer[2048];
  char *result;

  rna_construct_function_name(buffer, sizeof(buffer), structname, propname, type);
  result = MEM_callocN(sizeof(char) * strlen(buffer) + 1, "rna_alloc_function_name");
  strcpy(result, buffer);

  alloc = MEM_callocN(sizeof(AllocDefRNA), "AllocDefRNA");
  alloc->mem = result;
  rna_addtail(&DefRNA.allocs, alloc);

  return result;
}

static StructRNA *rna_find_struct(const char *identifier)
{
  StructDefRNA *ds;

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (STREQ(ds->srna->identifier, identifier)) {
      return ds->srna;
    }
  }

  return NULL;
}

static const char *rna_find_type(const char *type)
{
  StructDefRNA *ds;

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (ds->dnaname && STREQ(ds->dnaname, type)) {
      return ds->srna->identifier;
    }
  }

  return NULL;
}

static const char *rna_find_dna_type(const char *type)
{
  StructDefRNA *ds;

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (STREQ(ds->srna->identifier, type)) {
      return ds->dnaname;
    }
  }

  return NULL;
}

static const char *rna_type_type_name(PropertyRNA *prop)
{
  switch (prop->type) {
    case PROP_BOOLEAN:
      return "bool";
    case PROP_INT:
    case PROP_ENUM:
      return "int";
    case PROP_FLOAT:
      return "float";
    case PROP_STRING:
      if (prop->flag & PROP_THICK_WRAP) {
        return "char *";
      }
      else {
        return "const char *";
      }
    default:
      return NULL;
  }
}

static const char *rna_type_type(PropertyRNA *prop)
{
  const char *type;

  type = rna_type_type_name(prop);

  if (type) {
    return type;
  }

  return "PointerRNA";
}

static const char *rna_type_struct(PropertyRNA *prop)
{
  const char *type;

  type = rna_type_type_name(prop);

  if (type) {
    return "";
  }

  return "struct ";
}

static const char *rna_parameter_type_name(PropertyRNA *parm)
{
  const char *type;

  type = rna_type_type_name(parm);

  if (type) {
    return type;
  }

  switch (parm->type) {
    case PROP_POINTER: {
      PointerPropertyRNA *pparm = (PointerPropertyRNA *)parm;

      if (parm->flag_parameter & PARM_RNAPTR) {
        return "PointerRNA";
      }
      else {
        return rna_find_dna_type((const char *)pparm->type);
      }
    }
    case PROP_COLLECTION: {
      return "CollectionListBase";
    }
    default:
      return "<error, no type specified>";
  }
}

static int rna_enum_bitmask(PropertyRNA *prop)
{
  EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
  int a, mask = 0;

  if (eprop->item) {
    for (a = 0; a < eprop->totitem; a++) {
      if (eprop->item[a].identifier[0]) {
        mask |= eprop->item[a].value;
      }
    }
  }

  return mask;
}

static int rna_color_quantize(PropertyRNA *prop, PropertyDefRNA *dp)
{
  return ((prop->type == PROP_FLOAT) &&
          (prop->subtype == PROP_COLOR || prop->subtype == PROP_COLOR_GAMMA) &&
          (IS_DNATYPE_FLOAT_COMPAT(dp->dnatype) == 0));
}

static const char *rna_function_string(void *func)
{
  return (func) ? (const char *)func : "NULL";
}

static void rna_float_print(FILE *f, float num)
{
  if (num == -FLT_MAX) {
    fprintf(f, "-FLT_MAX");
  }
  else if (num == FLT_MAX) {
    fprintf(f, "FLT_MAX");
  }
  else if ((ABS(num) < INT64_MAX) && ((int64_t)num == num)) {
    fprintf(f, "%.1ff", num);
  }
  else {
    fprintf(f, "%.10ff", num);
  }
}

static void rna_int_print(FILE *f, int num)
{
  if (num == INT_MIN) {
    fprintf(f, "INT_MIN");
  }
  else if (num == INT_MAX) {
    fprintf(f, "INT_MAX");
  }
  else {
    fprintf(f, "%d", num);
  }
}

static char *rna_def_property_get_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
  char *func;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
      DefRNA.error = 1;
      return NULL;
    }

    /* typecheck,  */
    if (dp->dnatype && *dp->dnatype) {

      if (prop->type == PROP_FLOAT) {
        if (IS_DNATYPE_FLOAT_COMPAT(dp->dnatype) == 0) {
          if (prop->subtype !=
              PROP_COLOR_GAMMA) { /* colors are an exception. these get translated */
            CLOG_ERROR(&LOG,
                       "%s.%s is a '%s' but wrapped as type '%s'.",
                       srna->identifier,
                       prop->identifier,
                       dp->dnatype,
                       RNA_property_typename(prop->type));
            DefRNA.error = 1;
            return NULL;
          }
        }
      }
      else if (prop->type == PROP_INT || prop->type == PROP_BOOLEAN || prop->type == PROP_ENUM) {
        if (IS_DNATYPE_INT_COMPAT(dp->dnatype) == 0) {
          CLOG_ERROR(&LOG,
                     "%s.%s is a '%s' but wrapped as type '%s'.",
                     srna->identifier,
                     prop->identifier,
                     dp->dnatype,
                     RNA_property_typename(prop->type));
          DefRNA.error = 1;
          return NULL;
        }
      }
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

  switch (prop->type) {
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
      fprintf(f, "void %s(PointerRNA *ptr, char *value)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    %s(ptr, value);\n", manualfunc);
      }
      else {
        const PropertySubType subtype = prop->subtype;
        const char *string_copy_func = (subtype == PROP_FILEPATH || subtype == PROP_DIRPATH ||
                                        subtype == PROP_FILENAME || subtype == PROP_BYTESTRING) ?
                                           "BLI_strncpy" :
                                           "BLI_strncpy_utf8";

        rna_print_data_get(f, dp);

        if (!(prop->flag & PROP_NEVER_NULL)) {
          fprintf(f, "    if (data->%s == NULL) {\n", dp->dnaname);
          fprintf(f, "        *value = '\\0';\n");
          fprintf(f, "        return;\n");
          fprintf(f, "    }\n");
        }

        if (sprop->maxlength) {
          fprintf(f,
                  "    %s(value, data->%s, %d);\n",
                  string_copy_func,
                  dp->dnaname,
                  sprop->maxlength);
        }
        else {
          fprintf(f,
                  "    %s(value, data->%s, sizeof(data->%s));\n",
                  string_copy_func,
                  dp->dnaname,
                  dp->dnaname);
        }
      }
      fprintf(f, "}\n\n");
      break;
    }
    case PROP_POINTER: {
      fprintf(f, "PointerRNA %s(PointerRNA *ptr)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    return %s(ptr);\n", manualfunc);
      }
      else {
        PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
        rna_print_data_get(f, dp);
        if (dp->dnapointerlevel == 0) {
          fprintf(f,
                  "    return rna_pointer_inherit_refine(ptr, &RNA_%s, &data->%s);\n",
                  (const char *)pprop->type,
                  dp->dnaname);
        }
        else {
          fprintf(f,
                  "    return rna_pointer_inherit_refine(ptr, &RNA_%s, data->%s);\n",
                  (const char *)pprop->type,
                  dp->dnaname);
        }
      }
      fprintf(f, "}\n\n");
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

      fprintf(f, "static PointerRNA %s(CollectionPropertyIterator *iter)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        if (STREQ(manualfunc, "rna_iterator_listbase_get") ||
            STREQ(manualfunc, "rna_iterator_array_get") ||
            STREQ(manualfunc, "rna_iterator_array_dereference_get")) {
          fprintf(f,
                  "    return rna_pointer_inherit_refine(&iter->parent, &RNA_%s, %s(iter));\n",
                  (cprop->item_type) ? (const char *)cprop->item_type : "UnknownType",
                  manualfunc);
        }
        else {
          fprintf(f, "    return %s(iter);\n", manualfunc);
        }
      }
      fprintf(f, "}\n\n");
      break;
    }
    default:
      if (prop->arraydimension) {
        if (prop->flag & PROP_DYNAMIC) {
          fprintf(f, "void %s(PointerRNA *ptr, %s values[])\n", func, rna_type_type(prop));
        }
        else {
          fprintf(f,
                  "void %s(PointerRNA *ptr, %s values[%u])\n",
                  func,
                  rna_type_type(prop),
                  prop->totarraylength);
        }
        fprintf(f, "{\n");

        if (manualfunc) {
          fprintf(f, "    %s(ptr, values);\n", manualfunc);
        }
        else {
          rna_print_data_get(f, dp);

          if (prop->flag & PROP_DYNAMIC) {
            char *lenfunc = rna_alloc_function_name(
                srna->identifier, rna_safe_id(prop->identifier), "get_length");
            fprintf(f, "    unsigned int arraylen[RNA_MAX_ARRAY_DIMENSION];\n");
            fprintf(f, "    unsigned int i;\n");
            fprintf(f, "    unsigned int len = %s(ptr, arraylen);\n\n", lenfunc);
            fprintf(f, "    for (i = 0; i < len; i++) {\n");
            MEM_freeN(lenfunc);
          }
          else {
            fprintf(f, "    unsigned int i;\n\n");
            fprintf(f, "    for (i = 0; i < %u; i++) {\n", prop->totarraylength);
          }

          if (dp->dnaarraylength == 1) {
            if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
              fprintf(f,
                      "        values[i] = %s((data->%s & (%du << i)) != 0);\n",
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname,
                      dp->booleanbit);
            }
            else {
              fprintf(f,
                      "        values[i] = (%s)%s((&data->%s)[i]);\n",
                      rna_type_type(prop),
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname);
            }
          }
          else {
            if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
              fprintf(f,
                      "        values[i] = %s((data->%s[i] & ",
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, ") != 0);\n");
            }
            else if (rna_color_quantize(prop, dp)) {
              fprintf(f,
                      "        values[i] = (%s)(data->%s[i] * (1.0f / 255.0f));\n",
                      rna_type_type(prop),
                      dp->dnaname);
            }
            else if (dp->dnatype) {
              fprintf(f,
                      "        values[i] = (%s)%s(((%s *)data->%s)[i]);\n",
                      rna_type_type(prop),
                      (dp->booleannegative) ? "!" : "",
                      dp->dnatype,
                      dp->dnaname);
            }
            else {
              fprintf(f,
                      "        values[i] = (%s)%s((data->%s)[i]);\n",
                      rna_type_type(prop),
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname);
            }
          }
          fprintf(f, "    }\n");
        }
        fprintf(f, "}\n\n");
      }
      else {
        fprintf(f, "%s %s(PointerRNA *ptr)\n", rna_type_type(prop), func);
        fprintf(f, "{\n");

        if (manualfunc) {
          fprintf(f, "    return %s(ptr);\n", manualfunc);
        }
        else {
          rna_print_data_get(f, dp);
          if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
            fprintf(
                f, "    return %s(((data->%s) & ", (dp->booleannegative) ? "!" : "", dp->dnaname);
            rna_int_print(f, dp->booleanbit);
            fprintf(f, ") != 0);\n");
          }
          else if (prop->type == PROP_ENUM && dp->enumbitflags) {
            fprintf(f, "    return ((data->%s) & ", dp->dnaname);
            rna_int_print(f, rna_enum_bitmask(prop));
            fprintf(f, ");\n");
          }
          else {
            fprintf(f,
                    "    return (%s)%s(data->%s);\n",
                    rna_type_type(prop),
                    (dp->booleannegative) ? "!" : "",
                    dp->dnaname);
          }
        }

        fprintf(f, "}\n\n");
      }
      break;
  }

  return func;
}

/* defined min/max variables to be used by rna_clamp_value() */
static void rna_clamp_value_range(FILE *f, PropertyRNA *prop)
{
  if (prop->type == PROP_FLOAT) {
    FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
    if (fprop->range) {
      fprintf(f,
              "    float prop_clamp_min = -FLT_MAX, prop_clamp_max = FLT_MAX, prop_soft_min, "
              "prop_soft_max;\n");
      fprintf(f,
              "    %s(ptr, &prop_clamp_min, &prop_clamp_max, &prop_soft_min, &prop_soft_max);\n",
              rna_function_string(fprop->range));
    }
  }
  else if (prop->type == PROP_INT) {
    IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
    if (iprop->range) {
      fprintf(f,
              "    int prop_clamp_min = INT_MIN, prop_clamp_max = INT_MAX, prop_soft_min, "
              "prop_soft_max;\n");
      fprintf(f,
              "    %s(ptr, &prop_clamp_min, &prop_clamp_max, &prop_soft_min, &prop_soft_max);\n",
              rna_function_string(iprop->range));
    }
  }
}

#ifdef USE_RNA_RANGE_CHECK
static void rna_clamp_value_range_check(FILE *f,
                                        PropertyRNA *prop,
                                        const char *dnaname_prefix,
                                        const char *dnaname)
{
  if (prop->type == PROP_INT) {
    IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
    fprintf(f,
            "    { BLI_STATIC_ASSERT("
            "(TYPEOF_MAX(%s%s) >= %d) && "
            "(TYPEOF_MIN(%s%s) <= %d), "
            "\"invalid limits\"); }\n",
            dnaname_prefix,
            dnaname,
            iprop->hardmax,
            dnaname_prefix,
            dnaname,
            iprop->hardmin);
  }
}
#endif /* USE_RNA_RANGE_CHECK */

static void rna_clamp_value(FILE *f, PropertyRNA *prop, int array)
{
  if (prop->type == PROP_INT) {
    IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

    if (iprop->hardmin != INT_MIN || iprop->hardmax != INT_MAX || iprop->range) {
      if (array) {
        fprintf(f, "CLAMPIS(values[i], ");
      }
      else {
        fprintf(f, "CLAMPIS(value, ");
      }
      if (iprop->range) {
        fprintf(f, "prop_clamp_min, prop_clamp_max);");
      }
      else {
        rna_int_print(f, iprop->hardmin);
        fprintf(f, ", ");
        rna_int_print(f, iprop->hardmax);
        fprintf(f, ");\n");
      }
      return;
    }
  }
  else if (prop->type == PROP_FLOAT) {
    FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

    if (fprop->hardmin != -FLT_MAX || fprop->hardmax != FLT_MAX || fprop->range) {
      if (array) {
        fprintf(f, "CLAMPIS(values[i], ");
      }
      else {
        fprintf(f, "CLAMPIS(value, ");
      }
      if (fprop->range) {
        fprintf(f, "prop_clamp_min, prop_clamp_max);");
      }
      else {
        rna_float_print(f, fprop->hardmin);
        fprintf(f, ", ");
        rna_float_print(f, fprop->hardmax);
        fprintf(f, ");\n");
      }
      return;
    }
  }

  if (array) {
    fprintf(f, "values[i];\n");
  }
  else {
    fprintf(f, "value;\n");
  }
}

static char *rna_def_property_set_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
  char *func;

  if (!(prop->flag & PROP_EDITABLE)) {
    return NULL;
  }
  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      if (prop->flag & PROP_EDITABLE) {
        CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
        DefRNA.error = 1;
      }
      return NULL;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "set");

  switch (prop->type) {
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
      fprintf(f, "void %s(PointerRNA *ptr, const char *value)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    %s(ptr, value);\n", manualfunc);
      }
      else {
        const PropertySubType subtype = prop->subtype;
        const char *string_copy_func = (subtype == PROP_FILEPATH || subtype == PROP_DIRPATH ||
                                        subtype == PROP_FILENAME || subtype == PROP_BYTESTRING) ?
                                           "BLI_strncpy" :
                                           "BLI_strncpy_utf8";

        rna_print_data_get(f, dp);

        if (!(prop->flag & PROP_NEVER_NULL)) {
          fprintf(f, "    if (data->%s == NULL) {\n", dp->dnaname);
          fprintf(f, "        return;\n");
          fprintf(f, "    }\n");
        }

        if (sprop->maxlength) {
          fprintf(f,
                  "    %s(data->%s, value, %d);\n",
                  string_copy_func,
                  dp->dnaname,
                  sprop->maxlength);
        }
        else {
          fprintf(f,
                  "    %s(data->%s, value, sizeof(data->%s));\n",
                  string_copy_func,
                  dp->dnaname,
                  dp->dnaname);
        }
      }
      fprintf(f, "}\n\n");
      break;
    }
    case PROP_POINTER: {
      fprintf(f, "void %s(PointerRNA *ptr, PointerRNA value)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    %s(ptr, value);\n", manualfunc);
      }
      else {
        rna_print_data_get(f, dp);

        if (prop->flag & PROP_ID_SELF_CHECK) {
          rna_print_id_get(f, dp);
          fprintf(f, "    if (id == value.data) return;\n\n");
        }

        if (prop->flag & PROP_ID_REFCOUNT) {
          fprintf(f, "\n    if (data->%s)\n", dp->dnaname);
          fprintf(f, "        id_us_min((ID *)data->%s);\n", dp->dnaname);
          fprintf(f, "    if (value.data)\n");
          fprintf(f, "        id_us_plus((ID *)value.data);\n\n");
        }
        else {
          PointerPropertyRNA *pprop = (PointerPropertyRNA *)dp->prop;
          StructRNA *type = (pprop->type) ? rna_find_struct((const char *)pprop->type) : NULL;
          if (type && (type->flag & STRUCT_ID)) {
            fprintf(f, "    if (value.data)\n");
            fprintf(f, "        id_lib_extern((ID *)value.data);\n\n");
          }
        }

        fprintf(f, "    data->%s = value.data;\n", dp->dnaname);
      }
      fprintf(f, "}\n\n");
      break;
    }
    default:
      if (prop->arraydimension) {
        if (prop->flag & PROP_DYNAMIC) {
          fprintf(f, "void %s(PointerRNA *ptr, const %s values[])\n", func, rna_type_type(prop));
        }
        else {
          fprintf(f,
                  "void %s(PointerRNA *ptr, const %s values[%u])\n",
                  func,
                  rna_type_type(prop),
                  prop->totarraylength);
        }
        fprintf(f, "{\n");

        if (manualfunc) {
          fprintf(f, "    %s(ptr, values);\n", manualfunc);
        }
        else {
          rna_print_data_get(f, dp);

          if (prop->flag & PROP_DYNAMIC) {
            char *lenfunc = rna_alloc_function_name(
                srna->identifier, rna_safe_id(prop->identifier), "set_length");
            fprintf(f, "    unsigned int i, arraylen[RNA_MAX_ARRAY_DIMENSION];\n");
            fprintf(f, "    unsigned int len = %s(ptr, arraylen);\n\n", lenfunc);
            rna_clamp_value_range(f, prop);
            fprintf(f, "    for (i = 0; i < len; i++) {\n");
            MEM_freeN(lenfunc);
          }
          else {
            fprintf(f, "    unsigned int i;\n\n");
            rna_clamp_value_range(f, prop);
            fprintf(f, "    for (i = 0; i < %u; i++) {\n", prop->totarraylength);
          }

          if (dp->dnaarraylength == 1) {
            if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
              fprintf(f,
                      "        if (%svalues[i]) data->%s |= (%du << i);\n",
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname,
                      dp->booleanbit);
              fprintf(f, "        else data->%s &= ~(%du << i);\n", dp->dnaname, dp->booleanbit);
            }
            else {
              fprintf(
                  f, "        (&data->%s)[i] = %s", dp->dnaname, (dp->booleannegative) ? "!" : "");
              rna_clamp_value(f, prop, 1);
            }
          }
          else {
            if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
              fprintf(f,
                      "        if (%svalues[i]) data->%s[i] |= ",
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, ";\n");
              fprintf(f, "        else data->%s[i] &= ~", dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, ";\n");
            }
            else if (rna_color_quantize(prop, dp)) {
              fprintf(
                  f, "        data->%s[i] = unit_float_to_uchar_clamp(values[i]);\n", dp->dnaname);
            }
            else {
              if (dp->dnatype) {
                fprintf(f,
                        "        ((%s *)data->%s)[i] = %s",
                        dp->dnatype,
                        dp->dnaname,
                        (dp->booleannegative) ? "!" : "");
              }
              else {
                fprintf(f,
                        "        (data->%s)[i] = %s",
                        dp->dnaname,
                        (dp->booleannegative) ? "!" : "");
              }
              rna_clamp_value(f, prop, 1);
            }
          }
          fprintf(f, "    }\n");
        }

#ifdef USE_RNA_RANGE_CHECK
        if (dp->dnaname && manualfunc == NULL) {
          if (dp->dnaarraylength == 1) {
            rna_clamp_value_range_check(f, prop, "data->", dp->dnaname);
          }
          else {
            rna_clamp_value_range_check(f, prop, "*data->", dp->dnaname);
          }
        }
#endif

        fprintf(f, "}\n\n");
      }
      else {
        fprintf(f, "void %s(PointerRNA *ptr, %s value)\n", func, rna_type_type(prop));
        fprintf(f, "{\n");

        if (manualfunc) {
          fprintf(f, "    %s(ptr, value);\n", manualfunc);
        }
        else {
          rna_print_data_get(f, dp);
          if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
            fprintf(
                f, "    if (%svalue) data->%s |= ", (dp->booleannegative) ? "!" : "", dp->dnaname);
            rna_int_print(f, dp->booleanbit);
            fprintf(f, ";\n");
            fprintf(f, "    else data->%s &= ~", dp->dnaname);
            rna_int_print(f, dp->booleanbit);
            fprintf(f, ";\n");
          }
          else if (prop->type == PROP_ENUM && dp->enumbitflags) {
            fprintf(f, "    data->%s &= ~", dp->dnaname);
            rna_int_print(f, rna_enum_bitmask(prop));
            fprintf(f, ";\n");
            fprintf(f, "    data->%s |= value;\n", dp->dnaname);
          }
          else {
            rna_clamp_value_range(f, prop);
            fprintf(f, "    data->%s = %s", dp->dnaname, (dp->booleannegative) ? "!" : "");
            rna_clamp_value(f, prop, 0);
          }
        }

#ifdef USE_RNA_RANGE_CHECK
        if (dp->dnaname && manualfunc == NULL) {
          rna_clamp_value_range_check(f, prop, "data->", dp->dnaname);
        }
#endif

        fprintf(f, "}\n\n");
      }
      break;
  }

  return func;
}

static char *rna_def_property_length_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
  char *func = NULL;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  if (prop->type == PROP_STRING) {
    if (!manualfunc) {
      if (!dp->dnastructname || !dp->dnaname) {
        CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
        DefRNA.error = 1;
        return NULL;
      }
    }

    func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "length");

    fprintf(f, "int %s(PointerRNA *ptr)\n", func);
    fprintf(f, "{\n");
    if (manualfunc) {
      fprintf(f, "    return %s(ptr);\n", manualfunc);
    }
    else {
      rna_print_data_get(f, dp);
      if (!(prop->flag & PROP_NEVER_NULL)) {
        fprintf(f, "    if (data->%s == NULL) return 0;\n", dp->dnaname);
      }
      fprintf(f, "    return strlen(data->%s);\n", dp->dnaname);
    }
    fprintf(f, "}\n\n");
  }
  else if (prop->type == PROP_COLLECTION) {
    if (!manualfunc) {
      if (prop->type == PROP_COLLECTION &&
          (!(dp->dnalengthname || dp->dnalengthfixed) || !dp->dnaname)) {
        CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
        DefRNA.error = 1;
        return NULL;
      }
    }

    func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "length");

    fprintf(f, "int %s(PointerRNA *ptr)\n", func);
    fprintf(f, "{\n");
    if (manualfunc) {
      fprintf(f, "    return %s(ptr);\n", manualfunc);
    }
    else {
      if (dp->dnaarraylength <= 1 || dp->dnalengthname) {
        rna_print_data_get(f, dp);
      }

      if (dp->dnaarraylength > 1) {
        fprintf(f, "    return ");
      }
      else {
        fprintf(f, "    return (data->%s == NULL) ? 0 : ", dp->dnaname);
      }

      if (dp->dnalengthname) {
        fprintf(f, "data->%s;\n", dp->dnalengthname);
      }
      else {
        fprintf(f, "%d;\n", dp->dnalengthfixed);
      }
    }
    fprintf(f, "}\n\n");
  }

  return func;
}

static char *rna_def_property_begin_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
  char *func, *getfunc;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
      DefRNA.error = 1;
      return NULL;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "begin");

  fprintf(f, "void %s(CollectionPropertyIterator *iter, PointerRNA *ptr)\n", func);
  fprintf(f, "{\n");

  if (!manualfunc) {
    rna_print_data_get(f, dp);
  }

  fprintf(f, "\n    memset(iter, 0, sizeof(*iter));\n");
  fprintf(f, "    iter->parent = *ptr;\n");
  fprintf(f, "    iter->prop = (PropertyRNA *)&rna_%s_%s;\n", srna->identifier, prop->identifier);

  if (dp->dnalengthname || dp->dnalengthfixed) {
    if (manualfunc) {
      fprintf(f, "\n    %s(iter, ptr);\n", manualfunc);
    }
    else {
      if (dp->dnalengthname) {
        fprintf(f,
                "\n    rna_iterator_array_begin(iter, data->%s, sizeof(data->%s[0]), data->%s, 0, "
                "NULL);\n",
                dp->dnaname,
                dp->dnaname,
                dp->dnalengthname);
      }
      else {
        fprintf(
            f,
            "\n    rna_iterator_array_begin(iter, data->%s, sizeof(data->%s[0]), %d, 0, NULL);\n",
            dp->dnaname,
            dp->dnaname,
            dp->dnalengthfixed);
      }
    }
  }
  else {
    if (manualfunc) {
      fprintf(f, "\n    %s(iter, ptr);\n", manualfunc);
    }
    else if (dp->dnapointerlevel == 0) {
      fprintf(f, "\n    rna_iterator_listbase_begin(iter, &data->%s, NULL);\n", dp->dnaname);
    }
    else {
      fprintf(f, "\n    rna_iterator_listbase_begin(iter, data->%s, NULL);\n", dp->dnaname);
    }
  }

  getfunc = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

  fprintf(f, "\n    if (iter->valid)\n");
  fprintf(f, "        iter->ptr = %s(iter);\n", getfunc);

  fprintf(f, "}\n\n");

  return func;
}

static char *rna_def_property_lookup_int_func(FILE *f,
                                              StructRNA *srna,
                                              PropertyRNA *prop,
                                              PropertyDefRNA *dp,
                                              const char *manualfunc,
                                              const char *nextfunc)
{
  /* note on indices, this is for external functions and ignores skipped values.
   * so the index can only be checked against the length when there is no 'skip' function. */
  char *func;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      return NULL;
    }

    /* only supported in case of standard next functions */
    if (STREQ(nextfunc, "rna_iterator_array_next")) {
    }
    else if (STREQ(nextfunc, "rna_iterator_listbase_next")) {
    }
    else {
      return NULL;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "lookup_int");

  fprintf(f, "int %s(PointerRNA *ptr, int index, PointerRNA *r_ptr)\n", func);
  fprintf(f, "{\n");

  if (manualfunc) {
    fprintf(f, "\n    return %s(ptr, index, r_ptr);\n", manualfunc);
    fprintf(f, "}\n\n");
    return func;
  }

  fprintf(f, "    int found = 0;\n");
  fprintf(f, "    CollectionPropertyIterator iter;\n\n");

  fprintf(f, "    %s_%s_begin(&iter, ptr);\n\n", srna->identifier, rna_safe_id(prop->identifier));
  fprintf(f, "    if (iter.valid) {\n");

  if (STREQ(nextfunc, "rna_iterator_array_next")) {
    fprintf(f, "        ArrayIterator *internal = &iter.internal.array;\n");
    fprintf(f, "        if (index < 0 || index >= internal->length) {\n");
    fprintf(f, "#ifdef __GNUC__\n");
    fprintf(f,
            "            printf(\"Array iterator out of range: %%s (index %%d)\\n\", __func__, "
            "index);\n");
    fprintf(f, "#else\n");
    fprintf(f, "            printf(\"Array iterator out of range: (index %%d)\\n\", index);\n");
    fprintf(f, "#endif\n");
    fprintf(f, "        }\n");
    fprintf(f, "        else if (internal->skip) {\n");
    fprintf(f, "            while (index-- > 0 && iter.valid) {\n");
    fprintf(f, "                rna_iterator_array_next(&iter);\n");
    fprintf(f, "            }\n");
    fprintf(f, "            found = (index == -1 && iter.valid);\n");
    fprintf(f, "        }\n");
    fprintf(f, "        else {\n");
    fprintf(f, "            internal->ptr += internal->itemsize * index;\n");
    fprintf(f, "            found = 1;\n");
    fprintf(f, "        }\n");
  }
  else if (STREQ(nextfunc, "rna_iterator_listbase_next")) {
    fprintf(f, "        ListBaseIterator *internal = &iter.internal.listbase;\n");
    fprintf(f, "        if (internal->skip) {\n");
    fprintf(f, "            while (index-- > 0 && iter.valid) {\n");
    fprintf(f, "                rna_iterator_listbase_next(&iter);\n");
    fprintf(f, "            }\n");
    fprintf(f, "            found = (index == -1 && iter.valid);\n");
    fprintf(f, "        }\n");
    fprintf(f, "        else {\n");
    fprintf(f, "            while (index-- > 0 && internal->link)\n");
    fprintf(f, "                internal->link = internal->link->next;\n");
    fprintf(f, "            found = (index == -1 && internal->link);\n");
    fprintf(f, "        }\n");
  }

  fprintf(f,
          "        if (found) *r_ptr = %s_%s_get(&iter);\n",
          srna->identifier,
          rna_safe_id(prop->identifier));
  fprintf(f, "    }\n\n");
  fprintf(f, "    %s_%s_end(&iter);\n\n", srna->identifier, rna_safe_id(prop->identifier));

  fprintf(f, "    return found;\n");

#if 0
  rna_print_data_get(f, dp);
  item_type = (cprop->item_type) ? (const char *)cprop->item_type : "UnknownType";

  if (dp->dnalengthname || dp->dnalengthfixed) {
    if (dp->dnalengthname)
      fprintf(f,
              "\n    rna_array_lookup_int(ptr, &RNA_%s, data->%s, sizeof(data->%s[0]), data->%s, "
              "index);\n",
              item_type,
              dp->dnaname,
              dp->dnaname,
              dp->dnalengthname);
    else
      fprintf(
          f,
          "\n    rna_array_lookup_int(ptr, &RNA_%s, data->%s, sizeof(data->%s[0]), %d, index);\n",
          item_type,
          dp->dnaname,
          dp->dnaname,
          dp->dnalengthfixed);
  }
  else {
    if (dp->dnapointerlevel == 0)
      fprintf(f,
              "\n    return rna_listbase_lookup_int(ptr, &RNA_%s, &data->%s, index);\n",
              item_type,
              dp->dnaname);
    else
      fprintf(f,
              "\n    return rna_listbase_lookup_int(ptr, &RNA_%s, data->%s, index);\n",
              item_type,
              dp->dnaname);
  }
#endif

  fprintf(f, "}\n\n");

  return func;
}

static char *rna_def_property_lookup_string_func(FILE *f,
                                                 StructRNA *srna,
                                                 PropertyRNA *prop,
                                                 PropertyDefRNA *dp,
                                                 const char *manualfunc,
                                                 const char *item_type)
{
  char *func;
  StructRNA *item_srna, *item_name_base;
  PropertyRNA *item_name_prop;
  const int namebuflen = 1024;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      return NULL;
    }

    /* only supported for collection items with name properties */
    item_srna = rna_find_struct(item_type);
    if (item_srna && item_srna->nameproperty) {
      item_name_prop = item_srna->nameproperty;
      item_name_base = item_srna;
      while (item_name_base->base && item_name_base->base->nameproperty == item_name_prop) {
        item_name_base = item_name_base->base;
      }
    }
    else {
      return NULL;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "lookup_string");

  fprintf(f, "int %s(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)\n", func);
  fprintf(f, "{\n");

  if (manualfunc) {
    fprintf(f, "    return %s(ptr, key, r_ptr);\n", manualfunc);
    fprintf(f, "}\n\n");
    return func;
  }

  /* XXX extern declaration could be avoid by including RNA_blender.h, but this has lots of unknown
   * DNA types in functions, leading to conflicting function signatures.
   */
  fprintf(f,
          "    extern int %s_%s_length(PointerRNA *);\n",
          item_name_base->identifier,
          rna_safe_id(item_name_prop->identifier));
  fprintf(f,
          "    extern void %s_%s_get(PointerRNA *, char *);\n\n",
          item_name_base->identifier,
          rna_safe_id(item_name_prop->identifier));

  fprintf(f, "    bool found = false;\n");
  fprintf(f, "    CollectionPropertyIterator iter;\n");
  fprintf(f, "    char namebuf[%d];\n", namebuflen);
  fprintf(f, "    char *name;\n\n");

  fprintf(f, "    %s_%s_begin(&iter, ptr);\n\n", srna->identifier, rna_safe_id(prop->identifier));

  fprintf(f, "    while (iter.valid) {\n");
  fprintf(f, "        if (iter.ptr.data) {\n");
  fprintf(f,
          "            int namelen = %s_%s_length(&iter.ptr);\n",
          item_name_base->identifier,
          rna_safe_id(item_name_prop->identifier));
  fprintf(f, "            if (namelen < %d) {\n", namebuflen);
  fprintf(f,
          "                %s_%s_get(&iter.ptr, namebuf);\n",
          item_name_base->identifier,
          rna_safe_id(item_name_prop->identifier));
  fprintf(f, "                if (strcmp(namebuf, key) == 0) {\n");
  fprintf(f, "                    found = true;\n");
  fprintf(f, "                    *r_ptr = iter.ptr;\n");
  fprintf(f, "                    break;\n");
  fprintf(f, "                }\n");
  fprintf(f, "            }\n");
  fprintf(f, "            else {\n");
  fprintf(f, "                name = MEM_mallocN(namelen+1, \"name string\");\n");
  fprintf(f,
          "                %s_%s_get(&iter.ptr, name);\n",
          item_name_base->identifier,
          rna_safe_id(item_name_prop->identifier));
  fprintf(f, "                if (strcmp(name, key) == 0) {\n");
  fprintf(f, "                    MEM_freeN(name);\n\n");
  fprintf(f, "                    found = true;\n");
  fprintf(f, "                    *r_ptr = iter.ptr;\n");
  fprintf(f, "                    break;\n");
  fprintf(f, "                }\n");
  fprintf(f, "                else {\n");
  fprintf(f, "                    MEM_freeN(name);\n");
  fprintf(f, "                }\n");
  fprintf(f, "            }\n");
  fprintf(f, "        }\n");
  fprintf(f, "        %s_%s_next(&iter);\n", srna->identifier, rna_safe_id(prop->identifier));
  fprintf(f, "    }\n");
  fprintf(f, "    %s_%s_end(&iter);\n\n", srna->identifier, rna_safe_id(prop->identifier));

  fprintf(f, "    return found;\n");
  fprintf(f, "}\n\n");

  return func;
}

static char *rna_def_property_next_func(FILE *f,
                                        StructRNA *srna,
                                        PropertyRNA *prop,
                                        PropertyDefRNA *UNUSED(dp),
                                        const char *manualfunc)
{
  char *func, *getfunc;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  if (!manualfunc) {
    return NULL;
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "next");

  fprintf(f, "void %s(CollectionPropertyIterator *iter)\n", func);
  fprintf(f, "{\n");
  fprintf(f, "    %s(iter);\n", manualfunc);

  getfunc = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

  fprintf(f, "\n    if (iter->valid)\n");
  fprintf(f, "        iter->ptr = %s(iter);\n", getfunc);

  fprintf(f, "}\n\n");

  return func;
}

static char *rna_def_property_end_func(FILE *f,
                                       StructRNA *srna,
                                       PropertyRNA *prop,
                                       PropertyDefRNA *UNUSED(dp),
                                       const char *manualfunc)
{
  char *func;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == NULL) {
    return NULL;
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "end");

  fprintf(f, "void %s(CollectionPropertyIterator *iter)\n", func);
  fprintf(f, "{\n");
  if (manualfunc) {
    fprintf(f, "    %s(iter);\n", manualfunc);
  }
  fprintf(f, "}\n\n");

  return func;
}

static void rna_set_raw_property(PropertyDefRNA *dp, PropertyRNA *prop)
{
  if (dp->dnapointerlevel != 0) {
    return;
  }
  if (!dp->dnatype || !dp->dnaname || !dp->dnastructname) {
    return;
  }

  if (STREQ(dp->dnatype, "char")) {
    prop->rawtype = PROP_RAW_CHAR;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "short")) {
    prop->rawtype = PROP_RAW_SHORT;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "int")) {
    prop->rawtype = PROP_RAW_INT;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "float")) {
    prop->rawtype = PROP_RAW_FLOAT;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "double")) {
    prop->rawtype = PROP_RAW_DOUBLE;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
}

static void rna_set_raw_offset(FILE *f, StructRNA *srna, PropertyRNA *prop)
{
  PropertyDefRNA *dp = rna_find_struct_property_def(srna, prop);

  fprintf(f, "\toffsetof(%s, %s), %d", dp->dnastructname, dp->dnaname, prop->rawtype);
}

static void rna_def_property_funcs(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
  PropertyRNA *prop;

  prop = dp->prop;

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;

      if (!prop->arraydimension) {
        if (!bprop->get && !bprop->set && !dp->booleanbit) {
          rna_set_raw_property(dp, prop);
        }

        bprop->get = (void *)rna_def_property_get_func(
            f, srna, prop, dp, (const char *)bprop->get);
        bprop->set = (void *)rna_def_property_set_func(
            f, srna, prop, dp, (const char *)bprop->set);
      }
      else {
        bprop->getarray = (void *)rna_def_property_get_func(
            f, srna, prop, dp, (const char *)bprop->getarray);
        bprop->setarray = (void *)rna_def_property_set_func(
            f, srna, prop, dp, (const char *)bprop->setarray);
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

      if (!prop->arraydimension) {
        if (!iprop->get && !iprop->set) {
          rna_set_raw_property(dp, prop);
        }

        iprop->get = (void *)rna_def_property_get_func(
            f, srna, prop, dp, (const char *)iprop->get);
        iprop->set = (void *)rna_def_property_set_func(
            f, srna, prop, dp, (const char *)iprop->set);
      }
      else {
        if (!iprop->getarray && !iprop->setarray) {
          rna_set_raw_property(dp, prop);
        }

        iprop->getarray = (void *)rna_def_property_get_func(
            f, srna, prop, dp, (const char *)iprop->getarray);
        iprop->setarray = (void *)rna_def_property_set_func(
            f, srna, prop, dp, (const char *)iprop->setarray);
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

      if (!prop->arraydimension) {
        if (!fprop->get && !fprop->set) {
          rna_set_raw_property(dp, prop);
        }

        fprop->get = (void *)rna_def_property_get_func(
            f, srna, prop, dp, (const char *)fprop->get);
        fprop->set = (void *)rna_def_property_set_func(
            f, srna, prop, dp, (const char *)fprop->set);
      }
      else {
        if (!fprop->getarray && !fprop->setarray) {
          rna_set_raw_property(dp, prop);
        }

        fprop->getarray = (void *)rna_def_property_get_func(
            f, srna, prop, dp, (const char *)fprop->getarray);
        fprop->setarray = (void *)rna_def_property_set_func(
            f, srna, prop, dp, (const char *)fprop->setarray);
      }
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;

      eprop->get = (void *)rna_def_property_get_func(f, srna, prop, dp, (const char *)eprop->get);
      eprop->set = (void *)rna_def_property_set_func(f, srna, prop, dp, (const char *)eprop->set);
      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;

      sprop->get = (void *)rna_def_property_get_func(f, srna, prop, dp, (const char *)sprop->get);
      sprop->length = (void *)rna_def_property_length_func(
          f, srna, prop, dp, (const char *)sprop->length);
      sprop->set = (void *)rna_def_property_set_func(f, srna, prop, dp, (const char *)sprop->set);
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

      pprop->get = (void *)rna_def_property_get_func(f, srna, prop, dp, (const char *)pprop->get);
      pprop->set = (void *)rna_def_property_set_func(f, srna, prop, dp, (const char *)pprop->set);
      if (!pprop->type) {
        CLOG_ERROR(
            &LOG, "%s.%s, pointer must have a struct type.", srna->identifier, prop->identifier);
        DefRNA.error = 1;
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
      const char *nextfunc = (const char *)cprop->next;
      const char *item_type = (const char *)cprop->item_type;

      if (cprop->length) {
        /* always generate if we have a manual implementation */
        cprop->length = (void *)rna_def_property_length_func(
            f, srna, prop, dp, (const char *)cprop->length);
      }
      else if (dp->dnatype && STREQ(dp->dnatype, "ListBase")) {
        /* pass */
      }
      else if (dp->dnalengthname || dp->dnalengthfixed) {
        cprop->length = (void *)rna_def_property_length_func(
            f, srna, prop, dp, (const char *)cprop->length);
      }

      /* test if we can allow raw array access, if it is using our standard
       * array get/next function, we can be sure it is an actual array */
      if (cprop->next && cprop->get) {
        if (STREQ((const char *)cprop->next, "rna_iterator_array_next") &&
            STREQ((const char *)cprop->get, "rna_iterator_array_get")) {
          prop->flag_internal |= PROP_INTERN_RAW_ARRAY;
        }
      }

      cprop->get = (void *)rna_def_property_get_func(f, srna, prop, dp, (const char *)cprop->get);
      cprop->begin = (void *)rna_def_property_begin_func(
          f, srna, prop, dp, (const char *)cprop->begin);
      cprop->next = (void *)rna_def_property_next_func(
          f, srna, prop, dp, (const char *)cprop->next);
      cprop->end = (void *)rna_def_property_end_func(f, srna, prop, dp, (const char *)cprop->end);
      cprop->lookupint = (void *)rna_def_property_lookup_int_func(
          f, srna, prop, dp, (const char *)cprop->lookupint, nextfunc);
      cprop->lookupstring = (void *)rna_def_property_lookup_string_func(
          f, srna, prop, dp, (const char *)cprop->lookupstring, item_type);

      if (!(prop->flag & PROP_IDPROPERTY)) {
        if (!cprop->begin) {
          CLOG_ERROR(&LOG,
                     "%s.%s, collection must have a begin function.",
                     srna->identifier,
                     prop->identifier);
          DefRNA.error = 1;
        }
        if (!cprop->next) {
          CLOG_ERROR(&LOG,
                     "%s.%s, collection must have a next function.",
                     srna->identifier,
                     prop->identifier);
          DefRNA.error = 1;
        }
        if (!cprop->get) {
          CLOG_ERROR(&LOG,
                     "%s.%s, collection must have a get function.",
                     srna->identifier,
                     prop->identifier);
          DefRNA.error = 1;
        }
      }
      if (!cprop->item_type) {
        CLOG_ERROR(&LOG,
                   "%s.%s, collection must have a struct type.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = 1;
      }
      break;
    }
  }
}

static void rna_def_property_funcs_header(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
  PropertyRNA *prop;
  const char *func;

  prop = dp->prop;

  if (prop->flag & PROP_IDPROPERTY || prop->flag_internal & PROP_INTERN_BUILTIN) {
    return;
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "");

  switch (prop->type) {
    case PROP_BOOLEAN: {
      if (!prop->arraydimension) {
        fprintf(f, "bool %sget(PointerRNA *ptr);\n", func);
        fprintf(f, "void %sset(PointerRNA *ptr, bool value);\n", func);
      }
      else if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "void %sget(PointerRNA *ptr, bool values[%u]);\n", func, prop->totarraylength);
        fprintf(f,
                "void %sset(PointerRNA *ptr, const bool values[%u]);\n",
                func,
                prop->totarraylength);
      }
      else {
        fprintf(f, "void %sget(PointerRNA *ptr, bool values[]);\n", func);
        fprintf(f, "void %sset(PointerRNA *ptr, const bool values[]);\n", func);
      }
      break;
    }
    case PROP_INT: {
      if (!prop->arraydimension) {
        fprintf(f, "int %sget(PointerRNA *ptr);\n", func);
        fprintf(f, "void %sset(PointerRNA *ptr, int value);\n", func);
      }
      else if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "void %sget(PointerRNA *ptr, int values[%u]);\n", func, prop->totarraylength);
        fprintf(
            f, "void %sset(PointerRNA *ptr, const int values[%u]);\n", func, prop->totarraylength);
      }
      else {
        fprintf(f, "void %sget(PointerRNA *ptr, int values[]);\n", func);
        fprintf(f, "void %sset(PointerRNA *ptr, const int values[]);\n", func);
      }
      break;
    }
    case PROP_FLOAT: {
      if (!prop->arraydimension) {
        fprintf(f, "float %sget(PointerRNA *ptr);\n", func);
        fprintf(f, "void %sset(PointerRNA *ptr, float value);\n", func);
      }
      else if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "void %sget(PointerRNA *ptr, float values[%u]);\n", func, prop->totarraylength);
        fprintf(f,
                "void %sset(PointerRNA *ptr, const float values[%u]);\n",
                func,
                prop->totarraylength);
      }
      else {
        fprintf(f, "void %sget(PointerRNA *ptr, float values[]);\n", func);
        fprintf(f, "void %sset(PointerRNA *ptr, const float values[]);", func);
      }
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      int i;

      if (eprop->item && eprop->totitem) {
        fprintf(f, "enum {\n");

        for (i = 0; i < eprop->totitem; i++) {
          if (eprop->item[i].identifier[0]) {
            fprintf(f,
                    "\t%s_%s_%s = %d,\n",
                    srna->identifier,
                    prop->identifier,
                    eprop->item[i].identifier,
                    eprop->item[i].value);
          }
        }

        fprintf(f, "};\n\n");
      }

      fprintf(f, "int %sget(PointerRNA *ptr);\n", func);
      fprintf(f, "void %sset(PointerRNA *ptr, int value);\n", func);

      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;

      if (sprop->maxlength) {
        fprintf(
            f, "#define %s_%s_MAX %d\n\n", srna->identifier, prop->identifier, sprop->maxlength);
      }

      fprintf(f, "void %sget(PointerRNA *ptr, char *value);\n", func);
      fprintf(f, "int %slength(PointerRNA *ptr);\n", func);
      fprintf(f, "void %sset(PointerRNA *ptr, const char *value);\n", func);

      break;
    }
    case PROP_POINTER: {
      fprintf(f, "PointerRNA %sget(PointerRNA *ptr);\n", func);
      /*fprintf(f, "void %sset(PointerRNA *ptr, PointerRNA value);\n", func); */
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
      fprintf(f, "void %sbegin(CollectionPropertyIterator *iter, PointerRNA *ptr);\n", func);
      fprintf(f, "void %snext(CollectionPropertyIterator *iter);\n", func);
      fprintf(f, "void %send(CollectionPropertyIterator *iter);\n", func);
      if (cprop->length) {
        fprintf(f, "int %slength(PointerRNA *ptr);\n", func);
      }
      if (cprop->lookupint) {
        fprintf(f, "int %slookup_int(PointerRNA *ptr, int key, PointerRNA *r_ptr);\n", func);
      }
      if (cprop->lookupstring) {
        fprintf(f,
                "int %slookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr);\n",
                func);
      }
      break;
    }
  }

  if (prop->getlength) {
    char funcname[2048];
    rna_construct_wrapper_function_name(
        funcname, sizeof(funcname), srna->identifier, prop->identifier, "get_length");
    fprintf(f, "int %s(PointerRNA *ptr, int *arraylen);\n", funcname);
  }

  fprintf(f, "\n");
}

static void rna_def_function_funcs_header(FILE *f, StructRNA *srna, FunctionDefRNA *dfunc)
{
  FunctionRNA *func = dfunc->func;
  char funcname[2048];

  rna_construct_wrapper_function_name(
      funcname, sizeof(funcname), srna->identifier, func->identifier, "func");
  rna_generate_static_parameter_prototypes(f, srna, dfunc, funcname, 1);
}

static void rna_def_property_funcs_header_cpp(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
  PropertyRNA *prop;

  prop = dp->prop;

  if (prop->flag & PROP_IDPROPERTY || prop->flag_internal & PROP_INTERN_BUILTIN) {
    return;
  }

  /* disabled for now to avoid msvc compiler error due to large file size */
#if 0
  if (prop->name && prop->description && prop->description[0] != '\0')
    fprintf(f, "\t/* %s: %s */\n", prop->name, prop->description);
  else if (prop->name)
    fprintf(f, "\t/* %s */\n", prop->name);
  else
    fprintf(f, "\t/* */\n");
#endif

  switch (prop->type) {
    case PROP_BOOLEAN: {
      if (!prop->arraydimension) {
        fprintf(f, "\tinline bool %s(void);\n", rna_safe_id(prop->identifier));
        fprintf(f, "\tinline void %s(bool value);", rna_safe_id(prop->identifier));
      }
      else if (prop->totarraylength) {
        fprintf(f,
                "\tinline Array<bool, %u> %s(void);\n",
                prop->totarraylength,
                rna_safe_id(prop->identifier));
        fprintf(f,
                "\tinline void %s(bool values[%u]);",
                rna_safe_id(prop->identifier),
                prop->totarraylength);
      }
      else if (prop->getlength) {
        fprintf(f, "\tinline DynamicArray<bool> %s(void);\n", rna_safe_id(prop->identifier));
        fprintf(f, "\tinline void %s(bool values[]);", rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_INT: {
      if (!prop->arraydimension) {
        fprintf(f, "\tinline int %s(void);\n", rna_safe_id(prop->identifier));
        fprintf(f, "\tinline void %s(int value);", rna_safe_id(prop->identifier));
      }
      else if (prop->totarraylength) {
        fprintf(f,
                "\tinline Array<int, %u> %s(void);\n",
                prop->totarraylength,
                rna_safe_id(prop->identifier));
        fprintf(f,
                "\tinline void %s(int values[%u]);",
                rna_safe_id(prop->identifier),
                prop->totarraylength);
      }
      else if (prop->getlength) {
        fprintf(f, "\tinline DynamicArray<int> %s(void);\n", rna_safe_id(prop->identifier));
        fprintf(f, "\tinline void %s(int values[]);", rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_FLOAT: {
      if (!prop->arraydimension) {
        fprintf(f, "\tinline float %s(void);\n", rna_safe_id(prop->identifier));
        fprintf(f, "\tinline void %s(float value);", rna_safe_id(prop->identifier));
      }
      else if (prop->totarraylength) {
        fprintf(f,
                "\tinline Array<float, %u> %s(void);\n",
                prop->totarraylength,
                rna_safe_id(prop->identifier));
        fprintf(f,
                "\tinline void %s(float values[%u]);",
                rna_safe_id(prop->identifier),
                prop->totarraylength);
      }
      else if (prop->getlength) {
        fprintf(f, "\tinline DynamicArray<float> %s(void);\n", rna_safe_id(prop->identifier));
        fprintf(f, "\tinline void %s(float values[]);", rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      int i;

      if (eprop->item) {
        fprintf(f, "\tenum %s_enum {\n", rna_safe_id(prop->identifier));

        for (i = 0; i < eprop->totitem; i++) {
          if (eprop->item[i].identifier[0]) {
            fprintf(f,
                    "\t\t%s_%s = %d,\n",
                    rna_safe_id(prop->identifier),
                    eprop->item[i].identifier,
                    eprop->item[i].value);
          }
        }

        fprintf(f, "\t};\n");
      }

      fprintf(f,
              "\tinline %s_enum %s(void);\n",
              rna_safe_id(prop->identifier),
              rna_safe_id(prop->identifier));
      fprintf(f,
              "\tinline void %s(%s_enum value);",
              rna_safe_id(prop->identifier),
              rna_safe_id(prop->identifier));
      break;
    }
    case PROP_STRING: {
      fprintf(f, "\tinline std::string %s(void);\n", rna_safe_id(prop->identifier));
      fprintf(f, "\tinline void %s(const std::string& value);", rna_safe_id(prop->identifier));
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)dp->prop;

      if (pprop->type) {
        fprintf(
            f, "\tinline %s %s(void);", (const char *)pprop->type, rna_safe_id(prop->identifier));
      }
      else {
        fprintf(f, "\tinline %s %s(void);", "UnknownType", rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)dp->prop;
      const char *collection_funcs = "DefaultCollectionFunctions";

      if (!(dp->prop->flag & PROP_IDPROPERTY || dp->prop->flag_internal & PROP_INTERN_BUILTIN) &&
          cprop->property.srna) {
        collection_funcs = (char *)cprop->property.srna;
      }

      if (cprop->item_type) {
        fprintf(f,
                "\tCOLLECTION_PROPERTY(%s, %s, %s, %s, %s, %s, %s)",
                collection_funcs,
                (const char *)cprop->item_type,
                srna->identifier,
                rna_safe_id(prop->identifier),
                (cprop->length ? "true" : "false"),
                (cprop->lookupint ? "true" : "false"),
                (cprop->lookupstring ? "true" : "false"));
      }
      else {
        fprintf(f,
                "\tCOLLECTION_PROPERTY(%s, %s, %s, %s, %s, %s, %s)",
                collection_funcs,
                "UnknownType",
                srna->identifier,
                rna_safe_id(prop->identifier),
                (cprop->length ? "true" : "false"),
                (cprop->lookupint ? "true" : "false"),
                (cprop->lookupstring ? "true" : "false"));
      }
      break;
    }
  }

  fprintf(f, "\n");
}

static const char *rna_parameter_type_cpp_name(PropertyRNA *prop)
{
  if (prop->type == PROP_POINTER) {
    /* for cpp api we need to use RNA structures names for pointers */
    PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

    return (const char *)pprop->type;
  }
  else {
    return rna_parameter_type_name(prop);
  }
}

static void rna_def_struct_function_prototype_cpp(FILE *f,
                                                  StructRNA *UNUSED(srna),
                                                  FunctionDefRNA *dfunc,
                                                  const char *namespace,
                                                  int close_prototype)
{
  PropertyDefRNA *dp;
  FunctionRNA *func = dfunc->func;

  int first = 1;
  const char *retval_type = "void";

  if (func->c_ret) {
    dp = rna_find_parameter_def(func->c_ret);
    retval_type = rna_parameter_type_cpp_name(dp->prop);
  }

  if (namespace && namespace[0]) {
    fprintf(f, "\tinline %s %s::%s(", retval_type, namespace, rna_safe_id(func->identifier));
  }
  else {
    fprintf(f, "\tinline %s %s(", retval_type, rna_safe_id(func->identifier));
  }

  if (func->flag & FUNC_USE_MAIN)
    WRITE_PARAM("void *main");

  if (func->flag & FUNC_USE_CONTEXT)
    WRITE_PARAM("Context C");

  for (dp = dfunc->cont.properties.first; dp; dp = dp->next) {
    int type, flag, flag_parameter, pout;
    const char *ptrstr;

    if (dp->prop == func->c_ret) {
      continue;
    }

    type = dp->prop->type;
    flag = dp->prop->flag;
    flag_parameter = dp->prop->flag_parameter;
    pout = (flag_parameter & PARM_OUTPUT);

    if (flag & PROP_DYNAMIC) {
      ptrstr = pout ? "**" : "*";
    }
    else if (type == PROP_POINTER) {
      ptrstr = pout ? "*" : "";
    }
    else if (dp->prop->arraydimension) {
      ptrstr = "*";
    }
    else if (type == PROP_STRING && (flag & PROP_THICK_WRAP)) {
      ptrstr = "";
    }
    else {
      ptrstr = pout ? "*" : "";
    }

    WRITE_COMMA;

    if (flag & PROP_DYNAMIC) {
      fprintf(
          f, "int %s%s_len, ", (flag_parameter & PARM_OUTPUT) ? "*" : "", dp->prop->identifier);
    }

    if (!(flag & PROP_DYNAMIC) && dp->prop->arraydimension) {
      fprintf(f,
              "%s %s[%u]",
              rna_parameter_type_cpp_name(dp->prop),
              rna_safe_id(dp->prop->identifier),
              dp->prop->totarraylength);
    }
    else {
      fprintf(f,
              "%s%s%s%s",
              rna_parameter_type_cpp_name(dp->prop),
              (dp->prop->type == PROP_POINTER && ptrstr[0] == '\0') ? "& " : " ",
              ptrstr,
              rna_safe_id(dp->prop->identifier));
    }
  }

  fprintf(f, ")");
  if (close_prototype) {
    fprintf(f, ";\n");
  }
}

static void rna_def_struct_function_header_cpp(FILE *f, StructRNA *srna, FunctionDefRNA *dfunc)
{
  if (dfunc->call) {
    /* disabled for now to avoid msvc compiler error due to large file size */
#if 0
    FunctionRNA *func = dfunc->func;
    fprintf(f, "\n\t/* %s */\n", func->description);
#endif

    rna_def_struct_function_prototype_cpp(f, srna, dfunc, NULL, 1);
  }
}

static void rna_def_property_funcs_impl_cpp(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
  PropertyRNA *prop;

  prop = dp->prop;

  if (prop->flag & PROP_IDPROPERTY || prop->flag_internal & PROP_INTERN_BUILTIN) {
    return;
  }

  switch (prop->type) {
    case PROP_BOOLEAN: {
      if (!prop->arraydimension) {
        fprintf(f, "\tBOOLEAN_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
      }
      else if (prop->totarraylength) {
        fprintf(f,
                "\tBOOLEAN_ARRAY_PROPERTY(%s, %u, %s)",
                srna->identifier,
                prop->totarraylength,
                rna_safe_id(prop->identifier));
      }
      else if (prop->getlength) {
        fprintf(f,
                "\tBOOLEAN_DYNAMIC_ARRAY_PROPERTY(%s, %s)",
                srna->identifier,
                rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_INT: {
      if (!prop->arraydimension) {
        fprintf(f, "\tINT_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
      }
      else if (prop->totarraylength) {
        fprintf(f,
                "\tINT_ARRAY_PROPERTY(%s, %u, %s)",
                srna->identifier,
                prop->totarraylength,
                rna_safe_id(prop->identifier));
      }
      else if (prop->getlength) {
        fprintf(f,
                "\tINT_DYNAMIC_ARRAY_PROPERTY(%s, %s)",
                srna->identifier,
                rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_FLOAT: {
      if (!prop->arraydimension) {
        fprintf(f, "\tFLOAT_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
      }
      else if (prop->totarraylength) {
        fprintf(f,
                "\tFLOAT_ARRAY_PROPERTY(%s, %u, %s)",
                srna->identifier,
                prop->totarraylength,
                rna_safe_id(prop->identifier));
      }
      else if (prop->getlength) {
        fprintf(f,
                "\tFLOAT_DYNAMIC_ARRAY_PROPERTY(%s, %s)",
                srna->identifier,
                rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_ENUM: {
      fprintf(f,
              "\tENUM_PROPERTY(%s_enum, %s, %s)",
              rna_safe_id(prop->identifier),
              srna->identifier,
              rna_safe_id(prop->identifier));

      break;
    }
    case PROP_STRING: {
      fprintf(f, "\tSTRING_PROPERTY(%s, %s)", srna->identifier, rna_safe_id(prop->identifier));
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)dp->prop;

      if (pprop->type) {
        fprintf(f,
                "\tPOINTER_PROPERTY(%s, %s, %s)",
                (const char *)pprop->type,
                srna->identifier,
                rna_safe_id(prop->identifier));
      }
      else {
        fprintf(f,
                "\tPOINTER_PROPERTY(%s, %s, %s)",
                "UnknownType",
                srna->identifier,
                rna_safe_id(prop->identifier));
      }
      break;
    }
    case PROP_COLLECTION: {
#if 0
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)dp->prop;

      if (cprop->type)
        fprintf(f,
                "\tCOLLECTION_PROPERTY(%s, %s, %s, %s, %s, %s)",
                (const char *)cprop->type,
                srna->identifier,
                prop->identifier,
                (cprop->length ? "true" : "false"),
                (cprop->lookupint ? "true" : "false"),
                (cprop->lookupstring ? "true" : "false"));
      else
        fprintf(f,
                "\tCOLLECTION_PROPERTY(%s, %s, %s, %s, %s, %s)",
                "UnknownType",
                srna->identifier,
                prop->identifier,
                (cprop->length ? "true" : "false"),
                (cprop->lookupint ? "true" : "false"),
                (cprop->lookupstring ? "true" : "false"));
#endif
      break;
    }
  }

  fprintf(f, "\n");
}

static void rna_def_struct_function_call_impl_cpp(FILE *f, StructRNA *srna, FunctionDefRNA *dfunc)
{
  PropertyDefRNA *dp;
  StructDefRNA *dsrna;
  FunctionRNA *func = dfunc->func;
  char funcname[2048];

  int first = 1;

  rna_construct_wrapper_function_name(
      funcname, sizeof(funcname), srna->identifier, func->identifier, "func");

  fprintf(f, "%s(", funcname);

  dsrna = rna_find_struct_def(srna);

  if (func->flag & FUNC_USE_SELF_ID)
    WRITE_PARAM("(::ID *) ptr.id.data");

  if ((func->flag & FUNC_NO_SELF) == 0) {
    WRITE_COMMA;
    if (dsrna->dnafromprop) {
      fprintf(f, "(::%s *) this->ptr.data", dsrna->dnafromname);
    }
    else if (dsrna->dnaname) {
      fprintf(f, "(::%s *) this->ptr.data", dsrna->dnaname);
    }
    else {
      fprintf(f, "(::%s *) this->ptr.data", srna->identifier);
    }
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    WRITE_COMMA;
    fprintf(f, "this->ptr.type");
  }

  if (func->flag & FUNC_USE_MAIN)
    WRITE_PARAM("(::Main *) main");

  if (func->flag & FUNC_USE_CONTEXT)
    WRITE_PARAM("(::bContext *) C.ptr.data");

  if (func->flag & FUNC_USE_REPORTS)
    WRITE_PARAM("NULL");

  dp = dfunc->cont.properties.first;
  for (; dp; dp = dp->next) {
    if (dp->prop == func->c_ret) {
      continue;
    }

    WRITE_COMMA;

    if (dp->prop->flag & PROP_DYNAMIC) {
      fprintf(f, "%s_len, ", dp->prop->identifier);
    }

    if (dp->prop->type == PROP_POINTER) {
      if ((dp->prop->flag_parameter & PARM_RNAPTR) && !(dp->prop->flag & PROP_THICK_WRAP)) {
        fprintf(f,
                "(::%s *) &%s.ptr",
                rna_parameter_type_name(dp->prop),
                rna_safe_id(dp->prop->identifier));
      }
      else if (dp->prop->flag_parameter & PARM_OUTPUT) {
        if (dp->prop->flag_parameter & PARM_RNAPTR) {
          fprintf(f, "&%s->ptr", rna_safe_id(dp->prop->identifier));
        }
        else {
          fprintf(f,
                  "(::%s **) &%s->ptr.data",
                  rna_parameter_type_name(dp->prop),
                  rna_safe_id(dp->prop->identifier));
        }
      }
      else {
        fprintf(f,
                "(::%s *) %s.ptr.data",
                rna_parameter_type_name(dp->prop),
                rna_safe_id(dp->prop->identifier));
      }
    }
    else {
      fprintf(f, "%s", rna_safe_id(dp->prop->identifier));
    }
  }

  fprintf(f, ");\n");
}

static void rna_def_struct_function_impl_cpp(FILE *f, StructRNA *srna, FunctionDefRNA *dfunc)
{
  PropertyDefRNA *dp;
  PointerPropertyRNA *pprop;

  FunctionRNA *func = dfunc->func;

  if (!dfunc->call) {
    return;
  }

  rna_def_struct_function_prototype_cpp(f, srna, dfunc, srna->identifier, 0);

  fprintf(f, " {\n");

  if (func->c_ret) {
    dp = rna_find_parameter_def(func->c_ret);

    if (dp->prop->type == PROP_POINTER) {
      pprop = (PointerPropertyRNA *)dp->prop;

      fprintf(f, "\t\tPointerRNA result;\n");

      if ((dp->prop->flag_parameter & PARM_RNAPTR) == 0) {
        StructRNA *ret_srna = rna_find_struct((const char *)pprop->type);
        fprintf(f, "\t\t::%s *retdata = ", rna_parameter_type_name(dp->prop));
        rna_def_struct_function_call_impl_cpp(f, srna, dfunc);
        if (ret_srna->flag & STRUCT_ID) {
          fprintf(f, "\t\tRNA_id_pointer_create((::ID *) retdata, &result);\n");
        }
        else {
          fprintf(f,
                  "\t\tRNA_pointer_create((::ID *) ptr.id.data, &RNA_%s, retdata, &result);\n",
                  (const char *)pprop->type);
        }
      }
      else {
        fprintf(f, "\t\tresult = ");
        rna_def_struct_function_call_impl_cpp(f, srna, dfunc);
      }

      fprintf(f, "\t\treturn %s(result);\n", (const char *)pprop->type);
    }
    else {
      fprintf(f, "\t\treturn ");
      rna_def_struct_function_call_impl_cpp(f, srna, dfunc);
    }
  }
  else {
    fprintf(f, "\t\t");
    rna_def_struct_function_call_impl_cpp(f, srna, dfunc);
  }

  fprintf(f, "\t}\n\n");
}

static void rna_def_property_wrapper_funcs(FILE *f, StructDefRNA *dsrna, PropertyDefRNA *dp)
{
  if (dp->prop->getlength) {
    char funcname[2048];
    rna_construct_wrapper_function_name(
        funcname, sizeof(funcname), dsrna->srna->identifier, dp->prop->identifier, "get_length");
    fprintf(f, "int %s(PointerRNA *ptr, int *arraylen)\n", funcname);
    fprintf(f, "{\n");
    fprintf(f, "\treturn %s(ptr, arraylen);\n", rna_function_string(dp->prop->getlength));
    fprintf(f, "}\n\n");
  }
}

static void rna_def_function_wrapper_funcs(FILE *f, StructDefRNA *dsrna, FunctionDefRNA *dfunc)
{
  StructRNA *srna = dsrna->srna;
  FunctionRNA *func = dfunc->func;
  PropertyDefRNA *dparm;

  int first;
  char funcname[2048];

  if (!dfunc->call) {
    return;
  }

  rna_construct_wrapper_function_name(
      funcname, sizeof(funcname), srna->identifier, func->identifier, "func");

  rna_generate_static_parameter_prototypes(f, srna, dfunc, funcname, 0);

  fprintf(f, "\n{\n");

  if (func->c_ret) {
    fprintf(f, "\treturn %s(", dfunc->call);
  }
  else {
    fprintf(f, "\t%s(", dfunc->call);
  }

  first = 1;

  if (func->flag & FUNC_USE_SELF_ID)
    WRITE_PARAM("_selfid");

  if ((func->flag & FUNC_NO_SELF) == 0) {
    WRITE_PARAM("_self");
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    WRITE_PARAM("_type");
  }

  if (func->flag & FUNC_USE_MAIN)
    WRITE_PARAM("bmain");

  if (func->flag & FUNC_USE_CONTEXT)
    WRITE_PARAM("C");

  if (func->flag & FUNC_USE_REPORTS)
    WRITE_PARAM("reports");

  dparm = dfunc->cont.properties.first;
  for (; dparm; dparm = dparm->next) {
    if (dparm->prop == func->c_ret) {
      continue;
    }

    WRITE_COMMA;

    if (dparm->prop->flag & PROP_DYNAMIC) {
      fprintf(f, "%s_len, %s", dparm->prop->identifier, dparm->prop->identifier);
    }
    else {
      fprintf(f, "%s", rna_safe_id(dparm->prop->identifier));
    }
  }

  fprintf(f, ");\n");
  fprintf(f, "}\n\n");
}

static void rna_def_function_funcs(FILE *f, StructDefRNA *dsrna, FunctionDefRNA *dfunc)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyDefRNA *dparm;
  PropertyType type;
  const char *funcname, *valstr;
  const char *ptrstr;
  const bool has_data = (dfunc->cont.properties.first != NULL);
  int flag, flag_parameter, pout, cptr, first;

  srna = dsrna->srna;
  func = dfunc->func;

  if (!dfunc->call) {
    return;
  }

  funcname = rna_alloc_function_name(srna->identifier, func->identifier, "call");

  /* function definition */
  fprintf(f,
          "void %s(bContext *C, ReportList *reports, PointerRNA *_ptr, ParameterList *_parms)",
          funcname);
  fprintf(f, "\n{\n");

  /* variable definitions */

  if (func->flag & FUNC_USE_SELF_ID) {
    fprintf(f, "\tstruct ID *_selfid;\n");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if (dsrna->dnafromprop) {
      fprintf(f, "\tstruct %s *_self;\n", dsrna->dnafromname);
    }
    else if (dsrna->dnaname) {
      fprintf(f, "\tstruct %s *_self;\n", dsrna->dnaname);
    }
    else {
      fprintf(f, "\tstruct %s *_self;\n", srna->identifier);
    }
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    fprintf(f, "\tstruct StructRNA *_type;\n");
  }

  dparm = dfunc->cont.properties.first;
  for (; dparm; dparm = dparm->next) {
    type = dparm->prop->type;
    flag = dparm->prop->flag;
    flag_parameter = dparm->prop->flag_parameter;
    pout = (flag_parameter & PARM_OUTPUT);
    cptr = ((type == PROP_POINTER) && !(flag_parameter & PARM_RNAPTR));

    if (dparm->prop == func->c_ret) {
      ptrstr = cptr || dparm->prop->arraydimension ? "*" : "";
      /* XXX only arrays and strings are allowed to be dynamic, is this checked anywhere? */
    }
    else if (cptr || (flag & PROP_DYNAMIC)) {
      ptrstr = pout ? "**" : "*";
      /* Fixed size arrays and RNA pointers are pre-allocated on the ParameterList stack,
       * pass a pointer to it. */
    }
    else if (type == PROP_POINTER || dparm->prop->arraydimension) {
      ptrstr = "*";
    }
    else if ((type == PROP_POINTER) && (flag_parameter & PARM_RNAPTR) &&
             !(flag & PROP_THICK_WRAP)) {
      ptrstr = "*";
      /* PROP_THICK_WRAP strings are pre-allocated on the ParameterList stack,
       * but type name for string props is already (char *), so leave empty */
    }
    else if (type == PROP_STRING && (flag & PROP_THICK_WRAP)) {
      ptrstr = "";
    }
    else {
      ptrstr = pout ? "*" : "";
    }

    /* for dynamic parameters we pass an additional int for the length of the parameter */
    if (flag & PROP_DYNAMIC) {
      fprintf(f, "\tint %s%s_len;\n", pout ? "*" : "", dparm->prop->identifier);
    }

    fprintf(f,
            "\t%s%s %s%s;\n",
            rna_type_struct(dparm->prop),
            rna_parameter_type_name(dparm->prop),
            ptrstr,
            dparm->prop->identifier);
  }

  if (has_data) {
    fprintf(f, "\tchar *_data");
    if (func->c_ret) {
      fprintf(f, ", *_retdata");
    }
    fprintf(f, ";\n");
    fprintf(f, "\t\n");
  }

  /* assign self */
  if (func->flag & FUNC_USE_SELF_ID) {
    fprintf(f, "\t_selfid = (struct ID *)_ptr->id.data;\n");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if (dsrna->dnafromprop) {
      fprintf(f, "\t_self = (struct %s *)_ptr->data;\n", dsrna->dnafromname);
    }
    else if (dsrna->dnaname) {
      fprintf(f, "\t_self = (struct %s *)_ptr->data;\n", dsrna->dnaname);
    }
    else {
      fprintf(f, "\t_self = (struct %s *)_ptr->data;\n", srna->identifier);
    }
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    fprintf(f, "\t_type = _ptr->type;\n");
  }

  if (has_data) {
    fprintf(f, "\t_data = (char *)_parms->data;\n");
  }

  dparm = dfunc->cont.properties.first;
  for (; dparm; dparm = dparm->next) {
    type = dparm->prop->type;
    flag = dparm->prop->flag;
    flag_parameter = dparm->prop->flag_parameter;
    pout = (flag_parameter & PARM_OUTPUT);
    cptr = ((type == PROP_POINTER) && !(flag_parameter & PARM_RNAPTR));

    if (dparm->prop == func->c_ret) {
      fprintf(f, "\t_retdata = _data;\n");
    }
    else {
      const char *data_str;
      if (cptr || (flag & PROP_DYNAMIC)) {
        ptrstr = "**";
        valstr = "*";
      }
      else if ((type == PROP_POINTER) && !(flag & PROP_THICK_WRAP)) {
        ptrstr = "**";
        valstr = "*";
      }
      else if (type == PROP_POINTER || dparm->prop->arraydimension) {
        ptrstr = "*";
        valstr = "";
      }
      else if (type == PROP_STRING && (flag & PROP_THICK_WRAP)) {
        ptrstr = "";
        valstr = "";
      }
      else {
        ptrstr = "*";
        valstr = "*";
      }

      /* This must be kept in sync with RNA_parameter_dynamic_length_get_data and
       * RNA_parameter_get, we could just call the function directly, but this is faster. */
      if (flag & PROP_DYNAMIC) {
        fprintf(f,
                "\t%s_len = %s((ParameterDynAlloc *)_data)->array_tot;\n",
                dparm->prop->identifier,
                pout ? "(int *)&" : "(int)");
        data_str = "(&(((ParameterDynAlloc *)_data)->array))";
      }
      else {
        data_str = "_data";
      }
      fprintf(f, "\t%s = ", dparm->prop->identifier);

      if (!pout) {
        fprintf(f, "%s", valstr);
      }

      fprintf(f,
              "((%s%s %s)%s);\n",
              rna_type_struct(dparm->prop),
              rna_parameter_type_name(dparm->prop),
              ptrstr,
              data_str);
    }

    if (dparm->next) {
      fprintf(f, "\t_data += %d;\n", rna_parameter_size(dparm->prop));
    }
  }

  if (dfunc->call) {
    fprintf(f, "\t\n");
    fprintf(f, "\t");
    if (func->c_ret) {
      fprintf(f, "%s = ", func->c_ret->identifier);
    }
    fprintf(f, "%s(", dfunc->call);

    first = 1;

    if (func->flag & FUNC_USE_SELF_ID) {
      fprintf(f, "_selfid");
      first = 0;
    }

    if ((func->flag & FUNC_NO_SELF) == 0) {
      if (!first) {
        fprintf(f, ", ");
      }
      fprintf(f, "_self");
      first = 0;
    }
    else if (func->flag & FUNC_USE_SELF_TYPE) {
      if (!first) {
        fprintf(f, ", ");
      }
      fprintf(f, "_type");
      first = 0;
    }

    if (func->flag & FUNC_USE_MAIN) {
      if (!first) {
        fprintf(f, ", ");
      }
      first = 0;
      fprintf(f, "CTX_data_main(C)"); /* may have direct access later */
    }

    if (func->flag & FUNC_USE_CONTEXT) {
      if (!first) {
        fprintf(f, ", ");
      }
      first = 0;
      fprintf(f, "C");
    }

    if (func->flag & FUNC_USE_REPORTS) {
      if (!first) {
        fprintf(f, ", ");
      }
      first = 0;
      fprintf(f, "reports");
    }

    dparm = dfunc->cont.properties.first;
    for (; dparm; dparm = dparm->next) {
      if (dparm->prop == func->c_ret) {
        continue;
      }

      if (!first) {
        fprintf(f, ", ");
      }
      first = 0;

      if (dparm->prop->flag & PROP_DYNAMIC) {
        fprintf(f, "%s_len, %s", dparm->prop->identifier, dparm->prop->identifier);
      }
      else {
        fprintf(f, "%s", dparm->prop->identifier);
      }
    }

    fprintf(f, ");\n");

    if (func->c_ret) {
      dparm = rna_find_parameter_def(func->c_ret);
      ptrstr = (((dparm->prop->type == PROP_POINTER) &&
                 !(dparm->prop->flag_parameter & PARM_RNAPTR)) ||
                (dparm->prop->arraydimension)) ?
                   "*" :
                   "";
      fprintf(f,
              "\t*((%s%s %s*)_retdata) = %s;\n",
              rna_type_struct(dparm->prop),
              rna_parameter_type_name(dparm->prop),
              ptrstr,
              func->c_ret->identifier);
    }
  }

  fprintf(f, "}\n\n");

  dfunc->gencall = funcname;
}

static void rna_auto_types(void)
{
  StructDefRNA *ds;
  PropertyDefRNA *dp;

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    /* DNA name for Screen is patched in 2.5, we do the reverse here .. */
    if (ds->dnaname) {
      if (STREQ(ds->dnaname, "Screen")) {
        ds->dnaname = "bScreen";
      }
      if (STREQ(ds->dnaname, "Group")) {
        ds->dnaname = "Collection";
      }
      if (STREQ(ds->dnaname, "GroupObject")) {
        ds->dnaname = "CollectionObject";
      }
    }

    for (dp = ds->cont.properties.first; dp; dp = dp->next) {
      if (dp->dnastructname) {
        if (STREQ(dp->dnastructname, "Screen")) {
          dp->dnastructname = "bScreen";
        }
        if (STREQ(dp->dnastructname, "Group")) {
          dp->dnastructname = "Collection";
        }
        if (STREQ(dp->dnastructname, "GroupObject")) {
          dp->dnastructname = "CollectionObject";
        }
      }

      if (dp->dnatype) {
        if (dp->prop->type == PROP_POINTER) {
          PointerPropertyRNA *pprop = (PointerPropertyRNA *)dp->prop;
          StructRNA *type;

          if (!pprop->type && !pprop->get) {
            pprop->type = (StructRNA *)rna_find_type(dp->dnatype);
          }

          if (pprop->type) {
            type = rna_find_struct((const char *)pprop->type);
            if (type && (type->flag & STRUCT_ID_REFCOUNT)) {
              pprop->property.flag |= PROP_ID_REFCOUNT;
            }
          }
        }
        else if (dp->prop->type == PROP_COLLECTION) {
          CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)dp->prop;

          if (!cprop->item_type && !cprop->get && STREQ(dp->dnatype, "ListBase")) {
            cprop->item_type = (StructRNA *)rna_find_type(dp->dnatype);
          }
        }
      }
    }
  }
}

static void rna_sort(BlenderRNA *brna)
{
  StructDefRNA *ds;
  StructRNA *srna;

  rna_sortlist(&brna->structs, cmp_struct);
  rna_sortlist(&DefRNA.structs, cmp_def_struct);

  for (srna = brna->structs.first; srna; srna = srna->cont.next) {
    rna_sortlist(&srna->cont.properties, cmp_property);
  }

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    rna_sortlist(&ds->cont.properties, cmp_def_property);
  }
}

static const char *rna_property_structname(PropertyType type)
{
  switch (type) {
    case PROP_BOOLEAN:
      return "BoolPropertyRNA";
    case PROP_INT:
      return "IntPropertyRNA";
    case PROP_FLOAT:
      return "FloatPropertyRNA";
    case PROP_STRING:
      return "StringPropertyRNA";
    case PROP_ENUM:
      return "EnumPropertyRNA";
    case PROP_POINTER:
      return "PointerPropertyRNA";
    case PROP_COLLECTION:
      return "CollectionPropertyRNA";
    default:
      return "UnknownPropertyRNA";
  }
}

static const char *rna_property_subtypename(PropertySubType type)
{
  switch (type) {
    case PROP_NONE:
      return "PROP_NONE";
    case PROP_FILEPATH:
      return "PROP_FILEPATH";
    case PROP_FILENAME:
      return "PROP_FILENAME";
    case PROP_DIRPATH:
      return "PROP_DIRPATH";
    case PROP_PIXEL:
      return "PROP_PIXEL";
    case PROP_BYTESTRING:
      return "PROP_BYTESTRING";
    case PROP_UNSIGNED:
      return "PROP_UNSIGNED";
    case PROP_PERCENTAGE:
      return "PROP_PERCENTAGE";
    case PROP_FACTOR:
      return "PROP_FACTOR";
    case PROP_ANGLE:
      return "PROP_ANGLE";
    case PROP_TIME:
      return "PROP_TIME";
    case PROP_DISTANCE:
      return "PROP_DISTANCE";
    case PROP_DISTANCE_CAMERA:
      return "PROP_DISTANCE_CAMERA";
    case PROP_COLOR:
      return "PROP_COLOR";
    case PROP_TRANSLATION:
      return "PROP_TRANSLATION";
    case PROP_DIRECTION:
      return "PROP_DIRECTION";
    case PROP_MATRIX:
      return "PROP_MATRIX";
    case PROP_EULER:
      return "PROP_EULER";
    case PROP_QUATERNION:
      return "PROP_QUATERNION";
    case PROP_AXISANGLE:
      return "PROP_AXISANGLE";
    case PROP_VELOCITY:
      return "PROP_VELOCITY";
    case PROP_ACCELERATION:
      return "PROP_ACCELERATION";
    case PROP_XYZ:
      return "PROP_XYZ";
    case PROP_COLOR_GAMMA:
      return "PROP_COLOR_GAMMA";
    case PROP_COORDS:
      return "PROP_COORDS";
    case PROP_LAYER:
      return "PROP_LAYER";
    case PROP_LAYER_MEMBER:
      return "PROP_LAYER_MEMBER";
    case PROP_PASSWORD:
      return "PROP_PASSWORD";
    case PROP_POWER:
      return "PROP_POWER";
    default: {
      /* in case we don't have a type preset that includes the subtype */
      if (RNA_SUBTYPE_UNIT(type)) {
        return rna_property_subtypename(type & ~RNA_SUBTYPE_UNIT(type));
      }
      else {
        return "PROP_SUBTYPE_UNKNOWN";
      }
    }
  }
}

static const char *rna_property_subtype_unit(PropertySubType type)
{
  switch (RNA_SUBTYPE_UNIT(type)) {
    case PROP_UNIT_NONE:
      return "PROP_UNIT_NONE";
    case PROP_UNIT_LENGTH:
      return "PROP_UNIT_LENGTH";
    case PROP_UNIT_AREA:
      return "PROP_UNIT_AREA";
    case PROP_UNIT_VOLUME:
      return "PROP_UNIT_VOLUME";
    case PROP_UNIT_MASS:
      return "PROP_UNIT_MASS";
    case PROP_UNIT_ROTATION:
      return "PROP_UNIT_ROTATION";
    case PROP_UNIT_TIME:
      return "PROP_UNIT_TIME";
    case PROP_UNIT_VELOCITY:
      return "PROP_UNIT_VELOCITY";
    case PROP_UNIT_ACCELERATION:
      return "PROP_UNIT_ACCELERATION";
    case PROP_UNIT_CAMERA:
      return "PROP_UNIT_CAMERA";
    case PROP_UNIT_POWER:
      return "PROP_UNIT_POWER";
    default:
      return "PROP_UNIT_UNKNOWN";
  }
}

static void rna_generate_prototypes(BlenderRNA *brna, FILE *f)
{
  StructRNA *srna;

  for (srna = brna->structs.first; srna; srna = srna->cont.next) {
    fprintf(f, "extern StructRNA RNA_%s;\n", srna->identifier);
  }
  fprintf(f, "\n");
}

static void rna_generate_blender(BlenderRNA *brna, FILE *f)
{
  StructRNA *srna;

  fprintf(f,
          "BlenderRNA BLENDER_RNA = {\n"
          "\t.structs = {");
  srna = brna->structs.first;
  if (srna) {
    fprintf(f, "&RNA_%s, ", srna->identifier);
  }
  else {
    fprintf(f, "NULL, ");
  }

  srna = brna->structs.last;
  if (srna) {
    fprintf(f, "&RNA_%s},\n", srna->identifier);
  }
  else {
    fprintf(f, "NULL},\n");
  }

  fprintf(f,
          "\t.structs_map = NULL,\n"
          "\t.structs_len = 0,\n"
          "};\n\n");
}

static void rna_generate_property_prototypes(BlenderRNA *UNUSED(brna), StructRNA *srna, FILE *f)
{
  PropertyRNA *prop;
  StructRNA *base;

  base = srna->base;
  while (base) {
    fprintf(f, "\n");
    for (prop = base->cont.properties.first; prop; prop = prop->next) {
      fprintf(f,
              "%s%s rna_%s_%s;\n",
              "extern ",
              rna_property_structname(prop->type),
              base->identifier,
              prop->identifier);
    }
    base = base->base;
  }

  if (srna->cont.properties.first) {
    fprintf(f, "\n");
  }

  for (prop = srna->cont.properties.first; prop; prop = prop->next) {
    fprintf(f,
            "%s rna_%s_%s;\n",
            rna_property_structname(prop->type),
            srna->identifier,
            prop->identifier);
  }
  fprintf(f, "\n");
}

static void rna_generate_parameter_prototypes(BlenderRNA *UNUSED(brna),
                                              StructRNA *srna,
                                              FunctionRNA *func,
                                              FILE *f)
{
  PropertyRNA *parm;

  for (parm = func->cont.properties.first; parm; parm = parm->next) {
    fprintf(f,
            "%s%s rna_%s_%s_%s;\n",
            "extern ",
            rna_property_structname(parm->type),
            srna->identifier,
            func->identifier,
            parm->identifier);
  }

  if (func->cont.properties.first) {
    fprintf(f, "\n");
  }
}

static void rna_generate_function_prototypes(BlenderRNA *brna, StructRNA *srna, FILE *f)
{
  FunctionRNA *func;
  StructRNA *base;

  base = srna->base;
  while (base) {
    for (func = base->functions.first; func; func = func->cont.next) {
      fprintf(f,
              "%s%s rna_%s_%s_func;\n",
              "extern ",
              "FunctionRNA",
              base->identifier,
              func->identifier);
      rna_generate_parameter_prototypes(brna, base, func, f);
    }

    if (base->functions.first) {
      fprintf(f, "\n");
    }

    base = base->base;
  }

  for (func = srna->functions.first; func; func = func->cont.next) {
    fprintf(
        f, "%s%s rna_%s_%s_func;\n", "extern ", "FunctionRNA", srna->identifier, func->identifier);
    rna_generate_parameter_prototypes(brna, srna, func, f);
  }

  if (srna->functions.first) {
    fprintf(f, "\n");
  }
}

static void rna_generate_static_parameter_prototypes(FILE *f,
                                                     StructRNA *srna,
                                                     FunctionDefRNA *dfunc,
                                                     const char *name_override,
                                                     int close_prototype)
{
  FunctionRNA *func;
  PropertyDefRNA *dparm;
  StructDefRNA *dsrna;
  PropertyType type;
  int flag, flag_parameter, pout, cptr, first;
  const char *ptrstr;

  dsrna = rna_find_struct_def(srna);
  func = dfunc->func;

  /* return type */
  for (dparm = dfunc->cont.properties.first; dparm; dparm = dparm->next) {
    if (dparm->prop == func->c_ret) {
      if (dparm->prop->arraydimension) {
        fprintf(f, "XXX no array return types yet"); /* XXX not supported */
      }
      else if (dparm->prop->type == PROP_POINTER && !(dparm->prop->flag_parameter & PARM_RNAPTR)) {
        fprintf(f, "%s%s *", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop));
      }
      else {
        fprintf(f, "%s%s ", rna_type_struct(dparm->prop), rna_parameter_type_name(dparm->prop));
      }

      break;
    }
  }

  /* void if nothing to return */
  if (!dparm) {
    fprintf(f, "void ");
  }

  /* function name */
  if (name_override == NULL || name_override[0] == '\0') {
    fprintf(f, "%s(", dfunc->call);
  }
  else {
    fprintf(f, "%s(", name_override);
  }

  first = 1;

  /* self, context and reports parameters */
  if (func->flag & FUNC_USE_SELF_ID) {
    fprintf(f, "struct ID *_selfid");
    first = 0;
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if (!first) {
      fprintf(f, ", ");
    }
    if (dsrna->dnafromprop) {
      fprintf(f, "struct %s *_self", dsrna->dnafromname);
    }
    else if (dsrna->dnaname) {
      fprintf(f, "struct %s *_self", dsrna->dnaname);
    }
    else {
      fprintf(f, "struct %s *_self", srna->identifier);
    }
    first = 0;
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    if (!first) {
      fprintf(f, ", ");
    }
    fprintf(f, "struct StructRNA *_type");
    first = 0;
  }

  if (func->flag & FUNC_USE_MAIN) {
    if (!first) {
      fprintf(f, ", ");
    }
    first = 0;
    fprintf(f, "Main *bmain");
  }

  if (func->flag & FUNC_USE_CONTEXT) {
    if (!first) {
      fprintf(f, ", ");
    }
    first = 0;
    fprintf(f, "bContext *C");
  }

  if (func->flag & FUNC_USE_REPORTS) {
    if (!first) {
      fprintf(f, ", ");
    }
    first = 0;
    fprintf(f, "ReportList *reports");
  }

  /* defined parameters */
  for (dparm = dfunc->cont.properties.first; dparm; dparm = dparm->next) {
    type = dparm->prop->type;
    flag = dparm->prop->flag;
    flag_parameter = dparm->prop->flag_parameter;
    pout = (flag_parameter & PARM_OUTPUT);
    cptr = ((type == PROP_POINTER) && !(flag_parameter & PARM_RNAPTR));

    if (dparm->prop == func->c_ret) {
      continue;
    }

    if (cptr || (flag & PROP_DYNAMIC)) {
      ptrstr = pout ? "**" : "*";
    }
    else if (type == PROP_POINTER || dparm->prop->arraydimension) {
      ptrstr = "*";
    }
    else if (type == PROP_STRING && (flag & PROP_THICK_WRAP)) {
      ptrstr = "";
    }
    else {
      ptrstr = pout ? "*" : "";
    }

    if (!first) {
      fprintf(f, ", ");
    }
    first = 0;

    if (flag & PROP_DYNAMIC) {
      fprintf(f, "int %s%s_len, ", pout ? "*" : "", dparm->prop->identifier);
    }

    if (!(flag & PROP_DYNAMIC) && dparm->prop->arraydimension) {
      fprintf(f,
              "%s%s %s[%u]",
              rna_type_struct(dparm->prop),
              rna_parameter_type_name(dparm->prop),
              rna_safe_id(dparm->prop->identifier),
              dparm->prop->totarraylength);
    }
    else {
      fprintf(f,
              "%s%s %s%s",
              rna_type_struct(dparm->prop),
              rna_parameter_type_name(dparm->prop),
              ptrstr,
              rna_safe_id(dparm->prop->identifier));
    }
  }

  /* ensure func(void) if there are no args */
  if (first) {
    fprintf(f, "void");
  }

  fprintf(f, ")");

  if (close_prototype) {
    fprintf(f, ";\n");
  }
}

static void rna_generate_static_function_prototypes(BlenderRNA *UNUSED(brna),
                                                    StructRNA *srna,
                                                    FILE *f)
{
  FunctionRNA *func;
  FunctionDefRNA *dfunc;
  int first = 1;

  for (func = srna->functions.first; func; func = func->cont.next) {
    dfunc = rna_find_function_def(func);

    if (dfunc->call) {
      if (first) {
        fprintf(f, "/* Repeated prototypes to detect errors */\n\n");
        first = 0;
      }

      rna_generate_static_parameter_prototypes(f, srna, dfunc, NULL, 1);
    }
  }

  fprintf(f, "\n");
}

static void rna_generate_struct_prototypes(FILE *f)
{
  StructDefRNA *ds;
  PropertyDefRNA *dp;
  FunctionDefRNA *dfunc;
  const char *structures[2048];
  int all_structures = 0;

  /* structures definitions */
  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    for (dfunc = ds->functions.first; dfunc; dfunc = dfunc->cont.next) {
      if (dfunc->call) {
        for (dp = dfunc->cont.properties.first; dp; dp = dp->next) {
          if (dp->prop->type == PROP_POINTER) {
            int a, found = 0;
            const char *struct_name = rna_parameter_type_name(dp->prop);
            if (struct_name == NULL) {
              printf("No struct found for property '%s'\n", dp->prop->identifier);
              exit(1);
            }

            for (a = 0; a < all_structures; a++) {
              if (STREQ(struct_name, structures[a])) {
                found = 1;
                break;
              }
            }

            if (found == 0) {
              fprintf(f, "struct %s;\n", struct_name);

              if (all_structures >= sizeof(structures) / sizeof(structures[0])) {
                printf("Array size to store all structures names is too small\n");
                exit(1);
              }

              structures[all_structures++] = struct_name;
            }
          }
        }
      }
    }
  }

  fprintf(f, "\n");
}

static void rna_generate_property(FILE *f, StructRNA *srna, const char *nest, PropertyRNA *prop)
{
  char *strnest = (char *)"", *errnest = (char *)"";
  int len, freenest = 0;

  if (nest != NULL) {
    len = strlen(nest);

    strnest = MEM_mallocN(sizeof(char) * (len + 2), "rna_generate_property -> strnest");
    errnest = MEM_mallocN(sizeof(char) * (len + 2), "rna_generate_property -> errnest");

    strcpy(strnest, "_");
    strcat(strnest, nest);
    strcpy(errnest, ".");
    strcat(errnest, nest);

    freenest = 1;
  }

  switch (prop->type) {
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      int i, defaultfound = 0, totflag = 0;

      if (eprop->item) {
        fprintf(f,
                "static const EnumPropertyItem rna_%s%s_%s_items[%d] = {\n\t",
                srna->identifier,
                strnest,
                prop->identifier,
                eprop->totitem + 1);

        for (i = 0; i < eprop->totitem; i++) {
          fprintf(f, "{%d, ", eprop->item[i].value);
          rna_print_c_string(f, eprop->item[i].identifier);
          fprintf(f, ", ");
          fprintf(f, "%d, ", eprop->item[i].icon);
          rna_print_c_string(f, eprop->item[i].name);
          fprintf(f, ", ");
          rna_print_c_string(f, eprop->item[i].description);
          fprintf(f, "},\n\t");

          if (eprop->item[i].identifier[0]) {
            if (prop->flag & PROP_ENUM_FLAG) {
              totflag |= eprop->item[i].value;
            }
            else {
              if (eprop->defaultvalue == eprop->item[i].value) {
                defaultfound = 1;
              }
            }
          }
        }

        fprintf(f, "{0, NULL, 0, NULL, NULL}\n};\n\n");

        if (prop->flag & PROP_ENUM_FLAG) {
          if (eprop->defaultvalue & ~totflag) {
            CLOG_ERROR(&LOG,
                       "%s%s.%s, enum default includes unused bits (%d).",
                       srna->identifier,
                       errnest,
                       prop->identifier,
                       eprop->defaultvalue & ~totflag);
            DefRNA.error = 1;
          }
        }
        else {
          if (!defaultfound && !(eprop->itemf && eprop->item == DummyRNA_NULL_items)) {
            CLOG_ERROR(&LOG,
                       "%s%s.%s, enum default is not in items.",
                       srna->identifier,
                       errnest,
                       prop->identifier);
            DefRNA.error = 1;
          }
        }
      }
      else {
        CLOG_ERROR(&LOG,
                   "%s%s.%s, enum must have items defined.",
                   srna->identifier,
                   errnest,
                   prop->identifier);
        DefRNA.error = 1;
      }
      break;
    }
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
      unsigned int i;

      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f,
                "static bool rna_%s%s_%s_default[%u] = {\n\t",
                srna->identifier,
                strnest,
                prop->identifier,
                prop->totarraylength);

        for (i = 0; i < prop->totarraylength; i++) {
          if (bprop->defaultarray) {
            fprintf(f, "%d", bprop->defaultarray[i]);
          }
          else {
            fprintf(f, "%d", bprop->defaultvalue);
          }
          if (i != prop->totarraylength - 1) {
            fprintf(f, ",\n\t");
          }
        }

        fprintf(f, "\n};\n\n");
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
      unsigned int i;

      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f,
                "static int rna_%s%s_%s_default[%u] = {\n\t",
                srna->identifier,
                strnest,
                prop->identifier,
                prop->totarraylength);

        for (i = 0; i < prop->totarraylength; i++) {
          if (iprop->defaultarray) {
            fprintf(f, "%d", iprop->defaultarray[i]);
          }
          else {
            fprintf(f, "%d", iprop->defaultvalue);
          }
          if (i != prop->totarraylength - 1) {
            fprintf(f, ",\n\t");
          }
        }

        fprintf(f, "\n};\n\n");
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
      unsigned int i;

      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f,
                "static float rna_%s%s_%s_default[%u] = {\n\t",
                srna->identifier,
                strnest,
                prop->identifier,
                prop->totarraylength);

        for (i = 0; i < prop->totarraylength; i++) {
          if (fprop->defaultarray) {
            rna_float_print(f, fprop->defaultarray[i]);
          }
          else {
            rna_float_print(f, fprop->defaultvalue);
          }
          if (i != prop->totarraylength - 1) {
            fprintf(f, ",\n\t");
          }
        }

        fprintf(f, "\n};\n\n");
      }
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

      /* XXX This systematically enforces that flag on ID pointers...
       * we'll probably have to revisit. :/ */
      StructRNA *type = rna_find_struct((const char *)pprop->type);
      if (type && (type->flag & STRUCT_ID)) {
        prop->flag |= PROP_PTR_NO_OWNERSHIP;
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

      /* XXX This systematically enforces that flag on ID pointers...
       * we'll probably have to revisit. :/ */
      StructRNA *type = rna_find_struct((const char *)cprop->item_type);
      if (type && (type->flag & STRUCT_ID)) {
        prop->flag |= PROP_PTR_NO_OWNERSHIP;
      }
      break;
    }
    default:
      break;
  }

  fprintf(f,
          "%s rna_%s%s_%s = {\n",
          rna_property_structname(prop->type),
          srna->identifier,
          strnest,
          prop->identifier);

  if (prop->next) {
    fprintf(
        f, "\t{(PropertyRNA *)&rna_%s%s_%s, ", srna->identifier, strnest, prop->next->identifier);
  }
  else {
    fprintf(f, "\t{NULL, ");
  }
  if (prop->prev) {
    fprintf(
        f, "(PropertyRNA *)&rna_%s%s_%s,\n", srna->identifier, strnest, prop->prev->identifier);
  }
  else {
    fprintf(f, "NULL,\n");
  }
  fprintf(f, "\t%d, ", prop->magic);
  rna_print_c_string(f, prop->identifier);
  fprintf(f,
          ", %d, %d, %d, %d, %d, ",
          prop->flag,
          prop->flag_override,
          prop->flag_parameter,
          prop->flag_internal,
          prop->tags);
  rna_print_c_string(f, prop->name);
  fprintf(f, ",\n\t");
  rna_print_c_string(f, prop->description);
  fprintf(f, ",\n\t");
  fprintf(f, "%d, ", prop->icon);
  rna_print_c_string(f, prop->translation_context);
  fprintf(f, ",\n");
  fprintf(f,
          "\t%s, %s | %s, %s, %u, {%u, %u, %u}, %u,\n",
          RNA_property_typename(prop->type),
          rna_property_subtypename(prop->subtype),
          rna_property_subtype_unit(prop->subtype),
          rna_function_string(prop->getlength),
          prop->arraydimension,
          prop->arraylength[0],
          prop->arraylength[1],
          prop->arraylength[2],
          prop->totarraylength);
  fprintf(f,
          "\t%s%s, %d, %s, %s, %s, %s, %s,\n",
          (prop->flag & PROP_CONTEXT_UPDATE) ? "(UpdateFunc)" : "",
          rna_function_string(prop->update),
          prop->noteflag,
          rna_function_string(prop->editable),
          rna_function_string(prop->itemeditable),
          rna_function_string(prop->override_diff),
          rna_function_string(prop->override_store),
          rna_function_string(prop->override_apply));

  if (prop->flag_internal & PROP_INTERN_RAW_ACCESS) {
    rna_set_raw_offset(f, srna, prop);
  }
  else {
    fprintf(f, "\t0, -1");
  }

  /* our own type - collections/arrays only */
  if (prop->srna) {
    fprintf(f, ", &RNA_%s", (const char *)prop->srna);
  }
  else {
    fprintf(f, ", NULL");
  }

  fprintf(f, "},\n");

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, %d, ",
              rna_function_string(bprop->get),
              rna_function_string(bprop->set),
              rna_function_string(bprop->getarray),
              rna_function_string(bprop->setarray),
              rna_function_string(bprop->get_ex),
              rna_function_string(bprop->set_ex),
              rna_function_string(bprop->getarray_ex),
              rna_function_string(bprop->setarray_ex),
              bprop->defaultvalue);
      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
      }
      else {
        fprintf(f, "NULL\n");
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s,\n\t",
              rna_function_string(iprop->get),
              rna_function_string(iprop->set),
              rna_function_string(iprop->getarray),
              rna_function_string(iprop->setarray),
              rna_function_string(iprop->range),
              rna_function_string(iprop->get_ex),
              rna_function_string(iprop->set_ex),
              rna_function_string(iprop->getarray_ex),
              rna_function_string(iprop->setarray_ex),
              rna_function_string(iprop->range_ex));
      rna_int_print(f, iprop->softmin);
      fprintf(f, ", ");
      rna_int_print(f, iprop->softmax);
      fprintf(f, ", ");
      rna_int_print(f, iprop->hardmin);
      fprintf(f, ", ");
      rna_int_print(f, iprop->hardmax);
      fprintf(f, ", ");
      rna_int_print(f, iprop->step);
      fprintf(f, ", ");
      rna_int_print(f, iprop->defaultvalue);
      fprintf(f, ", ");
      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
      }
      else {
        fprintf(f, "NULL\n");
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ",
              rna_function_string(fprop->get),
              rna_function_string(fprop->set),
              rna_function_string(fprop->getarray),
              rna_function_string(fprop->setarray),
              rna_function_string(fprop->range),
              rna_function_string(fprop->get_ex),
              rna_function_string(fprop->set_ex),
              rna_function_string(fprop->getarray_ex),
              rna_function_string(fprop->setarray_ex),
              rna_function_string(fprop->range_ex));
      rna_float_print(f, fprop->softmin);
      fprintf(f, ", ");
      rna_float_print(f, fprop->softmax);
      fprintf(f, ", ");
      rna_float_print(f, fprop->hardmin);
      fprintf(f, ", ");
      rna_float_print(f, fprop->hardmax);
      fprintf(f, ", ");
      rna_float_print(f, fprop->step);
      fprintf(f, ", ");
      rna_int_print(f, (int)fprop->precision);
      fprintf(f, ", ");
      rna_float_print(f, fprop->defaultvalue);
      fprintf(f, ", ");
      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
      }
      else {
        fprintf(f, "NULL\n");
      }
      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %d, ",
              rna_function_string(sprop->get),
              rna_function_string(sprop->length),
              rna_function_string(sprop->set),
              rna_function_string(sprop->get_ex),
              rna_function_string(sprop->length_ex),
              rna_function_string(sprop->set_ex),
              sprop->maxlength);
      rna_print_c_string(f, sprop->defaultvalue);
      fprintf(f, "\n");
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, NULL, ",
              rna_function_string(eprop->get),
              rna_function_string(eprop->set),
              rna_function_string(eprop->itemf),
              rna_function_string(eprop->get_ex),
              rna_function_string(eprop->set_ex));
      if (eprop->item) {
        fprintf(f, "rna_%s%s_%s_items, ", srna->identifier, strnest, prop->identifier);
      }
      else {
        fprintf(f, "NULL, ");
      }
      fprintf(f, "%d, %d\n", eprop->totitem, eprop->defaultvalue);
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s,",
              rna_function_string(pprop->get),
              rna_function_string(pprop->set),
              rna_function_string(pprop->typef),
              rna_function_string(pprop->poll));
      if (pprop->type) {
        fprintf(f, "&RNA_%s\n", (const char *)pprop->type);
      }
      else {
        fprintf(f, "NULL\n");
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, ",
              rna_function_string(cprop->begin),
              rna_function_string(cprop->next),
              rna_function_string(cprop->end),
              rna_function_string(cprop->get),
              rna_function_string(cprop->length),
              rna_function_string(cprop->lookupint),
              rna_function_string(cprop->lookupstring),
              rna_function_string(cprop->assignint));
      if (cprop->item_type) {
        fprintf(f, "&RNA_%s\n", (const char *)cprop->item_type);
      }
      else {
        fprintf(f, "NULL\n");
      }
      break;
    }
  }

  fprintf(f, "};\n\n");

  if (freenest) {
    MEM_freeN(strnest);
    MEM_freeN(errnest);
  }
}

static void rna_generate_struct(BlenderRNA *UNUSED(brna), StructRNA *srna, FILE *f)
{
  FunctionRNA *func;
  FunctionDefRNA *dfunc;
  PropertyRNA *prop, *parm;
  StructRNA *base;

  fprintf(f, "/* %s */\n", srna->name);

  for (prop = srna->cont.properties.first; prop; prop = prop->next) {
    rna_generate_property(f, srna, NULL, prop);
  }

  for (func = srna->functions.first; func; func = func->cont.next) {
    for (parm = func->cont.properties.first; parm; parm = parm->next) {
      rna_generate_property(f, srna, func->identifier, parm);
    }

    fprintf(f, "%s%s rna_%s_%s_func = {\n", "", "FunctionRNA", srna->identifier, func->identifier);

    if (func->cont.next) {
      fprintf(f,
              "\t{(FunctionRNA *)&rna_%s_%s_func, ",
              srna->identifier,
              ((FunctionRNA *)func->cont.next)->identifier);
    }
    else {
      fprintf(f, "\t{NULL, ");
    }
    if (func->cont.prev) {
      fprintf(f,
              "(FunctionRNA *)&rna_%s_%s_func,\n",
              srna->identifier,
              ((FunctionRNA *)func->cont.prev)->identifier);
    }
    else {
      fprintf(f, "NULL,\n");
    }

    fprintf(f, "\tNULL,\n");

    parm = func->cont.properties.first;
    if (parm) {
      fprintf(f,
              "\t{(PropertyRNA *)&rna_%s_%s_%s, ",
              srna->identifier,
              func->identifier,
              parm->identifier);
    }
    else {
      fprintf(f, "\t{NULL, ");
    }

    parm = func->cont.properties.last;
    if (parm) {
      fprintf(f,
              "(PropertyRNA *)&rna_%s_%s_%s}},\n",
              srna->identifier,
              func->identifier,
              parm->identifier);
    }
    else {
      fprintf(f, "NULL}},\n");
    }

    fprintf(f, "\t");
    rna_print_c_string(f, func->identifier);
    fprintf(f, ", %d, ", func->flag);
    rna_print_c_string(f, func->description);
    fprintf(f, ",\n");

    dfunc = rna_find_function_def(func);
    if (dfunc->gencall) {
      fprintf(f, "\t%s,\n", dfunc->gencall);
    }
    else {
      fprintf(f, "\tNULL,\n");
    }

    if (func->c_ret) {
      fprintf(f,
              "\t(PropertyRNA *)&rna_%s_%s_%s\n",
              srna->identifier,
              func->identifier,
              func->c_ret->identifier);
    }
    else {
      fprintf(f, "\tNULL\n");
    }

    fprintf(f, "};\n");
    fprintf(f, "\n");
  }

  fprintf(f, "StructRNA RNA_%s = {\n", srna->identifier);

  if (srna->cont.next) {
    fprintf(f, "\t{(ContainerRNA *)&RNA_%s, ", ((StructRNA *)srna->cont.next)->identifier);
  }
  else {
    fprintf(f, "\t{NULL, ");
  }
  if (srna->cont.prev) {
    fprintf(f, "(ContainerRNA *)&RNA_%s,\n", ((StructRNA *)srna->cont.prev)->identifier);
  }
  else {
    fprintf(f, "NULL,\n");
  }

  fprintf(f, "\tNULL,\n");

  prop = srna->cont.properties.first;
  if (prop) {
    fprintf(f, "\t{(PropertyRNA *)&rna_%s_%s, ", srna->identifier, prop->identifier);
  }
  else {
    fprintf(f, "\t{NULL, ");
  }

  prop = srna->cont.properties.last;
  if (prop) {
    fprintf(f, "(PropertyRNA *)&rna_%s_%s}},\n", srna->identifier, prop->identifier);
  }
  else {
    fprintf(f, "NULL}},\n");
  }
  fprintf(f, "\t");
  rna_print_c_string(f, srna->identifier);
  fprintf(f, ", NULL, NULL"); /* PyType - Cant initialize here */
  fprintf(f, ", %d, NULL, ", srna->flag);
  rna_print_c_string(f, srna->name);
  fprintf(f, ",\n\t");
  rna_print_c_string(f, srna->description);
  fprintf(f, ",\n\t");
  rna_print_c_string(f, srna->translation_context);
  fprintf(f, ", %d,\n", srna->icon);

  prop = srna->nameproperty;
  if (prop) {
    base = srna;
    while (base->base && base->base->nameproperty == prop) {
      base = base->base;
    }

    fprintf(f, "\t(PropertyRNA *)&rna_%s_%s, ", base->identifier, prop->identifier);
  }
  else {
    fprintf(f, "\tNULL, ");
  }

  prop = srna->iteratorproperty;
  base = srna;
  while (base->base && base->base->iteratorproperty == prop) {
    base = base->base;
  }
  fprintf(f, "(PropertyRNA *)&rna_%s_rna_properties,\n", base->identifier);

  if (srna->base) {
    fprintf(f, "\t&RNA_%s,\n", srna->base->identifier);
  }
  else {
    fprintf(f, "\tNULL,\n");
  }

  if (srna->nested) {
    fprintf(f, "\t&RNA_%s,\n", srna->nested->identifier);
  }
  else {
    fprintf(f, "\tNULL,\n");
  }

  fprintf(f, "\t%s,\n", rna_function_string(srna->refine));
  fprintf(f, "\t%s,\n", rna_function_string(srna->path));
  fprintf(f, "\t%s,\n", rna_function_string(srna->reg));
  fprintf(f, "\t%s,\n", rna_function_string(srna->unreg));
  fprintf(f, "\t%s,\n", rna_function_string(srna->instance));
  fprintf(f, "\t%s,\n", rna_function_string(srna->idproperties));

  if (srna->reg && !srna->refine) {
    CLOG_ERROR(
        &LOG, "%s has a register function, must also have refine function.", srna->identifier);
    DefRNA.error = 1;
  }

  func = srna->functions.first;
  if (func) {
    fprintf(f, "\t{(FunctionRNA *)&rna_%s_%s_func, ", srna->identifier, func->identifier);
  }
  else {
    fprintf(f, "\t{NULL, ");
  }

  func = srna->functions.last;
  if (func) {
    fprintf(f, "(FunctionRNA *)&rna_%s_%s_func}\n", srna->identifier, func->identifier);
  }
  else {
    fprintf(f, "NULL}\n");
  }

  fprintf(f, "};\n");

  fprintf(f, "\n");
}

typedef struct RNAProcessItem {
  const char *filename;
  const char *api_filename;
  void (*define)(BlenderRNA *brna);
} RNAProcessItem;

static RNAProcessItem PROCESS_ITEMS[] = {
    {"rna_rna.c", NULL, RNA_def_rna},
    {"rna_ID.c", NULL, RNA_def_ID},
    {"rna_texture.c", "rna_texture_api.c", RNA_def_texture},
    {"rna_action.c", "rna_action_api.c", RNA_def_action},
    {"rna_animation.c", "rna_animation_api.c", RNA_def_animation},
    {"rna_animviz.c", NULL, RNA_def_animviz},
    {"rna_armature.c", "rna_armature_api.c", RNA_def_armature},
    {"rna_boid.c", NULL, RNA_def_boid},
    {"rna_brush.c", NULL, RNA_def_brush},
    {"rna_cachefile.c", NULL, RNA_def_cachefile},
    {"rna_camera.c", "rna_camera_api.c", RNA_def_camera},
    {"rna_cloth.c", NULL, RNA_def_cloth},
    {"rna_collection.c", NULL, RNA_def_collections},
    {"rna_color.c", NULL, RNA_def_color},
    {"rna_constraint.c", NULL, RNA_def_constraint},
    {"rna_context.c", NULL, RNA_def_context},
    {"rna_curve.c", "rna_curve_api.c", RNA_def_curve},
    {"rna_dynamicpaint.c", NULL, RNA_def_dynamic_paint},
    {"rna_fcurve.c", "rna_fcurve_api.c", RNA_def_fcurve},
    {"rna_fluidsim.c", NULL, RNA_def_fluidsim},
    {"rna_gpencil.c", NULL, RNA_def_gpencil},
    {"rna_image.c", "rna_image_api.c", RNA_def_image},
    {"rna_key.c", NULL, RNA_def_key},
    {"rna_light.c", NULL, RNA_def_light},
    {"rna_lattice.c", "rna_lattice_api.c", RNA_def_lattice},
    {"rna_layer.c", NULL, RNA_def_view_layer},
    {"rna_linestyle.c", NULL, RNA_def_linestyle},
    {"rna_main.c", "rna_main_api.c", RNA_def_main},
    {"rna_material.c", "rna_material_api.c", RNA_def_material},
    {"rna_mesh.c", "rna_mesh_api.c", RNA_def_mesh},
    {"rna_meta.c", "rna_meta_api.c", RNA_def_meta},
    {"rna_modifier.c", NULL, RNA_def_modifier},
    {"rna_gpencil_modifier.c", NULL, RNA_def_greasepencil_modifier},
    {"rna_shader_fx.c", NULL, RNA_def_shader_fx},
    {"rna_nla.c", NULL, RNA_def_nla},
    {"rna_nodetree.c", NULL, RNA_def_nodetree},
    {"rna_object.c", "rna_object_api.c", RNA_def_object},
    {"rna_object_force.c", NULL, RNA_def_object_force},
    {"rna_depsgraph.c", NULL, RNA_def_depsgraph},
    {"rna_packedfile.c", NULL, RNA_def_packedfile},
    {"rna_palette.c", NULL, RNA_def_palette},
    {"rna_particle.c", NULL, RNA_def_particle},
    {"rna_pose.c", "rna_pose_api.c", RNA_def_pose},
    {"rna_lightprobe.c", NULL, RNA_def_lightprobe},
    {"rna_render.c", NULL, RNA_def_render},
    {"rna_rigidbody.c", NULL, RNA_def_rigidbody},
    {"rna_scene.c", "rna_scene_api.c", RNA_def_scene},
    {"rna_screen.c", NULL, RNA_def_screen},
    {"rna_sculpt_paint.c", NULL, RNA_def_sculpt_paint},
    {"rna_sequencer.c", "rna_sequencer_api.c", RNA_def_sequencer},
    {"rna_smoke.c", NULL, RNA_def_smoke},
    {"rna_space.c", "rna_space_api.c", RNA_def_space},
    {"rna_speaker.c", NULL, RNA_def_speaker},
    {"rna_test.c", NULL, RNA_def_test},
    {"rna_text.c", "rna_text_api.c", RNA_def_text},
    {"rna_timeline.c", NULL, RNA_def_timeline_marker},
    {"rna_sound.c", "rna_sound_api.c", RNA_def_sound},
    {"rna_ui.c", "rna_ui_api.c", RNA_def_ui},
    {"rna_userdef.c", NULL, RNA_def_userdef},
    {"rna_vfont.c", "rna_vfont_api.c", RNA_def_vfont},
    {"rna_wm.c", "rna_wm_api.c", RNA_def_wm},
    {"rna_wm_gizmo.c", "rna_wm_gizmo_api.c", RNA_def_wm_gizmo},
    {"rna_workspace.c", "rna_workspace_api.c", RNA_def_workspace},
    {"rna_world.c", NULL, RNA_def_world},
    {"rna_movieclip.c", NULL, RNA_def_movieclip},
    {"rna_tracking.c", NULL, RNA_def_tracking},
    {"rna_mask.c", NULL, RNA_def_mask},
    {NULL, NULL},
};

static void rna_generate(BlenderRNA *brna, FILE *f, const char *filename, const char *api_filename)
{
  StructDefRNA *ds;
  PropertyDefRNA *dp;
  FunctionDefRNA *dfunc;

  fprintf(f,
          "\n"
          "/* Automatically generated struct definitions for the Data API.\n"
          " * Do not edit manually, changes will be overwritten.           */\n\n"
          "#define RNA_RUNTIME\n\n");

  fprintf(f, "#include <float.h>\n");
  fprintf(f, "#include <stdio.h>\n");
  fprintf(f, "#include <limits.h>\n");
  fprintf(f, "#include <string.h>\n\n");
  fprintf(f, "#include <stddef.h>\n\n");

  fprintf(f, "#include \"MEM_guardedalloc.h\"\n\n");

  fprintf(f, "#include \"DNA_ID.h\"\n");
  fprintf(f, "#include \"DNA_scene_types.h\"\n");

  fprintf(f, "#include \"BLI_blenlib.h\"\n\n");
  fprintf(f, "#include \"BLI_utildefines.h\"\n\n");

  fprintf(f, "#include \"BKE_context.h\"\n");
  fprintf(f, "#include \"BKE_library.h\"\n");
  fprintf(f, "#include \"BKE_main.h\"\n");
  fprintf(f, "#include \"BKE_report.h\"\n");

  fprintf(f, "#include \"RNA_define.h\"\n");
  fprintf(f, "#include \"RNA_types.h\"\n");
  fprintf(f, "#include \"rna_internal.h\"\n\n");

  /* include the generated prototypes header */
  fprintf(f, "#include \"rna_prototypes_gen.h\"\n\n");

  fprintf(f, "#include \"%s\"\n", filename);
  if (api_filename) {
    fprintf(f, "#include \"%s\"\n", api_filename);
  }
  fprintf(f, "\n");

  /* we want the included C files to have warnings enabled but for the generated code
   * ignore unused-parameter warnings which are hard to prevent */
#if defined(__GNUC__) || defined(__clang__)
  fprintf(f, "#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n\n");
#endif

  fprintf(f, "/* Autogenerated Functions */\n\n");

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (!filename || ds->filename == filename) {
      rna_generate_property_prototypes(brna, ds->srna, f);
      rna_generate_function_prototypes(brna, ds->srna, f);
    }
  }

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (!filename || ds->filename == filename) {
      for (dp = ds->cont.properties.first; dp; dp = dp->next) {
        rna_def_property_funcs(f, ds->srna, dp);
      }
    }
  }

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (!filename || ds->filename == filename) {
      for (dp = ds->cont.properties.first; dp; dp = dp->next) {
        rna_def_property_wrapper_funcs(f, ds, dp);
      }

      for (dfunc = ds->functions.first; dfunc; dfunc = dfunc->cont.next) {
        rna_def_function_wrapper_funcs(f, ds, dfunc);
        rna_def_function_funcs(f, ds, dfunc);
      }

      rna_generate_static_function_prototypes(brna, ds->srna, f);
    }
  }

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (!filename || ds->filename == filename) {
      rna_generate_struct(brna, ds->srna, f);
    }
  }

  if (STREQ(filename, "rna_ID.c")) {
    /* this is ugly, but we cannot have c files compiled for both
     * makesrna and blender with some build systems at the moment */
    fprintf(f, "#include \"rna_define.c\"\n\n");

    rna_generate_blender(brna, f);
  }
}

static void rna_generate_header(BlenderRNA *UNUSED(brna), FILE *f)
{
  StructDefRNA *ds;
  PropertyDefRNA *dp;
  StructRNA *srna;
  FunctionDefRNA *dfunc;

  fprintf(f, "\n#ifndef __RNA_BLENDER_H__\n");
  fprintf(f, "#define __RNA_BLENDER_H__\n\n");

  fprintf(f,
          "/* Automatically generated function declarations for the Data API.\n"
          " * Do not edit manually, changes will be overwritten.              */\n\n");

  fprintf(f, "#include \"RNA_types.h\"\n\n");

  fprintf(f, "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n");

  fprintf(f, "#define FOREACH_BEGIN(property, sptr, itemptr) \\\n");
  fprintf(f, "    { \\\n");
  fprintf(f, "        CollectionPropertyIterator rna_macro_iter; \\\n");
  fprintf(f,
          "        for (property##_begin(&rna_macro_iter, sptr); rna_macro_iter.valid; "
          "property##_next(&rna_macro_iter)) { \\\n");
  fprintf(f, "            itemptr = rna_macro_iter.ptr;\n\n");

  fprintf(f, "#define FOREACH_END(property) \\\n");
  fprintf(f, "        } \\\n");
  fprintf(f, "        property##_end(&rna_macro_iter); \\\n");
  fprintf(f, "    }\n\n");

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    srna = ds->srna;

    fprintf(f, "/**************** %s ****************/\n\n", srna->name);

    while (srna) {
      fprintf(f, "extern StructRNA RNA_%s;\n", srna->identifier);
      srna = srna->base;
    }
    fprintf(f, "\n");

    for (dp = ds->cont.properties.first; dp; dp = dp->next) {
      rna_def_property_funcs_header(f, ds->srna, dp);
    }

    for (dfunc = ds->functions.first; dfunc; dfunc = dfunc->cont.next) {
      rna_def_function_funcs_header(f, ds->srna, dfunc);
    }
  }

  fprintf(f, "#ifdef __cplusplus\n}\n#endif\n\n");

  fprintf(f, "#endif /* __RNA_BLENDER_H__ */\n\n");
}

static const char *cpp_classes =
    ""
    "\n"
    "#include <stdlib.h> /* for malloc */\n"
    "#include <string>\n"
    "#include <string.h> /* for memcpy */\n"
    "\n"
    "namespace BL {\n"
    "\n"
    "#define BOOLEAN_PROPERTY(sname, identifier) \\\n"
    "    inline bool sname::identifier(void) { return sname##_##identifier##_get(&ptr) ? true: "
    "false; } \\\n"
    "    inline void sname::identifier(bool value) { sname##_##identifier##_set(&ptr, value); }\n"
    "\n"
    "#define BOOLEAN_ARRAY_PROPERTY(sname, size, identifier) \\\n"
    "    inline Array<bool, size> sname::identifier(void) \\\n"
    "        { Array<bool, size> ar; sname##_##identifier##_get(&ptr, ar.data); return ar; } \\\n"
    "    inline void sname::identifier(bool values[size]) \\\n"
    "        { sname##_##identifier##_set(&ptr, values); } \\\n"
    "\n"
    "#define BOOLEAN_DYNAMIC_ARRAY_PROPERTY(sname, identifier) \\\n"
    "    inline DynamicArray<bool> sname::identifier(void) { \\\n"
    "        int arraylen[3]; \\\n"
    "        int len = sname##_##identifier##_get_length(&ptr, arraylen); \\\n"
    "        DynamicArray<bool> ar(len); \\\n"
    "        sname##_##identifier##_get(&ptr, ar.data); \\\n"
    "        return ar; } \\\n"
    "    inline void sname::identifier(bool values[]) \\\n"
    "        { sname##_##identifier##_set(&ptr, values); } \\\n"
    "\n"
    "#define INT_PROPERTY(sname, identifier) \\\n"
    "    inline int sname::identifier(void) { return sname##_##identifier##_get(&ptr); } \\\n"
    "    inline void sname::identifier(int value) { sname##_##identifier##_set(&ptr, value); }\n"
    "\n"
    "#define INT_ARRAY_PROPERTY(sname, size, identifier) \\\n"
    "    inline Array<int, size> sname::identifier(void) \\\n"
    "        { Array<int, size> ar; sname##_##identifier##_get(&ptr, ar.data); return ar; } \\\n"
    "    inline void sname::identifier(int values[size]) \\\n"
    "        { sname##_##identifier##_set(&ptr, values); } \\\n"
    "\n"
    "#define INT_DYNAMIC_ARRAY_PROPERTY(sname, identifier) \\\n"
    "    inline DynamicArray<int> sname::identifier(void) { \\\n"
    "        int arraylen[3]; \\\n"
    "        int len = sname##_##identifier##_get_length(&ptr, arraylen); \\\n"
    "        DynamicArray<int> ar(len); \\\n"
    "        sname##_##identifier##_get(&ptr, ar.data); \\\n"
    "        return ar; } \\\n"
    "    inline void sname::identifier(int values[]) \\\n"
    "        { sname##_##identifier##_set(&ptr, values); } \\\n"
    "\n"
    "#define FLOAT_PROPERTY(sname, identifier) \\\n"
    "    inline float sname::identifier(void) { return sname##_##identifier##_get(&ptr); } \\\n"
    "    inline void sname::identifier(float value) { sname##_##identifier##_set(&ptr, value); }\n"
    "\n"
    "#define FLOAT_ARRAY_PROPERTY(sname, size, identifier) \\\n"
    "    inline Array<float, size> sname::identifier(void) \\\n"
    "        { Array<float, size> ar; sname##_##identifier##_get(&ptr, ar.data); return ar; } \\\n"
    "    inline void sname::identifier(float values[size]) \\\n"
    "        { sname##_##identifier##_set(&ptr, values); } \\\n"
    "\n"
    "#define FLOAT_DYNAMIC_ARRAY_PROPERTY(sname, identifier) \\\n"
    "    inline DynamicArray<float> sname::identifier(void) { \\\n"
    "        int arraylen[3]; \\\n"
    "        int len = sname##_##identifier##_get_length(&ptr, arraylen); \\\n"
    "        DynamicArray<float> ar(len); \\\n"
    "        sname##_##identifier##_get(&ptr, ar.data); \\\n"
    "        return ar; } \\\n"
    "    inline void sname::identifier(float values[]) \\\n"
    "        { sname##_##identifier##_set(&ptr, values); } \\\n"
    "\n"
    "#define ENUM_PROPERTY(type, sname, identifier) \\\n"
    "    inline sname::type sname::identifier(void) { return "
    "(type)sname##_##identifier##_get(&ptr); } \\\n"
    "    inline void sname::identifier(sname::type value) { sname##_##identifier##_set(&ptr, "
    "value); }\n"
    "\n"
    "#define STRING_PROPERTY(sname, identifier) \\\n"
    "    inline std::string sname::identifier(void) { \\\n"
    "        int len = sname##_##identifier##_length(&ptr); \\\n"
    "        std::string str; str.resize(len); \\\n"
    "        sname##_##identifier##_get(&ptr, &str[0]); return str; } \\\n"
    "    inline void sname::identifier(const std::string& value) { \\\n"
    "        sname##_##identifier##_set(&ptr, value.c_str()); } \\\n"
    "\n"
    "#define POINTER_PROPERTY(type, sname, identifier) \\\n"
    "    inline type sname::identifier(void) { return type(sname##_##identifier##_get(&ptr)); }\n"
    "\n"
    "#define COLLECTION_PROPERTY_LENGTH_false(sname, identifier) \\\n"
    "    inline static int sname##_##identifier##_length_wrap(PointerRNA *ptr) \\\n"
    "    { \\\n"
    "        CollectionPropertyIterator iter; \\\n"
    "        int length = 0; \\\n"
    "        sname##_##identifier##_begin(&iter, ptr); \\\n"
    "        while (iter.valid) { \\\n"
    "            sname##_##identifier##_next(&iter); \\\n"
    "            ++length; \\\n"
    "        } \\\n"
    "        sname##_##identifier##_end(&iter); \\\n"
    "        return length; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_LENGTH_true(sname, identifier) \\\n"
    "    inline static int sname##_##identifier##_length_wrap(PointerRNA *ptr) \\\n"
    "    { return sname##_##identifier##_length(ptr); } \n"
    "\n"
    "#define COLLECTION_PROPERTY_LOOKUP_INT_false(sname, identifier) \\\n"
    "    inline static int sname##_##identifier##_lookup_int_wrap(PointerRNA *ptr, int key, "
    "PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        CollectionPropertyIterator iter; \\\n"
    "        int i = 0, found = 0; \\\n"
    "        sname##_##identifier##_begin(&iter, ptr); \\\n"
    "        while (iter.valid) { \\\n"
    "            if (i == key) { \\\n"
    "                *r_ptr = iter.ptr; \\\n"
    "                found = 1; \\\n"
    "                break; \\\n"
    "            } \\\n"
    "            sname##_##identifier##_next(&iter); \\\n"
    "            ++i; \\\n"
    "        } \\\n"
    "        sname##_##identifier##_end(&iter); \\\n"
    "        if (!found) \\\n"
    "            memset(r_ptr, 0, sizeof(*r_ptr)); \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_LOOKUP_INT_true(sname, identifier) \\\n"
    "    inline static int sname##_##identifier##_lookup_int_wrap(PointerRNA *ptr, int key, "
    "PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        int found = sname##_##identifier##_lookup_int(ptr, key, r_ptr); \\\n"
    "        if (!found) \\\n"
    "            memset(r_ptr, 0, sizeof(*r_ptr)); \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_LOOKUP_STRING_false(sname, identifier) \\\n"
    "    inline static int sname##_##identifier##_lookup_string_wrap(PointerRNA *ptr, const char "
    "*key, PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        CollectionPropertyIterator iter; \\\n"
    "        int found = 0; \\\n"
    "        PropertyRNA *item_name_prop = RNA_struct_name_property(ptr->type); \\\n"
    "        sname##_##identifier##_begin(&iter, ptr); \\\n"
    "        while (iter.valid && !found) { \\\n"
    "            char name_fixed[32]; \\\n"
    "            const char *name; \\\n"
    "            int name_length; \\\n"
    "            name = RNA_property_string_get_alloc(&iter.ptr, item_name_prop, name_fixed, "
    "sizeof(name_fixed), &name_length); \\\n"
    "            if (!strncmp(name, key, name_length)) { \\\n"
    "                *r_ptr = iter.ptr; \\\n"
    "                found = 1; \\\n"
    "            } \\\n"
    "            if (name_fixed != name) \\\n"
    "                MEM_freeN((void *) name); \\\n"
    "            sname##_##identifier##_next(&iter); \\\n"
    "        } \\\n"
    "        sname##_##identifier##_end(&iter); \\\n"
    "        if (!found) \\\n"
    "            memset(r_ptr, 0, sizeof(*r_ptr)); \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_LOOKUP_STRING_true(sname, identifier) \\\n"
    "    inline static int sname##_##identifier##_lookup_string_wrap(PointerRNA *ptr, const char "
    "*key, PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        int found = sname##_##identifier##_lookup_string(ptr, key, r_ptr); \\\n"
    "        if (!found) \\\n"
    "            memset(r_ptr, 0, sizeof(*r_ptr)); \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY(collection_funcs, type, sname, identifier, has_length, "
    "has_lookup_int, has_lookup_string) \\\n"
    "    typedef CollectionIterator<type, sname##_##identifier##_begin, \\\n"
    "        sname##_##identifier##_next, sname##_##identifier##_end> identifier##_iterator; \\\n"
    "    COLLECTION_PROPERTY_LENGTH_##has_length(sname, identifier) \\\n"
    "    COLLECTION_PROPERTY_LOOKUP_INT_##has_lookup_int(sname, identifier) \\\n"
    "    COLLECTION_PROPERTY_LOOKUP_STRING_##has_lookup_string(sname, identifier) \\\n"
    "    CollectionRef<sname, type, sname##_##identifier##_begin, \\\n"
    "        sname##_##identifier##_next, sname##_##identifier##_end, \\\n"
    "        sname##_##identifier##_length_wrap, \\\n"
    "        sname##_##identifier##_lookup_int_wrap, sname##_##identifier##_lookup_string_wrap, "
    "collection_funcs> identifier;\n"
    "\n"
    "class Pointer {\n"
    "public:\n"
    "    Pointer(const PointerRNA &p) : ptr(p) { }\n"
    "    operator const PointerRNA&() { return ptr; }\n"
    "    bool is_a(StructRNA *type) { return RNA_struct_is_a(ptr.type, type) ? true: false; }\n"
    "    operator void*() { return ptr.data; }\n"
    "    operator bool() { return ptr.data != NULL; }\n"
    "\n"
    "    bool operator==(const Pointer &other) { return ptr.data == other.ptr.data; }\n"
    "    bool operator!=(const Pointer &other) { return ptr.data != other.ptr.data; }\n"
    "\n"
    "    PointerRNA ptr;\n"
    "};\n"
    "\n"
    "\n"
    "template<typename T, int Tsize>\n"
    "class Array {\n"
    "public:\n"
    "    T data[Tsize];\n"
    "\n"
    "    Array() {}\n"
    "    Array(const Array<T, Tsize>& other) { memcpy(data, other.data, sizeof(T) * Tsize); }\n"
    "    const Array<T, Tsize>& operator = (const Array<T, Tsize>& other) { memcpy(data, "
    "other.data, sizeof(T) * Tsize); "
    "return *this; }\n"
    "\n"
    "    operator T*() { return data; }\n"
    "    operator const T*() const { return data; }\n"
    "};\n"
    "\n"
    "template<typename T>\n"
    "class DynamicArray {\n"
    "public:\n"
    "    T *data;\n"
    "    int length;\n"
    "\n"
    "    DynamicArray() : data(NULL), length(0) {}\n"
    "    DynamicArray(int new_length) : data(NULL), length(new_length) { data = (T "
    "*)malloc(sizeof(T) * new_length); }\n"
    "    DynamicArray(const DynamicArray<T>& other) { copy_from(other); }\n"
    "    const DynamicArray<T>& operator = (const DynamicArray<T>& other) { copy_from(other); "
    "return *this; }\n"
    "\n"
    "    ~DynamicArray() { if (data) free(data); }\n"
    "\n"
    "    operator T*() { return data; }\n"
    "\n"
    "protected:\n"
    "    void copy_from(const DynamicArray<T>& other) {\n"
    "        if (data) free(data);\n"
    "        data = (T *)malloc(sizeof(T) * other.length);\n"
    "        memcpy(data, other.data, sizeof(T) * other.length);\n"
    "        length = other.length;\n"
    "    }\n"
    "};\n"
    "\n"
    "typedef void (*TBeginFunc)(CollectionPropertyIterator *iter, PointerRNA *ptr);\n"
    "typedef void (*TNextFunc)(CollectionPropertyIterator *iter);\n"
    "typedef void (*TEndFunc)(CollectionPropertyIterator *iter);\n"
    "typedef int (*TLengthFunc)(PointerRNA *ptr);\n"
    "typedef int (*TLookupIntFunc)(PointerRNA *ptr, int key, PointerRNA *r_ptr);\n"
    "typedef int (*TLookupStringFunc)(PointerRNA *ptr, const char *key, PointerRNA *r_ptr);\n"
    "\n"
    "template<typename T, TBeginFunc Tbegin, TNextFunc Tnext, TEndFunc Tend>\n"
    "class CollectionIterator {\n"
    "public:\n"
    "    CollectionIterator() : iter(), t(iter.ptr), init(false) { iter.valid = false; }\n"
    "    ~CollectionIterator(void) { if (init) Tend(&iter); };\n"
    "\n"
    "    operator bool(void)\n"
    "    { return iter.valid != 0; }\n"
    "    const CollectionIterator<T, Tbegin, Tnext, Tend>& operator++() { Tnext(&iter); t = "
    "T(iter.ptr); return *this; }\n"
    "\n"
    "    T& operator*(void) { return t; }\n"
    "    T* operator->(void) { return &t; }\n"
    "    bool operator == (const CollectionIterator<T, Tbegin, Tnext, Tend>& other) "
    "{ return iter.valid == other.iter.valid; }\n"
    "    bool operator!=(const CollectionIterator<T, Tbegin, Tnext, Tend>& other) "
    "{ return iter.valid != other.iter.valid; }\n"
    "\n"
    "    void begin(const Pointer &ptr)\n"
    "    { if (init) Tend(&iter); Tbegin(&iter, (PointerRNA *)&ptr.ptr); t = T(iter.ptr); init = "
    "true; }\n"
    "\n"
    "private:\n"
    "    const CollectionIterator<T, Tbegin, Tnext, Tend>& operator = "
    "(const CollectionIterator<T, Tbegin, Tnext, Tend>& /*copy*/) {}\n"
    ""
    "    CollectionPropertyIterator iter;\n"
    "    T t;\n"
    "    bool init;\n"
    "};\n"
    "\n"
    "template<typename Tp, typename T, TBeginFunc Tbegin, TNextFunc Tnext, TEndFunc Tend,\n"
    "         TLengthFunc Tlength, TLookupIntFunc Tlookup_int, TLookupStringFunc Tlookup_string,\n"
    "         typename Tcollection_funcs>\n"
    "class CollectionRef : public Tcollection_funcs {\n"
    "public:\n"
    "    CollectionRef(const PointerRNA &p) : Tcollection_funcs(p), ptr(p) {}\n"
    "\n"
    "    void begin(CollectionIterator<T, Tbegin, Tnext, Tend>& iter)\n"
    "    { iter.begin(ptr); }\n"
    "    CollectionIterator<T, Tbegin, Tnext, Tend> end()\n"
    "    { return CollectionIterator<T, Tbegin, Tnext, Tend>(); } /* test */ \n"
    ""
    "    int length()\n"
    "    { return Tlength(&ptr); }\n"
    "    T operator[](int key)\n"
    "    { PointerRNA r_ptr; Tlookup_int(&ptr, key, &r_ptr); return T(r_ptr); }\n"
    "    T operator[](const std::string &key)\n"
    "    { PointerRNA r_ptr; Tlookup_string(&ptr, key.c_str(), &r_ptr); return T(r_ptr); }\n"
    "\n"
    "private:\n"
    "    PointerRNA ptr;\n"
    "};\n"
    "\n"
    "class DefaultCollectionFunctions {\n"
    "public:\n"
    "    DefaultCollectionFunctions(const PointerRNA & /*p*/) {}\n"
    "};\n"
    "\n"
    "\n";

static int rna_is_collection_prop(PropertyRNA *prop)
{
  if (!(prop->flag & PROP_IDPROPERTY || prop->flag_internal & PROP_INTERN_BUILTIN)) {
    if (prop->type == PROP_COLLECTION) {
      return 1;
    }
  }

  return 0;
}

static int rna_is_collection_functions_struct(const char **collection_structs,
                                              const char *struct_name)
{
  int a = 0, found = 0;

  while (collection_structs[a]) {
    if (STREQ(collection_structs[a], struct_name)) {
      found = 1;
      break;
    }
    a++;
  }

  return found;
}

static void rna_generate_header_class_cpp(StructDefRNA *ds, FILE *f)
{
  StructRNA *srna = ds->srna;
  PropertyDefRNA *dp;
  FunctionDefRNA *dfunc;

  fprintf(f, "/**************** %s ****************/\n\n", srna->name);

  fprintf(f,
          "class %s : public %s {\n",
          srna->identifier,
          (srna->base) ? srna->base->identifier : "Pointer");
  fprintf(f, "public:\n");
  fprintf(f,
          "\t%s(const PointerRNA &ptr_arg) :\n\t\t%s(ptr_arg)",
          srna->identifier,
          (srna->base) ? srna->base->identifier : "Pointer");
  for (dp = ds->cont.properties.first; dp; dp = dp->next) {
    if (rna_is_collection_prop(dp->prop)) {
      fprintf(f, ",\n\t\t%s(ptr_arg)", dp->prop->identifier);
    }
  }
  fprintf(f, "\n\t\t{}\n\n");

  for (dp = ds->cont.properties.first; dp; dp = dp->next) {
    rna_def_property_funcs_header_cpp(f, ds->srna, dp);
  }

  fprintf(f, "\n");
  for (dfunc = ds->functions.first; dfunc; dfunc = dfunc->cont.next) {
    rna_def_struct_function_header_cpp(f, srna, dfunc);
  }

  fprintf(f, "};\n\n");
}

static void rna_generate_header_cpp(BlenderRNA *UNUSED(brna), FILE *f)
{
  StructDefRNA *ds;
  PropertyDefRNA *dp;
  StructRNA *srna;
  FunctionDefRNA *dfunc;
  const char *first_collection_func_struct = NULL;
  const char *collection_func_structs[256] = {NULL};
  int all_collection_func_structs = 0;
  int max_collection_func_structs = sizeof(collection_func_structs) /
                                        sizeof(collection_func_structs[0]) -
                                    1;

  fprintf(f, "\n#ifndef __RNA_BLENDER_CPP_H__\n");
  fprintf(f, "#define __RNA_BLENDER_CPP_H__\n\n");

  fprintf(f,
          "/* Automatically generated classes for the Data API.\n"
          " * Do not edit manually, changes will be overwritten. */\n\n");

  fprintf(f, "#include \"RNA_blender.h\"\n");
  fprintf(f, "#include \"RNA_types.h\"\n");
  fprintf(f, "#include \"RNA_access.h\"\n");

  fprintf(f, "%s", cpp_classes);

  fprintf(f, "/**************** Declarations ****************/\n\n");

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    fprintf(f, "class %s;\n", ds->srna->identifier);
  }
  fprintf(f, "\n");

  /* first get list of all structures used as collection functions, so they'll be declared first */
  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    for (dp = ds->cont.properties.first; dp; dp = dp->next) {
      if (rna_is_collection_prop(dp->prop)) {
        PropertyRNA *prop = dp->prop;

        if (prop->srna) {
          /* store name of structure which first uses custom functions for collections */
          if (first_collection_func_struct == NULL) {
            first_collection_func_struct = ds->srna->identifier;
          }

          if (!rna_is_collection_functions_struct(collection_func_structs, (char *)prop->srna)) {
            if (all_collection_func_structs >= max_collection_func_structs) {
              printf("Array size to store all collection structures names is too small\n");
              exit(1);
            }

            collection_func_structs[all_collection_func_structs++] = (char *)prop->srna;
          }
        }
      }
    }
  }

  /* declare all structures in such order:
   * - first N structures which doesn't use custom functions for collections
   * - all structures used for custom functions in collections
   * - all the rest structures
   * such an order prevents usage of non-declared classes
   */
  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    srna = ds->srna;

    if (STREQ(srna->identifier, first_collection_func_struct)) {
      StructDefRNA *ds2;
      StructRNA *srna2;

      for (ds2 = DefRNA.structs.first; ds2; ds2 = ds2->cont.next) {
        srna2 = ds2->srna;

        if (rna_is_collection_functions_struct(collection_func_structs, srna2->identifier)) {
          rna_generate_header_class_cpp(ds2, f);
        }
      }
    }

    if (!rna_is_collection_functions_struct(collection_func_structs, srna->identifier)) {
      rna_generate_header_class_cpp(ds, f);
    }
  }

  fprintf(f, "} /* namespace BL */\n");

  fprintf(f, "\n");
  fprintf(f, "/**************** Implementation ****************/\n");
  fprintf(f, "\n");

  fprintf(f, "/* Structure prototypes */\n\n");
  fprintf(f, "extern \"C\" {\n");
  rna_generate_struct_prototypes(f);
  fprintf(f, "}\n\n");

  fprintf(f, "namespace BL {\n");

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    srna = ds->srna;

    for (dp = ds->cont.properties.first; dp; dp = dp->next) {
      rna_def_property_funcs_impl_cpp(f, ds->srna, dp);
    }

    fprintf(f, "\n");

    for (dfunc = ds->functions.first; dfunc; dfunc = dfunc->cont.next) {
      rna_def_struct_function_impl_cpp(f, srna, dfunc);
    }

    fprintf(f, "\n");
  }

  fprintf(f, "}\n\n#endif /* __RNA_BLENDER_CPP_H__ */\n\n");
}

static void make_bad_file(const char *file, int line)
{
  FILE *fp = fopen(file, "w");
  fprintf(fp,
          "#error \"Error! can't make correct RNA file from %s:%d, "
          "check DNA properties.\"\n",
          __FILE__,
          line);
  fclose(fp);
}

static int rna_preprocess(const char *outfile)
{
  BlenderRNA *brna;
  StructDefRNA *ds;
  FILE *file;
  char deffile[4096];
  int i, status;
  const char *deps[3]; /* expand as needed */

  /* define rna */
  brna = RNA_create();

  for (i = 0; PROCESS_ITEMS[i].filename; i++) {
    if (PROCESS_ITEMS[i].define) {
      PROCESS_ITEMS[i].define(brna);

      /* sanity check */
      if (!DefRNA.animate) {
        fprintf(stderr, "Error: DefRNA.animate left disabled in %s\n", PROCESS_ITEMS[i].filename);
      }

      for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
        if (!ds->filename) {
          ds->filename = PROCESS_ITEMS[i].filename;
        }
      }
    }
  }

  rna_auto_types();

  status = (DefRNA.error != 0);

  /* create rna prototype header file */
  strcpy(deffile, outfile);
  strcat(deffile, "rna_prototypes_gen.h");
  if (status) {
    make_bad_file(deffile, __LINE__);
  }
  file = fopen(deffile, "w");
  if (!file) {
    fprintf(stderr, "Unable to open file: %s\n", deffile);
    status = 1;
  }
  else {
    fprintf(file,
            "/* Automatically generated function declarations for the Data API.\n"
            " * Do not edit manually, changes will be overwritten.              */\n\n");
    rna_generate_prototypes(brna, file);
    fclose(file);
    status = (DefRNA.error != 0);
  }

  /* create rna_gen_*.c files */
  for (i = 0; PROCESS_ITEMS[i].filename; i++) {
    strcpy(deffile, outfile);
    strcat(deffile, PROCESS_ITEMS[i].filename);
    deffile[strlen(deffile) - 2] = '\0';
    strcat(deffile, "_gen.c" TMP_EXT);

    if (status) {
      make_bad_file(deffile, __LINE__);
    }
    else {
      file = fopen(deffile, "w");

      if (!file) {
        fprintf(stderr, "Unable to open file: %s\n", deffile);
        status = 1;
      }
      else {
        rna_generate(brna, file, PROCESS_ITEMS[i].filename, PROCESS_ITEMS[i].api_filename);
        fclose(file);
        status = (DefRNA.error != 0);
      }
    }

    /* avoid unneeded rebuilds */
    deps[0] = PROCESS_ITEMS[i].filename;
    deps[1] = PROCESS_ITEMS[i].api_filename;
    deps[2] = NULL;

    replace_if_different(deffile, deps);
  }

  /* create RNA_blender_cpp.h */
  strcpy(deffile, outfile);
  strcat(deffile, "RNA_blender_cpp.h" TMP_EXT);

  if (status) {
    make_bad_file(deffile, __LINE__);
  }
  else {
    file = fopen(deffile, "w");

    if (!file) {
      fprintf(stderr, "Unable to open file: %s\n", deffile);
      status = 1;
    }
    else {
      rna_generate_header_cpp(brna, file);
      fclose(file);
      status = (DefRNA.error != 0);
    }
  }

  replace_if_different(deffile, NULL);

  rna_sort(brna);

  /* create RNA_blender.h */
  strcpy(deffile, outfile);
  strcat(deffile, "RNA_blender.h" TMP_EXT);

  if (status) {
    make_bad_file(deffile, __LINE__);
  }
  else {
    file = fopen(deffile, "w");

    if (!file) {
      fprintf(stderr, "Unable to open file: %s\n", deffile);
      status = 1;
    }
    else {
      rna_generate_header(brna, file);
      fclose(file);
      status = (DefRNA.error != 0);
    }
  }

  replace_if_different(deffile, NULL);

  /* free RNA */
  RNA_define_free(brna);
  RNA_free(brna);

  return status;
}

static void mem_error_cb(const char *errorStr)
{
  fprintf(stderr, "%s", errorStr);
  fflush(stderr);
}

int main(int argc, char **argv)
{
  int totblock, return_status = 0;

  CLG_init();

  /* Some useful defaults since this runs standalone. */
  CLG_output_use_basename_set(true);
  CLG_level_set(debugSRNA);

  if (argc < 2) {
    fprintf(stderr, "Usage: %s outdirectory/\n", argv[0]);
    return_status = 1;
  }
  else {
    if (debugSRNA > 0) {
      fprintf(stderr, "Running makesrna\n");
    }
    makesrna_path = argv[0];
    return_status = rna_preprocess(argv[1]);
  }

  CLG_exit();

  totblock = MEM_get_memory_blocks_in_use();
  if (totblock != 0) {
    fprintf(stderr, "Error Totblock: %d\n", totblock);
    MEM_set_error_callback(mem_error_cb);
    MEM_printmemlist();
  }

  return return_status;
}
