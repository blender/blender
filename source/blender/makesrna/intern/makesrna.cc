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

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_system.h" /* For #BLI_system_backtrace stub. */
#include "BLI_utildefines.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"

#include "rna_internal.hh"

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
  arr_new = MEM_malloc_arrayN<char>(size_t(len_new), "rna_cmp_file_new");
  arr_org = MEM_malloc_arrayN<char>(size_t(len_org), "rna_cmp_file_org");

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

  MEM_freeN(arr_new);
  MEM_freeN(arr_org);

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
  if (STREQ(propb->identifier, "rna_type")) {
    return 1;
  }

  if (STREQ(propa->identifier, "name")) {
    return -1;
  }
  if (STREQ(propb->identifier, "name")) {
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

  for (size = 0, link = static_cast<Link *>(listbase->first); link; link = link->next) {
    size++;
  }

  array = MEM_malloc_arrayN<void *>(size_t(size), "rna_sortlist");
  for (a = 0, link = static_cast<Link *>(listbase->first); link; link = link->next, a++) {
    array[a] = link;
  }

  qsort(array, size, sizeof(void *), cmp);

  listbase->first = listbase->last = nullptr;
  for (a = 0; a < size; a++) {
    link = static_cast<Link *>(array[a]);
    link->next = link->prev = nullptr;
    rna_addtail(listbase, link);
  }

  MEM_freeN(array);
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
  AllocDefRNA *alloc = MEM_callocN<AllocDefRNA>("AllocDefRNA");
  alloc->mem = MEM_mallocN(buffer_size, __func__);
  memcpy(alloc->mem, buffer, buffer_size);
  rna_addtail(&DefRNA.allocs, alloc);
  return alloc->mem;
}

