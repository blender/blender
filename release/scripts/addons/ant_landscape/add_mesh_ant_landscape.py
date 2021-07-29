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

# Another Noise Tool - Add Landscape
# Jimmy Hazevoet

# import modules
import bpy
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        IntProperty,
        StringProperty,
        FloatVectorProperty,
        )

from .ant_functions import (
        grid_gen,
        sphere_gen,
        create_mesh_object,
        store_properties,
        draw_ant_refresh,
        draw_ant_main,
        draw_ant_noise,
        draw_ant_displace,
        draw_ant_water,
        )

# ------------------------------------------------------------
# Add landscape
class AntAddLandscape(bpy.types.Operator):
    bl_idname = "mesh.landscape_add"
    bl_label = "Another Noise Tool - Landscape"
    bl_description = "Add landscape mesh"
    bl_options = {'REGISTER', 'UNDO', 'PRESET'}

    ant_terrain_name = StringProperty(
            name="Name",
            default="Landscape"
            )
    land_material = StringProperty(
            name='Material',
            default="",
            description="Terrain material"
            )
    water_material = StringProperty(
            name='Material',
            default="",
            description="Water plane material"
            )
    texture_block = StringProperty(
            name="Texture",
            default=""
            )
    at_cursor = BoolProperty(
            name="Cursor",
            default=True,
            description="Place at cursor location",
            )
    smooth_mesh = BoolProperty(
            name="Smooth",
            default=True,
            description="Shade smooth"
            )
    tri_face = BoolProperty(
            name="Triangulate",
            default=False,
            description="Triangulate faces"
            )
    sphere_mesh = BoolProperty(
            name="Sphere",
            default=False,
            description="Generate uv sphere - remove doubles when ready"
            )
    subdivision_x = IntProperty(
            name="Subdivisions X",
            default=128,
            min=4,
            max=6400,
            description="Mesh X subdivisions"
            )
    subdivision_y = IntProperty(
            default=128,
            name="Subdivisions Y",
            min=4,
            max=6400,
            description="Mesh Y subdivisions"
            )
    mesh_size = FloatProperty(
            default=2.0,
            name="Mesh Size",
            min=0.01,
            max=100000.0,
            description="Mesh size"
            )
    mesh_size_x = FloatProperty(
            default=2.0,
            name="Mesh Size X",
            min=0.01,
            description="Mesh x size"
            )
    mesh_size_y = FloatProperty(
            name="Mesh Size Y",
            default=2.0,
            min=0.01,
            description="Mesh y size"
            )

    random_seed = IntProperty(
            name="Random Seed",
            default=0,
            min=0,
            description="Randomize noise origin"
            )
    noise_offset_x = FloatProperty(
            name="Offset X",
            default=0.0,
            description="Noise X Offset"
            )
    noise_offset_y = FloatProperty(
            name="Offset Y",
            default=0.0,
            description="Noise Y Offset"
            )
    noise_offset_z = FloatProperty(
            name="Offset Z",
            default=0.0,
            description="Noise Z Offset"
            )
    noise_size_x = FloatProperty(
            default=1.0,
            name="Size X",
            min=0.01,
            max=1000.0,
            description="Noise x size"
            )
    noise_size_y = FloatProperty(
            name="Size Y",
            default=1.0,
            min=0.01,
            max=1000.0,
            description="Noise y size"
            )
    noise_size_z = FloatProperty(
            name="Size Z",
            default=1.0,
            min=0.01,
            max=1000.0,
            description="Noise Z size"
            )
    noise_size = FloatProperty(
            name="Noise Size",
            default=1.0,
            min=0.01,
            max=1000.0,
            description="Noise size"
            )
    noise_type = EnumProperty(
            name="Noise Type",
            default='hetero_terrain',
            description="Noise type",
            items = [
                ('multi_fractal', "Multi Fractal", "Blender: Multi Fractal algorithm", 0),
                ('ridged_multi_fractal', "Ridged MFractal", "Blender: Ridged Multi Fractal", 1),
                ('hybrid_multi_fractal', "Hybrid MFractal", "Blender: Hybrid Multi Fractal", 2),
                ('hetero_terrain', "Hetero Terrain", "Blender: Hetero Terrain", 3),
                ('fractal', "fBm Fractal", "Blender: fBm - Fractional Browninian motion", 4),
                ('turbulence_vector', "Turbulence", "Blender: Turbulence Vector", 5),
                ('variable_lacunarity', "Distorted Noise", "Blender: Distorted Noise", 6),
                ('marble_noise', "Marble", "A.N.T.: Marble Noise", 7),
                ('shattered_hterrain', "Shattered hTerrain", "A.N.T.: Shattered hTerrain", 8),
                ('strata_hterrain', "Strata hTerrain", "A.N.T: Strata hTerrain", 9),
                ('ant_turbulence', "Another Noise", "A.N.T: Turbulence variation", 10),
                ('vl_noise_turbulence', "vlNoise turbulence", "A.N.T: Real vlNoise turbulence", 11),
                ('vl_hTerrain', "vlNoise hTerrain", "A.N.T: vlNoise hTerrain", 12),
                ('distorted_heteroTerrain', "Distorted hTerrain", "A.N.T distorted hTerrain", 13),
                ('double_multiFractal', "Double MultiFractal", "A.N.T: double multiFractal", 14),
                ('rocks_noise', "Noise Rocks", "A.N.T: turbulence variation", 15),
                ('slick_rock', "Slick Rock", "A.N.T: slick rock", 16),
                ('planet_noise', "Planet Noise", "Planet Noise by: Farsthary", 17),
                ('blender_texture', "Blender Texture - Texture Nodes", "Blender texture data block", 18)]
            )
    basis_type = EnumProperty(
            name="Noise Basis",
            default="0",
            description="Noise basis algorithms",
            items = [
                ("0", "Blender", "Blender default noise", 0),
                ("1", "Perlin", "Perlin noise", 1),
                ("2", "New Perlin", "New Perlin noise", 2),
                ("3", "Voronoi F1", "Voronoi F1", 3),
                ("4", "Voronoi F2", "Voronoi F2", 4),
                ("5", "Voronoi F3", "Voronoi F3", 5),
                ("6", "Voronoi F4", "Voronoi F4", 6),
                ("7", "Voronoi F2-F1", "Voronoi F2-F1", 7),
                ("8", "Voronoi Crackle", "Voronoi Crackle", 8),
                ("9", "Cell Noise", "Cell noise", 9)]
            )
    vl_basis_type = EnumProperty(
            name="vlNoise Basis",
            default="0",
            description="VLNoise basis algorithms",
            items = [
                ("0", "Blender", "Blender default noise", 0),
                ("1", "Perlin", "Perlin noise", 1),
                ("2", "New Perlin", "New Perlin noise", 2),
                ("3", "Voronoi F1", "Voronoi F1", 3),
                ("4", "Voronoi F2", "Voronoi F2", 4),
                ("5", "Voronoi F3", "Voronoi F3", 5),
                ("6", "Voronoi F4", "Voronoi F4", 6),
                ("7", "Voronoi F2-F1", "Voronoi F2-F1", 7),
                ("8", "Voronoi Crackle", "Voronoi Crackle", 8),
                ("9", "Cell Noise", "Cell noise", 9)]
            )
    distortion = FloatProperty(
            name="Distortion",
            default=1.0,
            min=0.01,
            max=100.0,
            description="Distortion amount"
            )
    hard_noise = EnumProperty(
            name="Soft Hard",
            default="0",
            description="Soft Noise, Hard noise",
            items = [
                ("0", "Soft", "Soft Noise", 0),
                ("1", "Hard", "Hard noise", 1)]
            )
    noise_depth = IntProperty(
            name="Depth",
            default=8,
            min=0,
            max=16,
            description="Noise Depth - number of frequencies in the fBm"
            )
    amplitude = FloatProperty(
            name="Amp",
            default=0.5,
            min=0.01,
            max=1.0,
            description="Amplitude"
            )
    frequency = FloatProperty(
            name="Freq",
            default=2.0,
            min=0.01,
            max=5.0,
            description="Frequency"
            )
    dimension = FloatProperty(
            name="Dimension",
            default=1.0,
            min=0.01,
            max=2.0,
            description="H - fractal dimension of the roughest areas"
            )
    lacunarity = FloatProperty(
            name="Lacunarity",
            min=0.01,
            max=6.0,
            default=2.0,
            description="Lacunarity - gap between successive frequencies"
            )
    offset = FloatProperty(
            name="Offset",
            default=1.0,
            min=0.01,
            max=6.0,
            description="Offset - raises the terrain from sea level"
            )
    gain = FloatProperty(
            name="Gain",
            default=1.0,
            min=0.01,
            max=6.0,
            description="Gain - scale factor"
            )
    marble_bias = EnumProperty(
            name="Bias",
            default="0",
            description="Marble bias",
            items = [
                ("0", "Sin", "Sin", 0),
                ("1", "Cos", "Cos", 1),
                ("2", "Tri", "Tri", 2),
                ("3", "Saw", "Saw", 3)]
            )
    marble_sharp = EnumProperty(
            name="Sharp",
            default="0",
            description="Marble sharpness",
            items = [
                ("0", "Soft", "Soft", 0),
                ("1", "Sharp", "Sharp", 1),
                ("2", "Sharper", "Sharper", 2),
                ("3", "Soft inv.", "Soft", 3),
                ("4", "Sharp inv.", "Sharp", 4),
                ("5", "Sharper inv.", "Sharper", 5)]
            )
    marble_shape = EnumProperty(
            name="Shape",
            default="0",
            description="Marble shape",
            items= [
                ("0", "Default", "Default", 0),
                ("1", "Ring", "Ring", 1),
                ("2", "Swirl", "Swirl", 2),
                ("3", "Bump", "Bump", 3),
                ("4", "Wave", "Wave", 4),
                ("5", "Z", "Z", 5),
                ("6", "Y", "Y", 6),
                ("7", "X", "X", 7)]
        )
    height = FloatProperty(
            name="Height",
            default=0.5,
            min=-10000.0,
            max=10000.0,
            description="Noise intensity scale"
            )
    height_invert = BoolProperty(
            name="Invert",
            default=False,
            description="Height invert",
            )
    height_offset = FloatProperty(
            name="Offset",
            default=0.0,
            min=-10000.0,
            max=10000.0,
            description="Height offset"
            )
    fx_mixfactor = FloatProperty(
            name="Mix Factor",
            default=0.0,
            min=-1.0,
            max=1.0,
            description="Effect mix factor: -1.0 = Noise, +1.0 = Effect"
            )
    fx_mix_mode = EnumProperty(
            name="Effect Mix",
            default="0",
            description="Effect mix mode",
            items = [
                ("0", "Mix", "Mix", 0),
                ("1", "Add", "Add", 1),
                ("2", "Sub", "Subtract", 2),
                ("3", "Mul", "Multiply", 3),
                ("4", "Abs", "Absolute", 4),
                ("5", "Scr", "Screen", 5),
                ("6", "Mod", "Modulo", 6),
                ("7", "Min", "Minimum", 7),
                ("8", "Max", "Maximum", 8)
                ]
            )
    fx_type = EnumProperty(
            name="Effect Type",
            default="0",
            description="Effect type",
            items = [
                ("0", "None", "No effect", 0),
                ("1", "Gradient", "Gradient", 1),
                ("2", "Waves", "Waves - Bumps", 2),
                ("3", "Zigzag", "Zigzag", 3),
                ("4", "Wavy", "Wavy", 4),
                ("5", "Bump", "Bump", 5),
                ("6", "Dots", "Dots", 6),
                ("7", "Rings", "Rings", 7),
                ("8", "Spiral", "Spiral", 8),
                ("9", "Square", "Square", 9),
                ("10", "Blocks", "Blocks", 10),
                ("11", "Grid", "Grid", 11),
                ("12", "Tech", "Tech", 12),
                ("13", "Crackle", "Crackle", 13),
                ("14", "Cracks", "Cracks", 14),
                ("15", "Rock", "Rock noise", 15),
                ("16", "Lunar", "Craters", 16),
                ("17", "Cosine", "Cosine", 17),
                ("18", "Spikey", "Spikey", 18),
                ("19", "Stone", "Stone", 19),
                ("20", "Flat Turb", "Flat turbulence", 20),
                ("21", "Flat Voronoi", "Flat voronoi", 21)
                ]
            )
    fx_bias = EnumProperty(
            name="Effect Bias",
            default="0",
            description="Effect bias type",
            items = [
                ("0", "Sin", "Sin", 0),
                ("1", "Cos", "Cos", 1),
                ("2", "Tri", "Tri", 2),
                ("3", "Saw", "Saw", 3),
                ("4", "None", "None", 4)]
            )
    fx_turb = FloatProperty(
            name="Distortion",
            default=0.0,
            min=0.0,
            max=1000.0,
            description="Effect turbulence distortion"
            )
    fx_depth = IntProperty(
            name="Depth",
            default=0,
            min=0,
            max=16,
            description="Effect depth - number of frequencies"
            )
    fx_amplitude = FloatProperty(
            name="Amp",
            default=0.5,
            min=0.01,
            max=1.0,
            description="Amplitude"
            )
    fx_frequency = FloatProperty(
            name="Freq",
            default=2.0,
            min=0.01,
            max=5.0,
            description="Frequency"
            )
    fx_size = FloatProperty(
            name="Effect Size",
            default=1.0,
            min=0.01,
            max=1000.0,
            description="Effect size"
            )
    fx_loc_x = FloatProperty(
            name="Offset X",
            default=0.0,
            description="Effect x offset"
            )
    fx_loc_y = FloatProperty(
            name="Offset Y",
            default=0.0,
            description="Effect y offset"
            )
    fx_height = FloatProperty(
            name="Intensity",
            default=1.0,
            min=-1000.0,
            max=1000.0,
            description="Effect intensity scale"
            )
    fx_invert = BoolProperty(
            name="Invert",
            default=False,
            description="Effect invert"
            )
    fx_offset = FloatProperty(
            name="Offset",
            default=0.0,
            min=-1000.0,
            max=1000.0,
            description="Effect height offset"
            )

    edge_falloff = EnumProperty(
            name="Falloff",
            default="3",
            description="Flatten edges",
            items = [
                ("0", "None", "None", 0),
                ("1", "Y", "Y Falloff", 1),
                ("2", "X", "X Falloff", 2),
                ("3", "X Y", "X Y Falloff", 3)]
            )
    falloff_x = FloatProperty(
            name="Falloff X",
            default=4.0,
            min=0.1,
            max=100.0,
            description="Falloff x scale"
            )
    falloff_y = FloatProperty(
            name="Falloff Y",
            default=4.0,
            min=0.1,
            max=100.0,
            description="Falloff y scale"
            )
    edge_level = FloatProperty(
            name="Edge Level",
            default=0.0,
            min=-10000.0,
            max=10000.0,
            description="Edge level, sealevel offset"
            )
    maximum = FloatProperty(
            name="Maximum",
            default=1.0,
            min=-10000.0,
            max=10000.0,
            description="Maximum, flattens terrain at plateau level"
            )
    minimum = FloatProperty(
            name="Minimum",
            default=-1.0,
            min=-10000.0,
            max=10000.0,
            description="Minimum, flattens terrain at seabed level"
            )
    vert_group = StringProperty(
            name="Vertex Group",
            default=""
            )
    strata = FloatProperty(
            name="Amount",
            default=5.0,
            min=0.01,
            max=1000.0,
            description="Strata layers / terraces"
            )
    strata_type = EnumProperty(
            name="Strata",
            default="0",
            description="Strata types",
            items = [
                ("0", "None", "No strata", 0),
                ("1", "Smooth", "Smooth transitions", 1),
                ("2", "Sharp Sub", "Sharp substract transitions", 2),
                ("3", "Sharp Add", "Sharp add transitions", 3),
                ("4", "Quantize", "Quantize", 4),
                ("5", "Quantize Mix", "Quantize mixed", 5)]
            )
    water_plane = BoolProperty(
            name="Water Plane",
            default=False,
            description="Add water plane"
            )
    water_level = FloatProperty(
            name="Level",
            default=0.01,
            min=-10000.0,
            max=10000.0,
            description="Water level"
            )
    remove_double = BoolProperty(
            name="Remove Doubles",
            default=False,
            description="Remove doubles"
            )
    show_main_settings = BoolProperty(
            name="Main Settings",
            default=True,
            description="Show settings"
            )
    show_noise_settings = BoolProperty(
            name="Noise Settings",
            default=True,
            description="Show noise settings"
            )
    show_displace_settings = BoolProperty(
            name="Displace Settings",
            default=True,
            description="Show displace settings"
            )
    refresh = BoolProperty(
            name="Refresh",
            default=False,
            description="Refresh"
            )
    auto_refresh = BoolProperty(
            name="Auto",
            default=True,
            description="Automatic refresh"
            )

    def draw(self, context):
        draw_ant_refresh(self, context)
        draw_ant_main(self, context, generate=True)
        draw_ant_noise(self, context)
        draw_ant_displace(self, context, generate=True)
        draw_ant_water(self, context)


    def invoke(self, context, event):
        self.refresh = True
        return self.execute(context)


    def execute(self, context):
        if not self.refresh:
            return {'PASS_THROUGH'}

        # turn off undo
        undo = bpy.context.user_preferences.edit.use_global_undo
        bpy.context.user_preferences.edit.use_global_undo = False

        # deselect all objects when in object mode
        if bpy.ops.object.select_all.poll():
            bpy.ops.object.select_all(action='DESELECT')

        # Properties
        ant_props = [
            self.ant_terrain_name,
            self.at_cursor,
            self.smooth_mesh,
            self.tri_face,
            self.sphere_mesh,
            self.land_material,
            self.water_material,
            self.texture_block,
            self.subdivision_x,
            self.subdivision_y,
            self.mesh_size_x,
            self.mesh_size_y,
            self.mesh_size,
            self.random_seed,
            self.noise_offset_x,
            self.noise_offset_y,
            self.noise_offset_z,
            self.noise_size_x,
            self.noise_size_y,
            self.noise_size_z,
            self.noise_size,
            self.noise_type,
            self.basis_type,
            self.vl_basis_type,
            self.distortion,
            self.hard_noise,
            self.noise_depth,
            self.amplitude,
            self.frequency,
            self.dimension,
            self.lacunarity,
            self.offset,
            self.gain,
            self.marble_bias,
            self.marble_sharp,
            self.marble_shape,
            self.height,
            self.height_invert,
            self.height_offset,
            self.maximum,
            self.minimum,
            self.edge_falloff,
            self.edge_level,
            self.falloff_x,
            self.falloff_y,
            self.strata_type,
            self.strata,
            self.water_plane,
            self.water_level,
            self.vert_group,
            self.remove_double,
            self.fx_mixfactor,
            self.fx_mix_mode,
            self.fx_type,
            self.fx_bias,
            self.fx_turb,
            self.fx_depth,
            self.fx_frequency,
            self.fx_amplitude,
            self.fx_size,
            self.fx_loc_x,
            self.fx_loc_y,
            self.fx_height,
            self.fx_offset,
            self.fx_invert
            ]

        scene = context.scene

        # Main function, create landscape mesh object
        if self.ant_terrain_name != "":
            new_name = self.ant_terrain_name
        else:
            new_name  = "Landscape"

        if self.sphere_mesh:
            # sphere
            verts, faces = sphere_gen(
                    self.subdivision_y,
                    self.subdivision_x,
                    self.tri_face,
                    self.mesh_size,
                    ant_props,
                    False,
                    0.0
                    )
            new_ob = create_mesh_object(context, verts, [], faces, new_name)
            if self.remove_double:
                new_ob.select = True
                bpy.ops.object.mode_set(mode = 'EDIT')
                bpy.ops.mesh.remove_doubles(threshold=0.0001, use_unselected=False)
                bpy.ops.object.mode_set(mode = 'OBJECT')
        else:
            # grid
            verts, faces = grid_gen(
                    self.subdivision_x,
                    self.subdivision_y,
                    self.tri_face,
                    self.mesh_size_x,
                    self.mesh_size_y,
                    ant_props,
                    False,
                    0.0
                    )
            new_ob = create_mesh_object(context, verts, [], faces, new_name)

        new_ob.select = True

        if self.smooth_mesh:
            bpy.ops.object.shade_smooth()

        if not self.at_cursor:
            new_ob.object.location = (0.0, 0.0, 0.0)

        # Landscape Material
        if self.land_material != "" and self.land_material in bpy.data.materials:
            mat = bpy.data.materials[self.land_material]
            bpy.context.object.data.materials.append(mat)

        # Water plane
        if self.water_plane:
            if self.sphere_mesh:
                # sphere
                verts, faces = sphere_gen(
                        self.subdivision_y,
                        self.subdivision_x,
                        self.tri_face,
                        self.mesh_size,
                        ant_props,
                        self.water_plane,
                        self.water_level
                        )
                wobj = create_mesh_object(context, verts, [], faces, new_name+"_plane")
                if self.remove_double:
                    wobj.select = True
                    bpy.ops.object.mode_set(mode = 'EDIT')
                    bpy.ops.mesh.remove_doubles(threshold=0.0001, use_unselected=False)
                    bpy.ops.object.mode_set(mode = 'OBJECT')
            else:
                # grid
                verts, faces = grid_gen(
                        2,
                        2,
                        self.tri_face,
                        self.mesh_size_x,
                        self.mesh_size_y,
                        ant_props,
                        self.water_plane,
                        self.water_level
                        )
                wobj = create_mesh_object(context, verts, [], faces, new_name+"_plane")

            wobj.select = True

            if self.smooth_mesh:
                bpy.ops.object.shade_smooth()

            if not self.at_cursor:
                wobj.object.location = (0.0, 0.0, 0.0)

            # Water Material
            if self.water_material != "" and self.water_material in bpy.data.materials:
                mat = bpy.data.materials[self.water_material]
                bpy.context.object.data.materials.append(mat)

        # select landscape and make active
        new_ob.select = True
        scene.objects.active = new_ob.object
        #
        new_ob = store_properties(self, new_ob.object)

        if self.auto_refresh is False:
            self.refresh = False

        # restore pre operator undo state
        context.user_preferences.edit.use_global_undo = undo

        return {'FINISHED'}
