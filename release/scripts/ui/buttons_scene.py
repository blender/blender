
import bpy

class SceneButtonsPanel(bpy.types.Panel):
	__space_type__ = 'PROPERTIES'
	__region_type__ = 'WINDOW'
	__context__ = "scene"
	
	def poll(self, context):
		return context.scene

class SCENE_PT_scene(SceneButtonsPanel):
	__label__ = "Scene"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene

		layout.itemR(scene, "camera")
		layout.itemR(scene, "set", text="Background")

class SCENE_PT_unit(SceneButtonsPanel):
	__label__ = "Units"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw(self, context):
		layout = self.layout
		
		unit = context.scene.unit_settings
		
		col = layout.column()
		col.row().itemR(unit, "system", expand=True)
		
		row = layout.row()
		row.active = (unit.system != 'NONE')
		row.itemR(unit, "scale_length", text="Scale")
		row.itemR(unit, "use_separate")
		
class SCENE_PT_keying_sets(SceneButtonsPanel):
	__label__ = "Keying Sets"
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		
		row = layout.row()
		
		col = row.column()
		col.template_list(scene, "keying_sets", scene, "active_keying_set_index", rows=2)
		
		col = row.column(align=True)
		col.itemO("anim.keying_set_add", icon='ICON_ZOOMIN', text="")
		col.itemO("anim.keying_set_remove", icon='ICON_ZOOMOUT', text="")
		
		ks = scene.active_keying_set
		if ks:
			row = layout.row()
			
			col = row.column()
			col.itemR(ks, "name")
			col.itemR(ks, "absolute")
			
			col = row.column()
			col.itemL(text="Keyframing Settings:")
			col.itemR(ks, "insertkey_needed", text="Needed")
			col.itemR(ks, "insertkey_visual", text="Visual")
			
class SCENE_PT_keying_set_paths(SceneButtonsPanel):
	__label__ = "Active Keying Set"
	
	def poll(self, context):
		return (context.scene != None) and (context.scene.active_keying_set != None)
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		ks = scene.active_keying_set
		
		row = layout.row()
		row.itemL(text="Paths:")
		
		row = layout.row()
		
		col = row.column()
		col.template_list(ks, "paths", ks, "active_path_index", rows=2)
		
		col = row.column(align=True)
		col.itemO("anim.keying_set_path_add", icon='ICON_ZOOMIN', text="")
		col.itemO("anim.keying_set_path_remove", icon='ICON_ZOOMOUT', text="")
		
		ksp = ks.active_path
		if ksp:
			col = layout.column()
			col.itemL(text="Target:")
			col.template_any_ID(ksp, "id", "id_type")
			col.itemR(ksp, "rna_path")
			
			
			row = layout.row()
			
			col = row.column()
			col.itemL(text="Array Target:")
			col.itemR(ksp, "entire_array")
			if ksp.entire_array == False:
				col.itemR(ksp, "array_index")
				
			col = row.column()
			col.itemL(text="F-Curve Grouping:")
			col.itemR(ksp, "grouping")
			if ksp.grouping == 'NAMED':
				col.itemR(ksp, "group")

class SCENE_PT_physics(SceneButtonsPanel):
	__label__ = "Gravity"
	COMPAT_ENGINES = set(['BLENDER_RENDER'])

	def draw_header(self, context):
		self.layout.itemR(context.scene, "use_gravity", text="")

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene

		layout.active = scene.use_gravity

		layout.itemR(scene, "gravity", text="")

bpy.types.register(SCENE_PT_scene)		
bpy.types.register(SCENE_PT_unit)
bpy.types.register(SCENE_PT_keying_sets)
bpy.types.register(SCENE_PT_keying_set_paths)
bpy.types.register(SCENE_PT_physics)
