/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_volume.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_tree_view.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLT_translation.hh"

#include "ED_outliner.hh"
#include "ED_spreadsheet.hh"

#include "NOD_geometry_nodes_bundle.hh"
#include "NOD_geometry_nodes_closure.hh"
#include "NOD_geometry_nodes_log.hh"

#include "spreadsheet_data_source_geometry.hh"
#include "spreadsheet_dataset_draw.hh"
#include "spreadsheet_intern.hh"

namespace blender::ed::spreadsheet {

class GeometryDataSetTreeView;
class GeometryInstancesTreeView;

struct GeometryDataIdentifier {
  bke::GeometryComponent::Type component_type;
  std::optional<int> layer_index;
  std::optional<bke::AttrDomain> domain;
};

static void draw_row_suffix(ui::AbstractTreeViewItem &view_item, const StringRefNull str)
{
  /* Using the tree row button instead of a separate right aligned button gives padding
   * to the right side of the number, which it didn't have with the button. */
  UI_but_hint_drawstr_set(reinterpret_cast<uiBut *>(view_item.view_item_button()), str.c_str());
}

static void draw_count(ui::AbstractTreeViewItem &view_item, const int count)
{
  char element_count[BLI_STR_FORMAT_INT32_DECIMAL_UNIT_SIZE];
  BLI_str_format_decimal_unit(element_count, count);
  draw_row_suffix(view_item, element_count);
}

static StringRefNull mesh_domain_to_label(const bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return IFACE_("Vertex");
    case bke::AttrDomain::Edge:
      return IFACE_("Edge");
    case bke::AttrDomain::Face:
      return IFACE_("Face");
    case bke::AttrDomain::Corner:
      return IFACE_("Face Corner");
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static StringRefNull curves_domain_to_label(const bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return IFACE_("Control Point");
    case bke::AttrDomain::Curve:
      return IFACE_("Spline");
    default:
      BLI_assert_unreachable();
      return "";
  }
}

static BIFIconID mesh_domain_to_icon(const bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return ICON_VERTEXSEL;
    case bke::AttrDomain::Edge:
      return ICON_EDGESEL;
    case bke::AttrDomain::Face:
      return ICON_FACESEL;
    case bke::AttrDomain::Corner:
      return ICON_FACE_CORNER;
    default:
      BLI_assert_unreachable();
      return ICON_NONE;
  }
}

static BIFIconID curves_domain_to_icon(const bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return ICON_CURVE_BEZCIRCLE;
    case bke::AttrDomain::Curve:
      return ICON_CURVE_PATH;
    default:
      BLI_assert_unreachable();
      return ICON_NONE;
  }
}

class InstancesTreeViewItem : public ui::AbstractTreeViewItem {
 public:
  GeometryInstancesTreeView &get_tree() const;

  void get_parent_instance_ids(Vector<SpreadsheetInstanceID> &r_instance_ids) const;

  void on_activate(bContext &C) override;

  std::optional<bool> should_be_active() const override;
};

class RootGeometryViewItem : public InstancesTreeViewItem {
 public:
  RootGeometryViewItem(const bke::GeometrySet &geometry)
  {
    label_ = geometry.name.empty() ? IFACE_("(Geometry)") : geometry.name;
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_GEOMETRY_SET);
  }
};

class InstanceReferenceViewItem : public InstancesTreeViewItem {
 private:
  const bke::InstanceReference &reference_;
  int reference_index_;
  int user_count_;

 public:
  InstanceReferenceViewItem(const bke::Instances &instances, const int reference_index)
      : reference_(instances.references()[reference_index]), reference_index_(reference_index)
  {
    label_ = std::to_string(reference_index);
    user_count_ = instances.reference_user_counts()[reference_index];
  }

  void build_row(uiLayout &row) override
  {
    const int icon = get_instance_reference_icon(reference_);
    StringRefNull name = reference_.name();
    if (name.is_empty()) {
      name = IFACE_("(Geometry)");
    }
    row.label(name, icon);
    draw_count(*this, user_count_);
  }

  int reference_index() const
  {
    return reference_index_;
  }
};

class GeometryInstancesTreeView : public ui::AbstractTreeView {
 private:
  ResourceScope scope_;
  bke::GeometrySet root_geometry_set_;
  SpaceSpreadsheet &sspreadsheet_;
  bScreen &screen_;

  friend class InstancesTreeViewItem;

 public:
  GeometryInstancesTreeView(bke::GeometrySet geometry_set, const bContext &C)
      : root_geometry_set_(std::move(geometry_set)),
        sspreadsheet_(*CTX_wm_space_spreadsheet(&C)),
        screen_(*CTX_wm_screen(&C))
  {
  }

  void build_tree() override
  {
    auto &root_item = this->add_tree_item<RootGeometryViewItem>(root_geometry_set_);
    root_item.uncollapse_by_default();
    if (const bke::Instances *instances = root_geometry_set_.get_instances()) {
      this->build_tree_for_instances(root_item, *instances);
    }
  }

  void build_tree_for_instances(ui::TreeViewItemContainer &parent, const bke::Instances &instances)
  {
    const Span<bke::InstanceReference> references = instances.references();
    for (const int reference_i : references.index_range()) {
      auto &reference_item = parent.add_tree_item<InstanceReferenceViewItem>(instances,
                                                                             reference_i);
      const bke::InstanceReference &reference = references[reference_i];
      bke::GeometrySet &reference_geometry = scope_.construct<bke::GeometrySet>();
      reference.to_geometry_set(reference_geometry);
      if (const bke::Instances *child_instances = reference_geometry.get_instances()) {
        this->build_tree_for_instances(reference_item, *child_instances);
      }
    }
  }
};

