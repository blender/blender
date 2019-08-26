/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2009
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#ifndef LEMON_BITS_VARIANT_H
#define LEMON_BITS_VARIANT_H

#include <lemon/assert.h>

// \file
// \brief Variant types

namespace lemon {

  namespace _variant_bits {

    template <int left, int right>
    struct CTMax {
      static const int value = left < right ? right : left;
    };

  }


  // \brief Simple Variant type for two types
  //
  // Simple Variant type for two types. The Variant type is a type-safe
  // union. C++ has strong limitations for using unions, for
  // example you cannot store a type with non-default constructor or
  // destructor in a union. This class always knowns the current
  // state of the variant and it cares for the proper construction
  // and destruction.
  template <typename _First, typename _Second>
  class BiVariant {
  public:

    // \brief The \c First type.
    typedef _First First;
    // \brief The \c Second type.
    typedef _Second Second;

    // \brief Constructor
    //
    // This constructor initalizes to the default value of the \c First
    // type.
    BiVariant() {
      flag = true;
      new(reinterpret_cast<First*>(data)) First();
    }

    // \brief Constructor
    //
    // This constructor initalizes to the given value of the \c First
    // type.
    BiVariant(const First& f) {
      flag = true;
      new(reinterpret_cast<First*>(data)) First(f);
    }

    // \brief Constructor
    //
    // This constructor initalizes to the given value of the \c
    // Second type.
    BiVariant(const Second& s) {
      flag = false;
      new(reinterpret_cast<Second*>(data)) Second(s);
    }

    // \brief Copy constructor
    //
    // Copy constructor
    BiVariant(const BiVariant& bivariant) {
      flag = bivariant.flag;
      if (flag) {
        new(reinterpret_cast<First*>(data)) First(bivariant.first());
      } else {
        new(reinterpret_cast<Second*>(data)) Second(bivariant.second());
      }
    }

    // \brief Destrcutor
    //
    // Destructor
    ~BiVariant() {
      destroy();
    }

    // \brief Set to the default value of the \c First type.
    //
    // This function sets the variant to the default value of the \c
    // First type.
    BiVariant& setFirst() {
      destroy();
      flag = true;
      new(reinterpret_cast<First*>(data)) First();
      return *this;
    }

    // \brief Set to the given value of the \c First type.
    //
    // This function sets the variant to the given value of the \c
    // First type.
    BiVariant& setFirst(const First& f) {
      destroy();
      flag = true;
      new(reinterpret_cast<First*>(data)) First(f);
      return *this;
    }

    // \brief Set to the default value of the \c Second type.
    //
    // This function sets the variant to the default value of the \c
    // Second type.
    BiVariant& setSecond() {
      destroy();
      flag = false;
      new(reinterpret_cast<Second*>(data)) Second();
      return *this;
    }

    // \brief Set to the given value of the \c Second type.
    //
    // This function sets the variant to the given value of the \c
    // Second type.
    BiVariant& setSecond(const Second& s) {
      destroy();
      flag = false;
      new(reinterpret_cast<Second*>(data)) Second(s);
      return *this;
    }

    // \brief Operator form of the \c setFirst()
    BiVariant& operator=(const First& f) {
      return setFirst(f);
    }

    // \brief Operator form of the \c setSecond()
    BiVariant& operator=(const Second& s) {
      return setSecond(s);
    }

    // \brief Assign operator
    BiVariant& operator=(const BiVariant& bivariant) {
      if (this == &bivariant) return *this;
      destroy();
      flag = bivariant.flag;
      if (flag) {
        new(reinterpret_cast<First*>(data)) First(bivariant.first());
      } else {
        new(reinterpret_cast<Second*>(data)) Second(bivariant.second());
      }
      return *this;
    }

    // \brief Reference to the value
    //
    // Reference to the value of the \c First type.
    // \pre The BiVariant should store value of \c First type.
    First& first() {
      LEMON_DEBUG(flag, "Variant wrong state");
      return *reinterpret_cast<First*>(data);
    }

    // \brief Const reference to the value
    //
    // Const reference to the value of the \c First type.
    // \pre The BiVariant should store value of \c First type.
    const First& first() const {
      LEMON_DEBUG(flag, "Variant wrong state");
      return *reinterpret_cast<const First*>(data);
    }

    // \brief Operator form of the \c first()
    operator First&() { return first(); }
    // \brief Operator form of the const \c first()
    operator const First&() const { return first(); }

