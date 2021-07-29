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

# TO DO LIST #
# Add more options to curve radius/modulation plus cyclic/connect curve option

import bpy
import selection_utils
from bpy.types import Operator
from random import (
        choice as rand_choice,
        random as rand_random,
        randint as rand_randint,
        uniform as rand_uniform,
        )


def error_handlers(self, op_name, error, reports="ERROR", func=False):
    if self and reports:
        self.report({'WARNING'}, reports + " (See Console for more info)")

    is_func = "Function" if func else "Operator"
    print("\n[Btrace]\n{}: {}\nError: {}\n".format(op_name, is_func, error))


# Object Trace
# creates a curve with a modulated radius connecting points of a mesh

class OBJECT_OT_objecttrace(Operator):
    bl_idname = "object.btobjecttrace"
    bl_label = "Btrace: Object Trace"
    bl_description = ("Trace selected mesh object with a curve with the option to animate\n"
                      "The Active Object has to be of a Mesh or Font type")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.object and
                context.object.type in {'MESH', 'FONT'})

    def invoke(self, context, event):
        try:
            # Run through each selected object and convert to to a curved object
            brushObj = context.selected_objects
            Btrace = context.window_manager.curve_tracer
            check_materials = True
            # Duplicate Mesh
            if Btrace.object_duplicate:
                bpy.ops.object.duplicate_move()
                brushObj = context.selected_objects
            # Join Mesh
            if Btrace.convert_joinbefore:
                if len(brushObj) > 1:  # Only run if multiple objects selected
                    bpy.ops.object.join()
                    brushObj = context.selected_objects

            for i in brushObj:
                context.scene.objects.active = i
                if i and i.type != 'CURVE':
                    bpy.ops.object.btconvertcurve()
                    # Materials
                    trace_mats = addtracemat(bpy.context.object.data)
                    if not trace_mats and check_materials is True:
                        check_materials = False
                if Btrace.animate:
                    bpy.ops.curve.btgrow()

            if check_materials is False:
                self.report({'WARNING'}, "Some Materials could not be added")

            return {'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.btobjecttrace", e,
                           "Object Trace could not be completed")

            return {'CANCELLED'}


# Objects Connect
# connect selected objects with a curve + hooks to each node
# possible handle types: 'FREE' 'AUTO' 'VECTOR' 'ALIGNED'

class OBJECT_OT_objectconnect(Operator):
    bl_idname = "object.btobjectsconnect"
    bl_label = "Btrace: Objects Connect"
    bl_description = ("Connect selected objects with a curve and add hooks to each node\n"
                      "Needs at least two objects selected")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return len(context.selected_objects) > 1

    def invoke(self, context, event):
        try:
            lists = []
            Btrace = context.window_manager.curve_tracer
            curve_handle = Btrace.curve_handle
            if curve_handle == 'AUTOMATIC':  # hackish because of naming conflict in api
                curve_handle = 'AUTO'
            # Check if Btrace group exists, if not create
            bgroup = bpy.data.groups.keys()
            if 'Btrace' not in bgroup:
                bpy.ops.group.create(name="Btrace")
            #  check if noise
            if Btrace.connect_noise:
                bpy.ops.object.btfcnoise()
            # check if respect order is checked, create list of objects
            if Btrace.respect_order is True:
                selobnames = selection_utils.selected
                obnames = []
                for ob in selobnames:
                    obnames.append(bpy.data.objects[ob])
            else:
                obnames = bpy.context.selected_objects  # No selection order

            for a in obnames:
                lists.append(a)
                a.select = False

            # trace the origins
            tracer = bpy.data.curves.new('tracer', 'CURVE')
            tracer.dimensions = '3D'
            spline = tracer.splines.new('BEZIER')
            spline.bezier_points.add(len(lists) - 1)
            curve = bpy.data.objects.new('curve', tracer)
            bpy.context.scene.objects.link(curve)

            # render ready curve
            tracer.resolution_u = Btrace.curve_u
            # Set bevel resolution from Panel options
            tracer.bevel_resolution = Btrace.curve_resolution
            tracer.fill_mode = 'FULL'
            # Set bevel depth from Panel options
            tracer.bevel_depth = Btrace.curve_depth

            # move nodes to objects
            for i in range(len(lists)):
                p = spline.bezier_points[i]
                p.co = lists[i].location
                p.handle_right_type = curve_handle
                p.handle_left_type = curve_handle

            bpy.context.scene.objects.active = curve
            bpy.ops.object.mode_set(mode='OBJECT')

            # place hooks
            for i in range(len(lists)):
                lists[i].select = True
                curve.data.splines[0].bezier_points[i].select_control_point = True
                bpy.ops.object.mode_set(mode='EDIT')
                bpy.ops.object.hook_add_selob()
                bpy.ops.object.mode_set(mode='OBJECT')
                curve.data.splines[0].bezier_points[i].select_control_point = False
                lists[i].select = False

            bpy.ops.object.select_all(action='DESELECT')
            curve.select = True  # selected curve after it's created
            # Materials
            check_materials = True
            trace_mats = addtracemat(bpy.context.object.data)
            if not trace_mats and check_materials is True:
                check_materials = False

            if Btrace.animate:   # Add Curve Grow it?
                bpy.ops.curve.btgrow()

            bpy.ops.object.group_link(group="Btrace")  # add to Btrace group
            if Btrace.animate:
                bpy.ops.curve.btgrow()  # Add grow curve

            if check_materials is False:
                self.report({'WARNING'}, "Some Materials could not be added")

            return {'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.btobjectsconnect", e,
                           "Objects Connect could not be completed")

            return {'CANCELLED'}