class DataSetViewItem : public ui::AbstractTreeViewItem {
 public:
  GeometryDataSetTreeView &get_tree() const;

  void on_activate(bContext &C) override;

  std::optional<bool> should_be_active() const override;

  /** Get information about which part of a geometry this item corresponds to. */
  virtual std::optional<GeometryDataIdentifier> get_geometry_data_id() const
  {
    return std::nullopt;
  }
};

class MeshViewItem : public DataSetViewItem {
 private:
  bool has_mesh_;

 public:
  MeshViewItem(const bool has_mesh) : has_mesh_(has_mesh)
  {
    label_ = IFACE_("Mesh");
  }

  void build_row(uiLayout &row) override
  {
    if (!has_mesh_) {
      row.active_set(false);
    }
    row.label(label_, ICON_MESH_DATA);
  }
};

class MeshDomainViewItem : public DataSetViewItem {
 private:
  const Mesh *mesh_;
  bke::AttrDomain domain_;

 public:
  MeshDomainViewItem(const Mesh *mesh, const bke::AttrDomain domain) : mesh_(mesh), domain_(domain)
  {
    label_ = mesh_domain_to_label(domain);
  }

  std::optional<GeometryDataIdentifier> get_geometry_data_id() const override
  {
    return GeometryDataIdentifier{bke::GeometryComponent::Type::Mesh, std::nullopt, domain_};
  }

  void build_row(uiLayout &row) override
  {
    const BIFIconID icon = mesh_domain_to_icon(domain_);
    row.label(label_, icon);

    const int count = mesh_ ? mesh_->attributes().domain_size(domain_) : 0;
    draw_count(*this, count);
  }
};

class CurvesViewItem : public DataSetViewItem {
 private:
  bool has_curves_;

 public:
  CurvesViewItem(const bool has_curves) : has_curves_(has_curves)
  {
    label_ = IFACE_("Curve");
  }

  void build_row(uiLayout &row) override
  {
    if (!has_curves_) {
      row.active_set(false);
    }
    row.label(label_, ICON_CURVE_DATA);
  }
};

class CurvesDomainViewItem : public DataSetViewItem {
 private:
  const Curves *curves_;
  bke::AttrDomain domain_;

 public:
  CurvesDomainViewItem(const Curves *curves, const bke::AttrDomain domain)
      : curves_(curves), domain_(domain)
  {
    label_ = curves_domain_to_label(domain);
  }

  std::optional<GeometryDataIdentifier> get_geometry_data_id() const override
  {
    return GeometryDataIdentifier{bke::GeometryComponent::Type::Curve, std::nullopt, domain_};
  }

  void build_row(uiLayout &row) override
  {
    const BIFIconID icon = curves_domain_to_icon(domain_);
    row.label(label_, icon);

    const int count = curves_ ? curves_->geometry.wrap().attributes().domain_size(domain_) : 0;
    draw_count(*this, count);
  }
};

class GreasePencilViewItem : public DataSetViewItem {
  bool has_grease_pencil_;

 public:
  GreasePencilViewItem(const bool has_grease_pencil) : has_grease_pencil_(has_grease_pencil)
  {
    label_ = IFACE_("Grease Pencil");
  }

  void build_row(uiLayout &row) override
  {
    if (!has_grease_pencil_) {
      row.active_set(false);
    }
    row.label(label_, ICON_OUTLINER_DATA_GREASEPENCIL);
  }
};

class GreasePencilLayersViewItem : public DataSetViewItem {
 private:
  const GreasePencil *grease_pencil_;

 public:
  GreasePencilLayersViewItem(const GreasePencil *grease_pencil) : grease_pencil_(grease_pencil)
  {
    label_ = IFACE_("Layer");
  }

  std::optional<GeometryDataIdentifier> get_geometry_data_id() const override
  {
    return GeometryDataIdentifier{
        bke::GeometryComponent::Type::GreasePencil, std::nullopt, bke::AttrDomain::Layer};
  }

  void build_row(uiLayout &row) override
  {
    const int count = grease_pencil_ ? grease_pencil_->layers().size() : 0;
    row.label(label_, ICON_OUTLINER_DATA_GP_LAYER);
    draw_count(*this, count);
  }
};

class GreasePencilLayerViewItem : public DataSetViewItem {
 private:
  const bke::greasepencil::Layer &layer_;

 public:
  GreasePencilLayerViewItem(const GreasePencil &grease_pencil, const int layer_index)
      : layer_(grease_pencil.layer(layer_index))
  {
    label_ = std::to_string(layer_index);
  }

  void build_row(uiLayout &row) override
  {
    StringRefNull name = layer_.name();
    if (name.is_empty()) {
      name = IFACE_("(Layer)");
    }
    row.label(name, ICON_CURVE_DATA);
  }
};

class GreasePencilLayerCurvesDomainViewItem : public DataSetViewItem {
 private:
  const GreasePencil &grease_pencil_;
  int layer_index_;
  bke::AttrDomain domain_;

 public:
  GreasePencilLayerCurvesDomainViewItem(const GreasePencil &grease_pencil,
                                        const int layer_index,
                                        const bke::AttrDomain domain)
      : grease_pencil_(grease_pencil), layer_index_(layer_index), domain_(domain)
  {
    label_ = curves_domain_to_label(domain);
  }

