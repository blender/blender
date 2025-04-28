# Ceres Solver - A fast non-linear least squares minimizer
# Copyright 2023 Google Inc. All rights reserved.
# http://ceres-solver.org/
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
#
# Generate bundle adjustment tests as separate binaries. Since the bundle
# adjustment tests are fairly processing intensive, serializing them makes the
# tests take forever to run. Splitting them into separate binaries makes it
# easier to parallelize in continuous integration systems, and makes local
# processing on multi-core workstations much faster.

# Product of ORDERINGS, THREAD_CONFIGS, and SOLVER_CONFIGS is the full set of
# tests to generate.
ORDERINGS = ["kAutomaticOrdering", "kUserOrdering"]
SINGLE_THREADED = "1"
MULTI_THREADED = "4"
THREAD_CONFIGS = [SINGLE_THREADED, MULTI_THREADED]

DENSE_SOLVER_CONFIGS = [
    # Linear solver  Dense backend
    ('DENSE_SCHUR',  'EIGEN'),
    ('DENSE_SCHUR',  'LAPACK'),
    ('DENSE_SCHUR',  'CUDA'),
]

SPARSE_SOLVER_CONFIGS = [
    # Linear solver            Sparse backend
    ('SPARSE_NORMAL_CHOLESKY', 'SUITE_SPARSE'),
    ('SPARSE_NORMAL_CHOLESKY', 'EIGEN_SPARSE'),
    ('SPARSE_NORMAL_CHOLESKY', 'ACCELERATE_SPARSE'),
    ('SPARSE_SCHUR',           'SUITE_SPARSE'),
    ('SPARSE_SCHUR',           'EIGEN_SPARSE'),
    ('SPARSE_SCHUR',           'ACCELERATE_SPARSE'),
]

ITERATIVE_SOLVER_CONFIGS = [
    # Linear solver            Sparse backend      Preconditioner
    ('ITERATIVE_SCHUR',        'NO_SPARSE',        'JACOBI'),
    ('ITERATIVE_SCHUR',        'NO_SPARSE',        'SCHUR_JACOBI'),
    ('ITERATIVE_SCHUR',        'NO_SPARSE',        'SCHUR_POWER_SERIES_EXPANSION'),
    ('ITERATIVE_SCHUR',        'SUITE_SPARSE',     'CLUSTER_JACOBI'),
    ('ITERATIVE_SCHUR',        'EIGEN_SPARSE',     'CLUSTER_JACOBI'),
    ('ITERATIVE_SCHUR',        'ACCELERATE_SPARSE','CLUSTER_JACOBI'),
    ('ITERATIVE_SCHUR',        'SUITE_SPARSE',     'CLUSTER_TRIDIAGONAL'),
    ('ITERATIVE_SCHUR',        'EIGEN_SPARSE',     'CLUSTER_TRIDIAGONAL'),
    ('ITERATIVE_SCHUR',        'ACCELERATE_SPARSE','CLUSTER_TRIDIAGONAL'),
]

FILENAME_SHORTENING_MAP = dict(
  DENSE_SCHUR='denseschur',
  ITERATIVE_SCHUR='iterschur',
  SPARSE_NORMAL_CHOLESKY='sparsecholesky',
  SPARSE_SCHUR='sparseschur',
  EIGEN='eigen',
  LAPACK='lapack',
  CUDA='cuda',
  NO_SPARSE='',  # Omit sparse reference entirely for dense tests.
  SUITE_SPARSE='suitesparse',
  EIGEN_SPARSE='eigensparse',
  ACCELERATE_SPARSE='acceleratesparse',
  IDENTITY='identity',
  JACOBI='jacobi',
  SCHUR_JACOBI='schurjacobi',
  CLUSTER_JACOBI='clustjacobi',
  CLUSTER_TRIDIAGONAL='clusttri',
  SCHUR_POWER_SERIES_EXPANSION='spse',
  kAutomaticOrdering='auto',
  kUserOrdering='user',
)

COPYRIGHT_HEADER = (
"""// Ceres Solver - A fast non-linear least squares minimizer
// Copyright 2023 Google Inc. All rights reserved.
// http://ceres-solver.org/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors may be
//   used to endorse or promote products derived from this software without
//   specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// ========================================
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// THIS FILE IS AUTOGENERATED. DO NOT EDIT.
// ========================================
//
// This file is generated using generate_bundle_adjustment_tests.py.""")

BUNDLE_ADJUSTMENT_TEST_TEMPLATE = (COPYRIGHT_HEADER + """

#include "ceres/bundle_adjustment_test_util.h"
#include "ceres/internal/config.h"
#include "gtest/gtest.h"
%(preprocessor_conditions_begin)s
namespace ceres::internal {

TEST_F(BundleAdjustmentTest,
       %(test_class_name)s) {  // NOLINT
  BundleAdjustmentProblem bundle_adjustment_problem;
  Solver::Options* options = bundle_adjustment_problem.mutable_solver_options();
  options->eta = 0.01;
  options->num_threads = %(num_threads)s;
  options->linear_solver_type = %(linear_solver)s;
  options->dense_linear_algebra_library_type = %(dense_backend)s;
  options->sparse_linear_algebra_library_type = %(sparse_backend)s;
  options->preconditioner_type = %(preconditioner)s;
  if (%(ordering)s) {
    options->linear_solver_ordering = nullptr;
  }
  Problem* problem = bundle_adjustment_problem.mutable_problem();
  RunSolverForConfigAndExpectResidualsMatch(*options, problem);
}

}  // namespace ceres::internal
%(preprocessor_conditions_end)s""")

