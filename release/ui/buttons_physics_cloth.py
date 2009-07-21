
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		ob = context.object
		return (ob and ob.type == 'MESH')
		
class PHYSICS_PT_cloth(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_cloth"
	__label__ = "Cloth"

	def draw(self, context):
		layout = self.layout
		md = context.cloth
		ob = context.object

		split = layout.split()
		split.operator_context = "EXEC_DEFAULT"

		if md:
			# remove modifier + settings
			split.set_context_pointer("modifier", md)
			split.itemO("object.modifier_remove", text="Remove")

			row = split.row(align=True)
			row.itemR(md, "render", text="")
			row.itemR(md, "realtime", text="")
		else:
			# add modifier
			split.item_enumO("object.modifier_add", "type", "CLOTH", text="Add")
			split.itemL()

		if md:
			cloth = md.settings

			split = layout.split()
			
			col = split.column()
			col.itemR(cloth, "quality", slider=True)
			col.itemL(text="Gravity:")
			col.itemR(cloth, "gravity", text="")

			col.itemR(cloth, "pin_cloth", text="Pin")
			colsub = col.column(align=True)
			colsub.active = cloth.pin_cloth
			colsub.itemR(cloth, "pin_stiffness", text="Stiffness")
			colsub.item_pointerR(cloth, "mass_vertex_group", ob, "vertex_groups", text="")
			
			col = split.column()
			col.itemL(text="Presets...")
			col.itemL(text="Material:")
			colsub = col.column(align=True)
			colsub.itemR(cloth, "mass")
			colsub.itemR(cloth, "structural_stiffness", text="Structural")
			colsub.itemR(cloth, "bending_stiffness", text="Bending")
			col.itemL(text="Damping:")
			colsub = col.column(align=True)
			colsub.itemR(cloth, "spring_damping", text="Spring")
			colsub.itemR(cloth, "air_damping", text="Air")
			
			# Disabled for now
			"""
			if cloth.mass_vertex_group:
				layout.itemL(text="Goal:")
			
				col = layout.column_flow()
				col.itemR(cloth, "goal_default", text="Default")
				col.itemR(cloth, "goal_spring", text="Stiffness")
				col.itemR(cloth, "goal_friction", text="Friction")
			"""

class PHYSICS_PT_cloth_cache(PhysicButtonsPanel):
	__idname__= "PHYSICS_PT_cloth_cache"
	__label__ = "Cloth Cache"
	__default_closed__ = True

	def poll(self, context):
		return (context.cloth != None)

	def draw(self, context):
		layout = self.layout

		cache = context.cloth.point_cache
		
		row = layout.row()
		row.itemR(cache, "name")
		
		row = layout.row()
		row.itemR(cache, "start_frame")
		row.itemR(cache, "end_frame")
		
		row = layout.row()
		
		if cache.baked == True:
			row.itemO("ptcache.free_bake_cloth", text="Free Bake")
		else:
			row.item_booleanO("ptcache.cache_cloth", "bake", True, text="Bake")
		
		subrow = row.row()
		subrow.enabled = cache.frames_skipped or cache.outdated
		subrow.itemO("ptcache.cache_cloth", text="Calculate to Current Frame")
			
		row = layout.row()
		#row.enabled = particle_panel_enabled(psys)
		row.itemO("ptcache.bake_from_cloth_cache", text="Current Cache to Bake")
		row.itemR(cache, "step");
	
		row = layout.row()
		#row.enabled = particle_panel_enabled(psys)
		row.itemR(cache, "quick_cache")
		row.itemR(cache, "disk_cache")
		
		layout.itemL(text=cache.info)
		
		layout.itemS()
		
		row = layout.row()
		row.itemO("ptcache.bake_all", "bake", True, text="Bake All Dynamics")
		row.itemO("ptcache.free_bake_all", text="Free All Bakes")
		layout.itemO("ptcache.bake_all", text="Update All Dynamics to current frame")
		
class PHYSICS_PT_cloth_collision(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_clothcollision"
	__label__ = "Cloth Collision"
	__default_closed__ = True

	def poll(self, context):
		return (context.cloth != None)
	
	def draw_header(self, context):
		layout = self.layout
		cloth = context.cloth.collision_settings
	
		layout.itemR(cloth, "enable_collision", text="")

	def draw(self, context):
		layout = self.layout
		cloth = context.cloth.collision_settings
		split = layout.split()
		
		layout.active = cloth.enable_collision
		
		col = split.column(align=True)
		col.itemR(cloth, "collision_quality", slider=True, text="Quality")
		col.itemR(cloth, "min_distance", slider=True, text="Distance")
		col.itemR(cloth, "friction")
		
		col = split.column(align=True)
		col.itemR(cloth, "enable_self_collision", text="Self Collision")
		col = col.column(align=True)
		col.active = cloth.enable_self_collision
		col.itemR(cloth, "self_collision_quality", slider=True, text="Quality")
		col.itemR(cloth, "self_min_distance", slider=True, text="Distance")

class PHYSICS_PT_cloth_stiffness(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_stiffness"
	__label__ = "Cloth Stiffness Scaling"
	__default_closed__ = True

	def poll(self, context):
		return (context.cloth != None)
	
	def draw_header(self, context):
		layout = self.layout
		cloth = context.cloth.settings
	
		layout.itemR(cloth, "stiffness_scaling", text="")

	def draw(self, context):
		layout = self.layout
		ob = context.object
		cloth = context.cloth.settings
		
		layout.active = cloth.stiffness_scaling	
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Structural Stiffness:")
		colsub = col.column(align=True)
		colsub.itemR(cloth, "structural_stiffness_max", text="Max")
		colsub.item_pointerR(cloth, "structural_stiffness_vertex_group", ob, "vertex_groups", text="")
		
		col = split.column()
		col.itemL(text="Bending Stiffness:")
		colsub = col.column(align=True)
		colsub.itemR(cloth, "bending_stiffness_max", text="Max")
		colsub.item_pointerR(cloth, "bending_vertex_group", ob, "vertex_groups", text="")
		
bpy.types.register(PHYSICS_PT_cloth)
bpy.types.register(PHYSICS_PT_cloth_cache)
bpy.types.register(PHYSICS_PT_cloth_collision)
bpy.types.register(PHYSICS_PT_cloth_stiffness)

