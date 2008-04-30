//
//  Filename         : StrokeAdvancedIterators.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Iterators used to iterate over the elements of the Stroke
//                     Can't be used in python
//  Date of creation : 01/07/2003
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

#ifndef  STROKEADVANCEDITERATORS_H
# define STROKEADVANCEDITERATORS_H

# include "Stroke.h"

namespace StrokeInternal {

  class vertex_const_traits : public Const_traits<StrokeVertex*> {
  public:
    typedef std::deque<StrokeVertex*> vertex_container; 
    typedef vertex_container::const_iterator vertex_container_iterator ;
  };
  class vertex_nonconst_traits : public Nonconst_traits<StrokeVertex*> {
  public:
    typedef std::deque<StrokeVertex*> vertex_container; //! the vertices container
    typedef vertex_container::iterator vertex_container_iterator ;
  };


  template<class Traits>
    class vertex_iterator_base : public IteratorBase<Traits,BidirectionalIteratorTag_Traits>
  {
  public:
    typedef vertex_iterator_base<Traits>  Self;
  protected:
    typedef IteratorBase<Traits,BidirectionalIteratorTag_Traits> parent_class;
    typedef typename Traits::vertex_container_iterator vertex_container_iterator;
    typedef vertex_iterator_base<vertex_nonconst_traits> iterator;
    typedef vertex_iterator_base<vertex_const_traits> const_iterator;
    //protected:
  public:
    vertex_container_iterator _it;
    vertex_container_iterator _begin;
    vertex_container_iterator _end;
  public:
    friend class Stroke;
    //friend class vertex_iterator;
    inline vertex_iterator_base()
      : parent_class()
    {}
    inline vertex_iterator_base(const iterator& iBrother) 
      : parent_class()
    {_it = iBrother._it;_begin = iBrother._begin;_end = iBrother._end;} 
    inline vertex_iterator_base(const const_iterator& iBrother) 
      : parent_class()
    {_it = iBrother._it;_begin = iBrother._begin;_end = iBrother._end;} 
  //protected://FIXME
  public:
    inline vertex_iterator_base(vertex_container_iterator it, vertex_container_iterator begin, vertex_container_iterator end)
      : parent_class()
    {
      _it = it;
      _begin = begin;
      _end = end;
    }
    
  public:
    virtual ~vertex_iterator_base() {}
    
    virtual bool begin() const {return _it==_begin? true : false;}
    virtual bool end() const {return _it==_end ? true : false;}
    
    // operators
    inline Self& operator++()  // operator corresponding to ++i
    { 
      ++_it;
      return *(this);
    }
    inline Self operator++(int)  // opérateur correspondant à i++ 
    {
      Self tmp = *this;        // C'est pour cela qu'on stocke la valeur
      ++_it;                    // dans un temporaire. 
      return tmp;
    }
    inline Self& operator--()  // operator corresponding to ++i
    { 
      --_it;
      return *(this);
    }
    inline Self operator--(int)  // opérateur correspondant à i++ 
    {                                  // c.a.d qui renvoie la valeur *puis* incrémente.
      Self tmp = *this;        // C'est pour cela qu'on stocke la valeur
      --_it;                    // dans un temporaire. 
      return tmp;
    }
    
    // comparibility
    virtual bool operator!=(const Self& b) const
    {
      return (_it != b._it);
    }
    virtual bool operator==(const Self& b) const
    {
      return !(*this != b);
    }
    
    // dereferencing
    virtual typename Traits::reference operator*() const  {return *(_it);}
    virtual typename Traits::pointer operator->() const { return &(operator*());}
    
    /*! accessors */
    inline vertex_container_iterator it() const {return _it;}
    inline vertex_container_iterator getBegin() const {return _begin;}
    inline vertex_container_iterator getEnd() const {return _end;}
  };

} // end of namespace StrokeInternal


#endif // STROKEADVANCEDITERATORS_H