  std::optional<GeometryDataIdentifier> get_geometry_data_id() const override
  {
    return GeometryDataIdentifier{
        bke::GeometryComponent::Type::GreasePencil, layer_index_, domain_};
  }

  void build_row(uiLayout &row) override
  {
    const BIFIconID icon = curves_domain_to_icon(domain_);
    row.label(label_, icon);

    const bke::greasepencil::Drawing *drawing = grease_pencil_.get_eval_drawing(
        grease_pencil_.layer(layer_index_));
    const int count = drawing ? drawing->strokes().attributes().domain_size(domain_) : 0;
    draw_count(*this, count);
  }
};

class PointCloudViewItem : public DataSetViewItem {
 private:
  bool has_pointcloud_;

 public:
  PointCloudViewItem(const bool has_pointcloud) : has_pointcloud_(has_pointcloud)
  {
    label_ = IFACE_("Point Cloud");
  }

  void build_row(uiLayout &row) override
  {
    if (!has_pointcloud_) {
      row.active_set(false);
    }
    row.label(label_, ICON_POINTCLOUD_DATA);
  }
};

class PointsViewItem : public DataSetViewItem {
 private:
  const PointCloud *pointcloud_;

 public:
  PointsViewItem(const PointCloud *pointcloud) : pointcloud_(pointcloud)
  {
    label_ = IFACE_("Point");
  }

  std::optional<GeometryDataIdentifier> get_geometry_data_id() const override
  {
    return GeometryDataIdentifier{
        bke::GeometryComponent::Type::PointCloud, std::nullopt, bke::AttrDomain::Point};
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_POINTCLOUD_POINT);
    const int count = pointcloud_ ? pointcloud_->totpoint : 0;
    draw_count(*this, count);
  }
};

class VolumeGridsViewItem : public DataSetViewItem {
 private:
  const Volume *volume_;

 public:
  VolumeGridsViewItem(const Volume *volume) : volume_(volume)
  {
    label_ = IFACE_("Volume Grids");
  }

  std::optional<GeometryDataIdentifier> get_geometry_data_id() const override
  {
    return GeometryDataIdentifier{
        bke::GeometryComponent::Type::Volume, std::nullopt, std::nullopt};
  }

  void build_row(uiLayout &row) override
  {
    if (!volume_) {
      row.active_set(false);
    }
    row.label(label_, ICON_VOLUME_DATA);
    if (volume_) {
      draw_count(*this, BKE_volume_num_grids(volume_));
    }
  }
};

class InstancesViewItem : public DataSetViewItem {
 private:
  const bke::Instances *instances_;

 public:
  InstancesViewItem(const bke::Instances *instances) : instances_(instances)
  {
    label_ = IFACE_("Instances");
  }

  std::optional<GeometryDataIdentifier> get_geometry_data_id() const override
  {
    return GeometryDataIdentifier{
        bke::GeometryComponent::Type::Instance, std::nullopt, bke::AttrDomain::Instance};
  }

  void build_row(uiLayout &row) override
  {
    if (!instances_) {
      row.active_set(false);
    }
    row.label(label_, ICON_EMPTY_AXIS);
    if (instances_) {
      draw_count(*this, instances_->instances_num());
    }
  }
};

class GeometryDataSetTreeView : public ui::AbstractTreeView {
 private:
  bke::GeometrySet geometry_set_;
  SpaceSpreadsheet &sspreadsheet_;
  bScreen &screen_;

  friend class DataSetViewItem;

 public:
  GeometryDataSetTreeView(bke::GeometrySet geometry_set, const bContext &C)
      : geometry_set_(std::move(geometry_set)),
        sspreadsheet_(*CTX_wm_space_spreadsheet(&C)),
        screen_(*CTX_wm_screen(&C))
  {
  }

  void build_tree() override
  {
    this->build_tree_for_geometry(geometry_set_, *this);
  }

  void build_tree_for_geometry(const bke::GeometrySet &geometry, ui::TreeViewItemContainer &parent)
  {
    const Mesh *mesh = geometry.get_mesh();
    this->build_tree_for_mesh(mesh, parent);

    const Curves *curves = geometry.get_curves();
    this->build_tree_for_curves(curves, parent);

    const GreasePencil *grease_pencil = geometry.get_grease_pencil();
    this->build_tree_for_grease_pencil(grease_pencil, parent);

    const PointCloud *pointcloud = geometry.get_pointcloud();
    this->build_tree_for_pointcloud(pointcloud, parent);

    const Volume *volume = geometry.get_volume();
    this->build_tree_for_volume(volume, parent);

    const bke::Instances *instances = geometry.get_instances();
    this->build_tree_for_instances(instances, parent);
  }

  void build_tree_for_mesh(const Mesh *mesh, ui::TreeViewItemContainer &parent)
  {
    auto &mesh_item = parent.add_tree_item<MeshViewItem>(mesh != nullptr);
    mesh_item.uncollapse_by_default();
    if (!mesh) {
      return;
    }

    mesh_item.add_tree_item<MeshDomainViewItem>(mesh, bke::AttrDomain::Point);
    mesh_item.add_tree_item<MeshDomainViewItem>(mesh, bke::AttrDomain::Edge);
    mesh_item.add_tree_item<MeshDomainViewItem>(mesh, bke::AttrDomain::Face);
    mesh_item.add_tree_item<MeshDomainViewItem>(mesh, bke::AttrDomain::Corner);
  }

