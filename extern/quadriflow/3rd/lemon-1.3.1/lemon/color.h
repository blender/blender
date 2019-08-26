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

#ifndef LEMON_COLOR_H
#define LEMON_COLOR_H

#include<vector>
#include<lemon/math.h>
#include<lemon/maps.h>


///\ingroup misc
///\file
///\brief Tools to manage RGB colors.

namespace lemon {


  /// \addtogroup misc
  /// @{

  ///Data structure representing RGB colors.

  ///Data structure representing RGB colors.
  class Color
  {
    double _r,_g,_b;
  public:
    ///Default constructor
    Color() {}
    ///Constructor
    Color(double r,double g,double b) :_r(r),_g(g),_b(b) {};
    ///Set the red component
    double & red() {return _r;}
    ///Return the red component
    const double & red() const {return _r;}
    ///Set the green component
    double & green() {return _g;}
    ///Return the green component
    const double & green() const {return _g;}
    ///Set the blue component
    double & blue() {return _b;}
    ///Return the blue component
    const double & blue() const {return _b;}
    ///Set the color components
    void set(double r,double g,double b) { _r=r;_g=g;_b=b; };
  };

  /// White color constant
  extern const Color WHITE;
  /// Black color constant
  extern const Color BLACK;
  /// Red color constant
  extern const Color RED;
  /// Green color constant
  extern const Color GREEN;
  /// Blue color constant
  extern const Color BLUE;
  /// Yellow color constant
  extern const Color YELLOW;
  /// Magenta color constant
  extern const Color MAGENTA;
  /// Cyan color constant
  extern const Color CYAN;
  /// Grey color constant
  extern const Color GREY;
  /// Dark red color constant
  extern const Color DARK_RED;
  /// Dark green color constant
  extern const Color DARK_GREEN;
  /// Drak blue color constant
  extern const Color DARK_BLUE;
  /// Dark yellow color constant
  extern const Color DARK_YELLOW;
  /// Dark magenta color constant
  extern const Color DARK_MAGENTA;
  /// Dark cyan color constant
  extern const Color DARK_CYAN;

  ///Map <tt>int</tt>s to different <tt>Color</tt>s

  ///This map assigns one of the predefined \ref Color "Color"s to
  ///each <tt>int</tt>. It is possible to change the colors as well as
  ///their number. The integer range is cyclically mapped to the
  ///provided set of colors.
  ///
  ///This is a true \ref concepts::ReferenceMap "reference map", so
  ///you can also change the actual colors.

  class Palette : public MapBase<int,Color>
  {
    std::vector<Color> colors;
  public:
    ///Constructor

    ///Constructor.
    ///\param have_white Indicates whether white is among the
    ///provided initial colors (\c true) or not (\c false). If it is true,
    ///white will be assigned to \c 0.
    ///\param num The number of the allocated colors. If it is \c -1,
    ///the default color configuration is set up (26 color plus optionaly the
    ///white).  If \c num is less then 26/27 then the default color
    ///list is cut. Otherwise the color list is filled repeatedly with
    ///the default color list.  (The colors can be changed later on.)
    Palette(bool have_white=false,int num=-1)
    {
      if (num==0) return;
      do {
        if(have_white) colors.push_back(Color(1,1,1));

        colors.push_back(Color(0,0,0));
        colors.push_back(Color(1,0,0));
        colors.push_back(Color(0,1,0));
        colors.push_back(Color(0,0,1));
        colors.push_back(Color(1,1,0));
        colors.push_back(Color(1,0,1));
        colors.push_back(Color(0,1,1));

        colors.push_back(Color(.5,0,0));
        colors.push_back(Color(0,.5,0));
        colors.push_back(Color(0,0,.5));
        colors.push_back(Color(.5,.5,0));
        colors.push_back(Color(.5,0,.5));
        colors.push_back(Color(0,.5,.5));

        colors.push_back(Color(.5,.5,.5));
        colors.push_back(Color(1,.5,.5));
        colors.push_back(Color(.5,1,.5));
        colors.push_back(Color(.5,.5,1));
        colors.push_back(Color(1,1,.5));
        colors.push_back(Color(1,.5,1));
        colors.push_back(Color(.5,1,1));

        colors.push_back(Color(1,.5,0));
        colors.push_back(Color(.5,1,0));
        colors.push_back(Color(1,0,.5));
        colors.push_back(Color(0,1,.5));
        colors.push_back(Color(0,.5,1));
        colors.push_back(Color(.5,0,1));
      } while(int(colors.size())<num);
      if(num>=0) colors.resize(num);
    }
    ///\e
    Color &operator[](int i)
    {
      return colors[i%colors.size()];
    }
    ///\e
    const Color &operator[](int i) const
    {
      return colors[i%colors.size()];
    }
    ///\e
    void set(int i,const Color &c)
    {
      colors[i%colors.size()]=c;
    }
    ///Adds a new color to the end of the color list.
    void add(const Color &c)
    {
      colors.push_back(c);
    }

    ///Sets the number of the existing colors.
    void resize(int s) { colors.resize(s);}
    ///Returns the number of the existing colors.
    int size() const { return int(colors.size());}
  };

  ///Returns a visibly distinct \ref Color

  ///Returns a \ref Color which is as different from the given parameter
  ///as it is possible.
  inline Color distantColor(const Color &c)
  {
    return Color(c.red()<.5?1:0,c.green()<.5?1:0,c.blue()<.5?1:0);
  }
  ///Returns black for light colors and white for the dark ones.

  ///Returns black for light colors and white for the dark ones.
  inline Color distantBW(const Color &c){
    return (.2125*c.red()+.7154*c.green()+.0721*c.blue())<.5 ? WHITE : BLACK;
  }

  /// @}

} //END OF NAMESPACE LEMON

#endif // LEMON_COLOR_H