# Particle Trace
# creates a curve from each particle of a system

def curvetracer(curvename, splinename):
    Btrace = bpy.context.window_manager.curve_tracer
    tracer = bpy.data.curves.new(splinename, 'CURVE')
    tracer.dimensions = '3D'
    curve = bpy.data.objects.new(curvename, tracer)
    bpy.context.scene.objects.link(curve)
    try:
        tracer.fill_mode = 'FULL'
    except:
        tracer.use_fill_front = tracer.use_fill_back = False
    tracer.bevel_resolution = Btrace.curve_resolution
    tracer.bevel_depth = Btrace.curve_depth
    tracer.resolution_u = Btrace.curve_u

    return tracer, curve


class OBJECT_OT_particletrace(Operator):
    bl_idname = "particles.particletrace"
    bl_label = "Btrace: Particle Trace"
    bl_description = ("Creates a curve from each particle of a system.\n"
                      "Keeping particle amount under 250 will make this run faster")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.object is not None and
                context.object.particle_systems)

    def execute(self, context):
        try:
            Btrace = context.window_manager.curve_tracer
            particle_step = Btrace.particle_step    # step size in frames
            obj = bpy.context.object
            ps = obj.particle_systems.active
            curvelist = []
            curve_handle = Btrace.curve_handle
            check_materials = True

            if curve_handle == 'AUTOMATIC':   # hackish naming conflict
                curve_handle = 'AUTO'
            if curve_handle == 'FREE_ALIGN':  # hackish naming conflict
                curve_handle = 'FREE'

            # Check if Btrace group exists, if not create
            bgroup = bpy.data.groups.keys()
            if 'Btrace' not in bgroup:
                bpy.ops.group.create(name="Btrace")

            if Btrace.curve_join:
                tracer = curvetracer('Tracer', 'Splines')
            for x in ps.particles:
                if not Btrace.curve_join:
                    tracer = curvetracer('Tracer.000', 'Spline.000')
                spline = tracer[0].splines.new('BEZIER')
                # add point to spline based on step size
                spline.bezier_points.add((x.lifetime - 1) // particle_step)
                for t in list(range(int(x.lifetime))):
                    bpy.context.scene.frame_set(t + x.birth_time)
                    if not t % particle_step:
                        p = spline.bezier_points[t // particle_step]
                        p.co = x.location
                        p.handle_right_type = curve_handle
                        p.handle_left_type = curve_handle
                particlecurve = tracer[1]
                curvelist.append(particlecurve)
            # add to group
            bpy.ops.object.select_all(action='DESELECT')
            for curveobject in curvelist:
                curveobject.select = True
                bpy.context.scene.objects.active = curveobject
                bpy.ops.object.group_link(group="Btrace")
                # Materials
                trace_mats = addtracemat(curveobject.data)
                if not trace_mats and check_materials is True:
                    check_materials = False

            if Btrace.animate:
                bpy.ops.curve.btgrow()  # Add grow curve

            if check_materials is False:
                self.report({'WARNING'}, "Some Materials could not be added")

            return {'FINISHED'}

        except Exception as e:
            error_handlers(self, "particles.particletrace", e,
                           "Particle Trace could not be completed")

            return {'CANCELLED'}


# Particle Connect
# connect all particles in active system with a continuous animated curve

class OBJECT_OT_traceallparticles(Operator):
    bl_idname = "particles.connect"
    bl_label = "Connect Particles"
    bl_description = ("Create a continuous animated curve from particles in active system\n"
                      "Needs an Object with a particle system attached")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.object is not None and
                context.object.particle_systems)

    def execute(self, context):
        try:
            obj = context.object
            ps = obj.particle_systems.active
            setting = ps.settings

            # Grids distribution not supported
            if setting.distribution == 'GRID':
                self.report({'INFO'},
                            "Grid distribution mode for particles not supported")
                return{'CANCELLED'}

            Btrace = context.window_manager.curve_tracer
            # Get frame start
            particle_f_start = Btrace.particle_f_start
            # Get frame end
            particle_f_end = Btrace.particle_f_end
            curve_handle = Btrace.curve_handle
            # hackish because of naming conflict in api
            if curve_handle == 'AUTOMATIC':
                curve_handle = 'AUTO'
            if curve_handle == 'FREE_ALIGN':
                curve_handle = 'FREE'

            # define what kind of object to create
            tracer = bpy.data.curves.new('Splines', 'CURVE')
            # Create new object with settings listed above
            curve = bpy.data.objects.new('Tracer', tracer)
            # Link newly created object to the scene
            bpy.context.scene.objects.link(curve)
            # add a new Bezier point in the new curve
            spline = tracer.splines.new('BEZIER')
            spline.bezier_points.add(setting.count - 1)

            tracer.dimensions = '3D'
            tracer.resolution_u = Btrace.curve_u
            tracer.bevel_resolution = Btrace.curve_resolution
            tracer.fill_mode = 'FULL'
            tracer.bevel_depth = Btrace.curve_depth

            if Btrace.particle_auto:
                f_start = int(setting.frame_start)
                f_end = int(setting.frame_end + setting.lifetime)
            else:
                if particle_f_end <= particle_f_start:
                    particle_f_end = particle_f_start + 1
                f_start = particle_f_start
                f_end = particle_f_end

            for bFrames in range(f_start, f_end):
                bpy.context.scene.frame_set(bFrames)
                if not (bFrames - f_start) % Btrace.particle_step:
                    for bFrames in range(setting.count):
                        if ps.particles[bFrames].alive_state != 'UNBORN':
                            e = bFrames
                        bp = spline.bezier_points[bFrames]
                        pt = ps.particles[e]
                        bp.co = pt.location
                        bp.handle_right_type = curve_handle
                        bp.handle_left_type = curve_handle
                        bp.keyframe_insert('co')
                        bp.keyframe_insert('handle_left')
                        bp.keyframe_insert('handle_right')
            # Select new curve
            bpy.ops.object.select_all(action='DESELECT')
            curve.select = True
            bpy.context.scene.objects.active = curve

            # Materials
            trace_mats = addtracemat(curve.data)
            if not trace_mats:
                self.report({'WARNING'}, "Some Materials could not be added")

            if Btrace.animate:
                bpy.ops.curve.btgrow()

            return{'FINISHED'}

        except Exception as e:
            error_handlers(self, "particles.connect", e,
                           "Connect Particles could not be completed")

            return {'CANCELLED'}


# Writing Tool
# Writes a curve by animating its point's radii

class OBJECT_OT_writing(Operator):
    bl_idname = "curve.btwriting"
    bl_label = "Write"
    bl_description = ("Use Grease Pencil to write and convert to curves\n"
                      "Needs an existing Grease Pencil layer attached to the Scene")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        gp = context.scene.grease_pencil
        return gp and gp.layers

    def execute(self, context):
        try:
            # first check if the Grease Pencil is attached to the Scene
            tool_settings = context.scene.tool_settings
            source_data = tool_settings.grease_pencil_source
            if source_data in {"OBJECT"}:
                self.report({'WARNING'},
                            "Operation Cancelled. "
                            "The Grease Pencil data-block is attached to an Object")
                return {"CANCELLED"}

            Btrace = context.window_manager.curve_tracer
            # this is hacky - store objects in the scene for comparison later
            store_objects = [ob for ob in context.scene.objects]

            gactive = context.active_object
            # checking if there are any strokes the easy way
            if not bpy.ops.gpencil.convert.poll():
                self.report({'WARNING'},
                            "Operation Cancelled. "
                            "Are there any Grease Pencil Strokes ?")
                return {'CANCELLED'}

            bpy.ops.gpencil.convert(type='CURVE')
            # get curve after convert (compare the scenes to get the difference)
            scene_obj = context.scene.objects
            check_materials = True

            for obj in scene_obj:
                if obj not in store_objects and obj.type == "CURVE":
                    gactiveCurve = obj
                    break

            # render ready curve
            gactiveCurve.data.resolution_u = Btrace.curve_u
            # Set bevel resolution from Panel options
            gactiveCurve.data.bevel_resolution = Btrace.curve_resolution
            gactiveCurve.data.fill_mode = 'FULL'
            # Set bevel depth from Panel options
            gactiveCurve.data.bevel_depth = Btrace.curve_depth

            writeObj = context.selected_objects
            if Btrace.animate:
                for i in writeObj:
                    context.scene.objects.active = i
                    bpy.ops.curve.btgrow()
                    # Materials
                    trace_mats = addtracemat(bpy.context.object.data)
                    if not trace_mats and check_materials is True:
                        check_materials = False
            else:
                for i in writeObj:
                    context.scene.objects.active = i
                    # Materials
                    trace_mats = addtracemat(bpy.context.object.data)
                    if not trace_mats and check_materials is True:
                        check_materials = False

            # Delete grease pencil strokes
            context.scene.objects.active = gactive
            bpy.ops.gpencil.data_unlink()
            context.scene.objects.active = gactiveCurve
            # Smooth object
            bpy.ops.object.shade_smooth()
            # Return to first frame
            bpy.context.scene.frame_set(Btrace.anim_f_start)

            if check_materials is False:
                self.report({'WARNING'}, "Some Materials could not be added")

            return{'FINISHED'}

        except Exception as e:
            error_handlers(self, "curve.btwriting", e,
                           "Grease Pencil conversion could not be completed")

            return {'CANCELLED'}


# Create Curve
# Convert mesh to curve using either Continuous, All Edges, or Sharp Edges
# Option to create noise

class OBJECT_OT_convertcurve(Operator):
    bl_idname = "object.btconvertcurve"
    bl_label = "Btrace: Create Curve"
    bl_description = ("Convert Mesh to Curve using either Continuous, "
                      "All Edges or Sharp Edges\n"
                      "Active Object has to be of a Mesh or Font type")
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.object is not None and
                context.object.type in {"MESH", "FONT"})

    def execute(self, context):
        try:
            Btrace = context.window_manager.curve_tracer
            obj = context.object

            # Convert Font
            if obj.type == 'FONT':
                bpy.ops.object.mode_set(mode='OBJECT')
                bpy.ops.object.convert(target='CURVE')  # Convert edges to curve
                bpy.context.object.data.dimensions = '3D'

            # make a continuous edge through all vertices
            if obj.type == 'MESH':
                # Add noise to mesh
                if Btrace.distort_curve:
                    for v in obj.data.vertices:
                        for u in range(3):
                            v.co[u] += Btrace.distort_noise * (rand_uniform(-1, 1))

                if Btrace.convert_edgetype == 'CONTI':
                    # Start Continuous edge
                    bpy.ops.object.mode_set(mode='EDIT')
                    bpy.ops.mesh.select_all(action='SELECT')
                    bpy.ops.mesh.delete(type='EDGE_FACE')
                    bpy.ops.mesh.select_all(action='DESELECT')
                    verts = bpy.context.object.data.vertices
                    bpy.ops.object.mode_set(mode='OBJECT')
                    li = []
                    p1 = rand_randint(0, len(verts) - 1)

                    for v in verts:
                        li.append(v.index)
                    li.remove(p1)
                    for z in range(len(li)):
                        x = []
                        for px in li:
                            d = verts[p1].co - verts[px].co  # find distance from first vert
                            x.append(d.length)
                        p2 = li[x.index(min(x))]  # find the shortest distance list index
                        verts[p1].select = verts[p2].select = True
                        bpy.ops.object.mode_set(mode='EDIT')
                        bpy.context.tool_settings.mesh_select_mode = [True, False, False]
                        bpy.ops.mesh.edge_face_add()
                        bpy.ops.mesh.select_all(action='DESELECT')
                        bpy.ops.object.mode_set(mode='OBJECT')
                        li.remove(p2)  # remove item from list.
                        p1 = p2
                    # Convert edges to curve
                    bpy.ops.object.mode_set(mode='OBJECT')
                    bpy.ops.object.convert(target='CURVE')

                if Btrace.convert_edgetype == 'EDGEALL':
                    # Start All edges
                    bpy.ops.object.mode_set(mode='EDIT')
                    bpy.ops.mesh.select_all(action='SELECT')
                    bpy.ops.mesh.delete(type='ONLY_FACE')
                    bpy.ops.object.mode_set()
                    bpy.ops.object.convert(target='CURVE')
                    for sp in obj.data.splines:
                        sp.type = Btrace.curve_spline

            obj = context.object
            # Set spline type to custom property in panel
            bpy.ops.object.editmode_toggle()
            bpy.ops.curve.spline_type_set(type=Btrace.curve_spline)
            # Set handle type to custom property in panel
            bpy.ops.curve.handle_type_set(type=Btrace.curve_handle)
            bpy.ops.object.editmode_toggle()
            obj.data.fill_mode = 'FULL'
            # Set resolution to custom property in panel
            obj.data.bevel_resolution = Btrace.curve_resolution
            obj.data.resolution_u = Btrace.curve_u
            # Set depth to custom property in panel
            obj.data.bevel_depth = Btrace.curve_depth
            # Smooth object
            bpy.ops.object.shade_smooth()
            # Modulate curve radius and add distortion
            if Btrace.distort_curve:
                scale = Btrace.distort_modscale
                if scale == 0:
                    return {'FINISHED'}
                for u in obj.data.splines:
                    for v in u.bezier_points:
                        v.radius = scale * round(rand_random(), 3)

            return {'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.btconvertcurve", e,
                           "Conversion could not be completed")

            return {'CANCELLED'}


