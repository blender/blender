/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_volume.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_tree_view.hh"

#include "WM_types.h"

#include "BLT_translation.h"

#include "spreadsheet_dataset_draw.hh"
#include "spreadsheet_draw.hh"
#include "spreadsheet_intern.hh"

namespace blender::ed::spreadsheet {

class GeometryDataSetTreeView;

class GeometryDataSetTreeViewItem : public ui::AbstractTreeViewItem {
  GeometryComponentType component_type_;
  std::optional<eAttrDomain> domain_;
  BIFIconID icon_;

 public:
  GeometryDataSetTreeViewItem(GeometryComponentType component_type,
                              StringRef label,
                              BIFIconID icon);
  GeometryDataSetTreeViewItem(GeometryComponentType component_type,
                              eAttrDomain domain,
                              StringRef label,
                              BIFIconID icon);

  void on_activate() override;

  void build_row(uiLayout &row) override;

 protected:
  std::optional<bool> should_be_active() const override;
  bool supports_collapsing() const override;

 private:
  GeometryDataSetTreeView &get_tree() const;
  std::optional<int> count() const;
};

class GeometryDataSetTreeView : public ui::AbstractTreeView {
  GeometrySet geometry_set_;
  const bContext &C_;
  SpaceSpreadsheet &sspreadsheet_;
  bScreen &screen_;

  friend class GeometryDataSetTreeViewItem;

 public:
  GeometryDataSetTreeView(GeometrySet geometry_set, const bContext &C)
      : geometry_set_(std::move(geometry_set)),
        C_(C),
        sspreadsheet_(*CTX_wm_space_spreadsheet(&C)),
        screen_(*CTX_wm_screen(&C))
  {
  }

  void build_tree() override
  {
    GeometryDataSetTreeViewItem &mesh = this->add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_MESH, IFACE_("Mesh"), ICON_MESH_DATA);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_MESH, ATTR_DOMAIN_POINT, IFACE_("Vertex"), ICON_VERTEXSEL);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_MESH, ATTR_DOMAIN_EDGE, IFACE_("Edge"), ICON_EDGESEL);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_MESH, ATTR_DOMAIN_FACE, IFACE_("Face"), ICON_FACESEL);
    mesh.add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_MESH, ATTR_DOMAIN_CORNER, IFACE_("Face Corner"), ICON_NODE_CORNER);

    GeometryDataSetTreeViewItem &curve = this->add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_CURVE, IFACE_("Curve"), ICON_CURVE_DATA);
    curve.add_tree_item<GeometryDataSetTreeViewItem>(GEO_COMPONENT_TYPE_CURVE,
                                                     ATTR_DOMAIN_POINT,
                                                     IFACE_("Control Point"),
                                                     ICON_CURVE_BEZCIRCLE);
    curve.add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_CURVE, ATTR_DOMAIN_CURVE, IFACE_("Spline"), ICON_CURVE_PATH);

    GeometryDataSetTreeViewItem &pointcloud = this->add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_POINT_CLOUD, IFACE_("Point Cloud"), ICON_POINTCLOUD_DATA);
    pointcloud.add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_POINT_CLOUD, ATTR_DOMAIN_POINT, IFACE_("Point"), ICON_PARTICLE_POINT);

    this->add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_VOLUME, IFACE_("Volume Grids"), ICON_VOLUME_DATA);

    this->add_tree_item<GeometryDataSetTreeViewItem>(
        GEO_COMPONENT_TYPE_INSTANCES, ATTR_DOMAIN_INSTANCE, IFACE_("Instances"), ICON_EMPTY_AXIS);
  }
};

GeometryDataSetTreeViewItem::GeometryDataSetTreeViewItem(GeometryComponentType component_type,
                                                         StringRef label,
                                                         BIFIconID icon)
    : component_type_(component_type), domain_(std::nullopt), icon_(icon)
{
  label_ = label;
  this->set_collapsed(false);
}
GeometryDataSetTreeViewItem::GeometryDataSetTreeViewItem(GeometryComponentType component_type,
                                                         eAttrDomain domain,
                                                         StringRef label,
                                                         BIFIconID icon)
    : component_type_(component_type), domain_(domain), icon_(icon)
{
  label_ = label;
}

void GeometryDataSetTreeViewItem::on_activate()
{
  GeometryDataSetTreeView &tree_view = this->get_tree();
  bContext &C = const_cast<bContext &>(tree_view.C_);
  SpaceSpreadsheet &sspreadsheet = tree_view.sspreadsheet_;
  tree_view.sspreadsheet_.geometry_component_type = component_type_;
  if (domain_) {
    tree_view.sspreadsheet_.attribute_domain = *domain_;
  }
  PointerRNA ptr;
  RNA_pointer_create(&tree_view.screen_.id, &RNA_SpaceSpreadsheet, &sspreadsheet, &ptr);
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

  if (component_type_ == GEO_COMPONENT_TYPE_VOLUME) {
    return sspreadsheet.geometry_component_type == component_type_;
  }

  if (!domain_) {
    return false;
  }

  return sspreadsheet.geometry_component_type == component_type_ &&
         sspreadsheet.attribute_domain == *domain_;
}

bool GeometryDataSetTreeViewItem::supports_collapsing() const
{
  return false;
}

GeometryDataSetTreeView &GeometryDataSetTreeViewItem::get_tree() const
{
  return static_cast<GeometryDataSetTreeView &>(this->get_tree_view());
}

std::optional<int> GeometryDataSetTreeViewItem::count() const
{
  GeometryDataSetTreeView &tree_view = this->get_tree();
  GeometrySet &geometry = tree_view.geometry_set_;

  /* Special case for volumes since there is no grid domain. */
  if (component_type_ == GEO_COMPONENT_TYPE_VOLUME) {
    if (const Volume *volume = geometry.get_volume_for_read()) {
      return BKE_volume_num_grids(volume);
    }
    return 0;
  }

  if (!domain_) {
    return std::nullopt;
  }

  if (const GeometryComponent *component = geometry.get_component_for_read(component_type_)) {
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
