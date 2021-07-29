# Copyright (c) 2012 Jorge Hernandez - Melendez

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

# TODO : prop names into English, add missing tooltips

bl_info = {
    "name": "Rope Creator",
    "description": "Dynamic rope (with cloth) creator",
    "author": "Jorge Hernandez - Melenedez",
    "version": (0, 2, 2),
    "blender": (2, 7, 3),
    "location": "Left Toolbar > ClothRope",
    "warning": "",
    "wiki_url": "",
    "category": "Add Mesh"
}


import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        FloatProperty,
        IntProperty,
        )


def desocultar(quien):
    if quien == "todo":
        for ob in bpy.data.objects:
            ob.hide = False
    else:
        bpy.data.objects[quien].hide = False


def deseleccionar_todo():
    bpy.ops.object.select_all(action='DESELECT')


def seleccionar_todo():
    bpy.ops.object.select_all(action='SELECT')


def salir_de_editmode():
    if bpy.context.mode in ["EDIT", "EDIT_MESH", "EDIT_CURVE"]:
        bpy.ops.object.mode_set(mode='OBJECT')


# Clear scene:
def reset_scene():
    desocultar("todo")
    # playback to the start
    bpy.ops.screen.frame_jump(end=False)
    try:
        salir_de_editmode()
    except:
        pass
    try:
        area = bpy.context.area
        # expand everything in the outliner to be able to select children
        old_type = area.type
        area.type = 'OUTLINER'
        bpy.ops.outliner.expanded_toggle()

        # restore the original context
        area.type = old_type

        seleccionar_todo()
        bpy.ops.object.delete(use_global=False)

    except Exception as e:
        print("\n[rope_alpha]\nfunction: reset_scene\nError: %s" % e)


def entrar_en_editmode():
    if bpy.context.mode == "OBJECT":
        bpy.ops.object.mode_set(mode='EDIT')


def select_all_in_edit_mode(ob):
    if ob.mode != 'EDIT':
        entrar_en_editmode()
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.context.tool_settings.mesh_select_mode = (True, False, False)
    salir_de_editmode()
    for v in ob.data.vertices:
        if not v.select:
            v.select = True
    entrar_en_editmode()


def deselect_all_in_edit_mode(ob):
    if ob.mode != 'EDIT':
        entrar_en_editmode()
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.context.tool_settings.mesh_select_mode = (True, False, False)
    salir_de_editmode()
    for v in ob.data.vertices:
        if not v.select:
            v.select = False
    entrar_en_editmode()


def which_vertex_are_selected(ob):
    for v in ob.data.vertices:
        if v.select:
            print(str(v.index))
            print("Vertex " + str(v.index) + " is selected")


def seleccionar_por_nombre(nombre):
    scn = bpy.context.scene
    bpy.data.objects[nombre].select = True

    scn.objects.active = bpy.data.objects[nombre]


def deseleccionar_por_nombre(nombre):
    bpy.data.objects[nombre].select = False


def crear_vertices(ob):
    ob.data.vertices.add(1)
    ob.data.update


def borrar_elementos_seleccionados(tipo):
    if tipo == "vertices":
        bpy.ops.mesh.delete(type='VERT')


def obtener_coords_vertex_seleccionados():
    coordenadas_de_vertices = []
    for ob in bpy.context.selected_objects:
        if ob.type == 'MESH':
            for v in ob.data.vertices:
                if v.select:
                    coordenadas_de_vertices.append([v.co[0], v.co[1], v.co[2]])
            return coordenadas_de_vertices[0]


def crear_locator(pos):
    bpy.ops.object.empty_add(
            type='PLAIN_AXES', radius=1, view_align=False,
            location=(pos[0], pos[1], pos[2]),
            layers=(True, False, False, False, False, False, False,
                    False, False, False, False, False, False, False,
                    False, False, False, False, False, False)
            )