    // \brief Reference to the value
    //
    // Reference to the value of the \c Second type.
    // \pre The BiVariant should store value of \c Second type.
    Second& second() {
      LEMON_DEBUG(!flag, "Variant wrong state");
      return *reinterpret_cast<Second*>(data);
    }

    // \brief Const reference to the value
    //
    // Const reference to the value of the \c Second type.
    // \pre The BiVariant should store value of \c Second type.
    const Second& second() const {
      LEMON_DEBUG(!flag, "Variant wrong state");
      return *reinterpret_cast<const Second*>(data);
    }

    // \brief Operator form of the \c second()
    operator Second&() { return second(); }
    // \brief Operator form of the const \c second()
    operator const Second&() const { return second(); }

    // \brief %True when the variant is in the first state
    //
    // %True when the variant stores value of the \c First type.
    bool firstState() const { return flag; }

    // \brief %True when the variant is in the second state
    //
    // %True when the variant stores value of the \c Second type.
    bool secondState() const { return !flag; }

  private:

    void destroy() {
      if (flag) {
        reinterpret_cast<First*>(data)->~First();
      } else {
        reinterpret_cast<Second*>(data)->~Second();
      }
    }

    char data[_variant_bits::CTMax<sizeof(First), sizeof(Second)>::value];
    bool flag;
  };

  namespace _variant_bits {

    template <int _idx, typename _TypeMap>
    struct Memory {

      typedef typename _TypeMap::template Map<_idx>::Type Current;

      static void destroy(int index, char* place) {
        if (index == _idx) {
          reinterpret_cast<Current*>(place)->~Current();
        } else {
          Memory<_idx - 1, _TypeMap>::destroy(index, place);
        }
      }

      static void copy(int index, char* to, const char* from) {
        if (index == _idx) {
          new (reinterpret_cast<Current*>(to))
            Current(reinterpret_cast<const Current*>(from));
        } else {
          Memory<_idx - 1, _TypeMap>::copy(index, to, from);
        }
      }

    };

    template <typename _TypeMap>
    struct Memory<-1, _TypeMap> {

      static void destroy(int, char*) {
        LEMON_DEBUG(false, "Variant wrong index.");
      }

      static void copy(int, char*, const char*) {
        LEMON_DEBUG(false, "Variant wrong index.");
      }
    };

    template <int _idx, typename _TypeMap>
    struct Size {
      static const int value =
      CTMax<sizeof(typename _TypeMap::template Map<_idx>::Type),
            Size<_idx - 1, _TypeMap>::value>::value;
    };

    template <typename _TypeMap>
    struct Size<0, _TypeMap> {
      static const int value =
      sizeof(typename _TypeMap::template Map<0>::Type);
    };

  }

  // \brief Variant type
  //
  // Simple Variant type. The Variant type is a type-safe union.
  // C++ has strong limitations for using unions, for example you
  // cannot store type with non-default constructor or destructor in
  // a union. This class always knowns the current state of the
  // variant and it cares for the proper construction and
  // destruction.
  //
  // \param _num The number of the types which can be stored in the
  // variant type.
  // \param _TypeMap This class describes the types of the Variant. The
  // _TypeMap::Map<index>::Type should be a valid type for each index
  // in the range {0, 1, ..., _num - 1}. The \c VariantTypeMap is helper
  // class to define such type mappings up to 10 types.
  //
  // And the usage of the class:
  //\code
  // typedef Variant<3, VariantTypeMap<int, std::string, double> > MyVariant;
  // MyVariant var;
  // var.set<0>(12);
  // std::cout << var.get<0>() << std::endl;
  // var.set<1>("alpha");
  // std::cout << var.get<1>() << std::endl;
  // var.set<2>(0.75);
  // std::cout << var.get<2>() << std::endl;
  //\endcode
  //
  // The result of course:
  //\code
  // 12
  // alpha
  // 0.75
  //\endcode
  template <int _num, typename _TypeMap>
  class Variant {
  public:

    static const int num = _num;

    typedef _TypeMap TypeMap;

    // \brief Constructor
    //
    // This constructor initalizes to the default value of the \c type
    // with 0 index.
    Variant() {
      flag = 0;
      new(reinterpret_cast<typename TypeMap::template Map<0>::Type*>(data))
        typename TypeMap::template Map<0>::Type();
    }


    // \brief Copy constructor
    //
    // Copy constructor
    Variant(const Variant& variant) {
      flag = variant.flag;
      _variant_bits::Memory<num - 1, TypeMap>::copy(flag, data, variant.data);
    }

