# Built-In Keying Sets
# None of these Keying Sets should be removed, as these
# are needed by various parts of Blender in order for them
# to work correctly.

import bpy
from keyingsets_utils import *

###############################
# Built-In KeyingSets

# Location
class BUILTIN_KSI_Location(bpy.types.KeyingSetInfo):
	bl_idname = "Location"
	bl_builtin = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	generate = RKS_GEN_location
	
# Rotation
class BUILTIN_KSI_Rotation(bpy.types.KeyingSetInfo):
	bl_idname = "Rotation"
	bl_builtin = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	generate = RKS_GEN_rotation
	
# Scale
class BUILTIN_KSI_Scaling(bpy.types.KeyingSetInfo):
	bl_idname = "Scaling"
	bl_builtin = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	generate = RKS_GEN_scaling

# ------------
	
# LocRot
class BUILTIN_KSI_LocRot(bpy.types.KeyingSetInfo):
	bl_idname = "LocRot"
	bl_builtin = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	def generate(self, context, ks, data):
		# location
		RKS_GEN_location(self, context, ks, data)
		# rotation
		RKS_GEN_rotation(self, context, ks, data)

# LocScale
class BUILTIN_KSI_LocScale(bpy.types.KeyingSetInfo):
	bl_idname = "LocScale"
	bl_builtin = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	def generate(self, context, ks, data):
		# location
		RKS_GEN_location(self, context, ks, data)
		# scale
		RKS_GEN_scaling(self, context, ks, data)

# LocRotScale
class BUILTIN_KSI_LocRotScale(bpy.types.KeyingSetInfo):
	bl_idname = "LocRotScale"
	bl_builtin = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	def generate(self, context, ks, data):
		# location
		RKS_GEN_location(self, context, ks, data)
		# rotation
		RKS_GEN_rotation(self, context, ks, data)
		# scale
		RKS_GEN_scaling(self, context, ks, data)

# RotScale
class BUILTIN_KSI_RotScale(bpy.types.KeyingSetInfo):
	bl_idname = "RotScale"
	bl_builtin = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	def generate(self, context, ks, data):
		# rotation
		RKS_GEN_rotation(self, context, ks, data)
		# scaling
		RKS_GEN_scaling(self, context, ks, data)
		
# ------------

# Location
class BUILTIN_KSI_VisualLoc(bpy.types.KeyingSetInfo):
	bl_idname = "Visual Location"
	bl_builtin = True
	
	insertkey_visual = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	generate = RKS_GEN_location
	
# Rotation
class BUILTIN_KSI_VisualRot(bpy.types.KeyingSetInfo):
	bl_idname = "Visual Rotation"
	bl_builtin = True
	
	insertkey_visual = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	generate = RKS_GEN_rotation

# VisualLocRot
class BUILTIN_KSI_VisualLocRot(bpy.types.KeyingSetInfo):
	bl_idname = "Visual LocRot"
	bl_builtin = True
	
	insertkey_visual = True
	
	# poll - use predefined callback for selected bones/objects
	poll = RKS_POLL_selected_items
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	def generate(self, context, ks, data):
		# location
		RKS_GEN_location(self, context, ks, data)
		# rotation
		RKS_GEN_rotation(self, context, ks, data)

# ------------

# Available
class BUILTIN_KSI_Available(bpy.types.KeyingSetInfo):
	bl_idname = "Available"
	bl_builtin = True
	
	# poll - use predefined callback for selected objects
	# TODO: this should really check whether the selected object (or datablock) 
	# 		has any animation data defined yet
	poll = RKS_POLL_selected_objects
	
	# iterator - use callback for selected bones/objects
	iterator = RKS_ITER_selected_item
	
	# generator - use callback for location 
	generate = RKS_GEN_available

############################### 

classes = [
	BUILTIN_KSI_Location,
	BUILTIN_KSI_Rotation,
	BUILTIN_KSI_Scaling,
	
	BUILTIN_KSI_LocRot,
	BUILTIN_KSI_LocScale,
	BUILTIN_KSI_LocRotScale,
	BUILTIN_KSI_RotScale,
	
	BUILTIN_KSI_VisualLoc,
	BUILTIN_KSI_VisualRot,
	BUILTIN_KSI_VisualLocRot,
	
	BUILTIN_KSI_Available,
]


def register():
    register = bpy.types.register
    for cls in classes:
        register(cls)


def unregister():
    unregister = bpy.types.unregister
    for cls in classes:
        unregister(cls)

if __name__ == "__main__":
    register()

############################### 