def extruir_vertices(longitud, cuantos_segmentos):
    bpy.ops.mesh.extrude_region_move(
            MESH_OT_extrude_region={"mirror": False},
            TRANSFORM_OT_translate={
                    "value": (longitud / cuantos_segmentos, 0, 0),
                    "constraint_axis": (True, False, False),
                    "constraint_orientation": 'GLOBAL', "mirror": False,
                    "proportional": 'DISABLED', "proportional_edit_falloff": 'SMOOTH',
                    "proportional_size": 1, "snap": False, "snap_target": 'CLOSEST',
                    "snap_point": (0, 0, 0), "snap_align": False, "snap_normal": (0, 0, 0),
                    "gpencil_strokes": False, "texture_space": False,
                    "remove_on_cancel": False, "release_confirm": False
                    }
            )


def select_all_vertex_in_curve_bezier(bc):
    for i in range(len(bc.data.splines[0].points)):
        bc.data.splines[0].points[i].select = True


def deselect_all_vertex_in_curve_bezier(bc):
    for i in range(len(bc.data.splines[0].points)):
        bc.data.splines[0].points[i].select = False


def ocultar_relationships():
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces[0].show_relationship_lines = False


class ClothRope(Operator):
    bl_idname = "clot.rope"
    bl_label = "Rope Cloth"
    bl_description = ("Create a new Scene with a Cloth modifier\n"
                      "Rope Simulation with hooked Helper Objects")

    ropelenght = IntProperty(
            name="Rope Length",
            description="Length of the generated Rope",
            default=5
            )
    ropesegments = IntProperty(
            name="Rope Segments",
            description="Number of the Rope Segments",
            default=5
            )
    qcr = IntProperty(
            name="Collision Quality",
            description="Rope's Cloth modifier collsion quality",
            min=1, max=20,
            default=20
            )
    substeps = IntProperty(
            name="Rope Substeps",
            description="Rope's Cloth modifier quality",
            min=4, max=80,
            default=50
            )
    resrope = IntProperty(
            name="Rope Resolution",
            description="Rope's Bevel resolution",
            default=5
            )
    radiusrope = FloatProperty(
            name="Radius",
            description="Rope's Radius",
            min=0.04, max=1,
            default=0.04
            )
    hide_emptys = BoolProperty(
            name="Hide Empties",
            description="Hide Helper Objects",
            default=False
            )

    def execute(self, context):
        # add a new scene
        bpy.ops.scene.new(type="NEW")
        scene = bpy.context.scene
        scene.name = "Test Rope"
        seleccionar_todo()
        longitud = self.ropelenght

        # For the middle to have x segments between the first and
        # last point, must add 1 to the quantity:
        cuantos_segmentos = self.ropesegments + 1
        calidad_de_colision = self.qcr
        substeps = self.substeps
        deseleccionar_todo()
        # collect the possible empties that already exist in the data
        empties_prev = [obj.name for obj in bpy.data.objects if obj.type == "EMPTY"]

        # create an empty that will be the parent of everything
        bpy.ops.object.empty_add(
                type='SPHERE', radius=1, view_align=False, location=(0, 0, 0),
                layers=(True, False, False, False, False, False, False, False,
                        False, False, False, False, False, False, False, False,
                        False, False, False, False)
                )
        ob = bpy.context.selected_objects[0]
        ob.name = "Rope"
        # .001 and friends
        rope_name = ob.name
        deseleccionar_todo()

        # create a plane and delete it
        bpy.ops.mesh.primitive_plane_add(
                radius=1, view_align=False, enter_editmode=False, location=(0, 0, 0),
                layers=(True, False, False, False, False, False, False, False, False,
                        False, False, False, False, False, False, False, False,
                        False, False, False)
                )
        ob = bpy.context.selected_objects[0]
        # rename:
        ob.name = "cuerda"
        # .001 and friends
        cuerda_1_name = ob.name

        entrar_en_editmode()  # enter edit mode
        select_all_in_edit_mode(ob)

        borrar_elementos_seleccionados("vertices")
        salir_de_editmode()  # leave edit mode
        crear_vertices(ob)  # create a vertex

        # Creating a Group for the PIN
        # Group contains the vertices of the pin and the Group.001 contains the single main line
        entrar_en_editmode()  # enter edit mode
        bpy.ops.object.vertex_group_add()  # create a group
        select_all_in_edit_mode(ob)
        bpy.ops.object.vertex_group_assign()  # assign it

        salir_de_editmode()  # leave edit mode
        ob.vertex_groups[0].name = "Pin"
        deseleccionar_todo()
        seleccionar_por_nombre(cuerda_1_name)

        # extrude vertices:
        for i in range(cuantos_segmentos):
            entrar_en_editmode()
            extruir_vertices(longitud, cuantos_segmentos)
            # delete the PIN group
            bpy.ops.object.vertex_group_remove_from()
            # get the direction to create the locator on it's position
            pos = obtener_coords_vertex_seleccionados()

            salir_de_editmode()  # leave edit mode
            # create locator at position
            crear_locator(pos)
            deseleccionar_todo()
            seleccionar_por_nombre(cuerda_1_name)
        deseleccionar_todo()

        seleccionar_por_nombre(cuerda_1_name)  # select the rope
        entrar_en_editmode()

        pos = obtener_coords_vertex_seleccionados()  # get their positions
        salir_de_editmode()
        # create the last locator
        crear_locator(pos)
        deseleccionar_todo()
        seleccionar_por_nombre(cuerda_1_name)
        entrar_en_editmode()  # enter edit mode
        bpy.ops.object.vertex_group_add()  # Creating Master guide group
        select_all_in_edit_mode(ob)
        bpy.ops.object.vertex_group_assign()  # and assing it
        ob.vertex_groups[1].name = "Guide_rope"

        # extrude the Curve so it has a minumum thickness for collide
        bpy.ops.mesh.extrude_region_move(
                MESH_OT_extrude_region={"mirror": False},
                TRANSFORM_OT_translate={
                        "value": (0, 0.005, 0), "constraint_axis": (False, True, False),
                        "constraint_orientation": 'GLOBAL', "mirror": False,
                        "proportional": 'DISABLED', "proportional_edit_falloff": 'SMOOTH',
                        "proportional_size": 1, "snap": False, "snap_target": 'CLOSEST',
                        "snap_point": (0, 0, 0), "snap_align": False, "snap_normal": (0, 0, 0),
                        "gpencil_strokes": False, "texture_space": False,
                        "remove_on_cancel": False, "release_confirm": False
                        }
                )
        bpy.ops.object.vertex_group_remove_from()
        deselect_all_in_edit_mode(ob)
        salir_de_editmode()
        bpy.ops.object.modifier_add(type='CLOTH')
        bpy.context.object.modifiers["Cloth"].settings.use_pin_cloth = True
        bpy.context.object.modifiers["Cloth"].settings.vertex_group_mass = "Pin"
        bpy.context.object.modifiers["Cloth"].collision_settings.collision_quality = calidad_de_colision
        bpy.context.object.modifiers["Cloth"].settings.quality = substeps

        # Duplicate to convert into Curve:
        # select the vertices that are the part of the Group.001
        seleccionar_por_nombre(cuerda_1_name)
        entrar_en_editmode()
        bpy.ops.mesh.select_all(action="DESELECT")
        bpy.context.tool_settings.mesh_select_mode = (True, False, False)
        salir_de_editmode()
        gi = ob.vertex_groups["Guide_rope"].index  # get group index

        for v in ob.data.vertices:
            for g in v.groups:
                if g.group == gi:  # compare with index in VertexGroupElement
                    v.select = True

        # now we have to make a table of names of cuerdas to see which one will be new
        cuerda_names = [obj.name for obj in bpy.data.objects if "cuerda" in obj.name]

        entrar_en_editmode()

        # we already have the selected guide:
        # duplicate it:
        bpy.ops.mesh.duplicate_move(
                MESH_OT_duplicate={"mode": 1},
                TRANSFORM_OT_translate={
                        "value": (0, 0, 0), "constraint_axis": (False, False, False),
                        "constraint_orientation": 'GLOBAL', "mirror": False,
                        "proportional": 'DISABLED', "proportional_edit_falloff": 'SMOOTH',
                        "proportional_size": 1, "snap": False, "snap_target": 'CLOSEST',
                        "snap_point": (0, 0, 0), "snap_align": False, "snap_normal": (0, 0, 0),
                        "gpencil_strokes": False, "texture_space": False,
                        "remove_on_cancel": False, "release_confirm": False
                        }
                )
        # separate the selections:
        bpy.ops.mesh.separate(type='SELECTED')
        salir_de_editmode()
        deseleccionar_todo()

        cuerda_2_name = "cuerda.001"
        test = []
        for obj in bpy.data.objects:
            if "cuerda" in obj.name and obj.name not in cuerda_names:
                cuerda_2_name = obj.name
                test.append(obj.name)

        seleccionar_por_nombre(cuerda_2_name)

        # from the newly created curve remove the Cloth:
        bpy.ops.object.modifier_remove(modifier="Cloth")
        # convert the Curve:
        bpy.ops.object.convert(target='CURVE')

        # all Empties that are not previously present
        emptys = []
        for eo in bpy.data.objects:
            if eo.type == 'EMPTY' and eo.name not in empties_prev:
                if eo.name != rope_name:
                    emptys.append(eo)

        # select and deselect:
        bc = bpy.data.objects[cuerda_2_name]
        n = 0

        for e in emptys:
            deseleccionar_todo()
            seleccionar_por_nombre(e.name)
            seleccionar_por_nombre(bc.name)
            entrar_en_editmode()
            deselect_all_vertex_in_curve_bezier(bc)
            bc.data.splines[0].points[n].select = True
            bpy.ops.object.hook_add_selob(use_bone=False)
            salir_de_editmode()
            n = n + 1

        ob = bpy.data.objects[cuerda_1_name]
        n = 0

        for e in emptys:
            deseleccionar_todo()
            seleccionar_por_nombre(e.name)
            seleccionar_por_nombre(ob.name)
            entrar_en_editmode()
            bpy.ops.mesh.select_all(action="DESELECT")
            bpy.context.tool_settings.mesh_select_mode = (True, False, False)
            salir_de_editmode()

            for v in ob.data.vertices:
                if v.select:
                    v.select = False
            ob.data.vertices[n].select = True
            entrar_en_editmode()
            bpy.ops.object.vertex_parent_set()

            salir_de_editmode()
            n = n + 1

            # hide the Empties:
            deseleccionar_todo()

        # all parented to the spherical empty:
        seleccionar_por_nombre(cuerda_2_name)
        seleccionar_por_nombre(cuerda_1_name)
        seleccionar_por_nombre(rope_name)
        bpy.ops.object.parent_set(type='OBJECT', keep_transform=True)
        deseleccionar_todo()

        # do not display the relations
        ocultar_relationships()
        seleccionar_por_nombre(cuerda_2_name)

        # curved rope settings:
        bpy.context.object.data.fill_mode = 'FULL'
        bpy.context.object.data.bevel_depth = self.radiusrope
        bpy.context.object.data.bevel_resolution = self.resrope

        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width=350)

    def draw(self, context):
        layout = self.layout
        box = layout.box()
        col = box.column(align=True)

        col.label("Rope settings:")
        rowsub0 = col.row()
        rowsub0.prop(self, "ropelenght", text="Length")
        rowsub0.prop(self, "ropesegments", text="Segments")
        rowsub0.prop(self, "radiusrope", text="Radius")

        col.label("Quality Settings:")
        col.prop(self, "resrope", text="Resolution curve")
        col.prop(self, "qcr", text="Quality Collision")
        col.prop(self, "substeps", text="Substeps")


