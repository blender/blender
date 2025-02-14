/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "BLO_readfile.hh"

#include "BLT_translation.hh"

#include "DNA_space_types.h"

#include "BKE_blendfile_link_append.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#ifdef RNA_RUNTIME

#  include "BLI_bit_span.hh"
#  include "BLI_string.h"

void rna_BlendImportContextLibrary_filepath_get(PointerRNA *ptr, char *value)
{
  BlendfileLinkAppendContextLibrary *ctx_lib = static_cast<BlendfileLinkAppendContextLibrary *>(
      ptr->data);
  const size_t str_len = ctx_lib->path.length();
  BLI_strncpy(value, ctx_lib->path.c_str(), str_len + 1);
}

int rna_BlendImportContextLibrary_filepath_len(PointerRNA *ptr)
{
  BlendfileLinkAppendContextLibrary *ctx_lib = static_cast<BlendfileLinkAppendContextLibrary *>(
      ptr->data);
  return int(ctx_lib->path.length());
}

void rna_BlendImportContextItem_name_get(PointerRNA *ptr, char *value)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  const size_t str_len = ctx_item->name.length();
  BLI_strncpy(value, ctx_item->name.c_str(), str_len + 1);
}

int rna_BlendImportContextItem_name_len(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return int(ctx_item->name.length());
}

int rna_BlendImportContextItem_id_type_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return int(ctx_item->idcode);
}

struct RNABlendImportContextItemLibrariesIterator {
  BlendfileLinkAppendContextItem *ctx_item;
  blender::bits::BitIterator iter;
  int iter_index;
};

void rna_BlendImportContextItem_libraries_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);

  const blender::BitVector<> &libraries = ctx_item->libraries;
  RNABlendImportContextItemLibrariesIterator *libs_iter =
      MEM_new<RNABlendImportContextItemLibrariesIterator>(
          __func__, RNABlendImportContextItemLibrariesIterator{ctx_item, libraries.begin(), 0});
  iter->internal.custom = libs_iter;
  while (!(*libs_iter->iter) && libs_iter->iter != libs_iter->ctx_item->libraries.end()) {
    libs_iter->iter.operator++();
    libs_iter->iter_index++;
  }
  iter->valid = (libs_iter->iter != libs_iter->ctx_item->libraries.end());
}

void rna_BlendImportContextItem_libraries_next(CollectionPropertyIterator *iter)
{
  RNABlendImportContextItemLibrariesIterator *libs_iter =
      static_cast<RNABlendImportContextItemLibrariesIterator *>(iter->internal.custom);
  do {
    libs_iter->iter.operator++();
    libs_iter->iter_index++;
  } while (!(*libs_iter->iter) && libs_iter->iter != libs_iter->ctx_item->libraries.end());
  iter->valid = (libs_iter->iter != libs_iter->ctx_item->libraries.end());
}

void rna_BlendImportContextItem_libraries_end(CollectionPropertyIterator *iter)
{
  RNABlendImportContextItemLibrariesIterator *libs_iter =
      static_cast<RNABlendImportContextItemLibrariesIterator *>(iter->internal.custom);

  iter->valid = false;
  iter->internal.custom = nullptr;
  MEM_delete(libs_iter);
}

PointerRNA rna_BlendImportContextItem_libraries_get(CollectionPropertyIterator *iter)
{
  RNABlendImportContextItemLibrariesIterator *libs_iter =
      static_cast<RNABlendImportContextItemLibrariesIterator *>(iter->internal.custom);

  BlendfileLinkAppendContextLibrary &ctx_lib =
      libs_iter->ctx_item->lapp_context->libraries[libs_iter->iter_index];
  return rna_pointer_inherit_refine(&iter->parent, &RNA_BlendImportContextLibrary, &ctx_lib);
}

int rna_BlendImportContextItem_libraries_len(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);

  /* Count amount of enabled libraries in the item's bitmask. */
  int count = 0;
  for (const blender::BitRef &bit : ctx_item->libraries) {
    if (bit) {
      count++;
    }
  }
  return count;
}

int rna_BlendImportContextItem_append_action_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return int(ctx_item->action);
}

int rna_BlendImportContextItem_import_info_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return int(ctx_item->tag);
}

PointerRNA rna_BlendImportContextItem_id_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return RNA_id_pointer_create(ctx_item->new_id);
}

PointerRNA rna_BlendImportContextItem_source_library_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return RNA_id_pointer_create(&ctx_item->source_library->id);
}

