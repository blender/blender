
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = "BUTTONS_WINDOW"
	__region_type__ = "WINDOW"
	__context__ = "physics"

	def poll(self, context):
		rd = context.scene.render_data
		return (context.object != None) and (not rd.use_game_engine)
		
class PHYSICS_PT_field(PhysicButtonsPanel):
	__label__ = "Force Fields"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		field = ob.field

		#layout.active = field.enabled
		
		split = layout.split(percentage=0.3)
		
		split.itemL(text="Type:")
		split.itemR(field, "type", text="")

		split = layout.split()
		
		col = split.column()
							
		if field.type == "GUIDE":
			col.itemR(field, "guide_path_add")
			
		elif field.type == "WIND":
			col.itemR(field, "strength")
			
			col = split.column()
			col.itemR(field, "noise")
			col.itemR(field, "seed")

		elif field.type == "VORTEX":
			col.itemR(field, "strength")
			
			col = split.column()
			col.itemL(text="")

		elif field.type in ("SPHERICAL", "CHARGE", "LENNARDJ"):
			col.itemR(field, "strength")
			
			col = split.column()
			col.itemR(field, "planar")
			col.itemR(field, "surface")
			
		elif field.type == "BOID":
			col.itemR(field, "strength")
			
			col = split.column()
			col.itemR(field, "surface")
			
		elif field.type == "MAGNET":
			col.itemR(field, "strength")
			
			col = split.column()
			col.itemR(field, "planar")
			
		elif field.type == "HARMONIC":
			col.itemR(field, "strength")
			col.itemR(field, "harmonic_damping", text="Damping")
			
			col = split.column()
			col.itemR(field, "surface")
			col.itemR(field, "planar")
			
		elif field.type == "TEXTURE":
			col.itemR(field, "strength")
			col.itemR(field, "texture", text="")
			col.itemR(field, "texture_mode")
			col.itemR(field, "texture_nabla")
			
			col = split.column()
			col.itemR(field, "use_coordinates")
			col.itemR(field, "root_coordinates")
			col.itemR(field, "force_2d")
			
		if field.type in ("HARMONIC", "SPHERICAL", "CHARGE", "WIND", "VORTEX", "TEXTURE", "MAGNET", "BOID"):
			layout.itemS()			
			layout.itemL(text="Falloff:")
			layout.itemR(field, "falloff_type", expand=True)
			
			row = layout.row()
			row.itemR(field, "falloff_power", text="Power")
			row.itemR(field, "positive_z", text="Positive Z")
			
			layout.itemS()	
			split = layout.split()
			
			col = split.column()
			col.itemR(field, "use_min_distance", text="Minimum")
			sub = col.column()
			sub.active = field.use_min_distance
			sub.itemR(field, "minimum_distance", text="Distance")
			
			col = split.column()
			col.itemR(field, "use_max_distance", text="Maximum")
			sub = col.column()
			sub.active = field.use_max_distance
			sub.itemR(field, "maximum_distance", text="Distance")
			
			if field.falloff_type == "CONE":
				layout.itemS()	
				layout.itemL(text="Angular:")
				
				row = layout.row()
				row.itemR(field, "radial_falloff", text="Power")
				row.itemL()
				
				split = layout.split()
				
				col = split.column()
				col.itemR(field, "use_radial_min", text="Minimum")	
				sub = col.column()
				sub.active = field.use_radial_min
				sub.itemR(field, "radial_minimum", text="Angle")
				
				col = split.column()
				col.itemR(field, "use_radial_max", text="Maximum")
				sub = col.column()
				sub.active = field.use_radial_max
				sub.itemR(field, "radial_maximum", text="Angle")
				
			elif field.falloff_type == "TUBE":
				layout.itemS()	
				layout.itemL(text="Radial:")	
				
				row = layout.row()
				row.itemR(field, "radial_falloff", text="Power")
				row.itemL()
				
				split = layout.split()
				
				col = split.column()
				col.itemR(field, "use_radial_min", text="Minimum")	
				sub = col.column()
				sub.active = field.use_radial_min
				sub.itemR(field, "radial_minimum", text="Distance")
				
				col = split.column()
				col.itemR(field, "use_radial_max", text="Maximum")
				sub = col.column()
				sub.active = field.use_radial_max
				sub.itemR(field, "radial_maximum", text="Distance")
				
		#if ob.type in "CURVE":
			#if field.type == "GUIDE":
				#colsub = col.column(align=True)
			
		#if field.type != "NONE":
			#layout.itemR(field, "strength")

		#if field.type in ("HARMONIC", "SPHERICAL", "CHARGE", "LENNARDj"):
			#if ob.type in ("MESH", "SURFACE", "FONT", "CURVE"):
				#layout.itemR(field, "surface")

class PHYSICS_PT_collision(PhysicButtonsPanel):
	__label__ = "Collision"
	__default_closed__ = True
	
	def poll(self, context):
		ob = context.object
		rd = context.scene.render_data
		return (ob and ob.type == 'MESH') and (not rd.use_game_engine)

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
		sub = col.column(align=True)
		sub.itemR(settings, "damping_factor", text="Factor", slider=True)
		sub.itemR(settings, "random_damping", text="Random", slider=True)
		
		col.itemL(text="Soft Body and Cloth:")
		sub = col.column(align=True)
		sub.itemR(settings, "outer_thickness", text="Outer", slider=True)
		sub.itemR(settings, "inner_thickness", text="Inner", slider=True)
		
		layout.itemL(text="Force Fields:")
		layout.itemR(md, "absorption", text="Absorption")
		
		col = split.column()
		col.itemL(text="")
		col.itemR(settings, "kill_particles")
		col.itemL(text="Particle Friction:")
		sub = col.column(align=True)
		sub.itemR(settings, "friction_factor", text="Factor", slider=True)
		sub.itemR(settings, "random_friction", text="Random", slider=True)
		col.itemL(text="Soft Body Damping:")
		col.itemR(settings, "damping", text="Factor", slider=True)
		
bpy.types.register(PHYSICS_PT_field)
bpy.types.register(PHYSICS_PT_collision)
