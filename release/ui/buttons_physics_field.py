
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		return (context.object != None)
		
class PHYSICS_PT_field(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_field"
	__label__ = "Force Fields"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		ob = context.object
		field = ob.field

		#layout.active = field.enabled
		
		split = layout.split()
		col = split.column(align=True)
		col.itemL(text="Type:")
		col.itemR(field, "type", text="")
		colsub = split.column(align=True)
							
		if field.type == "GUIDE":
			colsub = col.column()
			colsub.itemL(text="blabla")
			colsub.itemR(field, "guide_path_add")
			
		if field.type == "WIND":
			col.itemR(field, "strength")
			col.itemR(field, "noise")
			col.itemR(field, "seed")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
		
		if field.type == "VORTEX":
			col.itemR(field, "strength")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")

		if field.type in ("SPHERICAL", "CHARGE", "LENNARDJ"):
			col.itemR(field, "strength")
			colsub.itemL(text="")
			colsub.itemR(field, "surface")
			colsub.itemR(field, "planar")
			colsub.itemL(text="")
			colsub.itemL(text="")
			
		if field.type == "MAGNET":
			col.itemR(field, "strength")
			colsub.itemL(text="")
			colsub.itemR(field, "planar")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			
		if field.type == "HARMONIC":
			col.itemR(field, "strength")
			col.itemR(field, "harmonic_damping", text="Damping")
			colsub.itemL(text="")
			colsub.itemR(field, "surface")
			colsub.itemR(field, "planar")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			
		if field.type == "TEXTURE":
			col.itemR(field, "strength")
			col.itemR(field, "texture", text="")
			col.itemL(text="Texture Mode:")
			col.itemR(field, "texture_mode", text="")
			col.itemR(field, "texture_nabla")
			colsub.itemL(text="")
			colsub.itemR(field, "use_coordinates")
			colsub.itemR(field, "root_coordinates")
			colsub.itemR(field, "force_2d")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			colsub.itemL(text="")
			
		if field.type in ("HARMONIC", "SPHERICAL", "CHARGE", "WIND", "VORTEX", "TEXTURE", "MAGNET"):
			col.itemL(text="Fall-Off:")
			col.itemR(field, "falloff_type", text="")
			col.itemR(field, "positive_z", text="Positive Z")
			col.itemR(field, "use_min_distance", text="Use Minimum")
			col.itemR(field, "use_max_distance", text="Use Maximum")
			colsub.itemR(field, "falloff_power", text="Power")
			colsub1 = colsub.column()
			colsub1.active = field.use_min_distance
			colsub1.itemR(field, "minimum_distance", text="Distance")
			colsub2 = colsub.column()
			colsub2.active = field.use_max_distance
			colsub2.itemR(field, "maximum_distance", text="Distance")
			
			if field.falloff_type == "CONE":
				col.itemL(text="")
				col.itemL(text="Angular:")
				col.itemR(field, "use_radial_min", text="Use Minimum")	
				col.itemR(field, "use_radial_max", text="Use Maximum")
				colsub.itemL(text="")		
				colsub.itemR(field, "radial_falloff", text="Power")
				colsub1 = colsub.column()
				colsub1.active = field.use_radial_min
				colsub1.itemR(field, "radial_minimum", text="Angle")
				colsub2 = colsub.column()
				colsub2.active = field.use_radial_max
				colsub2.itemR(field, "radial_maximum", text="Angle")
				
			if field.falloff_type == "TUBE":
				col.itemL(text="")
				col.itemL(text="Radial:")
				col.itemR(field, "use_radial_min", text="Use Minimum")	
				col.itemR(field, "use_radial_max", text="Use Maximum")
				colsub.itemL(text="")
				colsub.itemR(field, "radial_falloff", text="Power")
				colsub1 = colsub.column()
				colsub1.active = field.use_radial_min
				colsub1.itemR(field, "radial_minimum", text="Distance")
				colsub2 = colsub.column()
				colsub2.active = field.use_radial_max
				colsub2.itemR(field, "radial_maximum", text="Distance")
				
		#if ob.type in "CURVE":
			#if field.type == "GUIDE":
				#colsub = col.column(align=True)
			
		#if field.type != "NONE":
			#layout.itemR(field, "strength")

		#if field.type in ("HARMONIC", "SPHERICAL", "CHARGE", "LENNARDj"):
			#if ob.type in ("MESH", "SURFACE", "FONT", "CURVE"):
				#layout.itemR(field, "surface")

class PHYSICS_PT_collision(PhysicButtonsPanel):
	__idname__ = "PHYSICS_PT_collision"
	__label__ = "Collision"
	__default_closed__ = True
	
	def poll(self, context):
		ob = context.object
		return (ob and ob.type == 'MESH')

	def draw_header(self, context):
		settings = context.object.collision
		self.layout.itemR(settings, "enabled", text="")

	def draw(self, context):
		layout = self.layout
		md = context.collision
		settings = context.object.collision

		layout.active = settings.enabled
		
		split = layout.split()
		
		col = split.column()
		col.itemL(text="Particle:")
		col.itemR(settings, "permeability", slider=True)
		col.itemL(text="Particle Damping:")
		colsub = col.column(align=True)
		colsub.itemR(settings, "damping_factor", text="Factor", slider=True)
		colsub.itemR(settings, "random_damping", text="Random", slider=True)
		
		col.itemL(text="Soft Body and Cloth:")
		colsub = col.column(align=True)
		colsub.itemR(settings, "outer_thickness", text="Outer", slider=True)
		colsub.itemR(settings, "inner_thickness", text="Inner", slider=True)
		
		col.itemL(text="Force Fields:")
		layout.itemR(md, "absorption", text="Absorption")
		
		col = split.column()
		col.itemL(text="")
		col.itemR(settings, "kill_particles")
		col.itemL(text="Particle Friction:")
		colsub = col.column(align=True)
		colsub.itemR(settings, "friction_factor", text="Factor", slider=True)
		colsub.itemR(settings, "random_friction", text="Random", slider=True)
		col.itemL(text="Soft Body Damping:")
		col.itemR(settings, "damping", text="Factor", slider=True)
		
bpy.types.register(PHYSICS_PT_field)
bpy.types.register(PHYSICS_PT_collision)