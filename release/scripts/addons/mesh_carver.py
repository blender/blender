# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####


bl_info = {
    "name": "Carver MT",
    "category": "Object",
    "author": "Pixivore, Cedric LEPILLER, Ted Milker",
    "version": (1, 1, 7),
    "blender": (2, 77, 0),
    "wiki_url": "https://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Modeling/Carver",
    "description": "Multiple tools to carve or to create objects",
    }

import bpy
import bgl
import blf
import math
import mathutils
import sys
import random
import bmesh
import bpy_extras
from bpy.props import (
        BoolProperty,
        EnumProperty,
        IntProperty,
        StringProperty,
        )
from bpy_extras import view3d_utils
from bpy_extras.view3d_utils import (
        region_2d_to_vector_3d,
        region_2d_to_location_3d,
        )

Profils = [
    ("CTP_4882",
     mathutils.Vector((2.61824, -5.56469, 0)),
     [(-1.156501, 0.799282, 0.032334),
      (-0.967583, 0.838861, 0.032334),
      (-1.10386, 0.846403, 0.032334),
      (-1.034712, 0.86089, 0.032334),
      (-1.88472, -0.564419, 0.032334),
      (-1.924299, -0.375502, 0.032334),
      (-1.93184, -0.511778, 0.032334),
      (-1.946327, -0.44263, 0.032334),
      (-0.219065, -0.869195, 0.032334),
      (-0.149916, -0.854708, 0.032334),
      (-0.286193, -0.847167, 0.032334),
      (-0.097275, -0.807588, 0.032334),
      (0.692551, 0.434324, 0.032334),
      (0.678064, 0.503472, 0.032334),
      (0.670523, 0.367196, 0.032334),
      (0.630943, 0.556113, 0.032334),
      (-0.780424, -0.44263, 0.032334),
      (-0.765937, -0.511778, 0.032334),
      (-0.758396, -0.375502, 0.032334),
      (-0.718817, -0.564419, 0.032334),
      (-0.53496, 0.556113, 0.032334),
      (-0.49538, 0.367196, 0.032334),
      (-0.487839, 0.503472, 0.032334),
      (-0.473352, 0.434324, 0.032334),
      (-1.263178, -0.807588, 0.032334),
      (-1.452096, -0.847167, 0.032334),
      (-1.315819, -0.854708, 0.032334),
      (-1.384968, -0.869195, 0.032334),
      (0.131191, 0.86089, 0.032334),
      (0.062043, 0.846403, 0.032334),
      (0.19832, 0.838861, 0.032334),
      (0.009402, 0.799282, 0.032334),
      (0.946838, -0.869195, 0.032334),
      (1.015987, -0.854708, 0.032334),
      (0.87971, -0.847167, 0.032334),
      (1.068628, -0.807588, 0.032334),
      (1.858454, 0.434324, 0.032334),
      (1.843967, 0.503472, 0.032334),
      (1.836426, 0.367196, 0.032334),
      (1.796846, 0.556113, 0.032334),
      (0.385479, -0.44263, 0.032334),
      (0.399966, -0.511778, 0.032334),
      (0.407507, -0.375502, 0.032334),
      (0.447086, -0.564419, 0.032334),
      (1.297095, 0.86089, 0.032334),
      (1.227946, 0.846403, 0.032334),
      (1.364223, 0.838861, 0.032334),
      (1.175305, 0.799282, 0.032334),
      ],
     [[16, 17, 19], [5, 4, 24], [14, 12, 15], [14, 15, 31], [10, 8, 11], [15, 30, 31], [19, 10, 11],
      [11, 14, 31], [31, 18, 11], [8, 9, 11], [18, 16, 19], [12, 13, 15], [18, 19, 11], [28, 29, 31],
      [30, 28, 31], [24, 21, 0], [23, 22, 20], [20, 1, 0], [3, 2, 0], [0, 5, 24], [7, 6, 4], [4, 25, 24],
      [27, 26, 24], [21, 23, 20], [1, 3, 0], [5, 7, 4], [25, 27, 24], [21, 20, 0], [40, 41, 43], [38, 36, 39],
      [38, 39, 47], [34, 32, 35], [39, 46, 47], [43, 34, 35], [35, 38, 47], [47, 42, 35], [32, 33, 35],
      [42, 40, 43], [36, 37, 39], [42, 43, 35], [44, 45, 47], [46, 44, 47]]),
    ("CTP_8354",
     mathutils.Vector((-0.06267, -2.43829, -0.0)),
     [(-0.534254, -1.0, 0.032334),
      (-1.0, -0.534254, 0.032334),
      (-0.654798, -0.98413, 0.032334),
      (-0.767127, -0.937602, 0.032334),
      (-0.863586, -0.863586, 0.032334),
      (-0.937602, -0.767127, 0.032334),
      (-0.98413, -0.654798, 0.032334),
      (1.0, -0.534254, 0.032334),
      (0.534254, -1.0, 0.032334),
      (0.98413, -0.654798, 0.032334),
      (0.937602, -0.767127, 0.032334),
      (0.863586, -0.863586, 0.032334),
      (0.767127, -0.937602, 0.032334),
      (0.654798, -0.98413, 0.032334),
      (-1.0, 0.534254, 0.032334),
      (-0.534254, 1.0, 0.032334),
      (-0.98413, 0.654798, 0.032334),
      (-0.937602, 0.767127, 0.032334),
      (-0.863586, 0.863586, 0.032334),
      (-0.767127, 0.937602, 0.032334),
      (-0.654798, 0.98413, 0.032334),
      (0.534254, 1.0, 0.032334),
      (1.0, 0.534254, 0.032334),
      (0.654798, 0.98413, 0.032334),
      (0.767127, 0.937602, 0.032334),
      (0.863586, 0.863586, 0.032334),
      (0.937602, 0.767127, 0.032334),
      (0.98413, 0.654798, 0.032334),
      (-0.763998, 0.518786, 0.032334),
      (-0.763998, -0.518786, 0.032334),
      (-0.754202, -0.593189, 0.032334),
      (-0.731454, -0.648108, 0.032334),
      (-0.695267, -0.695267, 0.032334),
      (-0.648108, -0.731454, 0.032334),
      (-0.593189, -0.754202, 0.032334),
      (-0.518786, -0.763998, 0.032334),
      (0.518786, -0.763998, 0.032334),
      (0.593189, -0.754202, 0.032334),
      (0.648108, -0.731454, 0.032334),
      (0.695267, -0.695267, 0.032334),
      (0.731454, -0.648108, 0.032334),
      (0.754202, -0.593189, 0.032334),
      (0.763998, -0.518786, 0.032334),
      (0.763998, 0.518786, 0.032334),
      (0.754202, 0.593189, 0.032334),
      (0.731454, 0.648108, 0.032334),
      (0.695267, 0.695267, 0.032334),
      (0.648108, 0.731454, 0.032334),
      (0.593189, 0.754202, 0.032334),
      (0.518786, 0.763998, 0.032334),
      (-0.518786, 0.763998, 0.032334),
      (-0.593189, 0.754202, 0.032334),
      (-0.648108, 0.731454, 0.032334),
      (-0.695267, 0.695267, 0.032334),
      (-0.731454, 0.648108, 0.032334),
      (-0.754202, 0.593189, 0.032334),
      (0.518786, 0.518786, 0.032334),
      (-0.518786, 0.518786, 0.032334),
      (0.518786, -0.518786, 0.032334),
      (-0.518786, -0.518786, 0.032334),
      (-0.593189, 0.518786, 0.032334),
      (-0.593189, -0.518786, 0.032334),
      (0.518786, -0.593189, 0.032334),
      (-0.518786, -0.593189, 0.032334),
      (-0.593189, -0.593189, 0.032334),
      (0.593189, 0.518786, 0.032334),
      (0.593189, -0.518786, 0.032334),
      (0.593189, -0.593189, 0.032334),
      (-0.593189, 0.593189, 0.032334),
      (-0.518786, 0.593189, 0.032334),
      (0.518786, 0.593189, 0.032334),
      (0.593189, 0.593189, 0.032334),
      (-0.648108, 0.593189, 0.032334),
      (-0.648108, 0.518786, 0.032334),
      (-0.648108, -0.518786, 0.032334),
      (-0.648108, -0.593189, 0.032334),
      (-0.695267, 0.593189, 0.032334),
      (-0.695267, 0.518786, 0.032334),
      (-0.695267, -0.518786, 0.032334),
      (-0.695267, -0.593189, 0.032334),
      (0.648108, 0.593189, 0.032334),
      (0.648108, 0.518786, 0.032334),
      (0.648108, -0.518786, 0.032334),
      (0.648108, -0.593189, 0.032334),
      (0.695267, 0.593189, 0.032334),
      (0.695267, 0.518786, 0.032334),
      (0.695267, -0.518786, 0.032334),
      (0.695267, -0.593189, 0.032334),
      ],
     [[87, 39, 40, 41], [29, 28, 14, 1], [30, 29, 1, 6], [31, 30, 6, 5], [32, 31, 5, 4], [33, 32, 4, 3],
      [34, 33, 3, 2], [35, 34, 2, 0], [36, 35, 0, 8], [37, 36, 8, 13], [38, 37, 13, 12], [39, 38, 12, 11],
      [40, 39, 11, 10], [41, 40, 10, 9], [42, 41, 9, 7], [43, 42, 7, 22], [44, 43, 22, 27], [45, 44, 27, 26],
      [46, 45, 26, 25], [47, 46, 25, 24], [48, 47, 24, 23], [49, 48, 23, 21], [50, 49, 21, 15], [51, 50, 15, 20],
      [52, 51, 20, 19], [53, 52, 19, 18], [54, 53, 18, 17], [55, 54, 17, 16], [28, 55, 16, 14], [68, 69, 50, 51],
      [63, 35, 36, 62], [69, 57, 56, 70], [84, 85, 43, 44], [64, 34, 35, 63], [57, 59, 58, 56], [85, 86, 42, 43],
      [60, 61, 59, 57], [73, 74, 61, 60], [72, 68, 51, 52], [75, 33, 34, 64], [61, 64, 63, 59], [59, 63, 62, 58],
      [86, 87, 41, 42], [74, 75, 64, 61], [58, 62, 67, 66], [56, 58, 66, 65], [70, 56, 65, 71], [62, 36, 37, 67],
      [49, 70, 71, 48], [50, 69, 70, 49], [60, 57, 69, 68], [73, 60, 68, 72], [46, 84, 44, 45], [78, 79, 75, 74],
      [77, 78, 74, 73], [77, 73, 72, 76], [76, 72, 52, 53], [79, 32, 33, 75], [29, 30, 79, 78], [28, 29, 78, 77],
      [28, 77, 76, 55], [55, 76, 53, 54], [30, 31, 32, 79], [66, 67, 83, 82], [65, 66, 82, 81], [71, 65, 81, 80],
      [48, 71, 80, 47], [67, 37, 38, 83], [82, 83, 87, 86], [81, 82, 86, 85], [80, 81, 85, 84], [47, 80, 84, 46],
      [83, 38, 39, 87]]),
    ("CTP_5585",
     mathutils.Vector((5.0114, -2.4281, 0.0)),
     [(-0.490711, -1.0, 0.032334),
      (-1.0, -0.490711, 0.032334),
      (1.0, -0.490711, 0.032334),
      (0.490711, -1.0, 0.032334),
      (-1.0, 0.490711, 0.032334),
      (-0.490711, 1.0, 0.032334),
      (0.490711, 1.0, 0.032334),
      (1.0, 0.490711, 0.032334),
      (-0.51852, 0.291276, 0.032334),
      (-0.51852, -0.291276, 0.032334),
      (-0.291276, -0.51852, 0.032334),
      (0.291276, -0.51852, 0.032334),
      (0.51852, -0.291276, 0.032334),
      (0.51852, 0.291276, 0.032334),
      (0.291276, 0.51852, 0.032334),
      (-0.291276, 0.51852, 0.032334),
      ],
     [[11, 12, 13, 14], [9, 8, 4, 1], [10, 9, 1, 0], [11, 10, 0, 3], [12, 11, 3, 2], [13, 12, 2, 7],
      [14, 13, 7, 6], [15, 14, 6, 5], [8, 15, 5, 4], [9, 10, 15, 8], [10, 11, 14, 15]]),
    ("CTP_6960",
     mathutils.Vector((-0.11417, 2.48371, -0.0)),
     [(0.0, 1.0, 0.016827),
      (-0.382683, 0.92388, 0.016827),
      (-0.707107, 0.707107, 0.016827),
      (-0.92388, 0.382683, 0.016827),
      (-1.0, -0.0, 0.016827),
      (-0.92388, -0.382684, 0.016827),
      (-0.707107, -0.707107, 0.016827),
      (-0.382683, -0.92388, 0.016827),
      (-0.0, -1.0, 0.016827),
      (0.382683, -0.92388, 0.016827),
      (0.707107, -0.707107, 0.016827),
      (0.92388, -0.382684, 0.016827),
      (1.0, 0.0, 0.016827),
      (0.923879, 0.382684, 0.016827),
      (0.707107, 0.707107, 0.016827),
      (0.382683, 0.92388, 0.016827),
      (-0.0, 0.546859, 0.016827),
      (-0.209274, 0.505231, 0.016827),
      (-0.386687, 0.386687, 0.016827),
      (-0.505231, 0.209274, 0.016827),
      (-0.546859, -0.0, 0.016827),
      (-0.505231, -0.209274, 0.016827),
      (-0.386687, -0.386687, 0.016827),
      (-0.209274, -0.505231, 0.016827),
      (-0.0, -0.546859, 0.016827),
      (0.209274, -0.505231, 0.016827),
      (0.386687, -0.386688, 0.016827),
      (0.505231, -0.209274, 0.016827),
      (0.546858, 0.0, 0.016827),
      (0.505231, 0.209274, 0.016827),
      (0.386687, 0.386688, 0.016827),
      (0.209273, 0.505232, 0.016827),
      ],
     [[3, 19, 18, 2], [11, 27, 26, 10], [4, 20, 19, 3], [12, 28, 27, 11], [5, 21, 20, 4], [13, 29, 28, 12],
      [6, 22, 21, 5], [14, 30, 29, 13], [7, 23, 22, 6], [15, 31, 30, 14], [8, 24, 23, 7], [1, 17, 16, 0],
      [0, 16, 31, 15], [9, 25, 24, 8], [2, 18, 17, 1], [10, 26, 25, 9]]),
    ("CTP_5359",
     mathutils.Vector((5.50446, 2.41669, -0.0)),
     [(0.0, 0.714247, 0.023261),
      (-0.382683, 0.659879, 0.023261),
      (-0.707107, 0.505049, 0.023261),
      (-0.92388, 0.273331, 0.023261),
      (-1.0, -0.0, 0.023261),
      (-0.92388, -0.273331, 0.023261),
      (-0.707107, -0.505049, 0.023261),
      (-0.382683, -0.659879, 0.023261),
      (-0.0, -0.714247, 0.023261),
      (0.382683, -0.659879, 0.023261),
      (0.707107, -0.505049, 0.023261),
      (0.92388, -0.273331, 0.023261),
      (1.0, 0.0, 0.023261),
      (0.923879, 0.273331, 0.023261),
      (0.707107, 0.505049, 0.023261),
      (0.382683, 0.659879, 0.023261),
      (-0.0, 0.303676, 0.023261),
      (-0.162705, 0.28056, 0.023261),
      (-0.30064, 0.214731, 0.023261),
      (-0.392805, 0.116212, 0.023261),
      (-0.425169, -0.0, 0.023261),
      (-0.392805, -0.116212, 0.023261),
      (-0.30064, -0.214731, 0.023261),
      (-0.162705, -0.28056, 0.023261),
      (-0.0, -0.303676, 0.023261),
      (0.162705, -0.28056, 0.023261),
      (0.30064, -0.214731, 0.023261),
      (0.392805, -0.116212, 0.023261),
      (0.425169, 0.0, 0.023261),
      (0.392805, 0.116212, 0.023261),
      (0.30064, 0.214731, 0.023261),
      (0.162705, 0.28056, 0.023261),
      ],
     [[3, 19, 18, 2], [11, 27, 26, 10], [4, 20, 19, 3], [12, 28, 27, 11], [5, 21, 20, 4], [13, 29, 28, 12],
      [6, 22, 21, 5], [14, 30, 29, 13], [7, 23, 22, 6], [15, 31, 30, 14], [8, 24, 23, 7], [1, 17, 16, 0],
      [0, 16, 31, 15], [9, 25, 24, 8], [2, 18, 17, 1], [10, 26, 25, 9]]),
    ("CTP_5424",
     mathutils.Vector((2.61824, 2.34147, 0.0)),
     [(1.0, -1.0, 0.032334),
      (-1.0, 1.0, 0.032334),
      (1.0, 1.0, 0.032334),
      (0.783867, -0.259989, 0.032334),
      (-0.393641, 0.857073, 0.032334),
      (0.73142, -0.116299, 0.032334),
      (0.657754, 0.02916, 0.032334),
      (0.564682, 0.172804, 0.032334),
      (0.454497, 0.311098, 0.032334),
      (0.329912, 0.440635, 0.032334),
      (0.193995, 0.558227, 0.032334),
      (0.050092, 0.660978, 0.032334),
      (-0.098254, 0.746358, 0.032334),
      (-0.247389, 0.812263, 0.032334),
      ],
     [[3, 0, 2], [10, 9, 2], [2, 1, 4], [2, 4, 13], [5, 3, 2], [6, 5, 2], [2, 13, 12], [2, 12, 11], [7, 6, 2],
      [8, 7, 2], [2, 11, 10], [9, 8, 2]]),
    ("CTP_3774",
     mathutils.Vector((2.61824, -2.52425, 0.0)),
     [(1.0, 0.0, 0.020045),
      (-1.0, 0.0, 0.020045),
      (0.31903, -0.664947, 0.020045),
      (-0.31903, -0.664947, 0.020045),
      (-0.31903, 1.0, 0.020045),
      (0.31903, 1.0, 0.020045),
      (0.31903, 0.0, 0.020045),
      (-0.31903, 0.0, 0.020045),
      (-1.0, 0.614333, 0.020045),
      (-0.614333, 1.0, 0.020045),
      (-0.970643, 0.761921, 0.020045),
      (-0.887041, 0.887041, 0.020045),
      (-0.761921, 0.970643, 0.020045),
      (0.614333, 1.0, 0.020045),
      (1.0, 0.614333, 0.020045),
      (0.761921, 0.970643, 0.020045),
      (0.887041, 0.887041, 0.020045),
      (0.970643, 0.761921, 0.020045),
      (-0.31903, 0.614333, 0.020045),
      (0.31903, 0.614333, 0.020045),
      (0.31903, 0.761921, 0.020045),
      (-0.31903, 0.761921, 0.020045),
      (0.31903, 0.887041, 0.020045),
      (-0.31903, 0.887041, 0.020045),
      (0.614333, 0.614333, 0.020045),
      (0.614333, 0.0, 0.020045),
      (0.614333, 0.761921, 0.020045),
      (0.614333, 0.887041, 0.020045),
      (-0.614333, 0.761921, 0.020045),
      (-0.614333, 0.0, 0.020045),
      (-0.614333, 0.887041, 0.020045),
      (-0.614333, 0.614333, 0.020045),
      ],
     [[6, 25, 24, 19], [6, 19, 18, 7], [2, 6, 7, 3], [1, 29, 31, 8], [8, 31, 28, 10], [19, 24, 26, 20],
      [18, 19, 20, 21], [21, 20, 22, 23], [10, 28, 30, 11], [20, 26, 27, 22], [22, 27, 13, 5], [23, 22, 5, 4],
      [11, 30, 9, 12], [17, 16, 27, 26], [14, 17, 26, 24], [24, 25, 0, 14], [15, 13, 27, 16], [9, 30, 23, 4],
      [31, 29, 7, 18], [28, 31, 18, 21], [30, 28, 21, 23]]),
    ("CTP_4473",
     mathutils.Vector((7.31539, 0.0, 0.0)),
     [(0.24549, -1.0, 0.022454),
      (-0.24549, -1.0, 0.022454),
      (-0.24549, 1.0, 0.022454),
      (0.24549, 1.0, 0.022454),
      (1.0, 0.267452, 0.022454),
      (1.0, -0.267452, 0.022454),
      (-1.0, -0.267452, 0.022454),
      (-1.0, 0.267452, 0.022454),
      (0.24549, 0.267452, 0.022454),
      (0.24549, -0.267452, 0.022454),
      (-0.24549, 0.267452, 0.022454),
      (-0.24549, -0.267452, 0.022454),
      ],
     [[8, 3, 2, 10], [0, 9, 11, 1], [4, 8, 9, 5], [8, 10, 11, 9], [10, 7, 6, 11]]),
    ("CTP_4003",
     mathutils.Vector((4.91276, 0.0, 0.0)),
     [(-1.0, -1.0, 0.026945),
      (1.0, -1.0, 0.026945),
      (-1.0, 1.0, 0.026945),
      (-0.026763, -1.0, 0.026945),
      (-0.026763, 1.0, 0.026945),
      (1.0, -0.026763, 0.026945),
      (0.238983, 0.965014, 0.026945),
      (0.486619, 0.86244, 0.026945),
      (0.699268, 0.699268, 0.026945),
      (0.86244, 0.486619, 0.026945),
      (0.965014, 0.238983, 0.026945),
      (0.238983, -1.0, 0.026945),
      (0.486619, -1.0, 0.026945),
      (0.699268, -1.0, 0.026945),
      (0.86244, -1.0, 0.026945),
      (-0.026763, 0.479676, 0.026945),
      (0.486619, 0.479676, 0.026945),
      (0.699268, 0.479676, 0.026945),
      (0.238983, 0.479676, 0.026945),
      (0.865316, 0.479676, 0.026945),
      (-1.0, 0.479676, 0.026945),
      (0.86244, 0.479676, 0.026945),
      (-0.026763, 0.238983, 0.026945),
      (0.486619, 0.238983, 0.026945),
      (0.699268, 0.238983, 0.026945),
      (0.238983, 0.238983, 0.026945),
      (-1.0, 0.238983, 0.026945),
      (0.86244, 0.238983, 0.026945),
      (-0.026763, -0.026763, 0.026945),
      (0.486619, -0.026763, 0.026945),
      (0.699268, -0.026763, 0.026945),
      (0.238983, -0.026763, 0.026945),
      (-1.0, -0.026763, 0.026945),
      (0.86244, -0.026763, 0.026945),
      ],
     [[0, 3, 28, 32], [4, 15, 18, 6], [6, 18, 16, 7], [7, 16, 17, 8], [8, 17, 21, 9], [9, 21, 19], [18, 15, 22, 25],
      [19, 21, 27, 10], [16, 18, 25, 23], [17, 16, 23, 24], [20, 15, 4, 2], [21, 17, 24, 27], [27, 24, 30, 33],
      [23, 25, 31, 29], [24, 23, 29, 30], [25, 22, 28, 31], [26, 22, 15, 20], [10, 27, 33, 5], [31, 28, 3, 11],
      [33, 30, 13, 14], [29, 31, 11, 12], [5, 33, 14, 1], [30, 29, 12, 13], [32, 28, 22, 26]]),
    ("CTP_3430",
     mathutils.Vector((2.61824, 0.0, 0.0)),
     [(-1.0, -1.0, 0.032334),
      (1.0, -1.0, 0.032334),
      (-1.0, 1.0, 0.032334),
      (1.0, 1.0, 0.032334),
      ],
     [[0, 1, 3, 2]]),
    ("CTP_7175",
     mathutils.Vector((0.0, 0.0, 0.0)),
     [(-1.0, -1.0, 0.032334),
      (1.0, -1.0, 0.032334),
      (-1.0, 1.0, 0.032334),
      (1.0, 1.0, 0.032334),
      (0.0, 0.0, 0.032334),
      (0.0, 0.0, 0.032334),
      (0.0, 0.0, 0.032334),
      (0.0, 0.0, 0.032334),
      (0.0, 0.0, 0.032334),
      (-0.636126, 0.636126, 0.032334),
      (-0.636126, -0.636126, 0.032334),
      (0.636126, -0.636126, 0.032334),
      (0.636126, 0.636126, 0.032334),
      ],
     [[10, 9, 2, 0], [11, 10, 0, 1], [12, 11, 1, 3], [9, 12, 3, 2]]),
]

