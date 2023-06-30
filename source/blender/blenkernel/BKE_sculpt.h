#pragma once

/* NotForPR: Show uv mesh in uv editor in sculpt mode. */
//#define DEBUG_SHOW_SCULPT_BM_UV_EDGES

/** When #BRUSH_ACCUMULATE is used */
#define SCULPT_TOOL_HAS_ACCUMULATE(t) \
  ELEM(t, \
       SCULPT_TOOL_DRAW, \
       SCULPT_TOOL_DRAW_SHARP, \
       SCULPT_TOOL_SLIDE_RELAX, \
       SCULPT_TOOL_CREASE, \
       SCULPT_TOOL_BLOB, \
       SCULPT_TOOL_INFLATE, \
       SCULPT_TOOL_CLAY, \
       SCULPT_TOOL_CLAY_STRIPS, \
       SCULPT_TOOL_CLAY_THUMB, \
       SCULPT_TOOL_ROTATE, \
       SCULPT_TOOL_SCRAPE, \
       SCULPT_TOOL_FLATTEN)

#define SCULPT_TOOL_HAS_NORMAL_WEIGHT(t) \
  ELEM(t, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK, SCULPT_TOOL_ELASTIC_DEFORM)

#define SCULPT_TOOL_HAS_RAKE(t) ELEM(t, SCULPT_TOOL_SNAKE_HOOK)

#define SCULPT_TOOL_HAS_DYNTOPO(t) \
  (ELEM(t, /* These brushes, as currently coded, cannot support dynamic topology */ \
        SCULPT_TOOL_GRAB, \
        SCULPT_TOOL_CLOTH, \
        SCULPT_TOOL_DISPLACEMENT_ERASER, \
        SCULPT_TOOL_ELASTIC_DEFORM, \
        SCULPT_TOOL_BOUNDARY, \
        SCULPT_TOOL_POSE /*SCULPT_TOOL_DRAW_FACE_SETS,*/ \
\
        /* These brushes could handle dynamic topology, \ \
         * but user feedback indicates it's better not to */ \
        /*SCULPT_TOOL_MASK*/) == 0)

#define SCULPT_TOOL_HAS_TOPOLOGY_RAKE(t) \
  (ELEM(t, /* These brushes, as currently coded, cannot support topology rake. */ \
        SCULPT_TOOL_GRAB, \
        SCULPT_TOOL_ELASTIC_DEFORM, \
        SCULPT_TOOL_ROTATE, \
        SCULPT_TOOL_DISPLACEMENT_ERASER, \
        SCULPT_TOOL_SLIDE_RELAX, \
        SCULPT_TOOL_MASK) == 0)