PointerRNA rna_BlendImportContextItem_library_override_id_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return RNA_id_pointer_create(ctx_item->liboverride_id);
}

PointerRNA rna_BlendImportContextItem_reusable_local_id_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContextItem *ctx_item = static_cast<BlendfileLinkAppendContextItem *>(
      ptr->data);
  return RNA_id_pointer_create(ctx_item->reusable_local_id);
}

struct RNABlendImportContextItemsIterator {
  BlendfileLinkAppendContext *ctx;
  BlendfileLinkAppendContext::items_iterator_t iter;
};

void rna_BlendImportContext_import_items_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  BlendfileLinkAppendContext *ctx = static_cast<BlendfileLinkAppendContext *>(ptr->data);

  RNABlendImportContextItemsIterator *items_iter = MEM_new<RNABlendImportContextItemsIterator>(
      __func__, RNABlendImportContextItemsIterator{ctx, ctx->items.begin()});
  iter->internal.custom = items_iter;
  iter->valid = (items_iter->iter != items_iter->ctx->items.end());
}

void rna_BlendImportContext_import_items_next(CollectionPropertyIterator *iter)
{
  RNABlendImportContextItemsIterator *items_iter =
      static_cast<RNABlendImportContextItemsIterator *>(iter->internal.custom);
  items_iter->iter++;
  iter->valid = (items_iter->iter != items_iter->ctx->items.end());
}

void rna_BlendImportContext_import_items_end(CollectionPropertyIterator *iter)
{
  RNABlendImportContextItemsIterator *items_iter =
      static_cast<RNABlendImportContextItemsIterator *>(iter->internal.custom);

  iter->valid = false;
  iter->internal.custom = nullptr;
  MEM_delete(items_iter);
}

PointerRNA rna_BlendImportContext_import_items_get(CollectionPropertyIterator *iter)
{
  RNABlendImportContextItemsIterator *items_iter =
      static_cast<RNABlendImportContextItemsIterator *>(iter->internal.custom);

  BlendfileLinkAppendContextItem &ctx_item = *items_iter->iter;
  return rna_pointer_inherit_refine(&iter->parent, &RNA_BlendImportContextItem, &ctx_item);
}

int rna_BlendImportContext_import_items_len(PointerRNA *ptr)
{
  BlendfileLinkAppendContext *ctx = static_cast<BlendfileLinkAppendContext *>(ptr->data);
  return int(ctx->items.size());
}

int rna_BlendImportContext_options_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContext *ctx = static_cast<BlendfileLinkAppendContext *>(ptr->data);
  return ctx->params->flag;
}

int rna_BlendImportContext_process_stage_get(PointerRNA *ptr)
{
  BlendfileLinkAppendContext *ctx = static_cast<BlendfileLinkAppendContext *>(ptr->data);
  return int(ctx->process_stage);
}

#else /* RNA_RUNTIME */

static void rna_def_blendfile_import_library(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BlendImportContextLibrary", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Blendfile Import Context Library",
      "Library (blendfile) reference in a BlendImportContext data. Currently only "
      "exposed as read-only data for the pre/post blendimport handlers");

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(prop,
                                "rna_BlendImportContextLibrary_filepath_get",
                                "rna_BlendImportContextLibrary_filepath_len",
                                nullptr);

  RNA_define_verify_sdna(true); /* not in sdna */
}

static void RNA_def_blendfile_import_libraries(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "BlendImportContextLibraries");
  srna = RNA_def_struct(brna, "BlendImportContextLibraries", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Blendfile Import Context Libraries",
                         "Collection of source libraries, i.e. blendfile paths");
}

