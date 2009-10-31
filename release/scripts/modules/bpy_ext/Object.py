# This software is distributable under the terms of the GNU
# General Public License (GPL) v2, the text of which can be found at
# http://www.gnu.org/copyleft/gpl.html. Installing, importing or otherwise
# using this module constitutes acceptance of the terms of this License.

import bpy
class_obj = bpy.types.Object

class_obj.getChildren = lambda ob: [child for child in bpy.data.objects if child.parent == ob]
