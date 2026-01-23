/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string_utf8.h"

#include "DNA_key_types.h"
#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"

#include "BKE_asset.hh"
#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_wrapper.hh"
#include "BKE_node_runtime.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"
#include "BKE_workspace.hh"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_geometry.hh"
#include "ED_mesh.hh"
#include "ED_sculpt.hh"

#include "BLT_translation.hh"

#include "NOD_geometry_nodes_caller_ui.hh"
#include "NOD_geometry_nodes_dependencies.hh"
#include "NOD_geometry_nodes_execute.hh"
#include "NOD_geometry_nodes_lazy_function.hh"

#include "AS_asset_catalog.hh"
#include "AS_asset_catalog_path.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include <xxhash.h>

#include "geometry_intern.hh"

#include <fmt/format.h>

namespace blender {

namespace geo_log = nodes::geo_eval_log;

namespace ed::geometry {

using asset_system::AssetRepresentation;

struct ErrorsForType {
  int duplicate_count = 0;
  bool is_builtin_operator = false;
  Vector<std::string> idname_validation_errors;
  BLI_STRUCT_EQUALITY_OPERATORS_3(ErrorsForType,
                                  duplicate_count,
                                  is_builtin_operator,
                                  idname_validation_errors);
};
using OperatorRegisterErrors = Map<std::string, ErrorsForType>;

static OperatorRegisterErrors &get_registration_errors()
{
  static Map<std::string, ErrorsForType> errors_by_idname;
  return errors_by_idname;
}

/**
 * Abstraction layer over local node groups and assets, so operators can be registered and
 * referenced regardless of that distinction.
 */
struct OperatorTypeData : public wmOperatorType::TypeData {
  std::string name;
  std::string idname;
  StringRefNull custom_idname;
  std::string description;
  GeometryNodeAssetTraitFlag flag;

  struct LocalRef {
    uint32_t session_uid;
  };
  std::variant<AssetWeakReference, LocalRef> group_ref;

  std::array<int64_t, 2> hash;

  static std::optional<OperatorTypeData> from_asset(const AssetRepresentation &asset,
                                                    OperatorRegisterErrors &errors);
  static std::optional<OperatorTypeData> from_group(const bNodeTree &group,
                                                    OperatorRegisterErrors &errors);

 private:
  /** Should be called after any data changes. */
  void ensure_hash();
};

void OperatorTypeData::ensure_hash()
{
  XXH3_state_t *hash_state = XXH3_createState();
  BLI_SCOPED_DEFER([&hash_state]() -> void { XXH3_freeState(hash_state); })
  XXH3_128bits_reset(hash_state);
  XXH3_128bits_update(hash_state, this->name.data(), this->name.size());
  XXH3_128bits_update(hash_state, this->description.data(), this->description.size());
  XXH3_128bits_update(hash_state, &this->flag, sizeof(this->flag));
  std::visit(
      [&](const auto &value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, AssetWeakReference>) {
          XXH3_128bits_update(
              hash_state, &value.asset_library_type, sizeof(value.asset_library_type));
          if (value.asset_library_identifier) {
            XXH3_128bits_update(hash_state,
                                value.asset_library_identifier,
                                strlen(value.asset_library_identifier));
          }
          if (value.relative_asset_identifier) {
            XXH3_128bits_update(hash_state,
                                value.relative_asset_identifier,
                                strlen(value.relative_asset_identifier));
          }
        }
        else if constexpr (std::is_same_v<T, LocalRef>) {
          XXH3_128bits_update(hash_state, &value.session_uid, sizeof(value.session_uid));
        }
      },
      this->group_ref);
  static_assert(sizeof(this->hash) == sizeof(XXH128_hash_t));
  const XXH128_hash_t xxh3_hash = XXH3_128bits_digest(hash_state);
  this->hash[0] = xxh3_hash.low64;
  this->hash[1] = xxh3_hash.high64;
}

static std::optional<std::string> operator_idname_get(const StringRefNull custom_idname,
                                                      OperatorRegisterErrors *errors)
{
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE | RPT_PRINT_HANDLED_BY_OWNER);
  BLI_SCOPED_DEFER([&]() { BKE_reports_free(&reports); });
  if (!WM_operator_idname_ok_or_report(&reports, custom_idname.c_str())) {
    if (errors) {
      ErrorsForType &errors_for_type = errors->lookup_or_add_default_as(custom_idname);
      for (Report &report : reports.list) {
        errors_for_type.idname_validation_errors.append_as(report.message);
      }
    }
    return std::nullopt;
  }
  char idname_buf[OP_MAX_TYPENAME];
  WM_operator_bl_idname(idname_buf, custom_idname.c_str());
  return idname_buf;
}

static std::optional<StringRefNull> custom_idname_for_asset(const AssetRepresentation &asset)
{
  const AssetMetaData &metadata = asset.get_metadata();
  const IDProperty *id_property = BKE_asset_metadata_idprop_find(&metadata, "node_tool_idname");
  if (!id_property || id_property->type != IDP_STRING) {
    return std::nullopt;
  }
  return IDP_string_get(id_property);
}

static std::optional<std::string> operator_idname_for_asset(const AssetRepresentation &asset)
{
  const std::optional<StringRefNull> custom_idname = custom_idname_for_asset(asset);
  if (!custom_idname) {
    return std::nullopt;
  }
  return operator_idname_get(*custom_idname, nullptr);
}

std::optional<OperatorTypeData> OperatorTypeData::from_asset(
    const asset_system::AssetRepresentation &asset, OperatorRegisterErrors &errors)
{
  const std::optional<StringRefNull> custom_idname = custom_idname_for_asset(asset);
  if (!custom_idname) {
    return std::nullopt;
  }
  std::optional<std::string> idname = operator_idname_get(*custom_idname, &errors);
  if (!idname) {
    return std::nullopt;
  }

  const AssetMetaData &metadata = asset.get_metadata();
  OperatorTypeData type_data;
  type_data.name = asset.get_name();
  type_data.idname = std::move(*idname);
  type_data.custom_idname = *custom_idname;
  type_data.description = metadata.description ? metadata.description : "";
  const IDProperty *traits_flag = BKE_asset_metadata_idprop_find(
      &metadata, "geometry_node_asset_traits_flag");
  if (!traits_flag || traits_flag->type != IDP_INT) {
    return std::nullopt;
  }
  type_data.flag = GeometryNodeAssetTraitFlag(IDP_int_get(traits_flag));
  type_data.group_ref = asset.make_weak_reference();
  type_data.ensure_hash();
  return type_data;
}

static std::optional<StringRefNull> custom_idname_for_group(const bNodeTree &group)
{
  const char *idname = group.geometry_node_asset_traits->node_tool_idname;
  if (!idname) {
    return std::nullopt;
  }
  return StringRefNull(idname);
}

static std::optional<std::string> operator_idname_for_group(const bNodeTree &group)
{
  const char *idname = group.geometry_node_asset_traits->node_tool_idname;
  if (!idname) {
    return std::nullopt;
  }
  return operator_idname_get(idname, nullptr);
}

std::optional<OperatorTypeData> OperatorTypeData::from_group(const bNodeTree &group,
                                                             OperatorRegisterErrors &errors)
{
  const std::optional<StringRefNull> custom_idname = custom_idname_for_group(group);
  if (!custom_idname) {
    return std::nullopt;
  }
  std::optional<std::string> idname = operator_idname_get(*custom_idname, &errors);
  if (!idname) {
    return std::nullopt;
  }
  OperatorTypeData type_data;
  type_data.name = BKE_id_name(group.id);
  type_data.idname = std::move(*idname);
  type_data.custom_idname = *custom_idname;
  type_data.description = group.description ? group.description : "";
  type_data.flag = GeometryNodeAssetTraitFlag(group.geometry_node_asset_traits->flag);
  type_data.group_ref = OperatorTypeData::LocalRef{group.id.session_uid};
  type_data.ensure_hash();
  return type_data;
}

GeometryNodeAssetTraitFlag asset_flag_for_context(const Object &active_object);

/* -------------------------------------------------------------------- */
/** \name Operator
 * \{ */

