// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2008 Gael Guennebaud <g.gael@free.fr>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN_AMBIVECTOR_H
#define EIGEN_AMBIVECTOR_H

/** \internal
  * Hybrid sparse/dense vector class designed for intensive read-write operations.
  *
  * See BasicSparseLLT and SparseProduct for usage examples.
  */
template<typename _Scalar> class AmbiVector
{
  public:
    typedef _Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    AmbiVector(int size)
      : m_buffer(0), m_size(0), m_allocatedSize(0), m_allocatedElements(0), m_mode(-1)
    {
      resize(size);
    }

    void init(RealScalar estimatedDensity);
    void init(int mode);

    void nonZeros() const;

    /** Specifies a sub-vector to work on */
    void setBounds(int start, int end) { m_start = start; m_end = end; }

    void setZero();

    void restart();
    Scalar& coeffRef(int i);
    Scalar coeff(int i);

    class Iterator;

    ~AmbiVector() { delete[] m_buffer; }

    void resize(int size)
    {
      if (m_allocatedSize < size)
        reallocate(size);
      m_size = size;
    }

    int size() const { return m_size; }

  protected:

    void reallocate(int size)
    {
      // if the size of the matrix is not too large, let's allocate a bit more than needed such
      // that we can handle dense vector even in sparse mode.
      delete[] m_buffer;
      if (size<1000)
      {
        int allocSize = (size * sizeof(ListEl))/sizeof(Scalar);
        m_allocatedElements = (allocSize*sizeof(Scalar))/sizeof(ListEl);
        m_buffer = new Scalar[allocSize];
      }
      else
      {
        m_allocatedElements = (size*sizeof(Scalar))/sizeof(ListEl);
        m_buffer = new Scalar[size];
      }
      m_size = size;
      m_start = 0;
      m_end = m_size;
    }

    void reallocateSparse()
    {
      int copyElements = m_allocatedElements;
      m_allocatedElements = std::min(int(m_allocatedElements*1.5),m_size);
      int allocSize = m_allocatedElements * sizeof(ListEl);
      allocSize = allocSize/sizeof(Scalar) + (allocSize%sizeof(Scalar)>0?1:0);
      Scalar* newBuffer = new Scalar[allocSize];
      memcpy(newBuffer,  m_buffer,  copyElements * sizeof(ListEl));
    }

  protected:
    // element type of the linked list
    struct ListEl
    {
      int next;
      int index;
      Scalar value;
    };

    // used to store data in both mode
    Scalar* m_buffer;
    int m_size;
    int m_start;
    int m_end;
    int m_allocatedSize;
    int m_allocatedElements;
    int m_mode;

    // linked list mode
    int m_llStart;
    int m_llCurrent;
    int m_llSize;

  private:
    AmbiVector(const AmbiVector&);

};

/** \returns the number of non zeros in the current sub vector */
template<typename Scalar>
void AmbiVector<Scalar>::nonZeros() const
{
  if (m_mode==IsSparse)
    return m_llSize;
  else
    return m_end - m_start;
}

template<typename Scalar>
void AmbiVector<Scalar>::init(RealScalar estimatedDensity)
{
  if (estimatedDensity>0.1)
    init(IsDense);
  else
    init(IsSparse);
}

template<typename Scalar>
void AmbiVector<Scalar>::init(int mode)
{
  m_mode = mode;
  if (m_mode==IsSparse)
  {
    m_llSize = 0;
    m_llStart = -1;
  }
}

/** Must be called whenever we might perform a write access
  * with an index smaller than the previous one.
  *
  * Don't worry, this function is extremely cheap.
  */
template<typename Scalar>
void AmbiVector<Scalar>::restart()
{
  m_llCurrent = m_llStart;
}

/** Set all coefficients of current subvector to zero */
template<typename Scalar>
void AmbiVector<Scalar>::setZero()
{
  if (m_mode==IsDense)
  {
    for (int i=m_start; i<m_end; ++i)
      m_buffer[i] = Scalar(0);
  }
  else
  {
    ei_assert(m_mode==IsSparse);
    m_llSize = 0;
    m_llStart = -1;
  }
}

