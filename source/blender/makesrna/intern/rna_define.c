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

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_genfile.h"
#include "DNA_sdna_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "UI_interface.h" /* For things like UI_PRECISION_FLOAT_MAX... */

#include "RNA_define.h"

#include "rna_internal.h"

#include "CLG_log.h"

static CLG_LogRef LOG = {"rna.define"};

#ifdef DEBUG
#  define ASSERT_SOFT_HARD_LIMITS \
    if (softmin < hardmin || softmax > hardmax) { \
      CLOG_ERROR(&LOG, "error with soft/hard limits: %s.%s", CONTAINER_RNA_ID(cont), identifier); \
      BLI_assert(!"invalid soft/hard limits"); \
    } \
    (void)0
#else
#  define ASSERT_SOFT_HARD_LIMITS (void)0
#endif

/* Global used during defining */

BlenderDefRNA DefRNA = {
    .sdna = NULL,
    .structs = {NULL, NULL},
    .allocs = {NULL, NULL},
    .laststruct = NULL,
    .error = 0,
    .silent = false,
    .preprocess = false,
    .verify = true,
    .animate = true,
    .make_overridable = false,
};

#ifndef RNA_RUNTIME
static struct {
  GHash *struct_map_static_from_alias;
} g_version_data;
#endif

#ifndef RNA_RUNTIME
/**
 * When set, report details about which defaults are used.
 * Noisy but handy when investigating default extraction.
 */
static bool debugSRNA_defaults = false;

static void print_defult_info(const PropertyDefRNA *dp)
{
  fprintf(stderr,
          "dna_type=%s, dna_offset=%d, dna_struct=%s, dna_name=%s, id=%s\n",
          dp->dnatype,
          dp->dnaoffset,
          dp->dnastructname,
          dp->dnaname,
          dp->prop->identifier);
}
#endif /* RNA_RUNTIME */

/* Duplicated code since we can't link in blenkernel or blenlib */

/* pedantic check for final '.', note '...' are allowed though. */
#ifndef NDEBUG
#  define DESCR_CHECK(description, id1, id2) \
    if (description && (description)[0]) { \
      int i = strlen(description); \
      if (i > 3 && (description)[i - 1] == '.' && (description)[i - 3] != '.') { \
        CLOG_WARN(&LOG, \
                  "'%s' description from '%s' '%s' ends with a '.' !", \
                  description, \
                  id1 ? id1 : "", \
                  id2 ? id2 : ""); \
      } \
    } \
    (void)0

#else
#  define DESCR_CHECK(description, id1, id2)
#endif

void rna_addtail(ListBase *listbase, void *vlink)
{
  Link *link = vlink;

  link->next = NULL;
  link->prev = listbase->last;

  if (listbase->last) {
    ((Link *)listbase->last)->next = link;
  }
  if (listbase->first == NULL) {
    listbase->first = link;
  }
  listbase->last = link;
}

static void rna_remlink(ListBase *listbase, void *vlink)
{
  Link *link = vlink;

  if (link->next) {
    link->next->prev = link->prev;
  }
  if (link->prev) {
    link->prev->next = link->next;
  }

  if (listbase->last == link) {
    listbase->last = link->prev;
  }
  if (listbase->first == link) {
    listbase->first = link->next;
  }
}

PropertyDefRNA *rna_findlink(ListBase *listbase, const char *identifier)
{
  Link *link;

  for (link = listbase->first; link; link = link->next) {
    PropertyRNA *prop = ((PropertyDefRNA *)link)->prop;
    if (prop && (STREQ(prop->identifier, identifier))) {
      return (PropertyDefRNA *)link;
    }
  }

  return NULL;
}

void rna_freelinkN(ListBase *listbase, void *vlink)
{
  rna_remlink(listbase, vlink);
  MEM_freeN(vlink);
}

void rna_freelistN(ListBase *listbase)
{
  Link *link, *next;

  for (link = listbase->first; link; link = next) {
    next = link->next;
    MEM_freeN(link);
  }

  listbase->first = listbase->last = NULL;
}

static void rna_brna_structs_add(BlenderRNA *brna, StructRNA *srna)
{
  rna_addtail(&brna->structs, srna);
  brna->structs_len += 1;

  /* This exception is only needed for pre-processing.
   * otherwise we don't allow empty names. */
  if ((srna->flag & STRUCT_PUBLIC_NAMESPACE) && (srna->identifier[0] != '\0')) {
    BLI_ghash_insert(brna->structs_map, (void *)srna->identifier, srna);
  }
}

#ifdef RNA_RUNTIME
static void rna_brna_structs_remove_and_free(BlenderRNA *brna, StructRNA *srna)
{
  if ((srna->flag & STRUCT_PUBLIC_NAMESPACE) && brna->structs_map) {
    if (srna->identifier[0] != '\0') {
      BLI_ghash_remove(brna->structs_map, (void *)srna->identifier, NULL, NULL);
    }
  }

  RNA_def_struct_free_pointers(NULL, srna);

  if (srna->flag & STRUCT_RUNTIME) {
    rna_freelinkN(&brna->structs, srna);
  }
  brna->structs_len -= 1;
}
#endif

static int DNA_struct_find_nr_wrapper(const struct SDNA *sdna, const char *struct_name)
{
  struct_name = DNA_struct_rename_legacy_hack_static_from_alias(struct_name);
#ifdef RNA_RUNTIME
  /* We may support this at some point but for now we don't. */
  BLI_assert(0);
#else
  struct_name = BLI_ghash_lookup_default(
      g_version_data.struct_map_static_from_alias, struct_name, (void *)struct_name);
#endif
  return DNA_struct_find_nr(sdna, struct_name);
}

StructDefRNA *rna_find_struct_def(StructRNA *srna)
{
  StructDefRNA *dsrna;

  if (!DefRNA.preprocess) {
    /* we should never get here */
    CLOG_ERROR(&LOG, "only at preprocess time.");
    return NULL;
  }

  dsrna = DefRNA.structs.last;
  for (; dsrna; dsrna = dsrna->cont.prev) {
    if (dsrna->srna == srna) {
      return dsrna;
    }
  }

  return NULL;
}

PropertyDefRNA *rna_find_struct_property_def(StructRNA *srna, PropertyRNA *prop)
{
  StructDefRNA *dsrna;
  PropertyDefRNA *dprop;

  if (!DefRNA.preprocess) {
    /* we should never get here */
    CLOG_ERROR(&LOG, "only at preprocess time.");
    return NULL;
  }

  dsrna = rna_find_struct_def(srna);
  dprop = dsrna->cont.properties.last;
  for (; dprop; dprop = dprop->prev) {
    if (dprop->prop == prop) {
      return dprop;
    }
  }

  dsrna = DefRNA.structs.last;
  for (; dsrna; dsrna = dsrna->cont.prev) {
    dprop = dsrna->cont.properties.last;
    for (; dprop; dprop = dprop->prev) {
      if (dprop->prop == prop) {
        return dprop;
      }
    }
  }

  return NULL;
}

#if 0
static PropertyDefRNA *rna_find_property_def(PropertyRNA *prop)
{
  PropertyDefRNA *dprop;

  if (!DefRNA.preprocess) {
    /* we should never get here */
    CLOG_ERROR(&LOG, "only at preprocess time.");
    return NULL;
  }

  dprop = rna_find_struct_property_def(DefRNA.laststruct, prop);
  if (dprop) {
    return dprop;
  }

  dprop = rna_find_parameter_def(prop);
  if (dprop) {
    return dprop;
  }

  return NULL;
}
#endif

FunctionDefRNA *rna_find_function_def(FunctionRNA *func)
{
  StructDefRNA *dsrna;
  FunctionDefRNA *dfunc;

  if (!DefRNA.preprocess) {
    /* we should never get here */
    CLOG_ERROR(&LOG, "only at preprocess time.");
    return NULL;
  }

  dsrna = rna_find_struct_def(DefRNA.laststruct);
  dfunc = dsrna->functions.last;
  for (; dfunc; dfunc = dfunc->cont.prev) {
    if (dfunc->func == func) {
      return dfunc;
    }
  }

  dsrna = DefRNA.structs.last;
  for (; dsrna; dsrna = dsrna->cont.prev) {
    dfunc = dsrna->functions.last;
    for (; dfunc; dfunc = dfunc->cont.prev) {
      if (dfunc->func == func) {
        return dfunc;
      }
    }
  }

  return NULL;
}

PropertyDefRNA *rna_find_parameter_def(PropertyRNA *parm)
{
  StructDefRNA *dsrna;
  FunctionDefRNA *dfunc;
  PropertyDefRNA *dparm;

  if (!DefRNA.preprocess) {
    /* we should never get here */
    CLOG_ERROR(&LOG, "only at preprocess time.");
    return NULL;
  }

  dsrna = rna_find_struct_def(DefRNA.laststruct);
  dfunc = dsrna->functions.last;
  for (; dfunc; dfunc = dfunc->cont.prev) {
    dparm = dfunc->cont.properties.last;
    for (; dparm; dparm = dparm->prev) {
      if (dparm->prop == parm) {
        return dparm;
      }
    }
  }

  dsrna = DefRNA.structs.last;
  for (; dsrna; dsrna = dsrna->cont.prev) {
    dfunc = dsrna->functions.last;
    for (; dfunc; dfunc = dfunc->cont.prev) {
      dparm = dfunc->cont.properties.last;
      for (; dparm; dparm = dparm->prev) {
        if (dparm->prop == parm) {
          return dparm;
        }
      }
    }
  }

  return NULL;
}

static ContainerDefRNA *rna_find_container_def(ContainerRNA *cont)
{
  StructDefRNA *ds;
  FunctionDefRNA *dfunc;

  if (!DefRNA.preprocess) {
    /* we should never get here */
    CLOG_ERROR(&LOG, "only at preprocess time.");
    return NULL;
  }

  ds = rna_find_struct_def((StructRNA *)cont);
  if (ds) {
    return &ds->cont;
  }

  dfunc = rna_find_function_def((FunctionRNA *)cont);
  if (dfunc) {
    return &dfunc->cont;
  }

  return NULL;
}

/* DNA utility function for looking up members */

typedef struct DNAStructMember {
  const char *type;
  const char *name;
  int arraylength;
  int pointerlevel;
  int offset;
  int size;
} DNAStructMember;

static int rna_member_cmp(const char *name, const char *oname)
{
  int a = 0;

  /* compare without pointer or array part */
  while (name[0] == '*') {
    name++;
  }
  while (oname[0] == '*') {
    oname++;
  }

  while (1) {
    if (name[a] == '[' && oname[a] == 0) {
      return 1;
    }
    if (name[a] == '[' && oname[a] == '[') {
      return 1;
    }
    if (name[a] == 0) {
      break;
    }
    if (name[a] != oname[a]) {
      return 0;
    }
    a++;
  }
  if (name[a] == 0 && oname[a] == '.') {
    return 2;
  }
  if (name[a] == 0 && oname[a] == '-' && oname[a + 1] == '>') {
    return 3;
  }

  return (name[a] == oname[a]);
}

static int rna_find_sdna_member(SDNA *sdna,
                                const char *structname,
                                const char *membername,
                                DNAStructMember *smember,
                                int *offset)
{
  const char *dnaname;
  const short *sp;
  int a, b, structnr, totmember, cmp;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return 0;
  }
  structnr = DNA_struct_find_nr_wrapper(sdna, structname);

  smember->offset = -1;
  if (structnr == -1) {
    if (offset) {
      *offset = -1;
    }
    return 0;
  }

  sp = sdna->structs[structnr];
  totmember = sp[1];
  sp += 2;

  for (a = 0; a < totmember; a++, sp += 2) {
    const int size = DNA_elem_size_nr(sdna, sp[0], sp[1]);
    dnaname = sdna->alias.names[sp[1]];
    cmp = rna_member_cmp(dnaname, membername);

    if (cmp == 1) {
      smember->type = sdna->alias.types[sp[0]];
      smember->name = dnaname;
      smember->offset = *offset;
      smember->size = size;

      if (strstr(membername, "[")) {
        smember->arraylength = 0;
      }
      else {
        smember->arraylength = DNA_elem_array_size(smember->name);
      }

      smember->pointerlevel = 0;
      for (b = 0; dnaname[b] == '*'; b++) {
        smember->pointerlevel++;
      }

      return 1;
    }
    else if (cmp == 2) {
      smember->type = "";
      smember->name = dnaname;
      smember->offset = *offset;
      smember->size = size;
      smember->pointerlevel = 0;
      smember->arraylength = 0;

      membername = strstr(membername, ".") + strlen(".");
      rna_find_sdna_member(sdna, sdna->alias.types[sp[0]], membername, smember, offset);

      return 1;
    }
    else if (cmp == 3) {
      smember->type = "";
      smember->name = dnaname;
      smember->offset = *offset;
      smember->size = size;
      smember->pointerlevel = 0;
      smember->arraylength = 0;

      if (offset) {
        *offset = -1;
      }
      membername = strstr(membername, "->") + strlen("->");
      rna_find_sdna_member(sdna, sdna->alias.types[sp[0]], membername, smember, offset);

      return 1;
    }

    if (offset && *offset != -1) {
      *offset += size;
    }
  }

  return 0;
}

static int rna_validate_identifier(const char *identifier, char *error, bool property)
{
  int a = 0;

  /** List is from:
   * \code{.py}
   * ", ".join([
   *     '"%s"' % kw for kw in __import__("keyword").kwlist
   *     if kw not in {"False", "None", "True"}
   * ])
   * \endcode
   */
  static const char *kwlist[] = {
      /* "False", "None", "True", */
      "and",    "as",   "assert", "async",  "await",    "break", "class", "continue", "def",
      "del",    "elif", "else",   "except", "finally",  "for",   "from",  "global",   "if",
      "import", "in",   "is",     "lambda", "nonlocal", "not",   "or",    "pass",     "raise",
      "return", "try",  "while",  "with",   "yield",    NULL,
  };

  if (!isalpha(identifier[0])) {
    strcpy(error, "first character failed isalpha() check");
    return 0;
  }

  for (a = 0; identifier[a]; a++) {
    if (DefRNA.preprocess && property) {
      if (isalpha(identifier[a]) && isupper(identifier[a])) {
        strcpy(error, "property names must contain lower case characters only");
        return 0;
      }
    }

    if (identifier[a] == '_') {
      continue;
    }

    if (identifier[a] == ' ') {
      strcpy(error, "spaces are not okay in identifier names");
      return 0;
    }

    if (isalnum(identifier[a]) == 0) {
      strcpy(error, "one of the characters failed an isalnum() check and is not an underscore");
      return 0;
    }
  }

  for (a = 0; kwlist[a]; a++) {
    if (STREQ(identifier, kwlist[a])) {
      strcpy(error, "this keyword is reserved by python");
      return 0;
    }
  }

  if (property) {
    static const char *kwlist_prop[] = {
        /* not keywords but reserved all the same because py uses */
        "keys",
        "values",
        "items",
        "get",
        NULL,
    };

    for (a = 0; kwlist_prop[a]; a++) {
      if (STREQ(identifier, kwlist_prop[a])) {
        strcpy(error, "this keyword is reserved by python");
        return 0;
      }
    }
  }

  return 1;
}