static const bNodeTree *get_asset_or_local_node_group(const bContext &C,
                                                      const wmOperatorType &ot,
                                                      ReportList *reports)
{
  Main &bmain = *CTX_data_main(&C);
  const auto &type_data = static_cast<const OperatorTypeData &>(*ot.custom_data);
  return std::visit(
      [&](const auto &value) -> const bNodeTree * {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, OperatorTypeData::LocalRef>) {
          return id_cast<const bNodeTree *>(
              BKE_libblock_find_session_uid(&bmain, ID_NT, value.session_uid));
        }
        else if constexpr (std::is_same_v<T, AssetWeakReference>) {
          const asset_system::AssetRepresentation *asset = ed::asset::find_asset_from_weak_ref(
              C, value, reports);
          if (!asset) {
            return nullptr;
          }
          return id_cast<const bNodeTree *>(asset::asset_local_id_ensure_imported(bmain, *asset));
        }
        return nullptr;
      },
      type_data.group_ref);
}

static const bNodeTree *get_node_group(const bContext &C,
                                       const wmOperatorType &ot,
                                       ReportList *reports)
{
  const bNodeTree *group = get_asset_or_local_node_group(C, ot, reports);
  if (!group) {
    return nullptr;
  }
  if (group->type != NTREE_GEOMETRY) {
    if (reports) {
      BKE_report(reports, RPT_ERROR, "Asset is not a geometry node group");
    }
    return nullptr;
  }
  return group;
}

GeoOperatorLog::~GeoOperatorLog() = default;

/**
 * The socket value log is stored statically so it can be used in the node editor. A fancier
 * storage system shouldn't be necessary, since the goal is just to be able to debug intermediate
 * values when building a tool.
 */
static GeoOperatorLog &get_static_eval_log()
{
  static GeoOperatorLog log;
  return log;
}

const GeoOperatorLog &node_group_operator_static_eval_log()
{
  return get_static_eval_log();
}

/** Find all the visible node editors to log values for. */
static void find_socket_log_contexts(const Main &bmain,
                                     Set<ComputeContextHash> &r_socket_log_contexts)
{
  wmWindowManager *wm = static_cast<wmWindowManager *>(bmain.wm.first);
  if (wm == nullptr) {
    return;
  }
  for (const wmWindow &window : wm->windows) {
    const bScreen *screen = BKE_workspace_active_screen_get(window.workspace_hook);
    for (const ScrArea &area : screen->areabase) {
      const SpaceLink *sl = static_cast<SpaceLink *>(area.spacedata.first);
      if (sl->spacetype == SPACE_NODE) {
        const SpaceNode &snode = *reinterpret_cast<const SpaceNode *>(sl);
        if (snode.edittree == nullptr) {
          continue;
        }
        if (snode.node_tree_sub_type != SNODE_GEOMETRY_TOOL) {
          continue;
        }
        bke::ComputeContextCache compute_context_cache;
        const Map<const bke::bNodeTreeZone *, ComputeContextHash> hash_by_zone =
            geo_log::GeoNodesLog::get_context_hash_by_zone_for_node_editor(snode,
                                                                           compute_context_cache);
        for (const ComputeContextHash &hash : hash_by_zone.values()) {
          r_socket_log_contexts.add(hash);
        }
      }
    }
  }
}

static const ImplicitSharingInfo *get_vertex_group_sharing_info(const Mesh &mesh)
{
  const int layer_index = CustomData_get_layer_index(&mesh.vert_data, CD_MDEFORMVERT);
  if (layer_index == -1) {
    return nullptr;
  }
  return mesh.vert_data.layers[layer_index].sharing_info;
}

/**
 * This class adds a user to shared mesh data, requiring modifications of the mesh to reallocate
 * the data and its sharing info. This allows tracking which data is modified without having to
 * explicitly compare it.
 */
class MeshState {
  VectorSet<ImplicitSharingPtr<>> sharing_infos_;

 public:
  MeshState(const Mesh &mesh)
  {
    if (mesh.runtime->face_offsets_sharing_info) {
      this->freeze_shared_state(*mesh.runtime->face_offsets_sharing_info);
    }
    mesh.attributes().foreach_attribute([&](const bke::AttributeIter &iter) {
      const bke::GAttributeReader attribute = iter.get();
      if (attribute.varray.size() == 0) {
        return;
      }
      if (attribute.sharing_info) {
        this->freeze_shared_state(*attribute.sharing_info);
      }
    });
    if (const ImplicitSharingInfo *sharing_info = get_vertex_group_sharing_info(mesh)) {
      this->freeze_shared_state(*sharing_info);
    }
  }

  void freeze_shared_state(const ImplicitSharingInfo &sharing_info)
  {
    if (sharing_infos_.add(ImplicitSharingPtr<>{&sharing_info})) {
      sharing_info.add_user();
    }
  }
};

static std::string shape_key_attribute_name(const KeyBlock &kb)
{
  return fmt::format(".kb:{}", kb.name);
}

/** Support shape keys by propagating them through geometry nodes as attributes. */
static void add_shape_keys_as_attributes(Mesh &mesh, const Key &key)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  for (const KeyBlock &kb : key.block) {
    if (&kb == key.refkey) {
      /* The basis key will just receive values from the mesh positions. */
      continue;
    }
    const Span<float3> key_data(static_cast<float3 *>(kb.data), kb.totelem);
    attributes.add<float3>(shape_key_attribute_name(kb),
                           bke::AttrDomain::Point,
                           bke::AttributeInitVArray(VArray<float3>::from_span(key_data)));
  }
}

/* Copy shape key attributes back to the key data-block. */
static void store_attributes_to_shape_keys(const Mesh &mesh, Key &key)
{
  const bke::AttributeAccessor attributes = mesh.attributes();
  for (KeyBlock &kb : key.block) {
    const VArray attr = *attributes.lookup<float3>(shape_key_attribute_name(kb),
                                                   bke::AttrDomain::Point);
    if (!attr) {
      continue;
    }
    MEM_freeN(kb.data);
    kb.data = MEM_malloc_arrayN(attr.size(), sizeof(float3), __func__);
    kb.totelem = attr.size();
    attr.materialize({static_cast<float3 *>(kb.data), attr.size()});
  }
  if (KeyBlock *kb = key.refkey) {
    const Span<float3> positions = mesh.vert_positions();
    MEM_freeN(kb->data);
    kb->data = MEM_malloc_arrayN(positions.size(), sizeof(float3), __func__);
    kb->totelem = positions.size();
    array_utils::copy(positions, MutableSpan(static_cast<float3 *>(kb->data), positions.size()));
  }
}

static void remove_shape_key_attributes(Mesh &mesh, const Key &key)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  for (KeyBlock &kb : key.block) {
    attributes.remove(shape_key_attribute_name(kb));
  }
}

/**
 * Geometry nodes currently requires working on "evaluated" data-blocks (rather than "original"
 * data-blocks that are part of a #Main data-base). This could change in the future, but for now,
 * we need to create evaluated copies of geometry before passing it to geometry nodes. Implicit
 * sharing lets us avoid copying attribute data though.
 */