template<typename Scalar>
Scalar& AmbiVector<Scalar>::coeffRef(int i)
{
  if (m_mode==IsDense)
    return m_buffer[i];
  else
  {
    ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_buffer);
    // TODO factorize the following code to reduce code generation
    ei_assert(m_mode==IsSparse);
    if (m_llSize==0)
    {
      // this is the first element
      m_llStart = 0;
      m_llCurrent = 0;
      ++m_llSize;
      llElements[0].value = Scalar(0);
      llElements[0].index = i;
      llElements[0].next = -1;
      return llElements[0].value;
    }
    else if (i<llElements[m_llStart].index)
    {
      // this is going to be the new first element of the list
      ListEl& el = llElements[m_llSize];
      el.value = Scalar(0);
      el.index = i;
      el.next = m_llStart;
      m_llStart = m_llSize;
      ++m_llSize;
      m_llCurrent = m_llStart;
      return el.value;
    }
    else
    {
      int nextel = llElements[m_llCurrent].next;
      ei_assert(i>=llElements[m_llCurrent].index && "you must call restart() before inserting an element with lower or equal index");
      while (nextel >= 0 && llElements[nextel].index<=i)
      {
        m_llCurrent = nextel;
        nextel = llElements[nextel].next;
      }

      if (llElements[m_llCurrent].index==i)
      {
        // the coefficient already exists and we found it !
        return llElements[m_llCurrent].value;
      }
      else
      {
        if (m_llSize>=m_allocatedElements)
          reallocateSparse();
        ei_internal_assert(m_llSize<m_size && "internal error: overflow in sparse mode");
        // let's insert a new coefficient
        ListEl& el = llElements[m_llSize];
        el.value = Scalar(0);
        el.index = i;
        el.next = llElements[m_llCurrent].next;
        llElements[m_llCurrent].next = m_llSize;
        ++m_llSize;
        return el.value;
      }
    }
  }
}

template<typename Scalar>
Scalar AmbiVector<Scalar>::coeff(int i)
{
  if (m_mode==IsDense)
    return m_buffer[i];
  else
  {
    ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_buffer);
    ei_assert(m_mode==IsSparse);
    if ((m_llSize==0) || (i<llElements[m_llStart].index))
    {
      return Scalar(0);
    }
    else
    {
      int elid = m_llStart;
      while (elid >= 0 && llElements[elid].index<i)
        elid = llElements[elid].next;

      if (llElements[elid].index==i)
        return llElements[m_llCurrent].value;
      else
        return Scalar(0);
    }
  }
}

/** Iterator over the nonzero coefficients */
template<typename _Scalar>
class AmbiVector<_Scalar>::Iterator
{
  public:
    typedef _Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;

    /** Default constructor
      * \param vec the vector on which we iterate
      * \param epsilon the minimal value used to prune zero coefficients.
      * In practice, all coefficients having a magnitude smaller than \a epsilon
      * are skipped.
      */
    Iterator(const AmbiVector& vec, RealScalar epsilon = RealScalar(0.1)*precision<RealScalar>())
      : m_vector(vec)
    {
      m_epsilon = epsilon;
      m_isDense = m_vector.m_mode==IsDense;
      if (m_isDense)
      {
        m_cachedIndex = m_vector.m_start-1;
        ++(*this);
      }
      else
      {
        ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_vector.m_buffer);
        m_currentEl = m_vector.m_llStart;
        while (m_currentEl>=0 && ei_abs(llElements[m_currentEl].value)<m_epsilon)
          m_currentEl = llElements[m_currentEl].next;
        if (m_currentEl<0)
        {
          m_cachedIndex = -1;
        }
        else
        {
          m_cachedIndex = llElements[m_currentEl].index;
          m_cachedValue = llElements[m_currentEl].value;
        }
      }
    }

    int index() const { return m_cachedIndex; }
    Scalar value() const { return m_cachedValue; }

    operator bool() const { return m_cachedIndex>=0; }

    Iterator& operator++()
    {
      if (m_isDense)
      {
        do {
          ++m_cachedIndex;
        } while (m_cachedIndex<m_vector.m_end && ei_abs(m_vector.m_buffer[m_cachedIndex])<m_epsilon);
        if (m_cachedIndex<m_vector.m_end)
          m_cachedValue = m_vector.m_buffer[m_cachedIndex];
        else
          m_cachedIndex=-1;
      }
      else
      {
        ListEl* EIGEN_RESTRICT llElements = reinterpret_cast<ListEl*>(m_vector.m_buffer);
        do {
          m_currentEl = llElements[m_currentEl].next;
        } while (m_currentEl>=0 && ei_abs(llElements[m_currentEl].value)<m_epsilon);
        if (m_currentEl<0)
        {
          m_cachedIndex = -1;
        }
        else
        {
          m_cachedIndex = llElements[m_currentEl].index;
          m_cachedValue = llElements[m_currentEl].value;
        }
      }
      return *this;
    }

  protected:
    const AmbiVector& m_vector; // the target vector
    int m_currentEl;            // the current element in sparse/linked-list mode
    RealScalar m_epsilon;       // epsilon used to prune zero coefficients
    int m_cachedIndex;          // current coordinate
    Scalar m_cachedValue;       // current value
    bool m_isDense;             // mode of the vector
};


#endif // EIGEN_AMBIVECTOR_H