  void build_tree_for_curves(const Curves *curves, ui::TreeViewItemContainer &parent)
  {
    auto &curves_item = parent.add_tree_item<CurvesViewItem>(curves != nullptr);
    curves_item.uncollapse_by_default();
    if (!curves) {
      return;
    }

    curves_item.add_tree_item<CurvesDomainViewItem>(curves, bke::AttrDomain::Point);
    curves_item.add_tree_item<CurvesDomainViewItem>(curves, bke::AttrDomain::Curve);
  }

  void build_tree_for_grease_pencil(const GreasePencil *grease_pencil,
                                    ui::TreeViewItemContainer &parent)
  {
    auto &grease_pencil_item = parent.add_tree_item<GreasePencilViewItem>(grease_pencil !=
                                                                          nullptr);
    grease_pencil_item.uncollapse_by_default();
    if (!grease_pencil) {
      return;
    }

    auto &layers_item = grease_pencil_item.add_tree_item<GreasePencilLayersViewItem>(
        grease_pencil);
    const Span<const bke::greasepencil::Layer *> layers = grease_pencil->layers();
    for (const int layer_i : layers.index_range()) {
      auto &layer_item = layers_item.add_tree_item<GreasePencilLayerViewItem>(*grease_pencil,
                                                                              layer_i);
      layer_item.add_tree_item<GreasePencilLayerCurvesDomainViewItem>(
          *grease_pencil, layer_i, bke::AttrDomain::Point);
      layer_item.add_tree_item<GreasePencilLayerCurvesDomainViewItem>(
          *grease_pencil, layer_i, bke::AttrDomain::Curve);
    }
  }

  void build_tree_for_pointcloud(const PointCloud *pointcloud, ui::TreeViewItemContainer &parent)
  {
    auto &pointcloud_item = parent.add_tree_item<PointCloudViewItem>(pointcloud != nullptr);
    pointcloud_item.uncollapse_by_default();
    if (!pointcloud) {
      return;
    }

    pointcloud_item.add_tree_item<PointsViewItem>(pointcloud);
  }

  void build_tree_for_volume(const Volume *volume, ui::TreeViewItemContainer &parent)
  {
    parent.add_tree_item<VolumeGridsViewItem>(volume);
  }

  void build_tree_for_instances(const bke::Instances *instances, ui::TreeViewItemContainer &parent)
  {
    parent.add_tree_item<InstancesViewItem>(instances);
  }
};

GeometryDataSetTreeView &DataSetViewItem::get_tree() const
{
  return static_cast<GeometryDataSetTreeView &>(this->get_tree_view());
}

GeometryInstancesTreeView &InstancesTreeViewItem::get_tree() const
{
  return static_cast<GeometryInstancesTreeView &>(this->get_tree_view());
}

void InstancesTreeViewItem::get_parent_instance_ids(
    Vector<SpreadsheetInstanceID> &r_instance_ids) const
{
  if (const auto *reference_item = dynamic_cast<const InstanceReferenceViewItem *>(this)) {
    r_instance_ids.append({reference_item->reference_index()});
  }
  this->foreach_parent([&](const ui::AbstractTreeViewItem &item) {
    if (const auto *reference_item = dynamic_cast<const InstanceReferenceViewItem *>(&item)) {
      r_instance_ids.append({reference_item->reference_index()});
    }
  });
  std::reverse(r_instance_ids.begin(), r_instance_ids.end());
}

std::optional<bool> InstancesTreeViewItem::should_be_active() const
{
  GeometryInstancesTreeView &tree_view = this->get_tree();
  SpaceSpreadsheet &sspreadsheet = tree_view.sspreadsheet_;

  Vector<SpreadsheetInstanceID> instance_ids;
  this->get_parent_instance_ids(instance_ids);
  if (sspreadsheet.geometry_id.instance_ids_num != instance_ids.size()) {
    return false;
  }
  for (const int i : instance_ids.index_range()) {
    const SpreadsheetInstanceID &a = sspreadsheet.geometry_id.instance_ids[i];
    const SpreadsheetInstanceID &b = instance_ids[i];
    if (a.reference_index != b.reference_index) {
      return false;
    }
  }
  return true;
}

void InstancesTreeViewItem::on_activate(bContext &C)
{
  Vector<SpreadsheetInstanceID> instance_ids;
  this->get_parent_instance_ids(instance_ids);

  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(&C);

  MEM_SAFE_FREE(sspreadsheet.geometry_id.instance_ids);
  sspreadsheet.geometry_id.instance_ids = MEM_calloc_arrayN<SpreadsheetInstanceID>(
      instance_ids.size(), __func__);
  sspreadsheet.geometry_id.instance_ids_num = instance_ids.size();
  initialized_copy_n(
      instance_ids.data(), instance_ids.size(), sspreadsheet.geometry_id.instance_ids);

  WM_main_add_notifier(NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);
}

void DataSetViewItem::on_activate(bContext &C)
{
  std::optional<GeometryDataIdentifier> data_id = this->get_geometry_data_id();
  if (!data_id) {
    this->foreach_item_recursive([&](const ui::AbstractTreeViewItem &item) {
      if (data_id) {
        return;
      }
      if (const auto *data_set_view_item = dynamic_cast<const DataSetViewItem *>(&item)) {
        data_id = data_set_view_item->get_geometry_data_id();
      }
    });
    if (!data_id) {
      return;
    }
  }

  bScreen &screen = *CTX_wm_screen(&C);
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(&C);

  sspreadsheet.geometry_id.geometry_component_type = uint8_t(data_id->component_type);
  if (data_id->domain) {
    sspreadsheet.geometry_id.attribute_domain = uint8_t(*data_id->domain);
  }
  if (data_id->layer_index) {
    sspreadsheet.geometry_id.layer_index = *data_id->layer_index;
  }
  PointerRNA ptr = RNA_pointer_create_discrete(&screen.id, &RNA_SpaceSpreadsheet, &sspreadsheet);
  /* These updates also make sure that the attribute domain is set properly based on the
   * component type. */
  RNA_property_update(&C, &ptr, RNA_struct_find_property(&ptr, "attribute_domain"));
  RNA_property_update(&C, &ptr, RNA_struct_find_property(&ptr, "geometry_component_type"));
}