static bke::GeometrySet get_original_geometry_eval_copy(Depsgraph &depsgraph,
                                                        Object &object,
                                                        nodes::GeoNodesOperatorData &operator_data,
                                                        Vector<MeshState> &orig_mesh_states)
{
  switch (object.type) {
    case OB_CURVES: {
      Curves *curves = BKE_curves_copy_for_eval(id_cast<const Curves *>(object.data));
      return bke::GeometrySet::from_curves(curves);
    }
    case OB_POINTCLOUD: {
      PointCloud *points = BKE_pointcloud_copy_for_eval(id_cast<const PointCloud *>(object.data));
      return bke::GeometrySet::from_pointcloud(points);
    }
    case OB_MESH: {
      Mesh *mesh = id_cast<Mesh *>(object.data);

      if (std::shared_ptr<BMEditMesh> &em = mesh->runtime->edit_mesh) {
        operator_data.active_point_index = BM_mesh_active_vert_index_get(em->bm);
        operator_data.active_edge_index = BM_mesh_active_edge_index_get(em->bm);
        operator_data.active_face_index = BM_mesh_active_face_index_get(em->bm, false, true);
        EDBM_mesh_load_ex(DEG_get_bmain(&depsgraph), &object, true);
        EDBM_mesh_free_data(mesh->runtime->edit_mesh.get());
        em->bm = nullptr;
      }

      if (bke::pbvh::Tree *pbvh = bke::object::pbvh_get(object)) {
        /* Currently many sculpt mode operations do not tag normals dirty (see use of
         * #Mesh::tag_positions_changed_no_normals()), so access within geometry nodes cannot
         * know that normals are out of date and recalculate them. Update them here instead. */
        bke::pbvh::update_normals(depsgraph, object, *pbvh);
      }

      if (Key *key = mesh->key) {
        /* Add the shape key attributes to the original mesh before the evaluated copy. Applying
         * the result of the operator in sculpt mode uses the attributes on the original mesh to
         * detect which changes. */
        add_shape_keys_as_attributes(*mesh, *key);
      }

      Mesh *mesh_copy = BKE_mesh_copy_for_eval(*mesh);
      orig_mesh_states.append_as(*mesh_copy);
      return bke::GeometrySet::from_mesh(mesh_copy);
    }
    case OB_GREASE_PENCIL: {
      const GreasePencil *grease_pencil = id_cast<const GreasePencil *>(object.data);
      if (const bke::greasepencil::Layer *active_layer = grease_pencil->get_active_layer()) {
        operator_data.active_layer_index = *grease_pencil->get_layer_index(*active_layer);
      }
      GreasePencil *grease_pencil_copy = BKE_grease_pencil_copy_for_eval(grease_pencil);
      grease_pencil_copy->runtime->eval_frame = int(DEG_get_ctime(&depsgraph));
      return bke::GeometrySet::from_grease_pencil(grease_pencil_copy);
    }
    default:
      return {};
  }
}

static void store_result_geometry(const bContext &C,
                                  const wmOperator &op,
                                  const Depsgraph &depsgraph,
                                  Main &bmain,
                                  Scene &scene,
                                  Object &object,
                                  const RegionView3D *rv3d,
                                  bke::GeometrySet geometry)
{
  geometry.ensure_owns_direct_data();
  switch (object.type) {
    case OB_CURVES: {
      Curves &curves = *id_cast<Curves *>(object.data);
      Curves *new_curves = geometry.get_curves_for_write();
      if (!new_curves) {
        curves.geometry.wrap() = {};
        break;
      }

      /* Anonymous attributes shouldn't be available on the applied geometry. */
      new_curves->geometry.wrap().attributes_for_write().remove_anonymous();

      curves.geometry.wrap() = std::move(new_curves->geometry.wrap());
      BKE_object_material_from_eval_data(&bmain, &object, &new_curves->id);
      DEG_id_tag_update(&curves.id, ID_RECALC_GEOMETRY);
      break;
    }
    case OB_POINTCLOUD: {
      PointCloud &points = *id_cast<PointCloud *>(object.data);
      PointCloud *new_points =
          geometry.get_component_for_write<bke::PointCloudComponent>().release();
      if (!new_points) {
        new_points->attribute_storage.wrap() = {};
        points.totpoint = 0;
        break;
      }

      /* Anonymous attributes shouldn't be available on the applied geometry. */
      new_points->attributes_for_write().remove_anonymous();

      BKE_object_material_from_eval_data(&bmain, &object, &new_points->id);
      BKE_pointcloud_nomain_to_pointcloud(new_points, &points);
      DEG_id_tag_update(&points.id, ID_RECALC_GEOMETRY);
      break;
    }
    case OB_MESH: {
      Mesh &mesh = *id_cast<Mesh *>(object.data);

      Mesh *new_mesh = geometry.get_component_for_write<bke::MeshComponent>().release();
      if (new_mesh) {
        /* Anonymous attributes shouldn't be available on the applied geometry. */
        new_mesh->attributes_for_write().remove_anonymous();
        BKE_object_material_from_eval_data(&bmain, &object, &new_mesh->id);
      }
      else {
        new_mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
      }

      if (Key *key = mesh.key) {
        /* Copy the evaluated shape key attributes back to the key data-block. */
        store_attributes_to_shape_keys(*new_mesh, *key);
      }

      if (object.mode == OB_MODE_SCULPT) {
        sculpt_paint::store_mesh_from_eval(op, scene, depsgraph, rv3d, object, new_mesh);
        if (Key *key = mesh.key) {
          /* Make sure to free the shape key attributes after `sculpt_paint::store_mesh_from_eval`
           * because that uses the attributes to detect changes, for a critical performance
           * optimization when only changing specific attributes. */
          remove_shape_key_attributes(mesh, *key);
        }
      }
      else {
        if (Key *key = mesh.key) {
          /* Make sure to free the attributes before converting to #BMesh for edit mode; removing
           * attributes on #BMesh requires reallocating the dynamic AoS storage. */
          remove_shape_key_attributes(*new_mesh, *key);
        }
        if (object.mode == OB_MODE_EDIT) {
          EDBM_mesh_make_from_mesh(&object, new_mesh, scene.toolsettings->selectmode, true);
          BKE_editmesh_looptris_and_normals_calc(mesh.runtime->edit_mesh.get());
          BKE_id_free(nullptr, new_mesh);
          DEG_id_tag_update(&mesh.id, ID_RECALC_GEOMETRY);
        }
        else {
          BKE_mesh_nomain_to_mesh(new_mesh, &mesh, &object, false);
          DEG_id_tag_update(&mesh.id, ID_RECALC_GEOMETRY);
        }
      }

      break;
    }
    case OB_GREASE_PENCIL: {
      const int eval_frame = int(DEG_get_ctime(&depsgraph));

      GreasePencil &grease_pencil = *id_cast<GreasePencil *>(object.data);
      Vector<int> editable_layer_indices;
      for (const int layer_i : grease_pencil.layers().index_range()) {
        const bke::greasepencil::Layer &layer = grease_pencil.layer(layer_i);
        if (!layer.is_editable()) {
          continue;
        }
        editable_layer_indices.append(layer_i);
      }

      bool inserted_new_keyframe = false;
      for (const int layer_i : editable_layer_indices) {
        bke::greasepencil::Layer &layer = grease_pencil.layer(layer_i);
        /* TODO: For now, we always create a blank keyframe, but it might be good to expose this as
         * an option and allow to duplicate the previous key. */
        const bool duplicate_previous_key = false;
        ed::greasepencil::ensure_active_keyframe(
            scene, grease_pencil, layer, duplicate_previous_key, inserted_new_keyframe);
      }
      GreasePencil *new_grease_pencil =
          geometry.get_component_for_write<bke::GreasePencilComponent>().get_for_write();
      if (!new_grease_pencil) {
        /* Clear the Grease Pencil geometry. */
        for (const int layer_i : editable_layer_indices) {
          bke::greasepencil::Layer &layer = grease_pencil.layer(layer_i);
          if (bke::greasepencil::Drawing *drawing_orig = grease_pencil.get_drawing_at(layer,
                                                                                      eval_frame))
          {
            drawing_orig->strokes_for_write() = {};
            drawing_orig->tag_topology_changed();
          }
        }
      }
      else {
        IndexMaskMemory memory;
        const IndexMask editable_layers = IndexMask::from_indices(editable_layer_indices.as_span(),
                                                                  memory);
        ed::greasepencil::apply_eval_grease_pencil_data(
            *new_grease_pencil, eval_frame, editable_layers, grease_pencil);

        /* There might be layers with empty names after evaluation. Make sure to rename them. */
        bke::greasepencil::ensure_non_empty_layer_names(bmain, grease_pencil);
        BKE_object_material_from_eval_data(&bmain, &object, &new_grease_pencil->id);
      }

      DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
      if (inserted_new_keyframe) {
        WM_event_add_notifier(&C, NC_GPENCIL | NA_EDITED, nullptr);
      }
    }
  }
}

/**
 * Gather IDs used by the node group, and the node group itself if there are any. We need to use
 * *all* IDs because the only mechanism we have to replace the socket ID pointers with their
 * evaluated counterparts is evaluating the node group data-block itself.
 */