# Mesh Follow, trace vertex or faces
# Create curve at center of selection item, extruded along animation
# Needs to be an animated mesh!!!

class OBJECT_OT_meshfollow(Operator):
    bl_idname = "object.btmeshfollow"
    bl_label = "Btrace: Vertex Trace"
    bl_description = "Trace Vertex or Face on an animated mesh"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type in {'MESH'})

    def execute(self, context):
        try:
            Btrace = context.window_manager.curve_tracer
            stepsize = Btrace.particle_step

            obj = context.object
            scn = context.scene
            drawmethod = Btrace.fol_mesh_type  # Draw from Edges, Verts, or Faces

            if drawmethod == 'VERTS':
                meshobjs = obj.data.vertices
            if drawmethod == 'FACES':
                meshobjs = obj.data.polygons  # untested
            if drawmethod == 'EDGES':
                meshobjs = obj.data.edges     # untested

            # Frame properties
            start_frame, end_frame = Btrace.fol_start_frame, Btrace.fol_end_frame
            if start_frame > end_frame:      # Make sure the math works
                start_frame = end_frame - 5  # if start past end, goto (end - 5)
            frames = int((end_frame - start_frame) / stepsize)

            def getsel_option():  # Get selection objects
                sel = []
                # options are 'random', 'custom', 'all'
                seloption, fol_mesh_type = Btrace.fol_sel_option, Btrace.fol_mesh_type
                if fol_mesh_type == 'OBJECT':
                    pass
                else:
                    if seloption == 'CUSTOM':
                        for i in meshobjs:
                            if i.select is True:
                                sel.append(i.index)
                    if seloption == 'RANDOM':
                        for i in list(meshobjs):
                            sel.append(i.index)
                        finalsel = int(len(sel) * Btrace.fol_perc_verts)
                        remove = len(sel) - finalsel
                        for i in range(remove):
                            sel.pop(rand_randint(0, len(sel) - 1))
                    if seloption == 'ALL':
                        for i in list(meshobjs):
                            sel.append(i.index)

                return sel

            def get_coord(objindex):
                obj_co = []  # list of vector coordinates to use
                frame_x = start_frame
                for i in range(frames):  # create frame numbers list
                    scn.frame_set(frame_x)
                    if drawmethod != 'OBJECT':
                        followed_item = meshobjs[objindex]
                        if drawmethod == 'VERTS':
                            # find Vert vector
                            g_co = obj.matrix_local * followed_item.co

                        if drawmethod == 'FACES':
                            # find Face vector
                            g_co = obj.matrix_local * followed_item.normal

                        if drawmethod == 'EDGES':
                            v1 = followed_item.vertices[0]
                            v2 = followed_item.vertices[1]
                            co1 = bpy.context.object.data.vertices[v1]
                            co2 = bpy.context.object.data.vertices[v2]
                            localcenter = co1.co.lerp(co2.co, 0.5)
                            g_co = obj.matrix_world * localcenter

                    if drawmethod == 'OBJECT':
                        g_co = objindex.location.copy()

                    obj_co.append(g_co)
                    frame_x = frame_x + stepsize

                scn.frame_set(start_frame)
                return obj_co

            def make_curve(co_list):
                Btrace = bpy.context.window_manager.curve_tracer
                tracer = bpy.data.curves.new('tracer', 'CURVE')
                tracer.dimensions = '3D'
                spline = tracer.splines.new('BEZIER')
                spline.bezier_points.add(len(co_list) - 1)
                curve = bpy.data.objects.new('curve', tracer)
                scn.objects.link(curve)
                curvelist.append(curve)
                # render ready curve
                tracer.resolution_u = Btrace.curve_u
                # Set bevel resolution from Panel options
                tracer.bevel_resolution = Btrace.curve_resolution
                tracer.fill_mode = 'FULL'
                # Set bevel depth from Panel options
                tracer.bevel_depth = Btrace.curve_depth
                curve_handle = Btrace.curve_handle

                # hackish AUTOMATIC doesn't work here
                if curve_handle == 'AUTOMATIC':
                    curve_handle = 'AUTO'

                # move bezier points to objects
                for i in range(len(co_list)):
                    p = spline.bezier_points[i]
                    p.co = co_list[i]
                    p.handle_right_type = curve_handle
                    p.handle_left_type = curve_handle
                return curve

            # Run methods
            # Check if Btrace group exists, if not create
            bgroup = bpy.data.groups.keys()
            if 'Btrace' not in bgroup:
                bpy.ops.group.create(name="Btrace")

            Btrace = bpy.context.window_manager.curve_tracer
            sel = getsel_option()  # Get selection
            curvelist = []  # list to use for grow curve
            check_materials = True

            if Btrace.fol_mesh_type == 'OBJECT':
                vector_list = get_coord(obj)
                curvelist.append(make_curve(vector_list))
            else:
                for i in sel:
                    vector_list = get_coord(i)
                    curvelist.append(make_curve(vector_list))
            # Select new curves and add to group
            bpy.ops.object.select_all(action='DESELECT')
            for curveobject in curvelist:
                if curveobject.type == 'CURVE':
                    curveobject.select = True
                    context.scene.objects.active = curveobject
                    bpy.ops.object.group_link(group="Btrace")
                    # Materials
                    trace_mats = addtracemat(curveobject.data)
                    if not trace_mats and check_materials is True:
                        check_materials = False

                    curveobject.select = False

            if Btrace.animate:  # Add grow curve
                for curveobject in curvelist:
                    curveobject.select = True
                bpy.ops.curve.btgrow()
                for curveobject in curvelist:
                    curveobject.select = False

            obj.select = False  # Deselect original object

            if check_materials is False:
                self.report({'WARNING'}, "Some Materials could not be added")

            return {'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.btmeshfollow", e,
                           "Vertex Trace could not be completed")

            return {'CANCELLED'}