void RNA_identifier_sanitize(char *identifier, int property)
{
  int a = 0;

  /*  list from http://docs.python.org/py3k/reference/lexical_analysis.html#keywords */
  static const char *kwlist[] = {
      /* "False", "None", "True", */
      "and",    "as",     "assert", "break",   "class",    "continue", "def",    "del",
      "elif",   "else",   "except", "finally", "for",      "from",     "global", "if",
      "import", "in",     "is",     "lambda",  "nonlocal", "not",      "or",     "pass",
      "raise",  "return", "try",    "while",   "with",     "yield",    NULL,
  };

  if (!isalpha(identifier[0])) {
    /* first character failed isalpha() check */
    identifier[0] = '_';
  }

  for (a = 0; identifier[a]; a++) {
    if (DefRNA.preprocess && property) {
      if (isalpha(identifier[a]) && isupper(identifier[a])) {
        /* property names must contain lower case characters only */
        identifier[a] = tolower(identifier[a]);
      }
    }

    if (identifier[a] == '_') {
      continue;
    }

    if (identifier[a] == ' ') {
      /* spaces are not okay in identifier names */
      identifier[a] = '_';
    }

    if (isalnum(identifier[a]) == 0) {
      /* one of the characters failed an isalnum() check and is not an underscore */
      identifier[a] = '_';
    }
  }

  for (a = 0; kwlist[a]; a++) {
    if (STREQ(identifier, kwlist[a])) {
      /* this keyword is reserved by python.
       * just replace the last character by '_' to keep it readable.
       */
      identifier[strlen(identifier) - 1] = '_';
      break;
    }
  }

  if (property) {
    static const char *kwlist_prop[] = {
        /* not keywords but reserved all the same because py uses */
        "keys",
        "values",
        "items",
        "get",
        NULL,
    };

    for (a = 0; kwlist_prop[a]; a++) {
      if (STREQ(identifier, kwlist_prop[a])) {
        /* this keyword is reserved by python.
         * just replace the last character by '_' to keep it readable.
         */
        identifier[strlen(identifier) - 1] = '_';
        break;
      }
    }
  }
}

/* Blender Data Definition */

BlenderRNA *RNA_create(void)
{
  BlenderRNA *brna;

  brna = MEM_callocN(sizeof(BlenderRNA), "BlenderRNA");
  const char *error_message = NULL;

  BLI_listbase_clear(&DefRNA.structs);
  brna->structs_map = BLI_ghash_str_new_ex(__func__, 2048);

  DefRNA.error = false;
  DefRNA.preprocess = true;

  DefRNA.sdna = DNA_sdna_from_data(DNAstr, DNAlen, false, false, &error_message);
  if (DefRNA.sdna == NULL) {
    CLOG_ERROR(&LOG, "Failed to decode SDNA: %s.", error_message);
    DefRNA.error = true;
  }

  /* We need both alias and static (on-disk) DNA names. */
  DNA_sdna_alias_data_ensure(DefRNA.sdna);

#ifndef RNA_RUNTIME
  DNA_alias_maps(DNA_RENAME_STATIC_FROM_ALIAS, &g_version_data.struct_map_static_from_alias, NULL);
#endif

  return brna;
}

void RNA_define_free(BlenderRNA *UNUSED(brna))
{
  StructDefRNA *ds;
  FunctionDefRNA *dfunc;
  AllocDefRNA *alloc;

  for (alloc = DefRNA.allocs.first; alloc; alloc = alloc->next) {
    MEM_freeN(alloc->mem);
  }
  rna_freelistN(&DefRNA.allocs);

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    for (dfunc = ds->functions.first; dfunc; dfunc = dfunc->cont.next) {
      rna_freelistN(&dfunc->cont.properties);
    }

    rna_freelistN(&ds->cont.properties);
    rna_freelistN(&ds->functions);
  }

  rna_freelistN(&DefRNA.structs);

  if (DefRNA.sdna) {
    DNA_sdna_free(DefRNA.sdna);
    DefRNA.sdna = NULL;
  }

  DefRNA.error = false;
}

void RNA_define_verify_sdna(bool verify)
{
  DefRNA.verify = verify;
}

/**
 * Properties defined when this is enabled are lib-overridable by default (except for Pointer
 * ones).
 */
void RNA_define_lib_overridable(const bool make_overridable)
{
  DefRNA.make_overridable = make_overridable;
}

#ifndef RNA_RUNTIME
void RNA_define_animate_sdna(bool animate)
{
  DefRNA.animate = animate;
}
#endif

#ifndef RNA_RUNTIME
void RNA_define_fallback_property_update(int noteflag, const char *updatefunc)
{
  DefRNA.fallback.property_update.noteflag = noteflag;
  DefRNA.fallback.property_update.updatefunc = updatefunc;
}
#endif

void RNA_struct_free_extension(StructRNA *srna, ExtensionRNA *rna_ext)
{
#ifdef RNA_RUNTIME
  rna_ext->free(rna_ext->data);            /* decref's the PyObject that the srna owns */
  RNA_struct_blender_type_set(srna, NULL); /* this gets accessed again - XXX fixme */

  /* NULL the srna's value so RNA_struct_free wont complain of a leak */
  RNA_struct_py_type_set(srna, NULL);

#else
  (void)srna;
  (void)rna_ext;
#endif
}

void RNA_struct_free(BlenderRNA *brna, StructRNA *srna)
{
#ifdef RNA_RUNTIME
  FunctionRNA *func, *nextfunc;
  PropertyRNA *prop, *nextprop;
  PropertyRNA *parm, *nextparm;

#  if 0
  if (srna->flag & STRUCT_RUNTIME) {
    if (RNA_struct_py_type_get(srna)) {
      fprintf(stderr, "%s '%s' freed while holding a python reference.", srna->identifier);
    }
  }
#  endif

  for (prop = srna->cont.properties.first; prop; prop = nextprop) {
    nextprop = prop->next;

    RNA_def_property_free_pointers(prop);

    if (prop->flag_internal & PROP_INTERN_RUNTIME) {
      rna_freelinkN(&srna->cont.properties, prop);
    }
  }

  for (func = srna->functions.first; func; func = nextfunc) {
    nextfunc = func->cont.next;

    for (parm = func->cont.properties.first; parm; parm = nextparm) {
      nextparm = parm->next;

      RNA_def_property_free_pointers(parm);

      if (parm->flag_internal & PROP_INTERN_RUNTIME) {
        rna_freelinkN(&func->cont.properties, parm);
      }
    }

    RNA_def_func_free_pointers(func);

    if (func->flag & FUNC_RUNTIME) {
      rna_freelinkN(&srna->functions, func);
    }
  }

  rna_brna_structs_remove_and_free(brna, srna);
#else
  UNUSED_VARS(brna, srna);
#endif
}

void RNA_free(BlenderRNA *brna)
{
  StructRNA *srna, *nextsrna;
  FunctionRNA *func;

  BLI_ghash_free(brna->structs_map, NULL, NULL);
  brna->structs_map = NULL;

  if (DefRNA.preprocess) {
    RNA_define_free(brna);

    for (srna = brna->structs.first; srna; srna = srna->cont.next) {
      for (func = srna->functions.first; func; func = func->cont.next) {
        rna_freelistN(&func->cont.properties);
      }

      rna_freelistN(&srna->cont.properties);
      rna_freelistN(&srna->functions);
    }

    rna_freelistN(&brna->structs);

    MEM_freeN(brna);
  }
  else {
    for (srna = brna->structs.first; srna; srna = nextsrna) {
      nextsrna = srna->cont.next;
      RNA_struct_free(brna, srna);
    }
  }

#ifndef RNA_RUNTIME
  BLI_ghash_free(g_version_data.struct_map_static_from_alias, NULL, NULL);
  g_version_data.struct_map_static_from_alias = NULL;
#endif
}

static size_t rna_property_type_sizeof(PropertyType type)
{
  switch (type) {
    case PROP_BOOLEAN:
      return sizeof(BoolPropertyRNA);
    case PROP_INT:
      return sizeof(IntPropertyRNA);
    case PROP_FLOAT:
      return sizeof(FloatPropertyRNA);
    case PROP_STRING:
      return sizeof(StringPropertyRNA);
    case PROP_ENUM:
      return sizeof(EnumPropertyRNA);
    case PROP_POINTER:
      return sizeof(PointerPropertyRNA);
    case PROP_COLLECTION:
      return sizeof(CollectionPropertyRNA);
    default:
      return 0;
  }
}

static StructDefRNA *rna_find_def_struct(StructRNA *srna)
{
  StructDefRNA *ds;

  for (ds = DefRNA.structs.first; ds; ds = ds->cont.next) {
    if (ds->srna == srna) {
      return ds;
    }
  }

  return NULL;
}

/* Struct Definition */
StructRNA *RNA_def_struct_ptr(BlenderRNA *brna, const char *identifier, StructRNA *srnafrom)
{
  StructRNA *srna;
  StructDefRNA *ds = NULL, *dsfrom = NULL;
  PropertyRNA *prop;

  if (DefRNA.preprocess) {
    char error[512];

    if (rna_validate_identifier(identifier, error, false) == 0) {
      CLOG_ERROR(&LOG, "struct identifier \"%s\" error - %s", identifier, error);
      DefRNA.error = true;
    }
  }

  srna = MEM_callocN(sizeof(StructRNA), "StructRNA");
  DefRNA.laststruct = srna;

  if (srnafrom) {
    /* copy from struct to derive stuff, a bit clumsy since we can't
     * use MEM_dupallocN, data structs may not be alloced but builtin */
    memcpy(srna, srnafrom, sizeof(StructRNA));
    srna->cont.prophash = NULL;
    BLI_listbase_clear(&srna->cont.properties);
    BLI_listbase_clear(&srna->functions);
    srna->py_type = NULL;

    srna->base = srnafrom;

    if (DefRNA.preprocess) {
      dsfrom = rna_find_def_struct(srnafrom);
    }
    else {
      if (srnafrom->flag & STRUCT_PUBLIC_NAMESPACE_INHERIT) {
        srna->flag |= STRUCT_PUBLIC_NAMESPACE | STRUCT_PUBLIC_NAMESPACE_INHERIT;
      }
      else {
        srna->flag &= ~(STRUCT_PUBLIC_NAMESPACE | STRUCT_PUBLIC_NAMESPACE_INHERIT);
      }
    }
  }

  srna->identifier = identifier;
  srna->name = identifier; /* may be overwritten later RNA_def_struct_ui_text */
  srna->description = "";
  /* may be overwritten later RNA_def_struct_translation_context */
  srna->translation_context = BLT_I18NCONTEXT_DEFAULT_BPYRNA;
  if (!srnafrom) {
    srna->icon = ICON_DOT;
    srna->flag |= STRUCT_UNDO;
  }

  if (DefRNA.preprocess) {
    srna->flag |= STRUCT_PUBLIC_NAMESPACE;
  }

  rna_brna_structs_add(brna, srna);

  if (DefRNA.preprocess) {
    ds = MEM_callocN(sizeof(StructDefRNA), "StructDefRNA");
    ds->srna = srna;
    rna_addtail(&DefRNA.structs, ds);

    if (dsfrom) {
      ds->dnafromname = dsfrom->dnaname;
    }
  }

  /* in preprocess, try to find sdna */
  if (DefRNA.preprocess) {
    RNA_def_struct_sdna(srna, srna->identifier);
  }
  else {
    srna->flag |= STRUCT_RUNTIME;
  }

  if (srnafrom) {
    srna->nameproperty = srnafrom->nameproperty;
    srna->iteratorproperty = srnafrom->iteratorproperty;
  }
  else {
    /* define some builtin properties */
    prop = RNA_def_property(&srna->cont, "rna_properties", PROP_COLLECTION, PROP_NONE);
    prop->flag_internal |= PROP_INTERN_BUILTIN;
    RNA_def_property_ui_text(prop, "Properties", "RNA property collection");

    if (DefRNA.preprocess) {
      RNA_def_property_struct_type(prop, "Property");
      RNA_def_property_collection_funcs(prop,
                                        "rna_builtin_properties_begin",
                                        "rna_builtin_properties_next",
                                        "rna_iterator_listbase_end",
                                        "rna_builtin_properties_get",
                                        NULL,
                                        NULL,
                                        "rna_builtin_properties_lookup_string",
                                        NULL);
    }
    else {
#ifdef RNA_RUNTIME
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
      cprop->begin = rna_builtin_properties_begin;
      cprop->next = rna_builtin_properties_next;
      cprop->get = rna_builtin_properties_get;
      cprop->item_type = &RNA_Property;
#endif
    }

    prop = RNA_def_property(&srna->cont, "rna_type", PROP_POINTER, PROP_NONE);
    RNA_def_property_flag(prop, PROP_HIDDEN);
    RNA_def_property_ui_text(prop, "RNA", "RNA type definition");

    if (DefRNA.preprocess) {
      RNA_def_property_struct_type(prop, "Struct");
      RNA_def_property_pointer_funcs(prop, "rna_builtin_type_get", NULL, NULL, NULL);
    }
    else {
#ifdef RNA_RUNTIME
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
      pprop->get = rna_builtin_type_get;
      pprop->type = &RNA_Struct;
#endif
    }
  }

  return srna;
}