# Cut Type
RECTANGLE = 0
LINE = 1
CIRCLE = 2

# Boolean operation
DIFFERENCE = 0
UNION = 1


class CarverPrefs(bpy.types.AddonPreferences):
    bl_idname = __name__

    Enable_Tab_01 = BoolProperty(
            name="Info",
            description="Some general information and settings about the add-on",
            default=False
            )
    Enable_Tab_02 = BoolProperty(
            name="Hotkeys",
            description="List of the shortcuts used during carving",
            default=False
            )
    bpy.types.Scene.Key_Create = StringProperty(
            name="Object creation",
            description="Object creation",
            maxlen=1,
            default="C"
            )
    bpy.types.Scene.Key_Update = StringProperty(
            name="Auto Bevel Update",
            description="Auto Bevel Update",
            maxlen=1,
            default="A",
            )
    bpy.types.Scene.Key_Bool = StringProperty(
            name="Boolean type",
            description="Boolean operation type",
            maxlen=1,
            default="T",
            )
    bpy.types.Scene.Key_Brush = StringProperty(
            name="Brush Mode",
            description="Brush Mode",
            maxlen=1,
            default="B",
            )
    bpy.types.Scene.Key_Help = StringProperty(
            name="Help display",
            description="Help display",
            maxlen=1,
            default="H",
            )
    bpy.types.Scene.Key_Instant = StringProperty(
            name="Instantiate",
            description="Instantiate object",
            maxlen=1,
            default="I",
            )
    bpy.types.Scene.Key_Close = StringProperty(
            name="Close polygonal shape",
            description="Close polygonal shape",
            maxlen=1,
            default="X",
            )
    bpy.types.Scene.Key_Apply = StringProperty(
            name="Apply operation",
            description="Apply operation",
            maxlen=1,
            default="Q",
            )
    bpy.types.Scene.Key_Scale = StringProperty(
            name="Scale object",
            description="Scale object",
            maxlen=1,
            default="S",
            )
    bpy.types.Scene.Key_Gapy = StringProperty(
            name="Gap rows",
            description="Scale gap between columns",
            maxlen=1,
            default="J",
            )
    bpy.types.Scene.Key_Gapx = StringProperty(
            name="Gap columns",
            description="Scale gap between columns",
            maxlen=1,
            default="U",
            )
    bpy.types.Scene.Key_Depth = StringProperty(
            name="Depth",
            description="Cursor depth or solidify pattern",
            maxlen=1,
            default="D",
            )
    bpy.types.Scene.Key_BrushDepth = StringProperty(
            name="Brush Depth",
            description="Brush depth",
            maxlen=1,
            default="C",
            )
    bpy.types.Scene.Key_Subadd = StringProperty(
            name="Add subdivision",
            description="Add subdivision",
            maxlen=1,
            default="X",
            )
    bpy.types.Scene.Key_Subrem = StringProperty(
            name="Remove subdivision",
            description="Remove subdivision",
            maxlen=1,
            default="W",
            )
    bpy.types.Scene.Key_Randrot = StringProperty(
            name="Random rotation",
            description="Random rotation",
            maxlen=1,
            default="R",
            )
    bpy.types.Scene.Key_Solver = StringProperty(
            name="Solver",
            description="Switch between Carve and BMesh Boolean solver\n"
                        "depending on a specific use case",
            maxlen=1,
            default="V",
            )
    bpy.types.Scene.ProfilePrefix = StringProperty(
            name="Profile prefix",
            description="Prefix to look for profiles with",
            default="Carver_Profile-"
            )
    bpy.types.Scene.CarverSolver = EnumProperty(
            name="Boolean Solver",
            description="Boolean solver to use by default\n",
            default="CARVE",
            items=(
                ('CARVE', 'Carve', "Carve solver, as the legacy one, can handle\n"
                                   "basic coplanar but can often fail with\n"
                                   "non-closed geometry"),
                ('BMESH', 'BMesh', "BMesh solver is faster, but cannot handle\n"
                                   "coplanar and self-intersecting geometry")
                )
            )

    def draw(self, context):
        scene = context.scene
        layout = self.layout

        layout.prop(self, "Enable_Tab_01", text="Info and Settings", icon="QUESTION")
        if self.Enable_Tab_01:
            layout.label(text="Carver Operator:", icon="LAYER_ACTIVE")
            layout.label(text="Select a Mesh Object and press [CTRL]+[SHIFT]+[X] to carve",
                         icon="LAYER_USED")
            layout.label(text="To finish carving press [ESC] or [RIGHT CLICK]",
                         icon="LAYER_USED")

            layout.prop(scene, "ProfilePrefix", text="Profile prefix")
            layout.prop(scene, "CarverSolver", text="Solver")

        layout.prop(self, "Enable_Tab_02", text="Keys", icon="KEYINGSET")
        if self.Enable_Tab_02:
            split = layout.split()
            col = split.column()
            col.label("Object Creation:")
            col.prop(scene, "Key_Create", text="")
            col.label("Auto bevel update:")
            col.prop(scene, "Key_Update", text="")
            col.label("Boolean operation type:")
            col.prop(scene, "Key_Bool", text="")
            col.label("Solver:")
            col.prop(scene, "Key_Solver", text="")

            col = split.column()
            col.label("Brush Mode:")
            col.prop(scene, "Key_Brush", text="")
            col.label("Help display:")
            col.prop(scene, "Key_Help", text="")
            col.label("Instantiate object:")
            col.prop(scene, "Key_Instant", text="")
            col.label("Brush Depth:")
            col.prop(scene, "Key_BrushDepth", text="")

            col = split.column()
            col.label("Close polygonal shape:")
            col.prop(scene, "Key_Close", text="")
            col.label("Apply operation:")
            col.prop(scene, "Key_Apply", text="")
            col.label("Scale object:")
            col.prop(scene, "Key_Scale", text="")

            col = split.column()
            col.label("Gap rows:")
            col.prop(scene, "Key_Gapy", text="")
            col.label("Gap columns:")
            col.prop(scene, "Key_Gapx", text="")
            col.label("Depth / Solidify:")
            col.prop(scene, "Key_Depth", text="")

            col = split.column()
            col.label("Subdiv add:")
            col.prop(scene, "Key_Subadd", text="")
            col.label("Subdiv Remove:")
            col.prop(scene, "Key_Subrem", text="")
            col.label("Random rotation:")
            col.prop(scene, "Key_Randrot", text="")


# Draw Text (Center position)
def DrawCenterText(text, xt, yt, Size, Color, self):
    font_id = 0
    # Offset Shadow
    Sshadow_x = 2
    Sshadow_y = -2

    blf.size(font_id, Size, 72)
    blf.position(font_id, xt + Sshadow_x - blf.dimensions(font_id, text)[0] / 2, yt + Sshadow_y, 0)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)

    blf.draw(font_id, text)
    blf.position(font_id, xt - blf.dimensions(font_id, text)[0] / 2, yt, 0)
    if Color is not None:
        mColor = mathutils.Color((Color[0], Color[1], Color[2]))
        bgl.glColor4f(mColor.r, mColor.g, mColor.b, 1.0)
    else:
        bgl.glColor4f(1.0, 1.0, 1.0, 1.0)
    blf.draw(font_id, text)


# Draw text (Left position)
def DrawLeftText(text, xt, yt, Size, Color, self):
    font_id = 0
    # Offset Shadow
    Sshadow_x = 2
    Sshadow_y = -2

    blf.size(font_id, Size, 72)
    blf.position(font_id, xt + Sshadow_x, yt + Sshadow_y, 0)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)
    blf.draw(font_id, text)
    blf.position(font_id, xt, yt, 0)
    if Color is not None:
        mColor = mathutils.Color((Color[0], Color[1], Color[2]))
        bgl.glColor4f(mColor.r, mColor.g, mColor.b, 1.0)
    else:
        bgl.glColor4f(1.0, 1.0, 1.0, 1.0)
    blf.draw(font_id, text)


# Draw text (Right position)
def DrawRightText(text, xt, yt, Size, Color, self):
    font_id = 0
    # Offset Shadow
    Sshadow_x = 2
    Sshadow_y = -2

    blf.size(font_id, Size, 72)
    blf.position(font_id, xt + Sshadow_x - blf.dimensions(font_id, text)[0], yt + Sshadow_y, 0)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)
    blf.draw(font_id, text)
    blf.position(font_id, xt - blf.dimensions(font_id, text)[0], yt, 0)
    if Color is not None:
        mColor = mathutils.Color((Color[0], Color[1], Color[2]))
        bgl.glColor4f(mColor.r, mColor.g, mColor.b, 1.0)
    else:
        bgl.glColor4f(1.0, 1.0, 1.0, 1.0)
    blf.draw(font_id, text)


