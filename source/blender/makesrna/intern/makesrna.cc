/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <algorithm>
#include <cerrno>
#include <cfloat>
#include <cinttypes>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_system.h" /* For #BLI_system_backtrace stub. */
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "makesrna_utils.hh"
#include "rna_internal.hh"

#include "CLG_log.h"

namespace blender {

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
#endif /* !NDEBUG */

/* Replace if different */
#define TMP_EXT ".tmp"

/* copied from BLI_file_older */
#include <sys/stat.h>
static bool file_older(const char *file1, const char *file2)
{
  struct stat st1, st2;
  if (debugSRNA > 0) {
    printf("compare: %s %s\n", file1, file2);
  }

  if (stat(file1, &st1)) {
    return false;
  }
  if (stat(file2, &st2)) {
    return false;
  }

  return (st1.st_mtime < st2.st_mtime);
}
static const char *makesrna_path = nullptr;

static const char *path_basename(const char *path)
{
  const char *lfslash, *lbslash;

  lfslash = strrchr(path, '/');
  lbslash = strrchr(path, '\\');
  if (lbslash) {
    lbslash++;
  }
  if (lfslash) {
    lfslash++;
  }

  return std::max({path, lfslash, lbslash});
}

/* forward declarations */
static void rna_generate_static_parameter_prototypes(FILE *f,
                                                     StructRNA *srna,
                                                     FunctionDefRNA *dfunc,
                                                     const char *name_override,
                                                     int close_prototype);

/* helpers */
#define WRITE_COMMA \
  { \
    if (!first) { \
      fprintf(f, ", "); \
    } \
    first = 0; \
  } \
  (void)0

#define WRITE_PARAM(param) \
  { \
    WRITE_COMMA; \
    fprintf(f, param); \
  } \
  (void)0

/**
 * \return 1 when the file was renamed, 0 when no action was taken, -1 on error.
 */
static int replace_if_different(const char *tmpfile, const char *dep_files[])
{

#ifdef USE_MAKEFILE_WORKAROUND
  const bool use_makefile_workaround = true;
#else
  const bool use_makefile_workaround = false;
#endif

  /* Use for testing hand edited `rna_*_gen.c` files. */
  // return 0;

#define REN_IF_DIFF \
  { \
    FILE *file_test = fopen(orgfile, "rb"); \
    if (file_test) { \
      fclose(file_test); \
      if (fp_org) { \
        fclose(fp_org); \
      } \
      if (fp_new) { \
        fclose(fp_new); \
      } \
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
  return 1; \
  ((void)0)
  /* End `REN_IF_DIFF`. */

  FILE *fp_new = nullptr, *fp_org = nullptr;
  int len_new, len_org;
  char *arr_new, *arr_org;
  int cmp;

  const char *makesrna_source_filepath = __FILE__;
  const char *makesrna_source_filename = path_basename(makesrna_source_filepath);

  char orgfile[4096];

  STRNCPY(orgfile, tmpfile);
  orgfile[strlen(orgfile) - strlen(TMP_EXT)] = '\0'; /* Strip `.tmp`. */

  fp_org = fopen(orgfile, "rb");

  if (fp_org == nullptr) {
    REN_IF_DIFF;
  }

  /* NOTE(@ideasman42): trick to work around dependency problem.
   * The issue is as follows: When `makesrna.cc` or any of the `rna_*.c` files being newer than
   * their generated output, the build-system detects that the `rna_*_gen.c` file is out-dated and
   * requests the `rna_*_gen.c` files are re-generated (even if this function always returns 0).
   * It happens *every* rebuild, slowing incremental builds which isn't practical for development.
   *
   * This is only an issue for `Unix Makefiles`, `Ninja` generator doesn't have this problem.
   *
   * CMake will set `use_makefile_workaround` to 0 or 1 depending on the generator used. */

  if (use_makefile_workaround) {
    /* First check if `makesrna.cc` is newer than generated files.
     * For development on `makesrna.cc` you may want to disable this. */
    if (file_older(orgfile, makesrna_source_filepath)) {
      REN_IF_DIFF;
    }

    if (file_older(orgfile, makesrna_path)) {
      REN_IF_DIFF;
    }

    /* Now check if any files we depend on are newer than any generated files. */
    if (dep_files) {
      int pass;
      for (pass = 0; dep_files[pass]; pass++) {
        char from_path[4096];
        /* Only the directory (base-name). */
        SNPRINTF(from_path,
                 "%.*s%s",
                 int(makesrna_source_filename - makesrna_source_filepath),
                 makesrna_source_filepath,
                 dep_files[pass]);
        /* Account for build dependencies, if `makesrna.cc` (this file) is newer. */
        if (file_older(orgfile, from_path)) {
          REN_IF_DIFF;
        }
      }
    }
  }
  /* XXX end dep trick */

  fp_new = fopen(tmpfile, "rb");

  if (fp_new == nullptr) {
    /* Shouldn't happen, just to be safe. */
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
    fp_new = nullptr;
    fclose(fp_org);
    fp_org = nullptr;
    REN_IF_DIFF;
  }

  /* Now compare the files: */
  arr_new = MEM_new_array_uninitialized<char>(size_t(len_new), "rna_cmp_file_new");
  arr_org = MEM_new_array_uninitialized<char>(size_t(len_org), "rna_cmp_file_org");

  if (fread(arr_new, sizeof(char), len_new, fp_new) != len_new) {
    CLOG_ERROR(&LOG, "unable to read file %s for comparison.", tmpfile);
  }
  if (fread(arr_org, sizeof(char), len_org, fp_org) != len_org) {
    CLOG_ERROR(&LOG, "unable to read file %s for comparison.", orgfile);
  }

  fclose(fp_new);
  fp_new = nullptr;
  fclose(fp_org);
  fp_org = nullptr;

  cmp = memcmp(arr_new, arr_org, len_new);

  MEM_delete(arr_new);
  MEM_delete(arr_org);

  if (cmp) {
    REN_IF_DIFF;
  }
  remove(tmpfile);
  return 0;

#undef REN_IF_DIFF
}

/* Helper to solve keyword problems with C/C++. */

static const char *rna_safe_id(const char *id)
{
  if (STREQ(id, "default")) {
    return "default_value";
  }
  if (STREQ(id, "operator")) {
    return "operator_value";
  }
  if (STREQ(id, "new")) {
    return "create";
  }
  if (STREQ(id, "co_return")) {
    /* MSVC2015, C++ uses for coroutines */
    return "coord_return";
  }

  return id;
}

/* Preprocessing */

static void rna_print_c_string(FILE *f, const char *str)
{
  static const char *escape[] = {
      "\''", "\"\"", "\??", "\\\\", "\aa", "\bb", "\ff", "\nn", "\rr", "\tt", "\vv", nullptr};
  int i, j;

  if (!str) {
    fprintf(f, "nullptr");
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

static void rna_print_id_get(FILE *f, PropertyDefRNA * /*dp*/)
{
  fprintf(f, "    ID *id = ptr->owner_id;\n");
}

static void rna_construct_function_name(
    char *buffer, int size, const char *structname, const char *propname, const char *type)
{
  BLI_snprintf(buffer, size, "%s_%s_%s", structname, propname, type);
}

static void rna_construct_wrapper_function_name(
    char *buffer, int size, const char *structname, const char *propname, const char *type)
{
  if (type == nullptr || type[0] == '\0') {
    BLI_snprintf(buffer, size, "%s_%s", structname, propname);
  }
  else {
    BLI_snprintf(buffer, size, "%s_%s_%s", structname, propname, type);
  }
}

void *rna_alloc_from_buffer(const char *buffer, int buffer_size)
{
  AllocDefRNA *alloc = MEM_new_zeroed<AllocDefRNA>("AllocDefRNA");
  alloc->mem = MEM_new_uninitialized(buffer_size, __func__);
  memcpy(alloc->mem, buffer, buffer_size);
  rna_addtail(&DefRNA.allocs, alloc);
  return alloc->mem;
}

void *rna_calloc(int buffer_size)
{
  AllocDefRNA *alloc = MEM_new_zeroed<AllocDefRNA>("AllocDefRNA");
  alloc->mem = MEM_new_zeroed(buffer_size, __func__);
  rna_addtail(&DefRNA.allocs, alloc);
  return alloc->mem;
}

static char *rna_alloc_function_name(const char *structname,
                                     const char *propname,
                                     const char *type)
{
  char buffer[2048];
  rna_construct_function_name(buffer, sizeof(buffer), structname, propname, type);
  return static_cast<char *>(rna_alloc_from_buffer(buffer, strlen(buffer) + 1));
}

static StructRNA *rna_find_struct(const char *identifier)
{
  StructDefRNA *ds;

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (STREQ(ds->srna->identifier, identifier)) {
      return ds->srna;
    }
  }

  return nullptr;
}

static const char *rna_find_type(const char *type)
{
  StructDefRNA *ds;

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (ds->dnaname && STREQ(ds->dnaname, type)) {
      return ds->srna->identifier;
    }
  }

  return nullptr;
}

static const char *rna_find_dna_type(const char *type)
{
  StructDefRNA *ds;

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (STREQ(ds->srna->identifier, type)) {
      return ds->dnaname;
    }
  }

  return nullptr;
}

static const char *rna_type_type_name(PropertyRNA *prop)
{
  switch (prop->type) {
    case PROP_BOOLEAN:
      return "bool";
    case PROP_INT:
      return "int";
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop);
      if (eprop->native_enum_type) {
        return eprop->native_enum_type;
      }
      return "int";
    }
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
      return nullptr;
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

static const char *rna_parameter_type_name(PropertyRNA *parm)
{
  const char *type;

  type = rna_type_type_name(parm);

  if (type) {
    return type;
  }

  switch (parm->type) {
    case PROP_POINTER: {
      PointerPropertyRNA *pparm = reinterpret_cast<PointerPropertyRNA *>(parm);

      if (parm->flag_parameter & PARM_RNAPTR) {
        return "PointerRNA";
      }
      return rna_find_dna_type(reinterpret_cast<const char *>(pparm->type));
    }
    case PROP_COLLECTION: {
      return "CollectionVector";
    }
    default:
      return "<error, no type specified>";
  }
}

static int rna_enum_bitmask(PropertyRNA *prop)
{
  EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop);
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

static bool rna_parameter_is_const(const PropertyDefRNA *dparm)
{
  return (dparm->prop->arraydimension) && ((dparm->prop->flag_parameter & PARM_OUTPUT) == 0);
}

static bool rna_color_quantize(PropertyRNA *prop, PropertyDefRNA *dp)
{
  return ((prop->type == PROP_FLOAT) && ELEM(prop->subtype, PROP_COLOR, PROP_COLOR_GAMMA) &&
          (IS_DNATYPE_FLOAT_COMPAT(dp->dnatype) == 0));
}

/**
 * Return the identifier for an enum which is defined in `RNA_enum_items.hh`.
 *
 * Prevents expanding duplicate enums bloating the binary size.
 */
static const char *rna_enum_id_from_pointer(const EnumPropertyItem *item)
{
#define RNA_MAKESRNA
#define DEF_ENUM(id) \
  if (item == id) { \
    return STRINGIFY(id); \
  }
#include "RNA_enum_items.hh"
#undef RNA_MAKESRNA
  return nullptr;
}

template<typename T> static const char *rna_function_string(T *func)
{
  return (func) ? reinterpret_cast<const char *>(func) : "nullptr";
}

static void rna_float_print(FILE *f, float num)
{
  if (num == -FLT_MAX) {
    fprintf(f, "-FLT_MAX");
  }
  else if (num == FLT_MAX) {
    fprintf(f, "FLT_MAX");
  }
  else if ((fabsf(num) < float(INT64_MAX)) && (int64_t(num) == num)) {
    fprintf(f, "%.1ff", num);
  }
  else if (num == std::numeric_limits<float>::infinity()) {
    fprintf(f, "std::numeric_limits<float>::infinity()");
  }
  else if (num == -std::numeric_limits<float>::infinity()) {
    fprintf(f, "-std::numeric_limits<float>::infinity()");
  }
  else {
    fprintf(f, "%.10ff", num);
  }
}

static const char *rna_ui_scale_type_string(const PropertyScaleType type)
{
  switch (type) {
    case PROP_SCALE_LINEAR:
      return "PROP_SCALE_LINEAR";
    case PROP_SCALE_LOG:
      return "PROP_SCALE_LOG";
    case PROP_SCALE_CUBIC:
      return "PROP_SCALE_CUBIC";
  }
  BLI_assert_unreachable();
  return "";
}

static void rna_int_print(FILE *f, int64_t num)
{
  if (num == INT_MIN) {
    fprintf(f, "INT_MIN");
  }
  else if (num == INT_MAX) {
    fprintf(f, "INT_MAX");
  }
  else if (num == INT64_MIN) {
    fprintf(f, "INT64_MIN");
  }
  else if (num == INT64_MAX) {
    fprintf(f, "INT64_MAX");
  }
  else if (num < INT_MIN || num > INT_MAX) {
    fprintf(f, "%" PRId64 "LL", num);
  }
  else {
    fprintf(f, "%d", int(num));
  }
}

static char *rna_def_property_get_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
  char *func;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      return nullptr;
    }

