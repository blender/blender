# Ceres Solver - A fast non-linear least squares minimizer
# Copyright 2010, 2011, 2012 Google Inc. All rights reserved.
# http://code.google.com/p/ceres-solver/
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors may be
#   used to endorse or promote products derived from this software without
#   specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# Author: keir@google.com (Keir Mierle)

SET(CERES_INTERNAL_SRC
    array_utils.cc
    blas.cc
    block_evaluate_preparer.cc
    block_jacobi_preconditioner.cc
    block_jacobian_writer.cc
    block_random_access_dense_matrix.cc
    block_random_access_diagonal_matrix.cc
    block_random_access_matrix.cc
    block_random_access_sparse_matrix.cc
    block_sparse_matrix.cc
    block_structure.cc
    c_api.cc
    canonical_views_clustering.cc
    cgnr_solver.cc
    compressed_col_sparse_matrix_utils.cc
    compressed_row_jacobian_writer.cc
    compressed_row_sparse_matrix.cc
    conditioned_cost_function.cc
    conjugate_gradients_solver.cc
    coordinate_descent_minimizer.cc
    corrector.cc
    covariance.cc
    covariance_impl.cc
    cxsparse.cc
    dense_normal_cholesky_solver.cc
    dense_qr_solver.cc
    dense_sparse_matrix.cc
    detect_structure.cc
    dogleg_strategy.cc
    dynamic_compressed_row_jacobian_writer.cc
    dynamic_compressed_row_sparse_matrix.cc
    evaluator.cc
    file.cc
    gradient_checking_cost_function.cc
    implicit_schur_complement.cc
    incomplete_lq_factorization.cc
    iterative_schur_complement_solver.cc
    levenberg_marquardt_strategy.cc
    lapack.cc
    line_search.cc
    line_search_direction.cc
    line_search_minimizer.cc
    linear_least_squares_problems.cc
    linear_operator.cc
    linear_solver.cc
    local_parameterization.cc
    loss_function.cc
    low_rank_inverse_hessian.cc
    minimizer.cc
    normal_prior.cc
    parameter_block_ordering.cc
    partitioned_matrix_view.cc
    polynomial.cc
    preconditioner.cc
    problem.cc
    problem_impl.cc
    program.cc
    residual_block.cc
    residual_block_utils.cc
    schur_complement_solver.cc
    schur_eliminator.cc
    schur_jacobi_preconditioner.cc
    scratch_evaluate_preparer.cc
    single_linkage_clustering.cc
    solver.cc
    solver_impl.cc
    sparse_matrix.cc
    sparse_normal_cholesky_solver.cc
    split.cc
    stringprintf.cc
    suitesparse.cc
    triplet_sparse_matrix.cc
    trust_region_minimizer.cc
    trust_region_strategy.cc
    types.cc
    visibility.cc
    visibility_based_preconditioner.cc
    wall_time.cc
)

# Heuristic for determining LIB_SUFFIX. FHS recommends that 64-bit systems
# install native libraries to lib64 rather than lib. Most distros seem to
# follow this convention with a couple notable exceptions (Debian-based and
# Arch-based distros) which we try to detect here.
IF (CMAKE_SYSTEM_NAME MATCHES "Linux" AND
    NOT DEFINED LIB_SUFFIX AND
    NOT CMAKE_CROSSCOMPILING AND
    CMAKE_SIZEOF_VOID_P EQUAL "8" AND
    NOT EXISTS "/etc/debian_version" AND
    NOT EXISTS "/etc/arch-release")
  SET(LIB_SUFFIX "64")
ENDIF ()

# Also depend on the header files so that they appear in IDEs.
FILE(GLOB CERES_INTERNAL_HDRS *.h)