# Opengl draws
def draw_callback_px(self, context):
    font_id = 0
    region = context.region

    # Width screen
    overlap = context.user_preferences.system.use_region_overlap
    t_panel_width = 0
    if overlap:
        for region in context.area.regions:
            if region.type == 'TOOLS':
                t_panel_width = region.width

    # Initial position
    xt = int(region.width / 2.0)
    yt = 130
    if region.width >= 850:
        xt = int(region.width / 2.0)
        yt = 150

    # Command Display
    if self.CreateMode and ((self.ObjectMode is False) and (self.ProfileMode is False)):
        BooleanMode = "Create"
    else:
        if self.ObjectMode or self.ProfileMode:
            if self.BoolOps == DIFFERENCE:
                BooleanType = "Difference) [T]"
            else:
                BooleanType = "Union) [T]"

            if self.ObjectMode:
                BooleanMode = "Object Brush (" + BooleanType
            else:
                BooleanMode = "Profil Brush (" + BooleanType
        else:
            if (self.shift is False) and (self.ForceRebool is False):
                BooleanMode = "Difference"
            else:
                BooleanMode = "Rebool"

    UIColor = (0.992, 0.5518, 0.0, 1.0)

    # Display boolean mode
    if region.width >= 850:
        DrawCenterText(BooleanMode, xt, yt, 40, UIColor, self)
    else:
        DrawCenterText(BooleanMode, xt, yt, 20, UIColor, self)

    # Separator (Line)
    LineWidth = 75
    if region.width >= 850:
        LineWidth = 140
    bgl.glLineWidth(1)
    bgl.glColor4f(1.0, 1.0, 1.0, 1.0)
    bgl.glBegin(bgl.GL_LINE_STRIP)
    bgl.glVertex2i(int(xt - LineWidth), yt - 8)
    bgl.glVertex2i(int(xt + LineWidth), yt - 8)
    bgl.glEnd()

    # Text position
    xt = xt - blf.dimensions(font_id, "Difference")[0] / 2 + 80

    # Primitives type
    PrimitiveType = "Rectangle "
    if self.CutMode == CIRCLE:
        PrimitiveType = "Circle "
    if self.CutMode == LINE:
        PrimitiveType = "Line "

    # Variables according to screen size
    IFontSize = 12
    yInterval = 20
    xCmd = 0
    yCmd = yt - 30
    if region.width >= 850:
        IFontSize = 18
        yInterval = 25
        xCmd = 100

    # Color
    Color0 = None
    Color1 = UIColor

    # Help Display
    if (self.ObjectMode is False) and (self.ProfileMode is False):
        TypeStr = "Cut Type [Space] : "
        if self.CreateMode:
            TypeStr = "Type [Space] : "
        blf.size(font_id, IFontSize, 72)
        OpsStr = TypeStr + PrimitiveType
        TotalWidth = blf.dimensions(font_id, OpsStr)[0]
        xLeft = region.width / 2 - TotalWidth / 2
        xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
        DrawLeftText(TypeStr, xLeft, yCmd, IFontSize, Color0, self)
        DrawLeftText(PrimitiveType, xLeftP, yCmd, IFontSize, Color1, self)

        # Depth Cursor
        TypeStr = "Cursor Depth [" + context.scene.Key_Depth + "] : "
        if self.snapCursor:
            BoolStr = "(ON)"
        else:
            BoolStr = "(OFF)"
        OpsStr = TypeStr + BoolStr
        TotalWidth = blf.dimensions(font_id, OpsStr)[0]
        xLeft = region.width / 2 - TotalWidth / 2
        xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
        DrawLeftText(TypeStr, xLeft, yCmd - yInterval, IFontSize, Color0, self)
        DrawLeftText(BoolStr, xLeftP, yCmd - yInterval, IFontSize, Color1, self)

        if self.CreateMode is False:
            # Apply Booleans
            TypeStr = "Apply Operations [" + context.scene.Key_Apply + "] : "
            if self.DontApply:
                BoolStr = "(OFF)"
            else:
                BoolStr = "(ON)"
            OpsStr = TypeStr + BoolStr
            TotalWidth = blf.dimensions(font_id, OpsStr)[0]
            xLeft = region.width / 2 - TotalWidth / 2
            xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
            DrawLeftText(TypeStr, xLeft, yCmd - yInterval * 2, IFontSize, Color0, self)
            DrawLeftText(BoolStr, xLeftP, yCmd - yInterval * 2, IFontSize, Color1, self)

            # Auto update for bevel
            TypeStr = "Bevel Update [" + context.scene.Key_Update + "] : "
            if self.Auto_BevelUpdate:
                BoolStr = "(ON)"
            else:
                BoolStr = "(OFF)"
            OpsStr = TypeStr + BoolStr
            TotalWidth = blf.dimensions(font_id, OpsStr)[0]
            xLeft = region.width / 2 - TotalWidth / 2
            xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
            DrawLeftText(TypeStr, xLeft, yCmd - yInterval * 3, IFontSize, Color0, self)
            DrawLeftText(BoolStr, xLeftP, yCmd - yInterval * 3, IFontSize, Color1, self)

        # Subdivisions
        if self.CutMode == CIRCLE:
            if self.CreateMode is False:
                y = yCmd - yInterval * 4
            else:
                y = yCmd - yInterval * 2
            TypeStr = "Subdivisions [" + context.scene.Key_Subrem + "][" + context.scene.Key_Subadd + "] : "
            BoolStr = str((int(360 / self.stepAngle[self.step])))
            OpsStr = TypeStr + BoolStr
            TotalWidth = blf.dimensions(font_id, OpsStr)[0]
            xLeft = region.width / 2 - TotalWidth / 2
            xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
            DrawLeftText(TypeStr, xLeft, y, IFontSize, Color0, self)
            DrawLeftText(BoolStr, xLeftP, y, IFontSize, Color1, self)

    else:
        # INSTANTIATE:
        TypeStr = "Instantiate [" + context.scene.Key_Instant + "] : "
        if self.Instantiate:
            BoolStr = "(ON)"
        else:
            BoolStr = "(OFF)"
        OpsStr = TypeStr + BoolStr
        blf.size(font_id, IFontSize, 72)
        TotalWidth = blf.dimensions(font_id, OpsStr)[0]
        xLeft = region.width / 2 - TotalWidth / 2
        xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
        DrawLeftText(TypeStr, xLeft, yCmd, IFontSize, Color0, self)
        DrawLeftText(BoolStr, xLeftP, yCmd, IFontSize, Color1, self)

        # RANDOM ROTATION:
        if self.alt:
            TypeStr = "Random Rotation [" + context.scene.Key_Randrot + "] : "
            if self.RandomRotation:
                BoolStr = "(ON)"
            else:
                BoolStr = "(OFF)"
            OpsStr = TypeStr + BoolStr
            blf.size(font_id, IFontSize, 72)
            TotalWidth = blf.dimensions(font_id, OpsStr)[0]
            xLeft = region.width / 2 - TotalWidth / 2
            xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
            DrawLeftText(TypeStr, xLeft, yCmd - yInterval, IFontSize, Color0, self)
            DrawLeftText(BoolStr, xLeftP, yCmd - yInterval, IFontSize, Color1, self)

        # THICKNESS:
        if self.BrushSolidify:
            TypeStr = "Thickness [" + context.scene.Key_Depth + "] : "
            if self.ProfileMode:
                BoolStr = str(round(self.ProfileBrush.modifiers["CT_SOLIDIFY"].thickness, 2))
            if self.ObjectMode:
                BoolStr = str(round(self.ObjectBrush.modifiers["CT_SOLIDIFY"].thickness, 2))
            OpsStr = TypeStr + BoolStr
            blf.size(font_id, IFontSize, 72)
            TotalWidth = blf.dimensions(font_id, OpsStr)[0]
            xLeft = region.width / 2 - TotalWidth / 2
            xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
            if self.alt:
                DrawLeftText(TypeStr, xLeft, yCmd - yInterval * 2, IFontSize, Color0, self)
                DrawLeftText(BoolStr, xLeftP, yCmd - yInterval * 2, IFontSize, Color1, self)
            else:
                DrawLeftText(TypeStr, xLeft, yCmd - yInterval, IFontSize, Color0, self)
                DrawLeftText(BoolStr, xLeftP, yCmd - yInterval, IFontSize, Color1, self)

        # BRUSH DEPTH:
        if (self.ObjectMode):
            TypeStr = "Carve Depth [" + context.scene.Key_Depth + "] : "
            BoolStr = str(round(self.ObjectBrush.data.vertices[0].co.z, 2))
            OpsStr = TypeStr + BoolStr
            blf.size(font_id, IFontSize, 72)
            TotalWidth = blf.dimensions(font_id, OpsStr)[0]
            xLeft = region.width / 2 - TotalWidth / 2
            xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
            if self.alt:
                DrawLeftText(TypeStr, xLeft, yCmd - yInterval * 2, IFontSize, Color0, self)
                DrawLeftText(BoolStr, xLeftP, yCmd - yInterval * 2, IFontSize, Color1, self)
            else:
                DrawLeftText(TypeStr, xLeft, yCmd - yInterval, IFontSize, Color0, self)
                DrawLeftText(BoolStr, xLeftP, yCmd - yInterval, IFontSize, Color1, self)

            TypeStr = "Brush Depth [" + context.scene.Key_BrushDepth + "] : "
            BoolStr = str(round(self.BrushDepthOffset, 2))
            OpsStr = TypeStr + BoolStr
            blf.size(font_id, IFontSize, 72)
            TotalWidth = blf.dimensions(font_id, OpsStr)[0]
            xLeft = region.width / 2 - TotalWidth / 2
            xLeftP = xLeft + blf.dimensions(font_id, TypeStr)[0]
            if self.alt:
                DrawLeftText(TypeStr, xLeft, yCmd - yInterval * 3, IFontSize, Color0, self)
                DrawLeftText(BoolStr, xLeftP, yCmd - yInterval * 3, IFontSize, Color1, self)
            else:
                DrawLeftText(TypeStr, xLeft, yCmd - yInterval * 2, IFontSize, Color0, self)
                DrawLeftText(BoolStr, xLeftP, yCmd - yInterval * 2, IFontSize, Color1, self)

    bgl.glEnable(bgl.GL_BLEND)
    if region.width >= 850:
        if self.AskHelp is False:
            xrect = 40
            yrect = 40
            bgl.glColor4f(0.0, 0.0, 0.0, 0.3)
            bgl.glRecti(xrect, yrect, xrect + 90, yrect + 25)
            DrawLeftText("[" + context.scene.Key_Help + "] for help", xrect + 10, yrect + 8, 13, None, self)
        else:
            xHelp = 30 + t_panel_width
            yHelp = 80
            Help_FontSize = 12
            Help_Interval = 14
            if region.width >= 850:
                Help_FontSize = 15
                Help_Interval = 20
                yHelp = 220

            if self.ObjectMode or self.ProfileMode:
                if self.ProfileMode:
                    DrawLeftText("[" + context.scene.Key_Brush + "]", xHelp, yHelp +
                                 Help_Interval * 2, Help_FontSize, UIColor, self)
                    DrawLeftText(": Object Mode", 150 + t_panel_width, yHelp +
                                 Help_Interval * 2, Help_FontSize, None, self)
                else:
                    DrawLeftText("[" + context.scene.Key_Brush + "]", xHelp, yHelp +
                                 Help_Interval * 2, Help_FontSize, UIColor, self)
                    DrawLeftText(": Return", 150 + t_panel_width, yHelp + Help_Interval * 2, Help_FontSize, None, self)
            else:
                DrawLeftText("[" + context.scene.Key_Brush + "]", xHelp, yHelp +
                             Help_Interval * 2, Help_FontSize, UIColor, self)
                DrawLeftText(": Profil Brush", 150 + t_panel_width, yHelp +
                             Help_Interval * 2, Help_FontSize, None, self)
                DrawLeftText("[Ctrl + LMB]", xHelp, yHelp - Help_Interval * 6, Help_FontSize, UIColor, self)
                DrawLeftText(": Move Cursor", 150 + t_panel_width, yHelp -
                             Help_Interval * 6, Help_FontSize, None, self)

            if (self.ObjectMode is False) and (self.ProfileMode is False):
                if self.CreateMode is False:
                    DrawLeftText("[" + context.scene.Key_Create + "]", xHelp,
                                 yHelp + Help_Interval, Help_FontSize, UIColor, self)
                    DrawLeftText(": Create geometry", 150 + t_panel_width,
                                 yHelp + Help_Interval, Help_FontSize, None, self)
                else:
                    DrawLeftText("[" + context.scene.Key_Create + "]", xHelp,
                                 yHelp + Help_Interval, Help_FontSize, UIColor, self)
                    DrawLeftText(": Cut", 150 + t_panel_width, yHelp + Help_Interval, Help_FontSize, None, self)

                if self.CutMode == RECTANGLE:
                    DrawLeftText("MouseMove", xHelp, yHelp, Help_FontSize, UIColor, self)
                    DrawLeftText("[Alt]", xHelp, yHelp - Help_Interval, Help_FontSize, UIColor, self)
                    DrawLeftText("[" + context.scene.Key_Solver + "]",
                                xHelp, yHelp - Help_Interval * 2, Help_FontSize, UIColor, self)
                    DrawLeftText(": Dimension", 150 + t_panel_width, yHelp, Help_FontSize, None, self)
                    DrawLeftText(": Move all", 150 + t_panel_width, yHelp - Help_Interval, Help_FontSize, None, self)
                    DrawLeftText(": Solver [" + context.scene.CarverSolver + "]", 150 + t_panel_width,
                                yHelp - Help_Interval * 2, Help_FontSize, None, self)

                if self.CutMode == CIRCLE:
                    DrawLeftText("MouseMove", xHelp, yHelp, Help_FontSize, UIColor, self)
                    DrawLeftText("[Alt]", xHelp, yHelp - Help_Interval, Help_FontSize, UIColor, self)
                    DrawLeftText("[" + context.scene.Key_Subrem + "] [" + context.scene.Key_Subadd + "]",
                                 xHelp, yHelp - Help_Interval * 2, Help_FontSize, UIColor, self)
                    DrawLeftText("[Ctrl]", xHelp, yHelp - Help_Interval * 3, Help_FontSize, UIColor, self)
                    DrawLeftText("[" + context.scene.Key_Solver + "]",
                                xHelp, yHelp - Help_Interval * 4, Help_FontSize, UIColor, self)
                    DrawLeftText(": Rotation and Radius", 150 + t_panel_width, yHelp, Help_FontSize, None, self)
                    DrawLeftText(": Move all", 150 + t_panel_width, yHelp - Help_Interval, Help_FontSize, None, self)
                    DrawLeftText(": Subdivision", 150 + t_panel_width, yHelp -
                                 Help_Interval * 2, Help_FontSize, None, self)
                    DrawLeftText(": Incremental rotation", 150 + t_panel_width,
                                 yHelp - Help_Interval * 3, Help_FontSize, None, self)
                    DrawLeftText(": Solver [" + context.scene.CarverSolver + "]",
                                150 + t_panel_width, yHelp - Help_Interval * 4, Help_FontSize, None, self)

                if self.CutMode == LINE:
                    DrawLeftText("MouseMove", xHelp, yHelp, Help_FontSize, UIColor, self)
                    DrawLeftText("[Alt]", xHelp, yHelp - Help_Interval, Help_FontSize, UIColor, self)
                    DrawLeftText("[Space]", xHelp, yHelp - Help_Interval * 2, Help_FontSize, UIColor, self)
                    DrawLeftText("[Ctrl]", xHelp, yHelp - Help_Interval * 3, Help_FontSize, UIColor, self)
                    DrawLeftText("[" + context.scene.Key_Solver + "]",
                                xHelp, yHelp - Help_Interval * 4, Help_FontSize, UIColor, self)
                    DrawLeftText(": Dimension", 150 + t_panel_width, yHelp, Help_FontSize, None, self)
                    DrawLeftText(": Move all", 150 + t_panel_width, yHelp - Help_Interval, Help_FontSize, None, self)
                    DrawLeftText(": Validate", 150 + t_panel_width, yHelp -
                                 Help_Interval * 2, Help_FontSize, None, self)
                    DrawLeftText(": Incremental", 150 + t_panel_width, yHelp -
                                 Help_Interval * 3, Help_FontSize, None, self)
                    DrawLeftText(": Solver [" + context.scene.CarverSolver + "]",
                                150 + t_panel_width, yHelp - Help_Interval * 4, Help_FontSize, None, self)
                    if self.CreateMode:
                        DrawLeftText("[" + context.scene.Key_Subadd + "]", xHelp, yHelp -
                                     Help_Interval * 4, Help_FontSize, UIColor, self)
                        DrawLeftText(": Close geometry", 150 + t_panel_width, yHelp -
                                     Help_Interval * 4, Help_FontSize, None, self)
            else:
                DrawLeftText("[Space]", xHelp, yHelp + Help_Interval, Help_FontSize, UIColor, self)
                DrawLeftText(": Difference", 150 + t_panel_width, yHelp + Help_Interval, Help_FontSize, None, self)
                DrawLeftText("[Shift][Space]", xHelp, yHelp, Help_FontSize, UIColor, self)
                DrawLeftText(": Rebool", 150 + t_panel_width, yHelp, Help_FontSize, None, self)
                DrawLeftText("[Alt][Space]", xHelp, yHelp - Help_Interval, Help_FontSize, UIColor, self)
                DrawLeftText(": Duplicate", 150 + t_panel_width, yHelp - Help_Interval, Help_FontSize, None, self)
                DrawLeftText("[" + context.scene.Key_Scale + "]", xHelp, yHelp -
                             Help_Interval * 2, Help_FontSize, UIColor, self)
                DrawLeftText(": Scale", 150 + t_panel_width, yHelp - Help_Interval * 2, Help_FontSize, None, self)
                DrawLeftText("[LMB][Move]", xHelp, yHelp - Help_Interval * 3, Help_FontSize, UIColor, self)
                DrawLeftText(": Rotation", 150 + t_panel_width, yHelp - Help_Interval * 3, Help_FontSize, None, self)
                DrawLeftText("[Ctrl][LMB][Move]", xHelp, yHelp - Help_Interval * 4, Help_FontSize, UIColor, self)
                DrawLeftText(": Step Angle", 150 + t_panel_width, yHelp - Help_Interval * 4, Help_FontSize, None, self)
                if self.ProfileMode:
                    DrawLeftText("[" + context.scene.Key_Subadd + "][" + context.scene.Key_Subrem + "]",
                                 xHelp, yHelp - Help_Interval * 5, Help_FontSize, UIColor, self)
                    DrawLeftText(": Previous or Next Profile", 150 + t_panel_width,
                                 yHelp - Help_Interval * 5, Help_FontSize, None, self)
                DrawLeftText("[ARROWS]", xHelp, yHelp - Help_Interval * 6, Help_FontSize, UIColor, self)
                DrawLeftText(": Create / Delete rows or columns", 150 + t_panel_width,
                             yHelp - Help_Interval * 6, Help_FontSize, None, self)
                DrawLeftText("[" + context.scene.Key_Gapy + "][" + context.scene.Key_Gapx + "]",
                             xHelp, yHelp - Help_Interval * 7, Help_FontSize, UIColor, self)
                DrawLeftText(": Gap between rows or columns", 150 + t_panel_width,
                             yHelp - Help_Interval * 7, Help_FontSize, None, self)
                DrawLeftText("[" + context.scene.Key_Solver + "]",
                            xHelp, yHelp - Help_Interval * 8, Help_FontSize, UIColor, self)
                DrawLeftText(": Solver [" + context.scene.CarverSolver + "]",
                            150 + t_panel_width, yHelp - Help_Interval * 8, Help_FontSize, None, self)

    # Opengl Initialise
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glColor4f(0.512, 0.919, 0.04, 1.0)
    bgl.glLineWidth(2)

    # if context.space_data.region_3d.is_perspective is False:
    if 1:
        bgl.glEnable(bgl.GL_POINT_SMOOTH)

        bgl.glPointSize(6)

        if self.ProfileMode:
            xrect = region.width - t_panel_width - 80
            yrect = 80
            bgl.glColor4f(0.0, 0.0, 0.0, 0.3)
            bgl.glRecti(xrect, yrect, xrect + 60, yrect - 60)

            faces = self.Profils[self.nProfil][3]
            vertices = self.Profils[self.nProfil][2]
            WidthProfil = 50
            location = mathutils.Vector((region.width - t_panel_width - WidthProfil, 50, 0))
            ProfilScale = 20.0
            bgl.glColor4f(UIColor[0], UIColor[1], UIColor[2], 0.7)
            for f in faces:
                if len(f) == 4:
                    bgl.glBegin(bgl.GL_QUADS)
                    bgl.glVertex3f(vertices[f[0]][0] * ProfilScale + location.x, vertices[f[0]][1] *
                                   ProfilScale + location.y, vertices[f[0]][2] * ProfilScale + location.z)
                    bgl.glVertex3f(vertices[f[1]][0] * ProfilScale + location.x, vertices[f[1]][1] *
                                   ProfilScale + location.y, vertices[f[1]][2] * ProfilScale + location.z)
                    bgl.glVertex3f(vertices[f[2]][0] * ProfilScale + location.x, vertices[f[2]][1] *
                                   ProfilScale + location.y, vertices[f[2]][2] * ProfilScale + location.z)
                    bgl.glVertex3f(vertices[f[3]][0] * ProfilScale + location.x, vertices[f[3]][1] *
                                   ProfilScale + location.y, vertices[f[3]][2] * ProfilScale + location.z)
                    bgl.glEnd()
                if len(f) == 3:
                    bgl.glBegin(bgl.GL_TRIANGLES)
                    bgl.glVertex3f(vertices[f[0]][0] * ProfilScale + location.x, vertices[f[0]][1] *
                                   ProfilScale + location.y, vertices[f[0]][2] * ProfilScale + location.z)
                    bgl.glVertex3f(vertices[f[1]][0] * ProfilScale + location.x, vertices[f[1]][1] *
                                   ProfilScale + location.y, vertices[f[1]][2] * ProfilScale + location.z)
                    bgl.glVertex3f(vertices[f[2]][0] * ProfilScale + location.x, vertices[f[2]][1] *
                                   ProfilScale + location.y, vertices[f[2]][2] * ProfilScale + location.z)
                    bgl.glEnd()

        if self.bDone:
            if len(self.mouse_path) > 1:
                x0 = self.mouse_path[0][0]
                y0 = self.mouse_path[0][1]
                x1 = self.mouse_path[1][0]
                y1 = self.mouse_path[1][1]

            # Cut Line
            if self.CutMode == LINE:
                if (self.shift) or (self.CreateMode and self.Closed):
                    bgl.glColor4f(UIColor[0], UIColor[1], UIColor[2], 0.5)

                    bgl.glBegin(bgl.GL_POLYGON)
                    for x, y in self.mouse_path:
                        bgl.glVertex2i(x + self.xpos, y + self.ypos)
                    bgl.glEnd()

                bgl.glColor4f(UIColor[0], UIColor[1], UIColor[2], 1.0)
                bgl.glBegin(bgl.GL_LINE_STRIP)
                for x, y in self.mouse_path:
                    bgl.glVertex2i(x + self.xpos, y + self.ypos)
                bgl.glEnd()
                if (self.CreateMode is False) or (self.CreateMode and self.Closed):
                    bgl.glBegin(bgl.GL_LINE_STRIP)
                    bgl.glVertex2i(self.mouse_path[len(self.mouse_path) - 1][0] + self.xpos,
                                   self.mouse_path[len(self.mouse_path) - 1][1] + self.ypos)
                    bgl.glVertex2i(self.mouse_path[0][0] + self.xpos, self.mouse_path[0][1] + self.ypos)
                    bgl.glEnd()

                bgl.glPointSize(6)
                bgl.glBegin(bgl.GL_POINTS)
                for x, y in self.mouse_path:
                    bgl.glVertex2i(x + self.xpos, y + self.ypos)
                bgl.glEnd()

            # Cut rectangle
            if self.CutMode == RECTANGLE:
                bgl.glColor4f(UIColor[0], UIColor[1], UIColor[2], 0.5)

                # if SHIFT, fill primitive
                if self.shift or self.CreateMode:
                    bgl.glBegin(bgl.GL_QUADS)
                    bgl.glVertex2i(x0 + self.xpos, y0 + self.ypos)
                    bgl.glVertex2i(x1 + self.xpos, y0 + self.ypos)
                    bgl.glVertex2i(x1 + self.xpos, y1 + self.ypos)
                    bgl.glVertex2i(x0 + self.xpos, y1 + self.ypos)
                    bgl.glEnd()

                bgl.glBegin(bgl.GL_LINE_STRIP)
                bgl.glVertex2i(x0 + self.xpos, y0 + self.ypos)
                bgl.glVertex2i(x1 + self.xpos, y0 + self.ypos)
                bgl.glVertex2i(x1 + self.xpos, y1 + self.ypos)
                bgl.glVertex2i(x0 + self.xpos, y1 + self.ypos)
                bgl.glVertex2i(x0 + self.xpos, y0 + self.ypos)
                bgl.glEnd()
                bgl.glPointSize(6)

                bgl.glColor4f(UIColor[0], UIColor[1], UIColor[2], 1.0)
                bgl.glBegin(bgl.GL_POINTS)
                bgl.glVertex2i(x0 + self.xpos, y0 + self.ypos)
                bgl.glVertex2i(x1 + self.xpos, y0 + self.ypos)
                bgl.glVertex2i(x1 + self.xpos, y1 + self.ypos)
                bgl.glVertex2i(x0 + self.xpos, y1 + self.ypos)
                bgl.glEnd()

            # Circle Cut
            if self.CutMode == CIRCLE:
                DEG2RAD = 3.14159 / 180
                v0 = mathutils.Vector((self.mouse_path[0][0], self.mouse_path[0][1], 0))
                v1 = mathutils.Vector((self.mouse_path[1][0], self.mouse_path[1][1], 0))
                v0 -= v1
                radius = self.mouse_path[1][0] - self.mouse_path[0][0]
                DEG2RAD = 3.14159 / (180.0 / self.stepAngle[self.step])
                if self.ctrl:
                    self.stepR = (self.mouse_path[1][1] - self.mouse_path[0][1]) / 25
                    shift = (3.14159 / (360.0 / 60.0)) * int(self.stepR)
                else:
                    shift = (self.mouse_path[1][1] - self.mouse_path[0][1]) / 50

                if self.shift or self.CreateMode:
                    bgl.glColor4f(UIColor[0], UIColor[1], UIColor[2], 0.5)
                    bgl.glBegin(bgl.GL_TRIANGLE_FAN)
                    bgl.glVertex2f(x0 + self.xpos, y0 + self.ypos)
                    for i in range(0, int(360 / self.stepAngle[self.step])):
                        degInRad = i * DEG2RAD
                        bgl.glVertex2f(x0 + self.xpos + math.cos(degInRad + shift) * radius,
                                       y0 + self.ypos + math.sin(degInRad + shift) * radius)
                    bgl.glVertex2f(x0 + self.xpos + math.cos(0 + shift) * radius,
                                   y0 + self.ypos + math.sin(0 + shift) * radius)
                    bgl.glEnd()

                bgl.glColor4f(UIColor[0], UIColor[1], UIColor[2], 1.0)
                bgl.glBegin(bgl.GL_LINE_LOOP)
                for i in range(0, int(360 / self.stepAngle[self.step])):
                    degInRad = i * DEG2RAD
                    bgl.glVertex2f(x0 + self.xpos + math.cos(degInRad + shift) * radius,
                                   y0 + self.ypos + math.sin(degInRad + shift) * radius)
                bgl.glEnd()

    if self.ObjectMode or self.ProfileMode:
        if self.ShowCursor:
            region = context.region
            rv3d = context.space_data.region_3d
            view_width = context.region.width

            if self.ObjectMode:
                ob = self.ObjectBrush
            if self.ProfileMode:
                ob = self.ProfileBrush
            mat = ob.matrix_world

            # 50% alpha, 2 pixel width line
            bgl.glEnable(bgl.GL_BLEND)

            bbox = [mat * mathutils.Vector(b) for b in ob.bound_box]

            if self.shift:
                bgl.glLineWidth(4)
                bgl.glColor4f(0.5, 1.0, 0.0, 1.0)
            else:
                bgl.glLineWidth(2)
                bgl.glColor4f(1.0, 0.8, 0.0, 1.0)
            bgl.glBegin(bgl.GL_LINE_STRIP)
            idx = 0
            CRadius = ((bbox[7] - bbox[0]).length) / 2
            for i in range(int(len(self.CLR_C) / 3)):
                vector3d = (self.CLR_C[idx * 3] * CRadius + self.CurLoc.x, self.CLR_C[idx * 3 + 1] *
                            CRadius + self.CurLoc.y, self.CLR_C[idx * 3 + 2] * CRadius + self.CurLoc.z)
                vector2d = bpy_extras.view3d_utils.location_3d_to_region_2d(region, rv3d, vector3d)
                if vector2d is not None:
                    bgl.glVertex2f(*vector2d)
                idx += 1
            bgl.glEnd()

            bgl.glLineWidth(1)
            bgl.glDisable(bgl.GL_BLEND)
            bgl.glColor4f(0.0, 0.0, 0.0, 1.0)

            # Object display
            if self.qRot is not None:
                ob.location = self.CurLoc
                v = mathutils.Vector()
                v.x = v.y = 0.0
                v.z = self.BrushDepthOffset
                ob.location += self.qRot * v

                e = mathutils.Euler()
                e.x = 0.0
                e.y = 0.0
                e.z = self.aRotZ / 25.0

                qe = e.to_quaternion()
                qRot = self.qRot * qe
                ob.rotation_mode = 'QUATERNION'
                ob.rotation_quaternion = qRot
                ob.rotation_mode = 'XYZ'

                if self.ProfileMode:
                    if self.ProfileBrush is not None:
                        self.ProfileBrush.location = self.CurLoc
                        self.ProfileBrush.rotation_mode = 'QUATERNION'
                        self.ProfileBrush.rotation_quaternion = qRot
                        self.ProfileBrush.rotation_mode = 'XYZ'

    # Opengl defaults
    bgl.glLineWidth(1)
    bgl.glDisable(bgl.GL_BLEND)
    bgl.glColor4f(0.0, 0.0, 0.0, 1.0)
    bgl.glDisable(bgl.GL_POINT_SMOOTH)