    /* Type check. */
    if (dp->dnatype && *dp->dnatype) {

      if (prop->type == PROP_FLOAT) {
        if (IS_DNATYPE_FLOAT_COMPAT(dp->dnatype) == 0) {
          /* Colors are an exception. these get translated. */
          if (prop->subtype != PROP_COLOR_GAMMA) {
            CLOG_ERROR(&LOG,
                       "%s.%s is a '%s' but wrapped as type '%s'.",
                       srna->identifier,
                       prop->identifier,
                       dp->dnatype,
                       RNA_property_typename(prop->type));
            DefRNA.error = true;
            return nullptr;
          }
        }
      }
      else if (prop->type == PROP_BOOLEAN) {
        if (IS_DNATYPE_BOOLEAN_COMPAT(dp->dnatype) == 0) {
          CLOG_ERROR(&LOG,
                     "%s.%s is a '%s' but wrapped as type '%s'.",
                     srna->identifier,
                     prop->identifier,
                     dp->dnatype,
                     RNA_property_typename(prop->type));
          DefRNA.error = true;
          return nullptr;
        }
      }
      else if (ELEM(prop->type, PROP_INT, PROP_ENUM)) {
        if (IS_DNATYPE_INT_COMPAT(dp->dnatype) == 0) {
          CLOG_ERROR(&LOG,
                     "%s.%s is a '%s' but wrapped as type '%s'.",
                     srna->identifier,
                     prop->identifier,
                     dp->dnatype,
                     RNA_property_typename(prop->type));
          DefRNA.error = true;
          return nullptr;
        }
      }
    }

    /* Check log scale sliders for negative range. */
    if (prop->type == PROP_FLOAT) {
      FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop);
      /* NOTE: ButtonType::NumSlider can't have a softmin of zero. */
      if ((fprop->ui_scale_type == PROP_SCALE_LOG) && (fprop->hardmin < 0 || fprop->softmin < 0)) {
        CLOG_ERROR(
            &LOG, "\"%s.%s\", range for log scale < 0.", srna->identifier, prop->identifier);
        DefRNA.error = true;
        return nullptr;
      }
    }
    if (prop->type == PROP_INT) {
      IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop);
      /* Only ButtonType::NumSlider is implemented and that one can't have a softmin of zero. */
      if ((iprop->ui_scale_type == PROP_SCALE_LOG) && (iprop->hardmin <= 0 || iprop->softmin <= 0))
      {
        CLOG_ERROR(
            &LOG, "\"%s.%s\", range for log scale <= 0.", srna->identifier, prop->identifier);
        DefRNA.error = true;
        return nullptr;
      }
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

  switch (prop->type) {
    case PROP_STRING: {
      StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop);
      UNUSED_VARS_NDEBUG(sprop);
      fprintf(f, "void %s(PointerRNA *ptr, char *value)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    PropStringGetFunc fn = %s;\n", manualfunc);
        fprintf(f, "    fn(ptr, value);\n");
      }
      else {
        rna_print_data_get(f, dp);

        if (dp->dnapointerlevel == 1) {
          /* Handle allocated char pointer properties. */
          fprintf(f, "    if (data->%s == nullptr) {\n", dp->dnaname);
          fprintf(f, "        *value = '\\0';\n");
          fprintf(f, "        return;\n");
          fprintf(f, "    }\n");
          fprintf(f, "    strcpy(value, data->%s);\n", dp->dnaname);
        }
        else {
          /* Handle char array properties. */

#ifndef NDEBUG /* Assert lengths never exceed their maximum expected value. */
          if (sprop->maxlength) {
            fprintf(f, "    BLI_assert(strlen(data->%s) < %d);\n", dp->dnaname, sprop->maxlength);
          }
          else {
            fprintf(f,
                    "    BLI_assert(strlen(data->%s) < sizeof(data->%s));\n",
                    dp->dnaname,
                    dp->dnaname);
          }
#endif

          fprintf(f, "    strcpy(value, data->%s);\n", dp->dnaname);
        }
      }
      fprintf(f, "}\n\n");
      break;
    }
    case PROP_POINTER: {
      fprintf(f, "PointerRNA %s(PointerRNA *ptr)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    PropPointerGetFunc fn = %s;\n", manualfunc);
        fprintf(f, "    return fn(ptr);\n");
      }
      else {
        PointerPropertyRNA *pprop = reinterpret_cast<PointerPropertyRNA *>(prop);
        rna_print_data_get(f, dp);
        if (dp->dnapointerlevel == 0) {
          fprintf(f,
                  "    return RNA_pointer_create_with_parent(*ptr, RNA_%s, &data->%s);\n",
                  reinterpret_cast<const char *>(pprop->type),
                  dp->dnaname);
        }
        else {
          fprintf(f,
                  "    return RNA_pointer_create_with_parent(*ptr, RNA_%s, data->%s);\n",
                  reinterpret_cast<const char *>(pprop->type),
                  dp->dnaname);
        }
      }
      fprintf(f, "}\n\n");
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = reinterpret_cast<CollectionPropertyRNA *>(prop);

      fprintf(f, "static PointerRNA %s(CollectionPropertyIterator *iter)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        if (STR_ELEM(manualfunc,
                     "rna_iterator_listbase_get",
                     "rna_iterator_array_get",
                     "rna_iterator_array_dereference_get"))
        {
          fprintf(f,
                  "    return RNA_pointer_create_with_parent(iter->parent, RNA_%s, %s(iter));\n",
                  (cprop->item_type) ? reinterpret_cast<const char *>(cprop->item_type) :
                                       "UnknownType",
                  manualfunc);
        }
        else {
          fprintf(f, "    PropCollectionGetFunc fn = %s;\n", manualfunc);
          fprintf(f, "    return fn(iter);\n");
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
          /* Assign `fn` to ensure function signatures match. */
          if (prop->type == PROP_BOOLEAN) {
            fprintf(f, "    PropBooleanArrayGetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, values);\n");
          }
          else if (prop->type == PROP_INT) {
            fprintf(f, "    PropIntArrayGetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, values);\n");
          }
          else if (prop->type == PROP_FLOAT) {
            fprintf(f, "    PropFloatArrayGetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, values);\n");
          }
          else {
            BLI_assert_unreachable(); /* Valid but should be handled by type checks. */
            fprintf(f, "    %s(ptr, values);\n", manualfunc);
          }
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
            MEM_delete(lenfunc);
          }
          else {
            fprintf(f, "    unsigned int i;\n\n");
            fprintf(f, "    for (i = 0; i < %u; i++) {\n", prop->totarraylength);
          }

          if (dp->dnaarraylength == 1) {
            if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
              fprintf(f,
                      "        values[i] = %s((data->%s & (",
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, " << i)) != 0);\n");
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
          /* Assign `fn` to ensure function signatures match. */
          if (prop->type == PROP_BOOLEAN) {
            fprintf(f, "    PropBooleanGetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    return fn(ptr);\n");
          }
          else if (prop->type == PROP_INT) {
            fprintf(f, "    PropIntGetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    return fn(ptr);\n");
          }
          else if (prop->type == PROP_FLOAT) {
            fprintf(f, "    PropFloatGetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    return fn(ptr);\n");
          }
          else if (prop->type == PROP_ENUM) {
            fprintf(f, "    PropEnumGetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    return fn(ptr);\n");
          }
          else {
            BLI_assert_unreachable(); /* Valid but should be handled by type checks. */
            fprintf(f, "    return %s(ptr);\n", manualfunc);
          }
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
    FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop);
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
    IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop);
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
    fprintf(f, "    {\n");
    fprintf(f, "#ifdef __cplusplus\n");
    fprintf(f, "        using T = decltype(%s%s);\n", dnaname_prefix, dnaname);
    fprintf(f,
            "        static_assert(std::numeric_limits<std::decay_t<T>>::max() >= %d);\n",
            iprop->hardmax);
    fprintf(f,
            "        static_assert(std::numeric_limits<std::decay_t<T>>::min() <= %d);\n",
            iprop->hardmin);
    fprintf(f, "#else\n");
    fprintf(f,
            "        BLI_STATIC_ASSERT("
            "(TYPEOF_MAX(%s%s) >= %d) && "
            "(TYPEOF_MIN(%s%s) <= %d), "
            "\"invalid limits\");\n",
            dnaname_prefix,
            dnaname,
            iprop->hardmax,
            dnaname_prefix,
            dnaname,
            iprop->hardmin);
    fprintf(f, "#endif\n");
    fprintf(f, "    }\n");
  }
}
#endif /* USE_RNA_RANGE_CHECK */