# Add Tracer Material
def addtracemat(matobj):
    try:
        # Check if a material exists, skip if it does
        matslots = bpy.context.object.data.materials.items()

        if len(matslots) < 1:  # Make sure there is only one material slot
            engine = bpy.context.scene.render.engine
            Btrace = bpy.context.window_manager.curve_tracer

            # Check if color blender is to be run
            if not Btrace.mat_run_color_blender:
                # Create Random color for each item
                if Btrace.trace_mat_random:
                    # Use random color from chosen palette,
                    # assign color lists for each palette
                    brightColors = [
                            Btrace.brightColor1, Btrace.brightColor2,
                            Btrace.brightColor3, Btrace.brightColor4
                            ]
                    bwColors = [
                            Btrace.bwColor1, Btrace.bwColor2
                            ]
                    customColors = [
                            Btrace.mmColor1, Btrace.mmColor2, Btrace.mmColor3,
                            Btrace.mmColor4, Btrace.mmColor5, Btrace.mmColor6,
                            Btrace.mmColor7, Btrace.mmColor8
                            ]
                    earthColors = [
                            Btrace.earthColor1, Btrace.earthColor2,
                            Btrace.earthColor3, Btrace.earthColor4,
                            Btrace.earthColor5
                            ]
                    greenblueColors = [
                            Btrace.greenblueColor1, Btrace.greenblueColor2,
                            Btrace.greenblueColor3
                            ]
                    if Btrace.mmColors == 'BRIGHT':
                        mat_color = brightColors[
                                        rand_randint(0, len(brightColors) - 1)
                                        ]
                    if Btrace.mmColors == 'BW':
                        mat_color = bwColors[
                                        rand_randint(0, len(bwColors) - 1)
                                        ]
                    if Btrace.mmColors == 'CUSTOM':
                        mat_color = customColors[
                                        rand_randint(0, len(customColors) - 1)
                                        ]
                    if Btrace.mmColors == 'EARTH':
                        mat_color = earthColors[
                                        rand_randint(0, len(earthColors) - 1)
                                        ]
                    if Btrace.mmColors == 'GREENBLUE':
                        mat_color = greenblueColors[
                                        rand_randint(0, len(greenblueColors) - 1)
                                        ]
                    if Btrace.mmColors == 'RANDOM':
                        mat_color = (rand_random(), rand_random(), rand_random())
                else:
                    # Choose Single color
                    mat_color = Btrace.trace_mat_color

                TraceMat = bpy.data.materials.new('TraceMat')
                # add cycles or BI render material options
                if engine == 'CYCLES':
                    TraceMat.use_nodes = True
                    Diffuse_BSDF = TraceMat.node_tree.nodes['Diffuse BSDF']
                    r, g, b = mat_color[0], mat_color[1], mat_color[2]
                    Diffuse_BSDF.inputs[0].default_value = [r, g, b, 1]
                    TraceMat.diffuse_color = mat_color
                else:
                    TraceMat.diffuse_color = mat_color
                    TraceMat.specular_intensity = 0.5
                # Add material to object
                matobj.materials.append(bpy.data.materials.get(TraceMat.name))

            else:
                # Run color blender operator
                bpy.ops.object.colorblender()

        return True

    except Exception as e:
        error_handlers(False, "addtracemat", e, "Function error", True)

        return False