class BallRope(Operator):
    bl_idname = "ball.rope"
    bl_label = "Wrecking Ball"
    bl_description = ("Create a new Scene with a Rigid Body simulation of\n"
                      "Wrecking Ball on a rope")

    # defaults rope ball
    ropelenght2 = IntProperty(
            name="Rope Length",
            description="Length of the Wrecking Ball rope",
            default=10
            )
    ropesegments2 = IntProperty(
            name="Rope Segments",
            description="Number of the Wrecking Ball rope segments",
            min=0, max=999,
            default=6
            )
    radiuscubes = FloatProperty(
            name="Cube Radius",
            description="Size of the Linked Cubes helpers",
            default=0.5
            )
    radiusrope = FloatProperty(
            name="Rope Radius",
            description="Radius of the Rope",
            default=0.4
            )
    worldsteps = IntProperty(
            name="World Steps",
            description="Rigid Body Solver world steps per second (update)",
            min=60, max=1000,
            default=250
            )
    solveriterations = IntProperty(
            name="Solver Iterations",
            description="How many times the Rigid Body Solver should run",
            min=10, max=100,
            default=50
            )
    massball = IntProperty(
            name="Ball Mass",
            description="Mass of the Wrecking Ball",
            default=1
            )
    resrope = IntProperty(
            name="Resolution",
            description="Rope resolution",
            default=4
            )
    grados = FloatProperty(
            name="Degrees",
            description="Angle of the Wrecking Ball compared to the Ground Plane",
            default=45
            )
    separacion = FloatProperty(
            name="Link Cubes Gap",
            description="Space between the Rope's Linked Cubes",
            default=0.1
            )
    hidecubes = BoolProperty(
            name="Hide Link Cubes",
            description="Hide helper geometry for the Rope",
            default=False
            )

    def execute(self, context):
        world_steps = self.worldsteps
        solver_iterations = self.solveriterations
        longitud = self.ropelenght2

        # make a + 2, so the segments will be between the two end points...
        segmentos = self.ropesegments2 + 2
        offset_del_suelo = 1
        offset_del_suelo_real = (longitud / 2) + (segmentos / 2)
        radio = self.radiuscubes
        radiorope = self.radiusrope
        masa = self.massball
        resolucion = self.resrope
        rotrope = self.grados
        separation = self.separacion
        hidecubeslinks = self.hidecubes

        # add new scene
        bpy.ops.scene.new(type="NEW")
        scene = bpy.context.scene
        scene.name = "Test Ball"

        # collect the possible constraint empties that already exist in the data
        constraint_prev = [obj.name for obj in bpy.data.objects if
                           obj.type == "EMPTY" and "Constraint" in obj.name]
        # floor:
        bpy.ops.mesh.primitive_cube_add(
                radius=1, view_align=False, enter_editmode=False, location=(0, 0, 0),
                layers=(True, False, False, False, False, False, False, False, False,
                        False, False, False, False, False, False, False, False,
                        False, False, False)
                )
        bpy.context.object.scale.x = 10 + longitud
        bpy.context.object.scale.y = 10 + longitud
        bpy.context.object.scale.z = 0.05
        bpy.context.object.name = "groundplane"
        # The secret agents .001, 002 etc.
        groundplane_name = bpy.context.object.name

        bpy.ops.rigidbody.objects_add(type='PASSIVE')

        # create the first cube:
        cuboslink = []
        n = 0
        for i in range(segmentos):
            # if 0 start from 1
            if i == 0:
                i = offset_del_suelo
            else:  # if it is not 0, add one so it doesn't step on the first one starting from 1
                i = i + offset_del_suelo
            separacion = longitud * 2 / segmentos  # distance between linked cubes
            bpy.ops.mesh.primitive_cube_add(
                    radius=1, view_align=False, enter_editmode=False,
                    location=(0, 0, i * separacion),
                    layers=(True, False, False, False, False, False, False, False,
                            False, False, False, False, False, False, False, False,
                            False, False, False, False)
                    )
            bpy.ops.rigidbody.objects_add(type='ACTIVE')
            bpy.context.object.name = "CubeLink"
            if n != 0:
                bpy.context.object.draw_type = 'WIRE'
                bpy.context.object.hide_render = True
            n += 1
            bpy.context.object.scale.z = (longitud * 2) / (segmentos * 2) - separation
            bpy.context.object.scale.x = radio
            bpy.context.object.scale.y = radio
            cuboslink.append(bpy.context.object)

        for i in range(len(cuboslink)):
            deseleccionar_todo()
            if i != len(cuboslink) - 1:
                nombre1 = cuboslink[i]
                nombre2 = cuboslink[i + 1]
                seleccionar_por_nombre(nombre1.name)
                seleccionar_por_nombre(nombre2.name)
                bpy.ops.rigidbody.connect()

        # select by name
        constraint_new = [
                    obj.name for obj in bpy.data.objects if
                    obj.type == "EMPTY" and "Constraint" in obj.name and
                    obj.name not in constraint_prev
                    ]

        for names in constraint_new:
            seleccionar_por_nombre(names)

        for c in bpy.context.selected_objects:
            c.rigid_body_constraint.type = 'POINT'
        deseleccionar_todo()

        # create a Bezier curve:
        bpy.ops.curve.primitive_bezier_curve_add(
                radius=1, view_align=False, enter_editmode=False, location=(0, 0, 0),
                layers=(True, False, False, False, False, False, False, False, False,
                False, False, False, False, False, False, False, False, False, False, False)
                )
        bpy.context.object.name = "Cuerda"
        # Blender will automatically append the .001
        # if it is already in data
        real_name = bpy.context.object.name

        for i in range(len(cuboslink)):
            cubonombre = cuboslink[i].name
            seleccionar_por_nombre(cubonombre)
            seleccionar_por_nombre(real_name)
            x = cuboslink[i].location[0]
            y = cuboslink[i].location[1]
            z = cuboslink[i].location[2]

            # if it is 0 make it start from 1 as the offset from the ground...
            if i == 0:
                i = offset_del_suelo
            else:  # if it is not 0, add one so it doesn't step on the first one starting from 1
                i = i + offset_del_suelo

            salir_de_editmode()
            entrar_en_editmode()

            if i == 1:
                # select all the vertices and delete them
                select_all_vertex_in_curve_bezier(bpy.data.objects[real_name])
                bpy.ops.curve.delete(type='VERT')
                # create the first vertex:
                bpy.ops.curve.vertex_add(location=(x, y, z))
            else:
                # extrude the rest:
                bpy.ops.curve.extrude_move(
                        CURVE_OT_extrude={"mode": 'TRANSLATION'},
                        TRANSFORM_OT_translate={
                            "value": (0, 0, z / i),
                            "constraint_axis": (False, False, True),
                            "constraint_orientation": 'GLOBAL', "mirror": False,
                            "proportional": 'DISABLED', "proportional_edit_falloff": 'SMOOTH',
                            "proportional_size": 1, "snap": False, "snap_target": 'CLOSEST',
                            "snap_point": (0, 0, 0), "snap_align": False, "snap_normal": (0, 0, 0),
                            "gpencil_strokes": False, "texture_space": False,
                            "remove_on_cancel": False, "release_confirm": False
                            }
                        )
            bpy.ops.object.hook_add_selob(use_bone=False)
            salir_de_editmode()
            bpy.context.object.data.bevel_resolution = resolucion
            deseleccionar_todo()

        # create a sphere ball:
        deseleccionar_todo()
        seleccionar_por_nombre(cuboslink[0].name)
        entrar_en_editmode()
        z = cuboslink[0].scale.z + longitud / 2
        bpy.ops.view3d.snap_cursor_to_selected()
        bpy.ops.mesh.primitive_uv_sphere_add(
                view_align=False, enter_editmode=False,
                layers=(True, False, False, False, False, False, False,
                        False, False, False, False, False, False, False,
                        False, False, False, False, False, False)
                )
        bpy.ops.transform.translate(
                value=(0, 0, -z + 2), constraint_axis=(False, False, True),
                constraint_orientation='GLOBAL', mirror=False, proportional='DISABLED',
                proportional_edit_falloff='SMOOTH', proportional_size=1
                )
        bpy.ops.transform.resize(
                value=(longitud / 2, longitud / 2, longitud / 2),
                constraint_axis=(False, False, False),
                constraint_orientation='GLOBAL',
                mirror=False, proportional='DISABLED',
                proportional_edit_falloff='SMOOTH', proportional_size=1
                )
        deselect_all_in_edit_mode(cuboslink[0])
        salir_de_editmode()
        bpy.ops.object.shade_smooth()
        bpy.context.object.rigid_body.mass = masa
        bpy.ops.object.origin_set(type='ORIGIN_CENTER_OF_MASS')

        # move it all up a bit more:
        seleccionar_todo()
        deseleccionar_por_nombre(groundplane_name)
        bpy.ops.transform.translate(
                value=(0, 0, offset_del_suelo_real),
                constraint_axis=(False, False, True),
                constraint_orientation='GLOBAL', mirror=False,
                proportional='DISABLED', proportional_edit_falloff='SMOOTH',
                proportional_size=1
                )

        deseleccionar_todo()
        seleccionar_por_nombre(cuboslink[-1].name)
        bpy.ops.rigidbody.objects_add(type='PASSIVE')

        bpy.context.scene.rigidbody_world.steps_per_second = world_steps
        bpy.context.scene.rigidbody_world.solver_iterations = solver_iterations

        # move everything from the top one:
        seleccionar_por_nombre(cuboslink[-1].name)
        bpy.ops.view3d.snap_cursor_to_selected()
        seleccionar_todo()
        deseleccionar_por_nombre(groundplane_name)
        deseleccionar_por_nombre(cuboslink[-1].name)
        bpy.context.space_data.pivot_point = 'CURSOR'
        bpy.ops.transform.rotate(
                value=rotrope, axis=(1, 0, 0),
                constraint_axis=(True, False, False),
                constraint_orientation='GLOBAL',
                mirror=False, proportional='DISABLED',
                proportional_edit_falloff='SMOOTH',
                proportional_size=1
                )
        bpy.context.space_data.pivot_point = 'MEDIAN_POINT'
        deseleccionar_todo()

        seleccionar_por_nombre(real_name)
        bpy.context.object.data.fill_mode = 'FULL'
        bpy.context.object.data.bevel_depth = radiorope
        for ob in bpy.data.objects:
            if ob.name != cuboslink[0].name:
                if ob.name.find("CubeLink") >= 0:
                    deseleccionar_todo()
                    seleccionar_por_nombre(ob.name)
                    if hidecubeslinks:
                        bpy.context.object.hide = True
        ocultar_relationships()
        deseleccionar_todo()
        return {'FINISHED'}

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self, width=350)

    def draw(self, context):
        layout = self.layout
        box = layout.box()
        col = box.column(align=True)

        col.label("Rope settings:")
        rowsub0 = col.row()
        rowsub0.prop(self, "hidecubes", text="Hide Link Cubes")

        rowsub1 = col.row(align=True)
        rowsub1.prop(self, "ropelenght2", text="Length")
        rowsub1.prop(self, "ropesegments2", text="Segments")

        rowsub2 = col.row(align=True)
        rowsub2.prop(self, "radiuscubes", text="Radius Link Cubes")
        rowsub2.prop(self, "radiusrope", text="Radius Rope")

        rowsub3 = col.row(align=True)
        rowsub3.prop(self, "grados", text="Degrees")
        rowsub3.prop(self, "separacion", text="Separation Link Cubes")

        col.label("Quality Settings:")
        col.prop(self, "resrope", text="Resolution Rope")
        col.prop(self, "massball", text="Ball Mass")
        col.prop(self, "worldsteps", text="World Steps")
        col.prop(self, "solveriterations", text="Solver Iterarions")


# Register

def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
