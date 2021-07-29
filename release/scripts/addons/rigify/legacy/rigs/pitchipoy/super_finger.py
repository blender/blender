import bpy
from mathutils import Vector
from ...utils import copy_bone, flip_bone
from ...utils import strip_org, make_deformer_name, connected_children_names, make_mechanism_name
from ...utils import create_circle_widget, create_sphere_widget, create_widget
from ...utils import MetarigError
from rna_prop_ui import rna_idprop_ui_prop_get

script = """
controls    = [%s]
master_name = '%s'
if is_selected(controls):
    layout.prop(pose_bones[master_name], '["%s"]', text="Curvature", slider=True)
"""

class Rig:

    def __init__(self, obj, bone_name, params):
        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params

        if len(self.org_bones) <= 1:
            raise MetarigError("RIGIFY ERROR: Bone '%s': listen bro, that finger rig jusaint put tugetha rite. A little hint, use more than one bone!!" % (strip_org(bone_name)))


    def generate(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # Bone name lists
        ctrl_chain    = []
        def_chain     = []
        mch_chain     = []
        mch_drv_chain = []

        # Create ctrl master bone
        org_name  = self.org_bones[0]
        temp_name = strip_org(self.org_bones[0])

        suffix = temp_name[-2:]
        master_name      = temp_name[:-5] + "_master" + suffix
        master_name      = copy_bone( self.obj, org_name, master_name )
        ctrl_bone_master = eb[ master_name ]

        ## Parenting bug fix ??
        ctrl_bone_master.use_connect = False
        ctrl_bone_master.parent      = None

        ctrl_bone_master.tail += ( eb[ org_bones[-1] ].tail - eb[org_name].head ) * 1.25

        for bone in org_bones:
            eb[bone].use_connect = False
            if org_bones.index( bone ) != 0:
               eb[bone].parent      = None

        # Creating the bone chains
        for i in range(len(self.org_bones)):

            name      = self.org_bones[i]
            ctrl_name = strip_org(name)

            # Create control bones
            ctrl_bone   = copy_bone( self.obj, name, ctrl_name )
            ctrl_bone_e = eb[ ctrl_name ]

            # Create deformation bones
            def_name  = make_deformer_name( ctrl_name )
            def_bone  = copy_bone( self.obj, name, def_name )

            # Create mechanism bones
            mch_name  = make_mechanism_name( ctrl_name )
            mch_bone  = copy_bone( self.obj, name, mch_name )

            # Create mechanism driver bones
            drv_name  = make_mechanism_name(ctrl_name) + "_drv"
            mch_bone_drv    = copy_bone(self.obj, name, drv_name)
            mch_bone_drv_e  = eb[drv_name]

            # Adding to lists
            ctrl_chain    += [ctrl_name]
            def_chain     += [def_bone]
            mch_chain     += [mch_bone]
            mch_drv_chain += [drv_name]

        # Restoring org chain parenting
        for bone in org_bones[1:]:
            eb[bone].parent = eb[ org_bones[ org_bones.index(bone) - 1 ] ]

        # Parenting the master bone to the first org
        ctrl_bone_master = eb[ master_name ]
        ctrl_bone_master.parent = eb[ org_bones[0] ]

        # Parenting chain bones
        for i in range(len(self.org_bones)):
            # Edit bone references
            def_bone_e     = eb[def_chain[i]]
            ctrl_bone_e    = eb[ctrl_chain[i]]
            mch_bone_e     = eb[mch_chain[i]]
            mch_bone_drv_e = eb[mch_drv_chain[i]]

            if i == 0:
                # First ctl bone
                ctrl_bone_e.parent      = mch_bone_drv_e
                ctrl_bone_e.use_connect = False
                # First def bone
                def_bone_e.parent       = eb[self.org_bones[i]].parent
                def_bone_e.use_connect  = False
                # First mch bone
                mch_bone_e.parent = eb[self.org_bones[i]].parent
                mch_bone_e.use_connect  = False
                # First mch driver bone
                mch_bone_drv_e.parent = eb[self.org_bones[i]].parent
                mch_bone_drv_e.use_connect  = False
            else:
                # The rest
                ctrl_bone_e.parent         = mch_bone_drv_e
                ctrl_bone_e.use_connect    = False

                def_bone_e.parent          = eb[def_chain[i-1]]
                def_bone_e.use_connect     = True

                mch_bone_drv_e.parent      = eb[ctrl_chain[i-1]]
                mch_bone_drv_e.use_connect = False

                # Parenting mch bone
                mch_bone_e.parent      = ctrl_bone_e
                mch_bone_e.use_connect = False

        # Creating tip conrtol bone
        tip_name      = copy_bone( self.obj, org_bones[-1], temp_name )
        ctrl_bone_tip = eb[ tip_name ]
        flip_bone( self.obj, tip_name )
        ctrl_bone_tip.length /= 2

        ctrl_bone_tip.parent = eb[ctrl_chain[-1]]

        bpy.ops.object.mode_set(mode ='OBJECT')

        pb = self.obj.pose.bones

        # Setting pose bones locks
        pb_master = pb[master_name]
        pb_master.lock_scale    = True,False,True

        pb[tip_name].lock_scale      = True,True,True
        pb[tip_name].lock_rotation   = True,True,True
        pb[tip_name].lock_rotation_w = True

        pb_master['finger_curve'] = 0.0
        prop = rna_idprop_ui_prop_get(pb_master, 'finger_curve')
        prop["min"] = 0.0
        prop["max"] = 1.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0
        prop["description"] = "Rubber hose finger cartoon effect"

        # Pose settings
        for org, ctrl, deform, mch, mch_drv in zip(self.org_bones, ctrl_chain, def_chain, mch_chain, mch_drv_chain):

            # Constraining the org bones
            #con           = pb[org].constraints.new('COPY_TRANSFORMS')
            #con.target    = self.obj
            #con.subtarget = ctrl

            # Constraining the deform bones
            con           = pb[deform].constraints.new('COPY_TRANSFORMS')
            con.target    = self.obj
            con.subtarget = mch

            # Constraining the mch bones
            if mch_chain.index(mch) == 0:
                con           = pb[mch].constraints.new('COPY_LOCATION')
                con.target    = self.obj
                con.subtarget = ctrl

                con           = pb[mch].constraints.new('COPY_SCALE')
                con.target    = self.obj
                con.subtarget = ctrl

                con           = pb[mch].constraints.new('DAMPED_TRACK')
                con.target    = self.obj
                con.subtarget = ctrl_chain[ctrl_chain.index(ctrl)+1]

                con           = pb[mch].constraints.new('STRETCH_TO')
                con.target    = self.obj
                con.subtarget = ctrl_chain[ctrl_chain.index(ctrl)+1]
                con.volume    = 'NO_VOLUME'

            elif mch_chain.index(mch) == len(mch_chain) - 1:
                con           = pb[mch].constraints.new('DAMPED_TRACK')
                con.target    = self.obj
                con.subtarget = tip_name

                con           = pb[mch].constraints.new('STRETCH_TO')
                con.target    = self.obj
                con.subtarget = tip_name
                con.volume    = 'NO_VOLUME'
            else:
                con           = pb[mch].constraints.new('DAMPED_TRACK')
                con.target    = self.obj
                con.subtarget = ctrl_chain[ctrl_chain.index(ctrl)+1]

                con           = pb[mch].constraints.new('STRETCH_TO')
                con.target    = self.obj
                con.subtarget = ctrl_chain[ctrl_chain.index(ctrl)+1]
                con.volume    = 'NO_VOLUME'

            # Constraining and driving mch driver bones
            pb[mch_drv].rotation_mode = 'YZX'

            if mch_drv_chain.index(mch_drv) == 0:
                # Constraining to master bone
                con              = pb[mch_drv].constraints.new('COPY_LOCATION')
                con.target       = self.obj
                con.subtarget    = master_name

                con              = pb[mch_drv].constraints.new('COPY_ROTATION')
                con.target       = self.obj
                con.subtarget    = master_name
                con.target_space = 'LOCAL'
                con.owner_space  = 'LOCAL'

            else:
                # Match axis to expression
                options = {
                    "X"  : { "axis" : 0,
                             "expr" : '(1-sy)*pi' },
                    "-X" : { "axis" : 0,
                             "expr" : '-((1-sy)*pi)' },
                    "Y"  : { "axis" : 1,
                             "expr" : '(1-sy)*pi' },
                    "-Y" : { "axis" : 1,
                             "expr" : '-((1-sy)*pi)' },
                    "Z"  : { "axis" : 2,
                             "expr" : '(1-sy)*pi' },
                    "-Z" : { "axis" : 2,
                             "expr" : '-((1-sy)*pi)' }
                }

                axis = self.params.primary_rotation_axis

                # Drivers
                drv                          = pb[mch_drv].driver_add("rotation_euler", options[axis]["axis"]).driver
                drv.type                     = 'SCRIPTED'
                drv.expression               = options[axis]["expr"]
                drv_var                      = drv.variables.new()
                drv_var.name                 = 'sy'
                drv_var.type                 = "SINGLE_PROP"
                drv_var.targets[0].id        = self.obj
                drv_var.targets[0].data_path = pb[master_name].path_from_id() + '.scale.y'

            # Setting bone curvature setting, costum property, and drivers
            def_bone = self.obj.data.bones[deform]

            def_bone.bbone_segments = 8
            drv = def_bone.driver_add("bbone_in").driver # Ease in

            drv.type='SUM'
            drv_var = drv.variables.new()
            drv_var.name = "curvature"
            drv_var.type = "SINGLE_PROP"
            drv_var.targets[0].id = self.obj
            drv_var.targets[0].data_path = pb_master.path_from_id() + '["finger_curve"]'

            drv = def_bone.driver_add("bbone_out").driver # Ease out

            drv.type='SUM'
            drv_var = drv.variables.new()
            drv_var.name = "curvature"
            drv_var.type = "SINGLE_PROP"
            drv_var.targets[0].id = self.obj
            drv_var.targets[0].data_path = pb_master.path_from_id() + '["finger_curve"]'


            # Assigning shapes to control bones
            create_circle_widget(self.obj, ctrl, radius=0.3, head_tail=0.5)

        # Create ctrl master widget
        w = create_widget(self.obj, master_name)
        if w is not None:
            mesh = w.data
            verts = [(0, 0, 0), (0, 1, 0), (0.05, 1, 0), (0.05, 1.1, 0), (-0.05, 1.1, 0), (-0.05, 1, 0)]
            if 'Z' in self.params.primary_rotation_axis:
                # Flip x/z coordinates
                temp = []
                for v in verts:
                    temp += [(v[2], v[1], v[0])]
                verts = temp
            edges = [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 1)]
            mesh.from_pydata(verts, edges, [])
            mesh.update()

        # Create tip control widget
        create_circle_widget(self.obj, tip_name, radius=0.3, head_tail=0.0)

        # Create UI
        controls_string = ", ".join(
            ["'" + x + "'" for x in ctrl_chain]
            ) + ", " + "'" + master_name + "'"
        return [script % (controls_string, master_name, 'finger_curve')]


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """
    items = [('X', 'X', ''), ('Y', 'Y', ''), ('Z', 'Z', ''), ('-X', '-X', ''), ('-Y', '-Y', ''), ('-Z', '-Z', '')]
    params.primary_rotation_axis = bpy.props.EnumProperty(items=items, name="Primary Rotation Axis", default='X')


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters.
    """
    r = layout.row()
    r.label(text="Bend rotation axis:")
    r.prop(params, "primary_rotation_axis", text="")


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('palm.04.L')
    bone.head[:] = 0.0043, -0.0030, -0.0026
    bone.tail[:] = 0.0642, 0.0037, -0.0469
    bone.roll = -2.5155
    bone.use_connect = False
    bones['palm.04.L'] = bone.name
    bone = arm.edit_bones.new('f_pinky.01.L')
    bone.head[:] = 0.0642, 0.0037, -0.0469
    bone.tail[:] = 0.0703, 0.0039, -0.0741
    bone.roll = -1.9749
    bone.use_connect = False
    bone.parent = arm.edit_bones[bones['palm.04.L']]
    bones['f_pinky.01.L'] = bone.name
    bone = arm.edit_bones.new('f_pinky.02.L')
    bone.head[:] = 0.0703, 0.0039, -0.0741
    bone.tail[:] = 0.0732, 0.0044, -0.0965
    bone.roll = -1.9059
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['f_pinky.01.L']]
    bones['f_pinky.02.L'] = bone.name
    bone = arm.edit_bones.new('f_pinky.03.L')
    bone.head[:] = 0.0732, 0.0044, -0.0965
    bone.tail[:] = 0.0725, 0.0046, -0.1115
    bone.roll = -1.7639
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['f_pinky.02.L']]
    bones['f_pinky.03.L'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['palm.04.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'YXZ'
    pbone = obj.pose.bones[bones['f_pinky.01.L']]
    pbone.rigify_type = 'pitchipoy.simple_tentacle'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.separate_extra_layers = True
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.extra_layers = [False, False, False, False, False, True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.tweak_extra_layers = False
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['f_pinky.02.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['f_pinky.03.L']]
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