# Add Color Blender Material
# This is the magical material changer!
class OBJECT_OT_materialChango(Operator):
    bl_idname = "object.colorblender"
    bl_label = "Color Blender"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        try:
            # properties panel
            Btrace = context.window_manager.curve_tracer
            colorObjects = context.selected_objects

            # Set color lists
            brightColors = [
                    Btrace.brightColor1, Btrace.brightColor2,
                    Btrace.brightColor3, Btrace.brightColor4
                    ]
            bwColors = [Btrace.bwColor1, Btrace.bwColor2]
            customColors = [
                    Btrace.mmColor1, Btrace.mmColor2, Btrace.mmColor3,
                    Btrace.mmColor4, Btrace.mmColor5, Btrace.mmColor6,
                    Btrace.mmColor7, Btrace.mmColor8
                    ]
            earthColors = [
                    Btrace.earthColor1, Btrace.earthColor2, Btrace.earthColor3,
                    Btrace.earthColor4, Btrace.earthColor5
                    ]
            greenblueColors = [
                    Btrace.greenblueColor1, Btrace.greenblueColor2,
                    Btrace.greenblueColor3
                    ]
            engine = context.scene.render.engine

            # Go through each selected object and run the operator
            for i in colorObjects:
                theObj = i
                # Check to see if object has materials
                checkMaterials = len(theObj.data.materials)
                if engine == 'CYCLES':
                    materialName = "colorblendMaterial"
                    madMat = bpy.data.materials.new(materialName)
                    madMat.use_nodes = True
                    if checkMaterials == 0:
                        theObj.data.materials.append(madMat)
                    else:
                        theObj.material_slots[0].material = madMat
                else:  # This is internal
                    if checkMaterials == 0:
                        # Add a material
                        materialName = "colorblendMaterial"
                        madMat = bpy.data.materials.new(materialName)
                        theObj.data.materials.append(madMat)
                    else:
                        pass  # pass since we have what we need
                    # assign the first material of the object to "mat"
                    madMat = theObj.data.materials[0]

                # Numbers of frames to skip between keyframes
                skip = Btrace.mmSkip

                # Random material function
                def colorblenderRandom():
                    randomRGB = (rand_random(), rand_random(), rand_random())
                    if engine == 'CYCLES':
                        Diffuse_BSDF = madMat.node_tree.nodes['Diffuse BSDF']
                        mat_color = randomRGB
                        r, g, b = mat_color[0], mat_color[1], mat_color[2]
                        Diffuse_BSDF.inputs[0].default_value = [r, g, b, 1]
                        madMat.diffuse_color = mat_color
                    else:
                        madMat.diffuse_color = randomRGB

                def colorblenderCustom():
                    if engine == 'CYCLES':
                        Diffuse_BSDF = madMat.node_tree.nodes['Diffuse BSDF']
                        mat_color = rand_choice(customColors)
                        r, g, b = mat_color[0], mat_color[1], mat_color[2]
                        Diffuse_BSDF.inputs[0].default_value = [r, g, b, 1]
                        madMat.diffuse_color = mat_color
                    else:
                        madMat.diffuse_color = rand_choice(customColors)

                # Black and white color
                def colorblenderBW():
                    if engine == 'CYCLES':
                        Diffuse_BSDF = madMat.node_tree.nodes['Diffuse BSDF']
                        mat_color = rand_choice(bwColors)
                        r, g, b = mat_color[0], mat_color[1], mat_color[2]
                        Diffuse_BSDF.inputs[0].default_value = [r, g, b, 1]
                        madMat.diffuse_color = mat_color
                    else:
                        madMat.diffuse_color = rand_choice(bwColors)

                # Bright colors
                def colorblenderBright():
                    if engine == 'CYCLES':
                        Diffuse_BSDF = madMat.node_tree.nodes['Diffuse BSDF']
                        mat_color = rand_choice(brightColors)
                        r, g, b = mat_color[0], mat_color[1], mat_color[2]
                        Diffuse_BSDF.inputs[0].default_value = [r, g, b, 1]
                        madMat.diffuse_color = mat_color
                    else:
                        madMat.diffuse_color = rand_choice(brightColors)

                # Earth Tones
                def colorblenderEarth():
                    if engine == 'CYCLES':
                        Diffuse_BSDF = madMat.node_tree.nodes['Diffuse BSDF']
                        mat_color = rand_choice(earthColors)
                        r, g, b = mat_color[0], mat_color[1], mat_color[2]
                        Diffuse_BSDF.inputs[0].default_value = [r, g, b, 1]
                        madMat.diffuse_color = mat_color
                    else:
                        madMat.diffuse_color = rand_choice(earthColors)

                # Green to Blue Tones
                def colorblenderGreenBlue():
                    if engine == 'CYCLES':
                        Diffuse_BSDF = madMat.node_tree.nodes['Diffuse BSDF']
                        mat_color = rand_choice(greenblueColors)
                        r, g, b = mat_color[0], mat_color[1], mat_color[2]
                        Diffuse_BSDF.inputs[0].default_value = [r, g, b, 1]
                        madMat.diffuse_color = mat_color
                    else:
                        madMat.diffuse_color = rand_choice(greenblueColors)

                # define frame start/end variables
                scn = context.scene
                start = scn.frame_start
                end = scn.frame_end

                # Go to each frame in iteration and add material
                while start <= (end + (skip - 1)):
                    bpy.context.scene.frame_set(frame=start)

                    # Check what colors setting is checked and run the appropriate function
                    if Btrace.mmColors == 'RANDOM':
                        colorblenderRandom()
                    elif Btrace.mmColors == 'CUSTOM':
                        colorblenderCustom()
                    elif Btrace.mmColors == 'BW':
                        colorblenderBW()
                    elif Btrace.mmColors == 'BRIGHT':
                        colorblenderBright()
                    elif Btrace.mmColors == 'EARTH':
                        colorblenderEarth()
                    elif Btrace.mmColors == 'GREENBLUE':
                        colorblenderGreenBlue()
                    else:
                        pass

                    # Add keyframe to material
                    if engine == 'CYCLES':
                        madMat.node_tree.nodes[
                                'Diffuse BSDF'].inputs[0].keyframe_insert('default_value')
                        # not sure if this is need, it's viewport color only
                        madMat.keyframe_insert('diffuse_color')
                    else:
                        madMat.keyframe_insert('diffuse_color')

                    # Increase frame number
                    start += skip

            return{'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.colorblender", e,
                           "Color Blender could not be completed")

            return {'CANCELLED'}