void *rna_calloc(int buffer_size)
{
  AllocDefRNA *alloc = MEM_callocN<AllocDefRNA>("AllocDefRNA");
  alloc->mem = MEM_callocN(buffer_size, __func__);
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
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
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
      return rna_find_dna_type((const char *)pparm->type);
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
  return (func) ? (const char *)func : "nullptr";
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
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
      /* NOTE: ButType::NumSlider can't have a softmin of zero. */
      if ((fprop->ui_scale_type == PROP_SCALE_LOG) && (fprop->hardmin < 0 || fprop->softmin < 0)) {
        CLOG_ERROR(
            &LOG, "\"%s.%s\", range for log scale < 0.", srna->identifier, prop->identifier);
        DefRNA.error = true;
        return nullptr;
      }
    }
    if (prop->type == PROP_INT) {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
      /* Only ButType::NumSlider is implemented and that one can't have a softmin of zero. */
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
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
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
        PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
        rna_print_data_get(f, dp);
        if (dp->dnapointerlevel == 0) {
          fprintf(f,
                  "    return RNA_pointer_create_with_parent(*ptr, &RNA_%s, &data->%s);\n",
                  (const char *)pprop->type,
                  dp->dnaname);
        }
        else {
          fprintf(f,
                  "    return RNA_pointer_create_with_parent(*ptr, &RNA_%s, data->%s);\n",
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
        if (STR_ELEM(manualfunc,
                     "rna_iterator_listbase_get",
                     "rna_iterator_array_get",
                     "rna_iterator_array_dereference_get"))
        {
          fprintf(f,
                  "    return RNA_pointer_create_with_parent(iter->parent, &RNA_%s, %s(iter));\n",
                  (cprop->item_type) ? (const char *)cprop->item_type : "UnknownType",
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
            MEM_freeN(lenfunc);
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
    IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

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
    FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

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
          "blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)\n",
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
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
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
                  "    if (data->%s != nullptr) { MEM_freeN(data->%s); }\n",
                  dp->dnaname,
                  dp->dnaname);
          fprintf(f, "    const size_t length = strlen(value);\n");
          fprintf(f, "    if (length > 0) {\n");
          fprintf(f,
                  "        data->%s = MEM_malloc_arrayN<char>(length + 1, __func__);\n",
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
      fprintf(f, "void %s(PointerRNA *ptr, PointerRNA value, struct ReportList *reports)\n", func);
      fprintf(f, "{\n");
      if (manualfunc) {
        fprintf(f, "    PropPointerSetFunc fn = %s;\n", manualfunc);
        fprintf(f, "    fn(ptr, value, reports);\n");
      }
      else {
        rna_print_data_get(f, dp);

        PointerPropertyRNA *pprop = (PointerPropertyRNA *)dp->prop;
        StructRNA *type = (pprop->type) ? rna_find_struct((const char *)pprop->type) : nullptr;

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
              "\n    rna_array_lookup_int(ptr, &RNA_%s, data->%s, sizeof(data->%s[0]), data->%s, "
              "index);\n",
              item_type,
              dp->dnaname,
              dp->dnaname,
              dp->dnalengthname);
    }
    else {
      fprintf(
          f,
          "\n    rna_array_lookup_int(ptr, &RNA_%s, data->%s, sizeof(data->%s[0]), %d, index);\n",
          item_type,
          dp->dnaname,
          dp->dnaname,
          dp->dnalengthfixed);
    }
  }
  else {
    if (dp->dnapointerlevel == 0) {
      fprintf(f,
              "\n    return rna_listbase_lookup_int(ptr, &RNA_%s, &data->%s, index);\n",
              item_type,
              dp->dnaname);
    }
    else {
      fprintf(f,
              "\n    return rna_listbase_lookup_int(ptr, &RNA_%s, data->%s, index);\n",
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
  fprintf(f, "                name = MEM_malloc_arrayN<char>(size_t(namelen) + 1,\n");
  fprintf(f, "                                               \"name string\");\n");
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
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;

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

        bprop->get = reinterpret_cast<PropBooleanGetFunc>(
            rna_def_property_get_func(f, srna, prop, dp, (const char *)bprop->get));
        bprop->set = reinterpret_cast<PropBooleanSetFunc>(
            rna_def_property_set_func(f, srna, prop, dp, (const char *)bprop->set));
      }
      else {
        bprop->getarray = reinterpret_cast<PropBooleanArrayGetFunc>(
            rna_def_property_get_func(f, srna, prop, dp, (const char *)bprop->getarray));
        bprop->setarray = reinterpret_cast<PropBooleanArraySetFunc>(
            rna_def_property_set_func(f, srna, prop, dp, (const char *)bprop->setarray));
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

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

        iprop->get = reinterpret_cast<PropIntGetFunc>(
            rna_def_property_get_func(f, srna, prop, dp, (const char *)iprop->get));
        iprop->set = reinterpret_cast<PropIntSetFunc>(
            rna_def_property_set_func(f, srna, prop, dp, (const char *)iprop->set));
      }
      else {
        if (!iprop->getarray && !iprop->setarray) {
          rna_set_raw_property(dp, prop);
        }

        iprop->getarray = reinterpret_cast<PropIntArrayGetFunc>(
            rna_def_property_get_func(f, srna, prop, dp, (const char *)iprop->getarray));
        iprop->setarray = reinterpret_cast<PropIntArraySetFunc>(
            rna_def_property_set_func(f, srna, prop, dp, (const char *)iprop->setarray));
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

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

        fprop->get = reinterpret_cast<PropFloatGetFunc>(
            rna_def_property_get_func(f, srna, prop, dp, (const char *)fprop->get));
        fprop->set = reinterpret_cast<PropFloatSetFunc>(
            rna_def_property_set_func(f, srna, prop, dp, (const char *)fprop->set));
      }
      else {
        if (!fprop->getarray && !fprop->setarray) {
          rna_set_raw_property(dp, prop);
        }

        fprop->getarray = reinterpret_cast<PropFloatArrayGetFunc>(
            rna_def_property_get_func(f, srna, prop, dp, (const char *)fprop->getarray));
        fprop->setarray = reinterpret_cast<PropFloatArraySetFunc>(
            rna_def_property_set_func(f, srna, prop, dp, (const char *)fprop->setarray));
      }
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;

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

      eprop->get = reinterpret_cast<PropEnumGetFunc>(
          rna_def_property_get_func(f, srna, prop, dp, (const char *)eprop->get));
      eprop->set = reinterpret_cast<PropEnumSetFunc>(
          rna_def_property_set_func(f, srna, prop, dp, (const char *)eprop->set));
      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;

      if (!(prop->flag & PROP_EDITABLE) && (sprop->set || sprop->set_ex || sprop->set_transform)) {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      sprop->get = reinterpret_cast<PropStringGetFunc>(
          rna_def_property_get_func(f, srna, prop, dp, (const char *)sprop->get));
      sprop->length = reinterpret_cast<PropStringLengthFunc>(
          rna_def_property_length_func(f, srna, prop, dp, (const char *)sprop->length));
      sprop->set = reinterpret_cast<PropStringSetFunc>(
          rna_def_property_set_func(f, srna, prop, dp, (const char *)sprop->set));
      sprop->search = reinterpret_cast<StringPropertySearchFunc>(
          rna_def_property_search_func(f, srna, prop, dp, (const char *)sprop->search));
      break;
    }
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

      if (!(prop->flag & PROP_EDITABLE) && pprop->set) {
        CLOG_ERROR(&LOG,
                   "%s.%s, is read-only but has defines a \"set\" callback.",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
      }

      pprop->get = reinterpret_cast<PropPointerGetFunc>(
          rna_def_property_get_func(f, srna, prop, dp, (const char *)pprop->get));
      pprop->set = reinterpret_cast<PropPointerSetFunc>(
          rna_def_property_set_func(f, srna, prop, dp, (const char *)pprop->set));
      if (!pprop->type) {
        CLOG_ERROR(
            &LOG, "%s.%s, pointer must have a struct type.", srna->identifier, prop->identifier);
        DefRNA.error = true;
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
      const char *nextfunc = (const char *)cprop->next;
      const char *item_type = (const char *)cprop->item_type;

      if (cprop->length) {
        /* always generate if we have a manual implementation */
        cprop->length = reinterpret_cast<PropCollectionLengthFunc>(
            rna_def_property_length_func(f, srna, prop, dp, (const char *)cprop->length));
      }
      else if (dp->dnatype && STREQ(dp->dnatype, "ListBase")) {
        /* pass */
      }
      else if (dp->dnalengthname || dp->dnalengthfixed) {
        cprop->length = reinterpret_cast<PropCollectionLengthFunc>(
            rna_def_property_length_func(f, srna, prop, dp, (const char *)cprop->length));
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

      cprop->get = reinterpret_cast<PropCollectionGetFunc>(
          rna_def_property_get_func(f, srna, prop, dp, (const char *)cprop->get));
      cprop->begin = reinterpret_cast<PropCollectionBeginFunc>(
          rna_def_property_begin_func(f, srna, prop, dp, (const char *)cprop->begin));
      cprop->next = reinterpret_cast<PropCollectionNextFunc>(
          rna_def_property_next_func(f, srna, prop, dp, (const char *)cprop->next));
      cprop->end = reinterpret_cast<PropCollectionEndFunc>(
          rna_def_property_end_func(f, srna, prop, dp, (const char *)cprop->end));
      cprop->lookupint = reinterpret_cast<PropCollectionLookupIntFunc>(
          rna_def_property_lookup_int_func(
              f, srna, prop, dp, (const char *)cprop->lookupint, nextfunc));
      cprop->lookupstring = reinterpret_cast<PropCollectionLookupStringFunc>(
          rna_def_property_lookup_string_func(
              f, srna, prop, dp, (const char *)cprop->lookupstring, item_type));

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
      else if ((prop->flag & PROP_DYNAMIC) == 0 && prop->arraydimension && prop->totarraylength) {
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
      else if ((prop->flag & PROP_DYNAMIC) == 0 && prop->arraydimension && prop->totarraylength) {
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
      else if ((prop->flag & PROP_DYNAMIC) == 0 && prop->arraydimension && prop->totarraylength) {
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
      // fprintf(f, "void %sset(PointerRNA *ptr, PointerRNA value);\n", func);
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
        fprintf(f, "bool %slookup_int(PointerRNA *ptr, int key, PointerRNA *r_ptr);\n", func);
      }
      if (cprop->lookupstring) {
        fprintf(f,
                "bool %slookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr);\n",
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

/* Disabled for now to avoid MSVC compiler error due to large file size. */
#if 0
  if (prop->name && prop->description && prop->description[0] != '\0') {
    fprintf(f, "\t/* %s: %s */\n", prop->name, prop->description);
  }
  else if (prop->name) {
    fprintf(f, "\t/* %s */\n", prop->name);
  }
  else {
    fprintf(f, "\t/* */\n");
  }
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
          cprop->property.srna)
      {
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
    /* For the C++ API we need to use RNA structures names for pointers. */
    PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

    return (const char *)pprop->type;
  }
  return rna_parameter_type_name(prop);
}

static void rna_def_struct_function_prototype_cpp(FILE *f,
                                                  StructRNA * /*srna*/,
                                                  FunctionDefRNA *dfunc,
                                                  const char *cpp_namespace,
                                                  int close_prototype)
{
  FunctionRNA *func = dfunc->func;

  int first = 1;
  const char *retval_type = "void";

  if (func->c_ret) {
    PropertyDefRNA *dp = rna_find_parameter_def(func->c_ret);
    retval_type = rna_parameter_type_cpp_name(dp->prop);
  }

  if (cpp_namespace && cpp_namespace[0]) {
    fprintf(f, "\tinline %s %s::%s(", retval_type, cpp_namespace, rna_safe_id(func->identifier));
  }
  else {
    fprintf(f, "\tinline %s %s(", retval_type, rna_safe_id(func->identifier));
  }

  if (func->flag & FUNC_USE_MAIN) {
    WRITE_PARAM("void *main");
  }

  if (func->flag & FUNC_USE_CONTEXT) {
    WRITE_PARAM("Context C");
  }

  LISTBASE_FOREACH (PropertyDefRNA *, dp, &dfunc->cont.properties) {
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
      if (type == PROP_STRING) {
        ptrstr = pout ? "*" : "";
      }
      else {
        ptrstr = pout ? "**" : "*";
      }
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
/* Disabled for now to avoid MSVC compiler error due to large file size. */
#if 0
    FunctionRNA *func = dfunc->func;
    fprintf(f, "\n\t/* %s */\n", func->description);
#endif

    rna_def_struct_function_prototype_cpp(f, srna, dfunc, nullptr, 1);
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

      if (cprop->type) {
        fprintf(f,
                "\tCOLLECTION_PROPERTY(%s, %s, %s, %s, %s, %s)",
                (const char *)cprop->type,
                srna->identifier,
                prop->identifier,
                (cprop->length ? "true" : "false"),
                (cprop->lookupint ? "true" : "false"),
                (cprop->lookupstring ? "true" : "false"));
      }
      else {
        fprintf(f,
                "\tCOLLECTION_PROPERTY(%s, %s, %s, %s, %s, %s)",
                "UnknownType",
                srna->identifier,
                prop->identifier,
                (cprop->length ? "true" : "false"),
                (cprop->lookupint ? "true" : "false"),
                (cprop->lookupstring ? "true" : "false"));
      }
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

  if (func->flag & FUNC_USE_SELF_ID) {
    WRITE_PARAM("(::ID *) ptr.owner_id");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    WRITE_COMMA;
    if ((func->flag & FUNC_SELF_AS_RNA) != 0) {
      fprintf(f, "this->ptr");
    }
    else if (dsrna->dnafromprop) {
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

  if (func->flag & FUNC_USE_MAIN) {
    WRITE_PARAM("(::Main *) main");
  }

  if (func->flag & FUNC_USE_CONTEXT) {
    WRITE_PARAM("(::bContext *) C.ptr.data");
  }

  if (func->flag & FUNC_USE_REPORTS) {
    WRITE_PARAM("nullptr");
  }

  dp = static_cast<PropertyDefRNA *>(dfunc->cont.properties.first);
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
      else if (dp->prop->flag_parameter & PARM_RNAPTR) {
        fprintf(f,
                "(::%s *) &%s",
                rna_parameter_type_name(dp->prop),
                rna_safe_id(dp->prop->identifier));
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
          fprintf(f, "\t\tresult = RNA_id_pointer_create((::ID *) retdata);\n");
        }
        else {
          fprintf(f,
                  "\t\tresult = RNA_pointer_create_with_parent(ptr, &RNA_%s, retdata);\n",
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
    fprintf(f, "\tstruct ID *_selfid;\n");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if ((func->flag & FUNC_SELF_AS_RNA) != 0) {
      fprintf(f, "\tstruct PointerRNA _self;\n");
    }
    else if (dsrna->dnafromprop) {
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
            "\t%s%s%s %s%s;\n",
            rna_parameter_is_const(dparm) ? "const " : "",
            rna_type_struct(dparm->prop),
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
    fprintf(f, "\t_selfid = (struct ID *)_ptr->owner_id;\n");
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if ((func->flag & FUNC_SELF_AS_RNA) != 0) {
      fprintf(f, "\t_self = *_ptr;\n");
    }
    else if (dsrna->dnafromprop) {
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
              "((%s%s%s %s)%s);\n",
              rna_parameter_is_const(dparm) ? "const " : "",
              rna_type_struct(dparm->prop),
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
                  "\t*((%s%s %s*)_retdata) = %s;\n",
                  rna_type_struct(dparm->prop),
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

    LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
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

          /* Only automatically define `PROP_ID_REFCOUNT` if it was not already explicitly set or
           * cleared by calls to `RNA_def_property_flag` or `RNA_def_property_clear_flag`. */
          if ((pprop->property.flag_internal & PROP_INTERN_PTR_ID_REFCOUNT_FORCED) == 0 &&
              pprop->type)
          {
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

  for (srna = static_cast<StructRNA *>(brna->structs.first); srna;
       srna = static_cast<StructRNA *>(srna->cont.next))
  {
    rna_sortlist(&srna->cont.properties, cmp_property);
  }

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
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
  StructRNA *srna;

  for (srna = static_cast<StructRNA *>(brna->structs.first); srna;
       srna = static_cast<StructRNA *>(srna->cont.next))
  {
    fprintf(f, "extern struct StructRNA RNA_%s;\n", srna->identifier);
  }
  fprintf(f, "\n");
}

static void rna_generate_blender(BlenderRNA *brna, FILE *f)
{
  StructRNA *srna;

  fprintf(f,
          "BlenderRNA BLENDER_RNA = {\n"
          "\t/*structs*/ {");
  srna = static_cast<StructRNA *>(brna->structs.first);
  if (srna) {
    fprintf(f, "&RNA_%s, ", srna->identifier);
  }
  else {
    fprintf(f, "nullptr, ");
  }

  srna = static_cast<StructRNA *>(brna->structs.last);
  if (srna) {
    fprintf(f, "&RNA_%s},\n", srna->identifier);
  }
  else {
    fprintf(f, "nullptr},\n");
  }

  fprintf(f,
          "\t/*structs_map*/ nullptr,\n"
          "\t/*structs_len*/ 0,\n"
          "};\n\n");
}

static void rna_generate_external_property_prototypes(BlenderRNA *brna, FILE *f)
{
  fprintf(f, "struct PropertyRNA;\n\n");

  rna_generate_struct_rna_prototypes(brna, f);

  /* NOTE: Generate generic `PropertyRNA &` references. The actual, type-refined properties data
   * are static variables in their translation units (the `_gen.cc` files), which are assigned to
   * these public generic `PointerRNA &` references. */
  for (StructRNA *srna = static_cast<StructRNA *>(brna->structs.first); srna;
       srna = static_cast<StructRNA *>(srna->cont.next))
  {
    LISTBASE_FOREACH (PropertyRNA *, prop, &srna->cont.properties) {
      fprintf(f, "extern PropertyRNA &rna_%s_%s;\n", srna->identifier, prop->identifier);
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
    LISTBASE_FOREACH (PropertyRNA *, prop, &base->cont.properties) {
      fprintf(f, "extern PropertyRNA &rna_%s_%s;\n", base->identifier, prop->identifier);
    }
    base = base->base;
  }

  if (srna->cont.properties.first) {
    fprintf(f, "\n");
  }

  LISTBASE_FOREACH (PropertyRNA *, prop, &srna->cont.properties) {
    fprintf(f, "extern PropertyRNA &rna_%s_%s;\n", srna->identifier, prop->identifier);
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
  LISTBASE_FOREACH (PropertyRNA *, parm, &func->cont.properties) {
    fprintf(f,
            "extern PropertyRNA &rna_%s_%s_%s;\n",
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
    for (func = static_cast<FunctionRNA *>(base->functions.first); func;
         func = static_cast<FunctionRNA *>(func->cont.next))
    {
      fprintf(f, "extern FunctionRNA rna_%s_%s_func;\n", base->identifier, func->identifier);
      rna_generate_parameter_prototypes(brna, base, func, f);
    }

    if (base->functions.first) {
      fprintf(f, "\n");
    }

    base = base->base;
  }

  for (func = static_cast<FunctionRNA *>(srna->functions.first); func;
       func = static_cast<FunctionRNA *>(func->cont.next))
  {
    fprintf(f, "extern FunctionRNA rna_%s_%s_func;\n", srna->identifier, func->identifier);
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
  PropertyDefRNA *dparm_return = nullptr;
  StructDefRNA *dsrna;
  PropertyType type;
  int flag, flag_parameter, pout, cptr, first;
  const char *ptrstr;

  dsrna = rna_find_struct_def(srna);
  func = dfunc->func;

  /* return type */
  LISTBASE_FOREACH (PropertyDefRNA *, dparm, &dfunc->cont.properties) {
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

      dparm_return = dparm;
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
    fprintf(f, "struct ID *_selfid");
    first = 0;
  }

  if ((func->flag & FUNC_NO_SELF) == 0) {
    if (!first) {
      fprintf(f, ", ");
    }
    if ((func->flag & FUNC_SELF_AS_RNA) != 0) {
      fprintf(f, "struct PointerRNA _self");
    }
    else if (dsrna->dnafromprop) {
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
  LISTBASE_FOREACH (PropertyDefRNA *, dparm, &dfunc->cont.properties) {
    type = dparm->prop->type;
    flag = dparm->prop->flag;
    flag_parameter = dparm->prop->flag_parameter;
    pout = (flag_parameter & PARM_OUTPUT);
    cptr = ((type == PROP_POINTER) && !(flag_parameter & PARM_RNAPTR));

    if (dparm->prop == func->c_ret) {
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
      fprintf(f, "int %s%s_num, ", pout ? "*" : "", dparm->prop->identifier);
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

static void rna_generate_static_function_prototypes(BlenderRNA * /*brna*/,
                                                    StructRNA *srna,
                                                    FILE *f)
{
  FunctionRNA *func;
  FunctionDefRNA *dfunc;
  int first = 1;

  for (func = static_cast<FunctionRNA *>(srna->functions.first); func;
       func = static_cast<FunctionRNA *>(func->cont.next))
  {
    dfunc = rna_find_function_def(func);

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

static void rna_generate_struct_prototypes(FILE *f)
{
  StructDefRNA *ds;
  FunctionDefRNA *dfunc;
  const char *structures[2048];
  int all_structures = 0;

  /* structures definitions */
  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    for (dfunc = static_cast<FunctionDefRNA *>(ds->functions.first); dfunc;
         dfunc = static_cast<FunctionDefRNA *>(dfunc->cont.next))
    {
      if (dfunc->call) {
        LISTBASE_FOREACH (PropertyDefRNA *, dp, &dfunc->cont.properties) {
          if (dp->prop->type == PROP_POINTER) {
            int a, found = 0;
            const char *struct_name = rna_parameter_type_name(dp->prop);
            if (struct_name == nullptr) {
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

              if (all_structures >= ARRAY_SIZE(structures)) {
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
  bool freenest = false;

  if (nest != nullptr) {
    size_t len = strlen(nest);

    strnest = MEM_malloc_arrayN<char>(len + 2, "rna_generate_property -> strnest");
    errnest = MEM_malloc_arrayN<char>(len + 2, "rna_generate_property -> errnest");

    strnest[0] = '_';
    memcpy(strnest + 1, nest, len + 1);

    errnest[0] = '.';
    memcpy(errnest + 1, nest, len + 1);

    freenest = true;
  }

  if (prop->deprecated) {
    fprintf(f,
            "static const DeprecatedRNA rna_%s%s_%s_deprecated = {\n\t",
            srna->identifier,
            strnest,
            prop->identifier);
    rna_print_c_string(f, prop->deprecated->note);
    fprintf(f, ",\n\t%d, %d,\n", prop->deprecated->version, prop->deprecated->removal_version);
    fprintf(f, "};\n\n");
  }

  switch (prop->type) {
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      int i, defaultfound = 0, totflag = 0;

      if (eprop->item) {
        /* Inline the enum if this is not a defined in "RNA_enum_items.hh". */
        const char *item_global_id = rna_enum_id_from_pointer(eprop->item);
        if (item_global_id == nullptr) {
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

          fprintf(f, "{0, nullptr, 0, nullptr, nullptr}\n};\n\n");
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
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
      uint i;

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
      uint i;

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
      uint i;

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
      if (type && (type->flag & STRUCT_ID) &&
          !(prop->flag_internal & PROP_INTERN_PTR_OWNERSHIP_FORCED))
      {
        RNA_def_property_flag(prop, PROP_PTR_NO_OWNERSHIP);
      }
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

      /* XXX This systematically enforces that flag on ID pointers...
       * we'll probably have to revisit. :/ */
      StructRNA *type = rna_find_struct((const char *)cprop->item_type);
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

  /* Generate the RNA-private, type-refined property data.
   *
   * See #rna_generate_external_property_prototypes comments for details. */
  fprintf(f,
          "static %s rna_%s%s_%s_ = {\n",
          rna_property_structname(prop->type),
          srna->identifier,
          strnest,
          prop->identifier);

  if (prop->next) {
    fprintf(f, "\t{&rna_%s%s_%s, ", srna->identifier, strnest, prop->next->identifier);
  }
  else {
    fprintf(f, "\t{nullptr, ");
  }
  if (prop->prev) {
    fprintf(f, "&rna_%s%s_%s,\n", srna->identifier, strnest, prop->prev->identifier);
  }
  else {
    fprintf(f, "nullptr,\n");
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
  fprintf(f, "PropertyPathTemplateType(%d), ", prop->path_template_type);
  rna_print_c_string(f, prop->name);
  fprintf(f, ",\n\t");
  rna_print_c_string(f, prop->description);
  fprintf(f, ",\n\t");
  fprintf(f, "%d, ", prop->icon);
  rna_print_c_string(f, prop->translation_context);
  fprintf(f, ",\n\t");

  if (prop->deprecated) {
    fprintf(f, "&rna_%s%s_%s_deprecated,", srna->identifier, strnest, prop->identifier);
  }
  else {
    fprintf(f, "nullptr,\n");
  }

  fprintf(f,
          "\t%s, PropertySubType(int(%s) | int(%s)), %s, %u, {%u, %u, %u}, %u,\n",
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
          "\t%s%s, %d, %s, %s, %s, %s, %s, %s,\n",
          /* NOTE: void cast is needed to quiet function cast warning in C++. */
          (prop->flag & PROP_CONTEXT_UPDATE) ? "(UpdateFunc)(void *)" : "",
          rna_function_string(prop->update),
          prop->noteflag,
          rna_function_string(prop->editable),
          rna_function_string(prop->itemeditable),
          rna_function_string(prop->ui_name_func),
          rna_function_string(prop->override_diff),
          rna_function_string(prop->override_store),
          rna_function_string(prop->override_apply));

  if (prop->flag_internal & PROP_INTERN_RAW_ACCESS) {
    rna_set_raw_offset(f, srna, prop);
  }
  else {
    fprintf(f, "\t0, PROP_RAW_UNSET");
  }

  /* our own type - collections/arrays only */
  if (prop->srna) {
    fprintf(f, ", &RNA_%s", (const char *)prop->srna);
  }
  else {
    fprintf(f, ", nullptr");
  }

  fprintf(f, "},\n");

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %d, ",
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
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s,\n\t",
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
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ",
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
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, %s, eStringPropertySearchFlag(%d), %s, %d, ",
              rna_function_string(sprop->get),
              rna_function_string(sprop->length),
              rna_function_string(sprop->set),
              rna_function_string(sprop->get_ex),
              rna_function_string(sprop->length_ex),
              rna_function_string(sprop->set_ex),
              rna_function_string(sprop->get_transform),
              rna_function_string(sprop->set_transform),
              rna_function_string(sprop->search),
              int(sprop->search_flag),
              rna_function_string(sprop->path_filter),
              sprop->maxlength);
      rna_print_c_string(f, sprop->defaultvalue);
      fprintf(f, "\n");
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s, %s, %s, %s, %s, ",
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
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
      fprintf(f,
              "\t%s, %s, %s, %s,",
              rna_function_string(pprop->get),
              rna_function_string(pprop->set),
              rna_function_string(pprop->type_fn),
              rna_function_string(pprop->poll));
      if (pprop->type) {
        fprintf(f, "&RNA_%s\n", (const char *)pprop->type);
      }
      else {
        fprintf(f, "nullptr\n");
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
        fprintf(f, "nullptr\n");
      }
      break;
    }
  }

  fprintf(f, "};\n");

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
    MEM_freeN(strnest);
    MEM_freeN(errnest);
  }
}

static void rna_generate_struct(BlenderRNA * /*brna*/, StructRNA *srna, FILE *f)
{
  FunctionRNA *func;
  FunctionDefRNA *dfunc;
  PropertyRNA *prop, *parm;
  StructRNA *base;

  fprintf(f, "/* %s */\n", srna->name);

  LISTBASE_FOREACH (PropertyRNA *, prop, &srna->cont.properties) {
    rna_generate_property(f, srna, nullptr, prop);
  }

  for (func = static_cast<FunctionRNA *>(srna->functions.first); func;
       func = static_cast<FunctionRNA *>(func->cont.next))
  {
    LISTBASE_FOREACH (PropertyRNA *, parm, &func->cont.properties) {
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
      fprintf(f, "\t{nullptr, ");
    }
    if (func->cont.prev) {
      fprintf(f,
              "(FunctionRNA *)&rna_%s_%s_func,\n",
              srna->identifier,
              ((FunctionRNA *)func->cont.prev)->identifier);
    }
    else {
      fprintf(f, "nullptr,\n");
    }

    fprintf(f, "\tnullptr,\n");

    parm = static_cast<PropertyRNA *>(func->cont.properties.first);
    if (parm) {
      fprintf(f, "\t{&rna_%s_%s_%s, ", srna->identifier, func->identifier, parm->identifier);
    }
    else {
      fprintf(f, "\t{nullptr, ");
    }

    parm = static_cast<PropertyRNA *>(func->cont.properties.last);
    if (parm) {
      fprintf(f, "&rna_%s_%s_%s}},\n", srna->identifier, func->identifier, parm->identifier);
    }
    else {
      fprintf(f, "nullptr}},\n");
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
      fprintf(f, "\tnullptr,\n");
    }

    if (func->c_ret) {
      fprintf(f, "\t&rna_%s_%s_%s\n", srna->identifier, func->identifier, func->c_ret->identifier);
    }
    else {
      fprintf(f, "\tnullptr\n");
    }

    fprintf(f, "};\n");
    fprintf(f, "\n");
  }

  fprintf(f, "StructRNA RNA_%s = {\n", srna->identifier);

  if (srna->cont.next) {
    fprintf(f, "\t{(ContainerRNA *)&RNA_%s, ", ((StructRNA *)srna->cont.next)->identifier);
  }
  else {
    fprintf(f, "\t{nullptr, ");
  }
  if (srna->cont.prev) {
    fprintf(f, "(ContainerRNA *)&RNA_%s,\n", ((StructRNA *)srna->cont.prev)->identifier);
  }
  else {
    fprintf(f, "nullptr,\n");
  }

  fprintf(f, "\tnullptr,\n");

  prop = static_cast<PropertyRNA *>(srna->cont.properties.first);
  if (prop) {
    fprintf(f, "\t{&rna_%s_%s, ", srna->identifier, prop->identifier);
  }
  else {
    fprintf(f, "\t{nullptr, ");
  }

  prop = static_cast<PropertyRNA *>(srna->cont.properties.last);
  if (prop) {
    fprintf(f, "&rna_%s_%s}},\n", srna->identifier, prop->identifier);
  }
  else {
    fprintf(f, "nullptr}},\n");
  }
  fprintf(f, "\t");
  rna_print_c_string(f, srna->identifier);
  fprintf(f, ", nullptr, nullptr"); /* PyType - Can't initialize here */
  fprintf(f, ", %d, nullptr, ", srna->flag);
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

    fprintf(f, "\t&rna_%s_%s, ", base->identifier, prop->identifier);
  }
  else {
    fprintf(f, "\tnullptr, ");
  }

  prop = srna->iteratorproperty;
  base = srna;
  while (base->base && base->base->iteratorproperty == prop) {
    base = base->base;
  }
  fprintf(f, "&rna_%s_rna_properties,\n", base->identifier);

  if (srna->base) {
    fprintf(f, "\t&RNA_%s,\n", srna->base->identifier);
  }
  else {
    fprintf(f, "\tnullptr,\n");
  }

  if (srna->nested) {
    fprintf(f, "\t&RNA_%s,\n", srna->nested->identifier);
  }
  else {
    fprintf(f, "\tnullptr,\n");
  }

  fprintf(f, "\t%s,\n", rna_function_string(srna->refine));
  fprintf(f, "\t%s,\n", rna_function_string(srna->path));
  fprintf(f, "\t%s,\n", rna_function_string(srna->reg));
  fprintf(f, "\t%s,\n", rna_function_string(srna->unreg));
  fprintf(f, "\t%s,\n", rna_function_string(srna->instance));
  fprintf(f, "\t%s,\n", rna_function_string(srna->idproperties));
  fprintf(f, "\t%s,\n", rna_function_string(srna->system_idproperties));

  if (srna->reg && !srna->refine) {
    CLOG_ERROR(
        &LOG, "%s has a register function, must also have refine function.", srna->identifier);
    DefRNA.error = true;
  }

  func = static_cast<FunctionRNA *>(srna->functions.first);
  if (func) {
    fprintf(f, "\t{(FunctionRNA *)&rna_%s_%s_func, ", srna->identifier, func->identifier);
  }
  else {
    fprintf(f, "\t{nullptr, ");
  }

  func = static_cast<FunctionRNA *>(srna->functions.last);
  if (func) {
    fprintf(f, "(FunctionRNA *)&rna_%s_%s_func}\n", srna->identifier, func->identifier);
  }
  else {
    fprintf(f, "nullptr}\n");
  }

  fprintf(f, "};\n");

  fprintf(f, "\n");
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
          " * Do not edit manually, changes will be overwritten.           */\n\n"
          "#define RNA_RUNTIME\n\n");

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
      LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
        rna_def_property_funcs(f, ds->srna, dp);
      }
    }
  }

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    if (!filename || ds->filename == filename) {
      LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
        rna_def_property_wrapper_funcs(f, ds, dp);
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
      rna_generate_struct(brna, ds->srna, f);
    }
  }

  if (filename && STREQ(filename, "rna_ID.cc")) {
    /* this is ugly, but we cannot have c files compiled for both
     * makesrna and blender with some build systems at the moment */
    fprintf(f, "#include \"rna_define.cc\"\n\n");

    rna_generate_blender(brna, f);
  }
}

static void rna_generate_header(BlenderRNA * /*brna*/, FILE *f)
{
  StructDefRNA *ds;
  StructRNA *srna;
  FunctionDefRNA *dfunc;

  fprintf(f, "\n#ifndef __RNA_BLENDER_H__\n");
  fprintf(f, "#define __RNA_BLENDER_H__\n\n");

  fprintf(f,
          "/* Automatically generated function declarations for the Data API.\n"
          " * Do not edit manually, changes will be overwritten.              */\n\n");

  fprintf(f, "#include \"RNA_types.hh\"\n\n");
  fprintf(f, "#include \"DNA_node_types.h\"\n\n");

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

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    srna = ds->srna;

    fprintf(f, "/**************** %s ****************/\n\n", srna->name);

    while (srna) {
      fprintf(f, "extern StructRNA RNA_%s;\n", srna->identifier);
      srna = srna->base;
    }
    fprintf(f, "\n");

    LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
      rna_def_property_funcs_header(f, ds->srna, dp);
    }

    for (dfunc = static_cast<FunctionDefRNA *>(ds->functions.first); dfunc;
         dfunc = static_cast<FunctionDefRNA *>(dfunc->cont.next))
    {
      rna_def_function_funcs_header(f, ds->srna, dfunc);
    }
  }

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
    "#define COLLECTION_PROPERTY_EMPTY_false(sname, identifier) \\\n"
    "    inline static bool sname##_##identifier##_empty_wrap(PointerRNA *ptr) \\\n"
    "    { \\\n"
    "        CollectionPropertyIterator iter; \\\n"
    "        sname##_##identifier##_begin(&iter, ptr); \\\n"
    "        bool empty = !iter.valid; \\\n"
    "        sname##_##identifier##_end(&iter); \\\n"
    "        return empty; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_EMPTY_true(sname, identifier) \\\n"
    "    inline static bool sname##_##identifier##_empty_wrap(PointerRNA *ptr) \\\n"
    "    { return sname##_##identifier##_length(ptr) == 0; } \n"
    "\n"
    "#define COLLECTION_PROPERTY_LOOKUP_INT_false(sname, identifier) \\\n"
    "    inline static bool sname##_##identifier##_lookup_int_wrap(PointerRNA *ptr, int key, "
    "PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        CollectionPropertyIterator iter; \\\n"
    "        int i = 0; \\\n"
    "        bool found = false; \\\n"
    "        sname##_##identifier##_begin(&iter, ptr); \\\n"
    "        while (iter.valid) { \\\n"
    "            if (i == key) { \\\n"
    "                *r_ptr = iter.ptr; \\\n"
    "                found = true; \\\n"
    "                break; \\\n"
    "            } \\\n"
    "            sname##_##identifier##_next(&iter); \\\n"
    "            ++i; \\\n"
    "        } \\\n"
    "        sname##_##identifier##_end(&iter); \\\n"
    "        if (!found) { \\\n"
    "            *r_ptr = {}; \\\n"
    "        } \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_LOOKUP_INT_true(sname, identifier) \\\n"
    "    inline static bool sname##_##identifier##_lookup_int_wrap(PointerRNA *ptr, int key, "
    "PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        bool found = sname##_##identifier##_lookup_int(ptr, key, r_ptr); \\\n"
    "        if (!found) { \\\n"
    "            *r_ptr = {}; \\\n"
    "        } \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_LOOKUP_STRING_false(sname, identifier) \\\n"
    "    inline static bool sname##_##identifier##_lookup_string_wrap(PointerRNA *ptr, const char "
    "*key, PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        CollectionPropertyIterator iter; \\\n"
    "        bool found = false; \\\n"
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
    "                found = true; \\\n"
    "            } \\\n"
    "            if (name_fixed != name) { \\\n"
    "                MEM_freeN( name); \\\n"
    "            } \\\n"
    "            sname##_##identifier##_next(&iter); \\\n"
    "        } \\\n"
    "        sname##_##identifier##_end(&iter); \\\n"
    "        if (!found) { \\\n"
    "            *r_ptr = {}; \\\n"
    "        } \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY_LOOKUP_STRING_true(sname, identifier) \\\n"
    "    inline static bool sname##_##identifier##_lookup_string_wrap(PointerRNA *ptr, const char "
    "*key, PointerRNA *r_ptr) \\\n"
    "    { \\\n"
    "        bool found = sname##_##identifier##_lookup_string(ptr, key, r_ptr); \\\n"
    "        if (!found) { \\\n"
    "            *r_ptr = {}; \\\n"
    "        } \\\n"
    "        return found; \\\n"
    "    } \n"
    "#define COLLECTION_PROPERTY(collection_funcs, type, sname, identifier, has_length, "
    "has_lookup_int, has_lookup_string) \\\n"
    "    typedef CollectionIterator<type, sname##_##identifier##_begin, \\\n"
    "        sname##_##identifier##_next, sname##_##identifier##_end> identifier##_iterator; \\\n"
    "    COLLECTION_PROPERTY_LENGTH_##has_length(sname, identifier) \\\n"
    "    COLLECTION_PROPERTY_EMPTY_##has_length(sname, identifier) \\\n"
    "    COLLECTION_PROPERTY_LOOKUP_INT_##has_lookup_int(sname, identifier) \\\n"
    "    COLLECTION_PROPERTY_LOOKUP_STRING_##has_lookup_string(sname, identifier) \\\n"
    "    CollectionRef<sname, type, sname##_##identifier##_begin, \\\n"
    "        sname##_##identifier##_next, sname##_##identifier##_end, \\\n"
    "        sname##_##identifier##_length_wrap, \\\n"
    "        sname##_##identifier##_empty_wrap, \\\n"
    "        sname##_##identifier##_lookup_int_wrap, sname##_##identifier##_lookup_string_wrap, "
    "collection_funcs> identifier;\n"
    "\n"
    "class Pointer {\n"
    "public:\n"
    "    Pointer(const PointerRNA &p) : ptr(p) { }\n"
    "    operator const PointerRNA&() { return ptr; }\n"
    "    bool is_a(StructRNA *type) { return RNA_struct_is_a(ptr.type, type) ? true: false; }\n"
    "    operator void*() { return ptr.data; }\n"
    "    operator bool() const { return ptr.data != nullptr; }\n"
    "\n"
    "    bool operator==(const Pointer &other) const { return ptr.data == other.ptr.data; }\n"
    "    bool operator!=(const Pointer &other) const { return ptr.data != other.ptr.data; }\n"
    "    bool operator<(const Pointer &other) const { return ptr.data < other.ptr.data; }\n"
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
    "    DynamicArray() : data(nullptr), length(0) {}\n"
    "    DynamicArray(int new_length) : data(nullptr), length(new_length) { data = (T "
    "*)malloc(sizeof(T) * new_length); }\n"
    "    DynamicArray(const DynamicArray<T>& other) : data(nullptr), length(0) { "
    "copy_from(other); "
    "}\n"
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
    "typedef bool (*TEmptyFunc)(PointerRNA *ptr);\n"
    "typedef bool (*TLookupIntFunc)(PointerRNA *ptr, int key, PointerRNA *r_ptr);\n"
    "typedef bool (*TLookupStringFunc)(PointerRNA *ptr, const char *key, PointerRNA *r_ptr);\n"
    "\n"
    "template<typename T, TBeginFunc Tbegin, TNextFunc Tnext, TEndFunc Tend>\n"
    "class CollectionIterator {\n"
    "public:\n"
    "    CollectionIterator() : iter(), t(iter.ptr), init(false) { iter.valid = false; }\n"
    "    CollectionIterator(const PointerRNA &ptr) : CollectionIterator() { this->begin(ptr); }\n"
    "    ~CollectionIterator(void) { if (init) Tend(&iter); };\n"
    "\n"
    "    CollectionIterator(const CollectionIterator &other) = delete;\n"
    "    CollectionIterator(CollectionIterator &&other) = delete;\n"
    "    CollectionIterator &operator=(const CollectionIterator &other) = delete;\n"
    "    CollectionIterator &operator=(CollectionIterator &&other) = delete;\n"
    "\n"
    "    operator bool(void) const\n"
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
    "    CollectionPropertyIterator iter;\n"
    "    T t;\n"
    "    bool init;\n"
    "};\n"
    "\n"
    "template<typename Tp, typename T, TBeginFunc Tbegin, TNextFunc Tnext, TEndFunc Tend,\n"
    "         TLengthFunc Tlength, TEmptyFunc Tempty, TLookupIntFunc Tlookup_int,\n"
    "         TLookupStringFunc Tlookup_string, typename Tcollection_funcs>\n"
    "class CollectionRef : public Tcollection_funcs {\n"
    "public:\n"
    "    CollectionRef(const PointerRNA &p) : Tcollection_funcs(p), ptr(p) {}\n"
    "\n"
    "    void begin(CollectionIterator<T, Tbegin, Tnext, Tend>& iter)\n"
    "    { iter.begin(ptr); }\n"
    "    CollectionIterator<T, Tbegin, Tnext, Tend> begin()\n"
    "    { return CollectionIterator<T, Tbegin, Tnext, Tend>(ptr); }\n"
    "    CollectionIterator<T, Tbegin, Tnext, Tend> end()\n"
    "    { return CollectionIterator<T, Tbegin, Tnext, Tend>(); } /* test */ \n"
    ""
    "    int length()\n"
    "    { return Tlength(&ptr); }\n"
    "    bool empty()\n"
    "    { return Tempty(&ptr); }\n"
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

static bool rna_is_collection_prop(PropertyRNA *prop)
{
  if (!(prop->flag & PROP_IDPROPERTY || prop->flag_internal & PROP_INTERN_BUILTIN)) {
    if (prop->type == PROP_COLLECTION) {
      return true;
    }
  }

  return false;
}

static bool rna_is_collection_functions_struct(const char **collection_structs,
                                               const char *struct_name)
{
  int a = 0;
  bool found = false;

  while (collection_structs[a]) {
    if (STREQ(collection_structs[a], struct_name)) {
      found = true;
      break;
    }
    a++;
  }

  return found;
}

static void rna_generate_header_class_cpp(StructDefRNA *ds, FILE *f)
{
  StructRNA *srna = ds->srna;
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
  LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
    if (rna_is_collection_prop(dp->prop)) {
      fprintf(f, ",\n\t\t%s(ptr_arg)", dp->prop->identifier);
    }
  }
  fprintf(f, "\n\t\t{}\n\n");

  LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
    rna_def_property_funcs_header_cpp(f, ds->srna, dp);
  }

  fprintf(f, "\n");
  for (dfunc = static_cast<FunctionDefRNA *>(ds->functions.first); dfunc;
       dfunc = static_cast<FunctionDefRNA *>(dfunc->cont.next))
  {
    rna_def_struct_function_header_cpp(f, srna, dfunc);
  }

  fprintf(f, "};\n\n");
}

static void rna_generate_header_cpp(BlenderRNA * /*brna*/, FILE *f)
{
  StructDefRNA *ds;
  StructRNA *srna;
  FunctionDefRNA *dfunc;
  const char *first_collection_func_struct = nullptr;
  const char *collection_func_structs[256] = {nullptr};
  int all_collection_func_structs = 0;
  int max_collection_func_structs = sizeof(collection_func_structs) /
                                        sizeof(collection_func_structs[0]) -
                                    1;

  fprintf(f, "\n#ifndef __RNA_BLENDER_CPP_H__\n");
  fprintf(f, "#define __RNA_BLENDER_CPP_H__\n\n");

  fprintf(f,
          "/* Automatically generated classes for the Data API.\n"
          " * Do not edit manually, changes will be overwritten. */\n\n");

  fprintf(f, "#include \"RNA_blender.hh\"\n");
  fprintf(f, "#include \"RNA_types.hh\"\n");
  fprintf(f, "#include \"RNA_access.hh\"\n");
  fprintf(f, "#include \"DNA_node_types.h\"\n");

  fprintf(f, "%s", cpp_classes);

  fprintf(f, "/**************** Declarations ****************/\n\n");

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    fprintf(f, "class %s;\n", ds->srna->identifier);
  }
  fprintf(f, "\n");

  /* first get list of all structures used as collection functions, so they'll be declared first */
  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
      if (rna_is_collection_prop(dp->prop)) {
        PropertyRNA *prop = dp->prop;

        if (prop->srna) {
          /* store name of structure which first uses custom functions for collections */
          if (first_collection_func_struct == nullptr) {
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
  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    srna = ds->srna;

    if (STREQ(srna->identifier, first_collection_func_struct)) {
      StructDefRNA *ds2;
      StructRNA *srna2;

      for (ds2 = static_cast<StructDefRNA *>(DefRNA.structs.first); ds2;
           ds2 = static_cast<StructDefRNA *>(ds2->cont.next))
      {
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
  rna_generate_struct_prototypes(f);

  fprintf(f, "namespace BL {\n");

  for (ds = static_cast<StructDefRNA *>(DefRNA.structs.first); ds;
       ds = static_cast<StructDefRNA *>(ds->cont.next))
  {
    srna = ds->srna;

    LISTBASE_FOREACH (PropertyDefRNA *, dp, &ds->cont.properties) {
      rna_def_property_funcs_impl_cpp(f, ds->srna, dp);
    }

    fprintf(f, "\n");

    for (dfunc = static_cast<FunctionDefRNA *>(ds->functions.first); dfunc;
         dfunc = static_cast<FunctionDefRNA *>(dfunc->cont.next))
    {
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
    rna_generate_external_property_prototypes(brna, file);
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
    rna_generate_struct_rna_prototypes(brna, file);
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

  /* Create `RNA_blender_cpp.hh`. */
  SNPRINTF(deffile, "%s%s", outfile, "RNA_blender_cpp.hh" TMP_EXT);

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
      rna_generate_header_cpp(brna, file);
      fclose(file);
      if (DefRNA.error) {
        status = EXIT_FAILURE;
      }
    }
  }

  replace_if_different(deffile, nullptr);

  rna_sort(brna);

  /* Create `RNA_blender.hh`. */
  SNPRINTF(deffile, "%s%s", outfile, "RNA_blender.hh" TMP_EXT);

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
      rna_generate_header(brna, file);
      fclose(file);
      if (DefRNA.error) {
        status = EXIT_FAILURE;
      }
    }
  }

  replace_if_different(deffile, nullptr);

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
