//
//  Filename         : BaseIterator.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Classes defining the basic "Iterator" design pattern
//  Date of creation : 18/03/2003
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  BASEITERATOR_H
# define BASEITERATOR_H

# include <iterator>

// use for iterators defintions
template <class Element>
class Nonconst_traits;

template <class Element>
class Const_traits {
public:
  typedef Element value_type;
  typedef const Element&  reference;
  typedef const Element*  pointer;
  typedef ptrdiff_t       difference_type;
  typedef Nonconst_traits<Element> Non_const_traits;
};

template <class Element>
class Nonconst_traits {
public:
  typedef Element value_type;
  typedef Element& reference;
  typedef Element* pointer;
  typedef ptrdiff_t       difference_type;
  typedef Nonconst_traits<Element> Non_const_traits;
};

class InputIteratorTag_Traits {
public:
  typedef std::input_iterator_tag iterator_category;
};

class BidirectionalIteratorTag_Traits {
public:
  typedef std::bidirectional_iterator_tag iterator_category;
};

template<class Traits, class IteratorTagTraits>
class IteratorBase
{
public:

  virtual ~IteratorBase() {}

  virtual bool begin() const = 0;
  virtual bool end() const = 0;

  typedef typename IteratorTagTraits::iterator_category     iterator_category;
  typedef typename Traits::value_type                       value_type;
  typedef typename Traits::difference_type                  difference_type;
  typedef typename Traits::pointer                          pointer;
  typedef typename Traits::reference                        reference;

protected:

  IteratorBase() {}
};

#endif // BASEITERATOR_H
