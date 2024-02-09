/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_volume.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "UI_interface.hh"
#include "UI_tree_view.hh"

#include "WM_types.hh"

#include "BLT_translation.hh"

#include "spreadsheet_dataset_draw.hh"
#include "spreadsheet_draw.hh"
#include "spreadsheet_intern.hh"

namespace blender::ed::spreadsheet {

class GeometryDataSetTreeView;

class GeometryDataSetTreeViewItem : public ui::AbstractTreeViewItem {
  bke::GeometryComponent::Type component_type_;
  std::optional<int> layer_index_;
  std::optional<bke::AttrDomain> domain_;
  BIFIconID icon_;

 public:
  GeometryDataSetTreeViewItem(bke::GeometryComponent::Type component_type,
                              StringRef label,
                              BIFIconID icon);
  GeometryDataSetTreeViewItem(bke::GeometryComponent::Type component_type,
                              int layer_index,
                              StringRef label,
                              BIFIconID icon);
  GeometryDataSetTreeViewItem(bke::GeometryComponent::Type component_type,
                              bke::AttrDomain domain,
                              StringRef label,
                              BIFIconID icon);
  GeometryDataSetTreeViewItem(bke::GeometryComponent::Type component_type,
                              int layer_index,
                              bke::AttrDomain domain,
                              StringRef label,
                              BIFIconID icon);

  void on_activate(bContext &C) override;

  void build_row(uiLayout &row) override;

 protected:
  std::optional<bool> should_be_active() const override;
  bool supports_collapsing() const override;

 private:
  GeometryDataSetTreeView &get_tree() const;
  std::optional<int> count() const;
};

class GeometryDataSetTreeView : public ui::AbstractTreeView {
  bke::GeometrySet geometry_set_;
  SpaceSpreadsheet &sspreadsheet_;
  bScreen &screen_;

  friend class GeometryDataSetTreeViewItem;

 public:
  GeometryDataSetTreeView(bke::GeometrySet geometry_set, const bContext &C)
      : geometry_set_(std::move(geometry_set)),
        sspreadsheet_(*CTX_wm_space_spreadsheet(&C)),
        screen_(*CTX_wm_screen(&C))
  {
  }