static void rna_clamp_value(FILE *f, PropertyRNA *prop, int array)
{
  if (prop->type == PROP_INT) {
    IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop);

    if (iprop->hardmin != INT_MIN || iprop->hardmax != INT_MAX || iprop->range) {
      if (array) {
        fprintf(f, "std::clamp(values[i], ");
      }
      else {
        fprintf(f, "std::clamp(value, ");
      }
      if (iprop->range) {
        fprintf(f, "prop_clamp_min, prop_clamp_max);\n");
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
    FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop);

    if (fprop->hardmin != -FLT_MAX || fprop->hardmax != FLT_MAX || fprop->range) {
      if (array) {
        fprintf(f, "std::clamp(values[i], ");
      }
      else {
        fprintf(f, "std::clamp(value, ");
      }
      if (fprop->range) {
        fprintf(f, "prop_clamp_min, prop_clamp_max);\n");
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

static char *rna_def_property_search_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA * /*dp*/, const char *manualfunc)
{
  char *func;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }
  if (!manualfunc) {
    return nullptr;
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "search");

  fprintf(f,
          "void %s("
          "const bContext *C, "
          "PointerRNA *ptr, "
          "PropertyRNA *prop, "
          "const char *edit_text, "
          "FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)\n",
          func);
  fprintf(f, "{\n");
  fprintf(f, "\n    StringPropertySearchFunc fn = %s;\n", manualfunc);
  fprintf(f, "\n    fn(C, ptr, prop, edit_text, visit_fn);\n");
  fprintf(f, "}\n\n");
  return func;
}

static char *rna_def_property_set_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA *dp, const char *manualfunc)
{
  char *func;

  if (!(prop->flag & PROP_EDITABLE)) {
    return nullptr;
  }
  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      if (prop->flag & PROP_EDITABLE) {
        CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
        DefRNA.error = true;
      }
      return nullptr;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "set");

  switch (prop->type) {
    case PROP_STRING: {
      StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop);
      fprintf(f, "void %s(PointerRNA *ptr, const char *value)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    PropStringSetFunc fn = %s;\n", manualfunc);
        fprintf(f, "    fn(ptr, value);\n");
      }
      else {
        const PropertySubType subtype = prop->subtype;
        rna_print_data_get(f, dp);

        if (dp->dnapointerlevel == 1) {
          /* Handle allocated char pointer properties. */
          fprintf(f,
                  "    if (data->%s != nullptr) { MEM_delete(data->%s); }\n",
                  dp->dnaname,
                  dp->dnaname);
          fprintf(f, "    const size_t length = strlen(value);\n");
          fprintf(f, "    if (length > 0) {\n");
          fprintf(f,
                  "        data->%s = MEM_new_array_uninitialized<char>(length + 1, __func__);\n",
                  dp->dnaname);
          fprintf(f, "        memcpy(data->%s, value, length + 1);\n", dp->dnaname);
          fprintf(f, "    } else { data->%s = nullptr; }\n", dp->dnaname);
        }
        else {
          const char *string_copy_func =
              ELEM(subtype, PROP_FILEPATH, PROP_DIRPATH, PROP_FILENAME, PROP_BYTESTRING) ?
                  "BLI_strncpy" :
                  "BLI_strncpy_utf8";
          /* Handle char array properties. */
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
      }
      fprintf(f, "}\n\n");
      break;
    }
    case PROP_POINTER: {
      fprintf(f, "void %s(PointerRNA *ptr, PointerRNA value, ReportList *reports)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    PropPointerSetFunc fn = %s;\n", manualfunc);
        fprintf(f, "    fn(ptr, value, reports);\n");
      }
      else {
        rna_print_data_get(f, dp);

        PointerPropertyRNA *pprop = reinterpret_cast<PointerPropertyRNA *>(dp->prop);
        StructRNA *type = (pprop->type) ?
                              rna_find_struct(reinterpret_cast<const char *>(pprop->type)) :
                              nullptr;

        if (prop->flag & PROP_ID_SELF_CHECK) {
          /* No pointers to self allowed. */
          rna_print_id_get(f, dp);
          fprintf(f, "    if (id == value.data) {\n");
          fprintf(f, "      return;\n");
          fprintf(f, "    }\n");
        }

        if (type && (type->flag & STRUCT_ID)) {
          /* Check if pointers between datablocks are allowed. */
          fprintf(f,
                  "    if (value.data && ptr->owner_id && value.owner_id && "
                  "!BKE_id_can_use_id(*ptr->owner_id, *value.owner_id)) {\n");
          fprintf(f, "      return;\n");
          fprintf(f, "    }\n");
        }

        if (prop->flag & PROP_ID_REFCOUNT) {
          /* Perform reference counting. */
          fprintf(f, "\n    if (data->%s) {\n", dp->dnaname);
          fprintf(f, "        id_us_min((ID *)data->%s);\n", dp->dnaname);
          fprintf(f, "    }\n");
          fprintf(f, "    if (value.data) {\n");
          fprintf(f, "        id_us_plus((ID *)value.data);\n");
          fprintf(f, "    }\n");
        }
        else if (type && (type->flag & STRUCT_ID)) {
          /* Still mark linked data as used if not reference counting. */
          fprintf(f, "    if (value.data) {\n");
          fprintf(f, "        id_lib_extern((ID *)value.data);\n");
          fprintf(f, "    }\n");
        }

        fprintf(f, "    *(void **)&data->%s = value.data;\n", dp->dnaname);
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
          /* Assign `fn` to ensure function signatures match. */
          if (prop->type == PROP_BOOLEAN) {
            fprintf(f, "    PropBooleanArraySetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, values);\n");
          }
          else if (prop->type == PROP_INT) {
            fprintf(f, "    PropIntArraySetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, values);\n");
          }
          else if (prop->type == PROP_FLOAT) {
            fprintf(f, "    PropFloatArraySetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, values);\n");
          }
          else {
            BLI_assert_unreachable(); /* Valid but should be handled by type checks. */
            fprintf(f, "    %s(ptr, values);\n", manualfunc);
          }
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
            MEM_delete(lenfunc);
          }
          else {
            fprintf(f, "    unsigned int i;\n\n");
            rna_clamp_value_range(f, prop);
            fprintf(f, "    for (i = 0; i < %u; i++) {\n", prop->totarraylength);
          }

          if (dp->dnaarraylength == 1) {
            if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
              fprintf(f,
                      "        if (%svalues[i]) { data->%s |= (",
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, " << i); }\n");
              fprintf(f, "        else { data->%s &= ~(", dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, " << i); }\n");
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
                      "        if (%svalues[i]) { data->%s[i] |= ",
                      (dp->booleannegative) ? "!" : "",
                      dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, "; }\n");
              fprintf(f, "        else { data->%s[i] &= ~", dp->dnaname);
              rna_int_print(f, dp->booleanbit);
              fprintf(f, "; }\n");
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
        if (dp->dnaname && manualfunc == nullptr) {
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
          /* Assign `fn` to ensure function signatures match. */
          if (prop->type == PROP_BOOLEAN) {
            fprintf(f, "    PropBooleanSetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, value);\n");
          }
          else if (prop->type == PROP_INT) {
            fprintf(f, "    PropIntSetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, value);\n");
          }
          else if (prop->type == PROP_FLOAT) {
            fprintf(f, "    PropFloatSetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, value);\n");
          }
          else if (prop->type == PROP_ENUM) {
            fprintf(f, "    PropEnumSetFunc fn = %s;\n", manualfunc);
            fprintf(f, "    fn(ptr, value);\n");
          }
          else {
            BLI_assert_unreachable(); /* Valid but should be handled by type checks. */
            fprintf(f, "    %s(ptr, value);\n", manualfunc);
          }
        }
        else {
          rna_print_data_get(f, dp);
          if (prop->type == PROP_BOOLEAN && dp->booleanbit) {
            fprintf(f,
                    "    if (%svalue) { data->%s |= ",
                    (dp->booleannegative) ? "!" : "",
                    dp->dnaname);
            rna_int_print(f, dp->booleanbit);
            fprintf(f, "; }\n");
            fprintf(f, "    else { data->%s &= ~", dp->dnaname);
            rna_int_print(f, dp->booleanbit);
            fprintf(f, "; }\n");
          }
          else if (prop->type == PROP_ENUM && dp->enumbitflags) {
            fprintf(f, "    data->%s &= ~", dp->dnaname);
            rna_int_print(f, rna_enum_bitmask(prop));
            fprintf(f, ";\n");
            fprintf(f, "    data->%s |= value;\n", dp->dnaname);
          }
          else {
            rna_clamp_value_range(f, prop);
            /* C++ may require casting to an enum type. */
            fprintf(f, "#ifdef __cplusplus\n");
            fprintf(f,
                    /* If #rna_clamp_value() adds an expression like `std::clamp(...)`
                     * (instead of an `lvalue`), #decltype() yields a reference,
                     * so that has to be removed. */
                    "    data->%s = %s(std::remove_reference_t<decltype(data->%s)>)",
                    dp->dnaname,
                    (dp->booleannegative) ? "!" : "",
                    dp->dnaname);
            rna_clamp_value(f, prop, 0);
            fprintf(f, "#else\n");
            fprintf(f, "    data->%s = %s", dp->dnaname, (dp->booleannegative) ? "!" : "");
            rna_clamp_value(f, prop, 0);
            fprintf(f, "#endif\n");
          }
        }

#ifdef USE_RNA_RANGE_CHECK
        if (dp->dnaname && manualfunc == nullptr) {
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
  char *func = nullptr;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  if (prop->type == PROP_STRING) {
    if (!manualfunc) {
      if (!dp->dnastructname || !dp->dnaname) {
        CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
        DefRNA.error = true;
        return nullptr;
      }
    }

    func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "length");

    fprintf(f, "int %s(PointerRNA *ptr)\n", func);
    fprintf(f, "{\n");
    if (manualfunc) {
      fprintf(f, "    PropStringLengthFunc fn = %s;\n", manualfunc);
      fprintf(f, "    return fn(ptr);\n");
    }
    else {
      rna_print_data_get(f, dp);
      if (dp->dnapointerlevel == 1) {
        /* Handle allocated char pointer properties. */
        fprintf(f,
                "    return (data->%s == nullptr) ? 0 : strlen(data->%s);\n",
                dp->dnaname,
                dp->dnaname);
      }
      else {
        /* Handle char array properties. */
        fprintf(f, "    return strlen(data->%s);\n", dp->dnaname);
      }
    }
    fprintf(f, "}\n\n");
  }
  else if (prop->type == PROP_COLLECTION) {
    if (!manualfunc) {
      if (prop->type == PROP_COLLECTION &&
          (!(dp->dnalengthname || dp->dnalengthfixed) || !dp->dnaname))
      {
        CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
        DefRNA.error = true;
        return nullptr;
      }
    }

    func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "length");

    fprintf(f, "int %s(PointerRNA *ptr)\n", func);
    fprintf(f, "{\n");
    if (manualfunc) {
      fprintf(f, "    PropCollectionLengthFunc fn = %s;\n", manualfunc);
      fprintf(f, "    return fn(ptr);\n");
    }
    else {
      if (dp->dnaarraylength <= 1 || dp->dnalengthname) {
        rna_print_data_get(f, dp);
      }

      if (dp->dnaarraylength > 1) {
        fprintf(f, "    return ");
      }
      else {
        fprintf(f, "    return (data->%s == nullptr) ? 0 : ", dp->dnaname);
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

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      CLOG_ERROR(&LOG, "%s.%s has no valid dna info.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      return nullptr;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "begin");

  fprintf(f, "void %s(CollectionPropertyIterator *iter, PointerRNA *ptr)\n", func);
  fprintf(f, "{\n");

  if (!manualfunc) {
    rna_print_data_get(f, dp);
  }

  fprintf(f, "\n    *iter = {};\n");
  fprintf(f, "    iter->parent = *ptr;\n");
  fprintf(f, "    iter->prop = &rna_%s_%s;\n", srna->identifier, prop->identifier);

  if (dp->dnalengthname || dp->dnalengthfixed) {
    if (manualfunc) {
      fprintf(f, "\n    PropCollectionBeginFunc fn = %s;\n", manualfunc);
      fprintf(f, "    fn(iter, ptr);\n");
    }
    else {
      if (dp->dnalengthname) {
        fprintf(f,
                "\n    rna_iterator_array_begin(iter, ptr, data->%s, sizeof(data->%s[0]), "
                "data->%s, 0, nullptr);\n",
                dp->dnaname,
                dp->dnaname,
                dp->dnalengthname);
      }
      else {
        fprintf(f,
                "\n    rna_iterator_array_begin(iter, ptr, data->%s, sizeof(data->%s[0]), %d, 0, "
                "nullptr);\n",
                dp->dnaname,
                dp->dnaname,
                dp->dnalengthfixed);
      }
    }
  }
  else {
    if (manualfunc) {
      fprintf(f, "\n    PropCollectionBeginFunc fn = %s;\n", manualfunc);
      fprintf(f, "    fn(iter, ptr);\n");
    }
    else if (dp->dnapointerlevel == 0) {
      fprintf(
          f, "\n    rna_iterator_listbase_begin(iter, ptr, &data->%s, nullptr);\n", dp->dnaname);
    }
    else {
      fprintf(
          f, "\n    rna_iterator_listbase_begin(iter, ptr, data->%s, nullptr);\n", dp->dnaname);
    }
  }

  getfunc = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

  fprintf(f, "\n    if (iter->valid) {\n");
  fprintf(f, "        iter->ptr = %s(iter);", getfunc);
  fprintf(f, "\n    }\n");

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

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      return nullptr;
    }

    /* only supported in case of standard next functions */
    if (STREQ(nextfunc, "rna_iterator_array_next")) {
    }
    else if (STREQ(nextfunc, "rna_iterator_listbase_next")) {
    }
    else {
      return nullptr;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "lookup_int");

  fprintf(f, "bool %s(PointerRNA *ptr, int index, PointerRNA *r_ptr)\n", func);
  fprintf(f, "{\n");

  if (manualfunc) {
    fprintf(f, "\n    PropCollectionLookupIntFunc fn = %s;\n", manualfunc);
    fprintf(f, "    return fn(ptr, index, r_ptr);\n");
    fprintf(f, "}\n\n");
    return func;
  }

  fprintf(f, "    bool found = false;\n");
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
    fprintf(f, "            while (index-- > 0 && internal->link) {\n");
    fprintf(f, "                internal->link = internal->link->next;\n");
    fprintf(f, "            }\n");
    fprintf(f, "            found = (index == -1 && internal->link);\n");
    fprintf(f, "        }\n");
  }

  fprintf(f,
          "        if (found) { *r_ptr = %s_%s_get(&iter); }\n",
          srna->identifier,
          rna_safe_id(prop->identifier));
  fprintf(f, "    }\n\n");
  fprintf(f, "    %s_%s_end(&iter);\n\n", srna->identifier, rna_safe_id(prop->identifier));

  fprintf(f, "    return found;\n");

#if 0
  rna_print_data_get(f, dp);
  item_type = (cprop->item_type) ? (const char *)cprop->item_type : "UnknownType";

  if (dp->dnalengthname || dp->dnalengthfixed) {
    if (dp->dnalengthname) {
      fprintf(f,
              "\n    rna_array_lookup_int(ptr, RNA_%s, data->%s, sizeof(data->%s[0]), data->%s, "
              "index);\n",
              item_type,
              dp->dnaname,
              dp->dnaname,
              dp->dnalengthname);
    }
    else {
      fprintf(
          f,
          "\n    rna_array_lookup_int(ptr, RNA_%s, data->%s, sizeof(data->%s[0]), %d, index);\n",
          item_type,
          dp->dnaname,
          dp->dnaname,
          dp->dnalengthfixed);
    }
  }
  else {
    if (dp->dnapointerlevel == 0) {
      fprintf(f,
              "\n    return rna_listbase_lookup_int(ptr, RNA_%s, &data->%s, index);\n",
              item_type,
              dp->dnaname);
    }
    else {
      fprintf(f,
              "\n    return rna_listbase_lookup_int(ptr, RNA_%s, data->%s, index);\n",
              item_type,
              dp->dnaname);
    }
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

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  if (!manualfunc) {
    if (!dp->dnastructname || !dp->dnaname) {
      return nullptr;
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
      return nullptr;
    }
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "lookup_string");

  if (!manualfunc) {
    fprintf(f,
            "int %s_%s_length(PointerRNA *);\n",
            item_name_base->identifier,
            rna_safe_id(item_name_prop->identifier));
    fprintf(f,
            "void %s_%s_get(PointerRNA *, char *);\n\n",
            item_name_base->identifier,
            rna_safe_id(item_name_prop->identifier));
  }

  fprintf(f, "bool %s(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)\n", func);
  fprintf(f, "{\n");

  if (manualfunc) {
    fprintf(f, "    PropCollectionLookupStringFunc fn = %s;\n", manualfunc);
    fprintf(f, "    return fn(ptr, key, r_ptr);\n");
    fprintf(f, "}\n\n");
    return func;
  }

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
  fprintf(f, "                name = MEM_new_array_uninitialized<char>(size_t(namelen) + 1,\n");
  fprintf(f, "                                               \"name string\");\n");
  fprintf(f,
          "                %s_%s_get(&iter.ptr, name);\n",
          item_name_base->identifier,
          rna_safe_id(item_name_prop->identifier));
  fprintf(f, "                if (strcmp(name, key) == 0) {\n");
  fprintf(f, "                    MEM_delete(name);\n\n");
  fprintf(f, "                    found = true;\n");
  fprintf(f, "                    *r_ptr = iter.ptr;\n");
  fprintf(f, "                    break;\n");
  fprintf(f, "                }\n");
  fprintf(f, "                else {\n");
  fprintf(f, "                    MEM_delete(name);\n");
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

static char *rna_def_property_next_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA * /*dp*/, const char *manualfunc)
{
  char *func, *getfunc;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  if (!manualfunc) {
    return nullptr;
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "next");

  fprintf(f, "void %s(CollectionPropertyIterator *iter)\n", func);
  fprintf(f, "{\n");
  fprintf(f, "    PropCollectionNextFunc fn = %s;\n", manualfunc);
  fprintf(f, "    fn(iter);\n");

  getfunc = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "get");

  fprintf(f, "\n    if (iter->valid) {\n");
  fprintf(f, "        iter->ptr = %s(iter);", getfunc);
  fprintf(f, "\n    }\n");

  fprintf(f, "}\n\n");

  return func;
}

static char *rna_def_property_end_func(
    FILE *f, StructRNA *srna, PropertyRNA *prop, PropertyDefRNA * /*dp*/, const char *manualfunc)
{
  char *func;

  if (prop->flag & PROP_IDPROPERTY && manualfunc == nullptr) {
    return nullptr;
  }

  func = rna_alloc_function_name(srna->identifier, rna_safe_id(prop->identifier), "end");

  fprintf(f, "void %s(CollectionPropertyIterator *iter)\n", func);
  fprintf(f, "{\n");
  if (manualfunc) {
    fprintf(f, "    PropCollectionEndFunc fn = %s;\n", manualfunc);
    fprintf(f, "    fn(iter);\n");
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
    prop->rawtype = prop->type == PROP_BOOLEAN ? PROP_RAW_BOOLEAN : PROP_RAW_CHAR;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "int8_t")) {
    prop->rawtype = prop->type == PROP_BOOLEAN ? PROP_RAW_BOOLEAN : PROP_RAW_INT8;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "uchar")) {
    prop->rawtype = prop->type == PROP_BOOLEAN ? PROP_RAW_BOOLEAN : PROP_RAW_UINT8;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "short")) {
    prop->rawtype = PROP_RAW_SHORT;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "ushort")) {
    prop->rawtype = PROP_RAW_UINT16;
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
  else if (STREQ(dp->dnatype, "int64_t")) {
    prop->rawtype = PROP_RAW_INT64;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
  else if (STREQ(dp->dnatype, "uint64_t")) {
    prop->rawtype = PROP_RAW_UINT64;
    prop->flag_internal |= PROP_INTERN_RAW_ACCESS;
  }
}

static void rna_set_raw_offset(FILE *f, StructRNA *srna, PropertyRNA *prop)
{
  PropertyDefRNA *dp = rna_find_struct_property_def(srna, prop);

  fprintf(
      f, "\toffsetof(%s, %s), RawPropertyType(%d)", dp->dnastructname, dp->dnaname, prop->rawtype);
}

static void rna_def_property_funcs(FILE *f, StructRNA *srna, PropertyDefRNA *dp)
{
  PropertyRNA *prop;

  prop = dp->prop;

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(prop);

      if (!(prop->flag & PROP_EDITABLE) &&
          (bprop->set || bprop->set_ex || bprop->set_transform || bprop->setarray ||
           bprop->setarray_ex || bprop->setarray_transform))
      {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!prop->arraydimension &&
          (bprop->getarray || bprop->getarray_ex || bprop->getarray_transform || bprop->setarray ||
           bprop->setarray_ex || bprop->setarray_transform))
      {
        CLOG_ERROR(&LOG,
                   "%s.%s, is not an array but defines an array callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!prop->arraydimension) {
        if (!bprop->get && !bprop->set && !dp->booleanbit) {
          rna_set_raw_property(dp, prop);
        }

        bprop->get = reinterpret_cast<PropBooleanGetFunc>(rna_def_property_get_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(bprop->get)));
        bprop->set = reinterpret_cast<PropBooleanSetFunc>(rna_def_property_set_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(bprop->set)));
      }
      else {
        bprop->getarray = reinterpret_cast<PropBooleanArrayGetFunc>(rna_def_property_get_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(bprop->getarray)));
        bprop->setarray = reinterpret_cast<PropBooleanArraySetFunc>(rna_def_property_set_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(bprop->setarray)));
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop);

      if (!(prop->flag & PROP_EDITABLE) &&
          (iprop->set || iprop->set_ex || iprop->set_transform || iprop->setarray ||
           iprop->setarray_ex || iprop->setarray_transform))
      {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!prop->arraydimension &&
          (iprop->getarray || iprop->getarray_ex || iprop->getarray_transform || iprop->setarray ||
           iprop->setarray_ex || iprop->setarray_transform))
      {
        CLOG_ERROR(&LOG,
                   "%s.%s, is not an array but defines an array callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!prop->arraydimension) {
        if (!iprop->get && !iprop->set) {
          rna_set_raw_property(dp, prop);
        }

        iprop->get = reinterpret_cast<PropIntGetFunc>(rna_def_property_get_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(iprop->get)));
        iprop->set = reinterpret_cast<PropIntSetFunc>(rna_def_property_set_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(iprop->set)));
      }
      else {
        if (!iprop->getarray && !iprop->setarray) {
          rna_set_raw_property(dp, prop);
        }

        iprop->getarray = reinterpret_cast<PropIntArrayGetFunc>(rna_def_property_get_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(iprop->getarray)));
        iprop->setarray = reinterpret_cast<PropIntArraySetFunc>(rna_def_property_set_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(iprop->setarray)));
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop);

      if (!(prop->flag & PROP_EDITABLE) &&
          (fprop->set || fprop->set_ex || fprop->set_transform || fprop->setarray ||
           fprop->setarray_ex || fprop->setarray_transform))
      {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!prop->arraydimension &&
          (fprop->getarray || fprop->getarray_ex || fprop->getarray_transform || fprop->setarray ||
           fprop->setarray_ex || fprop->setarray_transform))
      {
        CLOG_ERROR(&LOG,
                   "%s.%s, is not an array but defines an array callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!prop->arraydimension) {
        if (!fprop->get && !fprop->set) {
          rna_set_raw_property(dp, prop);
        }

        fprop->get = reinterpret_cast<PropFloatGetFunc>(rna_def_property_get_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(fprop->get)));
        fprop->set = reinterpret_cast<PropFloatSetFunc>(rna_def_property_set_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(fprop->set)));
      }
      else {
        if (!fprop->getarray && !fprop->setarray) {
          rna_set_raw_property(dp, prop);
        }

        fprop->getarray = reinterpret_cast<PropFloatArrayGetFunc>(rna_def_property_get_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(fprop->getarray)));
        fprop->setarray = reinterpret_cast<PropFloatArraySetFunc>(rna_def_property_set_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(fprop->setarray)));
      }
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop);

      if (dp->enumbitflags && eprop->item_fn &&
          !(eprop->item != rna_enum_dummy_NULL_items || eprop->set || eprop->set_ex ||
            eprop->set_transform))
      {
        CLOG_ERROR(&LOG,
                   "%s.%s, bitflag enum should not define an `item` callback function, unless "
                   "they also define a static list of items, or a custom `set` callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!(prop->flag & PROP_EDITABLE) && (eprop->set || eprop->set_ex || eprop->set_transform)) {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      if (!eprop->get && !eprop->set) {
        rna_set_raw_property(dp, prop);
      }

      eprop->get = reinterpret_cast<PropEnumGetFunc>(rna_def_property_get_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(eprop->get)));
      eprop->set = reinterpret_cast<PropEnumSetFunc>(rna_def_property_set_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(eprop->set)));
      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop);

      if (!(prop->flag & PROP_EDITABLE) && (sprop->set || sprop->set_ex || sprop->set_transform)) {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      sprop->get = reinterpret_cast<PropStringGetFunc>(rna_def_property_get_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(sprop->get)));
      sprop->length = reinterpret_cast<PropStringLengthFunc>(rna_def_property_length_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(sprop->length)));
      sprop->set = reinterpret_cast<PropStringSetFunc>(rna_def_property_set_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(sprop->set)));
      sprop->search = reinterpret_cast<StringPropertySearchFunc>(rna_def_property_search_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(sprop->search)));
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = reinterpret_cast<PointerPropertyRNA *>(prop);

      if (!(prop->flag & PROP_EDITABLE) && pprop->set) {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      pprop->get = reinterpret_cast<PropPointerGetFunc>(rna_def_property_get_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(pprop->get)));
      pprop->set = reinterpret_cast<PropPointerSetFunc>(rna_def_property_set_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(pprop->set)));
      if (!pprop->type) {
        CLOG_ERROR(
            &LOG, "%s.%s, pointer must have a struct type.", srna->identifier, prop->identifier);
        DefRNA.error = true;
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = reinterpret_cast<CollectionPropertyRNA *>(prop);
      const char *nextfunc = reinterpret_cast<const char *>(cprop->next);
      const char *item_type = reinterpret_cast<const char *>(cprop->item_type);

      if (cprop->length) {
        /* always generate if we have a manual implementation */
        cprop->length = reinterpret_cast<PropCollectionLengthFunc>(rna_def_property_length_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(cprop->length)));
      }
      else if (dp->dnatype && STREQ(dp->dnatype, "ListBase")) {
        /* pass */
      }
      else if (dp->dnalengthname || dp->dnalengthfixed) {
        cprop->length = reinterpret_cast<PropCollectionLengthFunc>(rna_def_property_length_func(
            f, srna, prop, dp, reinterpret_cast<const char *>(cprop->length)));
      }

      /* test if we can allow raw array access, if it is using our standard
       * array get/next function, we can be sure it is an actual array */
      if (cprop->next && cprop->get) {
        if (STREQ((const char *)cprop->next, "rna_iterator_array_next") &&
            STREQ((const char *)cprop->get, "rna_iterator_array_get"))
        {
          prop->flag_internal |= PROP_INTERN_RAW_ARRAY;
        }
      }

      cprop->get = reinterpret_cast<PropCollectionGetFunc>(rna_def_property_get_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(cprop->get)));
      cprop->begin = reinterpret_cast<PropCollectionBeginFunc>(rna_def_property_begin_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(cprop->begin)));
      cprop->next = reinterpret_cast<PropCollectionNextFunc>(rna_def_property_next_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(cprop->next)));
      cprop->end = reinterpret_cast<PropCollectionEndFunc>(rna_def_property_end_func(
          f, srna, prop, dp, reinterpret_cast<const char *>(cprop->end)));
      cprop->lookupint = reinterpret_cast<PropCollectionLookupIntFunc>(
          rna_def_property_lookup_int_func(
              f, srna, prop, dp, reinterpret_cast<const char *>(cprop->lookupint), nextfunc));
      cprop->lookupstring = reinterpret_cast<PropCollectionLookupStringFunc>(
          rna_def_property_lookup_string_func(
              f, srna, prop, dp, reinterpret_cast<const char *>(cprop->lookupstring), item_type));

      if (!(prop->flag & PROP_IDPROPERTY)) {
        if (!cprop->begin) {
          CLOG_ERROR(&LOG,
                     "%s.%s, collection must have a begin function.",
                     srna->identifier,
                     prop->identifier);
          DefRNA.error = true;
        }
        if (!cprop->next) {
          CLOG_ERROR(&LOG,
                     "%s.%s, collection must have a next function.",
                     srna->identifier,
                     prop->identifier);
          DefRNA.error = true;
        }
        if (!cprop->get) {
          CLOG_ERROR(&LOG,
                     "%s.%s, collection must have a get function.",
                     srna->identifier,
                     prop->identifier);
          DefRNA.error = true;
        }
      }
      if (!cprop->item_type) {
        CLOG_ERROR(&LOG,
                   "%s.%s, collection must have a struct type.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }
      break;
    }
  }
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

  if (func->flag & FUNC_USE_SELF_ID) {
    WRITE_PARAM("_selfid");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    WRITE_PARAM("_self");
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    WRITE_PARAM("_type");
  }

  if (func->flag & FUNC_USE_MAIN) {
    WRITE_PARAM("bmain");
  }

  if (func->flag & FUNC_USE_CONTEXT) {
    WRITE_PARAM("C");
  }

  if (func->flag & FUNC_USE_REPORTS) {
    WRITE_PARAM("reports");
  }

  dparm = static_cast<PropertyDefRNA *>(dfunc->cont.properties.first);
  for (; dparm; dparm = dparm->next) {
    if (dparm->prop == func->c_ret) {
      continue;
    }

    WRITE_COMMA;

    if (dparm->prop->flag & PROP_DYNAMIC) {
      fprintf(f, "%s, %s_num", dparm->prop->identifier, dparm->prop->identifier);
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
  const bool has_data = (dfunc->cont.properties.first != nullptr);
  int flag, flag_parameter, pout, cptr, first;

  srna = dsrna->srna;
  func = dfunc->func;

  if (!dfunc->call) {
    return;
  }

  funcname = rna_alloc_function_name(srna->identifier, func->identifier, "call");

  /* function definition */
  fprintf(
      f,
      "static void %s(bContext *C, ReportList *reports, PointerRNA *_ptr, ParameterList *_parms)",
      funcname);
  fprintf(f, "\n{\n");

  /* variable definitions */

  if (func->flag & FUNC_USE_SELF_ID) {
    fprintf(f, "\tID *_selfid;\n");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if ((func->flag & FUNC_SELF_AS_RNA) != 0) {
      fprintf(f, "\tPointerRNA _self;\n");
    }
    else if (dsrna->dnafromprop) {
      fprintf(f, "\t%s *_self;\n", dsrna->dnafromname);
    }
    else if (dsrna->dnaname) {
      fprintf(f, "\t%s *_self;\n", dsrna->dnaname);
    }
    else {
      fprintf(f, "\t%s *_self;\n", srna->identifier);
    }
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    fprintf(f, "\tStructRNA *_type;\n");
  }

  dparm = static_cast<PropertyDefRNA *>(dfunc->cont.properties.first);
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
      if (type == PROP_STRING) {
        ptrstr = pout ? "*" : "";
      }
      else {
        ptrstr = pout ? "**" : "*";
      }
      /* Fixed size arrays and RNA pointers are pre-allocated on the ParameterList stack,
       * pass a pointer to it. */
    }
    else if (type == PROP_POINTER || dparm->prop->arraydimension) {
      ptrstr = "*";
    }
    else if ((type == PROP_POINTER) && (flag_parameter & PARM_RNAPTR) && !(flag & PROP_THICK_WRAP))
    {
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
      fprintf(f, "\tint %s%s_num;\n", pout ? "*" : "", dparm->prop->identifier);
    }

    fprintf(f,
            "\t%s%s %s%s;\n",
            rna_parameter_is_const(dparm) ? "const " : "",
            rna_parameter_type_name(dparm->prop),
            ptrstr,
            rna_safe_id(dparm->prop->identifier));
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
    fprintf(f, "\t_selfid = (ID *)_ptr->owner_id;\n");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if ((func->flag & FUNC_SELF_AS_RNA) != 0) {
      fprintf(f, "\t_self = *_ptr;\n");
    }
    else if (dsrna->dnafromprop) {
      fprintf(f, "\t_self = (%s *)_ptr->data;\n", dsrna->dnafromname);
    }
    else if (dsrna->dnaname) {
      fprintf(f, "\t_self = (%s *)_ptr->data;\n", dsrna->dnaname);
    }
    else {
      fprintf(f, "\t_self = (%s *)_ptr->data;\n", srna->identifier);
    }
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    fprintf(f, "\t_type = _ptr->type;\n");
  }

  if (has_data) {
    fprintf(f, "\t_data = (char *)_parms->data;\n");
  }

  dparm = static_cast<PropertyDefRNA *>(dfunc->cont.properties.first);
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
        if (type == PROP_STRING) {
          ptrstr = "*";
          valstr = "";
        }
        else {
          ptrstr = "**";
          valstr = "*";
        }
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
                "\t%s_num = %s((ParameterDynAlloc *)_data)->array_tot;\n",
                rna_safe_id(dparm->prop->identifier),
                pout ? "(int *)&" : "(int)");
        data_str = "(&(((ParameterDynAlloc *)_data)->array))";
      }
      else {
        data_str = "_data";
      }
      fprintf(f, "\t%s = ", rna_safe_id(dparm->prop->identifier));

      if (!pout) {
        fprintf(f, "%s", valstr);
      }

      fprintf(f,
              "((%s%s %s)%s);\n",
              rna_parameter_is_const(dparm) ? "const " : "",
              rna_parameter_type_name(dparm->prop),
              ptrstr,
              data_str);
    }

    if (dparm->next) {
      fprintf(f, "\t_data += %d;\n", rna_parameter_size_pad(rna_parameter_size(dparm->prop)));
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

    dparm = static_cast<PropertyDefRNA *>(dfunc->cont.properties.first);
    for (; dparm; dparm = dparm->next) {
      if (dparm->prop == func->c_ret) {
        continue;
      }

      if (!first) {
        fprintf(f, ", ");
      }
      first = 0;

      if (dparm->prop->flag & PROP_DYNAMIC) {
        fprintf(f,
                "%s, %s_num",
                rna_safe_id(dparm->prop->identifier),
                rna_safe_id(dparm->prop->identifier));
      }
      else {
        fprintf(f, "%s", rna_safe_id(dparm->prop->identifier));
      }
    }

    fprintf(f, ");\n");

    if (func->c_ret) {
      dparm = rna_find_parameter_def(func->c_ret);
      if ((dparm->prop->type == PROP_POINTER) && (dparm->prop->flag_parameter & PARM_RNAPTR) &&
          (dparm->prop->flag & PROP_THICK_WRAP))
      {
        const char *parameter_type_name = rna_parameter_type_name(dparm->prop);
        fprintf(f,
                "\t*reinterpret_cast<%s *>(_retdata) = %s;\n",
                parameter_type_name,
                func->c_ret->identifier);
      }
      else {
        ptrstr = (((dparm->prop->type == PROP_POINTER) &&
                   !(dparm->prop->flag_parameter & PARM_RNAPTR)) ||
                  (dparm->prop->arraydimension)) ?
                     "*" :
                     "";
        if (dparm->prop->type == PROP_COLLECTION) {
          /* Placement new is necessary because #ParameterList::data is not initialized. */
          fprintf(f,
                  "\tnew ((CollectionVector *)_retdata) CollectionVector(std::move(%s));\n",
                  func->c_ret->identifier);
        }
        else {
          fprintf(f,
                  "\t*((%s %s*)_retdata) = %s;\n",
                  rna_parameter_type_name(dparm->prop),
                  ptrstr,
                  func->c_ret->identifier);
        }
      }
    }
  }

  fprintf(f, "}\n\n");

  dfunc->gencall = funcname;
}