# Intersection
# intersection function (ideasman42)
def isect_line_plane_v3(p0, p1, p_co, p_no, epsilon=1e-6):
    """
    p0, p1: define the line
    p_co, p_no: define the plane:
        p_co is a point on the plane (plane coordinate).
        p_no is a normal vector defining the plane direction; does not need to be normalized.

    return a Vector or None (when the intersection can't be found).
    """

    u = sub_v3v3(p1, p0)
    dot = dot_v3v3(p_no, u)

    if abs(dot) > epsilon:
        # the factor of the point between p0 -> p1 (0 - 1)
        # if 'fac' is between (0 - 1) the point intersects with the segment.
        # otherwise:
        #  < 0.0: behind p0.
        #  > 1.0: infront of p1.
        w = sub_v3v3(p0, p_co)
        fac = -dot_v3v3(p_no, w) / dot
        u = mul_v3_fl(u, fac)
        return add_v3v3(p0, u)
    else:
        # The segment is parallel to plane
        return None

# ----------------------
# generic math functions


def add_v3v3(v0, v1):
    return (
        v0[0] + v1[0],
        v0[1] + v1[1],
        v0[2] + v1[2],
    )


def sub_v3v3(v0, v1):
    return (
        v0[0] - v1[0],
        v0[1] - v1[1],
        v0[2] - v1[2],
    )


def dot_v3v3(v0, v1):
    return (
        (v0[0] * v1[0]) +
        (v0[1] * v1[1]) +
        (v0[2] * v1[2])
    )


def len_squared_v3(v0):
    return dot_v3v3(v0, v0)


def mul_v3_fl(v0, f):
    return (
        v0[0] * f,
        v0[1] * f,
        v0[2] * f,
    )

# Cut Square


def CreateCutSquare(self, context):
    FAR_LIMIT = 10000.0

    # New mesh
    me = bpy.data.meshes.new('CMT_Square')

    # New object
    ob = bpy.data.objects.new('CMT_Square', me)
    # Save new object
    self.CurrentObj = ob
    # Scene informations
    scene = context.scene
    region = context.region
    rv3d = context.region_data
    coord = self.mouse_path[0][0], self.mouse_path[0][1]

    depthLocation = region_2d_to_vector_3d(region, rv3d, coord)
    self.ViewVector = depthLocation
    if self.snapCursor:
        PlanePoint = context.scene.cursor_location
    else:
        if self.OpsObj is not None:
            PlanePoint = self.OpsObj.location
        else:
            PlanePoint = mathutils.Vector((0.0, 0.0, 0.0))

    PlaneNormal = depthLocation
    PlaneNormalised = PlaneNormal.normalized()
    d = -PlanePoint.x * PlaneNormalised.x - PlanePoint.y * PlaneNormalised.y - PlanePoint.z * PlaneNormalised.z

    # Link object to scene
    context.scene.objects.link(ob)

    # New bmesh
    t_bm = bmesh.new()
    t_bm.from_mesh(me)
    # Convert in 3d space
    v0 = self.mouse_path[0][0] + self.xpos, self.mouse_path[0][1] + self.ypos
    v1 = self.mouse_path[1][0] + self.xpos, self.mouse_path[1][1] + self.ypos
    v2 = self.mouse_path[1][0] + self.xpos, self.mouse_path[0][1] + self.ypos
    v3 = self.mouse_path[0][0] + self.xpos, self.mouse_path[1][1] + self.ypos
    vec = region_2d_to_vector_3d(region, rv3d, v0)
    loc0 = region_2d_to_location_3d(region, rv3d, v0, vec)

    vec = region_2d_to_vector_3d(region, rv3d, v1)
    loc1 = region_2d_to_location_3d(region, rv3d, v1, vec)

    vec = region_2d_to_vector_3d(region, rv3d, v2)
    loc2 = region_2d_to_location_3d(region, rv3d, v2, vec)

    vec = region_2d_to_vector_3d(region, rv3d, v3)
    loc3 = region_2d_to_location_3d(region, rv3d, v3, vec)
    p0 = loc0
    p1 = loc0 + PlaneNormalised * FAR_LIMIT
    loc0 = isect_line_plane_v3(p0, p1, PlanePoint, PlaneNormalised)
    p0 = loc1
    p1 = loc1 + PlaneNormalised * FAR_LIMIT
    loc1 = isect_line_plane_v3(p0, p1, PlanePoint, PlaneNormalised)
    p0 = loc2
    p1 = loc2 + PlaneNormalised * FAR_LIMIT
    loc2 = isect_line_plane_v3(p0, p1, PlanePoint, PlaneNormalised)
    p0 = loc3
    p1 = loc3 + PlaneNormalised * FAR_LIMIT
    loc3 = isect_line_plane_v3(p0, p1, PlanePoint, PlaneNormalised)

    t_v0 = t_bm.verts.new(loc0)
    t_v1 = t_bm.verts.new(loc2)
    t_v2 = t_bm.verts.new(loc1)
    t_v3 = t_bm.verts.new(loc3)

    # Update vertices index
    t_bm.verts.index_update()
    # New faces
    t_face = t_bm.faces.new([t_v0, t_v1, t_v2, t_v3])
    # Set mesh
    t_bm.to_mesh(me)


# Cut Line
def CreateCutLine(self, context):
    FAR_LIMIT = 10000.0

    me = bpy.data.meshes.new('CMT_Line')

    ob = bpy.data.objects.new('CMT_Line', me)
    self.CurrentObj = ob

    scene = context.scene
    region = context.region
    rv3d = context.region_data
    coord = self.mouse_path[0][0], self.mouse_path[0][1]
    depthLocation = region_2d_to_vector_3d(region, rv3d, coord)
    self.ViewVector = depthLocation
    if self.snapCursor:
        PlanePoint = context.scene.cursor_location
    else:
        PlanePoint = mathutils.Vector((0.0, 0.0, 0.0))

    PlaneNormal = depthLocation
    PlaneNormalised = PlaneNormal.normalized()
    d = -PlanePoint.x * PlaneNormalised.x - PlanePoint.y * PlaneNormalised.y - PlanePoint.z * PlaneNormalised.z

    context.scene.objects.link(ob)

    t_bm = bmesh.new()
    t_bm.from_mesh(me)

    FacesList = []
    NbVertices = 0

    bLine = False

    if (len(self.mouse_path) == 2) or ((len(self.mouse_path) <= 3) and (self.mouse_path[1] == self.mouse_path[2])):
        PlanePoint = mathutils.Vector((0.0, 0.0, 0.0))
        PlaneNormal = depthLocation
        PlaneNormalised = PlaneNormal.normalized()
        # Force rebool
        self.ForceRebool = True
        # It's a line
        bLine = True
        Index = 0
        for x, y in self.mouse_path:
            if Index < 2:
                v0 = x + self.xpos, y + self.ypos
                vec = region_2d_to_vector_3d(region, rv3d, v0)
                loc0 = region_2d_to_location_3d(region, rv3d, v0, vec)

                p0 = loc0
                p1 = loc0 + PlaneNormalised * FAR_LIMIT
                loc0 = isect_line_plane_v3(p0, p1, PlanePoint, PlaneNormalised)

                NbVertices += 1
                Index += 1
                if NbVertices == 1:
                    t_v0 = t_bm.verts.new(loc0)
                    t_init = t_v0
                    LocInit = loc0
                    t_bm.verts.index_update()
                else:
                    t_v1 = t_bm.verts.new(loc0)
                    t_edges = t_bm.edges.new([t_v0, t_v1])
                    NbVertices = 1
                    t_v0 = t_v1

    else:
        for x, y in self.mouse_path:
            v0 = x + self.xpos, y + self.ypos
            vec = region_2d_to_vector_3d(region, rv3d, v0)
            loc0 = region_2d_to_location_3d(region, rv3d, v0, vec)

            p0 = loc0
            p1 = loc0 + PlaneNormalised * FAR_LIMIT
            loc0 = isect_line_plane_v3(p0, p1, PlanePoint, PlaneNormalised)

            NbVertices += 1
            if NbVertices == 1:
                t_v0 = t_bm.verts.new(loc0)
                t_init = t_v0
                LocInit = loc0
                t_bm.verts.index_update()
                FacesList.append(t_v0)
            else:
                t_v1 = t_bm.verts.new(loc0)
                t_edges = t_bm.edges.new([t_v0, t_v1])
                FacesList.append(t_v1)
                NbVertices = 1
                t_v0 = t_v1

    if self.CreateMode:
        if self.Closed and (bLine is False):
            t_v1 = t_bm.verts.new(LocInit)
            t_edges = t_bm.edges.new([t_v0, t_v1])
            FacesList.append(t_v1)
            t_face = t_bm.faces.new(FacesList)
    else:
        if bLine is False:
            t_v1 = t_bm.verts.new(LocInit)
            t_edges = t_bm.edges.new([t_v0, t_v1])
            FacesList.append(t_v1)
            t_face = t_bm.faces.new(FacesList)

    t_bm.to_mesh(me)