# This clears the keyframes
class OBJECT_OT_clearColorblender(Operator):
    bl_idname = "object.colorblenderclear"
    bl_label = "Clear colorblendness"
    bl_description = "Clear the color keyframes"
    bl_options = {'REGISTER', 'UNDO'}

    def invoke(self, context, event):
        try:
            colorObjects = context.selected_objects
            engine = context.scene.render.engine

            # Go through each selected object and run the operator
            for i in colorObjects:
                theObj = i
                # assign the first material of the object to "mat"
                matCl = theObj.data.materials[0]

                # define frame start/end variables
                scn = context.scene
                start = scn.frame_start
                end = scn.frame_end

                # Remove all keyframes from diffuse_color, super sloppy
                while start <= (end + 100):
                    context.scene.frame_set(frame=start)
                    try:
                        if engine == 'CYCLES':
                            matCl.node_tree.nodes[
                                'Diffuse BSDF'].inputs[0].keyframe_delete('default_value')
                        elif engine == 'BLENDER_RENDER':
                            matCl.keyframe_delete('diffuse_color')
                    except:
                        pass
                    start += 1

            return{'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.colorblenderclear", e,
                           "Reset Keyframes could not be completed")

            return {'CANCELLED'}


# F-Curve Noise
# will add noise modifiers to each selected object f-curves
# change type to: 'rotation' | 'location' | 'scale' | '' to effect all
# first record a keyframe for this to work (to generate the f-curves)

