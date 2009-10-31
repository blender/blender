
import bpy

import dynamic_menu

# ********** Header **********

class VIEW3D_HT_header(bpy.types.Header):
	bl_space_type = 'VIEW_3D'

	def draw(self, context):
		layout = self.layout
		
		view = context.space_data
		mode_string = context.mode
		edit_object = context.edit_object
		object = context.active_object
		
		row = layout.row(align=True)
		row.template_header()

		# Menus
		if context.area.show_menus:
			sub = row.row(align=True)

			sub.itemM("VIEW3D_MT_view")
			
			# Select Menu
			if mode_string not in ('EDIT_TEXT', 'SCULPT', 'PAINT_WEIGHT', 'PAINT_VERTEX', 'PAINT_TEXTURE'):
				sub.itemM("VIEW3D_MT_select_%s" % mode_string.lower())
			
			if edit_object:
				sub.itemM("VIEW3D_MT_edit_%s" % edit_object.type.lower())
			elif object:
				ob_mode_string = object.mode
				
				if mode_string not in ['PAINT_WEIGHT', 'PAINT_TEXTURE']:
					sub.itemM("VIEW3D_MT_%s" % mode_string.lower())
			else:
				sub.itemM("VIEW3D_MT_object")

		layout.template_header_3D()

# ********** Menu **********

# ********** Utilities **********

class VIEW3D_MT_showhide(bpy.types.Menu):
	bl_label = "Show/Hide"
	_operator_name = ""

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("%s.reveal" % self._operator_name, text="Show Hidden")
		layout.itemO("%s.hide" % self._operator_name, text="Hide Selected")
		layout.item_booleanO("%s.hide" % self._operator_name, "unselected", True, text="Hide Unselected")

class VIEW3D_MT_snap(bpy.types.Menu):
	bl_label = "Snap"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("view3d.snap_selected_to_grid", text="Selection to Grid")
		layout.itemO("view3d.snap_selected_to_cursor", text="Selection to Cursor")
		layout.itemO("view3d.snap_selected_to_center", text="Selection to Center")
		
		layout.itemS()
		
		layout.itemO("view3d.snap_cursor_to_selected", text="Cursor to Selected")
		layout.itemO("view3d.snap_cursor_to_grid", text="Cursor to Grid")
		layout.itemO("view3d.snap_cursor_to_active", text="Cursor to Active")

# ********** View menus **********

class VIEW3D_MT_view(bpy.types.Menu):
	bl_label = "View"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.properties", icon='ICON_MENU_PANEL')
		layout.itemO("view3d.toolbar", icon='ICON_MENU_PANEL')
		
		layout.itemS()
		
		layout.item_enumO("view3d.viewnumpad", "type", 'CAMERA')
		layout.item_enumO("view3d.viewnumpad", "type", 'TOP')
		layout.item_enumO("view3d.viewnumpad", "type", 'FRONT')
		layout.item_enumO("view3d.viewnumpad", "type", 'RIGHT')
		
		layout.itemM("VIEW3D_MT_view_cameras", text="Cameras")
		
		layout.itemS()
		
		layout.itemO("view3d.view_persportho")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_view_navigation")
		layout.itemM("VIEW3D_MT_view_align")
		
		layout.itemS()

		layout.operator_context = "INVOKE_REGION_WIN"
		
		layout.itemO("view3d.clip_border", text="Clipping Border...")
		layout.itemO("view3d.zoom_border", text="Zoom Border...")
		
		layout.itemS()

		layout.item_intO("view3d.layers", "nr", 0, text="Show All Layers")

		layout.itemS()
		
		layout.itemO("view3d.localview", text="View Global/Local")
		layout.itemO("view3d.view_center")
		layout.itemO("view3d.view_all")
		
		layout.itemS()
		
		layout.itemO("screen.region_foursplit", text="Toggle Quad View")
		layout.itemO("screen.screen_full_area", text="Toggle Full Screen")
		
		layout.itemS()
		
		layout.itemO("screen.animation_play", text="Playback Animation", icon='ICON_PLAY')
class VIEW3D_MT_view_navigation(bpy.types.Menu):
	bl_label = "Navigation"

	def draw(self, context):
		layout = self.layout

		layout.items_enumO("view3d.view_orbit", "type")
		
		layout.itemS()
		
		layout.items_enumO("view3d.view_pan", "type")
		
		layout.itemS()
		
		layout.item_floatO("view3d.zoom", "delta", 1.0, text="Zoom In")
		layout.item_floatO("view3d.zoom", "delta", -1.0, text="Zoom Out")
		
		layout.itemS()
		
		layout.itemO("view3d.fly")

class VIEW3D_MT_view_align(bpy.types.Menu):
	bl_label = "Align View"

	def draw(self, context):
		layout = self.layout
		
		layout.item_booleanO("view3d.view_all", "center", True, text="Center Cursor and View All")
		layout.itemO("view3d.camera_to_view", text="Align Active Camera to View")
		layout.itemO("view3d.view_center")
		
class VIEW3D_MT_view_cameras(bpy.types.Menu):
	bl_label = "Cameras"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("view3d.object_as_camera")
		layout.item_enumO("view3d.viewnumpad", "type", 'CAMERA', text="Active Camera")

# ********** Select menus, suffix from context.mode **********