static void rna_sanity_checks()
{
  /* Ensure RNA enum definitions follow naming convention. */
  {
#define DEF_ENUM(id) #id,
    const char *rna_enum_id_array[] = {
#include "RNA_enum_items.hh"
    };
    for (int i = 0; i < ARRAY_SIZE(rna_enum_id_array); i++) {
      if (!(BLI_str_startswith(rna_enum_id_array[i], "rna_enum_") &&
            BLI_str_endswith(rna_enum_id_array[i], "_items")))
      {
        fprintf(stderr,
                "Error: enum defined in \"RNA_enum_items.hh\" "
                "doesn't confirm to \"rna_enum_*_items\" convention!\n");
        DefRNA.error = true;
      }
    }
  }
}

static void rna_auto_types()
{
  StructDefRNA *ds;

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    /* DNA name for Screen is patched in 2.5, we do the reverse here. */
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

    for (PropertyDefRNA &dp : ds->cont.properties) {
      if (dp.dnastructname) {
        if (STREQ(dp.dnastructname, "Screen")) {
          dp.dnastructname = "bScreen";
        }
        if (STREQ(dp.dnastructname, "Group")) {
          dp.dnastructname = "Collection";
        }
        if (STREQ(dp.dnastructname, "GroupObject")) {
          dp.dnastructname = "CollectionObject";
        }
      }

      if (dp.dnatype) {
        if (dp.prop->type == PROP_POINTER) {
          PointerPropertyRNA *pprop = reinterpret_cast<PointerPropertyRNA *>(dp.prop);
          StructRNA *type;

          if (!pprop->type && !pprop->get) {
            pprop->type = reinterpret_cast<StructRNA *>(
                const_cast<char *>(rna_find_type(dp.dnatype)));
          }

          /* Only automatically define `PROP_ID_REFCOUNT` if it was not already explicitly set or
           * cleared by calls to `RNA_def_property_flag` or `RNA_def_property_clear_flag`. */
          if ((pprop->flag_internal & PROP_INTERN_PTR_ID_REFCOUNT_FORCED) == 0 && pprop->type) {
            type = rna_find_struct(reinterpret_cast<const char *>(pprop->type));
            if (type && (type->flag & STRUCT_ID_REFCOUNT)) {
              pprop->flag |= PROP_ID_REFCOUNT;
            }
          }
        }
        else if (dp.prop->type == PROP_COLLECTION) {
          CollectionPropertyRNA *cprop = reinterpret_cast<CollectionPropertyRNA *>(dp.prop);

          if (!cprop->item_type && !cprop->get && STREQ(dp.dnatype, "ListBase")) {
            cprop->item_type = reinterpret_cast<StructRNA *>(
                const_cast<char *>(rna_find_type(dp.dnatype)));
          }
        }
      }
    }
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
    case PROP_PIXEL_DIAMETER:
      return "PROP_PIXEL_DIAMETER";
    case PROP_BYTESTRING:
      return "PROP_BYTESTRING";
    case PROP_UNSIGNED:
      return "PROP_UNSIGNED";
    case PROP_PERCENTAGE:
      return "PROP_PERCENTAGE";
    case PROP_FACTOR:
      return "PROP_FACTOR";
    case PROP_MASS:
      return "PROP_MASS";
    case PROP_ANGLE:
      return "PROP_ANGLE";
    case PROP_TIME:
      return "PROP_TIME";
    case PROP_TIME_ABSOLUTE:
      return "PROP_TIME_ABSOLUTE";
    case PROP_DISTANCE:
      return "PROP_DISTANCE";
    case PROP_DISTANCE_DIAMETER:
      return "PROP_DISTANCE_DIAMETER";
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
    case PROP_TEMPERATURE:
      return "PROP_TEMPERATURE";
    case PROP_WAVELENGTH:
      return "PROP_WAVELENGTH";
    case PROP_COLOR_TEMPERATURE:
      return "PROP_COLOR_TEMPERATURE";
    case PROP_FREQUENCY:
      return "PROP_FREQUENCY";
    default: {
      /* in case we don't have a type preset that includes the subtype */
      if (RNA_SUBTYPE_UNIT(type)) {
        return rna_property_subtypename(PropertySubType(type & ~RNA_SUBTYPE_UNIT(type)));
      }
      return "PROP_SUBTYPE_UNKNOWN";
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
    case PROP_UNIT_TIME_ABSOLUTE:
      return "PROP_UNIT_TIME_ABSOLUTE";
    case PROP_UNIT_VELOCITY:
      return "PROP_UNIT_VELOCITY";
    case PROP_UNIT_ACCELERATION:
      return "PROP_UNIT_ACCELERATION";
    case PROP_UNIT_CAMERA:
      return "PROP_UNIT_CAMERA";
    case PROP_UNIT_POWER:
      return "PROP_UNIT_POWER";
    case PROP_UNIT_TEMPERATURE:
      return "PROP_UNIT_TEMPERATURE";
    case PROP_UNIT_WAVELENGTH:
      return "PROP_UNIT_WAVELENGTH";
    case PROP_UNIT_COLOR_TEMPERATURE:
      return "PROP_UNIT_COLOR_TEMPERATURE";
    case PROP_UNIT_FREQUENCY:
      return "PROP_UNIT_FREQUENCY";
    default:
      return "PROP_UNIT_UNKNOWN";
  }
}