    // \brief Assign operator
    //
    // Assign operator
    Variant& operator=(const Variant& variant) {
      if (this == &variant) return *this;
      _variant_bits::Memory<num - 1, TypeMap>::
        destroy(flag, data);
      flag = variant.flag;
      _variant_bits::Memory<num - 1, TypeMap>::
        copy(flag, data, variant.data);
      return *this;
    }

    // \brief Destrcutor
    //
    // Destructor
    ~Variant() {
      _variant_bits::Memory<num - 1, TypeMap>::destroy(flag, data);
    }

    // \brief Set to the default value of the type with \c _idx index.
    //
    // This function sets the variant to the default value of the
    // type with \c _idx index.
    template <int _idx>
    Variant& set() {
      _variant_bits::Memory<num - 1, TypeMap>::destroy(flag, data);
      flag = _idx;
      new(reinterpret_cast<typename TypeMap::template Map<_idx>::Type*>(data))
        typename TypeMap::template Map<_idx>::Type();
      return *this;
    }

    // \brief Set to the given value of the type with \c _idx index.
    //
    // This function sets the variant to the given value of the type
    // with \c _idx index.
    template <int _idx>
    Variant& set(const typename _TypeMap::template Map<_idx>::Type& init) {
      _variant_bits::Memory<num - 1, TypeMap>::destroy(flag, data);
      flag = _idx;
      new(reinterpret_cast<typename TypeMap::template Map<_idx>::Type*>(data))
        typename TypeMap::template Map<_idx>::Type(init);
      return *this;
    }

    // \brief Gets the current value of the type with \c _idx index.
    //
    // Gets the current value of the type with \c _idx index.
    template <int _idx>
    const typename TypeMap::template Map<_idx>::Type& get() const {
      LEMON_DEBUG(_idx == flag, "Variant wrong index");
      return *reinterpret_cast<const typename TypeMap::
        template Map<_idx>::Type*>(data);
    }

    // \brief Gets the current value of the type with \c _idx index.
    //
    // Gets the current value of the type with \c _idx index.
    template <int _idx>
    typename _TypeMap::template Map<_idx>::Type& get() {
      LEMON_DEBUG(_idx == flag, "Variant wrong index");
      return *reinterpret_cast<typename TypeMap::template Map<_idx>::Type*>
        (data);
    }

    // \brief Returns the current state of the variant.
    //
    // Returns the current state of the variant.
    int state() const {
      return flag;
    }

  private:

    char data[_variant_bits::Size<num - 1, TypeMap>::value];
    int flag;
  };

  namespace _variant_bits {

    template <int _index, typename _List>
    struct Get {
      typedef typename Get<_index - 1, typename _List::Next>::Type Type;
    };

    template <typename _List>
    struct Get<0, _List> {
      typedef typename _List::Type Type;
    };

    struct List {};

    template <typename _Type, typename _List>
    struct Insert {
      typedef _List Next;
      typedef _Type Type;
    };

    template <int _idx, typename _T0, typename _T1, typename _T2,
              typename _T3, typename _T4, typename _T5, typename _T6,
              typename _T7, typename _T8, typename _T9>
    struct Mapper {
      typedef List L10;
      typedef Insert<_T9, L10> L9;
      typedef Insert<_T8, L9> L8;
      typedef Insert<_T7, L8> L7;
      typedef Insert<_T6, L7> L6;
      typedef Insert<_T5, L6> L5;
      typedef Insert<_T4, L5> L4;
      typedef Insert<_T3, L4> L3;
      typedef Insert<_T2, L3> L2;
      typedef Insert<_T1, L2> L1;
      typedef Insert<_T0, L1> L0;
      typedef typename Get<_idx, L0>::Type Type;
    };

  }

  // \brief Helper class for Variant
  //
  // Helper class to define type mappings for Variant. This class
  // converts the template parameters to be mappable by integer.
  // \see Variant
  template <
    typename _T0,
    typename _T1 = void, typename _T2 = void, typename _T3 = void,
    typename _T4 = void, typename _T5 = void, typename _T6 = void,
    typename _T7 = void, typename _T8 = void, typename _T9 = void>
  struct VariantTypeMap {
    template <int _idx>
    struct Map {
      typedef typename _variant_bits::
      Mapper<_idx, _T0, _T1, _T2, _T3, _T4, _T5, _T6, _T7, _T8, _T9>::Type
      Type;
    };
  };

}


#endif