StructRNA *RNA_def_struct(BlenderRNA *brna, const char *identifier, const char *from)
{
  StructRNA *srnafrom = NULL;

  /* only use RNA_def_struct() while pre-processing, otherwise use RNA_def_struct_ptr() */
  BLI_assert(DefRNA.preprocess);

  if (from) {
    /* find struct to derive from */
    /* Inline RNA_struct_find(...) because it wont link from here. */
    srnafrom = BLI_ghash_lookup(brna->structs_map, from);
    if (!srnafrom) {
      CLOG_ERROR(&LOG, "struct %s not found to define %s.", from, identifier);
      DefRNA.error = true;
    }
  }

  return RNA_def_struct_ptr(brna, identifier, srnafrom);
}

void RNA_def_struct_sdna(StructRNA *srna, const char *structname)
{
  StructDefRNA *ds;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  ds = rna_find_def_struct(srna);

  /* There are far too many structs which initialize without valid DNA struct names,
   * this can't be checked without adding an option to disable
   * (tested this and it means changes all over - Campbell) */
#if 0
  if (DNA_struct_find_nr_wrapper(DefRNA.sdna, structname) == -1) {
    if (!DefRNA.silent) {
      CLOG_ERROR(&LOG, "%s not found.", structname);
      DefRNA.error = true;
    }
    return;
  }
#endif

  ds->dnaname = structname;
}

void RNA_def_struct_sdna_from(StructRNA *srna, const char *structname, const char *propname)
{
  StructDefRNA *ds;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  ds = rna_find_def_struct(srna);

  if (!ds->dnaname) {
    CLOG_ERROR(&LOG, "%s base struct must know DNA already.", structname);
    return;
  }

  if (DNA_struct_find_nr_wrapper(DefRNA.sdna, structname) == -1) {
    if (!DefRNA.silent) {
      CLOG_ERROR(&LOG, "%s not found.", structname);
      DefRNA.error = true;
    }
    return;
  }

  ds->dnafromprop = propname;
  ds->dnaname = structname;
}

void RNA_def_struct_name_property(struct StructRNA *srna, struct PropertyRNA *prop)
{
  if (prop->type != PROP_STRING) {
    CLOG_ERROR(&LOG, "\"%s.%s\", must be a string property.", srna->identifier, prop->identifier);
    DefRNA.error = true;
  }
  else {
    srna->nameproperty = prop;
  }
}

void RNA_def_struct_nested(BlenderRNA *brna, StructRNA *srna, const char *structname)
{
  StructRNA *srnafrom;

  /* find struct to derive from */
  srnafrom = BLI_ghash_lookup(brna->structs_map, structname);
  if (!srnafrom) {
    CLOG_ERROR(&LOG, "struct %s not found for %s.", structname, srna->identifier);
    DefRNA.error = true;
  }

  srna->nested = srnafrom;
}

void RNA_def_struct_flag(StructRNA *srna, int flag)
{
  srna->flag |= flag;
}

void RNA_def_struct_clear_flag(StructRNA *srna, int flag)
{
  srna->flag &= ~flag;
}

void RNA_def_struct_property_tags(StructRNA *srna, const EnumPropertyItem *prop_tag_defines)
{
  srna->prop_tag_defines = prop_tag_defines;
}

void RNA_def_struct_refine_func(StructRNA *srna, const char *refine)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (refine) {
    srna->refine = (StructRefineFunc)refine;
  }
}

void RNA_def_struct_idprops_func(StructRNA *srna, const char *idproperties)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (idproperties) {
    srna->idproperties = (IDPropertiesFunc)idproperties;
  }
}

void RNA_def_struct_register_funcs(StructRNA *srna,
                                   const char *reg,
                                   const char *unreg,
                                   const char *instance)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (reg) {
    srna->reg = (StructRegisterFunc)reg;
  }
  if (unreg) {
    srna->unreg = (StructUnregisterFunc)unreg;
  }
  if (instance) {
    srna->instance = (StructInstanceFunc)instance;
  }
}

void RNA_def_struct_path_func(StructRNA *srna, const char *path)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (path) {
    srna->path = (StructPathFunc)path;
  }
}

void RNA_def_struct_identifier(BlenderRNA *brna, StructRNA *srna, const char *identifier)
{
  if (DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only at runtime.");
    return;
  }

  /* Operator registration may set twice, see: operator_properties_init */
  if (srna->flag & STRUCT_PUBLIC_NAMESPACE) {
    if (identifier != srna->identifier) {
      if (srna->identifier[0] != '\0') {
        BLI_ghash_remove(brna->structs_map, (void *)srna->identifier, NULL, NULL);
      }
      if (identifier[0] != '\0') {
        BLI_ghash_insert(brna->structs_map, (void *)identifier, srna);
      }
    }
  }

  srna->identifier = identifier;
}

/**
 * Only used in one case when we name the struct for the purpose of useful error messages.
 */
void RNA_def_struct_identifier_no_struct_map(StructRNA *srna, const char *identifier)
{
  if (DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only at runtime.");
    return;
  }

  srna->identifier = identifier;
}

void RNA_def_struct_ui_text(StructRNA *srna, const char *name, const char *description)
{
  DESCR_CHECK(description, srna->identifier, NULL);

  srna->name = name;
  srna->description = description;
}

void RNA_def_struct_ui_icon(StructRNA *srna, int icon)
{
  srna->icon = icon;
}

void RNA_def_struct_translation_context(StructRNA *srna, const char *context)
{
  srna->translation_context = context ? context : BLT_I18NCONTEXT_DEFAULT_BPYRNA;
}

/* Property Definition */

PropertyRNA *RNA_def_property(StructOrFunctionRNA *cont_,
                              const char *identifier,
                              int type,
                              int subtype)
{
  /*StructRNA *srna = DefRNA.laststruct;*/ /* invalid for python defined props */
  ContainerRNA *cont = cont_;
  ContainerDefRNA *dcont;
  PropertyDefRNA *dprop = NULL;
  PropertyRNA *prop;

  if (DefRNA.preprocess) {
    char error[512];

    if (rna_validate_identifier(identifier, error, true) == 0) {
      CLOG_ERROR(
          &LOG, "property identifier \"%s.%s\" - %s", CONTAINER_RNA_ID(cont), identifier, error);
      DefRNA.error = true;
    }

    dcont = rna_find_container_def(cont);

    /* XXX - toto, detect supertype collisions */
    if (rna_findlink(&dcont->properties, identifier)) {
      CLOG_ERROR(&LOG, "duplicate identifier \"%s.%s\"", CONTAINER_RNA_ID(cont), identifier);
      DefRNA.error = true;
    }

    dprop = MEM_callocN(sizeof(PropertyDefRNA), "PropertyDefRNA");
    rna_addtail(&dcont->properties, dprop);
  }
  else {
#ifdef DEBUG
    char error[512];
    if (rna_validate_identifier(identifier, error, true) == 0) {
      CLOG_ERROR(&LOG,
                 "runtime property identifier \"%s.%s\" - %s",
                 CONTAINER_RNA_ID(cont),
                 identifier,
                 error);
      DefRNA.error = true;
    }
#endif
  }

  prop = MEM_callocN(rna_property_type_sizeof(type), "PropertyRNA");

  switch (type) {
    case PROP_BOOLEAN:
      if (DefRNA.preprocess) {
        if ((subtype & ~PROP_LAYER_MEMBER) != PROP_NONE) {
          CLOG_ERROR(&LOG,
                     "subtype does not apply to 'PROP_BOOLEAN' \"%s.%s\"",
                     CONTAINER_RNA_ID(cont),
                     identifier);
          DefRNA.error = true;
        }
      }
      break;
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

#ifndef RNA_RUNTIME
      if (subtype == PROP_DISTANCE) {
        CLOG_ERROR(&LOG,
                   "subtype does not apply to 'PROP_INT' \"%s.%s\"",
                   CONTAINER_RNA_ID(cont),
                   identifier);
        DefRNA.error = true;
      }
#endif

      iprop->hardmin = (subtype == PROP_UNSIGNED) ? 0 : INT_MIN;
      iprop->hardmax = INT_MAX;

      iprop->softmin = (subtype == PROP_UNSIGNED) ? 0 : -10000; /* rather arbitrary .. */
      iprop->softmax = 10000;
      iprop->step = 1;
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

      fprop->hardmin = (subtype == PROP_UNSIGNED) ? 0.0f : -FLT_MAX;
      fprop->hardmax = FLT_MAX;

      if (ELEM(subtype, PROP_COLOR, PROP_COLOR_GAMMA)) {
        fprop->softmin = fprop->hardmin = 0.0f;
        fprop->softmax = 1.0f;
      }
      else if (subtype == PROP_FACTOR) {
        fprop->softmin = fprop->hardmin = 0.0f;
        fprop->softmax = fprop->hardmax = 1.0f;
      }
      else {
        fprop->softmin = (subtype == PROP_UNSIGNED) ? 0.0f : -10000.0f; /* rather arbitrary .. */
        fprop->softmax = 10000.0f;
      }
      fprop->step = 10;
      fprop->precision = 3;
      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
      /* By default don't allow NULL string args, callers may clear. */
      RNA_def_property_flag(prop, PROP_NEVER_NULL);
      sprop->defaultvalue = "";
      break;
    }
    case PROP_POINTER:
      prop->flag |= PROP_THICK_WRAP; /* needed for default behavior when PARM_RNAPTR is set */
      break;
    case PROP_ENUM:
    case PROP_COLLECTION:
      break;
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", invalid property type.", CONTAINER_RNA_ID(cont), identifier);
      DefRNA.error = true;
      return NULL;
  }

  if (DefRNA.preprocess) {
    dprop->cont = cont;
    dprop->prop = prop;
  }

  prop->magic = RNA_MAGIC;
  prop->identifier = identifier;
  prop->type = type;
  prop->subtype = subtype;
  prop->name = identifier;
  prop->description = "";
  prop->translation_context = BLT_I18NCONTEXT_DEFAULT_BPYRNA;
  /* a priori not raw editable */
  prop->rawtype = -1;

  if (type != PROP_COLLECTION && type != PROP_POINTER) {
    prop->flag = PROP_EDITABLE;

    if (type != PROP_STRING) {
#ifdef RNA_RUNTIME
      prop->flag |= PROP_ANIMATABLE;
#else
      if (DefRNA.animate) {
        prop->flag |= PROP_ANIMATABLE;
      }
#endif
    }
  }

#ifndef RNA_RUNTIME
  if (DefRNA.make_overridable) {
    prop->flag_override |= PROPOVERRIDE_OVERRIDABLE_LIBRARY;
  }
#endif

  if (type == PROP_STRING) {
    /* used so generated 'get/length/set' functions skip a NULL check
     * in some cases we want it */
    RNA_def_property_flag(prop, PROP_NEVER_NULL);
  }

  if (DefRNA.preprocess) {
    switch (type) {
      case PROP_BOOLEAN:
        DefRNA.silent = true;
        RNA_def_property_boolean_sdna(prop, NULL, identifier, 0);
        DefRNA.silent = false;
        break;
      case PROP_INT: {
        DefRNA.silent = true;
        RNA_def_property_int_sdna(prop, NULL, identifier);
        DefRNA.silent = false;
        break;
      }
      case PROP_FLOAT: {
        DefRNA.silent = true;
        RNA_def_property_float_sdna(prop, NULL, identifier);
        DefRNA.silent = false;
        break;
      }
      case PROP_STRING: {
        DefRNA.silent = true;
        RNA_def_property_string_sdna(prop, NULL, identifier);
        DefRNA.silent = false;
        break;
      }
      case PROP_ENUM:
        DefRNA.silent = true;
        RNA_def_property_enum_sdna(prop, NULL, identifier);
        DefRNA.silent = false;
        break;
      case PROP_POINTER:
        DefRNA.silent = true;
        RNA_def_property_pointer_sdna(prop, NULL, identifier);
        DefRNA.silent = false;
        break;
      case PROP_COLLECTION:
        DefRNA.silent = true;
        RNA_def_property_collection_sdna(prop, NULL, identifier, NULL);
        DefRNA.silent = false;
        break;
    }
  }
  else {
    prop->flag |= PROP_IDPROPERTY;
    prop->flag_internal |= PROP_INTERN_RUNTIME;
#ifdef RNA_RUNTIME
    if (cont->prophash) {
      BLI_ghash_insert(cont->prophash, (void *)prop->identifier, prop);
    }
#endif
  }

  /* Override handling. */
  if (DefRNA.preprocess) {
    prop->override_diff = (RNAPropOverrideDiff) "rna_property_override_diff_default";
    prop->override_store = (RNAPropOverrideStore) "rna_property_override_store_default";
    prop->override_apply = (RNAPropOverrideApply) "rna_property_override_apply_default";
  }
  /* TODO: do we want that for runtime-defined stuff too? I’d say no, but... maybe yes :/ */

#ifndef RNA_RUNTIME
  /* Both are typically cleared. */
  RNA_def_property_update(
      prop, DefRNA.fallback.property_update.noteflag, DefRNA.fallback.property_update.updatefunc);
#endif

  rna_addtail(&cont->properties, prop);

  return prop;
}

void RNA_def_property_flag(PropertyRNA *prop, PropertyFlag flag)
{
  prop->flag |= flag;
}

void RNA_def_property_clear_flag(PropertyRNA *prop, PropertyFlag flag)
{
  prop->flag &= ~flag;
}

void RNA_def_property_override_flag(PropertyRNA *prop, PropertyOverrideFlag flag)
{
  prop->flag_override |= flag;
}

void RNA_def_property_override_clear_flag(PropertyRNA *prop, PropertyOverrideFlag flag)
{
  prop->flag_override &= ~flag;
}

/**
 * Add the property-tags passed as \a tags to \a prop (if valid).
 *
 * \note Multiple tags can be set by passing them within \a tags (using bitflags).
 * \note Doesn't do any type-checking with the tags defined in the parent StructRNA
 *       of \a prop. This should be done before (e.g. see #WM_operatortype_prop_tag).
 */
void RNA_def_property_tags(PropertyRNA *prop, int tags)
{
  prop->tags |= tags;
}

void RNA_def_parameter_flags(PropertyRNA *prop,
                             PropertyFlag flag_property,
                             ParameterFlag flag_parameter)
{
  prop->flag |= flag_property;
  prop->flag_parameter |= flag_parameter;
}

void RNA_def_parameter_clear_flags(PropertyRNA *prop,
                                   PropertyFlag flag_property,
                                   ParameterFlag flag_parameter)
{
  prop->flag &= ~flag_property;
  prop->flag_parameter &= ~flag_parameter;
}

void RNA_def_property_subtype(PropertyRNA *prop, PropertySubType subtype)
{
  prop->subtype = subtype;
}