# Cut Circle
def CreateCutCircle(self, context):
    FAR_LIMIT = 10000.0

    me = bpy.data.meshes.new('CMT_Circle')

    ob = bpy.data.objects.new('CMT_Circle', me)
    self.CurrentObj = ob

    scene = context.scene
    region = context.region
    rv3d = context.region_data
    coord = self.mouse_path[0][0], self.mouse_path[0][1]
    depthLocation = region_2d_to_vector_3d(region, rv3d, coord)
    self.ViewVector = depthLocation
    if self.snapCursor:
        PlanePoint = context.scene.cursor_location
    else:
        PlanePoint = mathutils.Vector((0.0, 0.0, 0.0))

    PlaneNormal = depthLocation
    PlaneNormalised = PlaneNormal.normalized()
    d = -PlanePoint.x * PlaneNormalised.x - PlanePoint.y * PlaneNormalised.y - PlanePoint.z * PlaneNormalised.z

    context.scene.objects.link(ob)

    t_bm = bmesh.new()
    t_bm.from_mesh(me)

    x0 = self.mouse_path[0][0]
    y0 = self.mouse_path[0][1]
    x1 = self.mouse_path[1][0]
    y1 = self.mouse_path[1][1]

    v0 = mathutils.Vector((self.mouse_path[0][0], self.mouse_path[0][1], 0))
    v1 = mathutils.Vector((self.mouse_path[1][0], self.mouse_path[1][1], 0))
    v0 -= v1
    radius = self.mouse_path[1][0] - self.mouse_path[0][0]
    DEG2RAD = math.pi / (180.0 / self.stepAngle[self.step])
    if self.ctrl:
        self.stepR = (self.mouse_path[1][1] - self.mouse_path[0][1]) / 25
        shift = (math.pi / (360.0 / self.stepAngle[self.step])) * (self.stepR)
    else:
        shift = (self.mouse_path[1][1] - self.mouse_path[0][1]) / 50

    # Convert point in 3D Space
    FacesList = []
    for i in range(0, int(360.0 / self.stepAngle[self.step])):
        degInRad = i * DEG2RAD
        v0 = x0 + self.xpos + math.cos(degInRad + shift) * radius, y0 + self.ypos + math.sin(degInRad + shift) * radius
        vec = region_2d_to_vector_3d(region, rv3d, v0)
        loc0 = region_2d_to_location_3d(region, rv3d, v0, vec)

        p0 = loc0
        p1 = loc0 + PlaneNormalised * FAR_LIMIT
        loc0 = isect_line_plane_v3(p0, p1, PlanePoint, PlaneNormalised)

        t_v0 = t_bm.verts.new(loc0)

        FacesList.append(t_v0)

    t_bm.verts.index_update()

    t_face = t_bm.faces.new(FacesList)

    t_bm.to_mesh(me)


# Object dimensions (SCULPT Tools tips)
def objDiagonal(obj):
    return ((obj.dimensions[0]**2) + (obj.dimensions[1]**2) + (obj.dimensions[2]**2))**0.5


# Bevel Update
def update_bevel(context):
    selection = context.selected_objects.copy()
    active = context.active_object

    if len(selection) > 0:
        for obj in selection:
            bpy.ops.object.select_all(action='DESELECT')
            obj.select = True
            context.scene.objects.active = obj

            # Test object name
            if obj.data.name.startswith("S_") or obj.data.name.startswith("S "):
                bpy.ops.object.mode_set(mode='EDIT')
                bpy.ops.mesh.region_to_loop()
                bpy.ops.transform.edge_bevelweight(value=1)
                bpy.ops.object.mode_set(mode='OBJECT')
            else:
                act_bevel = False
                for mod in obj.modifiers:
                    if mod.type == 'BEVEL':
                        act_bevel = True
                if act_bevel:
                    context.scene.objects.active = bpy.data.objects[obj.name]
                    active = obj

                    bpy.ops.object.mode_set(mode='EDIT')

                    # Edge mode
                    bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='EDGE')

                    # Clear all
                    bpy.ops.mesh.select_all(action='SELECT')
                    bpy.ops.mesh.mark_sharp(clear=True)
                    bpy.ops.transform.edge_crease(value=-1)

                    bpy.ops.transform.edge_bevelweight(value=-1)
                    bpy.ops.mesh.select_all(action='DESELECT')
                    bpy.ops.mesh.edges_select_sharp(sharpness=0.523599)
                    bpy.ops.mesh.mark_sharp()
                    bpy.ops.transform.edge_crease(value=1)
                    bpy.ops.mesh.select_all(action='DESELECT')
                    bpy.ops.mesh.edges_select_sharp(sharpness=0.523599)
                    bpy.ops.transform.edge_bevelweight(value=1)
                    bpy.ops.mesh.select_all(action='DESELECT')

                    bpy.ops.object.mode_set(mode="OBJECT")

                    active.data.use_customdata_edge_bevel = True

                    for i in range(len(active.data.edges)):
                        if active.data.edges[i].select is True:
                            active.data.edges[i].bevel_weight = 1.0
                            active.data.edges[i].use_edge_sharp = True

                    Already = False
                    for m in active.modifiers:
                        if m.name == 'Bevel':
                            Already = True

                    if Already is False:
                        bpy.ops.object.modifier_add(type='BEVEL')
                        mod = context.object.modifiers[-1]
                        mod.limit_method = 'WEIGHT'
                        mod.width = 0.01
                        mod.profile = 0.699099
                        mod.use_clamp_overlap = False
                        mod.segments = 3
                        mod.loop_slide = False

                    bpy.ops.object.shade_smooth()

                    context.object.data.use_auto_smooth = True
                    context.object.data.auto_smooth_angle = 1.0472

    bpy.ops.object.select_all(action='DESELECT')

    for obj in selection:
        obj.select = True
    context.scene.objects.active = active

# Create bevel


def CreateBevel(context, CurrentObject):
    # Save active object
    SavActive = context.active_object
    # Active "CurrentObject"
    context.scene.objects.active = CurrentObject

    bpy.ops.object.mode_set(mode='EDIT')

    # Edge mode
    bpy.ops.mesh.select_mode(use_extend=False, use_expand=False, type='EDGE')

    # Clear all
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.mark_sharp(clear=True)
    bpy.ops.transform.edge_crease(value=-1)

    bpy.ops.transform.edge_bevelweight(value=-1)
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.mesh.edges_select_sharp(sharpness=0.523599)
    bpy.ops.mesh.mark_sharp()
    bpy.ops.transform.edge_crease(value=1)
    bpy.ops.mesh.select_all(action='DESELECT')
    bpy.ops.mesh.edges_select_sharp(sharpness=0.523599)
    bpy.ops.transform.edge_bevelweight(value=1)
    bpy.ops.mesh.select_all(action='DESELECT')

    bpy.ops.object.mode_set(mode="OBJECT")

    bpy.ops.object.mode_set(mode='OBJECT')

    Already = False
    for m in CurrentObject.modifiers:
        if m.name == 'Bevel':
            Already = True

    if Already is False:
        bpy.ops.object.modifier_add(type='BEVEL')
        mod = context.object.modifiers[-1]
        mod.limit_method = 'WEIGHT'
        mod.width = 0.01
        mod.profile = 0.699099
        mod.use_clamp_overlap = False
        mod.segments = 3
        mod.loop_slide = False

    bpy.ops.object.shade_smooth()

    context.object.data.use_auto_smooth = True
    context.object.data.auto_smooth_angle = 1.0471975

    # Remet l'objet actif par défaut
    context.scene.objects.active = SavActive


# Picking (template)
def Picking(context, event):
    # get the context arguments
    scene = context.scene
    region = context.region
    rv3d = context.region_data
    coord = event.mouse_region_x, event.mouse_region_y

    # get the ray from the viewport and mouse
    view_vector = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
    ray_origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)

    ray_target = ray_origin + view_vector

    def visible_objects_and_duplis():
        for obj in context.visible_objects:
            if obj.type == 'MESH':
                yield (obj, obj.matrix_world.copy())

            if obj.dupli_type != 'NONE':
                obj.dupli_list_create(scene)
                for dob in obj.dupli_list:
                    obj_dupli = dob.object
                    if obj_dupli.type == 'MESH':
                        yield (obj_dupli, dob.matrix.copy())

            obj.dupli_list_clear()

    def obj_ray_cast(obj, matrix):
        # get the ray relative to the object
        matrix_inv = matrix.inverted()
        ray_origin_obj = matrix_inv * ray_origin
        ray_target_obj = matrix_inv * ray_target
        ray_direction_obj = ray_target_obj - ray_origin_obj
        # cast the ray
        success, location, normal, face_index = obj.ray_cast(ray_origin_obj, ray_direction_obj)
        if success:
            return location, normal, face_index
        else:
            return None, None, None

    # cast rays and find the closest object
    best_length_squared = -1.0
    best_obj = None

    # cast rays and find the closest object
    for obj, matrix in visible_objects_and_duplis():
        if obj.type == 'MESH':
            hit, normal, face_index = obj_ray_cast(obj, matrix)
            if hit is not None:
                hit_world = matrix * hit
                length_squared = (hit_world - ray_origin).length_squared
                if best_obj is None or length_squared < best_length_squared:
                    scene.cursor_location = hit_world
                    best_length_squared = length_squared
                    best_obj = obj
            else:
                if best_obj is None:
                    depthLocation = region_2d_to_vector_3d(region, rv3d, coord)
                    loc = region_2d_to_location_3d(region, rv3d, coord, depthLocation)
                    scene.cursor_location = loc


def CreatePrimitive(self, _AngleStep, _radius):
    CLRaw = []
    Angle = 0.0
    self.NbPointsInPrimitive = 0
    while(Angle < 360.0):
        self.CircleListRaw.append(math.cos(math.radians(Angle)) * _radius)
        self.CircleListRaw.append(math.sin(math.radians(Angle)) * _radius)
        self.CircleListRaw.append(0.0)
        Angle += _AngleStep
        self.NbPointsInPrimitive += 1
    self.CircleListRaw.append(math.cos(math.radians(0.0)) * _radius)
    self.CircleListRaw.append(math.sin(math.radians(0.0)) * _radius)
    self.CircleListRaw.append(0.0)
    self.NbPointsInPrimitive += 1


def MoveCursor(qRot, location, self):
    if qRot is not None:
        self.CLR_C.clear()
        vc = mathutils.Vector()
        idx = 0
        for i in range(int(len(self.CircleListRaw) / 3)):
            vc.x = self.CircleListRaw[idx * 3] * self.CRadius
            vc.y = self.CircleListRaw[idx * 3 + 1] * self.CRadius
            vc.z = self.CircleListRaw[idx * 3 + 2] * self.CRadius
            vc = qRot * vc
            self.CLR_C.append(vc.x)
            self.CLR_C.append(vc.y)
            self.CLR_C.append(vc.z)
            idx += 1


def RBenVe(Object, Dir):
    ObjectV = Object.normalized()
    DirV = Dir.normalized()
    cosTheta = ObjectV.dot(DirV)
    rotationAxis = mathutils.Vector((0.0, 0.0, 0.0))
    if (cosTheta < -1 + 0.001):
        v = mathutils.Vector((0.0, 1.0, 0.0))
        rotationAxis = ObjectV.cross(v)
        rotationAxis = rotationAxis.normalized()
        q = mathutils.Quaternion()
        q.w = 0.0
        q.x = rotationAxis.x
        q.y = rotationAxis.y
        q.z = rotationAxis.z
        return q
    rotationAxis = ObjectV.cross(DirV)
    s = math.sqrt((1.0 + cosTheta) * 2.0)
    invs = 1 / s
    q = mathutils.Quaternion()
    q.w = s * 0.5
    q.x = rotationAxis.x * invs
    q.y = rotationAxis.y * invs
    q.z = rotationAxis.z * invs
    return q


def Pick(context, event, self, ray_max=10000.0):
    scene = context.scene
    region = context.region
    rv3d = context.region_data
    coord = event.mouse_region_x, event.mouse_region_y
    view_vector = view3d_utils.region_2d_to_vector_3d(region, rv3d, coord)
    ray_origin = view3d_utils.region_2d_to_origin_3d(region, rv3d, coord)
    ray_target = ray_origin + (view_vector * ray_max)

    def obj_ray_cast(obj, matrix):
        matrix_inv = matrix.inverted()
        ray_origin_obj = matrix_inv * ray_origin
        ray_target_obj = matrix_inv * ray_target
        success, hit, normal, face_index = obj.ray_cast(ray_origin_obj, ray_target_obj)
        if success:
            return hit, normal, face_index
        else:
            return None, None, None

    best_length_squared = ray_max * ray_max
    best_obj = None
    for obj in self.CList:
        matrix = obj.matrix_world
        hit, normal, face_index = obj_ray_cast(obj, matrix)
        rotation = obj.rotation_euler.to_quaternion()
        if hit is not None:
            hit_world = matrix * hit
            length_squared = (hit_world - ray_origin).length_squared
            if length_squared < best_length_squared:
                best_length_squared = length_squared
                best_obj = obj
                hits = hit_world
                ns = normal
                fs = face_index

    if best_obj is not None:
        return hits, ns, fs, rotation
    else:
        return None, None, None


def SelectObject(self, copyobj):
    copyobj.select = True

    for child in copyobj.children:
        SelectObject(self, child)

    if copyobj.parent is None:
        bpy.context.scene.objects.active = copyobj


# Undo
def printUndo(self):
    for l in self.UList:
        print(l)


def UndoAdd(self, type, OpsObj):
    if OpsObj is None:
        return
    if type != "DUPLICATE":
        ob = OpsObj
        # Creation du mesh de 'sauvegarde'
        bm = bmesh.new()
        bm.from_mesh(ob.data)

        self.UndoOps.append((OpsObj, type, bm))
    else:
        self.UndoOps.append((OpsObj, type, None))


def UndoListUpdate(self):
    self.UList.append((self.UndoOps.copy()))
    self.UList_Index += 1
    self.UndoOps.clear()


def Undo(self):
    if self.UList_Index < 0:
        return
    # get previous mesh
    for o in self.UList[self.UList_Index]:
        if o[1] == "MESH":
            bm = o[2]
            bm.to_mesh(o[0].data)

    SelectObjList = bpy.context.selected_objects.copy()
    Active_Obj = bpy.context.active_object
    bpy.ops.object.select_all(action='TOGGLE')

    for o in self.UList[self.UList_Index]:
        if o[1] == "REBOOL":
            o[0].select = True
            o[0].hide = False

        if o[1] == "DUPLICATE":
            o[0].select = True
            o[0].hide = False

    bpy.ops.object.delete(use_global=False)

    for so in SelectObjList:
        bpy.data.objects[so.name].select = True
    bpy.context.scene.objects.active = Active_Obj

    self.UList_Index -= 1
    self.UList[self.UList_Index + 1:] = []


def duplicateObject(self):
    if self.Instantiate:
        bpy.ops.object.duplicate_move_linked(
            OBJECT_OT_duplicate={
                "linked": True,
                "mode": 'TRANSLATION',
            },
            TRANSFORM_OT_translate={
                "value": (0, 0, 0),
            },
        )
    else:
        bpy.ops.object.duplicate_move(
            OBJECT_OT_duplicate={
                "linked": False,
                "mode": 'TRANSLATION',
            },
            TRANSFORM_OT_translate={
                "value": (0, 0, 0),
            },
        )

    ob_new = bpy.context.active_object

    ob_new.location = self.CurLoc
    v = mathutils.Vector()
    v.x = v.y = 0.0
    v.z = self.BrushDepthOffset
    ob_new.location += self.qRot * v

    if self.ObjectMode:
        ob_new.scale = self.ObjectBrush.scale
    if self.ProfileMode:
        ob_new.scale = self.ProfileBrush.scale

    e = mathutils.Euler()
    e.x = e.y = 0.0
    e.z = self.aRotZ / 25.0

    # If duplicate with a grid, no random rotation (each mesh in the grid is already rotated randomly)
    if (self.alt is True) and ((self.nbcol + self.nbrow) < 3):
        if self.RandomRotation:
            e.z += random.random()

    qe = e.to_quaternion()
    qRot = self.qRot * qe
    ob_new.rotation_mode = 'QUATERNION'
    ob_new.rotation_quaternion = qRot
    ob_new.rotation_mode = 'XYZ'

    if (ob_new.draw_type == "WIRE") and (self.BrushSolidify is False):
        ob_new.hide = True

    if self.BrushSolidify:
        ob_new.draw_type = "SOLID"
        ob_new.show_x_ray = False

    for o in bpy.context.selected_objects:
        UndoAdd(self, "DUPLICATE", o)

    if len(bpy.context.selected_objects) > 0:
        bpy.ops.object.select_all(action='TOGGLE')
    for o in self.SavSel:
        o.select = True

    bpy.context.scene.objects.active = self.OpsObj


