//
//  Helper matrix class, RCMatrix.h
//  Required for PD optimizations (guiding)
//  Thanks to Ryoichi Ando, and Robert Bridson
//

#ifndef RCMATRIX3_H
#define RCMATRIX3_H

#include <iterator>
#include <cassert>
#include <vector>
#include <fstream>

// index type
#define int_index long long

// link to omp & tbb for now
#if OPENMP == 1 || TBB == 1
#  define MANTA_ENABLE_PARALLEL 1
// allow the preconditioner to be computed in parallel? (can lead to slightly non-deterministic
// results)
#  define MANTA_ENABLE_PARALLEL_PC 0
#else
#  define MANTA_ENABLE_PARALLEL 0
#  define MANTA_ENABLE_PARALLEL_PC 0
#endif

#if MANTA_ENABLE_PARALLEL == 1
#  include <thread>
#  include <algorithm>

static const int manta_num_threads = std::thread::hardware_concurrency();

// For clang
#  define parallel_for(total_size) \
    { \
      int_index parallel_array_size = (total_size); \
      std::vector<std::thread> threads(manta_num_threads); \
      for (int thread_number = 0; thread_number < manta_num_threads; thread_number++) { \
  threads[thread_number] = std::thread([&](int_index parallel_array_size, int_index thread_number ) { \
		for( int_index parallel_index=thread_number; parallel_index < parallel_array_size; parallel_index += manta_num_threads ) {

#  define parallel_end \
    } \
    },parallel_array_size,thread_number); \
    } \
    for (auto &thread : threads) \
      thread.join(); \
    }

#  define parallel_block \
    { \
      std::vector<std::thread> threads; \
      {

#  define do_parallel threads.push_back( std::thread([&]() {
#  define do_end \
    } ) );

#  define block_end \
    } \
    for (auto &thread : threads) { \
      thread.join(); \
    } \
    }

#else

#  define parallel_for(size) \
    { \
      int thread_number = 0; \
      int_index parallel_index = 0; \
      for (int_index parallel_index = 0; parallel_index < (int_index)size; parallel_index++) {
#  define parallel_end \
    } \
    thread_number = parallel_index = 0; \
    }

#  define parallel_block
#  define do_parallel
#  define do_end
#  define block_end

#endif

#include "vectorbase.h"

