/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.hh"

#include "BKE_volume_grid_process.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

#ifdef WITH_OPENVDB
#  include "openvdb/tools/PoissonSolver.h"
#endif

namespace blender::nodes::node_geo_grid_solve_poisson_cc {

enum class ThresholdMode {
  /** Threshold relative to the maximum absolute value of the input grid (infinity norm). */
  Relative,
  /** Threshold based on the maximum absolute value of the solution (infinity norm). */
  Absolute,
};

static const EnumPropertyItem threshold_mode_items[] = {
    {int(ThresholdMode::Relative),
     "RELATIVE",
     0,
     "Relative",
     "Threshold relative to the maximum absolute value of the input grid (infinity norm)"},
    {int(ThresholdMode::Absolute),
     "ABSOLUTE",
     0,
     "Absolute",
     "Threshold based on the maximum absolute value of the solution (infinity norm)"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class Boundary {
  /** Dirichlet boundary condition, specify the solution value for all exterior boundary voxels. */
  Fixed,
  /** Neumann boundary condition, specify gradient for all exterior boundary voxels. */
  Gradient,
  /** Select between \a Fixed and \a Normal boundary condition. */
  Mixed,
};

static const EnumPropertyItem boundary_items[] = {
    {int(Boundary::Fixed),
     "FIXED",
     0,
     "Fixed",
     "Dirichlet boundary condition, specify the solution value for all exterior boundary voxels"},
    {int(Boundary::Gradient),
     "GRADIENT",
     0,
     "Gradient",
     "Neumann boundary condition, specify gradient for all exterior boundary voxels"},
    {int(Boundary::Mixed),
     "MIXED",
     0,
     "Mixed",
     "Select between fixed and normal boundary condition"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_output<decl::Float>("Solution"_ustr)
      .structure_type(StructureType::Grid)
      .description("Solution to the Poisson equation");
  b.add_output<decl::Bool>("Success"_ustr).description("If the solver converged successfully");
  b.add_output<decl::Int>("Iterations"_ustr).description("Number of iterations performed");
  b.add_output<decl::Float>("Absolute Error"_ustr)
      .description("Final absolute error of the solution");
  b.add_output<decl::Float>("Relative Error"_ustr)
      .description("Final relative error of the solution");

  b.add_input<decl::Float>("Grid"_ustr)
      .hide_value()
      .structure_type(StructureType::Grid)
      .description("Right-hand side of the Poisson equation");
  b.add_input<decl::Int>("Max Iterations"_ustr)
      .default_value(100)
      .min(1)
      .max(10000)
      .description("Maximum number of solver iterations");
  b.add_input<decl::Float>("Error Threshold"_ustr)
      .min(0.0f)
      .default_value(1e-3f)
      .description("Error threshold below which a solution is considered valid");
  b.add_input<decl::Menu>("Threshold Mode"_ustr)
      .static_items(threshold_mode_items)
      .expanded()
      .default_value(ThresholdMode::Relative)
      .short_label("Threshold"_ustr)
      .description("Mode that defines how the error threshold is interpreted");

  b.add_input<decl::Menu>("Boundary"_ustr)
      .static_items(boundary_items)
      .default_value(Boundary::Fixed)
      .description("Boundary conditions for the solution between active and inactive voxels");
  b.add_input<decl::Float>("Boundary Value"_ustr)
      .structure_type(StructureType::Field)
      .hide_value()
      .usage_by_menu("Boundary"_ustr, {int(Boundary::Fixed), int(Boundary::Mixed)});
  b.add_input<decl::Vector>("Boundary Gradient"_ustr)
      .structure_type(StructureType::Field)
      .hide_value()
      .usage_by_menu("Boundary"_ustr, {int(Boundary::Gradient), int(Boundary::Mixed)});
  b.add_input<decl::Float>("Boundary Factor"_ustr)
      .subtype(PROP_FACTOR)
      .min(0.0f)
      .max(1.0f)
      .structure_type(StructureType::Field)
      .usage_by_menu("Boundary"_ustr, int(Boundary::Mixed))
      .description("Mix factor between boundary conditions (0=value, 1=gradient)");

  PanelDeclarationBuilder &p = b.add_panel("Debug"_ustr).default_closed(true);
  p.add_output<decl::Float>("Debug Boundary Value"_ustr)
      .structure_type(StructureType::Grid)
      .usage_by_menu("Boundary"_ustr, {int(Boundary::Fixed), int(Boundary::Mixed)});
  p.add_output<decl::Vector>("Debug Boundary Gradient"_ustr)
      .structure_type(StructureType::Grid)
      .usage_by_menu("Boundary"_ustr, {int(Boundary::Gradient), int(Boundary::Mixed)});
  p.add_output<decl::Float>("Debug Boundary Factor"_ustr)
      .structure_type(StructureType::Grid)
      .usage_by_menu("Boundary"_ustr, int(Boundary::Mixed));
}

#ifdef WITH_OPENVDB

struct DirichletBoundaryOp {
  const openvdb::FloatGrid::ConstAccessor value_accessor_;
  const float value_scale_;

  DirichletBoundaryOp(const openvdb::FloatGrid &value_grid)
      : value_accessor_(value_grid.getConstAccessor()),
        value_scale_(math::rcp(math::square(value_grid.voxelSize().x())))
  {
  }

  void operator()(const openvdb::Coord & /*ijk*/,
                  const openvdb::Coord &neighbor,
                  double &source,
                  double &diagonal) const
  {
    const float value = value_accessor_.getValue(neighbor);
    diagonal -= 1.0;
    source -= value * value_scale_;
  };
};

struct NeumannBoundaryOp {
  const openvdb::Vec3SGrid::ConstAccessor gradient_accessor_;
  const openvdb::Mat3s gradient_transform_;

  NeumannBoundaryOp(const openvdb::Vec3SGrid &gradient_grid)
      : gradient_accessor_(gradient_grid.getConstAccessor()),
        gradient_transform_(
            gradient_grid.transform().baseMap()->getAffineMap()->getMat4().getMat3().inverse())
  {
  }

  void operator()(const openvdb::Coord &ijk,
                  const openvdb::Coord &neighbor,
                  double &source,
                  double & /*diagonal*/) const
  {
    const openvdb::math::Vec3s normal = neighbor.asVec3s() - ijk.asVec3s();
    const openvdb::math::Vec3s gradient = gradient_transform_.transform(
        gradient_accessor_.getValue(neighbor));
    source -= gradient.dot(normal);
  };
};

struct MixedBoundaryOp {
  const openvdb::FloatGrid::ConstAccessor value_accessor_;
  const openvdb::Vec3SGrid::ConstAccessor gradient_accessor_;
  const openvdb::FloatGrid::ConstAccessor factor_accessor_;
  const float value_scale_;
  const openvdb::Mat3s gradient_transform_;

  MixedBoundaryOp(const openvdb::FloatGrid &value_grid,
                  const openvdb::Vec3SGrid &gradient_grid,
                  const openvdb::FloatGrid &factor_grid)
      : value_accessor_(value_grid.getConstAccessor()),
        gradient_accessor_(gradient_grid.getConstAccessor()),
        factor_accessor_(factor_grid.getConstAccessor()),
        value_scale_(math::rcp(math::square(value_grid.voxelSize().x()))),
        gradient_transform_(
            gradient_grid.transform().baseMap()->getAffineMap()->getMat4().getMat3().inverse())
  {
  }

  void operator()(const openvdb::Coord &ijk,
                  const openvdb::Coord &neighbor,
                  double &source,
                  double &diagonal) const
  {
    const openvdb::math::Vec3s normal = neighbor.asVec3s() - ijk.asVec3s();
    const float value = value_accessor_.getValue(neighbor);
    const openvdb::math::Vec3s gradient = gradient_transform_.transform(
        gradient_accessor_.getValue(neighbor));
    const float factor = factor_accessor_.getValue(neighbor);
    diagonal -= 1.0 * (1.0f - factor);
    source -= math::interpolate(value * value_scale_, gradient.dot(normal), factor);
  };
};

/* Extract the outermost layer of active voxels on which to evaluate boundary conditions. */
static openvdb::MaskTree::Ptr get_boundary_mask(const openvdb::FloatTree &tree)
{
  using MaskTreeT = openvdb::MaskTree;
  typename MaskTreeT::Ptr interior_mask(new MaskTreeT(tree, false, openvdb::TopologyCopy()));
  typename MaskTreeT::Ptr boundary_mask(new MaskTreeT(tree, false, openvdb::TopologyCopy()));

  openvdb::tools::dilateActiveValues(
      *boundary_mask, /*iterations=*/1, openvdb::tools::NN_FACE, openvdb::tools::PRESERVE_TILES);
  boundary_mask->topologyDifference(*interior_mask);
  return boundary_mask;
}

static bool is_uniform_scale(const float3 &s, const float epsilon = 1e-5f)
{
  return math::abs(s.x - s.y) < epsilon && math::abs(s.x - s.z) < epsilon;
}

/* Returns true if all values in the tree are finite. */
static bool all_values_finite(const openvdb::FloatTree &tree)
{
  threading::EnumerableThreadSpecific<bool> all_finite_tls = true;
  openvdb::tools::foreach(
      tree.cbeginValueOn(), [&](const typename openvdb::FloatTree::ValueOnCIter &iter) {
        const float value = iter.getValue();
        all_finite_tls.local() = all_finite_tls.local() && std::isfinite(value);
      });
  return std::all_of(
      all_finite_tls.begin(), all_finite_tls.end(), [](const bool value) { return value; });
}

#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const bke::VolumeGrid<float> source_grid = params.extract_input<bke::VolumeGrid<float>>(
      "Grid"_ustr);
  if (!source_grid) {
    params.set_default_remaining_outputs();
    return;
  }

  const int max_iterations = params.extract_input<int>("Max Iterations"_ustr);
  const ThresholdMode threshold_mode = params.extract_input<ThresholdMode>("Threshold Mode"_ustr);
  const float error_threshold = params.extract_input<float>("Error Threshold"_ustr);
  const Boundary boundary = params.extract_input<Boundary>("Boundary"_ustr);
  /* The OpenVDB Poisson solver has a "staggered" and a "collocated" variant.
   * The difference seems to be that the staggered version uses a +-1 stencil while the collocated
   * version uses a +-2 size stencil. The collocated stencil (according to code comments) leads to
   * a smoother solution at boundaries, but it requires a smoother input grid as well:
   * any variation smaller than 2 voxels can lead to "ringing" artifacts with odd/even coupling.
   * The "staggered" variant of the Laplacian seems to be the better choice in most cases.
   *
   * Note: By the looks of it the "staggered" Laplacian version was the original implementation,
   * and the collocated version with the +-2 stencil was added later. Not much documentation is
   * available to explain the use cases. This could be exposed as an option if it ever becomes
   * necessary.
   */
  const bool staggered = true;

  bke::VolumeTreeAccessToken tree_token;
  const openvdb::FloatGrid &source_vdb_grid = source_grid.grid(tree_token);
  const openvdb::math::Transform &grid_transform = source_vdb_grid.transform();
  const float3 voxel_size = bke::VolumeGridTraits<float3>::to_blender(grid_transform.voxelSize());
  if (!is_uniform_scale(voxel_size)) {
    params.error_message_add(NodeWarningType::Warning, "Non-uniform grid scale not supported");
  }
  const float voxel_scale = voxel_size.x;
  /* The solver works in index space, where the voxel size is h=1. The Laplace operator applies two
   * derivative operators div(grad(X)) which both add a voxel scale factor of 1/h in the
   * "staggered" 1st order version used here. The solver computes the inverse, so the resulting
   * solution should have a factor of h^2 applied, which we have to do manually to produce the
   * correct result in grid space. */
  const float solution_scale = math::square(voxel_scale);
  /* The absolute error is given as a solution value in grid space, which is scaled by h^2 compared
   * to index space. The absolute error in index space must be scaled by 1/h^2. */
  const float absolute_error_scale = math::rcp(solution_scale);

  openvdb::math::pcg::State solver_state;
  solver_state.iterations = max_iterations;
  switch (threshold_mode) {
    case ThresholdMode::Relative:
      solver_state.relativeError = double(error_threshold);
      solver_state.absoluteError = 0.0;
      break;
    case ThresholdMode::Absolute:
      solver_state.relativeError = 0.0;
      solver_state.absoluteError = double(error_threshold * absolute_error_scale);
      break;
  }

  openvdb::FloatTree::Ptr solution_tree;
  using PreconditionerType =
      openvdb::math::pcg::JacobiPreconditioner<openvdb::tools::poisson::LaplacianMatrix>;
  openvdb::util::NullInterrupter interrupter;
  SocketValueVariant debug_boundary_value = SocketValueVariant::from(0.0f);
  SocketValueVariant debug_boundary_gradient = SocketValueVariant::from(float3(0.0f));
  SocketValueVariant debug_boundary_factor = SocketValueVariant::from(0.0f);
  switch (boundary) {
    case Boundary::Fixed: {
      const Field<float> boundary_value_field = params.extract_input<Field<float>>(
          "Boundary Value"_ustr);

      std::array<bke::GVolumeGrid, 1> grids;
      bke::VolumeTreeAccessToken grid_token;
      if (boundary_value_field.depends_on_input()) {
        const openvdb::MaskTree::Ptr mask_tree = get_boundary_mask(source_vdb_grid.tree());
        bke::volume_grid::evaluate_fields_to_grid(
            *mask_tree, grid_transform, {boundary_value_field}, grids);
      }
      else {
        const float boundary_value = fn::evaluate_constant_field(boundary_value_field);
        openvdb::FloatGrid::Ptr vdb_grid = openvdb::FloatGrid::create(boundary_value);
        vdb_grid->setTransform(source_vdb_grid.constTransformPtr()->copy());
        grids[0] = bke::VolumeGrid<float>(std::move(vdb_grid));
      }

      DirichletBoundaryOp boundary_op(grids[0].typed<float>().grid(grid_token));
      solution_tree = openvdb::tools::poisson::solveWithBoundaryConditionsAndPreconditioner<
          PreconditionerType>(
          source_vdb_grid.tree(), boundary_op, solver_state, interrupter, staggered);

      debug_boundary_value = SocketValueVariant::from(grids[0]);
      break;
    }

    case Boundary::Gradient: {
      const Field<float3> boundary_gradient_field = params.extract_input<Field<float3>>(
          "Boundary Gradient"_ustr);

      std::array<bke::GVolumeGrid, 1> grids;
      bke::VolumeTreeAccessToken grid_token;
      if (boundary_gradient_field.depends_on_input()) {
        const openvdb::MaskTree::Ptr mask_tree = get_boundary_mask(source_vdb_grid.tree());
        bke::volume_grid::evaluate_fields_to_grid(
            *mask_tree, grid_transform, {boundary_gradient_field}, grids);
      }
      else {
        const float3 boundary_gradient = fn::evaluate_constant_field(boundary_gradient_field);
        openvdb::Vec3SGrid::Ptr vdb_grid = openvdb::Vec3SGrid::create(
            bke::VolumeGridTraits<float3>::to_openvdb(boundary_gradient));
        vdb_grid->setTransform(source_vdb_grid.constTransformPtr()->copy());
        grids[0] = bke::VolumeGrid<float3>(std::move(vdb_grid));
      }

      NeumannBoundaryOp boundary_op(grids[0].typed<float3>().grid(grid_token));
      solution_tree = openvdb::tools::poisson::solveWithBoundaryConditionsAndPreconditioner<
          PreconditionerType>(
          source_vdb_grid.tree(), boundary_op, solver_state, interrupter, staggered);

      debug_boundary_gradient = SocketValueVariant::from(grids[0]);
      break;
    }

    case Boundary::Mixed: {
      const Field<float> boundary_value_field = params.extract_input<Field<float>>(
          "Boundary Value"_ustr);
      const Field<float3> boundary_gradient_field = params.extract_input<Field<float3>>(
          "Boundary Gradient"_ustr);
      const Field<float> boundary_factor_field = params.extract_input<Field<float>>(
          "Boundary Factor"_ustr);
      const openvdb::MaskTree::Ptr mask_tree = get_boundary_mask(source_vdb_grid.tree());

      std::array<bke::GVolumeGrid, 3> grids;
      std::array<bke::VolumeTreeAccessToken, 3> grid_tokens;

      if (boundary_value_field.depends_on_input() || boundary_gradient_field.depends_on_input() ||
          boundary_factor_field.depends_on_input())
      {
        bke::volume_grid::evaluate_fields_to_grid(
            *mask_tree,
            grid_transform,
            {boundary_value_field, boundary_gradient_field, boundary_factor_field},
            grids);
      }
      else {
        const float boundary_value = fn::evaluate_constant_field(boundary_value_field);
        const float3 boundary_gradient = fn::evaluate_constant_field(boundary_gradient_field);
        const float boundary_factor = fn::evaluate_constant_field(boundary_factor_field);
        openvdb::FloatGrid::Ptr vdb_value_grid = openvdb::FloatGrid::create(boundary_value);
        openvdb::Vec3SGrid::Ptr vdb_gradient_grid = openvdb::Vec3SGrid::create(
            bke::VolumeGridTraits<float3>::to_openvdb(boundary_gradient));
        openvdb::FloatGrid::Ptr vdb_factor_grid = openvdb::FloatGrid::create(boundary_factor);
        vdb_value_grid->setTransform(source_vdb_grid.constTransformPtr()->copy());
        vdb_gradient_grid->setTransform(source_vdb_grid.constTransformPtr()->copy());
        vdb_factor_grid->setTransform(source_vdb_grid.constTransformPtr()->copy());
        grids[0] = bke::VolumeGrid<float>(std::move(vdb_value_grid));
        grids[1] = bke::VolumeGrid<float3>(std::move(vdb_gradient_grid));
        grids[2] = bke::VolumeGrid<float>(std::move(vdb_factor_grid));
      }

      MixedBoundaryOp boundary_op(grids[0].typed<float>().grid(grid_tokens[0]),
                                  grids[1].typed<float3>().grid(grid_tokens[1]),
                                  grids[2].typed<float>().grid(grid_tokens[2]));
      solution_tree = openvdb::tools::poisson::solveWithBoundaryConditionsAndPreconditioner<
          PreconditionerType>(
          source_vdb_grid.tree(), boundary_op, solver_state, interrupter, staggered);

      debug_boundary_value = SocketValueVariant::from(grids[0]);
      debug_boundary_gradient = SocketValueVariant::from(grids[1]);
      debug_boundary_factor = SocketValueVariant::from(grids[2]);
      break;
    }

    default:
      BLI_assert_unreachable();
      solution_tree = std::make_shared<openvdb::FloatTree>();
      break;
  }

  /* Note: OpenVDB infNorm function of the conjugate gradient solver
   * outputs 0.0 if the solution contains only non-finite values.
   * Handle the case of a non-finite solution explicitly here: If the solution contains any
   * non-finite value it has diverged and should be considered invalid. */
  if (solver_state.absoluteError == 0.0) {
    const bool all_finite = all_values_finite(*solution_tree);
    if (!all_finite) {
      solver_state.absoluteError = std::numeric_limits<float>::infinity();
      solver_state.relativeError = std::numeric_limits<float>::infinity();
      solver_state.success = false;
    }
  }

  openvdb::tools::foreach(solution_tree->beginValueOn(),
                          [&](const typename openvdb::FloatTree::ValueOnIter &iter) {
                            iter.setValue(iter.getValue() * solution_scale);
                          });

  openvdb::FloatGrid::Ptr solution_grid = openvdb::FloatGrid::create(std::move(solution_tree));
  solution_grid->setTransform(grid_transform.copy());

  params.set_output("Solution"_ustr, bke::VolumeGrid<float>(std::move(solution_grid)));
  params.set_output("Iterations"_ustr, solver_state.iterations);
  params.set_output("Success"_ustr, solver_state.success);
  params.set_output("Absolute Error"_ustr, float(solver_state.absoluteError * solution_scale));
  params.set_output("Relative Error"_ustr, float(solver_state.relativeError));
  if (params.output_is_required("Debug Boundary Value"_ustr)) {
    params.set_output("Debug Boundary Value"_ustr, std::move(debug_boundary_value));
  }
  if (params.output_is_required("Debug Boundary Gradient"_ustr)) {
    params.set_output("Debug Boundary Gradient"_ustr, std::move(debug_boundary_gradient));
  }
  if (params.output_is_required("Debug Boundary Factor"_ustr)) {
    params.set_output("Debug Boundary Factor"_ustr, std::move(debug_boundary_factor));
  }

  if (!solver_state.success) {
    params.error_message_add(
        NodeWarningType::Warning,
        "Poisson solver failed to converge within the iteration limit. Try increasing "
        "the maximum iterations or tolerance.");
  }
#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGridSolvePoisson"_ustr);
  ntype.ui_name = "Grid Solve Poisson";
  ntype.ui_description =
      "Solve the Poisson equation for a scalar field. Computes a grid whose Laplacian equals the "
      "input scalar grid.";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.default_width = bke::NodeWidth::_200;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_solve_poisson_cc
