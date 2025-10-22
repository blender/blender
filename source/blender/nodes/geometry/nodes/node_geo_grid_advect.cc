/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#ifdef WITH_OPENVDB
#  include "openvdb/tools/VolumeAdvect.h"
#endif

namespace blender::nodes::node_geo_grid_advect_cc {

enum class IntegrationScheme : int8_t {
  SemiLagrangian = 0,
  Midpoint = 1,
  RungeKutta3 = 2,
  RungeKutta4 = 3,
  MacCormack = 4,
  BFECC = 5,
};

enum class LimiterType : int8_t {
  None = 0,
  Clamp = 1,
  Revert = 2,
};

static const EnumPropertyItem integration_scheme_items[] = {
    {int(IntegrationScheme::SemiLagrangian),
     "SEMI",
     0,
     N_("Semi-Lagrangian"),
     N_("1st order semi-Lagrangian integration. Fast but least accurate, suitable for simple "
        "advection")},
    {int(IntegrationScheme::Midpoint),
     "MID",
     0,
     N_("Midpoint"),
     N_("2nd order midpoint integration. Good balance between speed and accuracy for most cases")},
    {int(IntegrationScheme::RungeKutta3),
     "RK3",
     0,
     N_("Runge-Kutta 3"),
     N_("3rd order Runge-Kutta integration. Higher accuracy at moderate computational cost")},
    {int(IntegrationScheme::RungeKutta4),
     "RK4",
     0,
     N_("Runge-Kutta 4"),
     N_("4th order Runge-Kutta integration. Highest accuracy single-step method but slower")},
    {int(IntegrationScheme::MacCormack),
     "MAC",
     0,
     N_("MacCormack"),
     N_("MacCormack scheme with implicit diffusion control. Reduces numerical dissipation while "
        "maintaining stability")},
    {int(IntegrationScheme::BFECC),
     "BFECC",
     0,
     N_("BFECC"),
     N_("Back and Forth Error Compensation and Correction. Advanced scheme that minimizes "
        "dissipation and diffusion")},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem limiter_type_items[] = {
    {int(LimiterType::None),
     "NONE",
     0,
     N_("None"),
     N_("No limiting applied. Fastest but may produce artifacts in high-order schemes")},
    {int(LimiterType::Clamp),
     "CLAMP",
     0,
     N_("Clamp"),
     N_("Clamp values to the range of the original neighborhood. Prevents overshooting and "
        "undershooting")},
    {int(LimiterType::Revert),
     "REVERT",
     0,
     N_("Revert"),
     N_("Revert to 1st order integration when clamping would be applied. More conservative than "
        "clamping")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();

  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
  b.add_input(data_type, "Grid")
      .hide_value()
      .structure_type(StructureType::Grid)
      .is_default_link_socket();
  b.add_output(data_type, "Grid").structure_type(StructureType::Grid).align_with_previous();
  b.add_input<decl::Vector>("Velocity").hide_value().structure_type(StructureType::Grid);
  b.add_input<decl::Float>("Time Step")
      .subtype(PROP_TIME_ABSOLUTE)
      .default_value(1.0f)
      .description("Time step for advection in seconds");
  b.add_input<decl::Menu>("Integration Scheme")
      .static_items(integration_scheme_items)
      .default_value(IntegrationScheme::RungeKutta3)
      .optional_label()
      .description("Numerical integration method for advection");
  b.add_input<decl::Menu>("Limiter")
      .static_items(limiter_type_items)
      .default_value(LimiterType::Clamp)
      .optional_label()
      .description("Limiting strategy to prevent numerical artifacts");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static std::optional<eNodeSocketDatatype> node_type_for_socket_type(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return SOCK_FLOAT;
    case SOCK_INT:
      return SOCK_INT;
    case SOCK_VECTOR:
    case SOCK_RGBA:
      return SOCK_VECTOR;
    default:
      return std::nullopt;
  }
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(
      params.other_socket());
  if (!data_type) {
    return;
  }
  if (params.in_out() == SOCK_IN) {
    if (params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                   SOCK_VECTOR))
    {
      params.add_item(IFACE_("Velocity"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridAdvect");
        params.update_and_connect_available_socket(node, "Velocity");
      });
    }
    if (params.node_tree().typeinfo->validate_link(eNodeSocketDatatype(params.other_socket().type),
                                                   SOCK_FLOAT))
    {
      params.add_item(IFACE_("Time Step"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridAdvect");
        params.update_and_connect_available_socket(node, "Time Step");
      });
    }
  }
  params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
    bNode &node = params.add_node("GeometryNodeGridAdvect");
    node.custom1 = *data_type;
    params.update_and_connect_available_socket(node, "Grid");
  });
}