namespace Manta {

static const unsigned default_expected_none_zeros = 7;

template<class N, class T> struct RCMatrix {
  struct RowEntry {
    std::vector<N> index;
    std::vector<T> value;
  };
  RCMatrix() : n(0), expected_none_zeros(default_expected_none_zeros)
  {
  }
  RCMatrix(N size, N expected_none_zeros = default_expected_none_zeros)
      : n(0), expected_none_zeros(expected_none_zeros)
  {
    resize(size);
  }
  RCMatrix(const RCMatrix &m) : n(0), expected_none_zeros(default_expected_none_zeros)
  {
    init(m);
  }
  RCMatrix &operator=(const RCMatrix &m)
  {
    expected_none_zeros = m.expected_none_zeros;
    init(m);
    return *this;
  }
  RCMatrix &operator=(RCMatrix &&m)
  {
    matrix = m.matrix;
    offsets = m.offsets;
    expected_none_zeros = m.expected_none_zeros;
    n = m.n;
    m.n = 0;
    m.matrix.clear();
    m.offsets.clear();
    return *this;
  }
  RCMatrix(RCMatrix &&m)
      : n(m.n), expected_none_zeros(m.expected_none_zeros), matrix(m.matrix), offsets(m.offsets)
  {
    m.n = 0;
    m.matrix.clear();
    m.offsets.clear();
  }
  void init(const RCMatrix &m)
  {
    expected_none_zeros = m.expected_none_zeros;
    resize(m.n);
    parallel_for(n)
    {
      N i = parallel_index;
      if (m.matrix[i]) {
        alloc_row(i);
        matrix[i]->index = m.matrix[i]->index;
        matrix[i]->value = m.matrix[i]->value;
      }
      else {
        dealloc_row(i);
      }
    }
    parallel_end
  }
  ~RCMatrix()
  {
    clear();
  }
  void clear()
  {
    for (N i = 0; i < n; i++) {
      dealloc_row(i);
      matrix[i] = NULL;
      if (offsets.size())
        offsets[i] = 0;
    }
  };
  bool empty(N i) const
  {
    return matrix[i] == NULL;
  }
  N row_nonzero_size(N i) const
  {
    return matrix[i] == NULL ? 0 : matrix[i]->index.size();
  }
  void resize(N size, N expected_none_zeros = 0)
  {
    if (!expected_none_zeros) {
      expected_none_zeros = this->expected_none_zeros;
    }
    if (n > size) {
      // Shrinking
      for (N i = size ? size - 1 : 0; i < n; i++)
        dealloc_row(i);
      matrix.resize(size);
    }
    else if (n < size) {
      // Expanding
      matrix.resize(size);
      for (N i = n; i < size; i++) {
        matrix[i] = NULL;
        if (offsets.size())
          offsets[i] = 0;
      }
    }
    n = size;
  }
  void alloc_row(N i)
  {
    assert(i < n);
    if (!matrix[i]) {
      matrix[i] = new RowEntry;
      matrix[i]->index.reserve(expected_none_zeros);
      matrix[i]->value.reserve(expected_none_zeros);
      if (offsets.size())
        offsets[i] = 0;
    }
  }
  void dealloc_row(N i)
  {
    assert(i < n);
    if (matrix[i]) {
      if (offsets.empty() || !offsets[i])
        delete matrix[i];
      matrix[i] = NULL;
      if (offsets.size())
        offsets[i] = 0;
    }
  }
  T operator()(N i, N j) const
  {
    assert(i < n);
    for (Iterator it = row_begin(i); it; ++it) {
      if (it.index() == j)
        return it.value();
    }
    return T(0.0);
  }
  void add_to_element_checked(N i, N j, T val)
  {
    if ((i < 0) || (j < 0) || (i >= n) || (j >= n))
      return;
    add_to_element(i, j, val);
  }
  void add_to_element(N i, N j, T increment_value)
  {
    if (std::abs(increment_value) > VECTOR_EPSILON) {
      assert(i < n);
      assert(offsets.empty() || offsets[i] == 0);
      alloc_row(i);
      std::vector<N> &index = matrix[i]->index;
      std::vector<T> &value = matrix[i]->value;
      for (N k = 0; k < (N)index.size(); ++k) {
        if (index[k] == j) {
          value[k] += increment_value;
          return;
        }
        else if (index[k] > j) {
          index.insert(index.begin() + k, j);
          value.insert(value.begin() + k, increment_value);
          return;
        }
      }
      index.push_back(j);
      value.push_back(increment_value);
    }
  }

  void set_element(N i, N j, T v)
  {
    if (std::abs(v) > VECTOR_EPSILON) {
      assert(i < n);
      assert(offsets.empty() || offsets[i] == 0);
      alloc_row(i);
      std::vector<N> &index = matrix[i]->index;
      std::vector<T> &value = matrix[i]->value;
      for (N k = 0; k < (N)index.size(); ++k) {
        if (index[k] == j) {
          value[k] = v;
          return;
        }
        else if (index[k] > j) {
          index.insert(index.begin() + k, j);
          value.insert(value.begin() + k, v);
          return;
        }
      }
      index.push_back(j);
      value.push_back(v);
    }
  }

  // Make sure that j is the biggest column in the row, no duplication allowed
  void fix_element(N i, N j, T v)
  {
    if (std::abs(v) > VECTOR_EPSILON) {
      assert(i < n);
      assert(offsets.empty() || offsets[i] == 0);
      alloc_row(i);
      std::vector<N> &index = matrix[i]->index;
      std::vector<T> &value = matrix[i]->value;
      index.push_back(j);
      value.push_back(v);
    }
  }
  int_index trim_zero_entries(double e = VECTOR_EPSILON)
  {
    std::vector<int_index> deleted_entries(n, 0);
    parallel_for(n)
    {
      N i = parallel_index;
      if (matrix[i]) {
        std::vector<N> &index = matrix[i]->index;
        std::vector<T> &value = matrix[i]->value;
        N head = 0;
        N k = 0;
        for (k = 0; k < index.size(); ++k) {
          if (std::abs(value[k]) > e) {
            index[head] = index[k];
            value[head] = value[k];
            ++head;
          }
        }
        if (head != k) {
          index.erase(index.begin() + head, index.end());
          value.erase(value.begin() + head, value.end());
          deleted_entries[i] += k - head;
        }
        if (!offsets.size() && !head) {
          remove_row(i);
        }
      }
    }
    parallel_end
        //
        int_index sum_deleted(0);
    for (int_index i = 0; i < n; i++)
      sum_deleted += deleted_entries[i];
    return sum_deleted;
  }
  void remove_reference(N i)
  {
    if (offsets.size() && offsets[i] && matrix[i]) {
      RowEntry *save = matrix[i];
      matrix[i] = new RowEntry;
      *matrix[i] = *save;
      for (N &index : matrix[i]->index)
        index += offsets[i];
      offsets[i] = 0;
    }
  }
  void remove_row(N i)
  {
    dealloc_row(i);
  }
  bool is_symmetric(double e = VECTOR_EPSILON) const
  {
    std::vector<bool> flags(n, true);
    parallel_for(n)
    {
      N i = parallel_index;
      bool flag = true;
      for (Iterator it = row_begin(i); it; ++it) {
        N index = it.index();
        T value = it.value();
        if (std::abs(value) > e) {
          bool found_entry = false;
          for (Iterator it_i = row_begin(index); it_i; ++it_i) {
            if (it_i.index() == i) {
              found_entry = true;
              if (std::abs(value - it_i.value()) > e) {
                flag = false;
                break;
              }
            }
          }
          if (!found_entry)
            flag = false;
          if (!flag)
            break;
        }
      }
      flags[i] = flag;
    }
    parallel_end for (N i = 0; i < matrix.size(); ++i)
    {
      if (!flags[i])
        return false;
    }
    return true;
  }

  void expand()
  {
    if (offsets.empty())
      return;
    for (N i = 1; i < n; i++) {
      if (offsets[i]) {
        RowEntry *ref = matrix[i];
        matrix[i] = new RowEntry;
        *matrix[i] = *ref;
        for (N j = 0; j < (N)matrix[i]->index.size(); j++) {
          matrix[i]->index[j] += offsets[i];
        }
      }
    }
    offsets.resize(0);
  }

  N column(N i) const
  {
    return empty(i) ? 0 : row_begin(i, row_nonzero_size(i) - 1).index();
  }
  N getColumnSize() const
  {
    N max_column(0);
    auto column = [&](N i) {
      N max_column(0);
      for (Iterator it = row_begin(i); it; ++it)
        max_column = std::max(max_column, it.index());
      return max_column + 1;
    };
    for (N i = 0; i < n; i++)
      max_column = std::max(max_column, column(i));
    return max_column;
  }
  N getNonzeroSize() const
  {
    N nonzeros(0);
    for (N i = 0; i < n; ++i) {
      nonzeros += row_nonzero_size(i);
    }
    return nonzeros;
  }
  class Iterator : std::iterator<std::input_iterator_tag, T> {
   public:
    Iterator(const RowEntry *rowEntry, N k, N offset) : rowEntry(rowEntry), k(k), offset(offset)
    {
    }
    operator bool() const
    {
      return rowEntry != NULL && k < (N)rowEntry->index.size();
    }
    Iterator &operator++()
    {
      ++k;
      return *this;
    }
    T value() const
    {
      return rowEntry->value[k];
    }
    N index() const
    {
      return rowEntry->index[k] + offset;
    }
    N index_raw() const
    {
      return rowEntry->index[k];
    }
    N size() const
    {
      return rowEntry == NULL ? 0 : rowEntry->index.size();
    }

   protected:
    const RowEntry *rowEntry;
    N k, offset;
  };
  Iterator row_begin(N n, N k = 0) const
  {
    return Iterator(matrix[n], k, offsets.size() ? offsets[n] : 0);
  }
  class DynamicIterator : public Iterator {
   public:
    DynamicIterator(RowEntry *rowEntry, N k, N offset)
        : rowEntry(rowEntry), Iterator(rowEntry, k, offset)
    {
    }
    void setValue(T value)
    {
      rowEntry->value[Iterator::k] = value;
    }
    void setIndex(N index)
    {
      rowEntry->index[Iterator::k] = index;
    }

   protected:
    RowEntry *rowEntry;
  };
  DynamicIterator dynamic_row_begin(N n, N k = 0)
  {
    N offset = offsets.size() ? offsets[n] : 0;
    if (offset) {
      printf("---- Warning ----\n");
      printf("Dynamic iterator is not allowed for referenced rows.\n");
      printf("You should be very careful otherwise this causes some bugs.\n");
      printf(
          "We encourage you that you convert this row into a raw format, then loop over it...\n");
      printf("-----------------\n");
      exit(0);
    }
    return DynamicIterator(matrix[n], k, offset);
  }
  RCMatrix transpose(N rowsize = 0,
                     unsigned expected_none_zeros = default_expected_none_zeros) const
  {
    if (!rowsize)
      rowsize = getColumnSize();
    RCMatrix result(rowsize, expected_none_zeros);
    for (N i = 0; i < n; i++) {
      for (Iterator it = row_begin(i); it; ++it)
        result.fix_element(it.index(), i, it.value());
    }
    return result;
  }

  RCMatrix getKtK() const
  {
    RCMatrix m = transpose();
    RCMatrix result(n, expected_none_zeros);
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      for (Iterator it_A = m.row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        assert(j < n);
        T a = it_A.value();
        if (std::abs(a) > VECTOR_EPSILON) {
          for (Iterator it_B = row_begin(j); it_B; ++it_B) {
            // result.add_to_element(i,it_B.index(),it_B.value()*a);
            double value = it_B.value() * a;
            if (std::abs(value) > VECTOR_EPSILON)
              result.add_to_element(i, it_B.index(), value);
          }
        }
      }
    }
    parallel_end return result;
  }

