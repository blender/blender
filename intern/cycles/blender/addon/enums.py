#
# Copyright 2011, Blender Foundation.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#

# <pep8 compliant>

from . import engine

devices = (
	("CPU", "CPU", "Use CPU for rendering"),
	("GPU", "GPU Compute", "Use GPU compute device for rendering, configured in user preferences"))

feature_set = (
    ("SUPPORTED", "Supported", "Only use finished and supported features"),
    ("EXPERIMENTAL", "Experimental", "Use experimental and incomplete features that might be broken or change in the future"),
    )

shading_systems = (
    ("GPU_COMPATIBLE", "GPU Compatible", "Restricted shading system compatible with GPU rendering"),
    ("OSL", "Open Shading Language", "Open Shading Language shading system that only runs on the CPU"),
    )

displacement_methods = (
    ("BUMP", "Bump", "Bump mapping to simulate the appearance of displacement"),
    ("TRUE", "True", "Use true displacement only, requires fine subdivision"),
    ("BOTH", "Both", "Combination of displacement and bump mapping"),
    )

bvh_types = (
    ("DYNAMIC_BVH", "Dynamic BVH", "Objects can be individually updated, at the cost of slower render time"),
    ("STATIC_BVH", "Static BVH", "Any object modification requires a complete BVH rebuild, but renders faster"),
    )

filter_types = (
    ("BOX", "Box", "Box filter"),
    ("GAUSSIAN", "Gaussian", "Gaussian filter"),
    )