#ifdef WITH_OPENVDB
static openvdb::tools::Scheme::SemiLagrangian to_openvdb_scheme(const IntegrationScheme scheme)
{
  switch (scheme) {
    case IntegrationScheme::SemiLagrangian:
      return openvdb::tools::Scheme::SEMI;
    case IntegrationScheme::Midpoint:
      return openvdb::tools::Scheme::MID;
    case IntegrationScheme::RungeKutta3:
      return openvdb::tools::Scheme::RK3;
    case IntegrationScheme::RungeKutta4:
      return openvdb::tools::Scheme::RK4;
    case IntegrationScheme::MacCormack:
      return openvdb::tools::Scheme::MAC;
    case IntegrationScheme::BFECC:
      return openvdb::tools::Scheme::BFECC;
  }
  return openvdb::tools::Scheme::SEMI;
}

static openvdb::tools::Scheme::Limiter to_openvdb_limiter(const LimiterType limiter)
{
  switch (limiter) {
    case LimiterType::None:
      return openvdb::tools::Scheme::NO_LIMITER;
    case LimiterType::Clamp:
      return openvdb::tools::Scheme::CLAMP;
    case LimiterType::Revert:
      return openvdb::tools::Scheme::REVERT;
  }
  return openvdb::tools::Scheme::NO_LIMITER;
}

template<typename GridType, typename SamplerType = openvdb::tools::Sampler<1>>
static typename GridType::Ptr advect_grid(const GridType &grid,
                                          const openvdb::Vec3SGrid &velocity_grid,
                                          const float time_step,
                                          const IntegrationScheme scheme,
                                          const LimiterType limiter)
{
  openvdb::tools::VolumeAdvection<openvdb::Vec3SGrid, false> advection(velocity_grid);

  advection.setIntegrator(to_openvdb_scheme(scheme));
  advection.setLimiter(to_openvdb_limiter(limiter));
  return advection.template advect<GridType, SamplerType>(grid, time_step);
}
#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  const bke::VolumeGrid<float3> velocity_grid = params.extract_input<bke::VolumeGrid<float3>>(
      "Velocity");
  if (!velocity_grid) {
    params.set_output("Grid", std::move(grid));
    return;
  }

  const float time_step = params.extract_input<float>("Time Step");
  const IntegrationScheme scheme = params.extract_input<IntegrationScheme>("Integration Scheme");
  const LimiterType limiter = params.extract_input<LimiterType>("Limiter");

  bke::VolumeTreeAccessToken tree_token;
  bke::VolumeTreeAccessToken velocity_token;
  const openvdb::GridBase &grid_base = grid->grid(tree_token);
  const openvdb::Vec3SGrid &velocity_vdb_grid = velocity_grid.grid(velocity_token);

  /* OpenVDB's advection requires uniform voxel scale on the grid being advected
  but not for the velocity grid being sampled */
  if (!grid_base.hasUniformVoxels()) {
    params.error_message_add(
        NodeWarningType::Error,
        TIP_("The input grid must have a uniform voxel scale to be advected."));
    params.set_output("Grid", std::move(grid));
    return;
  }

  const VolumeGridType grid_type = grid->grid_type();

  BKE_volume_grid_type_to_static_type(grid_type, [&](auto grid_type_tag) {
    using GridType = typename decltype(grid_type_tag)::type;
    if constexpr (std::is_same_v<GridType, openvdb::FloatGrid> ||
                  std::is_same_v<GridType, openvdb::Int32Grid> ||
                  std::is_same_v<GridType, openvdb::Vec3fGrid>)
    {
      typename GridType::Ptr result = advect_grid(
          static_cast<const GridType &>(grid->grid(tree_token)),
          velocity_vdb_grid,
          time_step,
          scheme,
          limiter);
      params.set_output("Grid", bke::GVolumeGrid(std::move(result)));
    }
    else {
      params.error_message_add(NodeWarningType::Error, "Unsupported grid type for advection");
      params.set_default_remaining_outputs();
    }
  });
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

/* Only float, int, and vector grids are supported for advection */
static const EnumPropertyItem *advect_grid_socket_type_filter(bContext * /*C*/,
                                                              PointerRNA * /*ptr*/,
                                                              PropertyRNA * /*prop*/,
                                                              bool *r_free)
{
  *r_free = true;
  return enum_items_filter(rna_enum_node_socket_data_type_items,
                           [](const EnumPropertyItem &item) -> bool {
                             const eNodeSocketDatatype type = eNodeSocketDatatype(item.value);
                             return ELEM(type, SOCK_FLOAT, SOCK_INT, SOCK_VECTOR);
                           });
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Node socket data type",
                    rna_enum_node_socket_data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    SOCK_FLOAT,
                    advect_grid_socket_type_filter);
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket &output_socket)
{
  return node.input_by_identifier(output_socket.identifier);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGridAdvect");
  ntype.ui_name = "Advect Grid";
  ntype.ui_description =
      "Move grid values through a velocity field using numerical integration. Supports multiple "
      "integration schemes for different accuracy and performance trade-offs";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_advect_cc