static void gather_node_group_ids(const bNodeTree &node_tree, Set<ID *> &ids)
{
  const int orig_size = ids.size();
  BLI_assert(node_tree.runtime->geometry_nodes_eval_dependencies);
  for (ID *id : node_tree.runtime->geometry_nodes_eval_dependencies->ids.values()) {
    ids.add(id);
  }
  if (ids.size() != orig_size) {
    /* Only evaluate the node group if it references data-blocks. In that case it needs to be
     * evaluated so that ID pointers are switched to point to evaluated data-blocks. */
    ids.add(const_cast<ID *>(&node_tree.id));
  }
}

static const bNodeTreeInterfaceSocket *find_group_input_by_identifier(const bNodeTree &node_group,
                                                                      const StringRef identifier)
{
  for (const bNodeTreeInterfaceSocket *input : node_group.interface_inputs()) {
    if (input->identifier == identifier) {
      return input;
    }
  }
  return nullptr;
}

static std::optional<ID_Type> socket_type_to_id_type(const eNodeSocketDatatype socket_type)
{
  switch (socket_type) {
    case SOCK_CUSTOM:
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_RGBA:
    case SOCK_SHADER:
    case SOCK_BOOLEAN:
    case SOCK_INT:
    case SOCK_STRING:
    case SOCK_GEOMETRY:
    case SOCK_ROTATION:
    case SOCK_MENU:
    case SOCK_MATRIX:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
      return std::nullopt;
    case SOCK_OBJECT:
      return ID_OB;
    case SOCK_IMAGE:
      return ID_IM;
    case SOCK_COLLECTION:
      return ID_GR;
    case SOCK_TEXTURE:
      return ID_TE;
    case SOCK_MATERIAL:
      return ID_MA;
    case SOCK_FONT:
      return ID_VF;
    case SOCK_SCENE:
      return ID_SCE;
    case SOCK_TEXT_ID:
      return ID_TXT;
    case SOCK_MASK:
      return ID_MSK;
    case SOCK_SOUND:
      return ID_SO;
  }
  return std::nullopt;
}

/**
 * Gather IDs referenced from node group input properties (the redo panel). In the end, the group
 * input properties will be copied to contain evaluated data-blocks from the active and/or an extra
 * depsgraph.
 */
static Map<StringRef, ID *> gather_input_ids(const Main &bmain,
                                             const bNodeTree &node_group,
                                             const IDProperty &properties)
{
  Map<StringRef, ID *> ids;
  IDP_foreach_property(
      &const_cast<IDProperty &>(properties), IDP_TYPE_FILTER_STRING, [&](IDProperty *prop) {
        const bNodeTreeInterfaceSocket *input = find_group_input_by_identifier(node_group,
                                                                               prop->name);
        if (!input) {
          return;
        }
        const std::optional<ID_Type> id_type = socket_type_to_id_type(
            input->socket_typeinfo()->type);
        if (!id_type) {
          return;
        }
        const char *id_name = IDP_string_get(prop);
        ID *id = BKE_libblock_find_name(&const_cast<Main &>(bmain), *id_type, id_name);
        if (!id) {
          return;
        }
        ids.add(prop->name, id);
      });
  return ids;
}

static Depsgraph *build_extra_depsgraph(const Depsgraph &depsgraph_active, const Set<ID *> &ids)
{
  Depsgraph *depsgraph = DEG_graph_new(DEG_get_bmain(&depsgraph_active),
                                       DEG_get_input_scene(&depsgraph_active),
                                       DEG_get_input_view_layer(&depsgraph_active),
                                       DEG_get_mode(&depsgraph_active));
  DEG_graph_build_from_ids(depsgraph, Vector<ID *>(ids.begin(), ids.end()));
  DEG_evaluate_on_refresh(depsgraph);
  return depsgraph;
}

static IDProperty *replace_strings_with_id_pointers(const IDProperty &op_properties,
                                                    const Map<StringRef, ID *> &input_ids)
{
  IDProperty *properties = bke::idprop::create_group("Exec Properties").release();
  IDP_foreach_property(&const_cast<IDProperty &>(op_properties), 0, [&](IDProperty *prop) {
    if (ID *id = input_ids.lookup_default(prop->name, nullptr)) {
      IDP_AddToGroup(properties, bke::idprop::create(prop->name, id).release());
    }
    else {
      IDP_AddToGroup(properties, IDP_CopyProperty(prop));
    }
  });
  return properties;
}

static void replace_inputs_evaluated_data_blocks(
    IDProperty &properties, const nodes::GeoNodesOperatorDepsgraphs &depsgraphs)
{
  IDP_foreach_property(&properties, IDP_TYPE_FILTER_ID, [&](IDProperty *property) {
    if (ID *id = IDP_ID_get(property)) {
      if (ID_TYPE_USE_COPY_ON_EVAL(GS(id->name))) {
        property->data.pointer = const_cast<ID *>(depsgraphs.get_evaluated_id(*id));
      }
    }
  });
}

static bool object_has_editable_data(const Main &bmain, const Object &object)
{
  if (!ELEM(object.type, OB_CURVES, OB_POINTCLOUD, OB_MESH, OB_GREASE_PENCIL)) {
    return false;
  }
  if (!BKE_id_is_editable(&bmain, static_cast<const ID *>(object.data))) {
    return false;
  }
  return true;
}

static Vector<Object *> gather_supported_objects(const bContext &C,
                                                 const Main &bmain,
                                                 const eObjectMode mode)
{
  Vector<Object *> objects;
  Set<const ID *> unique_object_data;

  auto handle_object = [&](Object *object) {
    if (object->mode != mode) {
      return;
    }
    if (!unique_object_data.add(static_cast<const ID *>(object->data))) {
      return;
    }
    if (!object_has_editable_data(bmain, *object)) {
      return;
    }
    objects.append(object);
  };

  if (mode == OB_MODE_OBJECT) {
    CTX_DATA_BEGIN (&C, Object *, object, selected_objects) {
      handle_object(object);
    }
    CTX_DATA_END;
  }
  else {
    Scene *scene = CTX_data_scene(&C);
    ViewLayer *view_layer = CTX_data_view_layer(&C);
    View3D *v3d = CTX_wm_view3d(&C);
    Object *active_object = CTX_data_active_object(&C);
    if (v3d && active_object) {
      FOREACH_OBJECT_IN_MODE_BEGIN (scene, view_layer, v3d, active_object->type, mode, ob) {
        handle_object(ob);
      }
      FOREACH_OBJECT_IN_MODE_END;
    }
  }
  return objects;
}