def update_grid(self, context):
    """
    Thanks to batFINGER for his help :
    source : http://blender.stackexchange.com/questions/55864/multiple-meshes-not-welded-with-pydata
    """
    verts = []
    edges = []
    faces = []
    numface = 0

    if self.nbcol < 1:
        self.nbcol = 1
    if self.nbrow < 1:
        self.nbrow = 1
    if self.gapx < 0:
        self.gapx = 0
    if self.gapy < 0:
        self.gapy = 0

    # Get the data from the profils or the object
    if self.ProfileMode:
        brush = bpy.data.objects.new(self.Profils[self.nProfil][0], bpy.data.meshes[self.Profils[self.nProfil][0]])
        obj = bpy.data.objects["CT_Profil"]
        obfaces = brush.data.polygons
        obverts = brush.data.vertices
        lenverts = len(obverts)
    else:
        brush = bpy.data.objects["CarverBrushCopy"]
        obj = context.selected_objects[0]
        obverts = brush.data.vertices
        obfaces = brush.data.polygons
        lenverts = len(brush.data.vertices)

    # Gap between each row / column
    gapx = self.gapx
    gapy = self.gapy

    # Width of each row / column
    widthx = brush.dimensions.x * self.scale_x
    widthy = brush.dimensions.y * self.scale_y

    # Compute the corners so the new object will be always at the center
    left = -((self.nbcol - 1) * (widthx + gapx)) / 2
    start = -((self.nbrow - 1) * (widthy + gapy)) / 2

    for i in range(self.nbrow * self.nbcol):
        row = i % self.nbrow
        col = i // self.nbrow
        startx = left + ((widthx + gapx) * col)
        starty = start + ((widthy + gapy) * row)

        # Add random rotation
        if (self.RandomRotation) and not (self.GridScaleX or self.GridScaleY):
            rotmat = mathutils.Matrix.Rotation(math.radians(360 * random.random()), 4, 'Z')
            for v in obverts:
                v.co = v.co * rotmat

        verts.extend([((v.co.x - startx, v.co.y - starty, v.co.z)) for v in obverts])
        faces.extend([[v + numface * lenverts for v in p.vertices] for p in obfaces])
        numface += 1

    # Update the mesh
    # Create mesh data
    mymesh = bpy.data.meshes.new("CT_Profil")
    # Generate mesh data
    mymesh.from_pydata(verts, edges, faces)
    # Calculate the edges
    mymesh.update(calc_edges=True)
    # Update data
    obj.data = mymesh
    # Make the the object the active one to remove double
    context.scene.objects.active = obj


def boolean_difference():
    ActiveObj = bpy.context.active_object

    if bpy.context.selected_objects[0] != bpy.context.active_object:
        bpy.ops.object.modifier_apply(apply_as='DATA', modifier="CT_SOLIDIFY")
        BoolMod = ActiveObj.modifiers.new("CT_" + bpy.context.selected_objects[0].name, "BOOLEAN")
        BoolMod.object = bpy.context.selected_objects[0]
        BoolMod.operation = "DIFFERENCE"
        BoolMod.solver = bpy.context.scene.CarverSolver
        bpy.context.selected_objects[0].draw_type = 'WIRE'
    else:
        BoolMod = ActiveObj.modifiers.new("CT_" + bpy.context.selected_objects[1].name, "BOOLEAN")
        BoolMod.object = bpy.context.selected_objects[1]
        BoolMod.operation = "DIFFERENCE"
        BoolMod.solver = bpy.context.scene.CarverSolver
        bpy.context.selected_objects[1].draw_type = 'WIRE'


def boolean_union():
    ActiveObj = bpy.context.active_object

    if bpy.context.selected_objects[0] != bpy.context.active_object:
        BoolMod = ActiveObj.modifiers.new("CT_" + bpy.context.selected_objects[0].name, "BOOLEAN")
        BoolMod.object = bpy.context.selected_objects[0]
        BoolMod.operation = "UNION"
        bpy.context.selected_objects[0].draw_type = 'WIRE'
    else:
        BoolMod = ActiveObj.modifiers.new("CT_" + bpy.context.selected_objects[1].name, "BOOLEAN")
        BoolMod.object = bpy.context.selected_objects[1]
        BoolMod.operation = "UNION"
        bpy.context.selected_objects[1].draw_type = 'WIRE'


def Rebool(context, self):
    SelObj_Name = []
    BoolObj = []

    LastObj = context.active_object

    Brush = context.selected_objects[0]
    Brush.draw_type = "WIRE"
    obj = context.selected_objects[1]

    bpy.ops.object.select_all(action='TOGGLE')

    context.scene.objects.active = obj
    obj.draw_type = "SOLID"
    obj.select = True
    bpy.ops.object.duplicate_move(
        OBJECT_OT_duplicate={
            "linked": False,
            "mode": 'TRANSLATION',
        },
        TRANSFORM_OT_translate={
            "value": (0, 0, 0),
            "constraint_axis": (False, False, False),
            "constraint_orientation": 'GLOBAL',
            "mirror": False,
            "proportional": 'DISABLED',
            "proportional_edit_falloff": 'SMOOTH',
            "proportional_size": 1,
            "snap": False,
            "snap_target": 'CLOSEST',
            "snap_point": (0, 0, 0),
            "snap_align": False,
            "snap_normal": (0, 0, 0),
            "gpencil_strokes": False,
            "texture_space": False,
            "remove_on_cancel": False,
            "release_confirm": False,
        },
    )
    LastObjectCreated = context.active_object

    m = LastObjectCreated.modifiers.new("CT_INTERSECT", "BOOLEAN")
    m.operation = "INTERSECT"
    m.solver = context.scene.CarverSolver
    m.object = Brush

    m = obj.modifiers.new("CT_DIFFERENCE", "BOOLEAN")
    m.operation = "DIFFERENCE"
    m.solver = context.scene.CarverSolver
    m.object = Brush

    for mb in LastObj.modifiers:
        if mb.type == 'BEVEL':
            mb.show_viewport = False

    if self.ObjectBrush or self.ProfileBrush:
        LastObjectCreated.show_x_ray = False
        try:
            bpy.ops.object.modifier_apply(apply_as='DATA', modifier="CT_SOLIDIFY")
        except:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            self.report({'ERROR'}, str(exc_value))

    if self.DontApply is False:
        try:
            bpy.ops.object.modifier_apply(apply_as='DATA', modifier="CT_INTERSECT")
        except:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            self.report({'ERROR'}, str(exc_value))

    bpy.ops.object.select_all(action='TOGGLE')

    for mb in LastObj.modifiers:
        if mb.type == 'BEVEL':
            mb.show_viewport = True

    context.scene.objects.active = obj
    obj.select = True
    if self.DontApply is False:
        try:
            bpy.ops.object.modifier_apply(apply_as='DATA', modifier="CT_DIFFERENCE")
        except:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            self.report({'ERROR'}, str(exc_value))

    bpy.ops.object.select_all(action='TOGGLE')

    LastObjectCreated.select = True


def createMeshFromData(self):
    if self.Profils[self.nProfil][0] not in bpy.data.meshes:
        # Create mesh and object
        me = bpy.data.meshes.new(self.Profils[self.nProfil][0])
        # Create mesh from given verts, faces.
        me.from_pydata(self.Profils[self.nProfil][2], [], self.Profils[self.nProfil][3])
        # Update mesh with new data
        me.update()

    if "CT_Profil" not in bpy.data.objects:
        ob = bpy.data.objects.new("CT_Profil", bpy.data.meshes[self.Profils[self.nProfil][0]])
        ob.location = mathutils.Vector((0.0, 0.0, 0.0))

        # Link object to scene and make active
        scn = bpy.context.scene
        scn.objects.link(ob)
        scn.objects.active = ob
        ob.select = True
        ob.location = mathutils.Vector((10000.0, 0.0, 0.0))
        ob.draw_type = "WIRE"

        self.SolidifyPossible = True
    else:
        bpy.data.objects["CT_Profil"].data = bpy.data.meshes[self.Profils[self.nProfil][0]]


def Selection_Save(self):
    self.SavSel = bpy.context.selected_objects.copy()
    self.Sav_ac = bpy.context.active_object


def Selection_Restore(self):
    for o in self.SavSel:
        o.select = True
    bpy.context.scene.objects.active = self.Sav_ac