void RNA_def_property_array(PropertyRNA *prop, int length)
{
  StructRNA *srna = DefRNA.laststruct;

  if (length < 0) {
    CLOG_ERROR(&LOG,
               "\"%s.%s\", array length must be zero of greater.",
               srna->identifier,
               prop->identifier);
    DefRNA.error = true;
    return;
  }

  if (length > RNA_MAX_ARRAY_LENGTH) {
    CLOG_ERROR(&LOG,
               "\"%s.%s\", array length must be smaller than %d.",
               srna->identifier,
               prop->identifier,
               RNA_MAX_ARRAY_LENGTH);
    DefRNA.error = true;
    return;
  }

  if (prop->arraydimension > 1) {
    CLOG_ERROR(&LOG,
               "\"%s.%s\", array dimensions has been set to %u but would be overwritten as 1.",
               srna->identifier,
               prop->identifier,
               prop->arraydimension);
    DefRNA.error = true;
    return;
  }

  switch (prop->type) {
    case PROP_BOOLEAN:
    case PROP_INT:
    case PROP_FLOAT:
      prop->arraylength[0] = length;
      prop->totarraylength = length;
      prop->arraydimension = 1;
      break;
    default:
      CLOG_ERROR(&LOG,
                 "\"%s.%s\", only boolean/int/float can be array.",
                 srna->identifier,
                 prop->identifier);
      DefRNA.error = true;
      break;
  }
}

/* common args for defaults. */
const float rna_default_quaternion[4] = {1, 0, 0, 0};
const float rna_default_axis_angle[4] = {0, 0, 1, 0};
const float rna_default_scale_3d[3] = {1, 1, 1};

/* common args for length */
const int rna_matrix_dimsize_3x3[] = {3, 3};
const int rna_matrix_dimsize_4x4[] = {4, 4};
const int rna_matrix_dimsize_4x2[] = {4, 2};

void RNA_def_property_multi_array(PropertyRNA *prop, int dimension, const int length[])
{
  StructRNA *srna = DefRNA.laststruct;
  int i;

  if (dimension < 1 || dimension > RNA_MAX_ARRAY_DIMENSION) {
    CLOG_ERROR(&LOG,
               "\"%s.%s\", array dimension must be between 1 and %d.",
               srna->identifier,
               prop->identifier,
               RNA_MAX_ARRAY_DIMENSION);
    DefRNA.error = true;
    return;
  }

  switch (prop->type) {
    case PROP_BOOLEAN:
    case PROP_INT:
    case PROP_FLOAT:
      break;
    default:
      CLOG_ERROR(&LOG,
                 "\"%s.%s\", only boolean/int/float can be array.",
                 srna->identifier,
                 prop->identifier);
      DefRNA.error = true;
      break;
  }

  prop->arraydimension = dimension;
  prop->totarraylength = 0;

  if (length) {
    memcpy(prop->arraylength, length, sizeof(int) * dimension);

    prop->totarraylength = length[0];
    for (i = 1; i < dimension; i++) {
      prop->totarraylength *= length[i];
    }
  }
  else {
    memset(prop->arraylength, 0, sizeof(prop->arraylength));
  }

  /* TODO make sure arraylength values are sane  */
}

void RNA_def_property_ui_text(PropertyRNA *prop, const char *name, const char *description)
{
  DESCR_CHECK(description, prop->identifier, NULL);

  prop->name = name;
  prop->description = description;
}

void RNA_def_property_ui_icon(PropertyRNA *prop, int icon, int consecutive)
{
  prop->icon = icon;
  if (consecutive != 0) {
    prop->flag |= PROP_ICONS_CONSECUTIVE;
  }
  if (consecutive < 0) {
    prop->flag |= PROP_ICONS_REVERSE;
  }
}

/**
 * The values hare are a little confusing:
 *
 * \param step: Used as the value to increase/decrease when clicking on number buttons,
 * as well as scaling mouse input for click-dragging number buttons.
 * For floats this is (step * UI_PRECISION_FLOAT_SCALE), why? - nobody knows.
 * For ints, whole values are used.
 *
 * \param precision: The number of zeros to show
 * (as a whole number - common range is 1 - 6), see UI_PRECISION_FLOAT_MAX
 */
