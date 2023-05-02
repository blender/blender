/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#include <cstdlib>

#include "COM_DoubleEdgeMaskOperation.h"

namespace blender::compositor {

/* This part has been copied from the double edge mask. */
static void do_adjacentKeepBorders(
    uint t, uint rw, const uint *limask, const uint *lomask, uint *lres, float *res, uint *rsize)
{
  int x;
  uint isz = 0; /* Inner edge size. */
  uint osz = 0; /* Outer edge size. */
  uint gsz = 0; /* Gradient fill area size. */
  /* Test the four corners */
  /* Upper left corner. */
  x = t - rw + 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel underneath, or to the right, are empty in the inner mask,
     * but filled in the outer mask. */
    if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + 1] && lomask[x + 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }
  /* Upper right corner. */
  x = t;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel underneath, or to the left, are empty in the inner mask,
     * but filled in the outer mask. */
    if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x - 1] && lomask[x - 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }
  /* Lower left corner. */
  x = 0;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel above, or to the right, are empty in the inner mask,
     * but filled in the outer mask. */
    if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x + 1] && lomask[x + 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }
  /* Lower right corner. */
  x = rw - 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel above, or to the left, are empty in the inner mask,
     * but filled in the outer mask. */
    if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x - 1] && lomask[x - 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }

  /* Test the TOP row of pixels in buffer, except corners */
  for (x = t - 1; x >= (t - rw) + 2; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel to the right, or to the left, are empty in the inner mask,
       * but filled in the outer mask. */
      if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }

  /* Test the BOTTOM row of pixels in buffer, except corners */
  for (x = rw - 2; x; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel to the right, or to the left, are empty in the inner mask,
       * but filled in the outer mask. */
      if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }
  /* Test the LEFT edge of pixels in buffer, except corners */
  for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel underneath, or above, are empty in the inner mask,
       * but filled in the outer mask. */
      if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }

  /* Test the RIGHT edge of pixels in buffer, except corners */
  for (x = t - rw; x > rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel underneath, or above, are empty in the inner mask,
       * but filled in the outer mask. */
      if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }

  rsize[0] = isz; /* Fill in our return sizes for edges + fill. */
  rsize[1] = osz;
  rsize[2] = gsz;
}