def camelcasify(token):
  """Convert capitalized underscore tokens to camel case"""
  return ''.join([x.lower().capitalize() for x in token.split('_')])


def generate_bundle_test(linear_solver,
                         dense_backend,
                         sparse_backend,
                         preconditioner,
                         ordering,
                         thread_config):
  """Generate a bundle adjustment test executable configured appropriately"""

  # Preconditioner only makes sense for iterative schur; drop it otherwise.
  preconditioner_tag = preconditioner
  if linear_solver != 'ITERATIVE_SCHUR':
    preconditioner_tag = ''

  dense_backend_tag = dense_backend
  if linear_solver != 'DENSE_SCHUR':
    dense_backend_tag=''

  # Omit references to the sparse backend when one is not in use.
  sparse_backend_tag = sparse_backend
  if sparse_backend == 'NO_SPARSE':
    sparse_backend_tag = ''

  # Use a double underscore; otherwise the names are harder to understand.
  test_class_name = '_'.join(filter(lambda x: x, [
      camelcasify(linear_solver),
      camelcasify(dense_backend_tag),
      camelcasify(sparse_backend_tag),
      camelcasify(preconditioner_tag),
      ordering[1:],  # Strip 'k'
      'Threads' if thread_config == MULTI_THREADED else '']))

  # Initial template parameters (augmented more below).
  template_parameters = dict(
          linear_solver=linear_solver,
          dense_backend=dense_backend,
          sparse_backend=sparse_backend,
          preconditioner=preconditioner,
          ordering=ordering,
          num_threads=thread_config,
          test_class_name=test_class_name)

  # Accumulate appropriate #ifdef/#ifndefs for the solver's sparse backend.
  preprocessor_conditions_begin = []
  preprocessor_conditions_end = []
  if sparse_backend == 'SUITE_SPARSE':
    preprocessor_conditions_begin.append('#ifndef CERES_NO_SUITESPARSE')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_NO_SUITESPARSE')
  elif sparse_backend == 'ACCELERATE_SPARSE':
    preprocessor_conditions_begin.append('#ifndef CERES_NO_ACCELERATE_SPARSE')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_NO_ACCELERATE_SPARSE')
  elif sparse_backend == 'EIGEN_SPARSE':
    preprocessor_conditions_begin.append('#ifdef CERES_USE_EIGEN_SPARSE')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_USE_EIGEN_SPARSE')

  if dense_backend == "LAPACK":
    preprocessor_conditions_begin.append('#ifndef CERES_NO_LAPACK')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_NO_LAPACK')
  elif dense_backend == "CUDA":
    preprocessor_conditions_begin.append('#ifndef CERES_NO_CUDA')
    preprocessor_conditions_end.insert(0, '#endif  // CERES_NO_CUDA')

  # If there are #ifdefs, put newlines around them.
  if preprocessor_conditions_begin:
    preprocessor_conditions_begin.insert(0, '')
    preprocessor_conditions_begin.append('')
    preprocessor_conditions_end.insert(0, '')
    preprocessor_conditions_end.append('')

  # Put #ifdef/#ifndef stacks into the template parameters.
  template_parameters['preprocessor_conditions_begin'] = '\n'.join(
      preprocessor_conditions_begin)
  template_parameters['preprocessor_conditions_end'] = '\n'.join(
      preprocessor_conditions_end)

  # Substitute variables into the test template, and write the result to a file.
  filename_tag = '_'.join(FILENAME_SHORTENING_MAP.get(x) for x in [
      linear_solver,
      dense_backend_tag,
      sparse_backend_tag,
      preconditioner_tag,
      ordering]
      if FILENAME_SHORTENING_MAP.get(x))

  if (thread_config == MULTI_THREADED):
    filename_tag += '_threads'

  filename = ('generated_bundle_adjustment_tests/ba_%s_test.cc' %
                filename_tag.lower())
  with open(filename, 'w') as fd:
    fd.write(BUNDLE_ADJUSTMENT_TEST_TEMPLATE % template_parameters)

  # All done.
  print('Generated', filename)

  return filename


if __name__ == '__main__':
  # Iterate over all the possible configurations and generate the tests.
  generated_files = []

  for ordering in ORDERINGS:
    for thread_config in THREAD_CONFIGS:
      for linear_solver, dense_backend in DENSE_SOLVER_CONFIGS:
        generated_files.append(
            generate_bundle_test(linear_solver,
                                 dense_backend,
                                 'NO_SPARSE',
                                 'IDENTITY',
                                 ordering,
                                 thread_config))

      for linear_solver, sparse_backend, in SPARSE_SOLVER_CONFIGS:
        generated_files.append(
            generate_bundle_test(linear_solver,
                                 'EIGEN',
                                 sparse_backend,
                                 'IDENTITY',
                                 ordering,
                                 thread_config))

      for linear_solver, sparse_backend, preconditioner, in ITERATIVE_SOLVER_CONFIGS:
        generated_files.append(
            generate_bundle_test(linear_solver,
                                 'EIGEN',
                                 sparse_backend,
                                 preconditioner,
                                 ordering,
                                 thread_config))


  # Generate the CMakeLists.txt as well.
  with open('generated_bundle_adjustment_tests/CMakeLists.txt', 'w') as fd:
    fd.write(COPYRIGHT_HEADER.replace('//', '#').replace('http:#', 'http://'))
    fd.write('\n')
    fd.write('\n')
    for generated_file in generated_files:
      fd.write('ceres_test(%s)\n' %
               generated_file.split('/')[1].replace('_test.cc', ''))