std::optional<bool> DataSetViewItem::should_be_active() const
{
  GeometryDataSetTreeView &tree_view = this->get_tree();
  SpaceSpreadsheet &sspreadsheet = tree_view.sspreadsheet_;

  const std::optional<GeometryDataIdentifier> data_id = this->get_geometry_data_id();
  if (!data_id) {
    return false;
  }
  if (bke::GeometryComponent::Type(sspreadsheet.geometry_id.geometry_component_type) !=
      data_id->component_type)
  {
    return false;
  }
  if (data_id->domain) {
    if (bke::AttrDomain(sspreadsheet.geometry_id.attribute_domain) != data_id->domain) {
      return false;
    }
  }
  if (data_id->layer_index) {
    if (sspreadsheet.geometry_id.layer_index != *data_id->layer_index) {
      return false;
    }
  }
  return true;
}

class ViewerPathTreeViewItem : public ui::AbstractTreeViewItem {
 private:
  int viewer_path_index_;

 public:
  ViewerPathTreeViewItem(int viewer_path_index) : viewer_path_index_(viewer_path_index) {}

  void on_activate(bContext &C) override;
  std::optional<bool> should_be_active() const override;
};

class IDViewerPathItem : public ViewerPathTreeViewItem {
  const IDViewerPathElem &id_elem_;

 public:
  IDViewerPathItem(const int viewer_path_index, const IDViewerPathElem &id_elem)
      : ViewerPathTreeViewItem(viewer_path_index), id_elem_(id_elem)
  {
    label_ = id_elem.id ? id_elem.id->name + 2 : IFACE_("No Data-Block");
  }

  void build_row(uiLayout &row) override
  {
    if (id_elem_.id) {
      const int icon = ED_outliner_icon_from_id(*id_elem_.id);
      row.label(BKE_id_name(*id_elem_.id), icon);
    }
    else {
      row.label(IFACE_("No Data-Block"), ICON_BLANK1);
    }
  }
};

class ModifierViewerPathItem : public ViewerPathTreeViewItem {
  const ModifierViewerPathElem &modifier_elem_;

 public:
  ModifierViewerPathItem(const int viewer_path_index, const ModifierViewerPathElem &modifier_elem)
      : ViewerPathTreeViewItem(viewer_path_index), modifier_elem_(modifier_elem)
  {
    label_ = modifier_elem.base.ui_name;
  }

  void build_row(uiLayout &row) override
  {
    row.label(modifier_elem_.base.ui_name, ICON_MODIFIER);
  }
};

class GroupNodeViewerPathItem : public ViewerPathTreeViewItem {
  const GroupNodeViewerPathElem &group_node_elem_;

 public:
  GroupNodeViewerPathItem(const int viewer_path_index,
                          const GroupNodeViewerPathElem &group_node_elem)
      : ViewerPathTreeViewItem(viewer_path_index), group_node_elem_(group_node_elem)
  {
    label_ = group_node_elem.base.ui_name;
  }

  void build_row(uiLayout &row) override
  {
    row.label(group_node_elem_.base.ui_name, ICON_NODE);
  }
};

class ViewerNodeViewerPathItem : public ViewerPathTreeViewItem {
  const ViewerNodeViewerPathElem &viewer_node_elem_;

 public:
  ViewerNodeViewerPathItem(const int viewer_path_index,
                           const ViewerNodeViewerPathElem &viewer_node_elem)
      : ViewerPathTreeViewItem(viewer_path_index), viewer_node_elem_(viewer_node_elem)
  {
    label_ = viewer_node_elem.base.ui_name;
  }

  void build_row(uiLayout &row) override
  {
    row.label(viewer_node_elem_.base.ui_name, ICON_RESTRICT_VIEW_OFF);
  }
};

class SimulationViewerPathPathItem : public ViewerPathTreeViewItem {

 public:
  SimulationViewerPathPathItem(const int viewer_path_index,
                               const SimulationZoneViewerPathElem & /*simulation_zone_elem*/)
      : ViewerPathTreeViewItem(viewer_path_index)
  {
    label_ = IFACE_("Simulation");
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_BLANK1);
  }
};

class RepeatViewerPathItem : public ViewerPathTreeViewItem {
 private:
  const RepeatZoneViewerPathElem &repeat_zone_;

 public:
  RepeatViewerPathItem(const int viewer_path_index, const RepeatZoneViewerPathElem &repeat_zone)
      : ViewerPathTreeViewItem(viewer_path_index), repeat_zone_(repeat_zone)
  {
    label_ = IFACE_("Repeat");
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_BLANK1);
    draw_row_suffix(*this, std::to_string(repeat_zone_.iteration));
  }
};

class ForeachElementViewerPathItem : public ViewerPathTreeViewItem {
 private:
  const ForeachGeometryElementZoneViewerPathElem &foreach_geo_elem_zone_;