  RCMatrix operator*(const RCMatrix &m) const
  {
    RCMatrix result(n, expected_none_zeros);
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      for (Iterator it_A = row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        assert(j < m.n);
        T a = it_A.value();
        if (std::abs(a) > VECTOR_EPSILON) {
          for (Iterator it_B = m.row_begin(j); it_B; ++it_B) {
            // result.add_to_element(i,it_B.index(),it_B.value()*a);
            double value = it_B.value() * a;
            if (std::abs(value) > VECTOR_EPSILON)
              result.add_to_element(i, it_B.index(), value);
          }
        }
      }
    }
    parallel_end return result;
  }

  RCMatrix sqrt() const
  {
    RCMatrix result(n, expected_none_zeros);
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      for (Iterator it_A = row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        result.set_element(i, j, std::sqrt(it_A.value()));
      }
    }
    parallel_end return result;
  }

  RCMatrix operator*(const double k) const
  {
    RCMatrix result(n, expected_none_zeros);
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      for (Iterator it_A = row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        result.add_to_element(i, j, it_A.value() * k);
      }
    }
    parallel_end return result;
  }

  RCMatrix applyKernel(const RCMatrix &kernel, const int nx, const int ny) const
  {
    RCMatrix result(n, expected_none_zeros);
    // find center position of kernel (half of kernel size)
    int kCols = kernel.n, kRows = kernel.n, rows = nx, cols = ny;
    int kCenterX = kCols / 2;
    int kCenterY = kRows / 2;
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      if (i >= rows)
        break;
      for (Iterator it_A = row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        if (j >= cols)
          break;
        for (int m = 0; m < kRows; ++m) {    // kernel rows
          int mm = kRows - 1 - m;            // row index of flipped kernel
          for (int n = 0; n < kCols; ++n) {  // kernel columns
            int nn = kCols - 1 - n;          // column index of flipped kernel
            // index of input signal, used for checking boundary
            int ii = i + (m - kCenterY);
            int jj = j + (n - kCenterX);
            // ignore input samples which are out of bound
            if (ii >= 0 && ii < rows && jj >= 0 && jj < cols)
              result.add_to_element(i, j, (*this)(ii, jj) * kernel(mm, nn));
          }
        }
      }
    }
    parallel_end return result;
  }

  RCMatrix applyHorizontalKernel(const RCMatrix &kernel, const int nx, const int ny) const
  {
    RCMatrix result(n, expected_none_zeros);
    // find center position of kernel (half of kernel size)
    int kCols = kernel.n, kRows = 1, rows = nx, cols = ny;
    int kCenterX = kCols / 2;
    int kCenterY = kRows / 2;
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      if (i >= rows)
        break;
      for (Iterator it_A = row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        if (j >= cols)
          break;
        for (int m = 0; m < kRows; ++m) {    // kernel rows
          int mm = kRows - 1 - m;            // row index of flipped kernel
          for (int n = 0; n < kCols; ++n) {  // kernel columns
            int nn = kCols - 1 - n;          // column index of flipped kernel
            // index of input signal, used for checking boundary
            int ii = i + (m - kCenterY);
            int jj = j + (n - kCenterX);
            // ignore input samples which are out of bound
            if (ii >= 0 && ii < rows && jj >= 0 && jj < cols)
              result.add_to_element(i, j, (*this)(ii, jj) * kernel(mm, nn));
          }
        }
      }
    }
    parallel_end return result;
  }

  RCMatrix applyVerticalKernel(const RCMatrix &kernel, const int nx, const int ny) const
  {
    RCMatrix result(n, expected_none_zeros);
    // find center position of kernel (half of kernel size)
    int kCols = 1, kRows = kernel.n, rows = nx, cols = ny;
    int kCenterX = kCols / 2;
    int kCenterY = kRows / 2;
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      if (i >= rows)
        break;
      for (Iterator it_A = row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        if (j >= cols)
          break;
        for (int m = 0; m < kRows; ++m) {    // kernel rows
          int mm = kRows - 1 - m;            // row index of flipped kernel
          for (int n = 0; n < kCols; ++n) {  // kernel columns
            int nn = kCols - 1 - n;          // column index of flipped kernel
            // index of input signal, used for checking boundary
            int ii = i + (m - kCenterY);
            int jj = j + (n - kCenterX);
            // ignore input samples which are out of bound
            if (ii >= 0 && ii < rows && jj >= 0 && jj < cols)
              result.add_to_element(i, j, (*this)(ii, jj) * kernel(mm, nn));
          }
        }
      }
    }
    parallel_end return result;
  }

  RCMatrix applySeparableKernel(const RCMatrix &kernelH,
                                const RCMatrix &kernelV,
                                const int nx,
                                const int ny) const
  {
    return applyHorizontalKernel(kernelH, nx, ny).applyVerticalKernel(kernelV, nx, ny);
  }

  RCMatrix applySeparableKernelTwice(const RCMatrix &kernelH,
                                     const RCMatrix &kernelV,
                                     const int nx,
                                     const int ny) const
  {
    return applySeparableKernel(kernelH, kernelV, nx, ny)
        .applySeparableKernel(kernelH, kernelV, nx, ny);
  }

  std::vector<T> operator*(const std::vector<T> &rhs) const
  {
    std::vector<T> result(n, 0.0);
    multiply(rhs, result);
    return result;
  }
  void multiply(const std::vector<T> &rhs, std::vector<T> &result) const
  {
    result.resize(n);
    for (N i = 0; i < n; i++) {
      T new_value = 0.0;
      for (Iterator it = row_begin(i); it; ++it) {
        N j_index = it.index();
        assert(j_index < rhs.size());
        new_value += rhs[j_index] * it.value();
      }
      result[i] = new_value;
    }
  }
  RCMatrix operator+(const RCMatrix &m) const
  {
    RCMatrix A(*this);
    return std::move(A.add(m));
  }
  RCMatrix &add(const RCMatrix &m)
  {
    if (m.n > n)
      resize(m.n);
    parallel_for(m.n)
    {
      N i = parallel_index;
      for (Iterator it = m.row_begin(i); it; ++it) {
        add_to_element(i, it.index(), it.value());
      }
    }
    parallel_end return *this;
  }
  RCMatrix operator-(const RCMatrix &m) const
  {
    RCMatrix A(*this);
    return std::move(A.sub(m));
  }
  RCMatrix &sub(const RCMatrix &m)
  {
    if (m.n > n)
      resize(m.n);
    parallel_for(m.n)
    {
      N i = parallel_index;
      for (Iterator it = m.row_begin(i); it; ++it) {
        add_to_element(i, it.index(), -it.value());
      }
    }
    parallel_end return *this;
  }
  RCMatrix &replace(const RCMatrix &m, int rowInd, int colInd)
  {
    if (m.n > n)
      resize(m.n);
    parallel_for(m.n)
    {
      N i = parallel_index;
      for (Iterator it = m.row_begin(i); it; ++it) {
        set_element(i + rowInd, it.index() + colInd, it.value());
      }
    }
    parallel_end return *this;
  }
  Real max_residual(const std::vector<T> &lhs, const std::vector<T> &rhs) const
  {
    std::vector<T> r = operator*(lhs);
    Real max_residual = 0.0;
    for (N i = 0; i < rhs.size(); i++) {
      if (!empty(i))
        max_residual = std::max(max_residual, std::abs(r[i] - rhs[i]));
    }
    return max_residual;
  }
  std::vector<T> residual_vector(const std::vector<T> &lhs, const std::vector<T> &rhs) const
  {
    std::vector<T> result = operator*(lhs);
    assert(result.size() == rhs.size());
    for (N i = 0; i < result.size(); i++) {
      result[i] = std::abs(result[i] - rhs[i]);
    }
    return result;
  }
  T norm() const
  {
    T result(0.0);
    for (N i = 0; i < n; ++i) {
      for (Iterator it = row_begin(i); it; ++it) {
        result = std::max(result, std::abs(it.value()));
      }
    }
    return result;
  }

  T norm_L2_sqr() const
  {
    T result(0.0);
    for (N i = 0; i < n; ++i) {
      for (Iterator it = row_begin(i); it; ++it) {
        result += (it.value()) * (it.value());
      }
    }
    return result;
  }

  void write_matlab(std::ostream &output,
                    unsigned int rows,
                    unsigned int columns,
                    const char *variable_name)
  {
    output << variable_name << "=sparse([";
    for (N i = 0; i < n; ++i) {
      if (matrix[i]) {
        const std::vector<N> &index = matrix[i]->index;
        for (N j = 0; j < (N)index.size(); ++j) {
          output << i + 1 << " ";
        }
      }
    }
    output << "],...\n  [";
    for (N i = 0; i < n; ++i) {
      if (matrix[i]) {
        const std::vector<N> &index = matrix[i]->index;
        for (N j = 0; j < (N)index.size(); ++j) {
          output << index[j] + (offsets.empty() ? 0 : offsets[i]) + 1 << " ";
        }
      }
    }
    output << "],...\n  [";
    for (N i = 0; i < n; ++i) {
      if (matrix[i]) {
        const std::vector<T> &value = matrix[i]->value;
        for (N j = 0; j < value.size(); ++j) {
          output << value[j] << " ";
        }
      }
    }
    output << "], " << rows << ", " << columns << ");" << std::endl;
  };
  void export_matlab(std::string filename, std::string name)
  {
    // Export this matrix
    std::ofstream file;
    file.open(filename.c_str());
    write_matlab(file, n, getColumnSize(), name.c_str());
    file.close();
  }
  void print_readable(std::string name, bool printNonZero = true)
  {
    std::cout << name << " \n";
    for (int i = 0; i < n; ++i) {
      for (int j = 0; j < n; ++j) {
        if (printNonZero) {
          if ((*this)(i, j) == 0) {
            std::cout << "  .";
            continue;
          }
        }
        else {
          if ((*this)(i, j) == 0) {
            continue;
          }
        }

        if ((*this)(i, j) >= 0)
          std::cout << " ";
        std::cout << " " << (*this)(i, j);
      }
      std::cout << " \n";
    }
  }
  ///
  N n;
  N expected_none_zeros;
  std::vector<RowEntry *> matrix;
  std::vector<int> offsets;
};