static wmOperatorStatus run_node_group_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *active_object = CTX_data_active_object(C);
  if (!active_object) {
    return OPERATOR_CANCELLED;
  }
  const eObjectMode mode = eObjectMode(active_object->mode);

  const bNodeTree *node_tree_orig = get_node_group(*C, *op->type, op->reports);
  if (!node_tree_orig) {
    return OPERATOR_CANCELLED;
  }

  const Vector<Object *> objects = gather_supported_objects(*C, *bmain, mode);

  Depsgraph *depsgraph_active = CTX_data_ensure_evaluated_depsgraph(C);
  Set<ID *> extra_ids;
  gather_node_group_ids(*node_tree_orig, extra_ids);
  const Map<StringRef, ID *> input_ids = gather_input_ids(
      *bmain, *node_tree_orig, *op->properties);
  for (ID *id : input_ids.values()) {
    /* Skip IDs that are already fully evaluated in the active depsgraph. */
    if (!DEG_id_is_fully_evaluated(depsgraph_active, id)) {
      extra_ids.add(id);
    }
  }

  const nodes::GeoNodesOperatorDepsgraphs depsgraphs{
      depsgraph_active,
      extra_ids.is_empty() ? nullptr : build_extra_depsgraph(*depsgraph_active, extra_ids),
  };

  IDProperty *properties = replace_strings_with_id_pointers(*op->properties, input_ids);
  BLI_SCOPED_DEFER([&]() { IDP_FreeProperty_ex(properties, false); });

  replace_inputs_evaluated_data_blocks(*properties, depsgraphs);

  const bNodeTree *node_tree = nullptr;
  if (depsgraphs.extra) {
    node_tree = DEG_get_evaluated(depsgraphs.extra, node_tree_orig);
  }
  else {
    node_tree = node_tree_orig;
  }

  const nodes::GeometryNodesLazyFunctionGraphInfo *lf_graph_info =
      nodes::ensure_geometry_nodes_lazy_function_graph(*node_tree).get();
  if (lf_graph_info == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Cannot evaluate node group");
    return OPERATOR_CANCELLED;
  }

  if (!node_tree->group_output_node()) {
    BKE_report(op->reports, RPT_ERROR, "Node group must have a group output node");
    return OPERATOR_CANCELLED;
  }
  if (node_tree->interface_outputs().is_empty() ||
      !STREQ(node_tree->interface_outputs()[0]->socket_type, "NodeSocketGeometry"))
  {
    BKE_report(op->reports, RPT_ERROR, "Node group's first output must be a geometry");
    return OPERATOR_CANCELLED;
  }

  bke::OperatorComputeContext compute_context;
  Set<ComputeContextHash> socket_log_contexts;
  GeoOperatorLog &eval_log = get_static_eval_log();
  eval_log.log = std::make_unique<geo_log::GeoNodesLog>();
  eval_log.node_group_name = node_tree->id.name + 2;
  find_socket_log_contexts(*bmain, socket_log_contexts);

  /* May be null if operator called from outside 3D view context. */
  const RegionView3D *rv3d = CTX_wm_region_view3d(C);
  Vector<MeshState> orig_mesh_states;

  for (Object *object : objects) {
    nodes::GeoNodesOperatorData operator_eval_data{};
    operator_eval_data.mode = mode;
    operator_eval_data.depsgraphs = &depsgraphs;
    operator_eval_data.self_object_orig = object;
    operator_eval_data.scene_orig = scene;
    RNA_int_get_array(op->ptr, "mouse_position", operator_eval_data.mouse_position);
    RNA_int_get_array(op->ptr, "region_size", operator_eval_data.region_size);
    RNA_float_get_array(op->ptr, "cursor_position", operator_eval_data.cursor_position);
    RNA_float_get_array(op->ptr, "cursor_rotation", &operator_eval_data.cursor_rotation.w);
    RNA_float_get_array(
        op->ptr, "viewport_projection_matrix", operator_eval_data.viewport_winmat.base_ptr());
    RNA_float_get_array(
        op->ptr, "viewport_view_matrix", operator_eval_data.viewport_viewmat.base_ptr());
    operator_eval_data.viewport_is_perspective = RNA_boolean_get(op->ptr,
                                                                 "viewport_is_perspective");

    nodes::GeoNodesCallData call_data{};
    call_data.operator_data = &operator_eval_data;
    call_data.eval_log = eval_log.log.get();
    if (object == active_object) {
      /* Only log values from the active object. */
      call_data.socket_log_contexts = &socket_log_contexts;
    }

    bke::GeometrySet geometry_orig = get_original_geometry_eval_copy(
        *depsgraph_active, *object, operator_eval_data, orig_mesh_states);

    bke::GeometrySet new_geometry = nodes::execute_geometry_nodes_on_geometry(
        *node_tree, properties, compute_context, call_data, std::move(geometry_orig));

    store_result_geometry(
        *C, *op, *depsgraph_active, *bmain, *scene, *object, rv3d, std::move(new_geometry));
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, object->data);
  }

  geo_log::GeoTreeLog &tree_log = eval_log.log->get_tree_log(compute_context.hash());
  tree_log.ensure_node_warnings(*bmain);
  for (const geo_log::NodeWarning &warning : tree_log.all_warnings) {
    if (warning.type == nodes::NodeWarningType::Info) {
      BKE_report(op->reports, RPT_INFO, warning.message.c_str());
    }
    else {
      BKE_report(op->reports, RPT_WARNING, warning.message.c_str());
    }
  }

  return OPERATOR_FINISHED;
}

/**
 * Input node values are stored as operator properties in order to support redoing from the redo
 * panel for a few reasons:
 *  1. Some data (like the mouse position) cannot be retrieved from the `exec` callback used for
 *     operator redo. Redo is meant to just call the operator again with the exact same properties.
 *  2. While adjusting an input in the redo panel, the user doesn't expect anything else to change.
 *     If we retrieve other data like the viewport transform on every execution, that won't be the
 *     case.
 * We use operator RNA properties instead of operator custom data because the custom data struct
 * isn't maintained for the redo `exec` call.
 */
static void store_input_node_values_rna_props(const bContext &C,
                                              wmOperator &op,
                                              const wmEvent &event)
{
  Scene *scene = CTX_data_scene(&C);
  /* NOTE: `region` and `rv3d` may be null when called from a script. */
  const ARegion *region = CTX_wm_region(&C);
  const RegionView3D *rv3d = CTX_wm_region_view3d(&C);

  /* Mouse position node inputs. */
  RNA_int_set_array(op.ptr, "mouse_position", event.mval);
  RNA_int_set_array(
      op.ptr,
      "region_size",
      region ? int2(BLI_rcti_size_x(&region->winrct), BLI_rcti_size_y(&region->winrct)) : int2(0));

  /* 3D cursor node inputs. */
  const View3DCursor &cursor = scene->cursor;
  RNA_float_set_array(op.ptr, "cursor_position", cursor.location);
  math::Quaternion cursor_rotation = cursor.rotation();
  RNA_float_set_array(op.ptr, "cursor_rotation", &cursor_rotation.w);

  /* Viewport transform node inputs. */
  RNA_float_set_array(op.ptr,
                      "viewport_projection_matrix",
                      rv3d ? float4x4(rv3d->winmat).base_ptr() : float4x4::identity().base_ptr());
  RNA_float_set_array(op.ptr,
                      "viewport_view_matrix",
                      rv3d ? float4x4(rv3d->viewmat).base_ptr() : float4x4::identity().base_ptr());
  RNA_boolean_set(op.ptr, "viewport_is_perspective", rv3d ? bool(rv3d->is_persp) : true);
}

static wmOperatorStatus run_node_group_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bNodeTree *node_tree = get_node_group(*C, *op->type, op->reports);
  if (!node_tree) {
    return OPERATOR_CANCELLED;
  }

  store_input_node_values_rna_props(*C, *op, *event);

  nodes::update_input_properties_from_node_tree(*node_tree, op->properties, *op->properties, true);
  nodes::update_output_properties_from_node_tree(*node_tree, op->properties, *op->properties);

  return run_node_group_exec(C, op);
}

static void run_node_group_ui(bContext *C, wmOperator *op)
{
  ui::Layout &layout = *op->layout;
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  Main *bmain = CTX_data_main(C);
  PointerRNA bmain_ptr = RNA_main_pointer_create(bmain);

  const bNodeTree *node_tree = get_node_group(*C, *op->type, nullptr);
  if (!node_tree) {
    return;
  }

  bke::OperatorComputeContext compute_context;
  GeoOperatorLog &eval_log = get_static_eval_log();

  geo_log::GeoTreeLog *tree_log = eval_log.log ?
                                      &eval_log.log->get_tree_log(compute_context.hash()) :
                                      nullptr;
  nodes::draw_geometry_nodes_operator_redo_ui(
      *C, *op, const_cast<bNodeTree &>(*node_tree), tree_log);
}

static bool run_node_ui_poll(wmOperatorType * /*ot*/, PointerRNA *ptr)
{
  bool result = false;
  RNA_STRUCT_BEGIN (ptr, prop) {
    int flag = RNA_property_flag(prop);
    if ((flag & PROP_HIDDEN) == 0) {
      result = true;
      break;
    }
  }
  RNA_STRUCT_END;
  return result;
}

static bool run_node_group_poll(bContext *C, wmOperatorType *ot)
{
  const auto &type_data = *static_cast<const OperatorTypeData *>(ot->custom_data.get());
  const Object *active_object = CTX_data_active_object(C);
  if (!active_object) {
    return false;
  }
  const GeometryNodeAssetTraitFlag flag = asset_flag_for_context(*active_object);
  if ((type_data.flag & flag) != flag) {
    return false;
  }
  return true;
}