 public:
  ForeachElementViewerPathItem(
      const int viewer_path_index,
      const ForeachGeometryElementZoneViewerPathElem &foreach_geo_elem_zone)
      : ViewerPathTreeViewItem(viewer_path_index), foreach_geo_elem_zone_(foreach_geo_elem_zone)
  {
    label_ = IFACE_("For Each Element");
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_BLANK1);
    draw_row_suffix(*this, std::to_string(foreach_geo_elem_zone_.index));
  }
};

class EvaluteClosureViewerPathItem : public ViewerPathTreeViewItem {
 public:
  EvaluteClosureViewerPathItem(const int viewer_path_index,
                               const EvaluateClosureNodeViewerPathElem & /*evalute_closure_elem*/)
      : ViewerPathTreeViewItem(viewer_path_index)
  {
    label_ = IFACE_("Evaluate Closure");
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_BLANK1);
  }
};

class ViewerPathTreeView : public ui::AbstractTreeView {
 private:
  SpaceSpreadsheet &sspreadsheet_;
  bScreen &screen_;

  friend ViewerPathTreeViewItem;

 public:
  ViewerPathTreeView(const bContext &C)
      : sspreadsheet_(*CTX_wm_space_spreadsheet(&C)), screen_(*CTX_wm_screen(&C))
  {
    /* This tree view contains only a flat list of items without. */
    is_flat_ = true;
  }

  void build_tree() override
  {
    const ViewerPath &viewer_path = sspreadsheet_.geometry_id.viewer_path;

    int index;
    LISTBASE_FOREACH_INDEX (const ViewerPathElem *, elem, &viewer_path.path, index) {
      if (elem == viewer_path.path.first) {
        /* The root item is drawn above the tree view already. */
        continue;
      }
      this->add_viewer_path_elem(index, *elem);
    }
  }