static void rna_generate_struct_rna_prototypes(BlenderRNA *brna, FILE *f)
{
  for (const std::unique_ptr<StructRNA> &srna : brna->structs) {
    fprintf(f, "extern struct StructRNA *RNA_%s;\n", srna->identifier);
  }
}

static void rna_generate_struct_register_prototypes(BlenderRNA *brna, FILE *f)
{
  fprintf(f, "struct BlenderRNA;\n");
  for (const std::unique_ptr<StructRNA> &srna : brna->structs) {
    fprintf(f, "void register_struct_%s(BlenderRNA &brna);\n", srna->identifier);
  }
}

static void rna_generate_blender(BlenderRNA *brna, FILE *f)
{
  fprintf(f,
          "BlenderRNA rna_blender_rna_create()\n"
          "{\n"
          "\tBlenderRNA brna{};\n");
  /* Allocate the structs before creating their definitions, so they can reference each other out
   * of their definition order.*/
  for (std::unique_ptr<StructRNA> &srna : brna->structs) {
    fprintf(f,
            "\tbrna.structs.append(std::make_unique<StructRNA>());\n"
            "\tRNA_%s = brna.structs.last().get();\n",
            srna->identifier);
  }
  for (std::unique_ptr<StructRNA> &srna : brna->structs) {
    fprintf(f, "\tregister_struct_%s(brna);\n", srna->identifier);
  }
  fprintf(f,
          "\treturn brna;\n"
          "}\n");
  fprintf(f,
          "BlenderRNA &RNA_blender_rna_get()\n"
          "{\n"
          "\tstatic BlenderRNA BLENDER_RNA = rna_blender_rna_create();\n");
  /* structs_map is created by RNA_init(). */
  fprintf(f,
          "\treturn BLENDER_RNA;\n"
          "}\n\n");
}

static void rna_generate_external_property_prototypes(BlenderRNA *brna, FILE *f)
{
  fprintf(f, "struct PropertyRNA;\n");
  fprintf(f, "struct StructRNA;\n\n");

  rna_generate_struct_rna_prototypes(brna, f);

  /* NOTE: Generate generic `PropertyRNA &` references. The actual, type-refined properties data
   * are static variables in their translation units (the `_gen.cc` files), which are assigned to
   * these public generic `PointerRNA &` references. */
  for (std::unique_ptr<StructRNA> &srna : brna->structs) {
    for (PropertyRNA &prop : srna->cont.properties) {
      fprintf(f, "extern PropertyRNA &rna_%s_%s;\n", srna->identifier, prop.identifier);
    }
    fprintf(f, "\n");
  }
}

static void rna_generate_internal_property_prototypes(BlenderRNA * /*brna*/,
                                                      StructRNA *srna,
                                                      FILE *f)
{
  StructRNA *base;

  /* NOTE: Generic `PropertyRNA &` references, see #rna_generate_external_property_prototypes
   * comments for details. */
  base = srna->base;
  while (base) {
    fprintf(f, "\n");
    for (PropertyRNA &prop : base->cont.properties) {
      fprintf(f, "extern PropertyRNA &rna_%s_%s;\n", base->identifier, prop.identifier);
    }
    base = base->base;
  }

  if (srna->cont.properties.first) {
    fprintf(f, "\n");
  }

  for (PropertyRNA &prop : srna->cont.properties) {
    fprintf(f, "extern PropertyRNA &rna_%s_%s;\n", srna->identifier, prop.identifier);
  }
  fprintf(f, "\n");
}

static void rna_generate_parameter_prototypes(BlenderRNA * /*brna*/,
                                              StructRNA *srna,
                                              FunctionRNA *func,
                                              FILE *f)
{
  /* NOTE: Generic `PropertyRNA &` references, see #rna_generate_external_property_prototypes
   * comments for details. */
  for (PropertyRNA &parm : func->cont.properties) {
    fprintf(f,
            "extern PropertyRNA &rna_%s_%s_%s;\n",
            srna->identifier,
            func->identifier,
            parm.identifier);
  }

  if (func->cont.properties.first) {
    fprintf(f, "\n");
  }
}

