
import bpy

class PhysicButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "physics"

	def poll(self, context):
		rd = context.scene.render_data
		return (context.object) and (not rd.use_game_engine)
		
class PHYSICS_PT_field(PhysicButtonsPanel):
	__label__ = "Force Fields"
	__default_closed__ = True

	def draw(self, context):
		layout = self.layout
		
		ob = context.object
		field = ob.field

		#layout.active = field.enabled
		
		split = layout.split(percentage=0.2)
		
		split.itemL(text="Type:")
		split.itemR(field, "type",text="")
			
		split = layout.split()
		
		if field.type == 'GUIDE':
			layout.itemR(field, "guide_path_add")
			
		elif field.type == 'WIND':
			split.itemR(field, "strength")
			
			col = split.column()
			col.itemR(field, "noise")
			col.itemR(field, "seed")

		elif field.type == 'VORTEX':
			split.itemR(field, "strength")
			split.itemL()
			
		elif field.type in ('SPHERICAL', 'CHARGE', 'LENNARDJ'):
			split.itemR(field, "strength")
			
			col = split.column()
			col.itemR(field, "planar")
			col.itemR(field, "surface")
			
		elif field.type == 'BOID':
			split.itemR(field, "strength")
			split.itemR(field, "surface")
			
		elif field.type == 'MAGNET':
			split.itemR(field, "strength")
			split.itemR(field, "planar")
			
		elif field.type == 'HARMONIC':
			col = split.column()
			col.itemR(field, "strength")
			col.itemR(field, "harmonic_damping", text="Damping")
			
			col = split.column()
			col.itemR(field, "planar")
			col.itemR(field, "surface")
			
		elif field.type == 'TEXTURE':
			col = split.column()
			col.itemR(field, "strength")
			col.itemR(field, "texture", text="")
			col.itemR(field, "texture_mode", text="")
			col.itemR(field, "texture_nabla")
			
			col = split.column()
			col.itemR(field, "use_coordinates")
			col.itemR(field, "root_coordinates")
			col.itemR(field, "force_2d")
			
		if field.type in ('HARMONIC', 'SPHERICAL', 'CHARGE', 'WIND', 'VORTEX', 'TEXTURE', 'MAGNET', 'BOID'):
			
			layout.itemL(text="Falloff:")
			layout.itemR(field, "falloff_type", expand=True)

			split = layout.split(percentage=0.35)
			
			col = split.column()
			col.itemR(field, "positive_z", text="Positive Z")
			col.itemR(field, "use_min_distance", text="Use Minimum")
			col.itemR(field, "use_max_distance", text="Use Maximum")

			col = split.column()
			col.itemR(field, "falloff_power", text="Power")
			
			sub = col.column()
			sub.active = field.use_min_distance
			sub.itemR(field, "minimum_distance", text="Distance")
			
			sub = col.column()
			sub.active = field.use_max_distance
			sub.itemR(field, "maximum_distance", text="Distance")
			
			if field.falloff_type == 'CONE':
				
				layout.itemS()
				
				split = layout.split(percentage=0.35)
				
				col = split.column()
				col.itemL(text="Angular:")
				col.itemR(field, "use_radial_min", text="Use Minimum")	
				col.itemR(field, "use_radial_max", text="Use Maximum")
				
				col = split.column()
				col.itemR(field, "radial_falloff", text="Power")
				
				sub = col.column()
				sub.active = field.use_radial_min
				sub.itemR(field, "radial_minimum", text="Angle")
				
				sub = col.column()
				sub.active = field.use_radial_max
				sub.itemR(field, "radial_maximum", text="Angle")
				
			elif field.falloff_type == 'TUBE':
				
				layout.itemS()
				
				split = layout.split(percentage=0.35)
					
				col = split.column()
				col.itemL(text="Radial:")	
				col.itemR(field, "use_radial_min", text="Use Minimum")	
				col.itemR(field, "use_radial_max", text="Use Maximum")
				
				col = split.column()
				col.itemR(field, "radial_falloff", text="Power")
				
				sub = col.column()
				sub.active = field.use_radial_min
				sub.itemR(field, "radial_minimum", text="Distance")
				
				sub = col.column()
				sub.active = field.use_radial_max
				sub.itemR(field, "radial_maximum", text="Distance")
				
		#if ob.type in 'CURVE':
			#if field.type == 'GUIDE':
				#colsub = col.column(align=True)
			
		#if field.type != 'NONE':
			#layout.itemR(field, "strength")

		#if field.type in ('HARMONIC', 'SPHERICAL', 'CHARGE', "LENNARDj"):
			#if ob.type in ('MESH', 'SURFACE', 'FONT', 'CURVE'):
				#layout.itemR(field, "surface")

class PHYSICS_PT_collision(PhysicButtonsPanel):
	__label__ = "Collision"
	__default_closed__ = True
	
	def poll(self, context):
		ob = context.object
		rd = context.scene.render_data
		return (ob and ob.type == 'MESH') and (not rd.use_game_engine)
	
	def draw(self, context):
		layout = self.layout
		
		md = context.collision

		split = layout.split()
		split.operator_context = 'EXEC_DEFAULT'

		if md:
			# remove modifier + settings
			split.set_context_pointer("modifier", md)
			split.itemO("object.modifier_remove", text="Remove")
			col = split.column()
			
			#row = split.row(align=True)
			#row.itemR(md, "render", text="")
			#row.itemR(md, "realtime", text="")
			
			settings = md.settings
			
		else:
			# add modifier
			split.item_enumO("object.modifier_add", "type", 'COLLISION', text="Add")
			split.itemL()
			
			settings = None
		
		if settings:
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