void RNA_def_property_ui_range(
    PropertyRNA *prop, double min, double max, double step, int precision)
{
  StructRNA *srna = DefRNA.laststruct;

#ifndef NDEBUG
  if (min > max) {
    CLOG_ERROR(&LOG, "\"%s.%s\", min > max.", srna->identifier, prop->identifier);
    DefRNA.error = true;
  }

  if (step < 0 || step > 100) {
    CLOG_ERROR(&LOG, "\"%s.%s\", step outside range.", srna->identifier, prop->identifier);
    DefRNA.error = true;
  }

  if (step == 0) {
    CLOG_ERROR(&LOG, "\"%s.%s\", step is zero.", srna->identifier, prop->identifier);
    DefRNA.error = true;
  }

  if (precision < -1 || precision > UI_PRECISION_FLOAT_MAX) {
    CLOG_ERROR(&LOG, "\"%s.%s\", precision outside range.", srna->identifier, prop->identifier);
    DefRNA.error = true;
  }
#endif

  switch (prop->type) {
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
      iprop->softmin = (int)min;
      iprop->softmax = (int)max;
      iprop->step = (int)step;
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
      fprop->softmin = (float)min;
      fprop->softmax = (float)max;
      fprop->step = (float)step;
      fprop->precision = (int)precision;
      break;
    }
    default:
      CLOG_ERROR(
          &LOG, "\"%s.%s\", invalid type for ui range.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_range(PropertyRNA *prop, double min, double max)
{
  StructRNA *srna = DefRNA.laststruct;

#ifdef DEBUG
  if (min > max) {
    CLOG_ERROR(&LOG, "\"%s.%s\", min > max.", srna->identifier, prop->identifier);
    DefRNA.error = true;
  }
#endif

  switch (prop->type) {
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
      iprop->hardmin = (int)min;
      iprop->hardmax = (int)max;
      iprop->softmin = MAX2((int)min, iprop->hardmin);
      iprop->softmax = MIN2((int)max, iprop->hardmax);
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
      fprop->hardmin = (float)min;
      fprop->hardmax = (float)max;
      fprop->softmin = MAX2((float)min, fprop->hardmin);
      fprop->softmax = MIN2((float)max, fprop->hardmax);
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", invalid type for range.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_struct_type(PropertyRNA *prop, const char *type)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    fprintf(stderr, "\"%s.%s\": only during preprocessing.", srna->identifier, prop->identifier);
    return;
  }

  switch (prop->type) {
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
      pprop->type = (StructRNA *)type;
      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
      cprop->item_type = (StructRNA *)type;
      break;
    }
    default:
      CLOG_ERROR(
          &LOG, "\"%s.%s\", invalid type for struct type.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_struct_runtime(PropertyRNA *prop, StructRNA *type)
{
  StructRNA *srna = DefRNA.laststruct;

  if (DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only at runtime.");
    return;
  }

  switch (prop->type) {
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;
      pprop->type = type;

      if (type && (type->flag & STRUCT_ID_REFCOUNT)) {
        prop->flag |= PROP_ID_REFCOUNT;
      }

      break;
    }
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
      cprop->item_type = type;
      break;
    }
    default:
      CLOG_ERROR(
          &LOG, "\"%s.%s\", invalid type for struct type.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_enum_native_type(PropertyRNA *prop, const char *native_enum_type)
{
  StructRNA *srna = DefRNA.laststruct;
  switch (prop->type) {
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      eprop->native_enum_type = native_enum_type;
      break;
    }
    default:
      CLOG_ERROR(
          &LOG, "\"%s.%s\", invalid type for struct type.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_enum_items(PropertyRNA *prop, const EnumPropertyItem *item)
{
  StructRNA *srna = DefRNA.laststruct;
  int i, defaultfound = 0;

  switch (prop->type) {
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      eprop->item = (EnumPropertyItem *)item;
      eprop->totitem = 0;
      for (i = 0; item[i].identifier; i++) {
        eprop->totitem++;

        if (item[i].identifier[0]) {
          /* Don't allow spaces in internal enum items (it's fine for Python ones). */
          if (DefRNA.preprocess && strstr(item[i].identifier, " ")) {
            CLOG_ERROR(&LOG,
                       "\"%s.%s\", enum identifiers must not contain spaces.",
                       srna->identifier,
                       prop->identifier);
            DefRNA.error = true;
            break;
          }
          else if (item[i].value == eprop->defaultvalue) {
            defaultfound = 1;
          }
        }
      }

      if (!defaultfound) {
        for (i = 0; item[i].identifier; i++) {
          if (item[i].identifier[0]) {
            eprop->defaultvalue = item[i].value;
            break;
          }
        }
      }

      break;
    }
    default:
      CLOG_ERROR(
          &LOG, "\"%s.%s\", invalid type for struct type.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_string_maxlength(PropertyRNA *prop, int maxlength)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
      sprop->maxlength = maxlength;
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not string.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_boolean_default(PropertyRNA *prop, bool value)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
      BLI_assert(ELEM(value, false, true));
#ifndef RNA_RUNTIME
      /* Default may be set from items. */
      if (bprop->defaultvalue) {
        CLOG_ERROR(&LOG, "\"%s.%s\", set from DNA.", srna->identifier, prop->identifier);
      }
#endif
      bprop->defaultvalue = value;
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not boolean.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_boolean_array_default(PropertyRNA *prop, const bool *array)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
      bprop->defaultarray = array;
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not boolean.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_int_default(PropertyRNA *prop, int value)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
#ifndef RNA_RUNTIME
      if (iprop->defaultvalue != 0) {
        CLOG_ERROR(&LOG, "\"%s.%s\", set from DNA.", srna->identifier, prop->identifier);
      }
#endif
      iprop->defaultvalue = value;
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not int.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_int_array_default(PropertyRNA *prop, const int *array)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
#ifndef RNA_RUNTIME
      if (iprop->defaultarray != NULL) {
        CLOG_ERROR(&LOG, "\"%s.%s\", set from DNA.", srna->identifier, prop->identifier);
      }
#endif
      iprop->defaultarray = array;
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not int.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_float_default(PropertyRNA *prop, float value)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
#ifndef RNA_RUNTIME
      if (fprop->defaultvalue != 0) {
        CLOG_ERROR(&LOG, "\"%s.%s\", set from DNA.", srna->identifier, prop->identifier);
      }
#endif
      fprop->defaultvalue = value;
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not float.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}
/* array must remain valid after this function finishes */
void RNA_def_property_float_array_default(PropertyRNA *prop, const float *array)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
#ifndef RNA_RUNTIME
      if (fprop->defaultarray != NULL) {
        CLOG_ERROR(&LOG, "\"%s.%s\", set from DNA.", srna->identifier, prop->identifier);
      }
#endif
      fprop->defaultarray = array; /* WARNING, this array must not come from the stack and lost */
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not float.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_string_default(PropertyRNA *prop, const char *value)
{
  StructRNA *srna = DefRNA.laststruct;

  switch (prop->type) {
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;

      if (value == NULL) {
        CLOG_ERROR(&LOG,
                   "\"%s.%s\", NULL string passed (dont call in this case).",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
        break;
      }

      if (!value[0]) {
        CLOG_ERROR(&LOG,
                   "\"%s.%s\", empty string passed (dont call in this case).",
                   srna->identifier,
                   prop->identifier);
        DefRNA.error = true;
        // BLI_assert(0);
        break;
      }
#ifndef RNA_RUNTIME
      if (sprop->defaultvalue != NULL && sprop->defaultvalue[0]) {
        CLOG_ERROR(&LOG, "\"%s.%s\", set from DNA.", srna->identifier, prop->identifier);
      }
#endif
      sprop->defaultvalue = value;
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not string.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_enum_default(PropertyRNA *prop, int value)
{
  StructRNA *srna = DefRNA.laststruct;
  int i, defaultfound = 0;

  switch (prop->type) {
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
      eprop->defaultvalue = value;

      if (prop->flag & PROP_ENUM_FLAG) {
        /* check all bits are accounted for */
        int totflag = 0;
        for (i = 0; i < eprop->totitem; i++) {
          if (eprop->item[i].identifier[0]) {
            totflag |= eprop->item[i].value;
          }
        }

        if (eprop->defaultvalue & ~totflag) {
          CLOG_ERROR(&LOG,
                     "\"%s.%s\", default includes unused bits (%d).",
                     srna->identifier,
                     prop->identifier,
                     eprop->defaultvalue & ~totflag);
          DefRNA.error = true;
        }
      }
      else {
        for (i = 0; i < eprop->totitem; i++) {
          if (eprop->item[i].identifier[0] && eprop->item[i].value == eprop->defaultvalue) {
            defaultfound = 1;
          }
        }

        if (!defaultfound && eprop->totitem) {
          if (value == 0) {
            eprop->defaultvalue = eprop->item[0].value;
          }
          else {
            CLOG_ERROR(
                &LOG, "\"%s.%s\", default is not in items.", srna->identifier, prop->identifier);
            DefRNA.error = true;
          }
        }
      }

      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not enum.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

/* SDNA */

static PropertyDefRNA *rna_def_property_sdna(PropertyRNA *prop,
                                             const char *structname,
                                             const char *propname)
{
  DNAStructMember smember;
  StructDefRNA *ds;
  PropertyDefRNA *dp;

  dp = rna_find_struct_property_def(DefRNA.laststruct, prop);
  if (dp == NULL) {
    return NULL;
  }

  ds = rna_find_struct_def((StructRNA *)dp->cont);

  if (!structname) {
    structname = ds->dnaname;
  }
  if (!propname) {
    propname = prop->identifier;
  }

  int dnaoffset = 0;
  if (!rna_find_sdna_member(DefRNA.sdna, structname, propname, &smember, &dnaoffset)) {
    if (DefRNA.silent) {
      return NULL;
    }
    else if (!DefRNA.verify) {
      /* some basic values to survive even with sdna info */
      dp->dnastructname = structname;
      dp->dnaname = propname;
      if (prop->type == PROP_BOOLEAN) {
        dp->dnaarraylength = 1;
      }
      if (prop->type == PROP_POINTER) {
        dp->dnapointerlevel = 1;
      }
      dp->dnaoffset = smember.offset;
      return dp;
    }
    else {
      CLOG_ERROR(&LOG,
                 "\"%s.%s\" (identifier \"%s\") not found. Struct must be in DNA.",
                 structname,
                 propname,
                 prop->identifier);
      DefRNA.error = true;
      return NULL;
    }
  }

  if (smember.arraylength > 1) {
    prop->arraylength[0] = smember.arraylength;
    prop->totarraylength = smember.arraylength;
    prop->arraydimension = 1;
  }
  else {
    prop->arraydimension = 0;
    prop->totarraylength = 0;
  }

  dp->dnastructname = structname;
  dp->dnastructfromname = ds->dnafromname;
  dp->dnastructfromprop = ds->dnafromprop;
  dp->dnaname = propname;
  dp->dnatype = smember.type;
  dp->dnaarraylength = smember.arraylength;
  dp->dnapointerlevel = smember.pointerlevel;
  dp->dnaoffset = smember.offset;
  dp->dnasize = smember.size;

  return dp;
}

void RNA_def_property_boolean_sdna(PropertyRNA *prop,
                                   const char *structname,
                                   const char *propname,
                                   int64_t bit)
{
  PropertyDefRNA *dp;
  BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (prop->type != PROP_BOOLEAN) {
    CLOG_ERROR(&LOG, "\"%s.%s\", type is not boolean.", srna->identifier, prop->identifier);
    DefRNA.error = true;
    return;
  }

  if ((dp = rna_def_property_sdna(prop, structname, propname))) {

    if (!DefRNA.silent) {
      /* error check to ensure floats are not wrapped as ints/bools */
      if (dp->dnatype && *dp->dnatype && IS_DNATYPE_BOOLEAN_COMPAT(dp->dnatype) == 0) {
        CLOG_ERROR(&LOG,
                   "%s.%s is a '%s' but wrapped as type '%s'.",
                   srna->identifier,
                   prop->identifier,
                   dp->dnatype,
                   RNA_property_typename(prop->type));
        DefRNA.error = true;
        return;
      }
    }

    dp->booleanbit = bit;

#ifndef RNA_RUNTIME
    /* Set the default if possible. */
    if (dp->dnaoffset != -1) {
      int SDNAnr = DNA_struct_find_nr_wrapper(DefRNA.sdna, dp->dnastructname);
      if (SDNAnr != -1) {
        const void *default_data = DNA_default_table[SDNAnr];
        if (default_data) {
          default_data = POINTER_OFFSET(default_data, dp->dnaoffset);
          bool has_default = true;
          if (prop->totarraylength > 0) {
            has_default = false;
            if (debugSRNA_defaults) {
              fprintf(stderr, "%s default: unsupported boolean array default\n", __func__);
            }
          }
          else {
            if (STREQ(dp->dnatype, "char")) {
              bprop->defaultvalue = *(const char *)default_data & bit;
            }
            else if (STREQ(dp->dnatype, "short")) {
              bprop->defaultvalue = *(const short *)default_data & bit;
            }
            else if (STREQ(dp->dnatype, "int")) {
              bprop->defaultvalue = *(const int *)default_data & bit;
            }
            else {
              has_default = false;
              if (debugSRNA_defaults) {
                fprintf(
                    stderr, "%s default: unsupported boolean type (%s)\n", __func__, dp->dnatype);
              }
            }

            if (has_default) {
              if (dp->booleannegative) {
                bprop->defaultvalue = !bprop->defaultvalue;
              }

              if (debugSRNA_defaults) {
                fprintf(stderr, "value=%d, ", bprop->defaultvalue);
                print_defult_info(dp);
              }
            }
          }
        }
      }
    }
#else
    UNUSED_VARS(bprop);
#endif
  }
}

void RNA_def_property_boolean_negative_sdna(PropertyRNA *prop,
                                            const char *structname,
                                            const char *propname,
                                            int64_t booleanbit)
{
  PropertyDefRNA *dp;

  RNA_def_property_boolean_sdna(prop, structname, propname, booleanbit);

  dp = rna_find_struct_property_def(DefRNA.laststruct, prop);

  if (dp) {
    dp->booleannegative = true;
  }
}

void RNA_def_property_int_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
  PropertyDefRNA *dp;
  IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (prop->type != PROP_INT) {
    CLOG_ERROR(&LOG, "\"%s.%s\", type is not int.", srna->identifier, prop->identifier);
    DefRNA.error = true;
    return;
  }

  if ((dp = rna_def_property_sdna(prop, structname, propname))) {

    /* error check to ensure floats are not wrapped as ints/bools */
    if (!DefRNA.silent) {
      if (dp->dnatype && *dp->dnatype && IS_DNATYPE_INT_COMPAT(dp->dnatype) == 0) {
        CLOG_ERROR(&LOG,
                   "%s.%s is a '%s' but wrapped as type '%s'.",
                   srna->identifier,
                   prop->identifier,
                   dp->dnatype,
                   RNA_property_typename(prop->type));
        DefRNA.error = true;
        return;
      }
    }

    /* SDNA doesn't pass us unsigned unfortunately .. */
    if (dp->dnatype && STREQ(dp->dnatype, "char")) {
      iprop->hardmin = iprop->softmin = CHAR_MIN;
      iprop->hardmax = iprop->softmax = CHAR_MAX;
    }
    else if (dp->dnatype && STREQ(dp->dnatype, "short")) {
      iprop->hardmin = iprop->softmin = SHRT_MIN;
      iprop->hardmax = iprop->softmax = SHRT_MAX;
    }
    else if (dp->dnatype && STREQ(dp->dnatype, "int")) {
      iprop->hardmin = INT_MIN;
      iprop->hardmax = INT_MAX;

      iprop->softmin = -10000; /* rather arbitrary .. */
      iprop->softmax = 10000;
    }

    if (prop->subtype == PROP_UNSIGNED || prop->subtype == PROP_PERCENTAGE ||
        prop->subtype == PROP_FACTOR) {
      iprop->hardmin = iprop->softmin = 0;
    }

#ifndef RNA_RUNTIME
    /* Set the default if possible. */
    if (dp->dnaoffset != -1) {
      int SDNAnr = DNA_struct_find_nr_wrapper(DefRNA.sdna, dp->dnastructname);
      if (SDNAnr != -1) {
        const void *default_data = DNA_default_table[SDNAnr];
        if (default_data) {
          default_data = POINTER_OFFSET(default_data, dp->dnaoffset);
          /* NOTE: Currently doesn't store sign, assume chars are unsigned because
           * we build with this enabled, otherwise check 'PROP_UNSIGNED'. */
          bool has_default = true;
          if (prop->totarraylength > 0) {
            const void *default_data_end = POINTER_OFFSET(default_data, dp->dnasize);
            const int size_final = sizeof(int) * prop->totarraylength;
            if (STREQ(dp->dnatype, "char")) {
              int *defaultarray = rna_calloc(size_final);
              for (int i = 0; i < prop->totarraylength && default_data < default_data_end; i++) {
                defaultarray[i] = *(const char *)default_data;
                default_data = POINTER_OFFSET(default_data, sizeof(char));
              }
              iprop->defaultarray = defaultarray;
            }
            else if (STREQ(dp->dnatype, "short")) {

              int *defaultarray = rna_calloc(size_final);
              for (int i = 0; i < prop->totarraylength && default_data < default_data_end; i++) {
                defaultarray[i] = (prop->subtype != PROP_UNSIGNED) ? *(const short *)default_data :
                                                                     *(const ushort *)default_data;
                default_data = POINTER_OFFSET(default_data, sizeof(short));
              }
              iprop->defaultarray = defaultarray;
            }
            else if (STREQ(dp->dnatype, "int")) {
              int *defaultarray = rna_calloc(size_final);
              memcpy(defaultarray, default_data, MIN2(size_final, dp->dnasize));
              iprop->defaultarray = defaultarray;
            }
            else {
              has_default = false;
              if (debugSRNA_defaults) {
                fprintf(stderr,
                        "%s default: unsupported int array type (%s)\n",
                        __func__,
                        dp->dnatype);
              }
            }

            if (has_default) {
              if (debugSRNA_defaults) {
                fprintf(stderr, "value=(");
                for (int i = 0; i < prop->totarraylength; i++) {
                  fprintf(stderr, "%d, ", iprop->defaultarray[i]);
                }
                fprintf(stderr, "), ");
                print_defult_info(dp);
              }
            }
          }
          else {
            if (STREQ(dp->dnatype, "char")) {
              iprop->defaultvalue = *(const char *)default_data;
            }
            else if (STREQ(dp->dnatype, "short")) {
              iprop->defaultvalue = (prop->subtype != PROP_UNSIGNED) ?
                                        *(const short *)default_data :
                                        *(const ushort *)default_data;
            }
            else if (STREQ(dp->dnatype, "int")) {
              iprop->defaultvalue = (prop->subtype != PROP_UNSIGNED) ? *(const int *)default_data :
                                                                       *(const uint *)default_data;
            }
            else {
              has_default = false;
              if (debugSRNA_defaults) {
                fprintf(stderr, "%s default: unsupported int type (%s)\n", __func__, dp->dnatype);
              }
            }

            if (has_default) {
              if (debugSRNA_defaults) {
                fprintf(stderr, "value=%d, ", iprop->defaultvalue);
                print_defult_info(dp);
              }
            }
          }
        }
      }
    }
#endif
  }
}

void RNA_def_property_float_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
  PropertyDefRNA *dp;
  FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (prop->type != PROP_FLOAT) {
    CLOG_ERROR(&LOG, "\"%s.%s\", type is not float.", srna->identifier, prop->identifier);
    DefRNA.error = true;
    return;
  }

  if ((dp = rna_def_property_sdna(prop, structname, propname))) {
    /* silent is for internal use */
    if (!DefRNA.silent) {
      if (dp->dnatype && *dp->dnatype && IS_DNATYPE_FLOAT_COMPAT(dp->dnatype) == 0) {
        /* Colors are an exception. these get translated. */
        if (prop->subtype != PROP_COLOR_GAMMA) {
          CLOG_ERROR(&LOG,
                     "%s.%s is a '%s' but wrapped as type '%s'.",
                     srna->identifier,
                     prop->identifier,
                     dp->dnatype,
                     RNA_property_typename(prop->type));
          DefRNA.error = true;
          return;
        }
      }
    }

    if (dp->dnatype && STREQ(dp->dnatype, "char")) {
      fprop->hardmin = fprop->softmin = 0.0f;
      fprop->hardmax = fprop->softmax = 1.0f;
    }

#ifndef RNA_RUNTIME
    /* Set the default if possible. */
    if (dp->dnaoffset != -1) {
      int SDNAnr = DNA_struct_find_nr_wrapper(DefRNA.sdna, dp->dnastructname);
      if (SDNAnr != -1) {
        const void *default_data = DNA_default_table[SDNAnr];
        if (default_data) {
          default_data = POINTER_OFFSET(default_data, dp->dnaoffset);
          bool has_default = true;
          if (prop->totarraylength > 0) {
            if (STREQ(dp->dnatype, "float")) {
              const int size_final = sizeof(float) * prop->totarraylength;
              float *defaultarray = rna_calloc(size_final);
              memcpy(defaultarray, default_data, MIN2(size_final, dp->dnasize));
              fprop->defaultarray = defaultarray;
            }
            else {
              has_default = false;
              if (debugSRNA_defaults) {
                fprintf(stderr,
                        "%s default: unsupported float array type (%s)\n",
                        __func__,
                        dp->dnatype);
              }
            }

            if (has_default) {
              if (debugSRNA_defaults) {
                fprintf(stderr, "value=(");
                for (int i = 0; i < prop->totarraylength; i++) {
                  fprintf(stderr, "%g, ", fprop->defaultarray[i]);
                }
                fprintf(stderr, "), ");
                print_defult_info(dp);
              }
            }
          }
          else {
            if (STREQ(dp->dnatype, "float")) {
              fprop->defaultvalue = *(const float *)default_data;
            }
            else if (STREQ(dp->dnatype, "char")) {
              fprop->defaultvalue = (float)*(const char *)default_data * (1.0f / 255.0f);
            }
            else {
              has_default = false;
              if (debugSRNA_defaults) {
                fprintf(
                    stderr, "%s default: unsupported float type (%s)\n", __func__, dp->dnatype);
              }
            }

            if (has_default) {
              if (debugSRNA_defaults) {
                fprintf(stderr, "value=%g, ", fprop->defaultvalue);
                print_defult_info(dp);
              }
            }
          }
        }
      }
    }
#endif
  }

  rna_def_property_sdna(prop, structname, propname);
}

void RNA_def_property_enum_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
  PropertyDefRNA *dp;
  EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (prop->type != PROP_ENUM) {
    CLOG_ERROR(&LOG, "\"%s.%s\", type is not enum.", srna->identifier, prop->identifier);
    DefRNA.error = true;
    return;
  }

  if ((dp = rna_def_property_sdna(prop, structname, propname))) {
    if (prop->arraydimension) {
      prop->arraydimension = 0;
      prop->totarraylength = 0;

      if (!DefRNA.silent) {
        CLOG_ERROR(&LOG, "\"%s.%s\", array not supported for enum type.", structname, propname);
        DefRNA.error = true;
      }
    }

#ifndef RNA_RUNTIME
    /* Set the default if possible. */
    if (dp->dnaoffset != -1) {
      int SDNAnr = DNA_struct_find_nr_wrapper(DefRNA.sdna, dp->dnastructname);
      if (SDNAnr != -1) {
        const void *default_data = DNA_default_table[SDNAnr];
        if (default_data) {
          default_data = POINTER_OFFSET(default_data, dp->dnaoffset);
          bool has_default = true;
          if (STREQ(dp->dnatype, "char")) {
            eprop->defaultvalue = *(const char *)default_data;
          }
          else if (STREQ(dp->dnatype, "short")) {
            eprop->defaultvalue = *(const short *)default_data;
          }
          else if (STREQ(dp->dnatype, "int")) {
            eprop->defaultvalue = *(const int *)default_data;
          }
          else {
            has_default = false;
            if (debugSRNA_defaults) {
              fprintf(stderr, "%s default: unsupported enum type (%s)\n", __func__, dp->dnatype);
            }
          }

          if (has_default) {
            if (debugSRNA_defaults) {
              fprintf(stderr, "value=%d, ", eprop->defaultvalue);
              print_defult_info(dp);
            }
          }
        }
      }
    }
#else
    UNUSED_VARS(eprop);
#endif
  }
}

void RNA_def_property_enum_bitflag_sdna(PropertyRNA *prop,
                                        const char *structname,
                                        const char *propname)
{
  PropertyDefRNA *dp;

  RNA_def_property_enum_sdna(prop, structname, propname);

  dp = rna_find_struct_property_def(DefRNA.laststruct, prop);

  if (dp) {
    dp->enumbitflags = 1;

#ifndef RNA_RUNTIME
    int defaultvalue_mask = 0;
    EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
    for (int i = 0; i < eprop->totitem; i++) {
      if (eprop->item[i].identifier[0]) {
        defaultvalue_mask |= eprop->defaultvalue & eprop->item[i].value;
      }
    }
    eprop->defaultvalue = defaultvalue_mask;
#endif
  }
}

void RNA_def_property_string_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
  PropertyDefRNA *dp;
  StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (prop->type != PROP_STRING) {
    CLOG_ERROR(&LOG, "\"%s.%s\", type is not string.", srna->identifier, prop->identifier);
    DefRNA.error = true;
    return;
  }

  if ((dp = rna_def_property_sdna(prop, structname, propname))) {
    if (prop->arraydimension) {
      sprop->maxlength = prop->totarraylength;
      prop->arraydimension = 0;
      prop->totarraylength = 0;
    }

#ifndef RNA_RUNTIME
    /* Set the default if possible. */
    if ((dp->dnaoffset != -1) && (dp->dnapointerlevel != 0)) {
      int SDNAnr = DNA_struct_find_nr_wrapper(DefRNA.sdna, dp->dnastructname);
      if (SDNAnr != -1) {
        const void *default_data = DNA_default_table[SDNAnr];
        if (default_data) {
          default_data = POINTER_OFFSET(default_data, dp->dnaoffset);
          sprop->defaultvalue = default_data;

          if (debugSRNA_defaults) {
            fprintf(stderr, "value=\"%s\", ", sprop->defaultvalue);
            print_defult_info(dp);
          }
        }
      }
    }
#endif
  }
}

void RNA_def_property_pointer_sdna(PropertyRNA *prop, const char *structname, const char *propname)
{
  /* PropertyDefRNA *dp; */
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (prop->type != PROP_POINTER) {
    CLOG_ERROR(&LOG, "\"%s.%s\", type is not pointer.", srna->identifier, prop->identifier);
    DefRNA.error = true;
    return;
  }

  if ((/* dp= */ rna_def_property_sdna(prop, structname, propname))) {
    if (prop->arraydimension) {
      prop->arraydimension = 0;
      prop->totarraylength = 0;

      if (!DefRNA.silent) {
        CLOG_ERROR(&LOG, "\"%s.%s\", array not supported for pointer type.", structname, propname);
        DefRNA.error = true;
      }
    }
  }
}

void RNA_def_property_collection_sdna(PropertyRNA *prop,
                                      const char *structname,
                                      const char *propname,
                                      const char *lengthpropname)
{
  PropertyDefRNA *dp;
  CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (prop->type != PROP_COLLECTION) {
    CLOG_ERROR(&LOG, "\"%s.%s\", type is not collection.", srna->identifier, prop->identifier);
    DefRNA.error = true;
    return;
  }

  if ((dp = rna_def_property_sdna(prop, structname, propname))) {
    if (prop->arraydimension && !lengthpropname) {
      prop->arraydimension = 0;
      prop->totarraylength = 0;

      if (!DefRNA.silent) {
        CLOG_ERROR(&LOG, "\"%s.%s\", array of collections not supported.", structname, propname);
        DefRNA.error = true;
      }
    }

    if (dp->dnatype && STREQ(dp->dnatype, "ListBase")) {
      cprop->next = (PropCollectionNextFunc) "rna_iterator_listbase_next";
      cprop->get = (PropCollectionGetFunc) "rna_iterator_listbase_get";
      cprop->end = (PropCollectionEndFunc) "rna_iterator_listbase_end";
    }
  }

  if (dp && lengthpropname) {
    DNAStructMember smember;
    StructDefRNA *ds = rna_find_struct_def((StructRNA *)dp->cont);

    if (!structname) {
      structname = ds->dnaname;
    }

    int dnaoffset = 0;
    if (lengthpropname[0] == 0 ||
        rna_find_sdna_member(DefRNA.sdna, structname, lengthpropname, &smember, &dnaoffset)) {
      if (lengthpropname[0] == 0) {
        dp->dnalengthfixed = prop->totarraylength;
        prop->arraydimension = 0;
        prop->totarraylength = 0;
      }
      else {
        dp->dnalengthstructname = structname;
        dp->dnalengthname = lengthpropname;
        prop->totarraylength = 0;
      }

      cprop->next = (PropCollectionNextFunc) "rna_iterator_array_next";
      cprop->end = (PropCollectionEndFunc) "rna_iterator_array_end";

      if (dp->dnapointerlevel >= 2) {
        cprop->get = (PropCollectionGetFunc) "rna_iterator_array_dereference_get";
      }
      else {
        cprop->get = (PropCollectionGetFunc) "rna_iterator_array_get";
      }
    }
    else {
      if (!DefRNA.silent) {
        CLOG_ERROR(&LOG, "\"%s.%s\" not found.", structname, lengthpropname);
        DefRNA.error = true;
      }
    }
  }
}

void RNA_def_property_translation_context(PropertyRNA *prop, const char *context)
{
  prop->translation_context = context ? context : BLT_I18NCONTEXT_DEFAULT_BPYRNA;
}

/* Functions */

void RNA_def_property_editable_func(PropertyRNA *prop, const char *editable)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (editable) {
    prop->editable = (EditableFunc)editable;
  }
}

void RNA_def_property_editable_array_func(PropertyRNA *prop, const char *editable)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (editable) {
    prop->itemeditable = (ItemEditableFunc)editable;
  }
}

/**
 * Set custom callbacks for override operations handling.
 *
 * \note \a diff callback will also be used by RNA comparison/equality functions.
 */
void RNA_def_property_override_funcs(PropertyRNA *prop,
                                     const char *diff,
                                     const char *store,
                                     const char *apply)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (diff) {
    prop->override_diff = (RNAPropOverrideDiff)diff;
  }
  if (store) {
    prop->override_store = (RNAPropOverrideStore)store;
  }
  if (apply) {
    prop->override_apply = (RNAPropOverrideApply)apply;
  }
}

void RNA_def_property_update(PropertyRNA *prop, int noteflag, const char *func)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  prop->noteflag = noteflag;
  prop->update = (UpdateFunc)func;
}

void RNA_def_property_update_runtime(PropertyRNA *prop, const void *func)
{
  prop->update = (void *)func;
}

void RNA_def_property_poll_runtime(PropertyRNA *prop, const void *func)
{
  if (prop->type == PROP_POINTER) {
    ((PointerPropertyRNA *)prop)->poll = (void *)func;
  }
  else {
    CLOG_ERROR(&LOG, "%s is not a Pointer Property.", prop->identifier);
  }
}

void RNA_def_property_dynamic_array_funcs(PropertyRNA *prop, const char *getlength)
{
  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  if (!(prop->flag & PROP_DYNAMIC)) {
    CLOG_ERROR(&LOG, "property is a not dynamic array.");
    DefRNA.error = true;
    return;
  }

  if (getlength) {
    prop->getlength = (PropArrayLengthGetFunc)getlength;
  }
}

void RNA_def_property_boolean_funcs(PropertyRNA *prop, const char *get, const char *set)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;

      if (prop->arraydimension) {
        if (get) {
          bprop->getarray = (PropBooleanArrayGetFunc)get;
        }
        if (set) {
          bprop->setarray = (PropBooleanArraySetFunc)set;
        }
      }
      else {
        if (get) {
          bprop->get = (PropBooleanGetFunc)get;
        }
        if (set) {
          bprop->set = (PropBooleanSetFunc)set;
        }
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not boolean.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_boolean_funcs_runtime(PropertyRNA *prop,
                                            BooleanPropertyGetFunc getfunc,
                                            BooleanPropertySetFunc setfunc)
{
  BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;

  if (getfunc) {
    bprop->get_ex = getfunc;
  }
  if (setfunc) {
    bprop->set_ex = setfunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_boolean_array_funcs_runtime(PropertyRNA *prop,
                                                  BooleanArrayPropertyGetFunc getfunc,
                                                  BooleanArrayPropertySetFunc setfunc)
{
  BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;

  if (getfunc) {
    bprop->getarray_ex = getfunc;
  }
  if (setfunc) {
    bprop->setarray_ex = setfunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_int_funcs(PropertyRNA *prop,
                                const char *get,
                                const char *set,
                                const char *range)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  switch (prop->type) {
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

      if (prop->arraydimension) {
        if (get) {
          iprop->getarray = (PropIntArrayGetFunc)get;
        }
        if (set) {
          iprop->setarray = (PropIntArraySetFunc)set;
        }
      }
      else {
        if (get) {
          iprop->get = (PropIntGetFunc)get;
        }
        if (set) {
          iprop->set = (PropIntSetFunc)set;
        }
      }
      if (range) {
        iprop->range = (PropIntRangeFunc)range;
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not int.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_int_funcs_runtime(PropertyRNA *prop,
                                        IntPropertyGetFunc getfunc,
                                        IntPropertySetFunc setfunc,
                                        IntPropertyRangeFunc rangefunc)
{
  IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

  if (getfunc) {
    iprop->get_ex = getfunc;
  }
  if (setfunc) {
    iprop->set_ex = setfunc;
  }
  if (rangefunc) {
    iprop->range_ex = rangefunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_int_array_funcs_runtime(PropertyRNA *prop,
                                              IntArrayPropertyGetFunc getfunc,
                                              IntArrayPropertySetFunc setfunc,
                                              IntPropertyRangeFunc rangefunc)
{
  IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

  if (getfunc) {
    iprop->getarray_ex = getfunc;
  }
  if (setfunc) {
    iprop->setarray_ex = setfunc;
  }
  if (rangefunc) {
    iprop->range_ex = rangefunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_float_funcs(PropertyRNA *prop,
                                  const char *get,
                                  const char *set,
                                  const char *range)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  switch (prop->type) {
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

      if (prop->arraydimension) {
        if (get) {
          fprop->getarray = (PropFloatArrayGetFunc)get;
        }
        if (set) {
          fprop->setarray = (PropFloatArraySetFunc)set;
        }
      }
      else {
        if (get) {
          fprop->get = (PropFloatGetFunc)get;
        }
        if (set) {
          fprop->set = (PropFloatSetFunc)set;
        }
      }
      if (range) {
        fprop->range = (PropFloatRangeFunc)range;
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not float.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_float_funcs_runtime(PropertyRNA *prop,
                                          FloatPropertyGetFunc getfunc,
                                          FloatPropertySetFunc setfunc,
                                          FloatPropertyRangeFunc rangefunc)
{
  FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

  if (getfunc) {
    fprop->get_ex = getfunc;
  }
  if (setfunc) {
    fprop->set_ex = setfunc;
  }
  if (rangefunc) {
    fprop->range_ex = rangefunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_float_array_funcs_runtime(PropertyRNA *prop,
                                                FloatArrayPropertyGetFunc getfunc,
                                                FloatArrayPropertySetFunc setfunc,
                                                FloatPropertyRangeFunc rangefunc)
{
  FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

  if (getfunc) {
    fprop->getarray_ex = getfunc;
  }
  if (setfunc) {
    fprop->setarray_ex = setfunc;
  }
  if (rangefunc) {
    fprop->range_ex = rangefunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_enum_funcs(PropertyRNA *prop,
                                 const char *get,
                                 const char *set,
                                 const char *item)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  switch (prop->type) {
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;

      if (get) {
        eprop->get = (PropEnumGetFunc)get;
      }
      if (set) {
        eprop->set = (PropEnumSetFunc)set;
      }
      if (item) {
        eprop->itemf = (PropEnumItemFunc)item;
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not enum.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_enum_funcs_runtime(PropertyRNA *prop,
                                         EnumPropertyGetFunc getfunc,
                                         EnumPropertySetFunc setfunc,
                                         EnumPropertyItemFunc itemfunc)
{
  EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;

  if (getfunc) {
    eprop->get_ex = getfunc;
  }
  if (setfunc) {
    eprop->set_ex = setfunc;
  }
  if (itemfunc) {
    eprop->itemf = itemfunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_enum_py_data(PropertyRNA *prop, void *py_data)
{
  EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
  eprop->py_data = py_data;
}

void RNA_def_property_string_funcs(PropertyRNA *prop,
                                   const char *get,
                                   const char *length,
                                   const char *set)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  switch (prop->type) {
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;

      if (get) {
        sprop->get = (PropStringGetFunc)get;
      }
      if (length) {
        sprop->length = (PropStringLengthFunc)length;
      }
      if (set) {
        sprop->set = (PropStringSetFunc)set;
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not string.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_string_funcs_runtime(PropertyRNA *prop,
                                           StringPropertyGetFunc getfunc,
                                           StringPropertyLengthFunc lengthfunc,
                                           StringPropertySetFunc setfunc)
{
  StringPropertyRNA *sprop = (StringPropertyRNA *)prop;

  if (getfunc) {
    sprop->get_ex = getfunc;
  }
  if (lengthfunc) {
    sprop->length_ex = lengthfunc;
  }
  if (setfunc) {
    sprop->set_ex = setfunc;
  }

  if (getfunc || setfunc) {
    /* don't save in id properties */
    prop->flag &= ~PROP_IDPROPERTY;

    if (!setfunc) {
      prop->flag &= ~PROP_EDITABLE;
    }
  }
}

void RNA_def_property_pointer_funcs(
    PropertyRNA *prop, const char *get, const char *set, const char *typef, const char *poll)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  switch (prop->type) {
    case PROP_POINTER: {
      PointerPropertyRNA *pprop = (PointerPropertyRNA *)prop;

      if (get) {
        pprop->get = (PropPointerGetFunc)get;
      }
      if (set) {
        pprop->set = (PropPointerSetFunc)set;
      }
      if (typef) {
        pprop->typef = (PropPointerTypeFunc)typef;
      }
      if (poll) {
        pprop->poll = (PropPointerPollFunc)poll;
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not pointer.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_collection_funcs(PropertyRNA *prop,
                                       const char *begin,
                                       const char *next,
                                       const char *end,
                                       const char *get,
                                       const char *length,
                                       const char *lookupint,
                                       const char *lookupstring,
                                       const char *assignint)
{
  StructRNA *srna = DefRNA.laststruct;

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only during preprocessing.");
    return;
  }

  switch (prop->type) {
    case PROP_COLLECTION: {
      CollectionPropertyRNA *cprop = (CollectionPropertyRNA *)prop;

      if (begin) {
        cprop->begin = (PropCollectionBeginFunc)begin;
      }
      if (next) {
        cprop->next = (PropCollectionNextFunc)next;
      }
      if (end) {
        cprop->end = (PropCollectionEndFunc)end;
      }
      if (get) {
        cprop->get = (PropCollectionGetFunc)get;
      }
      if (length) {
        cprop->length = (PropCollectionLengthFunc)length;
      }
      if (lookupint) {
        cprop->lookupint = (PropCollectionLookupIntFunc)lookupint;
      }
      if (lookupstring) {
        cprop->lookupstring = (PropCollectionLookupStringFunc)lookupstring;
      }
      if (assignint) {
        cprop->assignint = (PropCollectionAssignIntFunc)assignint;
      }
      break;
    }
    default:
      CLOG_ERROR(&LOG, "\"%s.%s\", type is not collection.", srna->identifier, prop->identifier);
      DefRNA.error = true;
      break;
  }
}

void RNA_def_property_srna(PropertyRNA *prop, const char *type)
{
  prop->srna = (StructRNA *)type;
}

void RNA_def_py_data(PropertyRNA *prop, void *py_data)
{
  prop->py_data = py_data;
}

/* Compact definitions */

PropertyRNA *RNA_def_boolean(StructOrFunctionRNA *cont_,
                             const char *identifier,
                             bool default_value,
                             const char *ui_name,
                             const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_default(prop, default_value);
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_boolean_array(StructOrFunctionRNA *cont_,
                                   const char *identifier,
                                   int len,
                                   bool *default_value,
                                   const char *ui_name,
                                   const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_BOOLEAN, PROP_NONE);
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_boolean_array_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_boolean_layer(StructOrFunctionRNA *cont_,
                                   const char *identifier,
                                   int len,
                                   bool *default_value,
                                   const char *ui_name,
                                   const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_BOOLEAN, PROP_LAYER);
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_boolean_array_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_boolean_layer_member(StructOrFunctionRNA *cont_,
                                          const char *identifier,
                                          int len,
                                          bool *default_value,
                                          const char *ui_name,
                                          const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_BOOLEAN, PROP_LAYER_MEMBER);
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_boolean_array_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_boolean_vector(StructOrFunctionRNA *cont_,
                                    const char *identifier,
                                    int len,
                                    bool *default_value,
                                    const char *ui_name,
                                    const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_BOOLEAN, PROP_XYZ); /* XXX */
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_boolean_array_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_int(StructOrFunctionRNA *cont_,
                         const char *identifier,
                         int default_value,
                         int hardmin,
                         int hardmax,
                         const char *ui_name,
                         const char *ui_description,
                         int softmin,
                         int softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_INT, PROP_NONE);
  RNA_def_property_int_default(prop, default_value);
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_int_vector(StructOrFunctionRNA *cont_,
                                const char *identifier,
                                int len,
                                const int *default_value,
                                int hardmin,
                                int hardmax,
                                const char *ui_name,
                                const char *ui_description,
                                int softmin,
                                int softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_INT, PROP_XYZ); /* XXX */
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_int_array_default(prop, default_value);
  }
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_int_array(StructOrFunctionRNA *cont_,
                               const char *identifier,
                               int len,
                               const int *default_value,
                               int hardmin,
                               int hardmax,
                               const char *ui_name,
                               const char *ui_description,
                               int softmin,
                               int softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_INT, PROP_NONE);
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_int_array_default(prop, default_value);
  }
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_string(StructOrFunctionRNA *cont_,
                            const char *identifier,
                            const char *default_value,
                            int maxlen,
                            const char *ui_name,
                            const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  BLI_assert(default_value == NULL || default_value[0]);

  prop = RNA_def_property(cont, identifier, PROP_STRING, PROP_NONE);
  if (maxlen != 0) {
    RNA_def_property_string_maxlength(prop, maxlen);
  }
  if (default_value) {
    RNA_def_property_string_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_string_file_path(StructOrFunctionRNA *cont_,
                                      const char *identifier,
                                      const char *default_value,
                                      int maxlen,
                                      const char *ui_name,
                                      const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  BLI_assert(default_value == NULL || default_value[0]);

  prop = RNA_def_property(cont, identifier, PROP_STRING, PROP_FILEPATH);
  if (maxlen != 0) {
    RNA_def_property_string_maxlength(prop, maxlen);
  }
  if (default_value) {
    RNA_def_property_string_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_string_dir_path(StructOrFunctionRNA *cont_,
                                     const char *identifier,
                                     const char *default_value,
                                     int maxlen,
                                     const char *ui_name,
                                     const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  BLI_assert(default_value == NULL || default_value[0]);

  prop = RNA_def_property(cont, identifier, PROP_STRING, PROP_DIRPATH);
  if (maxlen != 0) {
    RNA_def_property_string_maxlength(prop, maxlen);
  }
  if (default_value) {
    RNA_def_property_string_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_string_file_name(StructOrFunctionRNA *cont_,
                                      const char *identifier,
                                      const char *default_value,
                                      int maxlen,
                                      const char *ui_name,
                                      const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  BLI_assert(default_value == NULL || default_value[0]);

  prop = RNA_def_property(cont, identifier, PROP_STRING, PROP_FILENAME);
  if (maxlen != 0) {
    RNA_def_property_string_maxlength(prop, maxlen);
  }
  if (default_value) {
    RNA_def_property_string_default(prop, default_value);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_enum(StructOrFunctionRNA *cont_,
                          const char *identifier,
                          const EnumPropertyItem *items,
                          int default_value,
                          const char *ui_name,
                          const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  if (items == NULL) {
    CLOG_ERROR(&LOG, "items not allowed to be NULL.");
    return NULL;
  }

  prop = RNA_def_property(cont, identifier, PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, items);
  RNA_def_property_enum_default(prop, default_value);
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

/* same as above but sets 'PROP_ENUM_FLAG' before setting the default value */
PropertyRNA *RNA_def_enum_flag(StructOrFunctionRNA *cont_,
                               const char *identifier,
                               const EnumPropertyItem *items,
                               int default_value,
                               const char *ui_name,
                               const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  if (items == NULL) {
    CLOG_ERROR(&LOG, "items not allowed to be NULL.");
    return NULL;
  }

  prop = RNA_def_property(cont, identifier, PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG); /* important to run before default set */
  RNA_def_property_enum_items(prop, items);
  RNA_def_property_enum_default(prop, default_value);
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

void RNA_def_enum_funcs(PropertyRNA *prop, EnumPropertyItemFunc itemfunc)
{
  EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;
  eprop->itemf = itemfunc;
}

PropertyRNA *RNA_def_float(StructOrFunctionRNA *cont_,
                           const char *identifier,
                           float default_value,
                           float hardmin,
                           float hardmax,
                           const char *ui_name,
                           const char *ui_description,
                           float softmin,
                           float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_default(prop, default_value);
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_float_vector(StructOrFunctionRNA *cont_,
                                  const char *identifier,
                                  int len,
                                  const float *default_value,
                                  float hardmin,
                                  float hardmax,
                                  const char *ui_name,
                                  const char *ui_description,
                                  float softmin,
                                  float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, PROP_XYZ);
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_float_array_default(prop, default_value);
  }
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_float_vector_xyz(StructOrFunctionRNA *cont_,
                                      const char *identifier,
                                      int len,
                                      const float *default_value,
                                      float hardmin,
                                      float hardmax,
                                      const char *ui_name,
                                      const char *ui_description,
                                      float softmin,
                                      float softmax)
{
  PropertyRNA *prop;

  prop = RNA_def_float_vector(cont_,
                              identifier,
                              len,
                              default_value,
                              hardmin,
                              hardmax,
                              ui_name,
                              ui_description,
                              softmin,
                              softmax);
  prop->subtype = PROP_XYZ_LENGTH;

  return prop;
}

PropertyRNA *RNA_def_float_color(StructOrFunctionRNA *cont_,
                                 const char *identifier,
                                 int len,
                                 const float *default_value,
                                 float hardmin,
                                 float hardmax,
                                 const char *ui_name,
                                 const char *ui_description,
                                 float softmin,
                                 float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, PROP_COLOR);
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_float_array_default(prop, default_value);
  }
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_float_matrix(StructOrFunctionRNA *cont_,
                                  const char *identifier,
                                  int rows,
                                  int columns,
                                  const float *default_value,
                                  float hardmin,
                                  float hardmax,
                                  const char *ui_name,
                                  const char *ui_description,
                                  float softmin,
                                  float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;
  const int length[2] = {rows, columns};

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, length);
  if (default_value) {
    RNA_def_property_float_array_default(prop, default_value);
  }
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_float_translation(StructOrFunctionRNA *cont_,
                                       const char *identifier,
                                       int len,
                                       const float *default_value,
                                       float hardmin,
                                       float hardmax,
                                       const char *ui_name,
                                       const char *ui_description,
                                       float softmin,
                                       float softmax)
{
  PropertyRNA *prop;

  prop = RNA_def_float_vector(cont_,
                              identifier,
                              len,
                              default_value,
                              hardmin,
                              hardmax,
                              ui_name,
                              ui_description,
                              softmin,
                              softmax);
  prop->subtype = PROP_TRANSLATION;

  RNA_def_property_ui_range(prop, softmin, softmax, 1, RNA_TRANSLATION_PREC_DEFAULT);

  return prop;
}

PropertyRNA *RNA_def_float_rotation(StructOrFunctionRNA *cont_,
                                    const char *identifier,
                                    int len,
                                    const float *default_value,
                                    float hardmin,
                                    float hardmax,
                                    const char *ui_name,
                                    const char *ui_description,
                                    float softmin,
                                    float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, (len >= 3) ? PROP_EULER : PROP_ANGLE);
  if (len != 0) {
    RNA_def_property_array(prop, len);
    if (default_value) {
      RNA_def_property_float_array_default(prop, default_value);
    }
  }
  else {
    /* RNA_def_property_float_default must be called outside */
    BLI_assert(default_value == NULL);
  }
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 10, 3);

  return prop;
}

PropertyRNA *RNA_def_float_distance(StructOrFunctionRNA *cont_,
                                    const char *identifier,
                                    float default_value,
                                    float hardmin,
                                    float hardmax,
                                    const char *ui_name,
                                    const char *ui_description,
                                    float softmin,
                                    float softmax)
{
  PropertyRNA *prop = RNA_def_float(cont_,
                                    identifier,
                                    default_value,
                                    hardmin,
                                    hardmax,
                                    ui_name,
                                    ui_description,
                                    softmin,
                                    softmax);
  RNA_def_property_subtype(prop, PROP_DISTANCE);

  return prop;
}

PropertyRNA *RNA_def_float_array(StructOrFunctionRNA *cont_,
                                 const char *identifier,
                                 int len,
                                 const float *default_value,
                                 float hardmin,
                                 float hardmax,
                                 const char *ui_name,
                                 const char *ui_description,
                                 float softmin,
                                 float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, PROP_NONE);
  if (len != 0) {
    RNA_def_property_array(prop, len);
  }
  if (default_value) {
    RNA_def_property_float_array_default(prop, default_value);
  }
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_float_percentage(StructOrFunctionRNA *cont_,
                                      const char *identifier,
                                      float default_value,
                                      float hardmin,
                                      float hardmax,
                                      const char *ui_name,
                                      const char *ui_description,
                                      float softmin,
                                      float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_default(prop, default_value);
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_float_factor(StructOrFunctionRNA *cont_,
                                  const char *identifier,
                                  float default_value,
                                  float hardmin,
                                  float hardmax,
                                  const char *ui_name,
                                  const char *ui_description,
                                  float softmin,
                                  float softmax)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  ASSERT_SOFT_HARD_LIMITS;

  prop = RNA_def_property(cont, identifier, PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_default(prop, default_value);
  if (hardmin != hardmax) {
    RNA_def_property_range(prop, hardmin, hardmax);
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);
  RNA_def_property_ui_range(prop, softmin, softmax, 1, 3);

  return prop;
}

PropertyRNA *RNA_def_pointer(StructOrFunctionRNA *cont_,
                             const char *identifier,
                             const char *type,
                             const char *ui_name,
                             const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, type);
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_pointer_runtime(StructOrFunctionRNA *cont_,
                                     const char *identifier,
                                     StructRNA *type,
                                     const char *ui_name,
                                     const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_runtime(prop, type);
  if ((type->flag & STRUCT_ID) != 0) {
    prop->flag |= PROP_EDITABLE;
  }
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_collection(StructOrFunctionRNA *cont_,
                                const char *identifier,
                                const char *type,
                                const char *ui_name,
                                const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, type);
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

PropertyRNA *RNA_def_collection_runtime(StructOrFunctionRNA *cont_,
                                        const char *identifier,
                                        StructRNA *type,
                                        const char *ui_name,
                                        const char *ui_description)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  prop = RNA_def_property(cont, identifier, PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_runtime(prop, type);
  RNA_def_property_ui_text(prop, ui_name, ui_description);

  return prop;
}

/* Function */

static FunctionRNA *rna_def_function(StructRNA *srna, const char *identifier)
{
  FunctionRNA *func;
  StructDefRNA *dsrna;
  FunctionDefRNA *dfunc;

  if (DefRNA.preprocess) {
    char error[512];

    if (rna_validate_identifier(identifier, error, false) == 0) {
      CLOG_ERROR(&LOG, "function identifier \"%s\" - %s", identifier, error);
      DefRNA.error = true;
    }
  }

  func = MEM_callocN(sizeof(FunctionRNA), "FunctionRNA");
  func->identifier = identifier;
  func->description = identifier;

  rna_addtail(&srna->functions, func);

  if (DefRNA.preprocess) {
    dsrna = rna_find_struct_def(srna);
    dfunc = MEM_callocN(sizeof(FunctionDefRNA), "FunctionDefRNA");
    rna_addtail(&dsrna->functions, dfunc);
    dfunc->func = func;
  }
  else {
    func->flag |= FUNC_RUNTIME;
  }

  return func;
}

FunctionRNA *RNA_def_function(StructRNA *srna, const char *identifier, const char *call)
{
  FunctionRNA *func;
  FunctionDefRNA *dfunc;

  if (BLI_findstring_ptr(&srna->functions, identifier, offsetof(FunctionRNA, identifier))) {
    CLOG_ERROR(&LOG, "%s.%s already defined.", srna->identifier, identifier);
    return NULL;
  }

  func = rna_def_function(srna, identifier);

  if (!DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only at preprocess time.");
    return func;
  }

  dfunc = rna_find_function_def(func);
  dfunc->call = call;

  return func;
}

FunctionRNA *RNA_def_function_runtime(StructRNA *srna, const char *identifier, CallFunc call)
{
  FunctionRNA *func;

  func = rna_def_function(srna, identifier);

  if (DefRNA.preprocess) {
    CLOG_ERROR(&LOG, "only at runtime.");
    return func;
  }

  func->call = call;

  return func;
}

/* C return value only!, multiple RNA returns can be done with RNA_def_function_output */
void RNA_def_function_return(FunctionRNA *func, PropertyRNA *ret)
{
  if (ret->flag & PROP_DYNAMIC) {
    CLOG_ERROR(&LOG,
               "\"%s.%s\", dynamic values are not allowed as strict returns, "
               "use RNA_def_function_output instead.",
               func->identifier,
               ret->identifier);
    return;
  }
  else if (ret->arraydimension) {
    CLOG_ERROR(&LOG,
               "\"%s.%s\", arrays are not allowed as strict returns, "
               "use RNA_def_function_output instead.",
               func->identifier,
               ret->identifier);
    return;
  }

  BLI_assert(func->c_ret == NULL);
  func->c_ret = ret;

  RNA_def_function_output(func, ret);
}

void RNA_def_function_output(FunctionRNA *UNUSED(func), PropertyRNA *ret)
{
  ret->flag_parameter |= PARM_OUTPUT;
}

void RNA_def_function_flag(FunctionRNA *func, int flag)
{
  func->flag |= flag;
}

void RNA_def_function_ui_description(FunctionRNA *func, const char *description)
{
  func->description = description;
}

int rna_parameter_size(PropertyRNA *parm)
{
  PropertyType ptype = parm->type;
  int len = parm->totarraylength;

  /* XXX in other parts is mentioned that strings can be dynamic as well */
  if (parm->flag & PROP_DYNAMIC) {
    return sizeof(ParameterDynAlloc);
  }

  if (len > 0) {
    switch (ptype) {
      case PROP_BOOLEAN:
        return sizeof(bool) * len;
      case PROP_INT:
        return sizeof(int) * len;
      case PROP_FLOAT:
        return sizeof(float) * len;
      default:
        break;
    }
  }
  else {
    switch (ptype) {
      case PROP_BOOLEAN:
        return sizeof(bool);
      case PROP_INT:
      case PROP_ENUM:
        return sizeof(int);
      case PROP_FLOAT:
        return sizeof(float);
      case PROP_STRING:
        /* return values don't store a pointer to the original */
        if (parm->flag & PROP_THICK_WRAP) {
          StringPropertyRNA *sparm = (StringPropertyRNA *)parm;
          return sizeof(char) * sparm->maxlength;
        }
        else {
          return sizeof(char *);
        }
      case PROP_POINTER: {
#ifdef RNA_RUNTIME
        if (parm->flag_parameter & PARM_RNAPTR) {
          if (parm->flag & PROP_THICK_WRAP) {
            return sizeof(PointerRNA);
          }
          else {
            return sizeof(PointerRNA *);
          }
        }
        else {
          return sizeof(void *);
        }
#else
        if (parm->flag_parameter & PARM_RNAPTR) {
          if (parm->flag & PROP_THICK_WRAP) {
            return sizeof(PointerRNA);
          }
          else {
            return sizeof(PointerRNA *);
          }
        }
        else {
          return sizeof(void *);
        }
#endif
      }
      case PROP_COLLECTION:
        return sizeof(ListBase);
    }
  }

  return sizeof(void *);
}

/* Dynamic Enums */

void RNA_enum_item_add(EnumPropertyItem **items, int *totitem, const EnumPropertyItem *item)
{
  int tot = *totitem;

  if (tot == 0) {
    *items = MEM_callocN(sizeof(EnumPropertyItem) * 8, __func__);
  }
  else if (tot >= 8 && (tot & (tot - 1)) == 0) {
    /* power of two > 8 */
    *items = MEM_recallocN_id(*items, sizeof(EnumPropertyItem) * tot * 2, __func__);
  }

  (*items)[tot] = *item;
  *totitem = tot + 1;

  /* Ensure we get crashes on missing calls to 'RNA_enum_item_end', see T74227. */
#ifdef DEBUG
  static const EnumPropertyItem item_error = {
      -1, POINTER_FROM_INT(-1), -1, POINTER_FROM_INT(-1), POINTER_FROM_INT(-1)};
  if (item != &item_error) {
    RNA_enum_item_add(items, totitem, &item_error);
    *totitem -= 1;
  }
#endif
}

void RNA_enum_item_add_separator(EnumPropertyItem **items, int *totitem)
{
  static const EnumPropertyItem sepr = {0, "", 0, NULL, NULL};
  RNA_enum_item_add(items, totitem, &sepr);
}

void RNA_enum_items_add(EnumPropertyItem **items, int *totitem, const EnumPropertyItem *item)
{
  for (; item->identifier; item++) {
    RNA_enum_item_add(items, totitem, item);
  }
}

void RNA_enum_items_add_value(EnumPropertyItem **items,
                              int *totitem,
                              const EnumPropertyItem *item,
                              int value)
{
  for (; item->identifier; item++) {
    if (item->value == value) {
      RNA_enum_item_add(items, totitem, item);
      /* break on first match - does this break anything?
       * (is quick hack to get object->parent_type working ok for armature/lattice) */
      break;
    }
  }
}

void RNA_enum_item_end(EnumPropertyItem **items, int *totitem)
{
  static const EnumPropertyItem empty = {0, NULL, 0, NULL, NULL};
  RNA_enum_item_add(items, totitem, &empty);
}

/* Memory management */

#ifdef RNA_RUNTIME
void RNA_def_struct_duplicate_pointers(BlenderRNA *brna, StructRNA *srna)
{
  if (srna->identifier) {
    srna->identifier = BLI_strdup(srna->identifier);
    if (srna->flag & STRUCT_PUBLIC_NAMESPACE) {
      BLI_ghash_replace_key(brna->structs_map, (void *)srna->identifier);
    }
  }
  if (srna->name) {
    srna->name = BLI_strdup(srna->name);
  }
  if (srna->description) {
    srna->description = BLI_strdup(srna->description);
  }

  srna->flag |= STRUCT_FREE_POINTERS;
}

void RNA_def_struct_free_pointers(BlenderRNA *brna, StructRNA *srna)
{
  if (srna->flag & STRUCT_FREE_POINTERS) {
    if (srna->identifier) {
      if (srna->flag & STRUCT_PUBLIC_NAMESPACE) {
        if (brna != NULL) {
          BLI_ghash_remove(brna->structs_map, (void *)srna->identifier, NULL, NULL);
        }
      }
      MEM_freeN((void *)srna->identifier);
    }
    if (srna->name) {
      MEM_freeN((void *)srna->name);
    }
    if (srna->description) {
      MEM_freeN((void *)srna->description);
    }
  }
}

void RNA_def_func_duplicate_pointers(FunctionRNA *func)
{
  if (func->identifier) {
    func->identifier = BLI_strdup(func->identifier);
  }
  if (func->description) {
    func->description = BLI_strdup(func->description);
  }

  func->flag |= FUNC_FREE_POINTERS;
}

void RNA_def_func_free_pointers(FunctionRNA *func)
{
  if (func->flag & FUNC_FREE_POINTERS) {
    if (func->identifier) {
      MEM_freeN((void *)func->identifier);
    }
    if (func->description) {
      MEM_freeN((void *)func->description);
    }
  }
}

void RNA_def_property_duplicate_pointers(StructOrFunctionRNA *cont_, PropertyRNA *prop)
{
  ContainerRNA *cont = cont_;
  int a;

  /* annoying since we just added this to a hash, could make this add the correct key to the hash
   * in the first place */
  if (prop->identifier) {
    if (cont->prophash) {
      prop->identifier = BLI_strdup(prop->identifier);
      BLI_ghash_reinsert(cont->prophash, (void *)prop->identifier, prop, NULL, NULL);
    }
    else {
      prop->identifier = BLI_strdup(prop->identifier);
    }
  }

  if (prop->name) {
    prop->name = BLI_strdup(prop->name);
  }
  if (prop->description) {
    prop->description = BLI_strdup(prop->description);
  }

  switch (prop->type) {
    case PROP_BOOLEAN: {
      BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;

      if (bprop->defaultarray) {
        bool *array = MEM_mallocN(sizeof(bool) * prop->totarraylength, "RNA_def_property_store");
        memcpy(array, bprop->defaultarray, sizeof(bool) * prop->totarraylength);
        bprop->defaultarray = array;
      }
      break;
    }
    case PROP_INT: {
      IntPropertyRNA *iprop = (IntPropertyRNA *)prop;

      if (iprop->defaultarray) {
        int *array = MEM_mallocN(sizeof(int) * prop->totarraylength, "RNA_def_property_store");
        memcpy(array, iprop->defaultarray, sizeof(int) * prop->totarraylength);
        iprop->defaultarray = array;
      }
      break;
    }
    case PROP_ENUM: {
      EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;

      if (eprop->item) {
        EnumPropertyItem *array = MEM_mallocN(sizeof(EnumPropertyItem) * (eprop->totitem + 1),
                                              "RNA_def_property_store");
        memcpy(array, eprop->item, sizeof(EnumPropertyItem) * (eprop->totitem + 1));
        eprop->item = array;

        for (a = 0; a < eprop->totitem; a++) {
          if (array[a].identifier) {
            array[a].identifier = BLI_strdup(array[a].identifier);
          }
          if (array[a].name) {
            array[a].name = BLI_strdup(array[a].name);
          }
          if (array[a].description) {
            array[a].description = BLI_strdup(array[a].description);
          }
        }
      }
      break;
    }
    case PROP_FLOAT: {
      FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;

      if (fprop->defaultarray) {
        float *array = MEM_mallocN(sizeof(float) * prop->totarraylength, "RNA_def_property_store");
        memcpy(array, fprop->defaultarray, sizeof(float) * prop->totarraylength);
        fprop->defaultarray = array;
      }
      break;
    }
    case PROP_STRING: {
      StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
      if (sprop->defaultvalue) {
        sprop->defaultvalue = BLI_strdup(sprop->defaultvalue);
      }
      break;
    }
    default:
      break;
  }

  prop->flag_internal |= PROP_INTERN_FREE_POINTERS;
}

void RNA_def_property_free_pointers(PropertyRNA *prop)
{
  if (prop->flag_internal & PROP_INTERN_FREE_POINTERS) {
    int a;

    if (prop->identifier) {
      MEM_freeN((void *)prop->identifier);
    }
    if (prop->name) {
      MEM_freeN((void *)prop->name);
    }
    if (prop->description) {
      MEM_freeN((void *)prop->description);
    }
    if (prop->py_data) {
      MEM_freeN(prop->py_data);
    }

    switch (prop->type) {
      case PROP_BOOLEAN: {
        BoolPropertyRNA *bprop = (BoolPropertyRNA *)prop;
        if (bprop->defaultarray) {
          MEM_freeN((void *)bprop->defaultarray);
        }
        break;
      }
      case PROP_INT: {
        IntPropertyRNA *iprop = (IntPropertyRNA *)prop;
        if (iprop->defaultarray) {
          MEM_freeN((void *)iprop->defaultarray);
        }
        break;
      }
      case PROP_FLOAT: {
        FloatPropertyRNA *fprop = (FloatPropertyRNA *)prop;
        if (fprop->defaultarray) {
          MEM_freeN((void *)fprop->defaultarray);
        }
        break;
      }
      case PROP_ENUM: {
        EnumPropertyRNA *eprop = (EnumPropertyRNA *)prop;

        for (a = 0; a < eprop->totitem; a++) {
          if (eprop->item[a].identifier) {
            MEM_freeN((void *)eprop->item[a].identifier);
          }
          if (eprop->item[a].name) {
            MEM_freeN((void *)eprop->item[a].name);
          }
          if (eprop->item[a].description) {
            MEM_freeN((void *)eprop->item[a].description);
          }
        }

        if (eprop->item) {
          MEM_freeN((void *)eprop->item);
        }
        break;
      }
      case PROP_STRING: {
        StringPropertyRNA *sprop = (StringPropertyRNA *)prop;
        if (sprop->defaultvalue) {
          MEM_freeN((void *)sprop->defaultvalue);
        }
        break;
      }
      default:
        break;
    }
  }
}

static void rna_def_property_free(StructOrFunctionRNA *cont_, PropertyRNA *prop)
{
  ContainerRNA *cont = cont_;

  if (prop->flag_internal & PROP_INTERN_RUNTIME) {
    if (cont->prophash) {
      BLI_ghash_remove(cont->prophash, prop->identifier, NULL, NULL);
    }

    RNA_def_property_free_pointers(prop);
    rna_freelinkN(&cont->properties, prop);
  }
  else {
    RNA_def_property_free_pointers(prop);
  }
}

/* note: only intended for removing dynamic props */
int RNA_def_property_free_identifier(StructOrFunctionRNA *cont_, const char *identifier)
{
  ContainerRNA *cont = cont_;
  PropertyRNA *prop;

  for (prop = cont->properties.first; prop; prop = prop->next) {
    if (STREQ(prop->identifier, identifier)) {
      if (prop->flag_internal & PROP_INTERN_RUNTIME) {
        rna_def_property_free(cont_, prop);
        return 1;
      }
      else {
        return -1;
      }
    }
  }
  return 0;
}
#endif /* RNA_RUNTIME */

const char *RNA_property_typename(PropertyType type)
{
  switch (type) {
    case PROP_BOOLEAN:
      return "PROP_BOOLEAN";
    case PROP_INT:
      return "PROP_INT";
    case PROP_FLOAT:
      return "PROP_FLOAT";
    case PROP_STRING:
      return "PROP_STRING";
    case PROP_ENUM:
      return "PROP_ENUM";
    case PROP_POINTER:
      return "PROP_POINTER";
    case PROP_COLLECTION:
      return "PROP_COLLECTION";
  }

  return "PROP_UNKNOWN";
}