static void rna_generate_function_prototypes(BlenderRNA *brna, StructRNA *srna, FILE *f)
{
  StructRNA *base;

  base = srna->base;
  while (base) {
    for (const std::unique_ptr<FunctionRNA> &func : base->functions) {
      fprintf(f, "extern FunctionRNA *rna_%s_%s_func;\n", base->identifier, func->identifier);
      rna_generate_parameter_prototypes(brna, base, func.get(), f);
    }

    if (!base->functions.is_empty()) {
      fprintf(f, "\n");
    }

    base = base->base;
  }

  for (const std::unique_ptr<FunctionRNA> &func : srna->functions) {
    fprintf(f, "extern FunctionRNA *rna_%s_%s_func;\n", srna->identifier, func->identifier);
    rna_generate_parameter_prototypes(brna, srna, func.get(), f);
  }

  if (!srna->functions.is_empty()) {
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
  PropertyDefRNA *dparm_return = nullptr;
  StructDefRNA *dsrna;
  PropertyType type;
  int flag, flag_parameter, pout, cptr, first;
  const char *ptrstr;

  dsrna = rna_find_struct_def(srna);
  func = dfunc->func;

  /* return type */
  for (PropertyDefRNA &dparm : dfunc->cont.properties) {
    if (dparm.prop == func->c_ret) {
      if (dparm.prop->arraydimension) {
        fprintf(f, "XXX no array return types yet"); /* XXX not supported */
      }
      else if (dparm.prop->type == PROP_POINTER && !(dparm.prop->flag_parameter & PARM_RNAPTR)) {
        fprintf(f, "%s *", rna_parameter_type_name(dparm.prop));
      }
      else {
        fprintf(f, "%s ", rna_parameter_type_name(dparm.prop));
      }

      dparm_return = &dparm;
      break;
    }
  }

  /* void if nothing to return */
  if (!dparm_return) {
    fprintf(f, "void ");
  }

  /* function name */
  if (name_override == nullptr || name_override[0] == '\0') {
    fprintf(f, "%s(", dfunc->call);
  }
  else {
    fprintf(f, "%s(", name_override);
  }

  first = 1;

  /* self, context and reports parameters */
  if (func->flag & FUNC_USE_SELF_ID) {
    fprintf(f, "ID *_selfid");
    first = 0;
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if (!first) {
      fprintf(f, ", ");
    }
    if ((func->flag & FUNC_SELF_AS_RNA) != 0) {
      fprintf(f, "PointerRNA _self");
    }
    else if (dsrna->dnafromprop) {
      fprintf(f, "%s *_self", dsrna->dnafromname);
    }
    else if (dsrna->dnaname) {
      fprintf(f, "%s *_self", dsrna->dnaname);
    }
    else {
      fprintf(f, "%s *_self", srna->identifier);
    }
    first = 0;
  }
  else if (func->flag & FUNC_USE_SELF_TYPE) {
    if (!first) {
      fprintf(f, ", ");
    }
    fprintf(f, "StructRNA *_type");
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
  for (PropertyDefRNA &dparm : dfunc->cont.properties) {
    type = dparm.prop->type;
    flag = dparm.prop->flag;
    flag_parameter = dparm.prop->flag_parameter;
    pout = (flag_parameter & PARM_OUTPUT);
    cptr = ((type == PROP_POINTER) && !(flag_parameter & PARM_RNAPTR));

    if (dparm.prop == func->c_ret) {
      continue;
    }

    if (cptr || (flag & PROP_DYNAMIC)) {
      if (type == PROP_STRING) {
        ptrstr = pout ? "*" : "";
      }
      else {
        ptrstr = pout ? "**" : "*";
      }
    }
    else if (type == PROP_POINTER || dparm.prop->arraydimension) {
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
      fprintf(f, "int %s%s_num, ", pout ? "*" : "", dparm.prop->identifier);
    }

    if (!(flag & PROP_DYNAMIC) && dparm.prop->arraydimension) {
      fprintf(f,
              "%s %s[%u]",
              rna_parameter_type_name(dparm.prop),
              rna_safe_id(dparm.prop->identifier),
              dparm.prop->totarraylength);
    }
    else {
      fprintf(f,
              "%s %s%s",
              rna_parameter_type_name(dparm.prop),
              ptrstr,
              rna_safe_id(dparm.prop->identifier));
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

static void rna_generate_static_function_prototypes(BlenderRNA * /*brna*/,
                                                    StructRNA *srna,
                                                    FILE *f)
{
  FunctionDefRNA *dfunc;
  int first = 1;

  for (const std::unique_ptr<FunctionRNA> &func : srna->functions) {
    dfunc = rna_find_function_def(func.get());

    if (dfunc->call) {
      if (strstr(dfunc->call, "<")) {
        /* Can't generate the declaration for templates. We'll still get compile errors when trying
         * to call it with a wrong signature. */
        continue;
      }

      if (first) {
        fprintf(f, "/* Repeated prototypes to detect errors */\n\n");
        first = 0;
      }

      rna_generate_static_parameter_prototypes(f, srna, dfunc, nullptr, 1);
    }
  }

  fprintf(f, "\n");
}

static void rna_generate_property_decl(FILE *f,
                                       StructRNA *srna,
                                       const char *nest,
                                       PropertyRNA *prop)
{
  char *strnest = (char *)"", *errnest = (char *)"";
  bool freenest = false;

  if (nest != nullptr) {
    size_t len = strlen(nest);

    strnest = MEM_new_array_uninitialized<char>(len + 2, "rna_generate_property -> strnest");
    errnest = MEM_new_array_uninitialized<char>(len + 2, "rna_generate_property -> errnest");

    strnest[0] = '_';
    memcpy(strnest + 1, nest, len + 1);

    errnest[0] = '.';
    memcpy(errnest + 1, nest, len + 1);

    freenest = true;
  }

  /* Generate the RNA-private, type-refined property data.
   *
   * See #rna_generate_external_property_prototypes comments for details. */
  fprintf(f,
          "static %s rna_%s%s_%s_;\n",
          rna_property_structname(prop->type),
          srna->identifier,
          strnest,
          prop->identifier);

  /* Assign the RNA-private, type-refined static (local) property data to the public matching
   * generic `PropertyRNA &` reference.
   *
   * See #rna_generate_external_property_prototypes comments for details. */
  fprintf(
      f,
      /* Use a reference here instead of a pointer, because pointer usage somehow makes clang
       * optimizer take a very long time to compile the `rna_xxx_gen.cc` files (see faf56cc3bf).
       *
       * Note that in theory, any access to the 'public' `PointerRNA &` reference data is
       * undefined behavior (strict aliasing rules). This is currently not a real issue (these
       * PropertyRNA definitions are almost always only used as pointers, and are currently POD
       * types).
       *
       * `reinterpret_cast<PropertyRNA &>(rna_prop_data)` here is same as
       * `*reinterpret_cast<PropertyRNA *>(&rna_prop_data)` (see point (6) of
       * https://en.cppreference.com/w/cpp/language/reinterpret_cast). */
      "PropertyRNA &rna_%s%s_%s = reinterpret_cast<PropertyRNA &>(rna_%s%s_%s_);\n\n",
      srna->identifier,
      strnest,
      prop->identifier,
      srna->identifier,
      strnest,
      prop->identifier);

  if (freenest) {
    MEM_delete(strnest);
    MEM_delete(errnest);
  }
}

static void rna_generate_property(FILE *f, StructRNA *srna, const char *nest, PropertyRNA *prop)
{
  char *strnest = const_cast<char *>(""), *errnest = const_cast<char *>("");
  bool freenest = false;

  if (nest != nullptr) {
    size_t len = strlen(nest);

    strnest = MEM_new_array_uninitialized<char>(len + 2, "rna_generate_property -> strnest");
    errnest = MEM_new_array_uninitialized<char>(len + 2, "rna_generate_property -> errnest");

    strnest[0] = '_';
    memcpy(strnest + 1, nest, len + 1);

    errnest[0] = '.';
    memcpy(errnest + 1, nest, len + 1);

    freenest = true;
  }

  if (prop->deprecated) {
    fprintf(f,
            "\tstatic const DeprecatedRNA rna_%s%s_%s_deprecated = {\n\t",
            srna->identifier,
            strnest,
            prop->identifier);
    rna_print_c_string(f, prop->deprecated->note);
    fprintf(f, ",\n\t\t%d, %d,\n", prop->deprecated->version, prop->deprecated->removal_version);
    fprintf(f, "};\n\n");
  }

  switch (prop->type) {
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop);
      int i, defaultfound = 0, totflag = 0;

      if (eprop->item) {
        /* Inline the enum if this is not a defined in "RNA_enum_items.hh". */
        const char *item_global_id = rna_enum_id_from_pointer(eprop->item);
        if (item_global_id == nullptr) {
          fprintf(f,
                  "\tstatic const EnumPropertyItem rna_%s%s_%s_items[%d] = {\n\t\t",
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
            fprintf(f, "\t},\n\t\t");

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

          fprintf(f, "\t{0, nullptr, 0, nullptr, nullptr}\n\t};\n");
        }
        else {
          for (i = 0; i < eprop->totitem; i++) {
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
        }

        if (prop->flag & PROP_ENUM_FLAG) {
          if (eprop->defaultvalue & ~totflag) {
            CLOG_ERROR(&LOG,
                       "%s%s.%s, enum default includes unused bits (%d).",
                       srna->identifier,
                       errnest,
                       prop->identifier,
                       eprop->defaultvalue & ~totflag);
            DefRNA.error = true;
          }
        }
        else {
          if (!defaultfound && !(eprop->item_fn && eprop->item == rna_enum_dummy_NULL_items)) {
            CLOG_ERROR(&LOG,
                       "%s%s.%s, enum default '%d' is not in items.",
                       srna->identifier,
                       errnest,
                       prop->identifier,
                       eprop->defaultvalue);
            DefRNA.error = true;
          }
        }
      }
      else {
        CLOG_ERROR(&LOG,
                   "%s%s.%s, enum must have items defined.",
                   srna->identifier,
                   errnest,
                   prop->identifier);
        DefRNA.error = true;
      }
      break;
    }
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(prop);
      uint i;

      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f,
                "\tstatic bool rna_%s%s_%s_default[%u] = {\n\t\t",
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
            fprintf(f, ",\n\t\t");
          }
        }

        fprintf(f, "\n\t};\n");
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop);
      uint i;

      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f,
                "\tstatic int rna_%s%s_%s_default[%u] = {\n\t\t",
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
            fprintf(f, ",\n\t\t");
          }
        }

        fprintf(f, "\n\t};\n");
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop);
      uint i;

      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f,
                "\tstatic float rna_%s%s_%s_default[%u] = {\n\t\t",
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
            fprintf(f, ",\n\t\t");
          }
        }

        fprintf(f, "\n\t};\n");
      }
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = reinterpret_cast<PointerPropertyRNA *>(prop);

      /* XXX This systematically enforces that flag on ID pointers...
       * we'll probably have to revisit. :/ */
      StructRNA *type = rna_find_struct(reinterpret_cast<const char *>(pprop->type));
      if (type && (type->flag & STRUCT_ID) &&
          !(prop->flag_internal & PROP_INTERN_PTR_OWNERSHIP_FORCED))
      {
        RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = reinterpret_cast<CollectionPropertyRNA *>(prop);

      /* XXX This systematically enforces that flag on ID pointers...
       * we'll probably have to revisit. :/ */
      StructRNA *type = rna_find_struct(reinterpret_cast<const char *>(cprop->item_type));
      if (type && (type->flag & STRUCT_ID) &&
          !(prop->flag_internal & PROP_INTERN_PTR_OWNERSHIP_FORCED))
      {
        RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
      }
      break;
    }
    default:
      break;
  }

  fprintf(f, "\trna_%s%s_%s_ = {\n", srna->identifier, strnest, prop->identifier);

  if (prop->next) {
    fprintf(f, "\t\t{&rna_%s%s_%s, ", srna->identifier, strnest, prop->next->identifier);
  }
  else {
    fprintf(f, "\t\t{nullptr, ");
  }
  if (prop->prev) {
    fprintf(f, "\t&rna_%s%s_%s,\n", srna->identifier, strnest, prop->prev->identifier);
  }
  else {
    fprintf(f, "\tnullptr,\n");
  }
  fprintf(f, "\t\t%d, ", prop->magic);
  rna_print_c_string(f, prop->identifier);
  fprintf(f,
          ", %d, %d, %d, %d, %d, ",
          prop->flag,
          prop->flag_override,
          prop->flag_parameter,
          prop->flag_internal,
          prop->tags);
  fprintf(f, "PropertyPathTemplateType(%d), ", prop->path_template_type);
  rna_print_c_string(f, prop->name);
  fprintf(f, ",\n\t\t");
  rna_print_c_string(f, prop->description);
  fprintf(f, ",\n\t\t");
  fprintf(f, "%d, ", prop->icon);
  rna_print_c_string(f, prop->translation_context);
  fprintf(f, ",\n\t\t");

  if (prop->deprecated) {
    fprintf(f, "&rna_%s%s_%s_deprecated,", srna->identifier, strnest, prop->identifier);
  }
  else {
    fprintf(f, "nullptr,\n");
  }

  fprintf(f,
          "\t\t%s, PropertySubType(int(%s) | int(%s)), %s, %u, {%u, %u, %u}, %u,\n",
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
          "\t\t%s%s, %d, %s, %s, %s, %s, %s, %s, %s,\n\t",
          /* NOTE: void cast is needed to quiet function cast warning in C++. */
          (prop->flag & PROP_CONTEXT_UPDATE) ? "(UpdateFunc)(void *)" : "",
          rna_function_string(prop->update),
          prop->noteflag,
          rna_function_string(prop->editable),
          rna_function_string(prop->itemeditable),
          rna_function_string(prop->ui_name_func),
          rna_function_string(prop->ui_description_func),
          rna_function_string(prop->override_diff),
          rna_function_string(prop->override_store),
          rna_function_string(prop->override_apply));

  if (prop->flag_internal & PROP_INTERN_RAW_ACCESS) {
    rna_set_raw_offset(f, srna, prop);
  }
  else {
    fprintf(f, "\t\t0, PROP_RAW_UNSET");
  }

  /* our own type - collections/arrays only */
  if (prop->srna) {
    fprintf(f, ", RNA_%s", reinterpret_cast<const char *>(prop->srna));
  }
  else {
    fprintf(f, ", nullptr");
  }

  fprintf(f, "},\n");

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = reinterpret_cast<BoolPropertyRNA *>(prop);
      fprintf(f,
              "\t\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %d, ",
              rna_function_string(bprop->get),
              rna_function_string(bprop->set),
              rna_function_string(bprop->getarray),
              rna_function_string(bprop->setarray),
              rna_function_string(bprop->get_ex),
              rna_function_string(bprop->set_ex),
              rna_function_string(bprop->getarray_ex),
              rna_function_string(bprop->setarray_ex),
              rna_function_string(bprop->get_transform),
              rna_function_string(bprop->set_transform),
              rna_function_string(bprop->getarray_transform),
              rna_function_string(bprop->setarray_transform),
              rna_function_string(bprop->get_default),
              rna_function_string(bprop->get_default_array),
              bprop->defaultvalue);
      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
      }
      else {
        fprintf(f, "nullptr\n");
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = reinterpret_cast<IntPropertyRNA *>(prop);
      fprintf(f,
              "\t\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s,\n\t\t",
              rna_function_string(iprop->get),
              rna_function_string(iprop->set),
              rna_function_string(iprop->getarray),
              rna_function_string(iprop->setarray),
              rna_function_string(iprop->range),
              rna_function_string(iprop->get_ex),
              rna_function_string(iprop->set_ex),
              rna_function_string(iprop->getarray_ex),
              rna_function_string(iprop->setarray_ex),
              rna_function_string(iprop->range_ex),
              rna_function_string(iprop->get_transform),
              rna_function_string(iprop->set_transform),
              rna_function_string(iprop->getarray_transform),
              rna_function_string(iprop->setarray_transform));
      fprintf(f, "%s", rna_ui_scale_type_string(iprop->ui_scale_type));
      fprintf(f, ", ");
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
      fprintf(f,
              "%s, %s",
              rna_function_string(iprop->get_default),
              rna_function_string(iprop->get_default_array));
      fprintf(f, ", ");
      rna_int_print(f, iprop->defaultvalue);
      fprintf(f, ", ");
      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
      }
      else {
        fprintf(f, "nullptr\n");
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = reinterpret_cast<FloatPropertyRNA *>(prop);
      fprintf(f,
              "\t\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ",
              rna_function_string(fprop->get),
              rna_function_string(fprop->set),
              rna_function_string(fprop->getarray),
              rna_function_string(fprop->setarray),
              rna_function_string(fprop->range),
              rna_function_string(fprop->get_ex),
              rna_function_string(fprop->set_ex),
              rna_function_string(fprop->getarray_ex),
              rna_function_string(fprop->setarray_ex),
              rna_function_string(fprop->range_ex),
              rna_function_string(fprop->get_transform),
              rna_function_string(fprop->set_transform),
              rna_function_string(fprop->getarray_transform),
              rna_function_string(fprop->setarray_transform));
      fprintf(f, "%s, ", rna_ui_scale_type_string(fprop->ui_scale_type));
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
      rna_int_print(f, fprop->precision);
      fprintf(f, ", ");
      fprintf(f,
              "%s, %s",
              rna_function_string(fprop->get_default),
              rna_function_string(fprop->get_default_array));
      fprintf(f, ", ");
      rna_float_print(f, fprop->defaultvalue);
      fprintf(f, ", ");
      if (prop->arraydimension && prop->totarraylength) {
        fprintf(f, "rna_%s%s_%s_default\n", srna->identifier, strnest, prop->identifier);
      }
      else {
        fprintf(f, "nullptr\n");
      }
      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = reinterpret_cast<StringPropertyRNA *>(prop);
      fprintf(
          f,
          "\t\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, eStringPropertySearchFlag(%d), %s, %d, ",
          rna_function_string(sprop->get),
          rna_function_string(sprop->length),
          rna_function_string(sprop->set),
          rna_function_string(sprop->get_ex),
          rna_function_string(sprop->length_ex),
          rna_function_string(sprop->set_ex),
          rna_function_string(sprop->get_transform),
          rna_function_string(sprop->set_transform),
          rna_function_string(sprop->get_default),
          rna_function_string(sprop->search),
          int(sprop->search_flag),
          rna_function_string(sprop->path_filter),
          sprop->maxlength);
      rna_print_c_string(f, sprop->defaultvalue);
      fprintf(f, "\n");
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop);
      fprintf(f,
              "\t\t%s, %s, %s, %s, %s, %s, %s, %s, ",
              rna_function_string(eprop->get),
              rna_function_string(eprop->set),
              rna_function_string(eprop->item_fn),
              rna_function_string(eprop->get_ex),
              rna_function_string(eprop->set_ex),
              rna_function_string(eprop->get_transform),
              rna_function_string(eprop->set_transform),
              rna_function_string(eprop->get_default));
      if (eprop->item) {
        const char *item_global_id = rna_enum_id_from_pointer(eprop->item);
        if (item_global_id != nullptr) {
          fprintf(f, "%s, ", item_global_id);
        }
        else {
          fprintf(f, "rna_%s%s_%s_items, ", srna->identifier, strnest, prop->identifier);
        }
      }
      else {
        fprintf(f, "nullptr, ");
      }
      fprintf(f, "%d, %d\n", eprop->totitem, eprop->defaultvalue);
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = reinterpret_cast<PointerPropertyRNA *>(prop);
      fprintf(f,
              "\t\t%s, %s, %s, %s,",
              rna_function_string(pprop->get),
              rna_function_string(pprop->set),
              rna_function_string(pprop->type_fn),
              rna_function_string(pprop->poll));
      if (pprop->type) {
        fprintf(f, "RNA_%s\n", reinterpret_cast<const char *>(pprop->type));
      }
      else {
        fprintf(f, "nullptr\n");
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = reinterpret_cast<CollectionPropertyRNA *>(prop);
      fprintf(f,
              "\t\t%s, %s, %s, %s, %s, %s, %s, %s, ",
              rna_function_string(cprop->begin),
              rna_function_string(cprop->next),
              rna_function_string(cprop->end),
              rna_function_string(cprop->get),
              rna_function_string(cprop->length),
              rna_function_string(cprop->lookupint),
              rna_function_string(cprop->lookupstring),
              rna_function_string(cprop->assignint));
      if (cprop->item_type) {
        fprintf(f, "RNA_%s\n", reinterpret_cast<const char *>(cprop->item_type));
      }
      else {
        fprintf(f, "nullptr\n");
      }
      break;
    }
  }

  fprintf(f, "\t};\n");

  if (freenest) {
    MEM_delete(strnest);
    MEM_delete(errnest);
  }
}