  void build_grease_pencil()
  {
    if (!U.experimental.use_grease_pencil_version3) {
      return;
    }

    GeometryDataSetTreeViewItem &grease_pencil = this->add_tree_item<GeometryDataSetTreeViewItem>(
        bke::GeometryComponent::Type::GreasePencil,
        IFACE_("Grease Pencil"),
        ICON_OUTLINER_DATA_GREASEPENCIL);
    GeometryDataSetTreeViewItem &grease_pencil_layers =
        grease_pencil.add_tree_item<GeometryDataSetTreeViewItem>(
            bke::GeometryComponent::Type::GreasePencil,
            bke::AttrDomain::Layer,
            IFACE_("Layer"),
            ICON_OUTLINER_DATA_GP_LAYER);

    if (!geometry_set_.has_grease_pencil()) {
      return;
    }

    const Span<const bke::greasepencil::Layer *> layers =
        geometry_set_.get_grease_pencil()->layers();
    for (const int layer_i : layers.index_range()) {
      const bke::greasepencil::Layer *layer = layers[layer_i];
      GeometryDataSetTreeViewItem &curve =
          grease_pencil_layers.add_tree_item<GeometryDataSetTreeViewItem>(
              bke::GeometryComponent::Type::GreasePencil, layer_i, layer->name(), ICON_CURVE_DATA);
      curve.add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::GreasePencil,
                                                       layer_i,
                                                       bke::AttrDomain::Point,
                                                       IFACE_("Control Point"),
                                                       ICON_CURVE_BEZCIRCLE);
      curve.add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::GreasePencil,
                                                       layer_i,
                                                       bke::AttrDomain::Curve,
                                                       IFACE_("Spline"),
                                                       ICON_CURVE_PATH);
    }
  }

  void build_tree() override
  {
    GeometryDataSetTreeViewItem &mesh = this->add_tree_item<GeometryDataSetTreeViewItem>(
        bke::GeometryComponent::Type::Mesh, IFACE_("Mesh"), ICON_MESH_DATA);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::Mesh,
                                                    bke::AttrDomain::Point,
                                                    IFACE_("Vertex"),
                                                    ICON_VERTEXSEL);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(
        bke::GeometryComponent::Type::Mesh, bke::AttrDomain::Edge, IFACE_("Edge"), ICON_EDGESEL);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(
        bke::GeometryComponent::Type::Mesh, bke::AttrDomain::Face, IFACE_("Face"), ICON_FACESEL);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::Mesh,
                                                    bke::AttrDomain::Corner,
                                                    IFACE_("Face Corner"),
                                                    ICON_FACE_CORNER);

    GeometryDataSetTreeViewItem &curve = this->add_tree_item<GeometryDataSetTreeViewItem>(
        bke::GeometryComponent::Type::Curve, IFACE_("Curve"), ICON_CURVE_DATA);
    curve.add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::Curve,
                                                     bke::AttrDomain::Point,
                                                     IFACE_("Control Point"),
                                                     ICON_CURVE_BEZCIRCLE);
    curve.add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::Curve,
                                                     bke::AttrDomain::Curve,
                                                     IFACE_("Spline"),
                                                     ICON_CURVE_PATH);

    this->build_grease_pencil();

    GeometryDataSetTreeViewItem &pointcloud = this->add_tree_item<GeometryDataSetTreeViewItem>(
        bke::GeometryComponent::Type::PointCloud, IFACE_("Point Cloud"), ICON_POINTCLOUD_DATA);
    pointcloud.add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::PointCloud,
                                                          bke::AttrDomain::Point,
                                                          IFACE_("Point"),
                                                          ICON_POINTCLOUD_POINT);

    this->add_tree_item<GeometryDataSetTreeViewItem>(
        bke::GeometryComponent::Type::Volume, IFACE_("Volume Grids"), ICON_VOLUME_DATA);

    this->add_tree_item<GeometryDataSetTreeViewItem>(bke::GeometryComponent::Type::Instance,
                                                     bke::AttrDomain::Instance,
                                                     IFACE_("Instances"),
                                                     ICON_EMPTY_AXIS);
  }
};

GeometryDataSetTreeViewItem::GeometryDataSetTreeViewItem(
    bke::GeometryComponent::Type component_type, StringRef label, BIFIconID icon)
    : component_type_(component_type), domain_(std::nullopt), icon_(icon)
{
  label_ = label;
  this->set_collapsed(false);
}
GeometryDataSetTreeViewItem::GeometryDataSetTreeViewItem(
    bke::GeometryComponent::Type component_type, int layer_index, StringRef label, BIFIconID icon)
    : component_type_(component_type), layer_index_(layer_index), icon_(icon)
{
  label_ = label;
}
GeometryDataSetTreeViewItem::GeometryDataSetTreeViewItem(
    bke::GeometryComponent::Type component_type,
    bke::AttrDomain domain,
    StringRef label,
    BIFIconID icon)
    : component_type_(component_type), domain_(domain), icon_(icon)
{
  label_ = label;
}
GeometryDataSetTreeViewItem::GeometryDataSetTreeViewItem(
    bke::GeometryComponent::Type component_type,
    int layer_index,
    bke::AttrDomain domain,
    StringRef label,
    BIFIconID icon)
    : component_type_(component_type), layer_index_(layer_index), domain_(domain), icon_(icon)
{
  label_ = label;
}

void GeometryDataSetTreeViewItem::on_activate(bContext &C)
{
  GeometryDataSetTreeView &tree_view = this->get_tree();
  SpaceSpreadsheet &sspreadsheet = tree_view.sspreadsheet_;
  tree_view.sspreadsheet_.geometry_component_type = uint8_t(component_type_);
  if (domain_) {
    tree_view.sspreadsheet_.attribute_domain = uint8_t(*domain_);
  }
  if (layer_index_) {
    tree_view.sspreadsheet_.active_layer_index = *layer_index_;
  }
  PointerRNA ptr = RNA_pointer_create(&tree_view.screen_.id, &RNA_SpaceSpreadsheet, &sspreadsheet);
  RNA_property_update(&C, &ptr, RNA_struct_find_property(&ptr, "attribute_domain"));
  RNA_property_update(&C, &ptr, RNA_struct_find_property(&ptr, "geometry_component_type"));
}

