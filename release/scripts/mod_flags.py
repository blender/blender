# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://ourworld.compuserve.com/homepages/scorpius       |
# | scorpius@compuserve.com                                 |
# | October 25, 2002                                        |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Share Global Variables Across Modules                   |
# +---------------------------------------------------------+

import Blender

blender_version = Blender.Get('version')
blender_version_str = `blender_version`[0] + '.' + `blender_version`[1:]

show_progress = 1			# Set to 0 for faster performance
average_vcols = 1			# Off for per-face, On for per-vertex
overwrite_mesh_name = 0 	# Set to 0 for safety