static void rna_generate_struct_register_func(BlenderRNA * /*brna*/, StructRNA *srna, FILE *f)
{
  PropertyRNA *prop;
  StructRNA *base;

  fprintf(f, "/* %s */\n", srna->name);

  /* Generate static variables before their creation. */
  for (PropertyRNA &prop : srna->cont.properties) {
    rna_generate_property_decl(f, srna, nullptr, &prop);
  }
  for (const std::unique_ptr<FunctionRNA> &func : srna->functions) {
    for (PropertyRNA &parm : func->cont.properties) {
      rna_generate_property_decl(f, srna, func->identifier, &parm);
    }
    fprintf(f, "FunctionRNA *rna_%s_%s_func;\n", srna->identifier, func->identifier);
  }

  /* Struct and property creation runs on startup, on the first call to #RNA_blender_rna_get. */
  fprintf(f,
          "StructRNA *RNA_%s;\n"
          "void register_struct_%s(BlenderRNA &brna)\n"
          "{\n",
          srna->identifier,
          srna->identifier);

  for (const auto [i, prop] : srna->cont.properties.enumerate()) {
    if (i != 0) {
      fprintf(f, "\n");
    }
    rna_generate_property(f, srna, nullptr, &prop);
  }

  fprintf(f,
          "\n"
          "\tStructRNA *srna = RNA_%s;\n",
          srna->identifier);

  prop = static_cast<PropertyRNA *>(srna->cont.properties.first);
  if (prop) {
    fprintf(f, "\tsrna->cont.properties = {&rna_%s_%s, ", srna->identifier, prop->identifier);
  }
  else {
    fprintf(f, "\tsrna->cont.properties = {nullptr, ");
  }

  prop = static_cast<PropertyRNA *>(srna->cont.properties.last);
  if (prop) {
    fprintf(f, "&rna_%s_%s};\n", srna->identifier, prop->identifier);
  }
  else {
    fprintf(f, "nullptr};\n");
  }
  fprintf(f, "\tsrna->identifier = ");
  rna_print_c_string(f, srna->identifier);
  fprintf(f,
          ";\n"
          "\tsrna->flag = %d;\n",
          srna->flag);
  fprintf(f, "\tsrna->name = ");
  rna_print_c_string(f, srna->name);
  fprintf(f,
          ";\n"
          "\tsrna->description = ");
  rna_print_c_string(f, srna->description);
  fprintf(f,
          ";\n"
          "\tsrna->translation_context = ");
  rna_print_c_string(f, srna->translation_context);
  fprintf(f,
          ";\n"
          "\tsrna->icon = %d;\n",
          srna->icon);

  prop = srna->nameproperty;
  if (prop) {
    base = srna;
    while (base->base && base->base->nameproperty == prop) {
      base = base->base;
    }

    fprintf(f, "\tsrna->nameproperty = &rna_%s_%s;\n", base->identifier, prop->identifier);
  }

  prop = srna->iteratorproperty;
  base = srna;
  while (base->base && base->base->iteratorproperty == prop) {
    base = base->base;
  }
  fprintf(f, "\tsrna->iteratorproperty = &rna_%s_rna_properties;\n", base->identifier);

  if (srna->base) {
    fprintf(f, "\tsrna->base = RNA_%s;\n", srna->base->identifier);
  }

  if (srna->nested) {
    fprintf(f, "\tsrna->nested = RNA_%s;\n", srna->nested->identifier);
  }

  if (srna->refine) {
    fprintf(f, "\tsrna->refine = %s;\n", rna_function_string(srna->refine));
  }
  if (srna->path) {
    fprintf(f, "\tsrna->path = %s;\n", rna_function_string(srna->path));
  }
  if (srna->reg) {
    fprintf(f, "\tsrna->reg = %s;\n", rna_function_string(srna->reg));
  }
  if (srna->unreg) {
    fprintf(f, "\tsrna->unreg = %s;\n", rna_function_string(srna->unreg));
  }
  if (srna->instance) {
    fprintf(f, "\tsrna->instance = %s;\n", rna_function_string(srna->instance));
  }
  if (srna->idproperties) {
    fprintf(f, "\tsrna->idproperties = %s;\n", rna_function_string(srna->idproperties));
  }
  if (srna->system_idproperties) {
    fprintf(
        f, "\tsrna->system_idproperties = %s;\n", rna_function_string(srna->system_idproperties));
  }

  if (srna->reg && !srna->refine) {
    CLOG_ERROR(
        &LOG, "%s has a register function, must also have refine function.", srna->identifier);
    DefRNA.error = true;
  }

  for (const std::unique_ptr<FunctionRNA> &func : srna->functions) {
    fprintf(f, "\t{\n");
    for (PropertyRNA &parm : func->cont.properties) {
      rna_generate_property(f, srna, func->identifier, &parm);
    }
    fprintf(f, "\t\tauto func = std::make_unique<FunctionRNA>();\n");
    if (!BLI_listbase_is_empty(&func->cont.properties)) {
      fprintf(f,
              "\t\tfunc->cont.properties = {&rna_%s_%s_%s, &rna_%s_%s_%s};\n",
              srna->identifier,
              func->identifier,
              static_cast<PropertyRNA *>(func->cont.properties.first)->identifier,
              srna->identifier,
              func->identifier,
              static_cast<PropertyRNA *>(func->cont.properties.last)->identifier);
    }
    fprintf(f, "\t\tfunc->identifier = ");
    rna_print_c_string(f, func->identifier);
    fprintf(f, ";\n");
    if (func->flag != 0) {
      fprintf(f, "\t\tfunc->flag = %d;\n", func->flag);
    }
    fprintf(f, "\t\tfunc->description = ");
    rna_print_c_string(f, func->description);
    fprintf(f, ";\n");
    FunctionDefRNA *dfunc = rna_find_function_def(func.get());
    if (dfunc->gencall) {
      fprintf(f, "\t\tfunc->call = %s;\n", dfunc->gencall);
    }
    if (func->c_ret) {
      fprintf(f,
              "\t\tfunc->c_ret = &rna_%s_%s_%s;\n",
              srna->identifier,
              func->identifier,
              func->c_ret->identifier);
    }
    fprintf(f, "\t\trna_%s_%s_func = func.get();\n", srna->identifier, func->identifier);
    fprintf(f,
            "\t\tsrna->functions.append(std::move(func));\n"
            "\t}\n");
  }

  fprintf(f, "};\n\n");
}

struct RNAProcessItem {
  const char *filename;
  const char *api_filename;
  void (*define)(BlenderRNA *brna);
};

