# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

"""
This module has utility functions for renaming
rna values in fcurves and drivers.

The main function to use is: update_data_paths(...)
"""

IS_TESTING = False


def drepr(string):
    # is there a less crappy way to do this in python?, re.escape also escapes
    # single quotes strings so cant use it.
    return '"%s"' % repr(string)[1:-1].replace("\"", "\\\"").replace("\\'", "'")


class DataPathBuilder(object):
    """ Dummy class used to parse fcurve and driver data paths.
    """
    __slots__ = ("data_path", )

    def __init__(self, attrs):
        self.data_path = attrs

    def __getattr__(self, attr):
        str_value = ".%s" % attr
        return DataPathBuilder(self.data_path + (str_value, ))

    def __getitem__(self, key):
        if type(key) is int:
            str_value = '[%d]' % key
        elif type(key) is str:
            str_value = '[%s]' % drepr(key)
        else:
            raise Exception("unsupported accessor %r of type %r (internal error)" % (key, type(key)))
        return DataPathBuilder(self.data_path + (str_value, ))

    def resolve(self, real_base, rna_update_from_map=None):
        """ Return (attribute, value) pairs.
        """
        pairs = []
        base = real_base
        for item in self.data_path:
            if base is not Ellipsis:
                try:
                    # this only works when running with an old blender
                    # where the old path will resolve
                    base = eval("base" + item)
                except:
                    base_new = Ellipsis
                    # guess the new name
                    if item.startswith("."):
                        for item_new in rna_update_from_map.get(item[1:], ()):
                            try:
                                print("base." + item_new)
                                base_new = eval("base." + item_new)
                                break  # found, don't keep looking
                            except:
                                pass

                    if base_new is Ellipsis:
                        print("Failed to resolve data path:", self.data_path)
                    base = base_new

            pairs.append((item, base))
        return pairs

import bpy


def id_iter():
    type_iter = type(bpy.data.objects)

    for attr in dir(bpy.data):
        data_iter = getattr(bpy.data, attr, None)
        if type(data_iter) == type_iter:
            for id_data in data_iter:
                if id_data.library is None:
                    yield id_data


def anim_data_actions(anim_data):
    actions = []
    actions.append(anim_data.action)
    for track in anim_data.nla_tracks:
        for strip in track.strips:
            actions.append(strip.action)

    # filter out None
    return [act for act in actions if act]


def classes_recursive(base_type, clss=None):
    if clss is None:
        clss = [base_type]
    else:
        clss.append(base_type)

    for base_type_iter in base_type.__bases__:
        if base_type_iter is not object:
            classes_recursive(base_type_iter, clss)

    return clss


def find_path_new(id_data, data_path, rna_update_dict, rna_update_from_map):
    # note!, id_data can be ID type or a node tree
    # ignore ID props for now
    if data_path.startswith("["):
        return data_path

    # recursive path fixing, likely will be one in most cases.
    data_path_builder = eval("DataPathBuilder(tuple())." + data_path)
    data_resolve = data_path_builder.resolve(id_data, rna_update_from_map)

    path_new = [pair[0] for pair in data_resolve]

    # print(data_resolve)
    data_base = id_data

    for i, (attr, data) in enumerate(data_resolve):
        if data is Ellipsis:
            break

        if attr.startswith("."):
            # try all classes
            for data_base_type in classes_recursive(type(data_base)):
                attr_new = rna_update_dict.get(data_base_type.__name__, {}).get(attr[1:])
                if attr_new:
                    path_new[i] = "." + attr_new

        # set this as the base for further properties
        data_base = data

    data_path_new = "".join(path_new)[1:]  # skip the first "."
    return data_path_new


def update_data_paths(rna_update):
    ''' rna_update triple [(class_name, from, to), ...]
    '''

    # make a faster lookup dict
    rna_update_dict = {}
    for ren_class, ren_from, ren_to in rna_update:
        rna_update_dict.setdefault(ren_class, {})[ren_from] = ren_to

    rna_update_from_map = {}
    for ren_class, ren_from, ren_to in rna_update:
        rna_update_from_map.setdefault(ren_from, []).append(ren_to)

    for id_data in id_iter():

        # check node-trees too
        anim_data_ls = [(id_data, getattr(id_data, "animation_data", None))]
        node_tree = getattr(id_data, "node_tree", None)
        if node_tree:
            anim_data_ls.append((node_tree, node_tree.animation_data))

        for anim_data_base, anim_data in anim_data_ls:
            if anim_data is None:
                continue

            for fcurve in anim_data.drivers:
                data_path = fcurve.data_path
                data_path_new = find_path_new(anim_data_base, data_path, rna_update_dict, rna_update_from_map)
                # print(data_path_new)
                if data_path_new != data_path:
                    if not IS_TESTING:
                        fcurve.data_path = data_path_new
                        fcurve.driver.is_valid = True  # reset to allow this to work again
                    print("driver-fcurve (%s): %s -> %s" % (id_data.name, data_path, data_path_new))

                for var in fcurve.driver.variables:
                    if var.type == 'SINGLE_PROP':
                        for tar in var.targets:
                            id_data_other = tar.id
                            data_path = tar.data_path

                            if id_data_other and data_path:
                                data_path_new = find_path_new(id_data_other, data_path, rna_update_dict, rna_update_from_map)
                                # print(data_path_new)
                                if data_path_new != data_path:
                                    if not IS_TESTING:
                                        tar.data_path = data_path_new
                                    print("driver (%s): %s -> %s" % (id_data_other.name, data_path, data_path_new))

            for action in anim_data_actions(anim_data):
                for fcu in action.fcurves:
                    data_path = fcu.data_path
                    data_path_new = find_path_new(anim_data_base, data_path, rna_update_dict, rna_update_from_map)
                    # print(data_path_new)
                    if data_path_new != data_path:
                        if not IS_TESTING:
                            fcu.data_path = data_path_new
                        print("fcurve (%s): %s -> %s" % (id_data.name, data_path, data_path_new))