class OBJECT_OT_fcnoise(Operator):
    bl_idname = "object.btfcnoise"
    bl_label = "Btrace: F-curve Noise"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        try:
            Btrace = context.window_manager.curve_tracer
            amp = Btrace.fcnoise_amp
            timescale = Btrace.fcnoise_timescale
            addkeyframe = Btrace.fcnoise_key

            # This sets properties for Loc, Rot and Scale
            # if they're checked in the Tools window
            noise_rot = 'rotation'
            noise_loc = 'location'
            noise_scale = 'scale'
            if not Btrace.fcnoise_rot:
                noise_rot = 'none'
            if not Btrace.fcnoise_loc:
                noise_loc = 'none'
            if not Btrace.fcnoise_scale:
                noise_scale = 'none'

            # Add settings from panel for type of keyframes
            types = noise_loc, noise_rot, noise_scale
            amplitude = amp
            time_scale = timescale

            for i in context.selected_objects:
                # Add keyframes, this is messy and should only
                # add keyframes for what is checked
                if addkeyframe is True:
                    bpy.ops.anim.keyframe_insert(type="LocRotScale")
                for obj in context.selected_objects:
                    if obj.animation_data:
                        for c in obj.animation_data.action.fcurves:
                            if c.data_path.startswith(types):
                                # clean modifiers
                                for m in c.modifiers:
                                    c.modifiers.remove(m)
                                # add noide modifiers
                                n = c.modifiers.new('NOISE')
                                n.strength = amplitude
                                n.scale = time_scale
                                n.phase = rand_randint(0, 999)

            return {'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.btfcnoise", e,
                           "F-curve Noise could not be completed")

            return {'CANCELLED'}