static void rna_def_blendfile_import_item(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BlendImportContextItem", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Blendfile Import Context Item",
      "An item (representing a data-block) in a BlendImportContext data. Currently only "
      "exposed as read-only data for the pre/post linking handlers");

  RNA_define_verify_sdna(false); /* not in sdna */

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "ID Name", "ID name of the item");
  RNA_def_property_string_funcs(
      prop, "rna_BlendImportContextItem_name_get", "rna_BlendImportContextItem_name_len", nullptr);

  prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_id_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "ID Type", "ID type of the item");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_property_enum_funcs(prop, "rna_BlendImportContextItem_id_type_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "source_libraries", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BlendImportContextLibrary");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Source Libraries",
                           "List of libraries to search and import that ID from. The ID will be "
                           "imported from the first file in that list that contains it");
  RNA_def_property_collection_funcs(prop,
                                    "rna_BlendImportContextItem_libraries_begin",
                                    "rna_BlendImportContextItem_libraries_next",
                                    "rna_BlendImportContextItem_libraries_end",
                                    "rna_BlendImportContextItem_libraries_get",
                                    "rna_BlendImportContextItem_libraries_len",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_blendfile_import_libraries(brna, prop);

  static const EnumPropertyItem blend_import_item_append_action_items[] = {
      {LINK_APPEND_ACT_UNSET, "UNSET", 0, "", "Not yet defined"},
      {LINK_APPEND_ACT_KEEP_LINKED, "KEEP_LINKED", 0, "", "ID has been kept linked"},
      {LINK_APPEND_ACT_REUSE_LOCAL,
       "REUSE_LOCAL",
       0,
       "",
       "An existing matching local ID has been re-used"},
      {LINK_APPEND_ACT_MAKE_LOCAL, "MAKE_LOCAL", 0, "", "The newly linked ID has been made local"},
      {LINK_APPEND_ACT_COPY_LOCAL,
       "COPY_LOCAL",
       0,
       "",
       "The linked ID had other unrelated usages, so it has been duplicated into a local copy"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  prop = RNA_def_property(srna, "append_action", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, blend_import_item_append_action_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Append Action",
                           "How this item has been handled by the append operation. Only set if "
                           "the data has been appended");
  RNA_def_property_enum_funcs(
      prop, "rna_BlendImportContextItem_append_action_get", nullptr, nullptr);

  static const EnumPropertyItem blend_import_item_import_info_items[] = {
      {LINK_APPEND_TAG_INDIRECT,
       "INDIRECT_USAGE",
       0,
       "",
       "That item was added for an indirectly imported ID, as a dependency of another data-block"},
      {LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY,
       "LIBOVERRIDE_DEPENDENCY",
       0,
       "",
       "That item represents an ID also used as liboverride dependency (either directly, as a "
       "liboverride reference, or indirectly, as data used by a liboverride reference). It should "
       "never be directly made local. Mutually exclusive with `LIBOVERRIDE_DEPENDENCY_ONLY`"},
      {LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY,
       "LIBOVERRIDE_DEPENDENCY_ONLY",
       0,
       "",
       "That item represents an ID only used as liboverride dependency (either directly or "
       "indirectly, see `LIBOVERRIDE_DEPENDENCY` for precisions). It should not be considered "
       "during the 'make local' (append) process, and remain purely linked data. Mutually "
       "exclusive with `LIBOVERRIDE_DEPENDENCY`"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  prop = RNA_def_property(srna, "import_info", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, blend_import_item_import_info_items);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Import Info", "Various status info about an item after it has been imported");
  RNA_def_property_enum_funcs(
      prop, "rna_BlendImportContextItem_import_info_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Imported ID",
                           "The imported ID. None until it has been linked or appended. "
                           "May be the same as ``reusable_local_id`` when appended");
  RNA_def_property_pointer_funcs(
      prop, "rna_BlendImportContextItem_id_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "source_library", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Library");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Source Library",
                           "Library ID representing the blendfile from which the ID was imported. "
                           "None until the ID has been linked or appended");
  RNA_def_property_pointer_funcs(
      prop, "rna_BlendImportContextItem_source_library_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "library_override_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Library Overridden ID",
      "The library override of the linked ID. None until it has been created");
  RNA_def_property_pointer_funcs(
      prop, "rna_BlendImportContextItem_library_override_id_get", nullptr, nullptr, nullptr);

  prop = RNA_def_property(srna, "reusable_local_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Reusable Local ID",
                           "The already existing local ID that may be reused in append & reuse "
                           "case. None until it has been found");
  RNA_def_property_pointer_funcs(
      prop, "rna_BlendImportContextItem_reusable_local_id_get", nullptr, nullptr, nullptr);

  RNA_define_verify_sdna(true); /* not in sdna */
}

static void rna_def_blendfile_import_items(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "BlendImportContextItems");
  srna = RNA_def_struct(brna, "BlendImportContextItems", nullptr);
  RNA_def_struct_ui_text(
      srna, "Blendfile Import Context Items", "Collection of blendfile import context items");

  /* TODO: Add/Remove items _before_ doing link/append (i.e. for 'pre' handlers). */
}

static void rna_def_blendfile_import_context(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BlendImportContext", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Blendfile Import Context",
      "Contextual data for a blendfile library/linked-data related operation. Currently "
      "only exposed as read-only data for the pre/post blendimport handlers");

  RNA_define_verify_sdna(false); /* not in sdna */

  /* NOTE: Cannot use just `items` here as this is a reserved Python dict method name. */
  prop = RNA_def_property(srna, "import_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "BlendImportContextItem");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_BlendImportContext_import_items_begin",
                                    "rna_BlendImportContext_import_items_next",
                                    "rna_BlendImportContext_import_items_end",
                                    "rna_BlendImportContext_import_items_get",
                                    "rna_BlendImportContext_import_items_len",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  rna_def_blendfile_import_items(brna, prop);

  static const EnumPropertyItem blend_import_options_items[] = {
      {FILE_LINK, "LINK", 0, "", "Only link data, instead of appending it"},
      {FILE_RELPATH,
       "MAKE_PATHS_RELATIVE",
       0,
       "",
       "Make paths of used library blendfiles relative to current blendfile"},
      {BLO_LIBLINK_USE_PLACEHOLDERS,
       "USE_PLACEHOLDERS",
       0,
       "",
       "Generate a placeholder (empty ID) if not found in any library files"},
      {BLO_LIBLINK_FORCE_INDIRECT,
       "FORCE_INDIRECT",
       0,
       "",
       "Force loaded ID to be tagged as indirectly linked (used in reload context only)"},
      {BLO_LIBLINK_APPEND_SET_FAKEUSER,
       "APPEND_SET_FAKEUSER",
       0,
       "",
       "Set fake user on appended IDs"},
      {BLO_LIBLINK_APPEND_RECURSIVE,
       "APPEND_RECURSIVE",
       0,
       "",
       "Append (make local) also indirect dependencies of appended IDs coming from other "
       "libraries. NOTE: All IDs (including indirectly linked ones) coming from the same initial "
       "library are always made local"},
      {BLO_LIBLINK_APPEND_LOCAL_ID_REUSE,
       "APPEND_LOCAL_ID_REUSE",
       0,
       "",
       "Try to re-use previously appended matching IDs when appending them again, instead of "
       "creating local duplicates"},
      {BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR,
       "APPEND_ASSET_DATA_CLEAR",
       0,
       "",
       "Clear the asset data on append (it is always kept for linked data)"},
      {FILE_AUTOSELECT, "SELECT_OBJECTS", 0, "", "Automatically select imported objects"},
      {FILE_ACTIVE_COLLECTION,
       "USE_ACTIVE_COLLECTION",
       0,
       "",
       "Use the active Collection of the current View Layer to instantiate imported "
       "collections and objects"},
      {BLO_LIBLINK_OBDATA_INSTANCE,
       "OBDATA_INSTANCE",
       0,
       "",
       "Instantiate object data IDs (i.e. create objects for them if needed)"},
      {BLO_LIBLINK_COLLECTION_INSTANCE,
       "COLLECTION_INSTANCE",
       0,
       "",
       "Instantiate collections as empties, instead of linking them into the current view layer"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  prop = RNA_def_property(srna, "options", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, blend_import_options_items);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "", "Options for this blendfile import operation");
  RNA_def_property_enum_funcs(prop, "rna_BlendImportContext_options_get", nullptr, nullptr);

  /* NOTE: Only stages currently exposed to handlers are listed here. */
  static const EnumPropertyItem blend_import_process_stage_items[] = {
      {int(BlendfileLinkAppendContext::ProcessStage::Init),
       "INIT",
       0,
       "",
       "Blendfile import context has been initialized and filled with a list of items to import, "
       "no data has been linked or appended yet"},
      {int(BlendfileLinkAppendContext::ProcessStage::Done),
       "DONE",
       0,
       "",
       "All data has been imported and is available in the list of ``import_items``"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  prop = RNA_def_property(srna, "process_stage", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, blend_import_process_stage_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "", "Current stage of the import process");
  RNA_def_property_enum_funcs(prop, "rna_BlendImportContext_process_stage_get", nullptr, nullptr);

  RNA_define_verify_sdna(true); /* not in sdna */
}

void RNA_def_blendfile_import(BlenderRNA *brna)
{
  rna_def_blendfile_import_library(brna);
  rna_def_blendfile_import_item(brna);
  rna_def_blendfile_import_context(brna);
}

#endif /* RNA_RUNTIME */
