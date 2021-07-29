import bpy
from ...utils import copy_bone
from ...utils import strip_org, make_deformer_name, connected_children_names
from ...utils import make_mechanism_name, put_bone, create_sphere_widget
from ...utils import create_widget, create_circle_widget
from ...utils import MetarigError
from rna_prop_ui import rna_idprop_ui_prop_get


class Rig:

    def __init__(self, obj, bone_name, params):
        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params

        self.copy_rotation_axes = params.copy_rotation_axes

        if params.tweak_extra_layers:
            self.tweak_layers = list(params.tweak_layers)
        else:
            self.tweak_layers = None

        if len(self.org_bones) <= 1:
            raise MetarigError(
                "RIGIFY ERROR: invalid rig structure on bone: %s" % (strip_org(bone_name))
            )

    def make_controls(self):

        bpy.ops.object.mode_set(mode ='EDIT')
        org_bones = self.org_bones

        ctrl_chain = []
        for i in range( len( org_bones ) ):
            name = org_bones[i]

            ctrl_bone  = copy_bone(
                self.obj,
                name,
                strip_org(name)
            )

            ctrl_chain.append( ctrl_bone )

        # Make widgets
        bpy.ops.object.mode_set(mode ='OBJECT')

        for ctrl in ctrl_chain:
            create_circle_widget(self.obj, ctrl, radius=0.3, head_tail=0.5)

        return ctrl_chain

    def make_tweaks(self):

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones
        org_bones = self.org_bones

        tweak_chain = []
        for i in range( len( org_bones ) + 1 ):
            if i == len( org_bones ):
                # Make final tweak at the tip of the tentacle
                name = org_bones[i-1]
            else:
                name = org_bones[i]

            tweak_bone = copy_bone(
                self.obj,
                name,
                "tweak_" + strip_org(name)
            )

            tweak_e = eb[ tweak_bone ]

            tweak_e.length /= 2 # Set size to half

            if i == len( org_bones ):
                # Position final tweak at the tip
                put_bone( self.obj, tweak_bone, eb[ org_bones[-1]].tail )

            tweak_chain.append( tweak_bone )

        # Make widgets
        bpy.ops.object.mode_set(mode = 'OBJECT')

        for tweak in tweak_chain:
            create_sphere_widget( self.obj, tweak )

            tweak_pb = self.obj.pose.bones[ tweak ]

            # Set locks
            if tweak_chain.index( tweak ) != len( tweak_chain ) - 1:
                tweak_pb.lock_rotation = (True, False, True)
                tweak_pb.lock_scale    = (False, True, False)
            else:
                tweak_pb.lock_rotation_w = True
                tweak_pb.lock_rotation   = (True, True, True)
                tweak_pb.lock_scale      = (True, True, True)

            # Set up tweak bone layers
            if self.tweak_layers:
                tweak_pb.bone.layers = self.tweak_layers

        return tweak_chain

    def make_deform(self):

        bpy.ops.object.mode_set(mode ='EDIT')
        org_bones = self.org_bones

        def_chain = []
        for i in range( len( org_bones ) ):
            name = org_bones[i]

            def_bone  = copy_bone(
                self.obj,
                name,
                make_deformer_name(strip_org(name))
            )

            def_chain.append( def_bone )

        return def_chain

    def parent_bones(self, all_bones):

        bpy.ops.object.mode_set(mode ='EDIT')
        org_bones = self.org_bones
        eb        = self.obj.data.edit_bones

        # Parent control bones
        for bone in all_bones['control'][1:]:
            previous_index    = all_bones['control'].index( bone ) - 1
            eb[ bone ].parent = eb[ all_bones['control'][previous_index] ]

        # Parent tweak bones
        tweaks = all_bones['tweak']
        for tweak in all_bones['tweak']:
            parent = ''
            if tweaks.index( tweak ) == len( tweaks ) - 1:
                parent = all_bones['control'][ -1 ]
            else:
                parent = all_bones['control'][ tweaks.index( tweak ) ]

            eb[ tweak ].parent = eb[ parent ]

        # Parent deform bones
        for bone in all_bones['deform'][1:]:
            previous_index = all_bones['deform'].index( bone ) - 1

            eb[ bone ].parent = eb[ all_bones['deform'][previous_index] ]
            eb[ bone ].use_connect = True

        # Parent org bones ( to tweaks by default, or to the controls )
        for org, tweak in zip( org_bones, all_bones['tweak'] ):
            eb[ org ].parent = eb[ tweak ]

    def make_constraints(self, all_bones):

        bpy.ops.object.mode_set(mode ='OBJECT')
        org_bones = self.org_bones
        pb        = self.obj.pose.bones

        # Deform bones' constraints
        ctrls   = all_bones['control']
        tweaks  = all_bones['tweak'  ]
        deforms = all_bones['deform' ]

        for deform, tweak, ctrl in zip( deforms, tweaks, ctrls ):
            con           = pb[deform].constraints.new('COPY_TRANSFORMS')
            con.target    = self.obj
            con.subtarget = tweak

            con           = pb[deform].constraints.new('DAMPED_TRACK')
            con.target    = self.obj
            con.subtarget = tweaks[ tweaks.index( tweak ) + 1 ]

            con           = pb[deform].constraints.new('STRETCH_TO')
            con.target    = self.obj
            con.subtarget = tweaks[ tweaks.index( tweak ) + 1 ]

            # Control bones' constraints
            if ctrl != ctrls[0]:
                con = pb[ctrl].constraints.new('COPY_ROTATION')
                con.target = self.obj
                con.subtarget = ctrls[ctrls.index(ctrl) - 1]
                for i, prop in enumerate(['use_x', 'use_y', 'use_z']):
                    if self.copy_rotation_axes[i]:
                        setattr(con, prop, True)
                    else:
                        setattr(con, prop, False)
                con.use_offset = True
                con.target_space = 'LOCAL'
                con.owner_space = 'LOCAL'

    def generate(self):
        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # Clear all initial parenting
        for bone in self.org_bones:
        #    eb[ bone ].parent      = None
            eb[ bone ].use_connect = False

        # Creating all bones
        ctrl_chain  = self.make_controls()
        tweak_chain = self.make_tweaks()
        def_chain   = self.make_deform()

        all_bones = {
            'control' : ctrl_chain,
            'tweak'   : tweak_chain,
            'deform'  : def_chain
        }

        self.make_constraints(all_bones)
        self.parent_bones(all_bones)


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """
    params.copy_rotation_axes = bpy.props.BoolVectorProperty(
        size=3,
        description="Automation axes",
        default=tuple([i == 0 for i in range(0, 3)])
        )

    # Setting up extra tweak layers
    params.tweak_extra_layers = bpy.props.BoolProperty(
        name        = "tweak_extra_layers",
        default     = True,
        description = ""
        )

    params.tweak_layers = bpy.props.BoolVectorProperty(
        size        = 32,
        description = "Layers for the tweak controls to be on",
        default     = tuple( [ i == 1 for i in range(0, 32) ] )
        )


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters.
    """

    r = layout.row()
    col = r.column(align=True)
    row = col.row(align=True)
    for i, axis in enumerate(['x', 'y', 'z']):
        row.prop(params, "copy_rotation_axes", index=i, toggle=True, text=axis)

    r = layout.row()
    r.prop(params, "tweak_extra_layers")
    r.active = params.tweak_extra_layers

    col = r.column(align=True)
    row = col.row(align=True)

    bone_layers = bpy.context.active_pose_bone.bone.layers[:]

    for i in range(8):    # Layers 0-7
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    row = col.row(align=True)

    for i in range(16, 24):     # Layers 16-23
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)
    
    col = r.column(align=True)
    row = col.row(align=True)

    for i in range(8, 16):  # Layers 8-15
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    row = col.row(align=True)

    for i in range( 24, 32 ): # Layers 24-31
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('Bone')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.0000, 0.3333
    bone.roll = 0.0000
    bone.use_connect = False
    bones['Bone'] = bone.name

    bone = arm.edit_bones.new('Bone.002')
    bone.head[:] = 0.0000, 0.0000, 0.3333
    bone.tail[:] = 0.0000, 0.0000, 0.6667
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['Bone']]
    bones['Bone.002'] = bone.name

    bone = arm.edit_bones.new('Bone.001')
    bone.head[:] = 0.0000, 0.0000, 0.6667
    bone.tail[:] = 0.0000, 0.0000, 1.0000
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['Bone.002']]
    bones['Bone.001'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['Bone']]
    pbone.rigify_type = 'limbs.simple_tentacle'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['Bone.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['Bone.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'

    bpy.ops.object.mode_set(mode='EDIT')
    for bone in arm.edit_bones:
        bone.select = False
        bone.select_head = False
        bone.select_tail = False
    for b in bones:
        bone = arm.edit_bones[bones[b]]
        bone.select = True
        bone.select_head = True
        bone.select_tail = True
        arm.edit_bones.active = bone