# Curve Grow Animation
# Animate curve radius over length of time

class OBJECT_OT_curvegrow(Operator):
    bl_idname = "curve.btgrow"
    bl_label = "Run Script"
    bl_description = "Keyframe points radius"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return (context.object and context.object.type in {'CURVE'})

    def execute(self, context):
        try:
            # not so nice with the nested try blocks, however the inside one
            # is used as a switch statement
            Btrace = context.window_manager.curve_tracer
            anim_f_start, anim_length, anim_auto = Btrace.anim_f_start, \
                                                   Btrace.anim_length, \
                                                   Btrace.anim_auto
            curve_resolution, curve_depth = Btrace.curve_resolution, \
                                            Btrace.curve_depth
            # make the curve visible
            objs = context.selected_objects
            # Execute on multiple selected objects
            for i in objs:
                context.scene.objects.active = i
                obj = context.active_object
                try:
                    obj.data.fill_mode = 'FULL'
                except:
                    obj.data.dimensions = '3D'
                    obj.data.fill_mode = 'FULL'
                if not obj.data.bevel_resolution:
                    obj.data.bevel_resolution = curve_resolution
                if not obj.data.bevel_depth:
                    obj.data.bevel_depth = curve_depth
                if anim_auto:
                    anim_f_start = bpy.context.scene.frame_start
                    anim_length = bpy.context.scene.frame_end
                # get points data and beautify
                actual, total = anim_f_start, 0
                for sp in obj.data.splines:
                    total += len(sp.points) + len(sp.bezier_points)
                step = anim_length / total
                for sp in obj.data.splines:
                    sp.radius_interpolation = 'BSPLINE'
                    po = [p for p in sp.points] + [p for p in sp.bezier_points]
                    if not Btrace.anim_keepr:
                        for p in po:
                            p.radius = 1
                    if Btrace.anim_tails and not sp.use_cyclic_u:
                        po[0].radius = po[-1].radius = 0
                        po[1].radius = po[-2].radius = .65
                    ra = [p.radius for p in po]

                    # record the keyframes
                    for i in range(len(po)):
                        po[i].radius = 0
                        po[i].keyframe_insert('radius', frame=actual)
                        actual += step
                        po[i].radius = ra[i]
                        po[i].keyframe_insert(
                                    'radius',
                                    frame=(actual + Btrace.anim_delay)
                                    )

                        if Btrace.anim_f_fade:
                            po[i].radius = ra[i]
                            po[i].keyframe_insert(
                                    'radius',
                                    frame=(actual + Btrace.anim_f_fade - step)
                                    )
                            po[i].radius = 0
                            po[i].keyframe_insert(
                                    'radius',
                                    frame=(actual + Btrace.anim_delay + Btrace.anim_f_fade)
                                    )

                bpy.context.scene.frame_set(Btrace.anim_f_start)

            return{'FINISHED'}

        except Exception as e:
            error_handlers(self, "curve.btgrow", e,
                           "Grow curve could not be completed")

            return {'CANCELLED'}


# Remove animation and curve radius data
class OBJECT_OT_reset(Operator):
    bl_idname = "object.btreset"
    bl_label = "Clear animation"
    bl_description = "Remove animation / curve radius data"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        try:
            objs = context.selected_objects
            for i in objs:  # Execute on multiple selected objects
                context.scene.objects.active = i
                obj = context.active_object
                obj.animation_data_clear()
                if obj.type == 'CURVE':
                    for sp in obj.data.splines:
                        po = [p for p in sp.points] + [p for p in sp.bezier_points]
                        for p in po:
                            p.radius = 1

            return{'FINISHED'}

        except Exception as e:
            error_handlers(self, "object.btreset", e,
                           "Clear animation could not be completed")

            return {'CANCELLED'}