static void register_node_tool(wmOperatorType *ot,
                               std::unique_ptr<OperatorTypeData> &type_data_ptr)
{
  OperatorTypeData &type_data = *type_data_ptr;
  ot->custom_data = std::move(type_data_ptr);

  PropertyRNA *prop;
  ot->name = type_data.name.c_str();
  ot->idname = type_data.idname.c_str();
  ot->description = type_data.description.empty() ? nullptr : type_data.description.c_str();

  ot->pyop_poll = run_node_group_poll;
  ot->invoke = run_node_group_invoke;
  ot->exec = run_node_group_exec;
  ot->ui = run_node_group_ui;
  ot->ui_poll = run_node_ui_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_NODE_TOOL;
  if (type_data.flag & GEO_NODE_ASSET_WAIT_FOR_CURSOR) {
    ot->flag |= OPTYPE_DEPENDS_ON_CURSOR;
  }

  /* See comment for #store_input_node_values_rna_props. */
  prop = RNA_def_int_array(ot->srna,
                           "mouse_position",
                           2,
                           nullptr,
                           INT_MIN,
                           INT_MAX,
                           "Mouse Position",
                           "Mouse coordinates in region space",
                           INT_MIN,
                           INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_int_array(
      ot->srna, "region_size", 2, nullptr, 0, INT_MAX, "Region Size", "", 0, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float_array(ot->srna,
                             "cursor_position",
                             3,
                             nullptr,
                             -FLT_MAX,
                             FLT_MAX,
                             "3D Cursor Position",
                             "",
                             -FLT_MAX,
                             FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float_array(ot->srna,
                             "cursor_rotation",
                             4,
                             nullptr,
                             -FLT_MAX,
                             FLT_MAX,
                             "3D Cursor Rotation",
                             "",
                             -FLT_MAX,
                             FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float_array(ot->srna,
                             "viewport_projection_matrix",
                             16,
                             nullptr,
                             -FLT_MAX,
                             FLT_MAX,
                             "Viewport Projection Transform",
                             "",
                             -FLT_MAX,
                             FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_float_array(ot->srna,
                             "viewport_view_matrix",
                             16,
                             nullptr,
                             -FLT_MAX,
                             FLT_MAX,
                             "Viewport View Transform",
                             "",
                             -FLT_MAX,
                             FLT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  prop = RNA_def_boolean(
      ot->srna, "viewport_is_perspective", false, "Viewport Is Perspective", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

void ui_template_node_operator_registration_errors(ui::Layout &layout,
                                                   const StringRefNull idname_py)
{
  const OperatorRegisterErrors &errors = get_registration_errors();
  const ErrorsForType *errors_for_type = errors.lookup_ptr(idname_py);
  if (!errors_for_type) {
    return;
  }
  ui::Layout &col = layout.column(false);
  if (errors_for_type->is_builtin_operator) {
    col.label(TIP_("Operator is already registered"), ICON_ERROR);
  }
  if (errors_for_type->duplicate_count != 0) {
    col.label(fmt::format(fmt::runtime(TIP_("Duplicates: {}")), errors_for_type->duplicate_count),
              ICON_ERROR);
  }
  for (const std::string &error : errors_for_type->idname_validation_errors) {
    col.label(error, ICON_ERROR);
  }
}

static Vector<std::unique_ptr<OperatorTypeData>> get_node_tools_type_data(
    const bContext &C, Main &bmain, OperatorRegisterErrors &errors)
{
  Vector<std::unique_ptr<OperatorTypeData>> all_types;
  for (bNodeTree &ntree : bmain.nodetrees) {
    if (ID_IS_ASSET(&ntree.id)) {
      continue;
    }
    if (!ntree.geometry_node_asset_traits) {
      continue;
    }
    if ((ntree.geometry_node_asset_traits->flag & GEO_NODE_ASSET_TOOL) == 0) {
      continue;
    }
    std::optional<OperatorTypeData> type_data = OperatorTypeData::from_group(ntree, errors);
    if (!type_data) {
      continue;
    }
    all_types.append(std::make_unique<OperatorTypeData>(std::move(*type_data)));
  }

  const AssetLibraryReference library_ref = asset_system::all_library_reference();
  ed::asset::list::storage_fetch(&library_ref, &C);
  if (ed::asset::list::library_get_once_available(library_ref)) {
    ed::asset::list::iterate(library_ref, [&](AssetRepresentation &asset) {
      if (asset.get_id_type() != ID_NT) {
        return true;
      }
      const AssetMetaData &meta_data = asset.get_metadata();
      const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
      if (tree_type == nullptr) {
        return true;
      }
      if (IDP_int_get(tree_type) != NTREE_GEOMETRY) {
        return true;
      }
      const IDProperty *traits_flag = BKE_asset_metadata_idprop_find(
          &meta_data, "geometry_node_asset_traits_flag");
      if (!traits_flag) {
        return true;
      }
      if (traits_flag->type != IDP_INT) {
        return true;
      }
      if ((IDP_int_get(traits_flag) & GEO_NODE_ASSET_TOOL) == 0) {
        return true;
      }
      std::optional<OperatorTypeData> type_data = OperatorTypeData::from_asset(asset, errors);
      if (!type_data) {
        return true;
      }
      all_types.append(std::make_unique<OperatorTypeData>(std::move(*type_data)));
      return true;
    });
  }

  return all_types;
}

void register_node_group_operators(const bContext &C)
{
  wmWindowManager &wm = *CTX_wm_manager(&C);
  Main &bmain = *CTX_data_main(&C);

  OperatorRegisterErrors &errors = get_registration_errors();
  OperatorRegisterErrors last_errors = errors;
  errors.clear();

  Vector<std::unique_ptr<OperatorTypeData>> node_tool_types = get_node_tools_type_data(
      C, bmain, errors);

  Vector<std::unique_ptr<OperatorTypeData>> types_to_register;
  Set<StringRefNull> handled_types;
  Set<wmOperatorType *> types_to_remove;
  for (std::unique_ptr<OperatorTypeData> &type : node_tool_types) {
    if (!handled_types.add_as(type->idname)) {
      errors.lookup_or_add_default_as(type->custom_idname).duplicate_count++;
      continue;
    }
    if (wmOperatorType *ot = WM_operatortype_find(type->idname.c_str(), true)) {
      if ((ot->flag & OPTYPE_NODE_TOOL) == 0) {
        errors.lookup_or_add_default(type->custom_idname).is_builtin_operator = true;
        continue;
      }
      const OperatorTypeData &type_data = static_cast<const OperatorTypeData &>(*ot->custom_data);
      if (type_data.hash == type->hash) {
        continue;
      }
      types_to_remove.add(ot);
    }
    types_to_register.append(std::move(type));
  }

  /* Also remove old operators for now-unused identifier names. */
  for (wmOperatorType *ot : WM_operatortypes_registered_get()) {
    if ((ot->flag & OPTYPE_NODE_TOOL) == 0) {
      continue;
    }
    if (handled_types.contains(ot->idname)) {
      continue;
    }
    types_to_remove.add(ot);
  }

  if (!types_to_remove.is_empty()) {
    WM_operator_stack_clear(&wm, types_to_remove);
    WM_operator_handlers_clear(&wm, types_to_remove);
    for (wmOperatorType *ot : types_to_remove) {
      WM_operatortype_remove_ptr(ot);
    }
  }

  for (std::unique_ptr<OperatorTypeData> &type : types_to_register) {
    WM_operatortype_append_ptr(
        [](wmOperatorType *ot, void *user_data) {
          register_node_tool(ot, *static_cast<std::unique_ptr<OperatorTypeData> *>(user_data));
        },
        &type);
  }

  /* Don't display the same errors twice. That can be very noisy since this operator registration
   * process runs so often. */
  if (last_errors != errors) {
    ReportList *reports = CTX_wm_reports(&C);
    for (const OperatorRegisterErrors::Item &item : errors.items()) {
      if (item.value.is_builtin_operator) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Error registering node tool \"%s\", operator is already registered",
                    item.key.c_str());
      }
      if (item.value.duplicate_count != 0) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Error registering node tool \"%s\", %d duplicate(s)",
                    item.key.c_str(),
                    item.value.duplicate_count);
      }
      for (const std::string &error : item.value.idname_validation_errors) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Error registering node tool \"%s\", %s",
                    item.key.c_str(),
                    error.c_str());
      }
    }
    WM_report_banner_show(&wm, nullptr);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu
 * \{ */

static bool asset_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  return CTX_wm_view3d(C);
}

static GeometryNodeAssetTraitFlag asset_flag_for_context(const ObjectType type,
                                                         const eObjectMode mode)
{
  switch (type) {
    case OB_MESH: {
      switch (mode) {
        case OB_MODE_OBJECT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_OBJECT | GEO_NODE_ASSET_MESH);
        case OB_MODE_EDIT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_EDIT | GEO_NODE_ASSET_MESH);
        case OB_MODE_SCULPT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_SCULPT | GEO_NODE_ASSET_MESH);
        default:
          break;
      }
      break;
    }
    case OB_CURVES: {
      switch (mode) {
        case OB_MODE_OBJECT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_OBJECT | GEO_NODE_ASSET_CURVE);
        case OB_MODE_EDIT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_EDIT | GEO_NODE_ASSET_CURVE);
        case OB_MODE_SCULPT_CURVES:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_SCULPT | GEO_NODE_ASSET_CURVE);
        default:
          break;
      }
      break;
    }
    case OB_POINTCLOUD: {
      switch (mode) {
        case OB_MODE_OBJECT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_OBJECT | GEO_NODE_ASSET_POINTCLOUD);
        case OB_MODE_EDIT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_EDIT | GEO_NODE_ASSET_POINTCLOUD);
        default:
          break;
      }
      break;
    }
    case OB_GREASE_PENCIL: {
      switch (mode) {
        case OB_MODE_OBJECT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_OBJECT | GEO_NODE_ASSET_GREASE_PENCIL);
        case OB_MODE_EDIT:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_EDIT | GEO_NODE_ASSET_GREASE_PENCIL);
        case OB_MODE_SCULPT_GREASE_PENCIL:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_SCULPT | GEO_NODE_ASSET_GREASE_PENCIL);
        case OB_MODE_PAINT_GREASE_PENCIL:
          return (GEO_NODE_ASSET_TOOL | GEO_NODE_ASSET_PAINT | GEO_NODE_ASSET_GREASE_PENCIL);
        default:
          break;
      }
    }
    default:
      break;
  }
  return GeometryNodeAssetTraitFlag(0);
}

