/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#pragma once

/* Macros to help reduce code clutter in rna_mesh.c */

/* Define the accessors for a basic CustomDataLayer collection, skipping anonymous layers */
#define DEFINE_CUSTOMDATA_LAYER_COLLECTION(collection_name, customdata_type, layer_type) \
  /* check */ \
  static int rna_##collection_name##_check(CollectionPropertyIterator *UNUSED(iter), void *data) \
  { \
    CustomDataLayer *layer = (CustomDataLayer *)data; \
    return (layer->anonymous_id != NULL || layer->type != layer_type); \
  } \
  /* begin */ \
  static void rna_Mesh_##collection_name##s_begin(CollectionPropertyIterator *iter, \
                                                  PointerRNA *ptr) \
  { \
    CustomData *data = rna_mesh_##customdata_type(ptr); \
    if (data) { \
      rna_iterator_array_begin(iter, \
                               (void *)data->layers, \
                               sizeof(CustomDataLayer), \
                               data->totlayer, \
                               0, \
                               rna_##collection_name##_check); \
    } \
    else { \
      rna_iterator_array_begin(iter, NULL, 0, 0, 0, NULL); \
    } \
  } \
  /* length */ \
  static int rna_Mesh_##collection_name##s_length(PointerRNA *ptr) \
  { \
    CustomData *data = rna_mesh_##customdata_type(ptr); \
    return data ? CustomData_number_of_layers(data, layer_type) - \
                      CustomData_number_of_anonymous_layers(data, layer_type) : \
                  0; \
  } \
  /* index range */ \
  static void rna_Mesh_##collection_name##_index_range( \
      PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax)) \
  { \
    CustomData *data = rna_mesh_##customdata_type(ptr); \
    *min = 0; \
    *max = data ? CustomData_number_of_layers(data, layer_type) - \
                      CustomData_number_of_anonymous_layers(data, layer_type) - 1 : \
                  0; \
    *max = MAX2(0, *max); \
  }

/* Define the accessors for special CustomDataLayers in the collection
 * (active, render, clone, stencil, etc) */
#define DEFINE_CUSTOMDATA_LAYER_COLLECTION_ACTIVEITEM( \
    collection_name, customdata_type, layer_type, active_type, layer_rna_type) \
\
  static PointerRNA rna_Mesh_##collection_name##_##active_type##_get(PointerRNA *ptr) \
  { \
    CustomData *data = rna_mesh_##customdata_type(ptr); \
    CustomDataLayer *layer; \
    if (data) { \
      int index = CustomData_get_##active_type##_layer_index(data, layer_type); \
      layer = (index == -1) ? NULL : &data->layers[index]; \
    } \
    else { \
      layer = NULL; \
    } \
    return rna_pointer_inherit_refine(ptr, &RNA_##layer_rna_type, layer); \
  } \
\
  static void rna_Mesh_##collection_name##_##active_type##_set( \
      PointerRNA *ptr, PointerRNA value, struct ReportList *UNUSED(reports)) \
  { \
    Mesh *me = rna_mesh(ptr); \
    CustomData *data = rna_mesh_##customdata_type(ptr); \
    int a; \
    if (data) { \
      CustomDataLayer *layer; \
      int layer_index = CustomData_get_layer_index(data, layer_type); \
      for (layer = data->layers + layer_index, a = 0; layer_index + a < data->totlayer; \
           layer++, a++) { \
        if (value.data == layer) { \
          CustomData_set_layer_##active_type(data, layer_type, a); \
          BKE_mesh_tessface_clear(me); \
          return; \
        } \
      } \
    } \
  } \
\
  static int rna_Mesh_##collection_name##_##active_type##_index_get(PointerRNA *ptr) \
  { \
    CustomData *data = rna_mesh_##customdata_type(ptr); \
    if (data) { \
      return CustomData_get_##active_type##_layer(data, layer_type); \
    } \
    else { \
      return 0; \
    } \
  } \
\
  static void rna_Mesh_##collection_name##_##active_type##_index_set(PointerRNA *ptr, int value) \
  { \
    Mesh *me = rna_mesh(ptr); \
    CustomData *data = rna_mesh_##customdata_type(ptr); \
    if (data) { \
      if (UNLIKELY(value < 0)) { \
        value = 0; \
      } \
      else if (value > 0) { \
        value = min_ii(value, CustomData_number_of_layers(data, layer_type) - 1); \
      } \
      CustomData_set_layer_##active_type(data, layer_type, value); \
      BKE_mesh_tessface_clear(me); \
    } \
  }