static void do_adjacentBleedBorders(
    uint t, uint rw, const uint *limask, const uint *lomask, uint *lres, float *res, uint *rsize)
{
  int x;
  uint isz = 0; /* Inner edge size. */
  uint osz = 0; /* Outer edge size. */
  uint gsz = 0; /* Gradient fill area size. */
  /* Test the four corners */
  /* Upper left corner. */
  x = t - rw + 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel underneath, or to the right, are empty in the inner mask,
     * but filled in the outer mask. */
    if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + 1] && lomask[x + 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty underneath or to the right. */
    if (!lomask[x - rw] || !lomask[x + 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Upper right corner. */
  x = t;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel underneath, or to the left, are empty in the inner mask,
     * but filled in the outer mask. */
    if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x - 1] && lomask[x - 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty underneath or to the left. */
    if (!lomask[x - rw] || !lomask[x - 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Lower left corner. */
  x = 0;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel above, or to the right, are empty in the inner mask,
     * But filled in the outer mask. */
    if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x + 1] && lomask[x + 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty above or to the right. */
    if (!lomask[x + rw] || !lomask[x + 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Lower right corner. */
  x = rw - 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if pixel above, or to the left, are empty in the inner mask,
     * but filled in the outer mask. */
    if ((!limask[x + rw] && lomask[x + rw]) || (!limask[x - 1] && lomask[x - 1])) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty above or to the left. */
    if (!lomask[x + rw] || !lomask[x - 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Test the TOP row of pixels in buffer, except corners */
  for (x = t - 1; x >= (t - rw) + 2; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel to the left, or to the right, are empty in the inner mask,
       * but filled in the outer mask. */
      if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty to the left or to the right. */
      if (!lomask[x - 1] || !lomask[x + 1]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }

  /* Test the BOTTOM row of pixels in buffer, except corners */
  for (x = rw - 2; x; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel to the left, or to the right, are empty in the inner mask,
       * but filled in the outer mask. */
      if ((!limask[x - 1] && lomask[x - 1]) || (!limask[x + 1] && lomask[x + 1])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty to the left or to the right. */
      if (!lomask[x - 1] || !lomask[x + 1]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }
  /* Test the LEFT edge of pixels in buffer, except corners */
  for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel underneath, or above, are empty in the inner mask,
       * but filled in the outer mask. */
      if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty underneath or above. */
      if (!lomask[x - rw] || !lomask[x + rw]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }

  /* Test the RIGHT edge of pixels in buffer, except corners */
  for (x = t - rw; x > rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if pixel underneath, or above, are empty in the inner mask,
       * But filled in the outer mask. */
      if ((!limask[x - rw] && lomask[x - rw]) || (!limask[x + rw] && lomask[x + rw])) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty underneath or above. */
      if (!lomask[x - rw] || !lomask[x + rw]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }

  rsize[0] = isz; /* Fill in our return sizes for edges + fill. */
  rsize[1] = osz;
  rsize[2] = gsz;
}

static void do_allKeepBorders(
    uint t, uint rw, const uint *limask, const uint *lomask, uint *lres, float *res, uint *rsize)
{
  int x;
  uint isz = 0; /* Inner edge size. */
  uint osz = 0; /* Outer edge size. */
  uint gsz = 0; /* Gradient fill area size. */
  /* Test the four corners. */
  /* Upper left corner. */
  x = t - rw + 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if the inner mask is empty underneath or to the right. */
    if (!limask[x - rw] || !limask[x + 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }
  /* Upper right corner. */
  x = t;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if the inner mask is empty underneath or to the left. */
    if (!limask[x - rw] || !limask[x - 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }
  /* Lower left corner. */
  x = 0;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if inner mask is empty above or to the right. */
    if (!limask[x + rw] || !limask[x + 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }
  /* Lower right corner. */
  x = rw - 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if inner mask is empty above or to the left. */
    if (!limask[x + rw] || !limask[x - 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    osz++;       /* Increment outer edge size. */
    lres[x] = 3; /* Flag pixel as outer edge. */
  }

  /* Test the TOP row of pixels in buffer, except corners */
  for (x = t - 1; x >= (t - rw) + 2; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty to the left or to the right. */
      if (!limask[x - 1] || !limask[x + 1]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }

  /* Test the BOTTOM row of pixels in buffer, except corners */
  for (x = rw - 2; x; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty to the left or to the right. */
      if (!limask[x - 1] || !limask[x + 1]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }
  /* Test the LEFT edge of pixels in buffer, except corners */
  for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty underneath or above. */
      if (!limask[x - rw] || !limask[x + rw]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }

  /* Test the RIGHT edge of pixels in buffer, except corners */
  for (x = t - rw; x > rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty underneath or above. */
      if (!limask[x - rw] || !limask[x + rw]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
  }

  rsize[0] = isz; /* Fill in our return sizes for edges + fill. */
  rsize[1] = osz;
  rsize[2] = gsz;
}

static void do_allBleedBorders(
    uint t, uint rw, const uint *limask, const uint *lomask, uint *lres, float *res, uint *rsize)
{
  int x;
  uint isz = 0; /* Inner edge size. */
  uint osz = 0; /* Outer edge size. */
  uint gsz = 0; /* Gradient fill area size. */
  /* Test the four corners */
  /* Upper left corner. */
  x = t - rw + 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if the inner mask is empty underneath or to the right. */
    if (!limask[x - rw] || !limask[x + 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty underneath or to the right. */
    if (!lomask[x - rw] || !lomask[x + 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Upper right corner. */
  x = t;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if the inner mask is empty underneath or to the left. */
    if (!limask[x - rw] || !limask[x - 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty above or to the left. */
    if (!lomask[x - rw] || !lomask[x - 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Lower left corner. */
  x = 0;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if inner mask is empty above or to the right. */
    if (!limask[x + rw] || !limask[x + 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty underneath or to the right. */
    if (!lomask[x + rw] || !lomask[x + 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Lower right corner. */
  x = rw - 1;
  /* Test if inner mask is filled. */
  if (limask[x]) {
    /* Test if inner mask is empty above or to the left. */
    if (!limask[x + rw] || !limask[x - 1]) {
      isz++;       /* Increment inner edge size. */
      lres[x] = 4; /* Flag pixel as inner edge. */
    }
    else {
      res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
    }
  } /* Inner mask was empty, test if outer mask is filled. */
  else if (lomask[x]) {
    /* Test if outer mask is empty underneath or to the left. */
    if (!lomask[x + rw] || !lomask[x - 1]) {
      osz++;       /* Increment outer edge size. */
      lres[x] = 3; /* Flag pixel as outer edge. */
    }
    else {
      gsz++;       /* Increment the gradient pixel count. */
      lres[x] = 2; /* Flag pixel as gradient. */
    }
  }
  /* Test the TOP row of pixels in buffer, except corners */
  for (x = t - 1; x >= (t - rw) + 2; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty to the left or to the right. */
      if (!limask[x - 1] || !limask[x + 1]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty to the left or to the right. */
      if (!lomask[x - 1] || !lomask[x + 1]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }

  /* Test the BOTTOM row of pixels in buffer, except corners */
  for (x = rw - 2; x; x--) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty to the left or to the right. */
      if (!limask[x - 1] || !limask[x + 1]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty to the left or to the right. */
      if (!lomask[x - 1] || !lomask[x + 1]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }
  /* Test the LEFT edge of pixels in buffer, except corners */
  for (x = t - (rw << 1) + 1; x >= rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty underneath or above. */
      if (!limask[x - rw] || !limask[x + rw]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty underneath or above. */
      if (!lomask[x - rw] || !lomask[x + rw]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }

  /* Test the RIGHT edge of pixels in buffer, except corners */
  for (x = t - rw; x > rw; x -= rw) {
    /* Test if inner mask is filled. */
    if (limask[x]) {
      /* Test if inner mask is empty underneath or above. */
      if (!limask[x - rw] || !limask[x + rw]) {
        isz++;       /* Increment inner edge size. */
        lres[x] = 4; /* Flag pixel as inner edge. */
      }
      else {
        res[x] = 1.0f; /* Pixel is just part of inner mask, and it's not an edge. */
      }
    } /* Inner mask was empty, test if outer mask is filled. */
    else if (lomask[x]) {
      /* Test if outer mask is empty underneath or above. */
      if (!lomask[x - rw] || !lomask[x + rw]) {
        osz++;       /* Increment outer edge size. */
        lres[x] = 3; /* Flag pixel as outer edge. */
      }
      else {
        gsz++;       /* Increment the gradient pixel count. */
        lres[x] = 2; /* Flag pixel as gradient. */
      }
    }
  }

  rsize[0] = isz; /* Fill in our return sizes for edges + fill. */
  rsize[1] = osz;
  rsize[2] = gsz;
}

static void do_allEdgeDetection(uint t,
                                uint rw,
                                const uint *limask,
                                const uint *lomask,
                                uint *lres,
                                float *res,
                                uint *rsize,
                                uint in_isz,
                                uint in_osz,
                                uint in_gsz)
{
  int x;           /* Pixel loop counter. */
  int a;           /* Pixel loop counter. */
  int dx;          /* Delta x. */
  int pix_prevRow; /* Pixel one row behind the one we are testing in a loop. */
  int pix_nextRow; /* Pixel one row in front of the one we are testing in a loop. */
  int pix_prevCol; /* Pixel one column behind the one we are testing in a loop. */
  int pix_nextCol; /* Pixel one column in front of the one we are testing in a loop. */

  /* Test all rows between the FIRST and LAST rows, excluding left and right edges */
  for (x = (t - rw) + 1, dx = x - (rw - 2); dx > rw; x -= rw, dx -= rw) {
    a = x - 2;
    pix_prevRow = a + rw;
    pix_nextRow = a - rw;
    pix_prevCol = a + 1;
    pix_nextCol = a - 1;
    while (a > dx - 2) {
      if (!limask[a]) {  /* If the inner mask is empty. */
        if (lomask[a]) { /* If the outer mask is full. */
          /*
           * Next we test all 4 directions around the current pixel: next/previous/up/down
           * The test ensures that the outer mask is empty and that the inner mask
           * is also empty. If both conditions are true for any one of the 4 adjacent pixels
           * then the current pixel is counted as being a true outer edge pixel.
           */
          if ((!lomask[pix_nextCol] && !limask[pix_nextCol]) ||
              (!lomask[pix_prevCol] && !limask[pix_prevCol]) ||
              (!lomask[pix_nextRow] && !limask[pix_nextRow]) ||
              (!lomask[pix_prevRow] && !limask[pix_prevRow]))
          {
            in_osz++;    /* Increment the outer boundary pixel count. */
            lres[a] = 3; /* Flag pixel as part of outer edge. */
          }
          else {         /* It's not a boundary pixel, but it is a gradient pixel. */
            in_gsz++;    /* Increment the gradient pixel count. */
            lres[a] = 2; /* Flag pixel as gradient. */
          }
        }
      }
      else {
        if (!limask[pix_nextCol] || !limask[pix_prevCol] || !limask[pix_nextRow] ||
            !limask[pix_prevRow]) {
          in_isz++;    /* Increment the inner boundary pixel count. */
          lres[a] = 4; /* Flag pixel as part of inner edge. */
        }
        else {
          res[a] = 1.0f; /* Pixel is part of inner mask, but not at an edge. */
        }
      }
      a--;
      pix_prevRow--;
      pix_nextRow--;
      pix_prevCol--;
      pix_nextCol--;
    }
  }

  rsize[0] = in_isz; /* Fill in our return sizes for edges + fill. */
  rsize[1] = in_osz;
  rsize[2] = in_gsz;
}

static void do_adjacentEdgeDetection(uint t,
                                     uint rw,
                                     const uint *limask,
                                     const uint *lomask,
                                     uint *lres,
                                     float *res,
                                     uint *rsize,
                                     uint in_isz,
                                     uint in_osz,
                                     uint in_gsz)
{
  int x;           /* Pixel loop counter. */
  int a;           /* Pixel loop counter. */
  int dx;          /* Delta x. */
  int pix_prevRow; /* Pixel one row behind the one we are testing in a loop. */
  int pix_nextRow; /* Pixel one row in front of the one we are testing in a loop. */
  int pix_prevCol; /* Pixel one column behind the one we are testing in a loop. */
  int pix_nextCol; /* Pixel one column in front of the one we are testing in a loop. */
  /* Test all rows between the FIRST and LAST rows, excluding left and right edges */
  for (x = (t - rw) + 1, dx = x - (rw - 2); dx > rw; x -= rw, dx -= rw) {
    a = x - 2;
    pix_prevRow = a + rw;
    pix_nextRow = a - rw;
    pix_prevCol = a + 1;
    pix_nextCol = a - 1;
    while (a > dx - 2) {
      if (!limask[a]) {  /* If the inner mask is empty. */
        if (lomask[a]) { /* If the outer mask is full. */
          /*
           * Next we test all 4 directions around the current pixel: next/previous/up/down
           * The test ensures that the outer mask is empty and that the inner mask
           * is also empty. If both conditions are true for any one of the 4 adjacent pixels
           * then the current pixel is counted as being a true outer edge pixel.
           */
          if ((!lomask[pix_nextCol] && !limask[pix_nextCol]) ||
              (!lomask[pix_prevCol] && !limask[pix_prevCol]) ||
              (!lomask[pix_nextRow] && !limask[pix_nextRow]) ||
              (!lomask[pix_prevRow] && !limask[pix_prevRow]))
          {
            in_osz++;    /* Increment the outer boundary pixel count. */
            lres[a] = 3; /* Flag pixel as part of outer edge. */
          }
          else {         /* It's not a boundary pixel, but it is a gradient pixel. */
            in_gsz++;    /* Increment the gradient pixel count. */
            lres[a] = 2; /* Flag pixel as gradient. */
          }
        }
      }
      else {
        if ((!limask[pix_nextCol] && lomask[pix_nextCol]) ||
            (!limask[pix_prevCol] && lomask[pix_prevCol]) ||
            (!limask[pix_nextRow] && lomask[pix_nextRow]) ||
            (!limask[pix_prevRow] && lomask[pix_prevRow]))
        {
          in_isz++;    /* Increment the inner boundary pixel count. */
          lres[a] = 4; /* Flag pixel as part of inner edge. */
        }
        else {
          res[a] = 1.0f; /* Pixel is part of inner mask, but not at an edge. */
        }
      }
      a--;
      pix_prevRow--; /* Advance all four "surrounding" pixel pointers. */
      pix_nextRow--;
      pix_prevCol--;
      pix_nextCol--;
    }
  }

  rsize[0] = in_isz; /* Fill in our return sizes for edges + fill. */
  rsize[1] = in_osz;
  rsize[2] = in_gsz;
}

static void do_createEdgeLocationBuffer(uint t,
                                        uint rw,
                                        const uint *lres,
                                        float *res,
                                        ushort *gbuf,
                                        uint *inner_edge_offset,
                                        uint *outer_edge_offset,
                                        uint isz,
                                        uint gsz)
{
  int x;     /* Pixel loop counter. */
  int a;     /* Temporary pixel index buffer loop counter. */
  uint ud;   /* Unscaled edge distance. */
  uint dmin; /* Minimum edge distance. */

  uint rsl; /* Long used for finding fast `1.0/sqrt`. */
  uint gradient_fill_offset;

  /* For looping inner edge pixel indexes, represents current position from offset. */
  uint inner_accum = 0;
  /* For looping outer edge pixel indexes, represents current position from offset. */
  uint outer_accum = 0;
  /* For looping gradient pixel indexes, represents current position from offset. */
  uint gradient_accum = 0;

  /* Disable clang-format to prevent line-wrapping. */
  /* clang-format off */
  /*
   * Here we compute the size of buffer needed to hold (row,col) coordinates
   * for each pixel previously determined to be either gradient, inner edge,
   * or outer edge.
   *
   * Allocation is done by requesting 4 bytes "sizeof(int)" per pixel, even
   * though gbuf[] is declared as `(ushort *)` (2 bytes) because we don't
   * store the pixel indexes, we only store x,y location of pixel in buffer.
   *
   * This does make the assumption that x and y can fit in 16 unsigned bits
   * so if Blender starts doing renders greater than 65536 in either direction
   * this will need to allocate gbuf[] as uint *and allocate 8 bytes
   * per flagged pixel.
   *
   * In general, the buffer on-screen:
   *
   * Example:  9 by 9 pixel block
   *
   * `.` = Pixel non-white in both outer and inner mask.
   * `o` = Pixel white in outer, but not inner mask, adjacent to "." pixel.
   * `g` = Pixel white in outer, but not inner mask, not adjacent to "." pixel.
   * `i` = Pixel white in inner mask, adjacent to "g" or "." pixel.
   * `F` = Pixel white in inner mask, only adjacent to other pixels white in the inner mask.
   *
   *
   * .........   <----- pixel #80
   * ..oooo...
   * .oggggo..
   * .oggiggo.
   * .ogi_figo.
   * .oggiggo.
   * .oggggo..
   * ..oooo...
   * pixel #00 -----> .........
   *
   * gsz = 18   (18 "g" pixels above)
   * isz = 4    (4 "i" pixels above)
   * osz = 18   (18 "o" pixels above)
   *
   *
   * The memory in gbuf[] after filling will look like this:
   *
   * gradient_fill_offset (0 pixels)                   inner_edge_offset (18 pixels)    outer_edge_offset (22 pixels)
   * /                                               /                              /
   * /                                               /                              /
   * |X   Y   X   Y   X   Y   X   Y   >     <X   Y   X   Y   >     <X   Y   X   Y   X   Y   >     <X   Y   X   Y   | <- (x,y)
   * +-------------------------------->     <---------------->     <------------------------>     <----------------+
   * |0   2   4   6   8   10  12  14  > ... <68  70  72  74  > ... <80  82  84  86  88  90  > ... <152 154 156 158 | <- bytes
   * +-------------------------------->     <---------------->     <------------------------>     <----------------+
   * |g0  g0  g1  g1  g2  g2  g3  g3  >     <g17 g17 i0  i0  >     <i2  i2  i3  i3  o0  o0  >     <o16 o16 o17 o17 | <- pixel
   *       /                              /                              /
   *      /                              /                              /
   *        /                              /                              /
   * +---------- gradient_accum (18) ---------+      +--- inner_accum (22) ---+      +--- outer_accum (40) ---+
   *
   *
   * Ultimately we do need the pixel's memory buffer index to set the output
   * pixel color, but it's faster to reconstruct the memory buffer location
   * each iteration of the final gradient calculation than it is to deconstruct
   * a memory location into x,y pairs each round.
   */
  /* clang-format on */

  gradient_fill_offset = 0; /* Since there are likely "more" of these, put it first. :). */
  *inner_edge_offset = gradient_fill_offset + gsz; /* Set start of inner edge indexes. */
  *outer_edge_offset = (*inner_edge_offset) + isz; /* Set start of outer edge indexes. */
  /* Set the accumulators to correct positions */  /* Set up some accumulator variables for loops.
                                                    */
  gradient_accum = gradient_fill_offset; /* Each accumulator variable starts at its respective. */
  inner_accum = *inner_edge_offset;      /* Section's offset so when we start filling, each. */
  outer_accum = *outer_edge_offset;      /* Section fills up its allocated space in gbuf. */
  /* Uses `dmin=row`, `rsl=col`. */
  for (x = 0, dmin = 0; x < t; x += rw, dmin++) {
    for (rsl = 0; rsl < rw; rsl++) {
      a = x + rsl;
      if (lres[a] == 2) {         /* It is a gradient pixel flagged by 2. */
        ud = gradient_accum << 1; /* Double the index to reach correct ushort location. */
        gbuf[ud] = dmin;          /* Insert pixel's row into gradient pixel location buffer. */
        gbuf[ud + 1] = rsl;       /* Insert pixel's column into gradient pixel location buffer. */
        gradient_accum++;         /* Increment gradient index buffer pointer. */
      }
      else if (lres[a] == 3) { /* It is an outer edge pixel flagged by 3. */
        ud = outer_accum << 1; /* Double the index to reach correct ushort location. */
        gbuf[ud] = dmin;       /* Insert pixel's row into outer edge pixel location buffer. */
        gbuf[ud + 1] = rsl;    /* Insert pixel's column into outer edge pixel location buffer. */
        outer_accum++;         /* Increment outer edge index buffer pointer. */
        res[a] = 0.0f;         /* Set output pixel intensity now since it won't change later. */
      }
      else if (lres[a] == 4) { /* It is an inner edge pixel flagged by 4. */
        ud = inner_accum << 1; /* Double int index to reach correct ushort location. */
        gbuf[ud] = dmin;       /* Insert pixel's row into inner edge pixel location buffer. */
        gbuf[ud + 1] = rsl;    /* Insert pixel's column into inner edge pixel location buffer. */
        inner_accum++;         /* Increment inner edge index buffer pointer. */
        res[a] = 1.0f;         /* Set output pixel intensity now since it won't change later. */
      }
    }
  }
}

static void do_fillGradientBuffer(uint rw,
                                  float *res,
                                  const ushort *gbuf,
                                  uint isz,
                                  uint osz,
                                  uint gsz,
                                  uint inner_edge_offset,
                                  uint outer_edge_offset)
{
  int x;                    /* Pixel loop counter. */
  int a;                    /* Temporary pixel index buffer loop counter. */
  int fsz;                  /* Size of the frame. */
  uint rsl;                 /* Long used for finding fast `1.0/sqrt`. */
  float rsf;                /* Float used for finding fast `1.0/sqrt`. */
  const float rsopf = 1.5f; /* Constant float used for finding fast `1.0/sqrt`. */

  uint gradient_fill_offset;
  uint t;
  uint ud;     /* Unscaled edge distance. */
  uint dmin;   /* Minimum edge distance. */
  float odist; /* Current outer edge distance. */
  float idist; /* Current inner edge distance. */
  int dx;      /* X-delta (used for distance proportion calculation) */
  int dy;      /* Y-delta (used for distance proportion calculation) */

  /*
   * The general algorithm used to color each gradient pixel is:
   *
   * 1.) Loop through all gradient pixels.
   * A.) For each gradient pixel:
   * a.) Loop through all outside edge pixels, looking for closest one
   * to the gradient pixel we are in.
   * b.) Loop through all inside edge pixels, looking for closest one
   * to the gradient pixel we are in.
   * c.) Find proportion of distance from gradient pixel to inside edge
   * pixel compared to sum of distance to inside edge and distance to
   * outside edge.
   *
   * In an image where:
   * `.` = Blank (black) pixels, not covered by inner mask or outer mask.
   * `+` = Desired gradient pixels, covered only by outer mask.
   * `*` = White full mask pixels, covered by at least inner mask.
   *
   * ...............................
   * ...............+++++++++++.....
   * ...+O++++++..++++++++++++++....
   * ..+++\++++++++++++++++++++.....
   * .+++++G+++++++++*******+++.....
   * .+++++|+++++++*********+++.....
   * .++***I****************+++.....
   * .++*******************+++......
   * .+++*****************+++.......
   * ..+++***************+++........
   * ....+++**********+++...........
   * ......++++++++++++.............
   * ...............................
   *
   * O = outside edge pixel
   * \
   *  G = gradient pixel
   *  |
   *  I = inside edge pixel
   *
   *   __
   *  *note that IO does not need to be a straight line, in fact
   *  many cases can arise where straight lines do not work
   *  correctly.
   *
   *     __       __     __
   * d.) Pixel color is assigned as |GO| / ( |GI| + |GO| )
   *
   * The implementation does not compute distance, but the reciprocal of the
   * distance. This is done to avoid having to compute a square root, as a
   * reciprocal square root can be computed faster. Therefore, the code computes
   * pixel color as |GI| / (|GI| + |GO|). Since these are reciprocals, GI serves the
   * purpose of GO for the proportion calculation.
   *
   * For the purposes of the minimum distance comparisons, we only check
   * the sums-of-squares against each other, since they are in the same
   * mathematical sort-order as if we did go ahead and take square roots
   *
   * Loop through all gradient pixels.
   */

  for (x = gsz - 1; x >= 0; x--) {
    gradient_fill_offset = x << 1;
    t = gbuf[gradient_fill_offset];       /* Calculate column of pixel indexed by `gbuf[x]`. */
    fsz = gbuf[gradient_fill_offset + 1]; /* Calculate row of pixel indexed by `gbuf[x]`. */
    dmin = 0xffffffff;                    /* Reset min distance to edge pixel. */
    /* Loop through all outer edge buffer pixels. */
    for (a = outer_edge_offset + osz - 1; a >= outer_edge_offset; a--) {
      ud = a << 1;
      dy = t - gbuf[ud];       /* Set dx to gradient pixel column - outer edge pixel row. */
      dx = fsz - gbuf[ud + 1]; /* Set dy to gradient pixel row - outer edge pixel column. */
      ud = dx * dx + dy * dy;  /* Compute sum of squares. */
      if (ud < dmin) {         /* If our new sum of squares is less than the current minimum. */
        dmin = ud;             /* Set a new minimum equal to the new lower value. */
      }
    }
    odist = float(dmin); /* Cast outer min to a float. */
    rsf = odist * 0.5f;
    rsl = *(uint *)&odist;         /* Use some peculiar properties of the way bits are stored. */
    rsl = 0x5f3759df - (rsl >> 1); /* In floats vs. uints to compute an approximate. */
    odist = *(float *)&rsl;        /* Reciprocal square root. */
    odist = odist * (rsopf - (rsf * odist *
                              odist)); /* -- This line can be iterated for more accuracy. -- */
    dmin = 0xffffffff;                 /* Reset min distance to edge pixel. */
    /* Loop through all inside edge pixels. */
    for (a = inner_edge_offset + isz - 1; a >= inner_edge_offset; a--) {
      ud = a << 1;
      dy = t - gbuf[ud];       /* Compute delta in Y from gradient pixel to inside edge pixel. */
      dx = fsz - gbuf[ud + 1]; /* Compute delta in X from gradient pixel to inside edge pixel. */
      ud = dx * dx + dy * dy;  /* Compute sum of squares. */
      /* If our new sum of squares is less than the current minimum we've found. */
      if (ud < dmin) {
        dmin = ud; /* Set a new minimum equal to the new lower value. */
      }
    }

    /* Cast inner min to a float. */
    idist = float(dmin);
    rsf = idist * 0.5f;
    rsl = *(uint *)&idist;

    /* See notes above. */
    rsl = 0x5f3759df - (rsl >> 1);
    idist = *(float *)&rsl;
    idist = idist * (rsopf - (rsf * idist * idist));

    /* NOTE: once again that since we are using reciprocals of distance values our
     * proportion is already the correct intensity, and does not need to be
     * subtracted from 1.0 like it would have if we used real distances. */

    /* Here we reconstruct the pixel's memory location in the CompBuf by
     * `Pixel Index = Pixel Column + ( Pixel Row * Row Width )`. */
    res[gbuf[gradient_fill_offset + 1] + (gbuf[gradient_fill_offset] * rw)] =
        (idist / (idist + odist)); /* Set intensity. */
  }
}

/* End of copy. */

void DoubleEdgeMaskOperation::do_double_edge_mask(float *imask, float *omask, float *res)
{
  uint *lres;   /* Pointer to output pixel buffer (for bit operations). */
  uint *limask; /* Pointer to inner mask (for bit operations). */
  uint *lomask; /* Pointer to outer mask (for bit operations). */

  int rw;  /* Pixel row width. */
  int t;   /* Total number of pixels in buffer - 1 (used for loop starts). */
  int fsz; /* Size of the frame. */

  uint isz = 0;               /* Size (in pixels) of inside edge pixel index buffer. */
  uint osz = 0;               /* Size (in pixels) of outside edge pixel index buffer. */
  uint gsz = 0;               /* Size (in pixels) of gradient pixel index buffer. */
  uint rsize[3];              /* Size storage to pass to helper functions. */
  uint inner_edge_offset = 0; /* Offset into final buffer where inner edge pixel indexes start. */
  uint outer_edge_offset = 0; /* Offset into final buffer where outer edge pixel indexes start. */

  ushort *gbuf; /* Gradient/inner/outer pixel location index buffer. */

  if (true) { /* If both input sockets have some data coming in... */

    rw = this->get_width();            /* Width of a row of pixels. */
    t = (rw * this->get_height()) - 1; /* Determine size of the frame. */

    /* Clear output buffer (not all pixels will be written later). */
    memset(res, 0, sizeof(float) * (t + 1));

    lres = (uint *)res;     /* Pointer to output buffer (for bit level ops).. */
    limask = (uint *)imask; /* Pointer to input mask (for bit level ops).. */
    lomask = (uint *)omask; /* Pointer to output mask (for bit level ops).. */

    /*
     * The whole buffer is broken up into 4 parts. The four CORNERS, the FIRST and LAST rows, the
     * LEFT and RIGHT edges (excluding the corner pixels), and all OTHER rows.
     * This allows for quick computation of outer edge pixels where
     * a screen edge pixel is marked to be gradient.
     *
     * The pixel type (gradient vs inner-edge vs outer-edge) tests change
     * depending on the user selected "Inner Edge Mode" and the user selected
     * "Buffer Edge Mode" on the node's GUI. There are 4 sets of basically the
     * same algorithm:
     *
     * 1.) Inner Edge -> Adjacent Only
     *   Buffer Edge -> Keep Inside
     *
     * 2.) Inner Edge -> Adjacent Only
     *   Buffer Edge -> Bleed Out
     *
     * 3.) Inner Edge -> All
     *   Buffer Edge -> Keep Inside
     *
     * 4.) Inner Edge -> All
     *   Buffer Edge -> Bleed Out
     *
     * Each version has slightly different criteria for detecting an edge pixel.
     */
    if (adjacent_only_) { /* If "adjacent only" inner edge mode is turned on. */
      if (keep_inside_) { /* If "keep inside" buffer edge mode is turned on. */
        do_adjacentKeepBorders(t, rw, limask, lomask, lres, res, rsize);
      }
      else { /* "bleed out" buffer edge mode is turned on. */
        do_adjacentBleedBorders(t, rw, limask, lomask, lres, res, rsize);
      }
      /* Set up inner edge, outer edge, and gradient buffer sizes after border pass. */
      isz = rsize[0];
      osz = rsize[1];
      gsz = rsize[2];
      /* Detect edges in all non-border pixels in the buffer. */
      do_adjacentEdgeDetection(t, rw, limask, lomask, lres, res, rsize, isz, osz, gsz);
    }
    else {                /* "all" inner edge mode is turned on. */
      if (keep_inside_) { /* If "keep inside" buffer edge mode is turned on. */
        do_allKeepBorders(t, rw, limask, lomask, lres, res, rsize);
      }
      else { /* "bleed out" buffer edge mode is turned on. */
        do_allBleedBorders(t, rw, limask, lomask, lres, res, rsize);
      }
      /* Set up inner edge, outer edge, and gradient buffer sizes after border pass. */
      isz = rsize[0];
      osz = rsize[1];
      gsz = rsize[2];
      /* Detect edges in all non-border pixels in the buffer. */
      do_allEdgeDetection(t, rw, limask, lomask, lres, res, rsize, isz, osz, gsz);
    }

    /* Set edge and gradient buffer sizes once again...
     * the sizes in rsize[] may have been modified
     * by the `do_*EdgeDetection()` function. */
    isz = rsize[0];
    osz = rsize[1];
    gsz = rsize[2];

    /* Calculate size of pixel index buffer needed. */
    fsz = gsz + isz + osz;
    /* Allocate edge/gradient pixel index buffer. */
    gbuf = (ushort *)MEM_callocN(sizeof(ushort) * fsz * 2, "DEM");

    do_createEdgeLocationBuffer(
        t, rw, lres, res, gbuf, &inner_edge_offset, &outer_edge_offset, isz, gsz);
    do_fillGradientBuffer(rw, res, gbuf, isz, osz, gsz, inner_edge_offset, outer_edge_offset);

    /* Free the gradient index buffer. */
    MEM_freeN(gbuf);
  }
}

DoubleEdgeMaskOperation::DoubleEdgeMaskOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  input_inner_mask_ = nullptr;
  input_outer_mask_ = nullptr;
  adjacent_only_ = false;
  keep_inside_ = false;
  flags_.complex = true;
  is_output_rendered_ = false;
}

bool DoubleEdgeMaskOperation::determine_depending_area_of_interest(
    rcti * /*input*/, ReadBufferOperation *read_operation, rcti *output)
{
  if (cached_instance_ == nullptr) {
    rcti new_input;
    new_input.xmax = this->get_width();
    new_input.xmin = 0;
    new_input.ymax = this->get_height();
    new_input.ymin = 0;
    return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
  }

  return false;
}

void DoubleEdgeMaskOperation::init_execution()
{
  input_inner_mask_ = this->get_input_socket_reader(0);
  input_outer_mask_ = this->get_input_socket_reader(1);
  init_mutex();
  cached_instance_ = nullptr;
}

void *DoubleEdgeMaskOperation::initialize_tile_data(rcti *rect)
{
  if (cached_instance_) {
    return cached_instance_;
  }

  lock_mutex();
  if (cached_instance_ == nullptr) {
    MemoryBuffer *inner_mask = (MemoryBuffer *)input_inner_mask_->initialize_tile_data(rect);
    MemoryBuffer *outer_mask = (MemoryBuffer *)input_outer_mask_->initialize_tile_data(rect);
    float *data = (float *)MEM_mallocN(sizeof(float) * this->get_width() * this->get_height(),
                                       __func__);
    float *imask = inner_mask->get_buffer();
    float *omask = outer_mask->get_buffer();
    do_double_edge_mask(imask, omask, data);
    cached_instance_ = data;
  }
  unlock_mutex();
  return cached_instance_;
}
void DoubleEdgeMaskOperation::execute_pixel(float output[4], int x, int y, void *data)
{
  float *buffer = (float *)data;
  int index = (y * this->get_width() + x);
  output[0] = buffer[index];
}

void DoubleEdgeMaskOperation::deinit_execution()
{
  input_inner_mask_ = nullptr;
  input_outer_mask_ = nullptr;
  deinit_mutex();
  if (cached_instance_) {
    MEM_freeN(cached_instance_);
    cached_instance_ = nullptr;
  }
}

void DoubleEdgeMaskOperation::get_area_of_interest(int /*input_idx*/,
                                                   const rcti & /*output_area*/,
                                                   rcti &r_input_area)
{
  r_input_area = this->get_canvas();
}

void DoubleEdgeMaskOperation::update_memory_buffer(MemoryBuffer *output,
                                                   const rcti & /*area*/,
                                                   Span<MemoryBuffer *> inputs)
{
  if (!is_output_rendered_) {
    /* Ensure full buffers to work with no strides. */
    MemoryBuffer *input_inner_mask = inputs[0];
    MemoryBuffer *inner_mask = input_inner_mask->is_a_single_elem() ? input_inner_mask->inflate() :
                                                                      input_inner_mask;
    MemoryBuffer *input_outer_mask = inputs[1];
    MemoryBuffer *outer_mask = input_outer_mask->is_a_single_elem() ? input_outer_mask->inflate() :
                                                                      input_outer_mask;

    BLI_assert(output->get_width() == this->get_width());
    BLI_assert(output->get_height() == this->get_height());
    /* TODO(manzanilla): Once tiled implementation is removed, use execution system to run
     * multi-threaded where possible. */
    do_double_edge_mask(inner_mask->get_buffer(), outer_mask->get_buffer(), output->get_buffer());
    is_output_rendered_ = true;

    if (inner_mask != input_inner_mask) {
      delete inner_mask;
    }
    if (outer_mask != input_outer_mask) {
      delete outer_mask;
    }
  }
}

}  // namespace blender::compositor