class VIEW3D_MT_select_object(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()

		layout.itemO("object.select_all_toggle", text="Select/Deselect All")
		layout.itemO("object.select_inverse", text="Inverse")
		layout.itemO("object.select_random", text="Random")
		layout.itemO("object.select_mirror", text="Mirror")
		layout.itemO("object.select_by_layer", text="Select All by Layer")
		layout.item_menu_enumO("object.select_by_type", "type", "", text="Select All by Type...")
		layout.item_menu_enumO("object.select_grouped", "type", text="Select Grouped...")
		layout.itemO("object.select_pattern", text="Select Pattern...")

class VIEW3D_MT_select_pose(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border", text="Border Select...")

		layout.itemS()
		
		layout.itemO("pose.select_all_toggle", text="Select/Deselect All")
		layout.itemO("pose.select_inverse", text="Inverse")
		layout.itemO("pose.select_constraint_target", text="Constraint Target")
		layout.itemO("pose.select_linked", text="Linked")
		
		layout.itemS()
		
		layout.item_enumO("pose.select_hierarchy", "direction", 'PARENT')
		layout.item_enumO("pose.select_hierarchy", "direction", 'CHILD')
		
		layout.itemS()
		
		props = layout.itemO("pose.select_hierarchy", properties=True, text="Extend Parent")
		props.extend = True
		props.direction = 'PARENT'

		props = layout.itemO("pose.select_hierarchy", properties=True, text="Extend Child")
		props.extend = True
		props.direction = 'CHILD'

class VIEW3D_MT_select_particle(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()
		
		layout.itemO("particle.select_all_toggle", text="Select/Deselect All")
		layout.itemO("particle.select_linked")
		
		layout.itemS()
		
		layout.itemO("particle.select_more")
		layout.itemO("particle.select_less")

class VIEW3D_MT_select_edit_mesh(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border", text="Border Select...")

		layout.itemS()

		layout.itemO("mesh.select_all_toggle", text="Select/Deselect All")
		layout.itemO("mesh.select_inverse", text="Inverse")

		layout.itemS()

		layout.itemO("mesh.select_random", text="Random...")
		layout.itemO("mesh.edges_select_sharp", text="Sharp Edges")
		layout.itemO("mesh.faces_select_linked_flat", text="Linked Flat Faces")

		layout.itemS()

		layout.item_enumO("mesh.select_by_number_vertices", "type", 'TRIANGLES', text="Triangles")
		layout.item_enumO("mesh.select_by_number_vertices", "type", 'QUADS', text="Quads")
		layout.item_enumO("mesh.select_by_number_vertices", "type", 'OTHER', text="Loose Verts/Edges")
		layout.itemO("mesh.select_similar", text="Similar...")

		layout.itemS()

		layout.itemO("mesh.select_less", text="Less")
		layout.itemO("mesh.select_more", text="More")

		layout.itemS()
		
		layout.itemO("mesh.select_mirror", text="Mirror")

		layout.itemO("mesh.select_linked", text="Linked")
		layout.itemO("mesh.select_vertex_path", text="Vertex Path")
		layout.itemO("mesh.loop_multi_select", text="Edge Loop")
		layout.item_booleanO("mesh.loop_multi_select", "ring", True, text="Edge Ring")

		layout.itemS()

		layout.itemO("mesh.loop_to_region")
		layout.itemO("mesh.region_to_loop")

class VIEW3D_MT_select_edit_curve(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border", text="Border Select...")
		layout.itemO("view3d.select_circle", text="Circle Select...")

		layout.itemS()
		
		layout.itemO("curve.select_all_toggle", text="Select/Deselect All")
		layout.itemO("curve.select_inverse")
		layout.itemO("curve.select_random")
		layout.itemO("curve.select_every_nth")

		layout.itemS()
		
		layout.itemO("curve.de_select_first")
		layout.itemO("curve.de_select_last")
		layout.itemO("curve.select_next")
		layout.itemO("curve.select_previous")

		layout.itemS()
		
		layout.itemO("curve.select_more")
		layout.itemO("curve.select_less")

class VIEW3D_MT_select_edit_surface(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border", text="Border Select...")
		layout.itemO("view3d.select_circle", text="Circle Select...")

		layout.itemS()
		
		layout.itemO("curve.select_all_toggle", text="Select/Deselect All")
		layout.itemO("curve.select_inverse")
		layout.itemO("curve.select_random")
		layout.itemO("curve.select_every_nth")

		layout.itemS()
		
		layout.itemO("curve.select_row")

		layout.itemS()
		
		layout.itemO("curve.select_more")
		layout.itemO("curve.select_less")

class VIEW3D_MT_select_edit_metaball(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")
		
		layout.itemS()
		
		layout.itemO("mball.select_deselect_all_metaelems")
		layout.itemO("mball.select_inverse_metaelems")
		
		layout.itemS()
		
		layout.itemO("mball.select_random_metaelems")

class VIEW3D_MT_select_edit_lattice(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border")

		layout.itemS()
		
		layout.itemO("lattice.select_all_toggle", text="Select/Deselect All")

class VIEW3D_MT_select_edit_armature(bpy.types.Menu):
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.itemO("view3d.select_border", text="Border Select...")

		layout.itemS()
		
		layout.itemO("armature.select_all_toggle", text="Select/Deselect All")
		layout.itemO("armature.select_inverse", text="Inverse")

		layout.itemS()
		
		layout.item_enumO("armature.select_hierarchy", "direction", 'PARENT', text="Parent")
		layout.item_enumO("armature.select_hierarchy", "direction", 'CHILD', text="Child")
		
		layout.itemS()
		
		props = layout.itemO("armature.select_hierarchy", properties=True, text="Extend Parent")
		props.extend = True
		props.direction = 'PARENT'

		props = layout.itemO("armature.select_hierarchy", properties=True, text="Extend Child")
		props.extend = True
		props.direction = 'CHILD'

class VIEW3D_MT_select_face(bpy.types.Menu):# XXX no matching enum
	bl_label = "Select"

	def draw(self, context):
		layout = self.layout

		layout.view3d_select_faceselmenu()

# ********** Object menu **********

class VIEW3D_MT_object(bpy.types.Menu):
	bl_context = "objectmode"
	bl_label = "Object"

	def draw(self, context):
		layout = self.layout

		layout.itemM("VIEW3D_MT_object_clear")
		layout.itemM("VIEW3D_MT_object_apply")
		layout.itemM("VIEW3D_MT_snap")
		
		layout.itemS()
		
		layout.itemO("anim.insert_keyframe_menu", text="Insert Keyframe...")
		layout.itemO("anim.delete_keyframe_v3d", text="Delete Keyframe...")
		
		layout.itemS()
		
		layout.itemO("object.duplicate_move")
		layout.item_booleanO("object.duplicate", "linked", True, text="Duplicate Linked")
		layout.itemO("object.delete", text="Delete...")
		layout.itemO("object.proxy_make", text="Make Proxy...")
		layout.item_menu_enumO("object.make_local", "type", text="Make Local...")
		layout.itemM("VIEW3D_MT_make_single_user")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_object_parent")
		layout.itemM("VIEW3D_MT_object_track")
		layout.itemM("VIEW3D_MT_object_group")
		layout.itemM("VIEW3D_MT_object_constraints")
		
		layout.itemS()
		
		layout.itemO("object.join")
		
		layout.itemS()
		
		layout.itemO("object.move_to_layer", text="Move to Layer...")
		layout.itemM("VIEW3D_MT_object_showhide")
		
		layout.item_menu_enumO("object.convert", "target")
		
class VIEW3D_MT_object_clear(bpy.types.Menu):
	bl_label = "Clear"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("object.location_clear", text="Location")
		layout.itemO("object.rotation_clear", text="Rotation")
		layout.itemO("object.scale_clear", text="Scale")
		layout.itemO("object.origin_clear", text="Origin")
		
class VIEW3D_MT_object_apply(bpy.types.Menu):
	bl_label = "Apply"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("object.location_apply", text="Location")
		layout.itemO("object.rotation_apply", text="Rotation")
		layout.itemO("object.scale_apply", text="Scale")
		layout.itemS()
		layout.itemO("object.visual_transform_apply", text="Visual Transform")
		layout.itemO("object.duplicates_make_real")
		

class VIEW3D_MT_object_parent(bpy.types.Menu):
	bl_label = "Parent"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("object.parent_set", text="Set")
		layout.itemO("object.parent_clear", text="Clear")
		
class VIEW3D_MT_object_track(bpy.types.Menu):
	bl_label = "Track"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("object.track_set", text="Set")
		layout.itemO("object.track_clear", text="Clear")
		
class VIEW3D_MT_object_group(bpy.types.Menu):
	bl_label = "Group"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("group.group_create")
		layout.itemO("group.objects_remove")
		
		layout.itemS()
		
		layout.itemO("group.objects_add_active")
		layout.itemO("group.objects_remove_active")
		
class VIEW3D_MT_object_constraints(bpy.types.Menu):
	bl_label = "Constraints"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("object.constraint_add_with_targets")
		layout.itemO("object.constraints_clear")
		
class VIEW3D_MT_object_showhide(bpy.types.Menu):
	bl_label = "Show/Hide"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("object.restrictview_clear", text="Show Hidden")
		layout.itemO("object.restrictview_set", text="Hide Selected")
		layout.item_booleanO("object.restrictview_set", "unselected", True, text="Hide Unselected")

class VIEW3D_MT_make_single_user(bpy.types.Menu):
	bl_label = "Make Single User"

	def draw(self, context):
		layout = self.layout
		
		props = layout.itemO("object.make_single_user", properties=True, text="Object")
		props.object = True
		
		props = layout.itemO("object.make_single_user", properties=True, text="Object & ObData")
		props.object = props.obdata = True
		
		props = layout.itemO("object.make_single_user", properties=True, text="Object & ObData & Materials+Tex")
		props.object = props.obdata = props.material = props.texture = True
		
		props = layout.itemO("object.make_single_user", properties=True, text="Materials+Tex")
		props.material = props.texture = True
		
		props = layout.itemO("object.make_single_user", properties=True, text="Animation")
		props.animation = True

# ********** Vertex paint menu **********	
	
class VIEW3D_MT_paint_vertex(bpy.types.Menu):
	bl_label = "Paint"

	def draw(self, context):
		layout = self.layout
		
		sculpt = context.tool_settings.sculpt

		layout.itemO("paint.vertex_color_set")
		props = layout.itemO("paint.vertex_color_set", text="Set Selected Vertex Colors", properties=True)
		props.selected = True

# ********** Sculpt menu **********	
	
class VIEW3D_MT_sculpt(bpy.types.Menu):
	bl_label = "Sculpt"

	def draw(self, context):
		layout = self.layout
		
		sculpt = context.tool_settings.sculpt
		brush = context.tool_settings.sculpt.brush
		
		layout.itemR(sculpt, "symmetry_x")
		layout.itemR(sculpt, "symmetry_y")
		layout.itemR(sculpt, "symmetry_z")
		layout.itemS()
		layout.itemR(sculpt, "lock_x")
		layout.itemR(sculpt, "lock_y")
		layout.itemR(sculpt, "lock_z")
		layout.itemS()
		layout.item_menu_enumO("brush.curve_preset", property="shape")
		layout.itemS()
		
		if brush.sculpt_tool != 'GRAB':
			layout.itemR(brush, "use_airbrush")
			
			if brush.sculpt_tool != 'LAYER':
				layout.itemR(brush, "use_anchor")
			
			if brush.sculpt_tool in ('DRAW', 'PINCH', 'INFLATE', 'LAYER', 'CLAY'):
				layout.itemR(brush, "flip_direction")

			if brush.sculpt_tool == 'LAYER':
				layout.itemR(brush, "use_persistent")
				layout.itemO("sculpt.set_persistent_base")

# ********** Particle menu **********	
	
class VIEW3D_MT_particle(bpy.types.Menu):
	bl_label = "Particle"

	def draw(self, context):
		layout = self.layout
		
		particle_edit = context.tool_settings.particle_edit
		
		layout.itemO("particle.mirror")
		
		layout.itemS()
		
		layout.itemO("particle.remove_doubles")
		layout.itemO("particle.delete")
		
		if particle_edit.selection_mode == 'POINT':
			layout.itemO("particle.subdivide")
		
		layout.itemO("particle.rekey")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_particle_showhide")

class VIEW3D_MT_particle_showhide(VIEW3D_MT_showhide):
	_operator_name = "particle"

# ********** Pose Menu **********

class VIEW3D_MT_pose(bpy.types.Menu):
	bl_label = "Pose"

	def draw(self, context):
		layout = self.layout
		
		arm = context.active_object.data
		
		if arm.drawtype in ('BBONE', 'ENVELOPE'):
			layout.item_enumO("tfm.transform", "mode", 'BONESIZE', text="Scale Envelope Distance")
		
		layout.itemM("VIEW3D_MT_pose_transform")
		
		layout.itemS()
		
		layout.itemO("anim.insert_keyframe_menu", text="Insert Keyframe...")
		layout.itemO("anim.delete_keyframe_v3d", text="Delete Keyframe...")
		
		layout.itemS()
		
		layout.itemO("pose.apply")
		
		layout.itemS()
		
		layout.itemO("pose.copy")
		layout.itemO("pose.paste")
		layout.item_booleanO("pose.paste", "flipped", True, text="Paste X-Flipped Pose")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_pose_pose")
		layout.itemM("VIEW3D_MT_pose_motion")
		layout.itemM("VIEW3D_MT_pose_group")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_pose_ik")
		layout.itemM("VIEW3D_MT_pose_constraints")
		
		layout.itemS()
		
		layout.operator_context = "EXEC_AREA"
		layout.item_enumO("pose.autoside_names", "axis", 'XAXIS', text="AutoName Left/Right")
		layout.item_enumO("pose.autoside_names", "axis", 'YAXIS', text="AutoName Front/Back")
		layout.item_enumO("pose.autoside_names", "axis", 'ZAXIS', text="AutoName Top/Bottom")
		
		layout.itemO("pose.flip_names")
		
		layout.itemS()
		
		layout.operator_context = "INVOKE_AREA"
		layout.itemO("pose.armature_layers", text="Change Armature Layers...")
		layout.itemO("pose.bone_layers", text="Change Bone Layers...")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_pose_showhide")
		layout.item_menu_enumO("pose.flags_set", 'mode', text="Bone Settings")

class VIEW3D_MT_pose_transform(bpy.types.Menu):
	bl_label = "Clear Transform"

	def draw(self, context):
		layout = self.layout
		
		layout.itemL(text="User Transform")
		
		layout.itemO("pose.loc_clear", text="Location")
		layout.itemO("pose.rot_clear", text="Rotation")
		layout.itemO("pose.scale_clear", text="Scale")
		
		layout.itemL(text="Origin")
		
class VIEW3D_MT_pose_pose(bpy.types.Menu):
	bl_label = "Pose Library"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("poselib.browse_interactive", text="Browse Poses...")
		
		layout.itemS()
		
		layout.itemO("poselib.pose_add", text="Add Pose...")
		layout.itemO("poselib.pose_rename", text="Rename Pose...")
		layout.itemO("poselib.pose_remove", text="Remove Pose...")

class VIEW3D_MT_pose_motion(bpy.types.Menu):
	bl_label = "Motion Paths"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("pose.paths_calculate", text="Calculate")
		layout.itemO("pose.paths_clear", text="Clear")
		
class VIEW3D_MT_pose_group(bpy.types.Menu):
	bl_label = "Bone Groups"

	def draw(self, context):
		layout = self.layout
		layout.itemO("pose.group_add")
		layout.itemO("pose.group_remove")
		
		layout.itemS()
		
		layout.itemO("pose.group_assign")
		layout.itemO("pose.group_unassign")
		
		
class VIEW3D_MT_pose_ik(bpy.types.Menu):
	bl_label = "Inverse Kinematics"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("pose.ik_add")
		layout.itemO("pose.ik_clear")
		
class VIEW3D_MT_pose_constraints(bpy.types.Menu):
	bl_label = "Constraints"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("pose.constraint_add_with_targets", text="Add (With Targets)...")
		layout.itemO("pose.constraints_clear")
		
class VIEW3D_MT_pose_showhide(VIEW3D_MT_showhide):
	_operator_name = "pose"

# ********** Edit Menus, suffix from ob.type **********

# Edit MESH
class VIEW3D_MT_edit_mesh(bpy.types.Menu):
	bl_label = "Mesh"

	def draw(self, context):
		layout = self.layout
		
		settings = context.tool_settings

		layout.itemO("ed.undo")
		layout.itemO("ed.redo")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_snap")
		
		layout.itemS()
		
		layout.itemO("uv.mapping_menu", text="UV Unwrap...")
		
		layout.itemS()
		
		layout.itemO("mesh.extrude_move")
		layout.itemO("mesh.duplicate_move")
		layout.itemO("mesh.delete", text="Delete...")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_edit_mesh_vertices")
		layout.itemM("VIEW3D_MT_edit_mesh_edges")
		layout.itemM("VIEW3D_MT_edit_mesh_faces")
		layout.itemM("VIEW3D_MT_edit_mesh_normals")
		
		layout.itemS()
		
		layout.itemR(settings, "automerge_editing")
		layout.item_menu_enumR(settings, "proportional_editing")
		layout.item_menu_enumR(settings, "proportional_editing_falloff")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_edit_mesh_showhide")

# Only used by the menu
class VIEW3D_MT_edit_mesh_specials(bpy.types.Menu):
	bl_label = "Specials"

	def draw(self, context):
		layout = self.layout
		
		layout.operator_context = 'INVOKE_REGION_WIN'
		
		layout.itemO("mesh.subdivide", text="Subdivide")
		layout.item_floatO("mesh.subdivide", "smoothness", 1.0, text="Subdivide Smooth")
		layout.itemO("mesh.merge", text="Merge...")
		layout.itemO("mesh.remove_doubles")
		layout.itemO("mesh.hide", text="Hide")
		layout.itemO("mesh.reveal", text="Reveal")
		layout.itemO("mesh.select_inverse")
		layout.itemO("mesh.flip_normals")
		layout.itemO("mesh.vertices_smooth", text="Smooth")
		# layout.itemO("mesh.bevel", text="Bevel")
		layout.itemO("mesh.faces_shade_smooth")
		layout.itemO("mesh.faces_shade_flat")
		layout.itemO("mesh.blend_from_shape")
		layout.itemO("mesh.shape_propagate_to_all")
		layout.itemO("mesh.select_vertex_path")

class VIEW3D_MT_edit_mesh_vertices(bpy.types.Menu):
	bl_label = "Vertices"

	def draw(self, context):
		layout = self.layout
		layout.operator_context = 'INVOKE_REGION_WIN'
		
		layout.itemO("mesh.merge")
		layout.itemO("mesh.rip")
		layout.itemO("mesh.split")
		layout.itemO("mesh.separate")

		layout.itemS()
		
		layout.itemO("mesh.vertices_smooth")
		layout.itemO("mesh.remove_doubles")
		
		layout.itemO("mesh.select_vertex_path")
		
		layout.itemO("mesh.blend_from_shape")
		
		layout.itemO("object.vertex_group_blend")
		layout.itemO("mesh.shape_propagate_to_all")

class VIEW3D_MT_edit_mesh_edges(bpy.types.Menu):
	bl_label = "Edges"

	def draw(self, context):
		layout = self.layout
		layout.operator_context = 'INVOKE_REGION_WIN'
		
		layout.itemO("mesh.edge_face_add")
		layout.itemO("mesh.subdivide")

		layout.itemS()
		
		layout.itemO("mesh.mark_seam")
		layout.item_booleanO("mesh.mark_seam", "clear", True, text="Clear Seam")
		
		layout.itemS()
		
		layout.itemO("mesh.mark_sharp")
		layout.item_booleanO("mesh.mark_sharp", "clear", True, text="Clear Sharp")
		
		layout.itemS()
		
		layout.item_enumO("mesh.edge_rotate", "direction", 'CW', text="Rotate Edge CW")
		layout.item_enumO("mesh.edge_rotate", "direction", 'CCW', text="Rotate Edge CCW")

		layout.itemS()
		
		layout.itemO("TFM_OT_edge_slide", text="Edge Slide")
		layout.itemO("mesh.loop_multi_select", text="Edge Loop")

		# uiItemO(layout, "Loopcut", 0, "mesh.loop_cut"); // CutEdgeloop(em, 1);
		# uiItemO(layout, "Edge Slide", 0, "mesh.edge_slide"); // EdgeSlide(em, 0,0.0);
		
		layout.item_booleanO("mesh.loop_multi_select", "ring", True, text="Edge Ring")
		
		layout.itemO("mesh.loop_to_region")
		layout.itemO("mesh.region_to_loop")


class VIEW3D_MT_edit_mesh_faces(dynamic_menu.DynMenu):
	bl_label = "Faces"

	def draw(self, context):
		layout = self.layout
		layout.operator_context = 'INVOKE_REGION_WIN'
		
		layout.itemO("mesh.flip_normals")
		# layout.itemO("mesh.bevel")
		# layout.itemO("mesh.bevel")
		layout.itemO("mesh.edge_face_add")
		layout.itemO("mesh.fill")
		layout.itemO("mesh.beauty_fill")

		layout.itemS()
		
		layout.itemO("mesh.quads_convert_to_tris")
		layout.itemO("mesh.tris_convert_to_quads")
		layout.itemO("mesh.edge_flip")
		
		layout.itemS()
		
		layout.itemO("mesh.faces_shade_smooth")
		layout.itemO("mesh.faces_shade_flat")
		
		layout.itemS()

		# uiItemO(layout, NULL, 0, "mesh.face_mode"); // mesh_set_face_flags(em, 1);
		# uiItemBooleanO(layout, NULL, 0, "mesh.face_mode", "clear", 1); // mesh_set_face_flags(em, 0);
		
		layout.item_enumO("mesh.edge_rotate", "direction", 'CW', text="Rotate Edge CW")
		
		layout.itemS()
		
		layout.item_menu_enumO("mesh.uvs_rotate", "direction")
		layout.item_menu_enumO("mesh.uvs_mirror", "axis")
		layout.item_menu_enumO("mesh.colors_rotate", "direction")
		layout.item_menu_enumO("mesh.colors_mirror", "axis")


class VIEW3D_MT_edit_mesh_normals(bpy.types.Menu):
	bl_label = "Normals"

	def draw(self, context):
		layout = self.layout

		layout.itemO("mesh.normals_make_consistent", text="Recalculate Outside")
		layout.item_booleanO("mesh.normals_make_consistent", "inside", True, text="Recalculate Inside")

		layout.itemS()
		
		layout.itemO("mesh.flip_normals")
		
class VIEW3D_MT_edit_mesh_showhide(VIEW3D_MT_showhide):
	_operator_name = "mesh"

# Edit Curve

# draw_curve is used by VIEW3D_MT_edit_curve and VIEW3D_MT_edit_surface
def draw_curve(self, context):
	layout = self.layout
	
	settings = context.tool_settings
	
	layout.itemM("VIEW3D_MT_snap")
	
	layout.itemS()
	
	layout.itemO("curve.extrude")
	layout.itemO("curve.duplicate")
	layout.itemO("curve.separate")
	layout.itemO("curve.make_segment")
	layout.itemO("curve.cyclic_toggle")
	layout.itemO("curve.delete", text="Delete...")
	
	layout.itemS()
	
	layout.itemM("VIEW3D_MT_edit_curve_ctrlpoints")
	layout.itemM("VIEW3D_MT_edit_curve_segments")
	
	layout.itemS()
	
	layout.itemR(settings, "proportional_editing")
	layout.item_menu_enumR(settings, "proportional_editing_falloff")
	
	layout.itemS()
	
	layout.itemM("VIEW3D_MT_edit_curve_showhide")

class VIEW3D_MT_edit_curve(bpy.types.Menu):
	bl_label = "Curve"

	draw = draw_curve
	
class VIEW3D_MT_edit_curve_ctrlpoints(bpy.types.Menu):
	bl_label = "Control Points"

	def draw(self, context):
		layout = self.layout
		
		edit_object = context.edit_object
		
		if edit_object.type == 'CURVE':
			layout.item_enumO("tfm.transform", "mode", 'TILT')
			layout.itemO("curve.tilt_clear")
			layout.itemO("curve.separate")
			
			layout.itemS()
			
			layout.item_menu_enumO("curve.handle_type_set", "type")
		
class VIEW3D_MT_edit_curve_segments(bpy.types.Menu):
	bl_label = "Segments"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("curve.subdivide")
		layout.itemO("curve.switch_direction")

class VIEW3D_MT_edit_curve_showhide(VIEW3D_MT_showhide):
	_operator_name = "curve"

# Edit SURFACE
class VIEW3D_MT_edit_surface(bpy.types.Menu):
	bl_label = "Surface"

	draw = draw_curve

# Edit TEXT
class VIEW3D_MT_edit_text(bpy.types.Menu):
	bl_label = "Text"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("font.file_paste")
		
		layout.itemS()
		
		layout.itemm("view3d_mt_edit_text_chars")

class VIEW3D_MT_edit_text_chars(bpy.types.Menu):
	bl_label = "Special Characters"

	def draw(self, context):
		layout = self.layout
		
		layout.item_stringO("font.text_insert", "text", b'\xC2\xA9'.decode(), text="Copyright|Alt C")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xAE'.decode(), text="Registered Trademark|Alt R")
		
		layout.itemS()
		
		layout.item_stringO("font.text_insert", "text", b'\xC2\xB0'.decode(), text="Degree Sign|Alt G")
		layout.item_stringO("font.text_insert", "text", b'\xC3\x97'.decode(), text="Multiplication Sign|Alt x")
		layout.item_stringO("font.text_insert", "text", b'\xC2\x8A'.decode(), text="Circle|Alt .")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xB9'.decode(), text="Superscript 1|Alt 1")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xB2'.decode(), text="Superscript 2|Alt 2")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xB3'.decode(), text="Superscript 3|Alt 3")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xBB'.decode(), text="Double >>|Alt >")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xAB'.decode(), text="Double <<|Alt <")
		layout.item_stringO("font.text_insert", "text", b'\xE2\x80\xB0'.decode(), text="Promillage|Alt %")
		
		layout.itemS()
		
		layout.item_stringO("font.text_insert", "text", b'\xC2\xA4'.decode(), text="Dutch Florin|Alt F")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xA3'.decode(), text="British Pound|Alt L")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xA5'.decode(), text="Japanese Yen|Alt Y")
		
		layout.itemS()
		
		layout.item_stringO("font.text_insert", "text", b'\xC3\x9F'.decode(), text="German S|Alt S")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xBF'.decode(), text="Spanish Question Mark|Alt ?")
		layout.item_stringO("font.text_insert", "text", b'\xC2\xA1'.decode(), text="Spanish Exclamation Mark|Alt !")

# Edit META
class VIEW3D_MT_edit_meta(bpy.types.Menu):
	bl_label = "Metaball"

	def draw(self, context):
		layout = self.layout
		
		settings = context.tool_settings

		layout.itemO("ed.undo")
		layout.itemO("ed.redo")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_snap")
		
		layout.itemS()
		
		layout.itemO("mball.delete_metaelems", text="Delete...")
		layout.itemO("mball.duplicate_metaelems")
		
		layout.itemS()
		
		layout.itemR(settings, "proportional_editing")
		layout.item_menu_enumR(settings, "proportional_editing_falloff")
		
		layout.itemS()
		
		layout.itemM("VIEW3D_MT_edit_meta_showhide")

class VIEW3D_MT_edit_meta_showhide(bpy.types.Menu):
	bl_label = "Show/Hide"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("mball.reveal_metaelems", text="Show Hidden")
		layout.itemO("mball.hide_metaelems", text="Hide Selected")
		layout.item_booleanO("mball.hide_metaelems", "unselected", True, text="Hide Unselected")

# Edit LATTICE
class VIEW3D_MT_edit_lattice(bpy.types.Menu):
	bl_label = "Lattice"

	def draw(self, context):
		layout = self.layout
		
		settings = context.tool_settings

		layout.itemM("VIEW3D_MT_snap")
		
		layout.itemS()
		
		layout.itemO("lattice.make_regular")
		
		layout.itemS()
		
		layout.itemR(settings, "proportional_editing")
		layout.item_menu_enumR(settings, "proportional_editing_falloff")

# Edit ARMATURE
class VIEW3D_MT_edit_armature(bpy.types.Menu):
	bl_label = "Armature"

	def draw(self, context):
		layout = self.layout
		
		edit_object = context.edit_object
		arm = edit_object.data
		
		layout.itemM("VIEW3D_MT_snap")
		layout.itemM("VIEW3D_MT_edit_armature_roll")
		
		if arm.drawtype == 'ENVELOPE':
			layout.item_enumO("tfm.transform", "mode", 'BONESIZE', text="Scale Envelope Distance")
		else:
			layout.item_enumO("tfm.transform", "mode", 'BONESIZE', text="Scale B-Bone Width")
				
		layout.itemS()
		
		layout.itemO("armature.extrude_move")

# EXTRUDE FORKED DOESN'T WORK YET		
#		if arm.x_axis_mirror:
#			layout.item_booleanO("armature.extrude_move", "forked", True, text="Extrude Forked")
		
		layout.itemO("armature.duplicate_move")
		layout.itemO("armature.merge")
		layout.itemO("armature.fill")
		layout.itemO("armature.delete")
		layout.itemO("armature.separate")

		layout.itemS()

		layout.itemO("armature.subdivide_multi", text="Subdivide")
		
		layout.itemS()
		
		layout.operator_context = "EXEC_AREA"
		layout.item_enumO("armature.autoside_names", "type", 'XAXIS', text="AutoName Left/Right")
		layout.item_enumO("armature.autoside_names", "type", 'YAXIS', text="AutoName Front/Back")
		layout.item_enumO("armature.autoside_names", "type", 'ZAXIS', text="AutoName Top/Bottom")
		layout.itemO("armature.flip_names")

		layout.itemS()
		
		layout.operator_context = "INVOKE_DEFAULT"
		layout.itemO("armature.armature_layers")
		layout.itemO("armature.bone_layers")

		layout.itemS()

		layout.itemM("VIEW3D_MT_edit_armature_parent")

		layout.itemS()
		
		layout.item_menu_enumO("armature.flags_set", "mode", text="Bone Settings")

class VIEW3D_MT_edit_armature_parent(bpy.types.Menu):
	bl_label = "Parent"

	def draw(self, context):
		layout = self.layout
		
		layout.itemO("armature.parent_set", text="Make")
		layout.itemO("armature.parent_clear", text="Clear")

class VIEW3D_MT_edit_armature_roll(bpy.types.Menu):
	bl_label = "Bone Roll"

	def draw(self, context):
		layout = self.layout
		
		layout.item_enumO("armature.calculate_roll", "type", 'GLOBALUP', text="Clear Roll (Z-Axis Up)")
		layout.item_enumO("armature.calculate_roll", "type", 'CURSOR', text="Roll to Cursor")
		
		layout.itemS()
		
		layout.item_enumO("tfm.transform", "mode", 'BONE_ROLL', text="Set Roll")

# ********** Panel **********

class VIEW3D_PT_3dview_properties(bpy.types.Panel):
	bl_space_type = 'VIEW_3D'
	bl_region_type = 'UI'
	bl_label = "View"

	def poll(self, context):
		view = context.space_data
		return (view)

	def draw(self, context):
		layout = self.layout
		
		view = context.space_data
		scene = context.scene
		
		col = layout.column()
		col.itemL(text="Camera:")
		col.itemR(view, "camera", text="")
		col.itemR(view, "lens")
		
		col = layout.column(align=True)
		col.itemL(text="Clip:")
		col.itemR(view, "clip_start", text="Start")
		col.itemR(view, "clip_end", text="End")
		
		col = layout.column(align=True)
		col.itemL(text="Grid:")
		col.itemR(view, "grid_lines", text="Lines")
		col.itemR(view, "grid_spacing", text="Spacing")
		col.itemR(view, "grid_subdivisions", text="Subdivisions")
		
		layout.column().itemR(scene, "cursor_location", text="3D Cursor:")
		
class VIEW3D_PT_3dview_display(bpy.types.Panel):
	bl_space_type = 'VIEW_3D'
	bl_region_type = 'UI'
	bl_label = "Display"
	bl_default_closed = True
	
	def poll(self, context):
		view = context.space_data
		return (view)

	def draw(self, context):
		layout = self.layout

		view = context.space_data
		gs = context.scene.game_data
		ob = context.object
		
		col = layout.column()
		col.itemR(view, "display_floor", text="Grid Floor")
		col.itemR(view, "display_x_axis", text="X Axis")
		col.itemR(view, "display_y_axis", text="Y Axis")
		col.itemR(view, "display_z_axis", text="Z Axis")
		col.itemR(view, "outline_selected")
		col.itemR(view, "all_object_centers")
		col.itemR(view, "relationship_lines")
		if ob and ob.type =='MESH':
			mesh = context.active_object.data
			col.itemR(mesh, "all_edges")
		
		col = layout.column()
		col.itemL(text="Shading:")
		col.itemR(gs, "material_mode", text="")
		col.itemR(view, "textured_solid")

# XXX - the Quad View options don't work yet		
#		layout.itemS()
#		
#		layout.itemO("screen.region_foursplit", text="Toggle Quad View")
#		col = layout.column()
#		col.itemR(view, "lock_rotation")
#		col.itemR(view, "box_preview")
#		col.itemR(view, "box_clip")

class VIEW3D_PT_3dview_meshdisplay(bpy.types.Panel):
	bl_space_type = 'VIEW_3D'
	bl_region_type = 'UI'
	bl_label = "Mesh Display"

	def poll(self, context):
		editmesh = context.mode == 'EDIT_MESH'
		return (editmesh)

	def draw(self, context):
		layout = self.layout

		mesh = context.active_object.data
		
		col = layout.column()
		col.itemL(text="Overlays:")
		col.itemR(mesh, "draw_edges", text="Edges")
		col.itemR(mesh, "draw_faces", text="Faces")
		col.itemR(mesh, "draw_creases", text="Creases")
		col.itemR(mesh, "draw_bevel_weights", text="Bevel Weights")
		col.itemR(mesh, "draw_seams", text="Seams")
		col.itemR(mesh, "draw_sharp", text="Sharp")
		
		col.itemS()
		col.itemL(text="Normals:")
		col.itemR(mesh, "draw_normals", text="Face")
		col.itemR(mesh, "draw_vertex_normals", text="Vertex")
		col.itemR(context.scene.tool_settings, "normal_size", text="Normal Size")
		
		col.itemS()
		col.itemL(text="Numerics:")
		col.itemR(mesh, "draw_edge_lenght")
		col.itemR(mesh, "draw_edge_angle")
		col.itemR(mesh, "draw_face_area")

class VIEW3D_PT_3dview_curvedisplay(bpy.types.Panel):
	bl_space_type = 'VIEW_3D'
	bl_region_type = 'UI'
	bl_label = "Curve Display"

	def poll(self, context):
		editmesh = context.mode == 'EDIT_CURVE'
		return (editmesh)

	def draw(self, context):
		layout = self.layout

		curve = context.active_object.data
		
		col = layout.column()
		col.itemL(text="Overlays:")
		col.itemR(curve, "draw_handles", text="Handles")
		col.itemR(curve, "draw_normals", text="Normals")
		col.itemR(context.scene.tool_settings, "normal_size", text="Normal Size")

class VIEW3D_PT_background_image(bpy.types.Panel):
	bl_space_type = 'VIEW_3D'
	bl_region_type = 'UI'
	bl_label = "Background Image"
	bl_default_closed = True

	def poll(self, context):
		view = context.space_data
		bg = context.space_data.background_image
		return (view)

	def draw_header(self, context):
		layout = self.layout
		view = context.space_data

		layout.itemR(view, "display_background_image", text="")

	def draw(self, context):
		layout = self.layout
		
		view = context.space_data
		bg = view.background_image

		if bg:
			layout.active = view.display_background_image

			col = layout.column()
			col.itemR(bg, "image", text="")
			#col.itemR(bg, "image_user")
			col.itemR(bg, "size")
			col.itemR(bg, "transparency", slider=True)
			
			
			col = layout.column(align=True)
			col.itemL(text="Offset:")
			col.itemR(bg, "offset_x", text="X")
			col.itemR(bg, "offset_y", text="Y")

class VIEW3D_PT_transform_orientations(bpy.types.Panel):
	bl_space_type = 'VIEW_3D'
	bl_region_type = 'UI'
	bl_label = "Transform Orientations"
	bl_default_closed = True

	def poll(self, context):
		view = context.space_data
		return (view)

	def draw(self, context):
		layout = self.layout
		
		view = context.space_data

		col = layout.column()

		col.itemR(view, "transform_orientation")
		col.itemO("tfm.create_orientation", text="Create")
		
		orientation = view.current_orientation
		
		if orientation:
			col.itemR(orientation, "name")
			col.itemO("tfm.delete_orientation", text="Delete")

class VIEW3D_PT_etch_a_ton(bpy.types.Panel):
	bl_space_type = 'VIEW_3D'
	bl_region_type = 'UI'
	bl_label = "Skeleton Sketching"
	bl_default_closed = True

	def poll(self, context):
		scene = context.space_data
		ob = context.active_object
		return scene and ob and ob.type == 'ARMATURE' and ob.mode == 'EDIT' 

	def draw_header(self, context):
		layout = self.layout
		toolsettings = context.scene.tool_settings

		layout.itemR(toolsettings, "bone_sketching", text="")

	def draw(self, context):
		layout = self.layout
		toolsettings = context.scene.tool_settings

		col = layout.column()

		col.itemR(toolsettings, "etch_quick")
		col.itemR(toolsettings, "etch_overdraw")

		col.itemR(toolsettings, "etch_convert_mode")
		
		if toolsettings.etch_convert_mode == "LENGTH":
			col.itemR(toolsettings, "etch_length_limit")
		elif toolsettings.etch_convert_mode == "ADAPTIVE":
			col.itemR(toolsettings, "etch_adaptive_limit")
		elif toolsettings.etch_convert_mode == "FIXED":
			col.itemR(toolsettings, "etch_subdivision_number")
		elif toolsettings.etch_convert_mode == "RETARGET":
			col.itemR(toolsettings, "etch_template")
			col.itemR(toolsettings, "etch_roll_mode")
			col.itemR(toolsettings, "etch_autoname")
			col.itemR(toolsettings, "etch_number")
			col.itemR(toolsettings, "etch_side")
		

# Operators 
from bpy.props import *


class OBJECT_OT_select_pattern(bpy.types.Operator):
	'''Select object matching a naming pattern.'''
	bl_idname = "object.select_pattern"
	bl_label = "Select Pattern"
	bl_register = True
	bl_undo = True
	
	pattern = StringProperty(name="Pattern", description="Name filter using '*' and '?' wildcard chars", maxlen= 32, default= "*")
	case_sensitive = BoolProperty(name="Case Sensitive", description="Do a case sensitive compare", default= False)
	extend = BoolProperty(name="Extend", description="Extend the existing selection", default= True)
	
	
	def execute(self, context):
	
		import fnmatch
		if self.case_sensitive:	pattern_match = fnmatch.fnmatchcase
		else:					pattern_match = lambda a, b: fnmatch.fnmatchcase(a.upper(), b.upper())

		for ob in context.visible_objects:
			if pattern_match(ob.name, self.pattern):
				ob.selected = True
			elif not self.extend:
				ob.selected = False

		return ('FINISHED',)
		
		# TODO - python cant do popups yet
	'''
	def invoke(self, context, event):	
		wm = context.manager
		wm.add_fileselect(self.__operator__)
		return ('RUNNING_MODAL',)
	'''

bpy.types.register(VIEW3D_HT_header) # Header

bpy.types.register(VIEW3D_MT_view) #View Menus
bpy.types.register(VIEW3D_MT_view_navigation)
bpy.types.register(VIEW3D_MT_view_align)
bpy.types.register(VIEW3D_MT_view_cameras)

bpy.types.register(VIEW3D_MT_select_object) # Select Menus
bpy.types.register(VIEW3D_MT_select_pose)
bpy.types.register(VIEW3D_MT_select_particle)
bpy.types.register(VIEW3D_MT_select_edit_mesh)
bpy.types.register(VIEW3D_MT_select_edit_curve)
bpy.types.register(VIEW3D_MT_select_edit_surface)
bpy.types.register(VIEW3D_MT_select_edit_metaball)
bpy.types.register(VIEW3D_MT_select_edit_lattice)
bpy.types.register(VIEW3D_MT_select_edit_armature)
bpy.types.register(VIEW3D_MT_select_face) # XXX todo

bpy.types.register(VIEW3D_MT_object) # Object Menu
bpy.types.register(VIEW3D_MT_object_apply)
bpy.types.register(VIEW3D_MT_object_clear)
bpy.types.register(VIEW3D_MT_object_parent)
bpy.types.register(VIEW3D_MT_object_track)
bpy.types.register(VIEW3D_MT_object_group)
bpy.types.register(VIEW3D_MT_object_constraints)
bpy.types.register(VIEW3D_MT_object_showhide)
bpy.types.register(VIEW3D_MT_make_single_user)


bpy.types.register(VIEW3D_MT_sculpt) # Sculpt Menu

bpy.types.register(VIEW3D_MT_paint_vertex)

bpy.types.register(VIEW3D_MT_particle) # Particle Menu
bpy.types.register(VIEW3D_MT_particle_showhide)

bpy.types.register(VIEW3D_MT_pose) # POSE Menu
bpy.types.register(VIEW3D_MT_pose_transform)
bpy.types.register(VIEW3D_MT_pose_pose)
bpy.types.register(VIEW3D_MT_pose_motion)
bpy.types.register(VIEW3D_MT_pose_group)
bpy.types.register(VIEW3D_MT_pose_ik)
bpy.types.register(VIEW3D_MT_pose_constraints)
bpy.types.register(VIEW3D_MT_pose_showhide)

bpy.types.register(VIEW3D_MT_snap) # Edit Menus

bpy.types.register(VIEW3D_MT_edit_mesh)
bpy.types.register(VIEW3D_MT_edit_mesh_specials) # Only as a menu for keybindings
bpy.types.register(VIEW3D_MT_edit_mesh_vertices)
bpy.types.register(VIEW3D_MT_edit_mesh_edges)
bpy.types.register(VIEW3D_MT_edit_mesh_faces)
bpy.types.register(VIEW3D_MT_edit_mesh_normals)
bpy.types.register(VIEW3D_MT_edit_mesh_showhide)

bpy.types.register(VIEW3D_MT_edit_curve)
bpy.types.register(VIEW3D_MT_edit_curve_ctrlpoints)
bpy.types.register(VIEW3D_MT_edit_curve_segments)
bpy.types.register(VIEW3D_MT_edit_curve_showhide)

bpy.types.register(VIEW3D_MT_edit_surface)

bpy.types.register(VIEW3D_MT_edit_text)
bpy.types.register(VIEW3D_MT_edit_text_chars)

bpy.types.register(VIEW3D_MT_edit_meta)
bpy.types.register(VIEW3D_MT_edit_meta_showhide)

bpy.types.register(VIEW3D_MT_edit_lattice)

bpy.types.register(VIEW3D_MT_edit_armature)
bpy.types.register(VIEW3D_MT_edit_armature_parent)
bpy.types.register(VIEW3D_MT_edit_armature_roll)

bpy.types.register(VIEW3D_PT_3dview_properties) # Panels
bpy.types.register(VIEW3D_PT_3dview_display)
bpy.types.register(VIEW3D_PT_3dview_meshdisplay)
bpy.types.register(VIEW3D_PT_3dview_curvedisplay)
bpy.types.register(VIEW3D_PT_background_image)
bpy.types.register(VIEW3D_PT_transform_orientations)
bpy.types.register(VIEW3D_PT_etch_a_ton)

bpy.ops.add(OBJECT_OT_select_pattern)