static RNAProcessItem PROCESS_ITEMS[] = {
    {"rna_rna.cc", nullptr, RNA_def_rna},
    {"rna_ID.cc", nullptr, RNA_def_ID},
    {"rna_texture.cc", "rna_texture_api.cc", RNA_def_texture},
    {"rna_action.cc", "rna_action_api.cc", RNA_def_action},
    {"rna_animation.cc", "rna_animation_api.cc", RNA_def_animation},
    {"rna_animviz.cc", nullptr, RNA_def_animviz},
    {"rna_armature.cc", "rna_armature_api.cc", RNA_def_armature},
    {"rna_attribute.cc", nullptr, RNA_def_attribute},
    {"rna_asset.cc", nullptr, RNA_def_asset},
    {"rna_boid.cc", nullptr, RNA_def_boid},
    {"rna_brush.cc", nullptr, RNA_def_brush},
    {"rna_cachefile.cc", nullptr, RNA_def_cachefile},
    {"rna_camera.cc", "rna_camera_api.cc", RNA_def_camera},
    {"rna_cloth.cc", nullptr, RNA_def_cloth},
    {"rna_collection.cc", nullptr, RNA_def_collections},
    {"rna_color.cc", nullptr, RNA_def_color},
    {"rna_constraint.cc", nullptr, RNA_def_constraint},
    {"rna_context.cc", nullptr, RNA_def_context},
    {"rna_curve.cc", "rna_curve_api.cc", RNA_def_curve},
    {"rna_dynamicpaint.cc", nullptr, RNA_def_dynamic_paint},
    {"rna_fcurve.cc", "rna_fcurve_api.cc", RNA_def_fcurve},
    {"rna_annotations.cc", nullptr, RNA_def_annotations},
    {"rna_grease_pencil.cc", "rna_grease_pencil_api.cc", RNA_def_grease_pencil},
    {"rna_curves.cc", "rna_curves_api.cc", RNA_def_curves},
    {"rna_image.cc", "rna_image_api.cc", RNA_def_image},
    {"rna_key.cc", nullptr, RNA_def_key},
    {"rna_light.cc", nullptr, RNA_def_light},
    {"rna_lattice.cc", "rna_lattice_api.cc", RNA_def_lattice},
    {"rna_layer.cc", nullptr, RNA_def_view_layer},
    {"rna_linestyle.cc", nullptr, RNA_def_linestyle},
    {"rna_blendfile_import.cc", nullptr, RNA_def_blendfile_import},
    {"rna_main.cc", "rna_main_api.cc", RNA_def_main},
    {"rna_fluid.cc", nullptr, RNA_def_fluid},
    {"rna_material.cc", "rna_material_api.cc", RNA_def_material},
    {"rna_mesh.cc", "rna_mesh_api.cc", RNA_def_mesh},
    {"rna_meta.cc", "rna_meta_api.cc", RNA_def_meta},
    {"rna_modifier.cc", nullptr, RNA_def_modifier},
    {"rna_shader_fx.cc", nullptr, RNA_def_shader_fx},
    {"rna_nla.cc", nullptr, RNA_def_nla},
    {"rna_nodetree.cc", nullptr, RNA_def_nodetree},
    {"rna_node_socket.cc", nullptr, RNA_def_node_socket_subtypes},
    {"rna_node_tree_interface.cc", nullptr, RNA_def_node_tree_interface},
    {"rna_object.cc", "rna_object_api.cc", RNA_def_object},
    {"rna_object_force.cc", nullptr, RNA_def_object_force},
    {"rna_depsgraph.cc", nullptr, RNA_def_depsgraph},
    {"rna_packedfile.cc", nullptr, RNA_def_packedfile},
    {"rna_palette.cc", nullptr, RNA_def_palette},
    {"rna_particle.cc", nullptr, RNA_def_particle},
    {"rna_pointcloud.cc", nullptr, RNA_def_pointcloud},
    {"rna_pose.cc", "rna_pose_api.cc", RNA_def_pose},
    {"rna_curveprofile.cc", nullptr, RNA_def_profile},
    {"rna_lightprobe.cc", nullptr, RNA_def_lightprobe},
    {"rna_render.cc", nullptr, RNA_def_render},
    {"rna_rigidbody.cc", nullptr, RNA_def_rigidbody},
    {"rna_scene.cc", "rna_scene_api.cc", RNA_def_scene},
    {"rna_screen.cc", nullptr, RNA_def_screen},
    {"rna_sculpt_paint.cc", nullptr, RNA_def_sculpt_paint},
    {"rna_sequencer.cc", "rna_sequencer_api.cc", RNA_def_sequencer},
    {"rna_space.cc", "rna_space_api.cc", RNA_def_space},
    {"rna_speaker.cc", nullptr, RNA_def_speaker},
    {"rna_test.cc", nullptr, RNA_def_test},
    {"rna_text.cc", "rna_text_api.cc", RNA_def_text},
    {"rna_timeline.cc", nullptr, RNA_def_timeline_marker},
    {"rna_sound.cc", "rna_sound_api.cc", RNA_def_sound},
    {"rna_ui.cc", "rna_ui_api.cc", RNA_def_ui},
#ifdef WITH_USD
    {"rna_usd.cc", nullptr, RNA_def_usd},
#endif
    {"rna_userdef.cc", nullptr, RNA_def_userdef},
    {"rna_vfont.cc", "rna_vfont_api.cc", RNA_def_vfont},
    {"rna_volume.cc", nullptr, RNA_def_volume},
    {"rna_wm.cc", "rna_wm_api.cc", RNA_def_wm},
    {"rna_wm_gizmo.cc", "rna_wm_gizmo_api.cc", RNA_def_wm_gizmo},
    {"rna_workspace.cc", "rna_workspace_api.cc", RNA_def_workspace},
    {"rna_world.cc", nullptr, RNA_def_world},
    {"rna_movieclip.cc", nullptr, RNA_def_movieclip},
    {"rna_tracking.cc", nullptr, RNA_def_tracking},
    {"rna_mask.cc", nullptr, RNA_def_mask},
    {"rna_xr.cc", nullptr, RNA_def_xr},
    {nullptr, nullptr},
};

static void rna_generate(BlenderRNA *brna, FILE *f, const char *filename, const char *api_filename)
{
  StructDefRNA *ds;
  FunctionDefRNA *dfunc;

  fprintf(f,
          "\n"
          "/* Automatically generated struct definitions for the Data API.\n"
          " * Do not edit manually, changes will be overwritten.           */\n\n");

  fprintf(f, "#include <float.h>\n");
  fprintf(f, "#include <stdio.h>\n");
  fprintf(f, "#include <limits.h>\n");
  fprintf(f, "#include <limits>\n");
  fprintf(f, "#include <string.h>\n\n");
  fprintf(f, "#include <stddef.h>\n\n");
  fprintf(f, "#include <algorithm>\n\n");

  fprintf(f, "#include \"MEM_guardedalloc.h\"\n\n");

  fprintf(f, "#include \"DNA_ID.h\"\n");
  fprintf(f, "#include \"DNA_scene_types.h\"\n");
  fprintf(f, "#include \"DNA_node_types.h\"\n");

  fprintf(f, "#include \"BLI_fileops.h\"\n\n");
  fprintf(f, "#include \"BLI_listbase.h\"\n\n");
  fprintf(f, "#include \"BLI_path_utils.hh\"\n\n");
  fprintf(f, "#include \"BLI_rect.h\"\n\n");
  fprintf(f, "#include \"BLI_string.h\"\n\n");
  fprintf(f, "#include \"BLI_string_utf8.h\"\n\n");
  fprintf(f, "#include \"BLI_utildefines.h\"\n\n");

  fprintf(f, "#include \"BKE_context.hh\"\n");
  fprintf(f, "#include \"BKE_lib_id.hh\"\n");
  fprintf(f, "#include \"BKE_main.hh\"\n");
  fprintf(f, "#include \"BKE_report.hh\"\n");

  fprintf(f, "#include \"RNA_define.hh\"\n");
  fprintf(f, "#include \"RNA_types.hh\"\n");
  fprintf(f, "#include \"rna_internal.hh\"\n\n");

  /* include the generated prototypes header */
  fprintf(f, "#include \"RNA_prototypes.hh\"\n\n");
  fprintf(f, "#include \"rna_prototypes_gen.hh\"\n\n");

  if (filename) {
    fprintf(f, "#include \"%s\"\n", filename);
  }
  if (api_filename) {
    fprintf(f, "#include \"%s\"\n", api_filename);
  }
  fprintf(f, "\n");

/* we want the included C files to have warnings enabled but for the generated code
 * ignore unused-parameter warnings which are hard to prevent */
#if defined(__GNUC__) || defined(__clang__)
  fprintf(f, "#pragma GCC diagnostic ignored \"-Wunused-parameter\"\n\n");
#endif

#if defined(__clang__)
  /* TODO(@ideasman42): ideally this workaround would not be needed,
   * could use some further investigation as these are intended to be declared. */
  fprintf(f, "#pragma GCC diagnostic ignored \"-Wmissing-variable-declarations\"\n\n");
#endif

  fprintf(f, "/* Auto-generated Functions. */\n\n");
  fprintf(f, "namespace blender {\n\n");

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (!filename || ds->filename == filename) {
      rna_generate_internal_property_prototypes(brna, ds->srna, f);
      rna_generate_function_prototypes(brna, ds->srna, f);
    }
  }

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (!filename || ds->filename == filename) {
      for (PropertyDefRNA &dp : ds->cont.properties) {
        rna_def_property_funcs(f, ds->srna, &dp);
      }
    }
  }

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (!filename || ds->filename == filename) {
      for (PropertyDefRNA &dp : ds->cont.properties) {
        rna_def_property_wrapper_funcs(f, ds, &dp);
      }

      for (dfunc = static_cast<FunctionDefRNA *>(ds->functions.first); dfunc;
           dfunc = static_cast<FunctionDefRNA *>(dfunc->cont.next))
      {
        rna_def_function_wrapper_funcs(f, ds, dfunc);
        rna_def_function_funcs(f, ds, dfunc);
      }

      rna_generate_static_function_prototypes(brna, ds->srna, f);
    }
  }

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (!filename || ds->filename == filename) {
      rna_generate_struct_register_func(brna, ds->srna, f);
    }
  }

  if (filename && STREQ(filename, "rna_ID.cc")) {
    rna_generate_blender(brna, f);
  }

  fprintf(f, "\n}  // namespace blender\n");
}

static void make_bad_file(const char *file, int line)
{
  FILE *fp = fopen(file, "w");
  fprintf(fp,
          "#error \"Error! cannot make correct RNA file from %s:%d, "
          "check DNA properties.\"\n",
          __FILE__,
          line);
  fclose(fp);
}

/**
 * \param extern_outfile: Directory to put public headers into. Can be nullptr, in which case
 *                        everything is put into \a outfile.
 */
static int rna_preprocess(const char *outfile, const char *public_header_outfile)
{
  BlenderRNA *brna;
  StructDefRNA *ds;
  FILE *file;
  char deffile[4096];
  int i;
  /* The exit code (returned from this function). */
  int status = EXIT_SUCCESS;
  const char *deps[3]; /* expand as needed */

  if (!public_header_outfile) {
    public_header_outfile = outfile;
  }

  /* define rna */
  brna = RNA_create();

  for (i = 0; PROCESS_ITEMS[i].filename; i++) {
    if (PROCESS_ITEMS[i].define) {
      PROCESS_ITEMS[i].define(brna);

      /* sanity check */
      if (!DefRNA.animate) {
        fprintf(stderr, "Error: DefRNA.animate left disabled in %s\n", PROCESS_ITEMS[i].filename);
      }

      for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
           ds = static_cast<StructDefRNA *>(ds->cont.next))
      {
        if (!ds->filename) {
          ds->filename = PROCESS_ITEMS[i].filename;
        }
      }
    }
  }

  rna_sanity_checks();
  if (DefRNA.error) {
    status = EXIT_FAILURE;
  }

  rna_auto_types();
  if (DefRNA.error) {
    status = EXIT_FAILURE;
  }

  /* Create external rna struct prototype header file RNA_prototypes.hh. */
  SNPRINTF(deffile, "%s%s", public_header_outfile, "RNA_prototypes.hh" TMP_EXT);
  if (status != EXIT_SUCCESS) {
    make_bad_file(deffile, __LINE__);
  }
  file = fopen(deffile, "w");
  if (!file) {
    fprintf(stderr, "Unable to open file: %s\n", deffile);
    status = EXIT_FAILURE;
  }
  else {
    fprintf(file,
            "/* Automatically generated RNA property declarations, to statically reference \n"
            " * properties as `rna_[struct-name]_[property-name]`.\n"
            " *\n"
            " * DO NOT EDIT MANUALLY, changes will be overwritten.\n"
            " */\n\n");

    fprintf(file, "#pragma once\n\n");
    fprintf(file, "namespace blender {\n\n");
    rna_generate_external_property_prototypes(brna, file);
    fprintf(file, "}  // namespace blender\n");
    fclose(file);
    if (DefRNA.error) {
      status = EXIT_FAILURE;
    }
    replace_if_different(deffile, nullptr);
  }

  /* create internal rna struct prototype header file */
  SNPRINTF(deffile, "%s%s", outfile, "rna_prototypes_gen.hh" TMP_EXT);
  if (status != EXIT_SUCCESS) {
    make_bad_file(deffile, __LINE__);
  }
  file = fopen(deffile, "w");
  if (!file) {
    fprintf(stderr, "Unable to open file: %s\n", deffile);
    status = EXIT_FAILURE;
  }
  else {
    fprintf(file,
            "/* Automatically generated function declarations for the Data API.\n"
            " * Do not edit manually, changes will be overwritten.              */\n\n");
    fprintf(file, "namespace blender {\n\n");
    rna_generate_struct_register_prototypes(brna, file);
    fprintf(file, "\n}  // namespace blender\n");
    fclose(file);
    replace_if_different(deffile, nullptr);
    if (DefRNA.error) {
      status = EXIT_FAILURE;
    }
  }

  /* Create `rna_gen_*.c` & `rna_gen_*.cc` files. */
  for (i = 0; PROCESS_ITEMS[i].filename; i++) {
    const bool is_cc = BLI_str_endswith(PROCESS_ITEMS[i].filename, ".cc");
    const int ext_len = is_cc ? 3 : 2;
    const int filename_len = strlen(PROCESS_ITEMS[i].filename);
    SNPRINTF(deffile,
             "%s%.*s%s" TMP_EXT,
             outfile,
             (filename_len - ext_len),
             PROCESS_ITEMS[i].filename,
             is_cc ? "_gen.cc" : "_gen.c");
    if (status != EXIT_SUCCESS) {
      make_bad_file(deffile, __LINE__);
    }
    else {
      file = fopen(deffile, "w");

      if (!file) {
        fprintf(stderr, "Unable to open file: %s\n", deffile);
        status = EXIT_FAILURE;
      }
      else {
        rna_generate(brna, file, PROCESS_ITEMS[i].filename, PROCESS_ITEMS[i].api_filename);
        fclose(file);
        if (DefRNA.error) {
          status = EXIT_FAILURE;
        }
      }
    }

    /* avoid unneeded rebuilds */
    deps[0] = PROCESS_ITEMS[i].filename;
    deps[1] = PROCESS_ITEMS[i].api_filename;
    deps[2] = nullptr;

    replace_if_different(deffile, deps);
  }

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

}  // namespace blender

int main(int argc, char **argv)
{
  using namespace blender;
  int return_status = EXIT_SUCCESS;

  MEM_init_memleak_detection();
  MEM_set_error_callback(mem_error_cb);

  CLG_init();

  /* Some useful defaults since this runs standalone. */
  CLG_output_use_basename_set(true);
  CLG_level_set(debugSRNA ? CLG_LEVEL_DEBUG : CLG_LEVEL_WARN);

  if (argc < 2) {
    fprintf(stderr, "Usage: %s outdirectory [public header outdirectory]/\n", argv[0]);
    return_status = EXIT_FAILURE;
  }
  else {
    if (debugSRNA > 0) {
      fprintf(stderr, "Running makesrna\n");
    }
    makesrna_path = argv[0];
    return_status = rna_preprocess(argv[1], (argc > 2) ? argv[2] : nullptr);
  }

  CLG_exit();

  return return_status;
}