# we could have this data in its own file but no point really
data_2_56_to_2_59 = (
    ("ClothCollisionSettings", "min_distance", "distance_min"),
    ("ClothCollisionSettings", "self_min_distance", "self_distance_min"),
    ("ClothCollisionSettings", "enable_collision", "use_collision"),
    ("ClothCollisionSettings", "enable_self_collision", "use_self_collision"),
    ("ClothSettings", "pin_cloth", "use_pin_cloth"),
    ("ClothSettings", "stiffness_scaling", "use_stiffness_scale"),
    ("CollisionSettings", "random_damping", "damping_random"),
    ("CollisionSettings", "random_friction", "friction_random"),
    ("CollisionSettings", "inner_thickness", "thickness_inner"),
    ("CollisionSettings", "outer_thickness", "thickness_outer"),
    ("CollisionSettings", "kill_particles", "use_particle_kill"),
    ("Constraint", "proxy_local", "is_proxy_local"),
    ("ActionConstraint", "maximum", "max"),
    ("ActionConstraint", "minimum", "min"),
    ("FollowPathConstraint", "use_fixed_position", "use_fixed_location"),
    ("KinematicConstraint", "chain_length", "chain_count"),
    ("KinematicConstraint", "pos_lock_x", "lock_location_x"),
    ("KinematicConstraint", "pos_lock_y", "lock_location_y"),
    ("KinematicConstraint", "pos_lock_z", "lock_location_z"),
    ("KinematicConstraint", "rot_lock_x", "lock_rotation_x"),
    ("KinematicConstraint", "rot_lock_y", "lock_rotation_y"),
    ("KinematicConstraint", "rot_lock_z", "lock_rotation_z"),
    ("KinematicConstraint", "axis_reference", "reference_axis"),
    ("KinematicConstraint", "use_position", "use_location"),
    ("LimitLocationConstraint", "maximum_x", "max_x"),
    ("LimitLocationConstraint", "maximum_y", "max_y"),
    ("LimitLocationConstraint", "maximum_z", "max_z"),
    ("LimitLocationConstraint", "minimum_x", "min_x"),
    ("LimitLocationConstraint", "minimum_y", "min_y"),
    ("LimitLocationConstraint", "minimum_z", "min_z"),
    ("LimitLocationConstraint", "use_maximum_x", "use_max_x"),
    ("LimitLocationConstraint", "use_maximum_y", "use_max_y"),
    ("LimitLocationConstraint", "use_maximum_z", "use_max_z"),
    ("LimitLocationConstraint", "use_minimum_x", "use_min_x"),
    ("LimitLocationConstraint", "use_minimum_y", "use_min_y"),
    ("LimitLocationConstraint", "use_minimum_z", "use_min_z"),
    ("LimitLocationConstraint", "limit_transform", "use_transform_limit"),
    ("LimitRotationConstraint", "maximum_x", "max_x"),
    ("LimitRotationConstraint", "maximum_y", "max_y"),
    ("LimitRotationConstraint", "maximum_z", "max_z"),
    ("LimitRotationConstraint", "minimum_x", "min_x"),
    ("LimitRotationConstraint", "minimum_y", "min_y"),
    ("LimitRotationConstraint", "minimum_z", "min_z"),
    ("LimitRotationConstraint", "limit_transform", "use_transform_limit"),
    ("LimitScaleConstraint", "maximum_x", "max_x"),
    ("LimitScaleConstraint", "maximum_y", "max_y"),
    ("LimitScaleConstraint", "maximum_z", "max_z"),
    ("LimitScaleConstraint", "minimum_x", "min_x"),
    ("LimitScaleConstraint", "minimum_y", "min_y"),
    ("LimitScaleConstraint", "minimum_z", "min_z"),
    ("LimitScaleConstraint", "use_maximum_x", "use_max_x"),
    ("LimitScaleConstraint", "use_maximum_y", "use_max_y"),
    ("LimitScaleConstraint", "use_maximum_z", "use_max_z"),
    ("LimitScaleConstraint", "use_minimum_x", "use_min_x"),
    ("LimitScaleConstraint", "use_minimum_y", "use_min_y"),
    ("LimitScaleConstraint", "use_minimum_z", "use_min_z"),
    ("LimitScaleConstraint", "limit_transform", "use_transform_limit"),
    ("PivotConstraint", "enabled_rotation_range", "rotation_range"),
    ("PivotConstraint", "use_relative_position", "use_relative_location"),
    ("PythonConstraint", "number_of_targets", "target_count"),
    ("SplineIKConstraint", "chain_length", "chain_count"),
    ("SplineIKConstraint", "chain_offset", "use_chain_offset"),
    ("SplineIKConstraint", "even_divisions", "use_even_divisions"),
    ("SplineIKConstraint", "y_stretch", "use_y_stretch"),
    ("SplineIKConstraint", "xz_scaling_mode", "xz_scale_mode"),
    ("StretchToConstraint", "original_length", "rest_length"),
    ("TrackToConstraint", "target_z", "use_target_z"),
    ("TransformConstraint", "extrapolate_motion", "use_motion_extrapolate"),
    ("FieldSettings", "do_location", "apply_to_location"),
    ("FieldSettings", "do_rotation", "apply_to_rotation"),
    ("FieldSettings", "maximum_distance", "distance_max"),
    ("FieldSettings", "minimum_distance", "distance_min"),
    ("FieldSettings", "radial_maximum", "radial_max"),
    ("FieldSettings", "radial_minimum", "radial_min"),
    ("FieldSettings", "force_2d", "use_2d_force"),
    ("FieldSettings", "do_absorption", "use_absorption"),
    ("FieldSettings", "global_coordinates", "use_global_coords"),
    ("FieldSettings", "guide_path_add", "use_guide_path_add"),
    ("FieldSettings", "multiple_springs", "use_multiple_springs"),
    ("FieldSettings", "use_coordinates", "use_object_coords"),
    ("FieldSettings", "root_coordinates", "use_root_coords"),
    ("ControlFluidSettings", "reverse_frames", "use_reverse_frames"),
    ("DomainFluidSettings", "real_world_size", "simulation_scale"),
    ("DomainFluidSettings", "surface_smoothing", "surface_smooth"),
    ("DomainFluidSettings", "reverse_frames", "use_reverse_frames"),
    ("DomainFluidSettings", "generate_speed_vectors", "use_speed_vectors"),
    ("DomainFluidSettings", "override_time", "use_time_override"),
    ("FluidFluidSettings", "export_animated_mesh", "use_animated_mesh"),
    ("InflowFluidSettings", "export_animated_mesh", "use_animated_mesh"),
    ("InflowFluidSettings", "local_coordinates", "use_local_coords"),
    ("ObstacleFluidSettings", "export_animated_mesh", "use_animated_mesh"),
    ("OutflowFluidSettings", "export_animated_mesh", "use_animated_mesh"),
    ("ParticleFluidSettings", "drops", "use_drops"),
    ("ParticleFluidSettings", "floats", "use_floats"),
    ("Armature", "drawtype", "draw_type"),
    ("Armature", "layer_protection", "layers_protected"),
    ("Armature", "auto_ik", "use_auto_ik"),
    ("Armature", "delay_deform", "use_deform_delay"),
    ("Armature", "deform_envelope", "use_deform_envelopes"),
    ("Armature", "deform_quaternion", "use_deform_preserve_volume"),
    ("Armature", "deform_vertexgroups", "use_deform_vertex_groups"),
    ("Armature", "x_axis_mirror", "use_mirror_x"),
    ("Curve", "width", "offset"),
    ("Image", "animation_speed", "fps"),
    ("Image", "animation_end", "frame_end"),
    ("Image", "animation_start", "frame_start"),
    ("Image", "animated", "use_animation"),
    ("Image", "clamp_x", "use_clamp_x"),
    ("Image", "clamp_y", "use_clamp_y"),
    ("Image", "premultiply", "use_premultiply"),
    ("AreaLamp", "shadow_ray_sampling_method", "shadow_ray_sample_method"),
    ("AreaLamp", "only_shadow", "use_only_shadow"),
    ("AreaLamp", "shadow_layer", "use_shadow_layer"),
    ("AreaLamp", "umbra", "use_umbra"),
    ("PointLamp", "shadow_ray_sampling_method", "shadow_ray_sample_method"),
    ("PointLamp", "only_shadow", "use_only_shadow"),
    ("PointLamp", "shadow_layer", "use_shadow_layer"),
    ("PointLamp", "sphere", "use_sphere"),
    ("SpotLamp", "shadow_ray_sampling_method", "shadow_ray_sample_method"),
    ("SpotLamp", "auto_clip_end", "use_auto_clip_end"),
    ("SpotLamp", "auto_clip_start", "use_auto_clip_start"),
    ("SpotLamp", "only_shadow", "use_only_shadow"),
    ("SpotLamp", "shadow_layer", "use_shadow_layer"),
    ("SpotLamp", "sphere", "use_sphere"),
    ("SunLamp", "only_shadow", "use_only_shadow"),
    ("SunLamp", "shadow_layer", "use_shadow_layer"),
    ("Material", "z_offset", "offset_z"),
    ("Material", "shadow_casting_alpha", "shadow_cast_alpha"),
    ("Material", "cast_approximate", "use_cast_approximate"),
    ("Material", "cast_buffer_shadows", "use_cast_buffer_shadows"),
    ("Material", "cast_shadows_only", "use_cast_shadows_only"),
    ("Material", "face_texture", "use_face_texture"),
    ("Material", "face_texture_alpha", "use_face_texture_alpha"),
    ("Material", "full_oversampling", "use_full_oversampling"),
    ("Material", "light_group_exclusive", "use_light_group_exclusive"),
    ("Material", "object_color", "use_object_color"),
    ("Material", "only_shadow", "use_only_shadow"),
    ("Material", "ray_shadow_bias", "use_ray_shadow_bias"),
    ("Material", "traceable", "use_raytrace"),
    ("Material", "shadeless", "use_shadeless"),
    ("Material", "tangent_shading", "use_tangent_shading"),
    ("Material", "transparency", "use_transparency"),
    ("Material", "receive_transparent_shadows", "use_transparent_shadows"),
    ("Material", "vertex_color_light", "use_vertex_color_light"),
    ("Material", "vertex_color_paint", "use_vertex_color_paint"),
    ("Mesh", "autosmooth_angle", "auto_smooth_angle"),
    ("Mesh", "autosmooth", "use_auto_smooth"),
    ("Object", "max_draw_type", "draw_type"),
    ("Object", "use_dupli_verts_rotation", "use_dupli_vertices_rotation"),
    ("Object", "shape_key_edit_mode", "use_shape_key_edit_mode"),
    ("Object", "slow_parent", "use_slow_parent"),
    ("Object", "time_offset_add_parent", "use_time_offset_add_parent"),
    ("Object", "time_offset_edit", "use_time_offset_edit"),
    ("Object", "time_offset_parent", "use_time_offset_parent"),
    ("Object", "time_offset_particle", "use_time_offset_particle"),
    ("ParticleSettings", "adaptive_pix", "adaptive_pixel"),
    ("ParticleSettings", "child_effector", "apply_effector_to_children"),
    ("ParticleSettings", "child_guide", "apply_guide_to_children"),
    ("ParticleSettings", "billboard_split_offset", "billboard_offset_split"),
    ("ParticleSettings", "billboard_random_tilt", "billboard_tilt_random"),
    ("ParticleSettings", "child_length_thres", "child_length_threshold"),
    ("ParticleSettings", "child_random_size", "child_size_random"),
    ("ParticleSettings", "clumppow", "clump_shape"),
    ("ParticleSettings", "damp_factor", "damping"),
    ("ParticleSettings", "draw_as", "draw_method"),
    ("ParticleSettings", "random_factor", "factor_random"),
    ("ParticleSettings", "grid_invert", "invert_grid"),
    ("ParticleSettings", "random_length", "length_random"),
    ("ParticleSettings", "random_lifetime", "lifetime_random"),
    ("ParticleSettings", "billboard_lock", "lock_billboard"),
    ("ParticleSettings", "boids_2d", "lock_boids_to_surface"),
    ("ParticleSettings", "object_aligned_factor", "object_align_factor"),
    ("ParticleSettings", "random_phase_factor", "phase_factor_random"),
    ("ParticleSettings", "ren_as", "render_type"),
    ("ParticleSettings", "rendered_child_nbr", "rendered_child_count"),
    ("ParticleSettings", "random_rotation_factor", "rotation_factor_random"),
    ("ParticleSettings", "rough1", "roughness_1"),
    ("ParticleSettings", "rough1_size", "roughness_1_size"),
    ("ParticleSettings", "rough2", "roughness_2"),
    ("ParticleSettings", "rough2_size", "roughness_2_size"),
    ("ParticleSettings", "rough2_thres", "roughness_2_threshold"),
    ("ParticleSettings", "rough_end_shape", "roughness_end_shape"),
    ("ParticleSettings", "rough_endpoint", "roughness_endpoint"),
    ("ParticleSettings", "random_size", "size_random"),
    ("ParticleSettings", "abs_path_time", "use_absolute_path_time"),
    ("ParticleSettings", "animate_branching", "use_animate_branching"),
    ("ParticleSettings", "branching", "use_branching"),
    ("ParticleSettings", "died", "use_dead"),
    ("ParticleSettings", "die_on_collision", "use_die_on_collision"),
    ("ParticleSettings", "rotation_dynamic", "use_dynamic_rotation"),
    ("ParticleSettings", "even_distribution", "use_even_distribution"),
    ("ParticleSettings", "rand_group", "use_group_pick_random"),
    ("ParticleSettings", "hair_bspline", "use_hair_bspline"),
    ("ParticleSettings", "sizemass", "use_multiply_size_mass"),
    ("ParticleSettings", "react_multiple", "use_react_multiple"),
    ("ParticleSettings", "react_start_end", "use_react_start_end"),
    ("ParticleSettings", "render_adaptive", "use_render_adaptive"),
    ("ParticleSettings", "self_effect", "use_self_effect"),
    ("ParticleSettings", "enable_simplify", "use_simplify"),
    ("ParticleSettings", "size_deflect", "use_size_deflect"),
    ("ParticleSettings", "render_strand", "use_strand_primitive"),
    ("ParticleSettings", "symmetric_branching", "use_symmetric_branching"),
    ("ParticleSettings", "velocity_length", "use_velocity_length"),
    ("ParticleSettings", "whole_group", "use_whole_group"),
    ("CloudsTexture", "noise_size", "noise_scale"),
    ("DistortedNoiseTexture", "noise_size", "noise_scale"),
    ("EnvironmentMapTexture", "filter_size_minimum", "use_filter_size_min"),
    ("EnvironmentMapTexture", "mipmap_gauss", "use_mipmap_gauss"),
    ("ImageTexture", "calculate_alpha", "use_calculate_alpha"),
    ("ImageTexture", "checker_even", "use_checker_even"),
    ("ImageTexture", "checker_odd", "use_checker_odd"),
    ("ImageTexture", "filter_size_minimum", "use_filter_size_min"),
    ("ImageTexture", "flip_axis", "use_flip_axis"),
    ("ImageTexture", "mipmap_gauss", "use_mipmap_gauss"),
    ("ImageTexture", "mirror_x", "use_mirror_x"),
    ("ImageTexture", "mirror_y", "use_mirror_y"),
    ("ImageTexture", "normal_map", "use_normal_map"),
    ("MarbleTexture", "noise_size", "noise_scale"),
    ("MarbleTexture", "noisebasis2", "noise_basis_2"),
    ("MarbleTexture", "noisebasis_2", "noise_basis_2"),
    ("MusgraveTexture", "highest_dimension", "dimension_max"),
    ("MusgraveTexture", "noise_size", "noise_scale"),
    ("StucciTexture", "noise_size", "noise_scale"),
    ("VoronoiTexture", "coloring", "color_mode"),
    ("VoronoiTexture", "noise_size", "noise_scale"),
    ("WoodTexture", "noise_size", "noise_scale"),
    ("WoodTexture", "noisebasis2", "noise_basis_2"),
    ("WoodTexture", "noisebasis_2", "noise_basis_2"),
    ("World", "blend_sky", "use_sky_blend"),
    ("World", "paper_sky", "use_sky_paper"),
    ("World", "real_sky", "use_sky_real"),
    ("ImageUser", "auto_refresh", "use_auto_refresh"),
    ("MaterialHalo", "flares_sub", "flare_subflare_count"),
    ("MaterialHalo", "flare_subsize", "flare_subflare_size"),
    ("MaterialHalo", "line_number", "line_count"),
    ("MaterialHalo", "rings", "ring_count"),
    ("MaterialHalo", "star_tips", "star_tip_count"),
    ("MaterialHalo", "xalpha", "use_extreme_alpha"),
    ("MaterialHalo", "flare_mode", "use_flare_mode"),
    ("MaterialHalo", "vertex_normal", "use_vertex_normal"),
    ("MaterialPhysics", "align_to_normal", "use_normal_align"),
    ("MaterialStrand", "min_size", "size_min"),
    ("MaterialStrand", "blender_units", "use_blender_units"),
    ("MaterialStrand", "surface_diffuse", "use_surface_diffuse"),
    ("MaterialStrand", "tangent_shading", "use_tangent_shading"),
    ("MaterialSubsurfaceScattering", "error_tolerance", "error_threshold"),
    ("MaterialVolume", "depth_cutoff", "depth_threshold"),
    ("MaterialVolume", "lighting_mode", "light_method"),
    ("MaterialVolume", "step_calculation", "step_method"),
    ("MaterialVolume", "external_shadows", "use_external_shadows"),
    ("MaterialVolume", "light_cache", "use_light_cache"),
    ("ArmatureModifier", "multi_modifier", "use_multi_modifier"),
    ("ArrayModifier", "constant_offset_displacement", "constant_offset_displace"),
    ("ArrayModifier", "merge_distance", "merge_threshold"),
    ("ArrayModifier", "relative_offset_displacement", "relative_offset_displace"),
    ("ArrayModifier", "constant_offset", "use_constant_offset"),
    ("ArrayModifier", "merge_adjacent_vertices", "use_merge_vertices"),
    ("ArrayModifier", "merge_end_vertices", "use_merge_vertices_cap"),
    ("ArrayModifier", "add_offset_object", "use_object_offset"),
    ("ArrayModifier", "relative_offset", "use_relative_offset"),
    ("BevelModifier", "only_vertices", "use_only_vertices"),
    ("CastModifier", "from_radius", "use_radius_as_size"),
    ("DisplaceModifier", "midlevel", "mid_level"),
    ("DisplaceModifier", "texture_coordinates", "texture_coords"),
    ("EdgeSplitModifier", "use_sharp", "use_edge_sharp"),
    ("ExplodeModifier", "split_edges", "use_edge_split"),
    ("MirrorModifier", "merge_limit", "merge_threshold"),
    ("MirrorModifier", "mirror_u", "use_mirror_u"),
    ("MirrorModifier", "mirror_v", "use_mirror_v"),
    ("MirrorModifier", "mirror_vertex_groups", "use_mirror_vertex_groups"),
    ("ParticleInstanceModifier", "particle_system_number", "particle_system_index"),
    ("ParticleInstanceModifier", "keep_shape", "use_preserve_shape"),
    ("ShrinkwrapModifier", "cull_back_faces", "use_cull_back_faces"),
    ("ShrinkwrapModifier", "cull_front_faces", "use_cull_front_faces"),
    ("ShrinkwrapModifier", "keep_above_surface", "use_keep_above_surface"),
    ("SimpleDeformModifier", "lock_x_axis", "lock_x"),
    ("SimpleDeformModifier", "lock_y_axis", "lock_y"),
    ("SmokeModifier", "smoke_type", "type"),
    ("SubsurfModifier", "subsurf_uv", "use_subsurf_uv"),
    ("UVProjectModifier", "num_projectors", "projector_count"),
    ("UVProjectModifier", "override_image", "use_image_override"),
    ("WaveModifier", "texture_coordinates", "texture_coords"),
    ("WaveModifier", "x_normal", "use_normal_x"),
    ("WaveModifier", "y_normal", "use_normal_y"),
    ("WaveModifier", "z_normal", "use_normal_z"),
    ("NlaStrip", "blending", "blend_type"),
    ("NlaStrip", "animated_influence", "use_animated_influence"),
    ("NlaStrip", "animated_time", "use_animated_time"),
    ("NlaStrip", "animated_time_cyclic", "use_animated_time_cyclic"),
    ("NlaStrip", "auto_blending", "use_auto_blend"),
    ("CompositorNodeAlphaOver", "convert_premul", "use_premultiply"),
    ("CompositorNodeBlur", "sizex", "size_x"),
    ("CompositorNodeBlur", "sizey", "size_y"),
    ("CompositorNodeChannelMatte", "algorithm", "limit_method"),
    ("CompositorNodeChromaMatte", "acceptance", "tolerance"),
    ("CompositorNodeColorBalance", "correction_formula", "correction_method"),
    ("CompositorNodeColorSpill", "algorithm", "limit_method"),
    ("CompositorNodeColorSpill", "unspill", "use_unspill"),
    ("CompositorNodeCrop", "x2", "max_x"),
    ("CompositorNodeCrop", "y2", "max_y"),
    ("CompositorNodeCrop", "x1", "min_x"),
    ("CompositorNodeCrop", "y1", "min_y"),
    ("CompositorNodeCrop", "crop_size", "use_crop_size"),
    ("CompositorNodeDefocus", "max_blur", "blur_max"),
    ("CompositorNodeDefocus", "gamma_correction", "use_gamma_correction"),
    ("CompositorNodeGlare", "rotate_45", "use_rotate_45"),
    ("CompositorNodeImage", "auto_refresh", "use_auto_refresh"),
    ("CompositorNodeLensdist", "projector", "use_projector"),
    ("CompositorNodeVecBlur", "max_speed", "speed_max"),
    ("CompositorNodeVecBlur", "min_speed", "speed_min"),
    ("ShaderNodeMapping", "maximum", "max"),
    ("ShaderNodeMapping", "minimum", "min"),
    ("ShaderNodeMapping", "clamp_maximum", "use_max"),
    ("ShaderNodeMapping", "clamp_minimum", "use_min"),
    ("ParticleEdit", "add_keys", "default_key_count"),
    ("ParticleEdit", "selection_mode", "select_mode"),
    ("ParticleEdit", "auto_velocity", "use_auto_velocity"),
    ("ParticleEdit", "add_interpolate", "use_default_interpolate"),
    ("ParticleEdit", "emitter_deflect", "use_emitter_deflect"),
    ("ParticleEdit", "fade_time", "use_fade_time"),
    ("ParticleEdit", "keep_lengths", "use_preserve_length"),
    ("ParticleEdit", "keep_root", "use_preserve_root"),
    ("ParticleSystem", "vertex_group_clump_negate", "invert_vertex_group_clump"),
    ("ParticleSystem", "vertex_group_density_negate", "invert_vertex_group_density"),
    ("ParticleSystem", "vertex_group_field_negate", "invert_vertex_group_field"),
    ("ParticleSystem", "vertex_group_kink_negate", "invert_vertex_group_kink"),
    ("ParticleSystem", "vertex_group_length_negate", "invert_vertex_group_length"),
    ("ParticleSystem", "vertex_group_rotation_negate", "invert_vertex_group_rotation"),
    ("ParticleSystem", "vertex_group_roughness1_negate", "invert_vertex_group_roughness_1"),
    ("ParticleSystem", "vertex_group_roughness2_negate", "invert_vertex_group_roughness_2"),
    ("ParticleSystem", "vertex_group_roughness_end_negate", "invert_vertex_group_roughness_end"),
    ("ParticleSystem", "vertex_group_size_negate", "invert_vertex_group_size"),
    ("ParticleSystem", "vertex_group_tangent_negate", "invert_vertex_group_tangent"),
    ("ParticleSystem", "vertex_group_velocity_negate", "invert_vertex_group_velocity"),
    ("ParticleSystem", "hair_dynamics", "use_hair_dynamics"),
    ("ParticleSystem", "keyed_timing", "use_keyed_timing"),
    ("PointDensity", "falloff_softness", "falloff_soft"),
    ("PointDensity", "particle_cache", "particle_cache_space"),
    ("PointDensity", "turbulence_size", "turbulence_scale"),
    ("PointDensity", "turbulence", "use_turbulence"),
    ("PointDensity", "vertices_cache", "vertex_cache_space"),
    ("PoseBone", "ik_lin_weight", "ik_linear_weight"),
    ("PoseBone", "ik_rot_weight", "ik_rotation_weight"),
    ("PoseBone", "ik_limit_x", "use_ik_limit_x"),
    ("PoseBone", "ik_limit_y", "use_ik_limit_y"),
    ("PoseBone", "ik_limit_z", "use_ik_limit_z"),
    ("PoseBone", "ik_lin_control", "use_ik_linear_control"),
    ("PoseBone", "ik_rot_control", "use_ik_rotation_control"),
    ("Bone", "use_hinge", "use_inherit_rotation"),
    ("SPHFluidSettings", "spring_k", "spring_force"),
    ("SPHFluidSettings", "stiffness_k", "stiffness"),
    ("SPHFluidSettings", "stiffness_knear", "stiffness_near"),
    ("SceneGameData", "framing_color", "frame_color"),
    ("SceneGameData", "framing_type", "frame_type"),
    ("SceneGameData", "eye_separation", "stereo_eye_separation"),
    ("SceneGameData", "activity_culling", "use_activity_culling"),
    ("SceneGameData", "auto_start", "use_auto_start"),
    ("SceneGameData", "glsl_extra_textures", "use_glsl_extra_textures"),
    ("SceneGameData", "glsl_lights", "use_glsl_lights"),
    ("SceneGameData", "glsl_nodes", "use_glsl_nodes"),
    ("SceneGameData", "glsl_ramps", "use_glsl_ramps"),
    ("SceneGameData", "glsl_shaders", "use_glsl_shaders"),
    ("SceneGameData", "glsl_shadows", "use_glsl_shadows"),
    ("Sequence", "blend_opacity", "blend_alpha"),
    ("Sequence", "blend_mode", "blend_type"),
    ("Sequence", "frame_final_length", "frame_final_duration"),
    ("Sequence", "use_effect_default_fade", "use_default_fade"),
    ("SequenceColorBalance", "inverse_gain", "invert_gain"),
    ("SequenceColorBalance", "inverse_gamma", "invert_gamma"),
    ("SequenceColorBalance", "inverse_lift", "invert_lift"),
    ("EffectSequence", "multiply_colors", "color_multiply"),
    ("EffectSequence", "de_interlace", "use_deinterlace"),
    ("EffectSequence", "flip_x", "use_flip_x"),
    ("EffectSequence", "flip_y", "use_flip_y"),
    ("EffectSequence", "convert_float", "use_float"),
    ("EffectSequence", "premultiply", "use_premultiply"),
    ("EffectSequence", "proxy_custom_directory", "use_proxy_custom_directory"),
    ("EffectSequence", "proxy_custom_file", "use_proxy_custom_file"),
    ("EffectSequence", "reverse_frames", "use_reverse_frames"),
    ("GlowSequence", "blur_distance", "blur_radius"),
    ("GlowSequence", "only_boost", "use_only_boost"),
    ("SpeedControlSequence", "curve_compress_y", "use_curve_compress_y"),
    ("SpeedControlSequence", "curve_velocity", "use_curve_velocity"),
    ("SpeedControlSequence", "frame_blending", "use_frame_blend"),
    ("TransformSequence", "uniform_scale", "use_uniform_scale"),
    ("ImageSequence", "animation_end_offset", "animation_offset_end"),
    ("ImageSequence", "animation_start_offset", "animation_offset_start"),
    ("ImageSequence", "multiply_colors", "color_multiply"),
    ("ImageSequence", "de_interlace", "use_deinterlace"),
    ("ImageSequence", "flip_x", "use_flip_x"),
    ("ImageSequence", "flip_y", "use_flip_y"),
    ("ImageSequence", "convert_float", "use_float"),
    ("ImageSequence", "premultiply", "use_premultiply"),
    ("ImageSequence", "proxy_custom_directory", "use_proxy_custom_directory"),
    ("ImageSequence", "proxy_custom_file", "use_proxy_custom_file"),
    ("ImageSequence", "reverse_frames", "use_reverse_frames"),
    ("MetaSequence", "animation_end_offset", "animation_offset_end"),
    ("MetaSequence", "animation_start_offset", "animation_offset_start"),
    ("MetaSequence", "multiply_colors", "color_multiply"),
    ("MetaSequence", "de_interlace", "use_deinterlace"),
    ("MetaSequence", "flip_x", "use_flip_x"),
    ("MetaSequence", "flip_y", "use_flip_y"),
    ("MetaSequence", "convert_float", "use_float"),
    ("MetaSequence", "premultiply", "use_premultiply"),
    ("MetaSequence", "proxy_custom_directory", "use_proxy_custom_directory"),
    ("MetaSequence", "proxy_custom_file", "use_proxy_custom_file"),
    ("MetaSequence", "reverse_frames", "use_reverse_frames"),
    ("MovieSequence", "animation_end_offset", "animation_offset_end"),
    ("MovieSequence", "animation_start_offset", "animation_offset_start"),
    ("MovieSequence", "multiply_colors", "color_multiply"),
    ("MovieSequence", "de_interlace", "use_deinterlace"),
    ("MovieSequence", "flip_x", "use_flip_x"),
    ("MovieSequence", "flip_y", "use_flip_y"),
    ("MovieSequence", "convert_float", "use_float"),
    ("MovieSequence", "premultiply", "use_premultiply"),
    ("MovieSequence", "proxy_custom_directory", "use_proxy_custom_directory"),
    ("MovieSequence", "proxy_custom_file", "use_proxy_custom_file"),
    ("MovieSequence", "reverse_frames", "use_reverse_frames"),
    ("MulticamSequence", "animation_end_offset", "animation_offset_end"),
    ("MulticamSequence", "animation_start_offset", "animation_offset_start"),
    ("MulticamSequence", "multiply_colors", "color_multiply"),
    ("MulticamSequence", "de_interlace", "use_deinterlace"),
    ("MulticamSequence", "flip_x", "use_flip_x"),
    ("MulticamSequence", "flip_y", "use_flip_y"),
    ("MulticamSequence", "convert_float", "use_float"),
    ("MulticamSequence", "premultiply", "use_premultiply"),
    ("MulticamSequence", "proxy_custom_directory", "use_proxy_custom_directory"),
    ("MulticamSequence", "proxy_custom_file", "use_proxy_custom_file"),
    ("MulticamSequence", "reverse_frames", "use_reverse_frames"),
    ("SceneSequence", "animation_end_offset", "animation_offset_end"),
    ("SceneSequence", "animation_start_offset", "animation_offset_start"),
    ("SceneSequence", "multiply_colors", "color_multiply"),
    ("SceneSequence", "de_interlace", "use_deinterlace"),
    ("SceneSequence", "flip_x", "use_flip_x"),
    ("SceneSequence", "flip_y", "use_flip_y"),
    ("SceneSequence", "convert_float", "use_float"),
    ("SceneSequence", "premultiply", "use_premultiply"),
    ("SceneSequence", "proxy_custom_directory", "use_proxy_custom_directory"),
    ("SceneSequence", "proxy_custom_file", "use_proxy_custom_file"),
    ("SceneSequence", "reverse_frames", "use_reverse_frames"),
    ("SoundSequence", "animation_end_offset", "animation_offset_end"),
    ("SoundSequence", "animation_start_offset", "animation_offset_start"),
    ("SmokeDomainSettings", "smoke_domain_colli", "collision_extents"),
    ("SmokeDomainSettings", "smoke_cache_high_comp", "point_cache_compress_high_type"),
    ("SmokeDomainSettings", "smoke_cache_comp", "point_cache_compress_type"),
    ("SmokeDomainSettings", "maxres", "resolution_max"),
    ("SmokeDomainSettings", "smoothemitter", "smooth_emitter"),
    ("SmokeDomainSettings", "dissolve_smoke", "use_dissolve_smoke"),
    ("SmokeDomainSettings", "dissolve_smoke_log", "use_dissolve_smoke_log"),
    ("SmokeDomainSettings", "highres", "use_high_resolution"),
    ("SoftBodySettings", "bending", "bend"),
    ("SoftBodySettings", "error_limit", "error_threshold"),
    ("SoftBodySettings", "lcom", "location_mass_center"),
    ("SoftBodySettings", "lrot", "rotation_estimate"),
    ("SoftBodySettings", "lscale", "scale_estimate"),
    ("SoftBodySettings", "maxstep", "step_max"),
    ("SoftBodySettings", "minstep", "step_min"),
    ("SoftBodySettings", "diagnose", "use_diagnose"),
    ("SoftBodySettings", "edge_collision", "use_edge_collision"),
    ("SoftBodySettings", "estimate_matrix", "use_estimate_matrix"),
    ("SoftBodySettings", "face_collision", "use_face_collision"),
    ("SoftBodySettings", "self_collision", "use_self_collision"),
    ("SoftBodySettings", "stiff_quads", "use_stiff_quads"),
    ("TexMapping", "maximum", "max"),
    ("TexMapping", "minimum", "min"),
    ("TexMapping", "has_maximum", "use_max"),
    ("TexMapping", "has_minimum", "use_min"),
    ("TextCharacterFormat", "bold", "use_bold"),
    ("TextCharacterFormat", "italic", "use_italic"),
    ("TextCharacterFormat", "underline", "use_underline"),
    ("TextureSlot", "rgb_to_intensity", "use_rgb_to_intensity"),
    ("TextureSlot", "stencil", "use_stencil"),
    ("LampTextureSlot", "texture_coordinates", "texture_coords"),
    ("LampTextureSlot", "map_color", "use_map_color"),
    ("LampTextureSlot", "map_shadow", "use_map_shadow"),
    ("MaterialTextureSlot", "coloremission_factor", "color_emission_factor"),
    ("MaterialTextureSlot", "colordiff_factor", "diffuse_color_factor"),
    ("MaterialTextureSlot", "x_mapping", "mapping_x"),
    ("MaterialTextureSlot", "y_mapping", "mapping_y"),
    ("MaterialTextureSlot", "z_mapping", "mapping_z"),
    ("MaterialTextureSlot", "colorreflection_factor", "reflection_color_factor"),
    ("MaterialTextureSlot", "colorspec_factor", "specular_color_factor"),
    ("MaterialTextureSlot", "texture_coordinates", "texture_coords"),
    ("MaterialTextureSlot", "colortransmission_factor", "transmission_color_factor"),
    ("MaterialTextureSlot", "from_dupli", "use_from_dupli"),
    ("MaterialTextureSlot", "from_original", "use_from_original"),
    ("MaterialTextureSlot", "map_alpha", "use_map_alpha"),
    ("MaterialTextureSlot", "map_ambient", "use_map_ambient"),
    ("MaterialTextureSlot", "map_colordiff", "use_map_color_diff"),
    ("MaterialTextureSlot", "map_coloremission", "use_map_color_emission"),
    ("MaterialTextureSlot", "map_colorreflection", "use_map_color_reflection"),
    ("MaterialTextureSlot", "map_colorspec", "use_map_color_spec"),
    ("MaterialTextureSlot", "map_colortransmission", "use_map_color_transmission"),
    ("MaterialTextureSlot", "map_density", "use_map_density"),
    ("MaterialTextureSlot", "map_diffuse", "use_map_diffuse"),
    ("MaterialTextureSlot", "map_displacement", "use_map_displacement"),
    ("MaterialTextureSlot", "map_emission", "use_map_emission"),
    ("MaterialTextureSlot", "map_emit", "use_map_emit"),
    ("MaterialTextureSlot", "map_hardness", "use_map_hardness"),
    ("MaterialTextureSlot", "map_mirror", "use_map_mirror"),
    ("MaterialTextureSlot", "map_normal", "use_map_normal"),
    ("MaterialTextureSlot", "map_raymir", "use_map_raymir"),
    ("MaterialTextureSlot", "map_reflection", "use_map_reflect"),
    ("MaterialTextureSlot", "map_scattering", "use_map_scatter"),
    ("MaterialTextureSlot", "map_specular", "use_map_specular"),
    ("MaterialTextureSlot", "map_translucency", "use_map_translucency"),
    ("MaterialTextureSlot", "map_warp", "use_map_warp"),
    ("WorldTextureSlot", "texture_coordinates", "texture_coords"),
    ("WorldTextureSlot", "map_blend", "use_map_blend"),
    ("WorldTextureSlot", "map_horizon", "use_map_horizon"),
    ("WorldTextureSlot", "map_zenith_down", "use_map_zenith_down"),
    ("WorldTextureSlot", "map_zenith_up", "use_map_zenith_up"),
    ("VoxelData", "still_frame_number", "still_frame"),
    ("WorldLighting", "ao_blend_mode", "ao_blend_type"),
    ("WorldLighting", "error_tolerance", "error_threshold"),
    ("WorldLighting", "use_ambient_occlusion", "use_ambient_occlusian"),
    ("WorldLighting", "pixel_cache", "use_cache"),
    ("WorldLighting", "use_environment_lighting", "use_environment_light"),
    ("WorldLighting", "use_indirect_lighting", "use_indirect_light"),
    ("WorldStarsSettings", "color_randomization", "color_random"),
    ("WorldStarsSettings", "min_distance", "distance_min"),
    ("WorldLighting", "falloff", "use_falloff"),
    ("Constraint", "disabled", "is_valid"),
    ("ClampToConstraint", "cyclic", "use_cyclic"),
    ("ImageTexture", "filter", "filter_type"),
    ("ImageTexture", "interpolation", "use_interpolation"),
    ("ImageTexture", "mipmap", "use_mipmap"),
    ("ImageUser", "frames", "frame_duration"),
    ("ImageUser", "offset", "frame_offset"),
    ("ImageUser", "cyclic", "use_cyclic"),
    ("ArmatureModifier", "invert", "invert_vertex_group"),
    ("ArmatureModifier", "quaternion", "use_deform_preserve_volume"),
    ("ArrayModifier", "length", "fit_length"),
    ("BevelModifier", "angle", "angle_limit"),
    ("BuildModifier", "length", "frame_duration"),
    ("BuildModifier", "randomize", "use_random_order"),
    ("CastModifier", "x", "use_x"),
    ("CastModifier", "y", "use_y"),
    ("CastModifier", "z", "use_z"),
    ("ExplodeModifier", "size", "use_size"),
    ("MaskModifier", "invert", "invert_vertex_group"),
    ("MeshDeformModifier", "invert", "invert_vertex_group"),
    ("MeshDeformModifier", "dynamic", "use_dynamic_bind"),
    ("MirrorModifier", "clip", "use_clip"),
    ("MirrorModifier", "x", "use_x"),
    ("MirrorModifier", "y", "use_y"),
    ("MirrorModifier", "z", "use_z"),
    ("ParticleInstanceModifier", "children", "use_children"),
    ("ParticleInstanceModifier", "normal", "use_normal"),
    ("ParticleInstanceModifier", "size", "use_size"),
    ("ShrinkwrapModifier", "negative", "use_negative_direction"),
    ("ShrinkwrapModifier", "positive", "use_positive_direction"),
    ("ShrinkwrapModifier", "x", "use_project_x"),
    ("ShrinkwrapModifier", "y", "use_project_y"),
    ("ShrinkwrapModifier", "z", "use_project_z"),
    ("ShrinkwrapModifier", "mode", "wrap_method"),
    ("SimpleDeformModifier", "mode", "deform_method"),
    ("SimpleDeformModifier", "relative", "use_relative"),
    ("SmoothModifier", "repeat", "iterations"),
    ("SmoothModifier", "x", "use_x"),
    ("SmoothModifier", "y", "use_y"),
    ("SmoothModifier", "z", "use_z"),
    ("SolidifyModifier", "invert", "invert_vertex_group"),
    ("WaveModifier", "cyclic", "use_cyclic"),
    ("WaveModifier", "normals", "use_normal"),
    ("WaveModifier", "x", "use_x"),
    ("WaveModifier", "y", "use_y"),
    ("DampedTrackConstraint", "track", "track_axis"),
    ("FloorConstraint", "sticky", "use_sticky"),
    ("FollowPathConstraint", "forward", "forward_axis"),
    ("FollowPathConstraint", "up", "up_axis"),
    ("LockedTrackConstraint", "lock", "lock_axis"),
    ("LockedTrackConstraint", "track", "track_axis"),
    ("MaintainVolumeConstraint", "axis", "free_axis"),
    ("TrackToConstraint", "track", "track_axis"),
    ("TrackToConstraint", "up", "up_axis"),
    ("GameProperty", "debug", "show_debug"),
    ("Image", "tiles", "use_tiles"),
    ("Lamp", "diffuse", "use_diffuse"),
    ("Lamp", "negative", "use_negative"),
    ("Lamp", "layer", "use_own_layer"),
    ("Lamp", "specular", "use_specular"),
    ("AreaLamp", "dither", "use_dither"),
    ("AreaLamp", "jitter", "use_jitter"),
    ("SpotLamp", "square", "use_square"),
    ("Material", "cubic", "use_cubic"),
    ("Material", "shadows", "use_shadows"),
    ("ParticleSettings", "amount", "count"),
    ("ParticleSettings", "display", "draw_percentage"),
    ("ParticleSettings", "velocity", "show_velocity"),
    ("ParticleSettings", "trand", "use_emit_random"),
    ("ParticleSettings", "parent", "use_parent_particles"),
    ("ParticleSettings", "emitter", "use_render_emitter"),
    ("ParticleSettings", "viewport", "use_simplify_viewport"),
    ("Texture", "brightness", "intensity"),
    ("CloudsTexture", "stype", "cloud_type"),
    ("EnvironmentMapTexture", "filter", "filter_type"),
    ("EnvironmentMapTexture", "mipmap", "use_mipmap"),
    ("MarbleTexture", "stype", "marble_type"),
    ("StucciTexture", "stype", "stucci_type"),
    ("WoodTexture", "stype", "wood_type"),
    ("World", "range", "color_range"),
    ("World", "lighting", "light_settings"),
    ("World", "mist", "mist_settings"),
    ("World", "stars", "star_settings"),
    ("MaterialHalo", "lines", "use_lines"),
    ("MaterialHalo", "ring", "use_ring"),
    ("MaterialHalo", "soft", "use_soft"),
    ("MaterialHalo", "star", "use_star"),
    ("MaterialHalo", "texture", "use_texture"),
    ("MaterialPhysics", "damp", "damping"),
    ("MaterialRaytraceTransparency", "limit", "depth_max"),
    ("NlaStrip", "reversed", "use_reverse"),
    ("CompositorNodeBlur", "bokeh", "use_bokeh"),
    ("CompositorNodeBlur", "gamma", "use_gamma_correction"),
    ("CompositorNodeBlur", "relative", "use_relative"),
    ("CompositorNodeChannelMatte", "high", "limit_max"),
    ("CompositorNodeChannelMatte", "low", "limit_min"),
    ("CompositorNodeChannelMatte", "channel", "matte_channel"),
    ("CompositorNodeChromaMatte", "cutoff", "threshold"),
    ("CompositorNodeColorMatte", "h", "color_hue"),
    ("CompositorNodeColorMatte", "s", "color_saturation"),
    ("CompositorNodeColorMatte", "v", "color_value"),
    ("CompositorNodeDBlur", "wrap", "use_wrap"),
    ("CompositorNodeDefocus", "preview", "use_preview"),
    ("CompositorNodeHueSat", "hue", "color_hue"),
    ("CompositorNodeHueSat", "sat", "color_saturation"),
    ("CompositorNodeHueSat", "val", "color_value"),
    ("CompositorNodeImage", "frames", "frame_duration"),
    ("CompositorNodeImage", "offset", "frame_offset"),
    ("CompositorNodeImage", "start", "frame_start"),
    ("CompositorNodeImage", "cyclic", "use_cyclic"),
    ("CompositorNodeInvert", "alpha", "invert_alpha"),
    ("CompositorNodeInvert", "rgb", "invert_rgb"),
    ("CompositorNodeLensdist", "fit", "use_fit"),
    ("CompositorNodeLensdist", "jitter", "use_jitter"),
    ("CompositorNodeMixRGB", "alpha", "use_alpha"),
    ("CompositorNodeRotate", "filter", "filter_type"),
    ("CompositorNodeTime", "end", "frame_end"),
    ("CompositorNodeTime", "start", "frame_start"),
    ("CompositorNodeVecBlur", "curved", "use_curved"),
    ("ShaderNodeExtendedMaterial", "diffuse", "use_diffuse"),
    ("ShaderNodeExtendedMaterial", "specular", "use_specular"),
    ("ShaderNodeMaterial", "diffuse", "use_diffuse"),
    ("ShaderNodeMaterial", "specular", "use_specular"),
    ("ShaderNodeMixRGB", "alpha", "use_alpha"),
    ("TextureNodeCurveTime", "end", "frame_end"),
    ("TextureNodeCurveTime", "start", "frame_start"),
    ("TextureNodeMixRGB", "alpha", "use_alpha"),
    ("TextureSlot", "negate", "invert"),
    ("TextureSlot", "size", "scale"),
    ("SoftBodySettings", "damp", "damping"),
    ("SequenceCrop", "right", "max_x"),
    ("SequenceCrop", "top", "max_y"),
    ("SequenceCrop", "bottom", "min_x"),
    ("SequenceCrop", "left", "min_y"),
    ("Sequence", "speed_fader", "speed_factor"),
    ("SpeedControlSequence", "global_speed", "multiply_speed"),
    ("SpeedControlSequence", "use_curve_velocity", "use_as_speed"),
    ("SpeedControlSequence", "use_curve_compress_y", "scale_to_length"),
    ("Key", "keys", "key_blocks"),
    )


if __name__ == "__main__":

    # Example, should be called externally
    # (class, from, to)
    replace_ls = [
        ("AnimVizMotionPaths", "frame_after", "frame_after"),
        ("AnimVizMotionPaths", "frame_before", "frame_before"),
        ("AnimVizOnionSkinning", "frame_after", "frame_after"),
    ]

    update_data_paths(replace_ls)