# Modal Operator
class Carver(bpy.types.Operator):
    bl_idname = "object.carver"
    bl_label = "Carver"
    bl_description = "Cut or create in object mode"
    bl_options = {'REGISTER', 'UNDO'}

    # --------------------------------------------------------------------------------------------------
    @classmethod
    def poll(cls, context):
        ob = None
        if len(context.selected_objects) > 0:
            ob = context.selected_objects[0]
        # Test if selected object or none (for create mode)
        return (
            (ob and ob.type == 'MESH' and context.mode == 'OBJECT') or
            (context.mode == 'OBJECT' and ob is None) or
            (context.mode == 'EDIT_MESH'))
    # --------------------------------------------------------------------------------------------------

    # --------------------------------------------------------------------------------------------------
    def modal(self, context, event):
        PI = 3.14156

        region_types = {'WINDOW', 'UI'}
        win = context.window
        for area in win.screen.areas:
            if area.type in ('VIEW_3D'):
                for region in area.regions:
                    if not region_types or region.type in region_types:
                        region.tag_redraw()

        if event.type in {
                'MIDDLEMOUSE', 'WHEELUPMOUSE', 'WHEELDOWNMOUSE',
                'NUMPAD_1', 'NUMPAD_2', 'NUMPAD_3', 'NUMPAD_4', 'NUMPAD_6',
                'NUMPAD_7', 'NUMPAD_8', 'NUMPAD_9', 'NUMPAD_5'}:
            return {'PASS_THROUGH'}
        try:
            # [Shift]
            self.shift = False
            if event.shift:
                self.shift = True

            # [Ctrl]
            self.ctrl = False
            if event.ctrl:
                self.ctrl = True

            # [Alt]
            self.alt = False
            if event.alt:
                if self.InitPosition is False:
                    # Initialise position variable for start position
                    self.xpos = 0
                    self.ypos = 0
                    self.last_mouse_region_x = event.mouse_region_x
                    self.last_mouse_region_y = event.mouse_region_y
                    self.InitPosition = True
                self.alt = True
            # [Alt] release
            if self.InitPosition and self.alt is False:
                # Update coordonnee
                for i in range(0, len(self.mouse_path)):
                    l = list(self.mouse_path[i])
                    l[0] += self.xpos
                    l[1] += self.ypos
                    self.mouse_path[i] = tuple(l)

                self.xpos = self.ypos = 0
                self.InitPosition = False

            # Mode change (cut type)
            if event.type == 'SPACE' and event.value == 'PRESS':
                if self.ObjectMode or self.ProfileMode:
                    # If grid, remove double with intersect meshes
                    if ((self.nbcol + self.nbrow) > 3):
                        # Go in edit mode mode
                        bpy.ops.object.mode_set(mode='EDIT')
                        # Remove duplicate vertices
                        bpy.ops.mesh.remove_doubles()
                        # Return in object mode
                        bpy.ops.object.mode_set(mode='OBJECT')

                    if self.alt:
                        # Save selected objects
                        self.SavSel = context.selected_objects.copy()
                        if len(context.selected_objects) > 0:
                            bpy.ops.object.select_all(action='TOGGLE')

                        if self.ObjectMode:
                            SelectObject(self, self.ObjectBrush)
                        else:
                            SelectObject(self, self.ProfileBrush)
                        duplicateObject(self)
                    else:
                        # Brush Cut
                        self.Cut()
                        # Save selected objects
                        if self.ObjectMode:
                            if len(self.ObjectBrush.children) > 0:
                                self.SavSel = context.selected_objects.copy()
                                if len(context.selected_objects) > 0:
                                    bpy.ops.object.select_all(action='TOGGLE')

                                if self.ObjectMode:
                                    SelectObject(self, self.ObjectBrush)
                                else:
                                    SelectObject(self, self.ProfileBrush)
                                duplicateObject(self)

                        UndoListUpdate(self)

                    # Save cursor position
                    self.SavMousePos = self.CurLoc
                else:
                    if self.bDone is False:
                        # Cut Mode
                        self.CutMode += 1
                        if self.CutMode > 2:
                            self.CutMode = 0
                    else:
                        if self.CutMode == LINE:
                            # Cuts creation
                            CreateCutLine(self, context)
                            if self.CreateMode:
                                # Object creation
                                self.CreateGeometry()
                                bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
                                # Cursor Snap
                                context.scene.DepthCursor = self.snapCursor
                                # Object Instantiate
                                context.scene.OInstanciate = self.Instantiate
                                # Random rotation
                                context.scene.ORandom = self.RandomRotation

                                return {'FINISHED'}
                            else:
                                # Cut
                                self.Cut()
                                UndoListUpdate(self)

            # Object creation
            if event.type == context.scene.Key_Create and event.value == 'PRESS':
                if self.ExclusiveCreateMode is False:
                    self.CreateMode = not self.CreateMode

            # Auto Bevel Update
            if event.type == context.scene.Key_Update and event.value == 'PRESS':
                self.Auto_BevelUpdate = not self.Auto_BevelUpdate

            # Boolean operation type
            if event.type == context.scene.Key_Bool and event.value == 'PRESS':
                if (self.ProfileMode is True) or (self.ObjectMode is True):
                    if self.BoolOps == DIFFERENCE:
                        self.BoolOps = UNION
                    else:
                        self.BoolOps = DIFFERENCE

            # Brush Mode
            if event.type == context.scene.Key_Brush and event.value == 'PRESS':
                self.DontApply = False
                if (self.ProfileMode is False) and (self.ObjectMode is False):
                    self.ProfileMode = True
                else:
                    self.ProfileMode = False
                    if self.ObjectBrush is not None:
                        if self.ObjectMode is False:
                            self.ObjectMode = True
                            self.BrushSolidify = False
                            self.CList = self.OB_List

                            if "CT_Profil" in bpy.data.objects:
                                Selection_Save(self)
                                bpy.ops.object.select_all(action='DESELECT')
                                bpy.data.objects["CT_Profil"].select = True
                                context.scene.objects.active = bpy.data.objects["CT_Profil"]
                                bpy.ops.object.delete(use_global=False)
                                Selection_Restore(self)

                            context.scene.nProfile = self.nProfil

                        else:
                            self.ObjectMode = False
                    else:
                        self.BrushSolidify = False

                        if "CT_Profil" in bpy.data.objects:
                            Selection_Save(self)
                            bpy.ops.object.select_all(action='DESELECT')
                            bpy.data.objects["CT_Profil"].select = True
                            context.scene.objects.active = bpy.data.objects["CT_Profil"]
                            bpy.ops.object.delete(use_global=False)
                            Selection_Restore(self)

                if self.ProfileMode:
                    createMeshFromData(self)
                    self.ProfileBrush = bpy.data.objects["CT_Profil"]
                    Selection_Save(self)
                    self.BrushSolidify = True

                    bpy.ops.object.select_all(action='TOGGLE')
                    self.ProfileBrush.select = True
                    context.scene.objects.active = self.ProfileBrush
                    # Set xRay
                    self.ProfileBrush.show_x_ray = True

                    bpy.ops.object.modifier_add(type='SOLIDIFY')
                    context.object.modifiers["Solidify"].name = "CT_SOLIDIFY"

                    context.object.modifiers["CT_SOLIDIFY"].thickness = 0.1

                    Selection_Restore(self)

                    self.CList = self.CurrentSelection
                else:
                    if self.ObjectBrush is not None:
                        if self.ObjectMode is False:
                            if self.ObjectBrush is not None:
                                self.ObjectBrush.location = self.InitBrushPosition
                                self.ObjectBrush.scale = self.InitBrushScale
                                self.ObjectBrush.rotation_quaternion = self.InitBrushQRotation
                                self.ObjectBrush.rotation_euler = self.InitBrushERotation
                                self.ObjectBrush.draw_type = self.ObjectBrush_DT
                                self.ObjectBrush.show_x_ray = self.XRay

                                # Remove solidify modifier
                                Selection_Save(self)
                                self.BrushSolidify = False

                                bpy.ops.object.select_all(action='TOGGLE')
                                self.ObjectBrush.select = True
                                context.scene.objects.active = self.ObjectBrush

                                bpy.ops.object.modifier_remove(modifier="CT_SOLIDIFY")

                                Selection_Restore(self)
                        else:

                            if self.Solidify_Active_Start:
                                Selection_Save(self)
                                self.BrushSolidify = True
                                self.SolidifyPossible = True
                                bpy.ops.object.select_all(action='TOGGLE')
                                self.ObjectBrush.select = True
                                context.scene.objects.active = self.ObjectBrush
                                # Set xRay
                                self.ObjectBrush.show_x_ray = True
                                bpy.ops.object.modifier_add(type='SOLIDIFY')
                                context.object.modifiers["Solidify"].name = "CT_SOLIDIFY"
                                context.object.modifiers["CT_SOLIDIFY"].thickness = 0.1
                                Selection_Restore(self)

            # Help display
            if event.type == context.scene.Key_Help and event.value == 'PRESS':
                self.AskHelp = not self.AskHelp

            # Instantiate object
            if event.type == context.scene.Key_Instant and event.value == 'PRESS':
                self.Instantiate = not self.Instantiate

            # Close polygonal shape
            if event.type == context.scene.Key_Close and event.value == 'PRESS':
                if self.CreateMode:
                    self.Closed = not self.Closed

            if event.type == context.scene.Key_Apply and event.value == 'PRESS':
                self.DontApply = not self.DontApply

            if event.type == context.scene.Key_Solver and event.value == 'PRESS':
                if context.scene.CarverSolver == "CARVE":
                    context.scene.CarverSolver = "BMESH"
                else:
                    context.scene.CarverSolver = "CARVE"

            # Scale object
            if event.type == context.scene.Key_Scale and event.value == 'PRESS':
                if self.ObjectScale is False:
                    self.am = event.mouse_region_x, event.mouse_region_y
                self.ObjectScale = True

            # Grid : Add column
            if event.type == 'UP_ARROW' and event.value == 'PRESS':
                self.nbcol += 1
                update_grid(self, context)

            # Grid : Add row
            elif event.type == 'RIGHT_ARROW' and event.value == 'PRESS':
                self.nbrow += 1
                update_grid(self, context)

            # Grid : Delete column
            elif event.type == 'DOWN_ARROW' and event.value == 'PRESS':
                self.nbcol -= 1
                update_grid(self, context)

            # Grid : Delete row
            elif event.type == 'LEFT_ARROW' and event.value == 'PRESS':
                self.nbrow -= 1
                update_grid(self, context)

            # Grid : Scale gap between columns
            if event.type == context.scene.Key_Gapy and event.value == 'PRESS':
                if self.GridScaleX is False:
                    self.am = event.mouse_region_x, event.mouse_region_y
                self.GridScaleX = True

            # Grid : Scale gap between rows
            if event.type == context.scene.Key_Gapx and event.value == 'PRESS':
                if self.GridScaleY is False:
                    self.am = event.mouse_region_x, event.mouse_region_y
                self.GridScaleY = True

            # Cursor depth or solidify pattern
            if event.type == context.scene.Key_Depth and event.value == 'PRESS':
                if (self.ObjectMode is False) and (self.ProfileMode is False):
                    self.snapCursor = not self.snapCursor
                else:
                    # Solidify

                    if (self.ObjectMode or self.ProfileMode) and (self.SolidifyPossible):
                        solidify = True

                        if self.ObjectMode:
                            z = self.ObjectBrush.data.vertices[0].co.z
                            ErrorMarge = 0.01
                            for v in self.ObjectBrush.data.vertices:
                                if abs(v.co.z - z) > ErrorMarge:
                                    solidify = False
                                    self.CarveDepth = True
                                    self.am = event.mouse_region_x, event.mouse_region_y
                                    break

                        if solidify:
                            if self.ObjectMode:
                                for mb in self.ObjectBrush.modifiers:
                                    if mb.type == 'SOLIDIFY':
                                        AlreadySoldify = True
                            else:
                                for mb in self.ProfileBrush.modifiers:
                                    if mb.type == 'SOLIDIFY':
                                        AlreadySoldify = True

                            if AlreadySoldify is False:
                                Selection_Save(self)
                                self.BrushSolidify = True

                                bpy.ops.object.select_all(action='TOGGLE')
                                if self.ObjectMode:
                                    self.ObjectBrush.select = True
                                    context.scene.objects.active = self.ObjectBrush
                                    # Active le xray
                                    self.ObjectBrush.show_x_ray = True
                                else:
                                    self.ProfileBrush.select = True
                                    context.scene.objects.active = self.ProfileBrush
                                    # Active le xray
                                    self.ProfileBrush.show_x_ray = True

                                bpy.ops.object.modifier_add(type='SOLIDIFY')
                                context.object.modifiers["Solidify"].name = "CT_SOLIDIFY"

                                context.object.modifiers["CT_SOLIDIFY"].thickness = 0.1

                                Selection_Restore(self)

                            self.WidthSolidify = not self.WidthSolidify
                            self.am = event.mouse_region_x, event.mouse_region_y

            if event.type == context.scene.Key_BrushDepth and event.value == 'PRESS':
                if self.ObjectMode:
                    self.CarveDepth = False

                    self.BrushDepth = True
                    self.am = event.mouse_region_x, event.mouse_region_y

            # Random rotation
            if event.type == 'R' and event.value == 'PRESS':
                self.RandomRotation = not self.RandomRotation

            # Undo
            if event.type == 'Z' and event.value == 'PRESS':
                if self.ctrl:
                    if (self.CutMode == LINE) and (self.bDone):
                        if len(self.mouse_path) > 1:
                            self.mouse_path[len(self.mouse_path) - 1:] = []
                    else:
                        Undo(self)

            # Mouse move
            if event.type == 'MOUSEMOVE':

                if self.ObjectMode or self.ProfileMode:
                    fac = 50.0
                    if self.shift:
                        fac = 500.0
                    if self.WidthSolidify:
                        if self.ObjectMode:
                            bpy.data.objects[self.ObjectBrush.name].modifiers[
                                "CT_SOLIDIFY"].thickness += (event.mouse_region_x - self.am[0]) / fac
                        elif self.ProfileMode:
                            bpy.data.objects[self.ProfileBrush.name].modifiers[
                                "CT_SOLIDIFY"].thickness += (event.mouse_region_x - self.am[0]) / fac
                        self.am = event.mouse_region_x, event.mouse_region_y
                    elif self.CarveDepth:
                        for v in self.ObjectBrush.data.vertices:
                            v.co.z += (event.mouse_region_x - self.am[0]) / fac
                        self.am = event.mouse_region_x, event.mouse_region_y
                    elif self.BrushDepth:
                        self.BrushDepthOffset += (event.mouse_region_x - self.am[0]) / fac
                        self.am = event.mouse_region_x, event.mouse_region_y
                    else:
                        if (self.GridScaleX):
                            self.gapx += (event.mouse_region_x - self.am[0]) / 50
                            self.am = event.mouse_region_x, event.mouse_region_y
                            update_grid(self, context)
                            return {'RUNNING_MODAL'}

                        elif (self.GridScaleY):
                            self.gapy += (event.mouse_region_x - self.am[0]) / 50
                            self.am = event.mouse_region_x, event.mouse_region_y
                            update_grid(self, context)
                            return {'RUNNING_MODAL'}

                        elif self.ObjectScale:
                            self.ascale = -(event.mouse_region_x - self.am[0])
                            self.am = event.mouse_region_x, event.mouse_region_y

                            if self.ObjectMode:
                                self.ObjectBrush.scale.x -= float(self.ascale) / 150.0
                                if self.ObjectBrush.scale.x <= 0.0:
                                    self.ObjectBrush.scale.x = 0.0
                                self.ObjectBrush.scale.y -= float(self.ascale) / 150.0
                                if self.ObjectBrush.scale.y <= 0.0:
                                    self.ObjectBrush.scale.y = 0.0
                                self.ObjectBrush.scale.z -= float(self.ascale) / 150.0
                                if self.ObjectBrush.scale.z <= 0.0:
                                    self.ObjectBrush.scale.z = 0.0

                            elif self.ProfileMode:
                                if self.ProfileBrush is not None:
                                    self.ProfileBrush.scale.x -= float(self.ascale) / 150.0
                                    self.ProfileBrush.scale.y -= float(self.ascale) / 150.0
                                    self.ProfileBrush.scale.z -= float(self.ascale) / 150.0

                        else:
                            if self.LMB:
                                if self.ctrl:
                                    self.aRotZ = - \
                                        ((int((event.mouse_region_x - self.xSavMouse) / 10.0) * PI / 4.0) * 25.0)
                                else:
                                    self.aRotZ -= event.mouse_region_x - self.am[0]
                                self.ascale = 0.0

                                self.am = event.mouse_region_x, event.mouse_region_y
                            else:
                                vBack = Pick(context, event, self)
                                if vBack[0] is not None:
                                    self.ShowCursor = True
                                    NormalObject = mathutils.Vector((0.0, 0.0, 1.0))
                                    qR = RBenVe(NormalObject, vBack[1])
                                    self.qRot = vBack[3] * qR
                                    Pos = vBack[0]
                                    MoveCursor(qR, vBack[0], self)
                                    self.SavCurLoc = vBack[0]
                                    if self.ctrl:
                                        if self.SavMousePos is not None:
                                            xEcart = abs(self.SavMousePos.x - self.SavCurLoc.x)
                                            yEcart = abs(self.SavMousePos.y - self.SavCurLoc.y)
                                            zEcart = abs(self.SavMousePos.z - self.SavCurLoc.z)
                                            if (xEcart > yEcart) and (xEcart > zEcart):
                                                self.CurLoc = mathutils.Vector(
                                                    (vBack[0].x, self.SavMousePos.y, self.SavMousePos.z))
                                            if (yEcart > xEcart) and (yEcart > zEcart):
                                                self.CurLoc = mathutils.Vector(
                                                    (self.SavMousePos.x, vBack[0].y, self.SavMousePos.z))
                                            if (zEcart > xEcart) and (zEcart > yEcart):
                                                self.CurLoc = mathutils.Vector(
                                                    (self.SavMousePos.x, self.SavMousePos.y, vBack[0].z))
                                            else:
                                                self.CurLoc = vBack[0]
                                    else:
                                        self.CurLoc = vBack[0]
                else:
                    if self.bDone:
                        if self.alt is False:
                            if self.bDone:
                                if self.ctrl and (self.CutMode == LINE):
                                    # Incremental mode
                                    coord = list(self.mouse_path[len(self.mouse_path) - 1])
                                    coord[0] = int(
                                                self.mouse_path[len(self.mouse_path) - 2][0] +
                                                int((event.mouse_region_x -
                                                    self.mouse_path[len(self.mouse_path) - 2][0]
                                                    ) / self.Increment) * self.Increment
                                                )
                                    coord[1] = int(
                                                self.mouse_path[len(self.mouse_path) - 2][1] +
                                                int((event.mouse_region_y -
                                                    self.mouse_path[len(self.mouse_path) - 2][1]
                                                    ) / self.Increment) * self.Increment
                                                )
                                    self.mouse_path[len(self.mouse_path) - 1] = tuple(coord)
                                else:
                                    if len(self.mouse_path) > 0:
                                        self.mouse_path[len(self.mouse_path) -
                                                        1] = (event.mouse_region_x, event.mouse_region_y)
                        else:
                            # [ALT] press, update position
                            self.xpos += (event.mouse_region_x - self.last_mouse_region_x)
                            self.ypos += (event.mouse_region_y - self.last_mouse_region_y)

                            self.last_mouse_region_x = event.mouse_region_x
                            self.last_mouse_region_y = event.mouse_region_y

            elif event.type == 'LEFTMOUSE' and event.value == 'PRESS':
                if self.ObjectMode or self.ProfileMode:
                    if self.LMB is False:
                        vBack = Pick(context, event, self)
                        if vBack[0] is not None:
                            NormalObject = mathutils.Vector((0.0, 0.0, 1.0))
                            self.aqR = RBenVe(NormalObject, vBack[1])
                            self.qRot = vBack[3] * self.aqR
                        self.am = event.mouse_region_x, event.mouse_region_y
                        self.xSavMouse = event.mouse_region_x

                        if self.ctrl:
                            self.nRotZ = int((self.aRotZ / 25.0) / (PI / 4.0))
                            self.aRotZ = self.nRotZ * (PI / 4.0) * 25.0

                    self.LMB = True

            # LEFTMOUSE
            elif event.type == 'LEFTMOUSE' and event.value == 'RELEASE':
                if self.ObjectMode or self.ProfileMode:
                    # Rotation and scale
                    self.LMB = False
                    if self.ObjectScale is True:
                        self.ObjectScale = False

                    if self.GridScaleX is True:
                        self.GridScaleX = False

                    if self.GridScaleY is True:
                        self.GridScaleY = False

                    if self.WidthSolidify:
                        self.WidthSolidify = False

                    if self.CarveDepth is True:
                        self.CarveDepth = False

                    if self.BrushDepth is True:
                        self.BrushDepth = False

                else:
                    if self.bDone is False:
                        if self.ctrl:
                            Picking(context, event)
                        else:
                            if self.CutMode == LINE:
                                if self.bDone is False:
                                    self.mouse_path.clear()
                                    self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))
                                    self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))
                            else:
                                self.mouse_path[0] = (event.mouse_region_x, event.mouse_region_y)
                                self.mouse_path[1] = (event.mouse_region_x, event.mouse_region_y)
                            self.bDone = True
                    else:
                        if self.CutMode != LINE:
                            # Cut creation
                            if self.CutMode == RECTANGLE:
                                CreateCutSquare(self, context)
                            if self.CutMode == CIRCLE:
                                CreateCutCircle(self, context)
                            if self.CutMode == LINE:
                                CreateCutLine(self, context)

                            if self.CreateMode:
                                # Object creation
                                self.CreateGeometry()
                                bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
                                # Depth Cursor
                                context.scene.DepthCursor = self.snapCursor
                                # Instantiate object
                                context.scene.OInstanciate = self.Instantiate
                                # Random rotation
                                context.scene.ORandom = self.RandomRotation
                                # Apply operation
                                context.scene.DontApply = self.DontApply

                                # if Object mode, set intiale state
                                if self.ObjectBrush is not None:
                                    self.ObjectBrush.location = self.InitBrushPosition
                                    self.ObjectBrush.scale = self.InitBrushScale
                                    self.ObjectBrush.rotation_quaternion = self.InitBrushQRotation
                                    self.ObjectBrush.rotation_euler = self.InitBrushERotation
                                    self.ObjectBrush.draw_type = self.ObjectBrush_DT
                                    self.ObjectBrush.show_x_ray = self.XRay

                                    # remove solidify
                                    Selection_Save(self)
                                    self.BrushSolidify = False

                                    bpy.ops.object.select_all(action='TOGGLE')
                                    self.ObjectBrush.select = True
                                    context.scene.objects.active = self.ObjectBrush

                                    bpy.ops.object.modifier_remove(modifier="CT_SOLIDIFY")

                                    Selection_Restore(self)

                                    context.scene.nProfile = self.nProfil

                                return {'FINISHED'}
                            else:
                                # Cut
                                self.Cut()
                                UndoListUpdate(self)

                        else:
                            # Line
                            self.mouse_path.append((event.mouse_region_x, event.mouse_region_y))

            # Change circle subdivisions
            elif (event.type == 'COMMA' and event.value == 'PRESS') or \
                        (event.type == context.scene.Key_Subrem and event.value == 'PRESS'):
                if self.ProfileMode:
                    self.nProfil += 1
                    if self.nProfil >= self.MaxProfil:
                        self.nProfil = 0
                    createMeshFromData(self)
                # Circle rotation
                if self.CutMode == CIRCLE:
                    if self.ctrl:
                        self.stepRotation += 1
                    else:
                        self.step += 1
                        if self.step >= len(self.stepAngle):
                            self.step = len(self.stepAngle) - 1
            elif (event.type == 'PERIOD' and event.value == 'PRESS') or \
                        (event.type == context.scene.Key_Subadd and event.value == 'PRESS'):
                if self.ProfileMode:
                    self.nProfil -= 1
                    if self.nProfil < 0:
                        self.nProfil = self.MaxProfil - 1
                    createMeshFromData(self)
                if self.CutMode == CIRCLE:
                    if self.ctrl:
                        self.stepRotation -= 1
                    else:
                        if self.step > 0:
                            self.step -= 1

            # Quit
            elif event.type in {'RIGHTMOUSE', 'ESC'}:
                # Depth Cursor
                context.scene.DepthCursor = self.snapCursor
                # Instantiate object
                context.scene.OInstanciate = self.Instantiate
                # Random Rotation
                context.scene.ORandom = self.RandomRotation
                # Apply boolean operation
                context.scene.DontApply = self.DontApply

                # Reset Object
                if self.ObjectBrush is not None:
                    self.ObjectBrush.location = self.InitBrushPosition
                    self.ObjectBrush.scale = self.InitBrushScale
                    self.ObjectBrush.rotation_quaternion = self.InitBrushQRotation
                    self.ObjectBrush.rotation_euler = self.InitBrushERotation
                    self.ObjectBrush.draw_type = self.ObjectBrush_DT
                    self.ObjectBrush.show_x_ray = self.XRay

                    # Remove solidify modifier
                    Selection_Save(self)
                    self.BrushSolidify = False

                    bpy.ops.object.select_all(action='TOGGLE')
                    self.ObjectBrush.select = True
                    context.scene.objects.active = self.ObjectBrush

                    bpy.ops.object.modifier_remove(modifier="CT_SOLIDIFY")

                    bpy.ops.object.select_all(action='TOGGLE')

                    Selection_Restore(self)

                if "CT_Profil" in bpy.data.objects:
                    Selection_Save(self)
                    bpy.ops.object.select_all(action='DESELECT')
                    bpy.data.objects["CT_Profil"].select = True
                    context.scene.objects.active = bpy.data.objects["CT_Profil"]
                    bpy.ops.object.delete(use_global=False)
                    Selection_Restore(self)

                context.scene.objects.active = self.CurrentActive

                context.scene.nProfile = self.nProfil

                bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')
                # Remove Copy Object Brush
                if bpy.data.objects.get("CarverBrushCopy") is not None:
                    brush = bpy.data.objects["CarverBrushCopy"]
                    self.ObjectBrush.data = bpy.data.meshes[brush.data.name]
                    bpy.ops.object.select_all(action='DESELECT')
                    bpy.data.objects["CarverBrushCopy"].select = True
                    bpy.ops.object.delete()

                return {'FINISHED'}

            return {'RUNNING_MODAL'}

        except:
            print("\n[Carver MT ERROR]\n")

            import traceback
            traceback.print_exc()

            context.window.cursor_modal_set("DEFAULT")
            context.area.header_text_set()
            bpy.types.SpaceView3D.draw_handler_remove(self._handle, 'WINDOW')

            self.report({'WARNING'},
                        "Operation finished. Failure during Carving (Check the console for more info)")

            return {'FINISHED'}

    # --------------------------------------------------------------------------------------------------

    # --------------------------------------------------------------------------------------------------
    def invoke(self, context, event):
        if context.area.type == 'VIEW_3D':
            if context.mode == 'EDIT_MESH':
                bpy.ops.object.mode_set(mode='OBJECT')

            win = context.window

            # Get default patterns
            self.Profils = []
            for p in Profils:
                self.Profils.append((p[0], p[1], p[2], p[3]))

            for o in context.scene.objects:
                if not o.name.startswith(context.scene.ProfilePrefix):
                    continue

                # In-scene profiles may have changed, remove them to refresh
                for m in bpy.data.meshes:
                    if m.name.startswith(context.scene.ProfilePrefix):
                        bpy.data.meshes.remove(m)

                vertices = []
                for v in o.data.vertices:
                    vertices.append((v.co.x, v.co.y, v.co.z))

                faces = []
                for f in o.data.polygons:
                    face = []

                    for v in f.vertices:
                        face.append(v)

                    faces.append(face)

                self.Profils.append(
                            (o.name,
                            mathutils.Vector((o.location.x, o.location.y, o.location.z)),
                            vertices, faces)
                            )

            self.nProfil = context.scene.nProfile
            self.MaxProfil = len(self.Profils)

            # reset selected profile if last profile exceeds length of array
            if self.nProfil >= self.MaxProfil:
                self.nProfil = context.scene.nProfile = 0

            # Save selection
            self.CurrentSelection = context.selected_objects.copy()
            self.CurrentActive = context.active_object
            self.SavSel = context.selected_objects.copy()
            self.Sav_ac = None

            args = (self, context)

            self._handle = bpy.types.SpaceView3D.draw_handler_add(draw_callback_px, args, 'WINDOW', 'POST_PIXEL')

            self.mouse_path = [(0, 0), (0, 0)]

            self.shift = False
            self.ctrl = False
            self.alt = False

            self.bDone = False

            self.DontApply = context.scene.DontApply
            self.Auto_BevelUpdate = True

            # Cut type (Rectangle, Circle, Line)
            self.CutMode = 0
            self.BoolOps = DIFFERENCE

            # Circle variables
            self.stepAngle = [2, 4, 5, 6, 9, 10, 15, 20, 30, 40, 45, 60, 72, 90]
            self.step = 4
            self.stepRotation = 0

            # Primitives Position
            self.xpos = 0
            self.ypos = 0
            self.InitPosition = False

            # Line Increment
            self.Increment = 15
            # Close polygonal shape
            self.Closed = False

            # Depth Cursor
            self.snapCursor = context.scene.DepthCursor

            # Help
            self.AskHelp = False

            # Working object
            self.OpsObj = context.active_object

            # Create mode
            self.CreateMode = False
            self.ExclusiveCreateMode = False
            if len(context.selected_objects) == 0:
                self.ExclusiveCreateMode = True
                self.CreateMode = True

            # Rebool forced (cut line)
            self.ForceRebool = False

            self.ViewVector = mathutils.Vector()

            self.CurrentObj = None

            # Brush
            self.BrushSolidify = False
            self.WidthSolidify = False
            self.CarveDepth = False
            self.BrushDepth = False
            self.BrushDepthOffset = 0.0

            self.ObjectScale = False

            self.CircleListRaw = []
            self.CLR_C = []
            self.CurLoc = mathutils.Vector((0.0, 0.0, 0.0))
            self.SavCurLoc = mathutils.Vector((0.0, 0.0, 0.0))
            self.CRadius = 1.0
            CreatePrimitive(self, 10.0, 1.0)
            self.VertsList = []
            self.FacesList = []

            self.am = -1, -1
            self.SavMousePos = None
            self.xSavMouse = 0

            self.ascale = 0
            self.aRotZ = 0
            self.nRotZ = 0
            self.aqR = None
            self.qRot = None

            self.RandomRotation = context.scene.ORandom

            self.ShowCursor = True

            self.ObjectMode = False
            self.ProfileMode = False
            self.Instantiate = context.scene.OInstanciate

            self.ProfileBrush = None
            self.ObjectBrush = None
            self.InitBrushPosition = None
            self.InitBrushScale = None
            self.InitBrushQRotation = None
            self.InitBrushERotation = None
            self.InitBrushARotation = None

            self.ObjectBrush_DT = "WIRE"
            self.XRay = False

            # Grid mesh
            self.nbcol = 1
            self.nbrow = 1
            self.gapx = 0
            self.gapy = 0
            self.scale_x = 1
            self.scale_y = 1

            self.GridScaleX = False
            self.GridScaleY = False

            if len(context.selected_objects) > 1:
                self.ObjectBrush = context.active_object

                # Copy the brush object
                ob = bpy.data.objects.new("CarverBrushCopy", context.object.data.copy())
                ob.location = self.ObjectBrush.location
                scene = context.scene
                scene.objects.link(ob)
                scene.update()

                # Get default variables
                self.InitBrushPosition = self.ObjectBrush.location.copy()
                self.InitBrushScale = self.ObjectBrush.scale.copy()
                self.InitBrushQRotation = self.ObjectBrush.rotation_quaternion.copy()
                self.InitBrushERotation = self.ObjectBrush.rotation_euler.copy()
                self.ObjectBrush_DT = self.ObjectBrush.draw_type
                self.XRay = self.ObjectBrush.show_x_ray
                # Test if flat object
                z = self.ObjectBrush.data.vertices[0].co.z
                ErrorMarge = 0.01
                self.Solidify_Active_Start = True
                for v in self.ObjectBrush.data.vertices:
                    if abs(v.co.z - z) > ErrorMarge:
                        self.Solidify_Active_Start = False
                        break
                self.SolidifyPossible = False

            self.CList = []
            self.OPList = []
            self.RList = []
            self.OB_List = []

            for ent in context.selected_objects:
                if ent != self.ObjectBrush:
                    self.OB_List.append(ent)

            # Left button
            self.LMB = False

            # Undo Variables
            self.undo_index = 0
            self.undo_limit = context.user_preferences.edit.undo_steps
            self.undo_list = []

            # Boolean operations type
            self.BooleanType = 0

            self.UList = []
            self.UList_Index = -1
            self.UndoOps = []

            context.window_manager.modal_handler_add(self)
            return {'RUNNING_MODAL'}
        else:
            self.report({'WARNING'}, "View3D not found, cannot run operator")
            return {'CANCELLED'}
    # --------------------------------------------------------------------------------------------------

    # --------------------------------------------------------------------------------------------------
    def CreateGeometry(self):
        context = bpy.context

        region_id = context.region.id

        bLocalView = False
        for area in context.screen.areas:
            if area.type == 'VIEW_3D':
                if area.spaces[0].local_view is not None:
                    bLocalView = True

        if bLocalView:
            bpy.ops.view3d.localview()

        if self.ExclusiveCreateMode:
            # Default width
            objBBDiagonal = 0.5
        else:
            ActiveObj = self.CurrentSelection[0]
            if ActiveObj is not None:
                objBBDiagonal = objDiagonal(ActiveObj) / 4
        subdivisions = 2

        if len(context.selected_objects) > 0:
            bpy.ops.object.select_all(action='TOGGLE')

        context.scene.objects.active = self.CurrentObj

        bpy.data.objects[self.CurrentObj.name].select = True
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')

        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.select_mode(type="EDGE")
        if self.snapCursor is False:
            bpy.ops.transform.translate(value=self.ViewVector * objBBDiagonal * subdivisions)
        bpy.ops.mesh.extrude_region_move(
            TRANSFORM_OT_translate={"value": -self.ViewVector * objBBDiagonal * subdivisions * 2})

        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.normals_make_consistent()
        bpy.ops.object.mode_set(mode='OBJECT')

        saved_location_0 = context.scene.cursor_location.copy()
        bpy.ops.view3d.snap_cursor_to_active()
        saved_location = context.scene.cursor_location.copy()
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        context.scene.cursor_location = saved_location
        bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
        context.scene.cursor_location = saved_location_0

        bpy.data.objects[self.CurrentObj.name].select = True
        bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')

        for o in self.SavSel:
            bpy.data.objects[o.name].select = True

        if bLocalView:
            bpy.ops.view3d.localview()

        self.bDone = False
        self.mouse_path.clear()
        self.mouse_path = [(0, 0), (0, 0)]
    # --------------------------------------------------------------------------------------------------

    # --------------------------------------------------------------------------------------------------
    def Cut(self):
        context = bpy.context

        UNDO = []

        # Local view ?
        bLocalView = False
        for area in context.screen.areas:
            if area.type == 'VIEW_3D':
                if area.spaces[0].local_view is not None:
                    bLocalView = True

        if bLocalView:
            bpy.ops.view3d.localview()

        # Save cursor position
        CursorLocation = context.scene.cursor_location.copy()

        ActiveObjList = []
        if (self.ObjectMode is False) and (self.ProfileMode is False):
            objBBDiagonal = objDiagonal(self.CurrentSelection[0])
            subdivisions = 32
            if self.DontApply:
                subdivisions = 1

            # Get selected objects
            ActiveObjList = context.selected_objects.copy()

            bpy.ops.object.select_all(action='TOGGLE')

            context.scene.objects.active = self.CurrentObj

            bpy.data.objects[self.CurrentObj.name].select = True
            bpy.ops.object.origin_set(type='ORIGIN_GEOMETRY')

            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.select_mode(type="EDGE")
            if (self.snapCursor is False) or (self.ForceRebool):
                bpy.ops.transform.translate(value=self.ViewVector * objBBDiagonal * subdivisions)
            bpy.ops.mesh.extrude_region_move(
                TRANSFORM_OT_translate={"value": -self.ViewVector * objBBDiagonal * subdivisions * 2})
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.mesh.normals_make_consistent()
            bpy.ops.object.mode_set(mode='OBJECT')
        else:
            # Create liste
            if self.ObjectMode:
                for o in self.CurrentSelection:
                    if o != self.ObjectBrush:
                        ActiveObjList.append(o)
                self.CurrentObj = self.ObjectBrush
            else:
                ActiveObjList = self.CurrentSelection
                self.CurrentObj = self.ProfileBrush

        for o in self.CurrentSelection:
            UndoAdd(self, "MESH", o)

        # List objects create with rebool
        lastSelected = []

        for ActiveObj in ActiveObjList:
            context.scene.cursor_location = CursorLocation

            if len(context.selected_objects) > 0:
                bpy.ops.object.select_all(action='TOGGLE')

            # Testif intitiale object has bevel
            BevelAO = False
            for obj in ActiveObjList:
                for mb in obj.modifiers:
                    if mb.type == 'BEVEL':
                        BevelAO = True

            # Select cut object
            bpy.data.objects[self.CurrentObj.name].select = True
            context.scene.objects.active = self.CurrentObj

            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='SELECT')
            bpy.ops.object.mode_set(mode='OBJECT')

            # Select object to cut
            bpy.data.objects[ActiveObj.name].select = True
            context.scene.objects.active = ActiveObj

            bpy.ops.object.mode_set(mode='EDIT')
            bpy.ops.mesh.select_all(action='DESELECT')
            bpy.ops.object.mode_set(mode='OBJECT')

            # Boolean operation
            if (self.shift is False) and (self.ForceRebool is False):
                if self.ObjectMode or self.ProfileMode:
                    if self.BoolOps == UNION:
                        # Union
                        boolean_union()
                    else:
                        # Cut object
                        boolean_difference()
                else:
                    # Cut
                    boolean_difference()

                # Apply booleans
                if self.DontApply is False:
                    BMname = "CT_" + self.CurrentObj.name
                    for mb in ActiveObj.modifiers:
                        if (mb.type == 'BOOLEAN') and (mb.name == BMname):
                            try:
                                bpy.ops.object.modifier_apply(apply_as='DATA', modifier=BMname)
                            except:
                                bpy.ops.object.modifier_remove(modifier=BMname)
                                exc_type, exc_value, exc_traceback = sys.exc_info()
                                self.report({'ERROR'}, str(exc_value))

                bpy.ops.object.select_all(action='TOGGLE')
            else:
                if self.ObjectMode or self.ProfileMode:
                    for mb in self.CurrentObj.modifiers:
                        if (mb.type == 'SOLIDIFY') and (mb.name == "CT_SOLIDIFY"):
                            try:
                                bpy.ops.object.modifier_apply(apply_as='DATA', modifier="CT_SOLIDIFY")
                            except:
                                exc_type, exc_value, exc_traceback = sys.exc_info()
                                self.report({'ERROR'}, str(exc_value))

                # Rebool
                Rebool(context, self)
                # Test if not empty object
                if context.selected_objects[0]:
                    rebool_RT = context.selected_objects[0]
                    if len(rebool_RT.data.vertices) > 0:
                        # Create Bevel for new objects
                        if BevelAO:
                            CreateBevel(context, context.selected_objects[0])
                        UndoAdd(self, "REBOOL", context.selected_objects[0])

                        context.scene.cursor_location = ActiveObj.location
                        bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
                    else:
                        bpy.ops.object.delete(use_global=False)

                context.scene.cursor_location = CursorLocation

                if self.ObjectMode:
                    context.scene.objects.active = self.ObjectBrush
                if self.ProfileMode:
                    context.scene.objects.active = self.ProfileBrush

            if self.DontApply is False:
                # Apply booleans
                BMname = "CT_" + self.CurrentObj.name
                for mb in ActiveObj.modifiers:
                    if (mb.type == 'BOOLEAN') and (mb.name == BMname):
                        try:
                            bpy.ops.object.modifier_apply(apply_as='DATA', modifier=BMname)
                        except:
                            bpy.ops.object.modifier_remove(modifier=BMname)
                            exc_type, exc_value, exc_traceback = sys.exc_info()
                            self.report({'ERROR'}, str(exc_value))
                # Get new objects created with rebool operations
                if len(context.selected_objects) > 0:
                    if self.shift is True:
                        # Get the last object selected
                        lastSelected.append(context.selected_objects[0])

        context.scene.cursor_location = CursorLocation

        if self.DontApply is False:
            # Remove cut object
            if (self.ObjectMode is False) and (self.ProfileMode is False):
                if len(context.selected_objects) > 0:
                    bpy.ops.object.select_all(action='TOGGLE')
                bpy.data.objects[self.CurrentObj.name].select = True
                cname = self.CurrentObj.name
                bpy.ops.object.delete(use_global=False)
            else:
                if self.ObjectMode:
                    self.ObjectBrush.draw_type = self.ObjectBrush_DT

        if len(context.selected_objects) > 0:
            bpy.ops.object.select_all(action='TOGGLE')

        # Select cutted objects
        for obj in lastSelected:
            bpy.data.objects[obj.name].select = True

        for ActiveObj in ActiveObjList:
            bpy.data.objects[ActiveObj.name].select = True
            context.scene.objects.active = ActiveObj
        # Update bevel
        list_act_obj = context.selected_objects.copy()
        if self.Auto_BevelUpdate:
            update_bevel(context)

        # Reselect intiale objects
        bpy.ops.object.select_all(action='TOGGLE')
        if self.ObjectMode:
            # Reselect brush
            self.ObjectBrush.select = True
        for ActiveObj in ActiveObjList:
            bpy.data.objects[ActiveObj.name].select = True
            context.scene.objects.active = ActiveObj

        # If object has children, set "Wire" draw type
        if self.ObjectBrush is not None:
            if len(self.ObjectBrush.children) > 0:
                self.ObjectBrush.draw_type = "WIRE"
        if self.ProfileMode:
            self.ProfileBrush.draw_type = "WIRE"

        if bLocalView:
            bpy.ops.view3d.localview()

        # Reset variables
        self.bDone = False
        self.mouse_path.clear()
        self.mouse_path = [(0, 0), (0, 0)]

        self.ForceRebool = False