# Include the specialized schur solvers.
IF (SCHUR_SPECIALIZATIONS)
  FILE(GLOB CERES_INTERNAL_SCHUR_FILES generated/*.cc)
ELSE (SCHUR_SPECIALIZATIONS)
  # Only the fully dynamic solver. The build is much faster this way.
  FILE(GLOB CERES_INTERNAL_SCHUR_FILES generated/*_d_d_d.cc)
ENDIF (SCHUR_SPECIALIZATIONS)

# Primarily for Android, but optionally for others, use the minimal internal
# Glog implementation.
IF (MINIGLOG)
  ADD_LIBRARY(miniglog STATIC miniglog/glog/logging.cc)
  INSTALL(TARGETS miniglog
          EXPORT  CeresExport
          RUNTIME DESTINATION bin
          LIBRARY DESTINATION lib${LIB_SUFFIX}
          ARCHIVE DESTINATION lib${LIB_SUFFIX})
ENDIF (MINIGLOG)

SET(CERES_LIBRARY_PUBLIC_DEPENDENCIES ${GLOG_LIBRARIES})

IF (SUITESPARSE AND SUITESPARSE_FOUND)
  LIST(APPEND CERES_LIBRARY_PRIVATE_DEPENDENCIES ${SUITESPARSE_LIBRARIES})
ENDIF (SUITESPARSE AND SUITESPARSE_FOUND)

IF (CXSPARSE AND CXSPARSE_FOUND)
  LIST(APPEND CERES_LIBRARY_PRIVATE_DEPENDENCIES ${CXSPARSE_LIBRARIES})
ENDIF (CXSPARSE AND CXSPARSE_FOUND)

IF (BLAS_FOUND AND LAPACK_FOUND)
  LIST(APPEND CERES_LIBRARY_PRIVATE_DEPENDENCIES ${LAPACK_LIBRARIES})
  LIST(APPEND CERES_LIBRARY_PRIVATE_DEPENDENCIES ${BLAS_LIBRARIES})
ENDIF (BLAS_FOUND AND LAPACK_FOUND)

IF (OPENMP_FOUND)
  IF (NOT MSVC)
    LIST(APPEND CERES_LIBRARY_PRIVATE_DEPENDENCIES gomp)
    LIST(APPEND CERES_LIBRARY_PRIVATE_DEPENDENCIES ${CMAKE_THREAD_LIBS_INIT})
  ENDIF (NOT MSVC)
ENDIF (OPENMP_FOUND)

SET(CERES_LIBRARY_SOURCE
    ${CERES_INTERNAL_SRC}
    ${CERES_INTERNAL_HDRS}
    ${CERES_INTERNAL_SCHUR_FILES})

ADD_LIBRARY(ceres ${CERES_LIBRARY_SOURCE})
SET_TARGET_PROPERTIES(ceres PROPERTIES
  VERSION ${CERES_VERSION}
  SOVERSION ${CERES_VERSION_MAJOR}
)

IF (BUILD_SHARED_LIBS)
  # When building a shared library, mark all external libraries as
  # PRIVATE so they don't show up as a dependency.
  TARGET_LINK_LIBRARIES(ceres
        LINK_PUBLIC ${CERES_LIBRARY_PUBLIC_DEPENDENCIES}
        LINK_PRIVATE ${CERES_LIBRARY_PRIVATE_DEPENDENCIES})
ELSE (BUILD_SHARED_LIBS)
  # When building a static library, all external libraries are
  # PUBLIC(default) since the user needs to link to them.
  # They will be listed in CeresTargets.cmake.
  SET(CERES_LIBRARY_DEPENDENCIES
        ${CERES_LIBRARY_PUBLIC_DEPENDENCIES}
        ${CERES_LIBRARY_PRIVATE_DEPENDENCIES})
  TARGET_LINK_LIBRARIES(ceres ${CERES_LIBRARY_DEPENDENCIES})
ENDIF (BUILD_SHARED_LIBS)

INSTALL(TARGETS ceres
        EXPORT  CeresExport
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib${LIB_SUFFIX}
        ARCHIVE DESTINATION lib${LIB_SUFFIX})

IF (BUILD_TESTING AND GFLAGS)
  ADD_LIBRARY(gtest gmock_gtest_all.cc gmock_main.cc)
  ADD_LIBRARY(test_util
              evaluator_test_utils.cc
              numeric_diff_test_utils.cc
              test_util.cc)

  TARGET_LINK_LIBRARIES(gtest ${GFLAGS_LIBRARIES} ${GLOG_LIBRARIES})
  TARGET_LINK_LIBRARIES(test_util ceres gtest ${GLOG_LIBRARIES})

  MACRO (CERES_TEST NAME)
    ADD_EXECUTABLE(${NAME}_test ${NAME}_test.cc)
    TARGET_LINK_LIBRARIES(${NAME}_test test_util ceres gtest)
    ADD_TEST(NAME ${NAME}_test
             COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${NAME}_test
             --test_srcdir
             ${CMAKE_SOURCE_DIR}/data)
  ENDMACRO (CERES_TEST)

  CERES_TEST(array_utils)
  CERES_TEST(autodiff)
  CERES_TEST(autodiff_cost_function)
  CERES_TEST(autodiff_local_parameterization)
  CERES_TEST(block_random_access_dense_matrix)
  CERES_TEST(block_random_access_diagonal_matrix)
  CERES_TEST(block_random_access_sparse_matrix)
  CERES_TEST(block_sparse_matrix)
  CERES_TEST(c_api)
  CERES_TEST(canonical_views_clustering)
  CERES_TEST(compressed_row_sparse_matrix)
  CERES_TEST(conditioned_cost_function)
  CERES_TEST(corrector)
  CERES_TEST(cost_function_to_functor)
  CERES_TEST(covariance)
  CERES_TEST(dense_sparse_matrix)
  CERES_TEST(dynamic_autodiff_cost_function)
  CERES_TEST(dynamic_compressed_row_sparse_matrix)
  CERES_TEST(dynamic_numeric_diff_cost_function)
  CERES_TEST(evaluator)
  CERES_TEST(gradient_checker)
  CERES_TEST(gradient_checking_cost_function)
  CERES_TEST(graph)
  CERES_TEST(graph_algorithms)
  CERES_TEST(implicit_schur_complement)
  CERES_TEST(incomplete_lq_factorization)
  CERES_TEST(iterative_schur_complement_solver)
  CERES_TEST(jet)
  CERES_TEST(levenberg_marquardt_strategy)
  CERES_TEST(dogleg_strategy)
  CERES_TEST(local_parameterization)
  CERES_TEST(loss_function)
  CERES_TEST(minimizer)
  CERES_TEST(normal_prior)
  CERES_TEST(numeric_diff_cost_function)
  CERES_TEST(numeric_diff_functor)
  CERES_TEST(ordered_groups)
  CERES_TEST(parameter_block)
  CERES_TEST(parameter_block_ordering)
  CERES_TEST(partitioned_matrix_view)
  CERES_TEST(polynomial)
  CERES_TEST(problem)
  CERES_TEST(residual_block)
  CERES_TEST(residual_block_utils)
  CERES_TEST(rotation)
  CERES_TEST(schur_complement_solver)
  CERES_TEST(schur_eliminator)
  CERES_TEST(single_linkage_clustering)
  CERES_TEST(small_blas)
  CERES_TEST(solver_impl)

  # TODO(sameeragarwal): This test should ultimately be made
  # independent of SuiteSparse.
  IF (SUITESPARSE AND SUITESPARSE_FOUND)
    CERES_TEST(compressed_col_sparse_matrix_utils)
  ENDIF (SUITESPARSE AND SUITESPARSE_FOUND)

  CERES_TEST(symmetric_linear_solver)
  CERES_TEST(triplet_sparse_matrix)
  CERES_TEST(trust_region_minimizer)
  CERES_TEST(unsymmetric_linear_solver)
  CERES_TEST(visibility)
  CERES_TEST(visibility_based_preconditioner)

  # Put the large end to end test last.
  CERES_TEST(system)
ENDIF (BUILD_TESTING AND GFLAGS)