  void add_viewer_path_elem(const int index, const ViewerPathElem &elem)
  {
    switch (ViewerPathElemType(elem.type)) {
      case VIEWER_PATH_ELEM_TYPE_ID: {
        this->add_tree_item<IDViewerPathItem>(index,
                                              reinterpret_cast<const IDViewerPathElem &>(elem));
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
        this->add_tree_item<ModifierViewerPathItem>(
            index, reinterpret_cast<const ModifierViewerPathElem &>(elem));
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
        this->add_tree_item<GroupNodeViewerPathItem>(
            index, reinterpret_cast<const GroupNodeViewerPathElem &>(elem));
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE: {
        this->add_tree_item<ViewerNodeViewerPathItem>(
            index, reinterpret_cast<const ViewerNodeViewerPathElem &>(elem));
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
        this->add_tree_item<SimulationViewerPathPathItem>(
            index, reinterpret_cast<const SimulationZoneViewerPathElem &>(elem));
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE: {
        this->add_tree_item<RepeatViewerPathItem>(
            index, reinterpret_cast<const RepeatZoneViewerPathElem &>(elem));
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE: {
        this->add_tree_item<ForeachElementViewerPathItem>(
            index, reinterpret_cast<const ForeachGeometryElementZoneViewerPathElem &>(elem));
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE: {
        this->add_tree_item<EvaluteClosureViewerPathItem>(
            index, reinterpret_cast<const EvaluateClosureNodeViewerPathElem &>(elem));
        break;
      }
    }
  }
};

void ViewerPathTreeViewItem::on_activate(bContext &C)
{
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(&C);
  sspreadsheet.active_viewer_path_index = viewer_path_index_;
  WM_main_add_notifier(NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);
}

std::optional<bool> ViewerPathTreeViewItem::should_be_active() const
{
  /* Can use SpaceSpreadsheet.active_viewer_path_index once selection is used. */
  return false;
}

class ViewerDataTreeItem : public ui::AbstractTreeViewItem {
 public:
  Vector<const ViewerDataTreeItem *> items_path() const
  {
    Vector<const ViewerDataTreeItem *> items;
    this->foreach_parent([&](const ui::AbstractTreeViewItem &parent) {
      items.append(dynamic_cast<const ViewerDataTreeItem *>(&parent));
    });
    items.as_mutable_span().reverse();
    items.append(this);
    return items;
  }

  void on_activate(bContext &C) override;
  std::optional<bool> should_be_active() const override;
};

struct ViewerDataPath {
  int viewer_item;
  Vector<StringRef> bundles;
  SpreadsheetClosureInputOutput closure_input_output = SPREADSHEET_CLOSURE_NONE;
  BLI_STRUCT_EQUALITY_OPERATORS_3(ViewerDataPath, viewer_item, bundles, closure_input_output);

  ViewerDataPath() = default;
  explicit ViewerDataPath(const SpreadsheetTableIDGeometry &table_id)
      : viewer_item(table_id.viewer_item_identifier)
  {
    for (const auto &elem : Span(table_id.bundle_path, table_id.bundle_path_num)) {
      this->bundles.append(elem.identifier);
    }
    this->closure_input_output = SpreadsheetClosureInputOutput(table_id.closure_input_output);
  }

  explicit ViewerDataPath(const Span<const ViewerDataTreeItem *> tree_items);

  void store(SpreadsheetTableIDGeometry &table_id)
  {
    table_id.viewer_item_identifier = this->viewer_item;
    if (table_id.bundle_path) {
      for (const int i : IndexRange(table_id.bundle_path_num)) {
        MEM_freeN(table_id.bundle_path[i].identifier);
      }
      MEM_freeN(table_id.bundle_path);
    }
    table_id.bundle_path = MEM_calloc_arrayN<SpreadsheetBundlePathElem>(this->bundles.size(),
                                                                        __func__);
    table_id.bundle_path_num = this->bundles.size();
    for (const int i : this->bundles.index_range()) {
      table_id.bundle_path[i].identifier = BLI_strdupn(this->bundles[i].data(),
                                                       this->bundles[i].size());
    }
    table_id.closure_input_output = int8_t(this->closure_input_output);
  }
};

class ViewerNodeItem : public ViewerDataTreeItem {
 private:
  const nodes::geo_eval_log::ViewerNodeLog::Item &item_;

  friend ViewerDataPath;

 public:
  ViewerNodeItem(const nodes::geo_eval_log::ViewerNodeLog::Item &item) : item_(item)
  {
    label_ = std::to_string(item.identifier);
  }

  void build_row(uiLayout &row) override
  {
    row.label(item_.name, ICON_NONE);
  }
};

class BundleItem : public ViewerDataTreeItem {
  friend ViewerDataPath;

 public:
  BundleItem(const StringRef key)
  {
    label_ = key;
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_NONE);
  }
};

class ClosureInputOutputItem : public ViewerDataTreeItem {
 private:
  SpreadsheetClosureInputOutput in_out_;

  friend ViewerDataPath;

 public:
  ClosureInputOutputItem(const SpreadsheetClosureInputOutput in_out) : in_out_(in_out)
  {
    label_ = in_out_ == SPREADSHEET_CLOSURE_INPUT ? IFACE_("Inputs") : IFACE_("Outputs");
  }

  void build_row(uiLayout &row) override
  {
    row.label(label_, ICON_NONE);
  }
};

ViewerDataPath::ViewerDataPath(const Span<const ViewerDataTreeItem *> tree_items)
{
  for (const ViewerDataTreeItem *item : tree_items) {
    if (const auto *viewer_node_item = dynamic_cast<const ViewerNodeItem *>(item)) {
      this->viewer_item = viewer_node_item->item_.identifier;
    }
    else if (const auto *bundle_item = dynamic_cast<const BundleItem *>(item)) {
      this->bundles.append(bundle_item->label_);
    }
    else if (const auto *bundle_item = dynamic_cast<const ClosureInputOutputItem *>(item)) {
      this->closure_input_output = bundle_item->in_out_;
    }
  }
}

class ViewerDataTreeView : public ui::AbstractTreeView {
 private:
  SpaceSpreadsheet &sspreadsheet_;

  friend ViewerDataTreeItem;

 public:
  ViewerDataTreeView(const bContext &C) : sspreadsheet_(*CTX_wm_space_spreadsheet(&C)) {}

  void build_tree() override
  {
    const nodes::geo_eval_log::ViewerNodeLog *log = viewer_node_log_lookup(sspreadsheet_);
    if (!log) {
      return;
    }
    for (const nodes::geo_eval_log::ViewerNodeLog::Item &item : log->items) {
      const bke::SocketValueVariant &value = item.value;
      auto &child_item = this->add_tree_item<ViewerNodeItem>(item);
      this->build_value(child_item, value);
    }
  }

  void build_value(ui::AbstractTreeViewItem &parent, const bke::SocketValueVariant &value)
  {
    if (!value.is_single()) {
      return;
    }
    const GPointer single_value = value.get_single_ptr();
    if (single_value.is_type<nodes::BundlePtr>()) {
      const nodes::BundlePtr &bundle_ptr = *single_value.get<nodes::BundlePtr>();
      if (bundle_ptr) {
        this->build_bundle_children(parent, *bundle_ptr);
      }
    }
    if (single_value.is_type<nodes::ClosurePtr>()) {
      const nodes::ClosurePtr &closure_ptr = *single_value.get<nodes::ClosurePtr>();
      if (closure_ptr) {
        this->build_closure_children(parent, closure_ptr);
      }
    }
  }

  void build_bundle_children(ui::AbstractTreeViewItem &parent, const nodes::Bundle &bundle)
  {
    for (const nodes::Bundle::StoredItem &item : bundle.items()) {
      auto &child_item = parent.add_tree_item<BundleItem>(item.key);
      const auto *stored_value = std::get_if<nodes::BundleItemSocketValue>(&item.value.value);
      if (!stored_value) {
        continue;
      }
      this->build_value(child_item, stored_value->value);
    }
  }

  void build_closure_children(ui::AbstractTreeViewItem &parent, const nodes::ClosurePtr &closure)
  {
    const nodes::ClosureSignature &signature = closure->signature();
    if (!signature.inputs.is_empty()) {
      parent.add_tree_item<ClosureInputOutputItem>(SPREADSHEET_CLOSURE_INPUT);
    }
    if (!signature.outputs.is_empty()) {
      parent.add_tree_item<ClosureInputOutputItem>(SPREADSHEET_CLOSURE_OUTPUT);
    }
  }
};

void ViewerDataTreeItem::on_activate(bContext & /*C*/)
{
  const auto &tree = static_cast<const ViewerDataTreeView &>(this->get_tree_view());
  SpaceSpreadsheet &sspreadsheet = tree.sspreadsheet_;
  SpreadsheetTableIDGeometry &table_id = sspreadsheet.geometry_id;
  ViewerDataPath(this->items_path()).store(table_id);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);
}

std::optional<bool> ViewerDataTreeItem::should_be_active() const
{
  const auto &tree = static_cast<const ViewerDataTreeView &>(this->get_tree_view());
  const SpaceSpreadsheet &sspreadsheet = tree.sspreadsheet_;
  const SpreadsheetTableIDGeometry &table_id = sspreadsheet.geometry_id;
  return ViewerDataPath(table_id) == ViewerDataPath(this->items_path());
}

static void draw_context_panel_without_context(uiLayout &layout)
{
  layout.label(IFACE_("No Active Context"), ICON_NONE);
}

static bool viewer_path_ends_with_viewer_node(const ViewerPath &viewer_path)
{
  if (BLI_listbase_is_empty(&viewer_path.path)) {
    return false;
  }
  const ViewerPathElem &last_elem = *static_cast<const ViewerPathElem *>(viewer_path.path.last);
  return ViewerPathElemType(last_elem.type) == VIEWER_PATH_ELEM_TYPE_VIEWER_NODE;
}

static void draw_viewer_path_panel(const bContext &C, uiLayout &layout)
{
  uiBlock *block = layout.block();
  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block, "Viewer Path", std::make_unique<ViewerPathTreeView>(C));
  tree_view->set_context_menu_title("Viewer Path");
  ui::TreeViewBuilder::build_tree_view(C, *tree_view, layout, true);
}

static void draw_viewer_data_panel(const bContext &C, uiLayout &layout)
{
  uiBlock *block = layout.block();
  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block, "Viewer Data", std::make_unique<ViewerDataTreeView>(C));
  tree_view->set_context_menu_title("Viewer Data");
  ui::TreeViewBuilder::build_tree_view(C, *tree_view, layout, false);
}