GeometryNodeAssetTraitFlag asset_flag_for_context(const Object &active_object)
{
  return asset_flag_for_context(ObjectType(active_object.type), eObjectMode(active_object.mode));
}

static asset::AssetItemTree *get_static_item_tree(const ObjectType type, const eObjectMode mode)
{
  switch (type) {
    case OB_MESH: {
      switch (mode) {
        case OB_MODE_OBJECT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_EDIT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_SCULPT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        default:
          return nullptr;
      }
    }
    case OB_CURVES: {
      switch (mode) {
        case OB_MODE_OBJECT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_EDIT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_SCULPT_CURVES: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        default:
          return nullptr;
      }
    }
    case OB_POINTCLOUD: {
      switch (mode) {
        case OB_MODE_OBJECT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_EDIT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        default:
          return nullptr;
      }
    }
    case OB_GREASE_PENCIL: {
      switch (mode) {
        case OB_MODE_OBJECT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_EDIT: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_SCULPT_GREASE_PENCIL: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        case OB_MODE_PAINT_GREASE_PENCIL: {
          static asset::AssetItemTree tree;
          return &tree;
        }
        default:
          return nullptr;
      }
    }
    default:
      return nullptr;
  }
}

static asset::AssetItemTree *get_static_item_tree(const Object &active_object)
{
  return get_static_item_tree(ObjectType(active_object.type), eObjectMode(active_object.mode));
}

void clear_operator_asset_trees()
{
  for (const ObjectType type : {OB_MESH, OB_CURVES, OB_POINTCLOUD, OB_GREASE_PENCIL}) {
    for (const eObjectMode mode : {OB_MODE_OBJECT,
                                   OB_MODE_EDIT,
                                   OB_MODE_SCULPT,
                                   OB_MODE_SCULPT_CURVES,
                                   OB_MODE_SCULPT_GREASE_PENCIL,
                                   OB_MODE_PAINT_GREASE_PENCIL})
    {
      if (asset::AssetItemTree *tree = get_static_item_tree(type, mode)) {
        tree->dirty = true;
      }
    }
  }
}

static asset::AssetItemTree build_catalog_tree(const bContext &C, const Object &active_object)
{
  asset::AssetFilterSettings type_filter{};
  type_filter.id_types = FILTER_ID_NT;
  const GeometryNodeAssetTraitFlag flag = asset_flag_for_context(active_object);
  auto meta_data_filter = [&](const AssetMetaData &meta_data) {
    const IDProperty *tree_type = BKE_asset_metadata_idprop_find(&meta_data, "type");
    if (tree_type == nullptr || IDP_int_get(tree_type) != NTREE_GEOMETRY) {
      return false;
    }
    const IDProperty *traits_flag = BKE_asset_metadata_idprop_find(
        &meta_data, "geometry_node_asset_traits_flag");
    if (traits_flag == nullptr || (IDP_int_get(traits_flag) & flag) != flag) {
      return false;
    }
    return true;
  };
  const AssetLibraryReference library = asset_system::all_library_reference();
  asset_system::all_library_reload_catalogs_if_dirty();
  return asset::build_filtered_all_catalog_tree(library, C, type_filter, meta_data_filter);
}

/**
 * Avoid adding a separate root catalog when the assets have already been added to one of the
 * builtin menus. The need to define the builtin menu labels here is non-ideal. We don't have
 * any UI introspection that can do this though.
 */
static Set<std::string> get_builtin_menus(const ObjectType object_type, const eObjectMode mode)
{
  Set<std::string> menus;
  switch (object_type) {
    case OB_CURVES:
      menus.add_new("View");
      menus.add_new("Select");
      menus.add_new("Curves");
      break;
    case OB_POINTCLOUD:
      menus.add_new("View");
      menus.add_new("Select");
      menus.add_new("Point Cloud");
      break;
    case OB_MESH:
      switch (mode) {
        case OB_MODE_OBJECT:
          menus.add_new("View");
          menus.add_new("Select");
          menus.add_new("Add");
          menus.add_new("Object");
          menus.add_new("Object/Apply");
          menus.add_new("Object/Convert");
          menus.add_new("Object/Quick Effects");
          break;
        case OB_MODE_EDIT:
          menus.add_new("View");
          menus.add_new("Select");
          menus.add_new("Add");
          menus.add_new("Mesh");
          menus.add_new("Mesh/Extrude");
          menus.add_new("Mesh/Clean Up");
          menus.add_new("Mesh/Delete");
          menus.add_new("Mesh/Merge");
          menus.add_new("Mesh/Normals");
          menus.add_new("Mesh/Shading");
          menus.add_new("Mesh/Split");
          menus.add_new("Mesh/Weights");
          menus.add_new("Vertex");
          menus.add_new("Edge");
          menus.add_new("Face");
          menus.add_new("Face/Face Data");
          menus.add_new("UV");
          menus.add_new("UV/Unwrap");
          break;
        case OB_MODE_SCULPT:
          menus.add_new("View");
          menus.add_new("Sculpt");
          menus.add_new("Mask");
          menus.add_new("Face Sets");
          break;
        case OB_MODE_VERTEX_PAINT:
          menus.add_new("View");
          menus.add_new("Paint");
          break;
        case OB_MODE_WEIGHT_PAINT:
          menus.add_new("View");
          menus.add_new("Weights");
          break;
        default:
          break;
      }
      break;
    case OB_GREASE_PENCIL: {
      switch (mode) {
        case OB_MODE_OBJECT:
          menus.add_new("View");
          menus.add_new("Select");
          menus.add_new("Add");
          menus.add_new("Object");
          menus.add_new("Object/Apply");
          menus.add_new("Object/Convert");
          menus.add_new("Object/Quick Effects");
          break;
        case OB_MODE_EDIT:
          menus.add_new("View");
          menus.add_new("Select");
          menus.add_new("Grease Pencil");
          menus.add_new("Stroke");
          menus.add_new("Point");
          break;
        case OB_MODE_SCULPT_GREASE_PENCIL:
          menus.add_new("View");
          break;
        case OB_MODE_PAINT_GREASE_PENCIL:
          menus.add_new("View");
          menus.add_new("Draw");
          break;
        default:
          break;
      }
    }
    default:
      break;
  }
  return menus;
}