void GeometryDataSetTreeViewItem::build_row(uiLayout &row)
{
  uiItemL(&row, label_.c_str(), icon_);

  if (const std::optional<int> count = this->count()) {
    /* Using the tree row button instead of a separate right aligned button gives padding
     * to the right side of the number, which it didn't have with the button. */
    char element_count[BLI_STR_FORMAT_INT32_DECIMAL_UNIT_SIZE];
    BLI_str_format_decimal_unit(element_count, *count);
    UI_but_hint_drawstr_set((uiBut *)this->view_item_button(), element_count);
  }
}

std::optional<bool> GeometryDataSetTreeViewItem::should_be_active() const
{
  GeometryDataSetTreeView &tree_view = this->get_tree();
  SpaceSpreadsheet &sspreadsheet = tree_view.sspreadsheet_;

  if (component_type_ == bke::GeometryComponent::Type::Volume) {
    return sspreadsheet.geometry_component_type == uint8_t(component_type_);
  }

  if (!domain_) {
    return false;
  }

  if (!layer_index_) {
    return sspreadsheet.geometry_component_type == uint8_t(component_type_) &&
           sspreadsheet.attribute_domain == uint8_t(*domain_);
  }

  return sspreadsheet.geometry_component_type == uint8_t(component_type_) &&
         sspreadsheet.attribute_domain == uint8_t(*domain_) &&
         sspreadsheet.active_layer_index == *layer_index_;
}

bool GeometryDataSetTreeViewItem::supports_collapsing() const
{
  return true;
}

GeometryDataSetTreeView &GeometryDataSetTreeViewItem::get_tree() const
{
  return static_cast<GeometryDataSetTreeView &>(this->get_tree_view());
}

std::optional<int> GeometryDataSetTreeViewItem::count() const
{
  GeometryDataSetTreeView &tree_view = this->get_tree();
  bke::GeometrySet &geometry = tree_view.geometry_set_;

  /* Special case for volumes since there is no grid domain. */
  if (component_type_ == bke::GeometryComponent::Type::Volume) {
    if (const Volume *volume = geometry.get_volume()) {
      return BKE_volume_num_grids(volume);
    }
    return 0;
  }

  if (!domain_) {
    return std::nullopt;
  }

  if (component_type_ == bke::GeometryComponent::Type::GreasePencil && layer_index_) {
    if (const bke::greasepencil::Drawing *drawing =
            bke::greasepencil::get_eval_grease_pencil_layer_drawing(*geometry.get_grease_pencil(),
                                                                    *layer_index_))
    {
      return drawing->strokes().attributes().domain_size(*domain_);
    }
  }

  if (const bke::GeometryComponent *component = geometry.get_component(component_type_)) {
    return component->attribute_domain_size(*domain_);
  }

  return 0;
}

void spreadsheet_data_set_panel_draw(const bContext *C, Panel *panel)
{
  const SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  Object *object = spreadsheet_get_object_eval(sspreadsheet, CTX_data_depsgraph_pointer(C));
  if (!object) {
    return;
  }
  uiLayout *layout = panel->layout;

  uiBlock *block = uiLayoutGetBlock(layout);

  UI_block_layout_set_current(block, layout);

  ui::AbstractTreeView *tree_view = UI_block_add_view(
      *block,
      "Data Set Tree View",
      std::make_unique<GeometryDataSetTreeView>(
          spreadsheet_get_display_geometry_set(sspreadsheet, object), *C));

  ui::TreeViewBuilder::build_tree_view(*tree_view, *layout);
}

}  // namespace blender::ed::spreadsheet
