// This file is part of Eigen, a lightweight C++ template library
// for linear algebra. Eigen itself is part of the KDE project.
//
// Copyright (C) 2006-2008 Benoit Jacob <jacob.benoit.1@gmail.com>
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

#ifndef EIGEN_IO_H
#define EIGEN_IO_H

enum { Raw, AlignCols };

/** \class IOFormat
  *
  * \brief Stores a set of parameters controlling the way matrices are printed
  *
  * List of available parameters:
  *  - \b precision number of digits for floating point values
  *  - \b flags can be either Raw (default) or AlignCols which aligns all the columns
  *  - \b coeffSeparator string printed between two coefficients of the same row
  *  - \b rowSeparator string printed between two rows
  *  - \b rowPrefix string printed at the beginning of each row
  *  - \b rowSuffix string printed at the end of each row
  *  - \b matPrefix string printed at the beginning of the matrix
  *  - \b matSuffix string printed at the end of the matrix
  *
  * Example: \include IOFormat.cpp
  * Output: \verbinclude IOFormat.out
  *
  * \sa MatrixBase::format(), class WithFormat
  */
struct IOFormat
{
  /** Default contructor, see class IOFormat for the meaning of the parameters */
  IOFormat(int _precision=4, int _flags=Raw,
    const std::string& _coeffSeparator = " ",
    const std::string& _rowSeparator = "\n", const std::string& _rowPrefix="", const std::string& _rowSuffix="",
    const std::string& _matPrefix="", const std::string& _matSuffix="")
  : matPrefix(_matPrefix), matSuffix(_matSuffix), rowPrefix(_rowPrefix), rowSuffix(_rowSuffix), rowSeparator(_rowSeparator),
    coeffSeparator(_coeffSeparator), precision(_precision), flags(_flags)
  {
    rowSpacer = "";
    int i = int(matSuffix.length())-1;
    while (i>=0 && matSuffix[i]!='\n')
    {
      rowSpacer += ' ';
      i--;
    }
  }
  std::string matPrefix, matSuffix;
  std::string rowPrefix, rowSuffix, rowSeparator, rowSpacer;
  std::string coeffSeparator;
  int precision;
  int flags;
};

/** \class WithFormat
  *
  * \brief Pseudo expression providing matrix output with given format
  *
  * \param ExpressionType the type of the object on which IO stream operations are performed
  *
  * This class represents an expression with stream operators controlled by a given IOFormat.
  * It is the return type of MatrixBase::format()
  * and most of the time this is the only way it is used.
  *
  * See class IOFormat for some examples.
  *
  * \sa MatrixBase::format(), class IOFormat
  */
template<typename ExpressionType>
class WithFormat
{
  public:

    WithFormat(const ExpressionType& matrix, const IOFormat& format)
      : m_matrix(matrix), m_format(format)
    {}

    friend std::ostream & operator << (std::ostream & s, const WithFormat& wf)
    {
      return ei_print_matrix(s, wf.m_matrix.eval(), wf.m_format);
    }

  protected:
    const typename ExpressionType::Nested m_matrix;
    IOFormat m_format;
};

/** \returns a WithFormat proxy object allowing to print a matrix the with given
  * format \a fmt.
  *
  * See class IOFormat for some examples.
  *
  * \sa class IOFormat, class WithFormat
  */
template<typename Derived>
inline const WithFormat<Derived>
MatrixBase<Derived>::format(const IOFormat& fmt) const
{
  return WithFormat<Derived>(derived(), fmt);
}

/** \internal
  * print the matrix \a _m to the output stream \a s using the output format \a fmt */
template<typename Derived>
std::ostream & ei_print_matrix(std::ostream & s, const Derived& _m, const IOFormat& fmt)
{
  const typename Derived::Nested m = _m;

  int width = 0;
  if (fmt.flags & AlignCols)
  {
    // compute the largest width
    for(int j = 1; j < m.cols(); ++j)
      for(int i = 0; i < m.rows(); ++i)
      {
        std::stringstream sstr;
        sstr.precision(fmt.precision);
        sstr << m.coeff(i,j);
        width = std::max<int>(width, int(sstr.str().length()));
      }
  }
  s.precision(fmt.precision);
  s << fmt.matPrefix;
  for(int i = 0; i < m.rows(); ++i)
  {
    if (i)
      s << fmt.rowSpacer;
    s << fmt.rowPrefix;
    if(width) s.width(width);
    s << m.coeff(i, 0);
    for(int j = 1; j < m.cols(); ++j)
    {
      s << fmt.coeffSeparator;
      if (width) s.width(width);
      s << m.coeff(i, j);
    }
    s << fmt.rowSuffix;
    if( i < m.rows() - 1)
      s << fmt.rowSeparator;
  }
  s << fmt.matSuffix;
  return s;
}

/** \relates MatrixBase
  *
  * Outputs the matrix, to the given stream.
  *
  * If you wish to print the matrix with a format different than the default, use MatrixBase::format().
  *
  * It is also possible to change the default format by defining EIGEN_DEFAULT_IO_FORMAT before including Eigen headers.
  * If not defined, this will automatically be defined to Eigen::IOFormat(), that is the Eigen::IOFormat with default parameters.
  *
  * \sa MatrixBase::format()
  */
template<typename Derived>
std::ostream & operator <<
(std::ostream & s,
 const MatrixBase<Derived> & m)
{
  return ei_print_matrix(s, m.eval(), EIGEN_DEFAULT_IO_FORMAT);
}

#endif // EIGEN_IO_H