static void missing_tool_idname_error(ui::Layout &layout, const StringRef name)
{
  layout.label(fmt::format(fmt::runtime(TIP_("Missing node tool identifier ({})")), name),
               ICON_NONE);
}

static void catalog_assets_draw(const bContext *C, Menu *menu)
{
  const Object *active_object = CTX_data_active_object(C);
  if (!active_object) {
    return;
  }
  asset::AssetItemTree *tree = get_static_item_tree(*active_object);
  if (!tree) {
    return;
  }
  const std::optional<StringRefNull> menu_path = CTX_data_string_get(C, "asset_catalog_path");
  if (!menu_path) {
    return;
  }
  const Span<asset_system::AssetRepresentation *> assets = tree->assets_per_path.lookup(
      menu_path->data());
  const asset_system::AssetCatalogTreeItem *catalog_item = tree->catalogs.find_item(
      menu_path->data());
  BLI_assert(catalog_item != nullptr);

  ui::Layout &layout = *menu->layout;
  bool add_separator = true;

  for (const asset_system::AssetRepresentation *asset : assets) {
    const std::optional<std::string> operator_idname = operator_idname_for_asset(*asset);
    if (!operator_idname) {
      missing_tool_idname_error(layout, asset->get_name());
      continue;
    }
    if (add_separator) {
      layout.separator();
      add_separator = false;
    }
    PointerRNA props_ptr = layout.op(*operator_idname,
                                     IFACE_(asset->get_name()),
                                     ICON_NONE,
                                     wm::OpCallContext::InvokeRegionWin,
                                     UI_ITEM_NONE);
  }

  const Set<std::string> builtin_menus = get_builtin_menus(ObjectType(active_object->type),
                                                           eObjectMode(active_object->mode));

  asset_system::AssetLibrary *all_library = asset::list::library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }

  catalog_item->foreach_child([&](const asset_system::AssetCatalogTreeItem &item) {
    if (builtin_menus.contains_as(item.catalog_path().str())) {
      return;
    }
    if (add_separator) {
      layout.separator();
      add_separator = false;
    }
    asset::draw_menu_for_catalog(item, "GEO_MT_node_operator_catalog_assets", layout);
  });
}

MenuType node_group_operator_assets_menu()
{
  MenuType type{};
  STRNCPY_UTF8(type.idname, "GEO_MT_node_operator_catalog_assets");
  type.poll = asset_menu_poll;
  type.draw = catalog_assets_draw;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  return type;
}

static bool unassigned_local_poll(const bContext &C)
{
  Main &bmain = *CTX_data_main(&C);
  const Object *active_object = CTX_data_active_object(&C);
  if (!active_object) {
    return false;
  }
  const GeometryNodeAssetTraitFlag flag = asset_flag_for_context(*active_object);
  for (const bNodeTree &group : bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (group.id.library_weak_reference || group.id.asset_data) {
      continue;
    }
    if (!group.geometry_node_asset_traits ||
        (group.geometry_node_asset_traits->flag & flag) != flag)
    {
      continue;
    }
    return true;
  }
  return false;
}

static void catalog_assets_draw_unassigned(const bContext *C, Menu *menu)
{
  const Object *active_object = CTX_data_active_object(C);
  if (!active_object) {
    return;
  }
  asset::AssetItemTree *tree = get_static_item_tree(*active_object);
  if (!tree) {
    return;
  }
  ui::Layout &layout = *menu->layout;
  for (const asset_system::AssetRepresentation *asset : tree->unassigned_assets) {
    const std::optional<std::string> operator_idname = operator_idname_for_asset(*asset);
    if (!operator_idname) {
      missing_tool_idname_error(layout, asset->get_name());
      continue;
    }
    layout.op(*operator_idname,
              IFACE_(asset->get_name()),
              ICON_NONE,
              wm::OpCallContext::InvokeRegionWin,
              UI_ITEM_NONE);
  }

  const GeometryNodeAssetTraitFlag flag = asset_flag_for_context(*active_object);

  bool first = true;
  bool add_separator = !tree->unassigned_assets.is_empty();
  Main &bmain = *CTX_data_main(C);
  for (const bNodeTree &group : bmain.nodetrees) {
    /* Assets are displayed in other menus, and non-local data-blocks aren't added to this menu. */
    if (group.id.library_weak_reference || group.id.asset_data) {
      continue;
    }
    if (!group.geometry_node_asset_traits ||
        (group.geometry_node_asset_traits->flag & flag) != flag)
    {
      continue;
    }
    const std::optional<std::string> operator_idname = operator_idname_for_group(group);
    if (!operator_idname) {
      missing_tool_idname_error(layout, BKE_id_name(group.id));
      continue;
    }
    if (add_separator) {
      layout.separator();
      add_separator = false;
    }
    if (first) {
      layout.label(IFACE_("Non-Assets"), ICON_NONE);
      first = false;
    }
    layout.op(*operator_idname,
              BKE_id_name(group.id),
              ICON_NONE,
              wm::OpCallContext::InvokeRegionWin,
              UI_ITEM_NONE);
  }
}

MenuType node_group_operator_assets_menu_unassigned()
{
  MenuType type{};
  STRNCPY_UTF8(type.label, N_("Unassigned Node Tools"));
  STRNCPY_UTF8(type.idname, "GEO_MT_node_operator_unassigned");
  type.poll = asset_menu_poll;
  type.draw = catalog_assets_draw_unassigned;
  type.listener = asset::list::asset_reading_region_listen_fn;
  type.flag = MenuTypeFlag::ContextDependent;
  type.description = N_(
      "Tool node group assets not assigned to a catalog.\n"
      "Catalogs can be assigned in the Asset Browser");
  return type;
}

void ui_template_node_operator_asset_menu_items(ui::Layout &layout,
                                                const bContext &C,
                                                const StringRef catalog_path)
{
  const Object *active_object = CTX_data_active_object(&C);
  if (!active_object) {
    return;
  }
  asset::AssetItemTree *tree = get_static_item_tree(*active_object);
  if (!tree) {
    return;
  }
  const asset_system::AssetCatalogTreeItem *item = tree->catalogs.find_item(catalog_path);
  if (!item) {
    return;
  }
  asset_system::AssetLibrary *all_library = asset::list::library_get_once_available(
      asset_system::all_library_reference());
  if (!all_library) {
    return;
  }
  ui::Layout &col = layout.column(false);
  col.context_string_set("asset_catalog_path", item->catalog_path().str());
  col.menu_contents("GEO_MT_node_operator_catalog_assets");
}

void ui_template_node_operator_asset_root_items(ui::Layout &layout, const bContext &C)
{
  const Object *active_object = CTX_data_active_object(&C);
  if (!active_object) {
    return;
  }
  asset::AssetItemTree *tree = get_static_item_tree(*active_object);
  if (!tree) {
    return;
  }
  if (tree->dirty) {
    *tree = build_catalog_tree(C, *active_object);
  }

  const Set<std::string> builtin_menus = get_builtin_menus(ObjectType(active_object->type),
                                                           eObjectMode(active_object->mode));

  tree->catalogs.foreach_root_item([&](const asset_system::AssetCatalogTreeItem &item) {
    if (!builtin_menus.contains_as(item.catalog_path().str())) {
      asset::draw_menu_for_catalog(item, "GEO_MT_node_operator_catalog_assets", layout);
    }
  });

  if (!tree->unassigned_assets.is_empty() || unassigned_local_poll(C)) {
    layout.menu("GEO_MT_node_operator_unassigned", "", ICON_FILE_HIDDEN);
  }
}

/** \} */

}  // namespace ed::geometry
}  // namespace blender