static void draw_context_panel_content(const bContext &C, uiLayout &layout)
{
  bScreen &screen = *CTX_wm_screen(&C);
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(&C);

  ViewerPath &viewer_path = sspreadsheet->geometry_id.viewer_path;
  ID *root_id = get_current_id(sspreadsheet);
  if (!root_id) {
    draw_context_panel_without_context(layout);
    return;
  }
  if (GS(root_id->name) != ID_OB) {
    draw_context_panel_without_context(layout);
    return;
  }

  PointerRNA sspreadsheet_ptr = RNA_pointer_create_discrete(
      &screen.id, &RNA_SpaceSpreadsheet, sspreadsheet);

  layout.prop(&sspreadsheet_ptr, "object_eval_state", UI_ITEM_NONE, "", ICON_NONE);

  if (sspreadsheet->geometry_id.object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE &&
      viewer_path_ends_with_viewer_node(viewer_path))
  {
    if (uiLayout *panel = layout.panel(&C, "viewer path", true, IFACE_("Viewer Path"))) {
      draw_viewer_path_panel(C, *panel);
    }
    if (uiLayout *panel = layout.panel(&C, "viewer data", true, IFACE_("Viewer Data"))) {
      draw_viewer_data_panel(C, *panel);
    }
  }
}

static void draw_context_panel(const bContext &C, uiLayout &layout)
{
  SpaceSpreadsheet &sspreadsheet = *CTX_wm_space_spreadsheet(&C);

  PanelLayout context_panel = layout.panel(&C, "context", false);
  context_panel.header->emboss_set(ui::EmbossType::None);
  if (ID *root_id = get_current_id(&sspreadsheet)) {
    std::string label = BKE_id_name(*root_id);
    if (!context_panel.body) {
      switch (sspreadsheet.geometry_id.object_eval_state) {
        case SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED:
          label += " (Evaluated)";
          break;
        case SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL:
          label += " (Original)";
          break;
        case SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE:
          label += " (Viewer)";
          break;
      }
    }
    context_panel.header->label(label, ICON_OBJECT_DATA);
  }
  else {
    context_panel.header->label(IFACE_("Context"), ICON_NONE);
  }
  context_panel.header->op("spreadsheet.toggle_pin",
                           "",
                           sspreadsheet.flag & SPREADSHEET_FLAG_PINNED ? ICON_PINNED :
                                                                         ICON_UNPINNED);
  if (context_panel.body) {
    draw_context_panel_content(C, *context_panel.body);
  }
}

void spreadsheet_data_set_panel_draw(const bContext *C, Panel *panel)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);

  uiLayout *layout = panel->layout;
  uiBlock *block = layout->block();
  ui::block_layout_set_current(block, layout);

  draw_context_panel(*C, *layout);

  Object *object = spreadsheet_get_object_eval(sspreadsheet, CTX_data_depsgraph_pointer(C));
  if (!object) {
    return;
  }

  if (const std::optional<bke::GeometrySet> root_geometry = root_geometry_set_get(sspreadsheet,
                                                                                  object))
  {
    if (uiLayout *panel = layout->panel(C, "instance tree", false, IFACE_("Geometry"))) {
      ui::AbstractTreeView *tree_view = UI_block_add_view(
          *block,
          "Instances Tree View",
          std::make_unique<GeometryInstancesTreeView>(*root_geometry, *C));
      tree_view->set_context_menu_title("Instance");
      ui::TreeViewBuilder::build_tree_view(*C, *tree_view, *panel, false);
    }
    if (uiLayout *panel = layout->panel(C, "geometry_domain_tree_view", false, IFACE_("Domain"))) {
      bke::GeometrySet instance_geometry = get_geometry_set_for_instance_ids(
          *root_geometry,
          {sspreadsheet->geometry_id.instance_ids, sspreadsheet->geometry_id.instance_ids_num});
      ui::AbstractTreeView *tree_view = UI_block_add_view(
          *block,
          "Data Set Tree View",
          std::make_unique<GeometryDataSetTreeView>(std::move(instance_geometry), *C));
      tree_view->set_context_menu_title("Domain");
      ui::TreeViewBuilder::build_tree_view(*C, *tree_view, *panel, false);
    }
  }
}

}  // namespace blender::ed::spreadsheet
