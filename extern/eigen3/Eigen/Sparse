#ifndef EIGEN_SPARSE_MODULE_H
#define EIGEN_SPARSE_MODULE_H

#include "Core"

#include "src/Core/util/DisableStupidWarnings.h"

#include <vector>
#include <map>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#ifdef EIGEN2_SUPPORT
#define EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
#endif

#ifndef EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
#error The sparse module API is not stable yet. To use it anyway, please define the EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET preprocessor token.
#endif

namespace Eigen {

/** \defgroup Sparse_Module Sparse module
  *
  *
  *
  * See the \ref TutorialSparse "Sparse tutorial"
  *
  * \code
  * #include <Eigen/Sparse>
  * \endcode
  */

/** The type used to identify a general sparse storage. */
struct Sparse {};

#include "src/Sparse/SparseUtil.h"
#include "src/Sparse/SparseMatrixBase.h"
#include "src/Sparse/CompressedStorage.h"
#include "src/Sparse/AmbiVector.h"
#include "src/Sparse/SparseMatrix.h"
#include "src/Sparse/DynamicSparseMatrix.h"
#include "src/Sparse/MappedSparseMatrix.h"
#include "src/Sparse/SparseVector.h"
#include "src/Sparse/CoreIterators.h"
#include "src/Sparse/SparseBlock.h"
#include "src/Sparse/SparseTranspose.h"
#include "src/Sparse/SparseCwiseUnaryOp.h"
#include "src/Sparse/SparseCwiseBinaryOp.h"
#include "src/Sparse/SparseDot.h"
#include "src/Sparse/SparseAssign.h"
#include "src/Sparse/SparseRedux.h"
#include "src/Sparse/SparseFuzzy.h"
#include "src/Sparse/SparseProduct.h"
#include "src/Sparse/SparseSparseProduct.h"
#include "src/Sparse/SparseDenseProduct.h"
#include "src/Sparse/SparseDiagonalProduct.h"
#include "src/Sparse/SparseTriangularView.h"
#include "src/Sparse/SparseSelfAdjointView.h"
#include "src/Sparse/TriangularSolver.h"
#include "src/Sparse/SparseView.h"

} // namespace Eigen

#include "src/Core/util/ReenableStupidWarnings.h"

#endif // EIGEN_SPARSE_MODULE_H