template<class N, class T>
static inline RCMatrix<N, T> operator*(const std::vector<T> &diagonal, const RCMatrix<N, T> &A)
{
  RCMatrix<N, T> result(A);
  parallel_for(result.n)
  {
    N row(parallel_index);
    for (auto it = result.dynamic_row_begin(row); it; ++it) {
      it.setValue(it.value() * diagonal[row]);
    }
  }
  parallel_end return std::move(result);
}

template<class N, class T> struct RCFixedMatrix {
  std::vector<N> rowstart;
  std::vector<N> index;
  std::vector<T> value;
  N n;
  N max_rowlength;
  //
  RCFixedMatrix() : n(0), max_rowlength(0)
  {
  }
  RCFixedMatrix(const RCMatrix<N, T> &matrix)
  {
    n = matrix.n;
    rowstart.resize(n + 1);
    rowstart[0] = 0;
    max_rowlength = 0;
    for (N i = 0; i < n; i++) {
      if (!matrix.empty(i)) {
        rowstart[i + 1] = rowstart[i] + matrix.row_nonzero_size(i);
        max_rowlength = std::max(max_rowlength, rowstart[i + 1] - rowstart[i]);
      }
      else {
        rowstart[i + 1] = rowstart[i];
      }
    }
    value.resize(rowstart[n]);
    index.resize(rowstart[n]);
    N j = 0;
    for (N i = 0; i < n; i++) {
      for (typename RCMatrix<N, T>::Iterator it = matrix.row_begin(i); it; ++it) {
        value[j] = it.value();
        index[j] = it.index();
        ++j;
      }
    }
  }
  class Iterator : std::iterator<std::input_iterator_tag, T> {
   public:
    Iterator(N start, N end, const std::vector<N> &index, const std::vector<T> &value)
        : index_array(index), value_array(value), k(start), start(start), end(end)
    {
    }
    operator bool() const
    {
      return k < end;
    }
    Iterator &operator++()
    {
      ++k;
      return *this;
    }
    T value() const
    {
      return value_array[k];
    }
    N index() const
    {
      return index_array[k];
    }
    N size() const
    {
      return end - start;
    }

   private:
    const std::vector<N> &index_array;
    const std::vector<T> &value_array;
    N k, start, end;
  };
  Iterator row_begin(N n) const
  {
    return Iterator(rowstart[n], rowstart[n + 1], index, value);
  }
  std::vector<T> operator*(const std::vector<T> &rhs) const
  {
    std::vector<T> result(n, 0.0);
    multiply(rhs, result);
    return result;
  }
  void multiply(const std::vector<T> &rhs, std::vector<T> &result) const
  {
    result.resize(n);
    parallel_for(n)
    {
      N i = parallel_index;
      T new_value = 0.0;
      for (Iterator it = row_begin(i); it; ++it) {
        N j_index = it.index();
        assert(j_index < rhs.size());
        new_value += rhs[j_index] * it.value();
      }
      result[i] = new_value;
    }
    parallel_end
  }
  RCMatrix<N, T> operator*(const RCFixedMatrix &m) const
  {
    RCMatrix<N, T> result(n, max_rowlength);
    // Run in parallel
    parallel_for(result.n)
    {
      N i = parallel_index;
      for (Iterator it_A = row_begin(i); it_A; ++it_A) {
        N j = it_A.index();
        assert(j < m.n);
        T a = it_A.value();
        if (std::abs(a) > VECTOR_EPSILON) {
          for (Iterator it_B = m.row_begin(j); it_B; ++it_B) {
            result.add_to_element(i, it_B.index(), it_B.value() * a);
          }
        }
      }
    }
    parallel_end return result;
  }

  RCMatrix<N, T> toRCMatrix() const
  {
    RCMatrix<N, T> result(n, 0);
    parallel_for(n)
    {
      N i = parallel_index;
      N size = rowstart[i + 1] - rowstart[i];
      result.matrix[i] = new typename RCMatrix<N, T>::RowEntry;
      result.matrix[i]->index.resize(size);
      result.matrix[i]->value.resize(size);
      for (N j = 0; j < size; j++) {
        result.matrix[i]->index[j] = index[rowstart[i] + j];
        result.matrix[i]->value[j] = value[rowstart[i] + j];
      }
    }
    parallel_end return result;
  }
};

typedef RCMatrix<int, Real> Matrix;
typedef RCFixedMatrix<int, Real> FixedMatrix;

}  // namespace Manta

#undef parallel_for
#undef parallel_end

#undef parallel_block
#undef do_parallel
#undef do_end
#undef block_end

#endif
