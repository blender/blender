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

devices = (
("CPU", "CPU", "Processor"),
("GPU", "GPU", "Graphics card (NVidia only)"))

shading_systems = (
("GPU_COMPATIBLE", "GPU Compatible", "Restricted shading system compatible with GPU rendering"),
("OSL", "Open Shading Language", "Open Shading Language shading system that only runs on the CPU"))

displacement_methods = (
("BUMP", "Bump", "Bump mapping to simulate the appearance of displacement"),
("TRUE", "True", "Use true displacement only, requires fine subdivision"),
("BOTH", "Both", "Combination of displacement and bump mapping"))

bvh_types = (
("DYNAMIC_BVH", "Dynamic BVH", "Objects can be individually updated, at the cost of slower render time"),
("STATIC_BVH", "Static BVH", "Any object modification requires a complete BVH rebuild, but renders faster"))

response_curves = (
("None", "None", ""),
("", "Agfa", ""),
("Agfacolor Futura 100", "Futura 100", ""),
("Agfacolor Futura 200", "Futura 200", ""),
("Agfacolor Futura 400", "Futura 400", ""),
("Agfacolor Futura II 100", "Futura II 100", ""),
("Agfacolor Futura II 200", "Futura II 200", ""),
("Agfacolor Futura II 400", "Futura II 400", ""),
("Agfacolor HDC 100 plus", "HDC 100 plus", ""),
("Agfacolor HDC 400 plus", "HDC 400 plus", ""),
("Agfacolor HDC 200 plus", "HDC 200 plus", ""),
("Agfacolor Optima II 100", "Optima II 100", ""),
("Agfacolor Optima II 200", "Optima II 200", ""),
("Agfacolor Ultra 050", "Ultra 050", ""),
("", "Agfa", ""),
("Agfacolor Vista 100", "Vista 100", ""),
("Agfacolor Vista 200", "Vista 200", ""),
("Agfacolor Vista 400", "Vista 400", ""),
("Agfacolor Vista 800", "Vista 800", ""),
("Agfachrome CT Precisa 100", "CT Precisa 100", ""),
("Agfachrome CT Precisa 200", "CT Precisa 200", ""),
("Agfachrome RSX2 050", "Agfachrome RSX2 050", ""),
("Agfachrome RSX2 100", "Agfachrome RSX2 100", ""),
("Agfachrome RSX2 200", "Agfachrome RSX2 200", ""),
("Advantix 100", "Advantix 100", ""),
("Advantix 200", "Advantix 200", ""),
("Advantix 400", "Advantix 400", ""),
("", "Kodak", ""),
("Gold 100", "Gold 100", ""),
("Gold 200", "Gold 200", ""),
("Max Zoom 800", "Max Zoom 800", ""),
("Portra 100T", "Portra 100T", ""),
("Portra 160NC", "Portra 160NC", ""),
("Portra 160VC", "Portra 160VC", ""),
("Portra 800", "Portra 800", ""),
("Portra 400VC", "Portra 400VC", ""),
("Portra 400NC", "Portra 400NC", ""),
("", "Kodak", ""),
("Ektachrome 100 plus", "Ektachrome 100 plus", ""),
("Ektachrome 320T", "Ektachrome 320T", ""),
("Ektachrome 400X", "Ektachrome 400X", ""),
("Ektachrome 64", "Ektachrome 64", ""),
("Ektachrome 64T", "Ektachrome 64T", ""),
("Ektachrome E100S", "Ektachrome E100S", ""),
("Ektachrome 100", "Ektachrome 100", ""),
("Kodachrome 200", "Kodachrome 200", ""),
("Kodachrome 25", "Kodachrome 25", ""),
("Kodachrome 64", "Kodachrome 64", ""),
#("DSCS 3151", "DSCS 3151", ""),
#("DSCS 3152", "DSCS 3152", ""),
#("DSCS 3153", "DSCS 3153", ""),
#("DSCS 3154", "DSCS 3154", ""),
#("DSCS 3155", "DSCS 3155", ""),
#("DSCS 3156", "DSCS 3156", ""),
#("KAI-0311", "KAI-0311", ""),
#("KAF-2001", "KAF-2001", ""),
#("KAF-3000", "KAF-3000", ""),
#("KAI-0372", "KAI-0372", ""),
#("KAI-1010", "KAI-1010", ""),
("", "Fujifilm", ""),
("F-125", "F-125", ""),
("F-250", "F-250", ""),
("F-400", "F-400", ""),
("FCI", "FCI", ""),
("FP2900Z", "FP2900Z", ""),
("", "Eastman", ""),
("Double X Neg 12min", "Double X Neg 12min", ""),
("Double X Neg 6min", "Double X Neg 6min", ""),
("Double X Neg 5min", "Double X Neg 5min", ""),
("Double X Neg 4min", "Double X Neg 4min", ""),
("", "Canon", ""),
("Optura 981111", "Optura 981111", ""),
("Optura 981113", "Optura 981113", ""),
("Optura 981114", "Optura 981114", ""),
("Optura 981111.SLRR", "Optura 981111.SLRR", "")
)