classes = (
    Carver,
    )

addon_keymaps = []


def register():
    bpy.types.Scene.DepthCursor = BoolProperty(name="DepthCursor", default=False)
    bpy.types.Scene.OInstanciate = BoolProperty(name="Obj_Instantiate", default=False)
    bpy.types.Scene.ORandom = BoolProperty(name="Random_Rotation", default=False)
    bpy.types.Scene.DontApply = BoolProperty(name="Dont_Apply", default=False)
    bpy.types.Scene.nProfile = IntProperty(name="Num_Profile", default=0)

    bpy.utils.register_class(CarverPrefs)

    bpy.utils.register_class(Carver)
    # add keymap entry
    kcfg = bpy.context.window_manager.keyconfigs.addon
    if kcfg:
        km = kcfg.keymaps.new(name='3D View', space_type='VIEW_3D')
        kmi = km.keymap_items.new("object.carver", 'X', 'PRESS', shift=True, ctrl=True)
        addon_keymaps.append((km, kmi))


def unregister():
    bpy.utils.unregister_class(CarverPrefs)

    # remove keymap entry
    for km, kmi in addon_keymaps:
        km.keymap_items.remove(kmi)
    addon_keymaps.clear()

    bpy.utils.unregister_class(Carver)


if __name__ == "__main__":
    register()
