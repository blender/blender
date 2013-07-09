// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <carve/carve.hpp>

#ifndef CARVE_USE_TIMINGS
#define CARVE_USE_TIMINGS 0
#endif

namespace carve {

#if CARVE_USE_TIMINGS

  class TimingName {
  public:
    TimingName(const char *name);
    int id;  
  };

  class TimingBlock {
  public:
    /**
     * Starts timing at the end of this constructor, using the given ID. To 
     * associate an ID with a textual name, use Timing::registerID.
     */
    TimingBlock(int id);  
    TimingBlock(const TimingName &name);
    ~TimingBlock();
  };

  class Timing {
  public:
  
    /**
     * Starts timing against a particular ID.
     */
    static void start(int id);
    
    static void start(const TimingName &id) {
      start(id.id);
    }
  
    /**
     * Stops the most recent timing block.
     */
    static double stop();
  
    /**
     * This will print out the current state of recorded time blocks. It will 
     * display the tree of timings, as well as the summaries down the bottom.
     */
    static void printTimings();
    
    /**
     * Associates a particular ID with a text string. This is used when 
     * printing out the timings.
     */
    static void registerID(int id, const char *name);
    
  };
  
#else

  struct TimingName {
    TimingName(const char *) {}
  };
  struct TimingBlock {
    TimingBlock(int /* id */) {}
    TimingBlock(const TimingName & /* name */) {}
  };
  struct Timing {
    static void start(int /* id */) {}
    static void start(const TimingName & /* id */) {}
    static double stop() { return 0; }
    static void printTimings() {}
    static void registerID(int /* id */, const char * /* name */) {}
  };

#endif
}
