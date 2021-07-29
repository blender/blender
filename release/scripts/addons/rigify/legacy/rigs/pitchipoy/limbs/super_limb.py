import bpy, re
from   .arm            import create_arm
from   .leg            import create_leg
from   .paw            import create_paw
from   .ui             import create_script
from   .limb_utils     import *
from   mathutils       import Vector
from   ....utils       import copy_bone, flip_bone, put_bone, create_cube_widget
from   ....utils       import strip_org, make_deformer_name, create_widget
from   ....utils       import create_circle_widget, create_sphere_widget
from   ....utils       import MetarigError, make_mechanism_name, org
from   ....utils       import create_limb_widget, connected_children_names
from   rna_prop_ui     import rna_idprop_ui_prop_get
from   ..super_widgets import create_ikarrow_widget
from   math            import trunc


class Rig:

    def __init__(self, obj, bone_name, params):
        """ Initialize super_limb rig and key rig properties """
        self.obj = obj
        self.params = params

        if params.limb_type != 'paw':
            self.org_bones = list(
                [bone_name] + connected_children_names(obj, bone_name)
                )[:3]  # The basic limb is the first 3 bones
        else:
            self.org_bones = list(
                [bone_name] + connected_children_names(obj, bone_name)
                )[:4]  # The basic limb is the first 4 bones for a paw

        self.segments = params.segments
        self.bbones = params.bbones
        self.limb_type = params.limb_type
        self.rot_axis = params.rotation_axis

        # Assign values to tweak/FK layers props if opted by user
        if params.tweak_extra_layers:
            self.tweak_layers = list(params.tweak_layers)
        else:
            self.tweak_layers = None

        if params.fk_extra_layers:
            self.fk_layers = list(params.fk_layers)
        else:
            self.fk_layers = None

    def create_parent(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        name = get_bone_name( strip_org( org_bones[0] ), 'mch', 'parent' )

        mch = copy_bone( self.obj, org_bones[0], name )
        orient_bone( self, eb[mch], 'y' )
        eb[ mch ].length = eb[ org_bones[0] ].length / 4

        eb[ mch ].parent = eb[ org_bones[0] ].parent

        eb[ mch ].roll = 0.0

        # Constraints
        make_constraint( self, mch, {
            'constraint'  : 'COPY_ROTATION',
            'subtarget'   : 'root'
        })

        make_constraint( self, mch, {
            'constraint'  : 'COPY_SCALE',
            'subtarget'   : 'root'
        })

        # Limb Follow Driver
        pb = self.obj.pose.bones

        name = 'FK_limb_follow'

        pb[ mch ][ name ] = 0.0
        prop = rna_idprop_ui_prop_get( pb[ mch ], name, create = True )

        prop["min"]         = 0.0
        prop["max"]         = 1.0
        prop["soft_min"]    = 0.0
        prop["soft_max"]    = 1.0
        prop["description"] = name

        drv = pb[ mch ].constraints[ 0 ].driver_add("influence").driver

        drv.type = 'AVERAGE'
        var = drv.variables.new()
        var.name = name
        var.type = "SINGLE_PROP"
        var.targets[0].id = self.obj
        var.targets[0].data_path = pb[ mch ].path_from_id() + \
                                   '[' + '"' + name + '"' + ']'

        return mch

    def create_tweak( self ):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        tweaks         = {}
        tweaks['ctrl'] = []
        tweaks['mch' ] = []

        # Create and parent mch and ctrl tweaks
        for i,org in enumerate(org_bones):
            if i < len(org_bones) - 1:
                # Create segments if specified
                for j in range( self.segments ):
                    # MCH
                    name = get_bone_name( strip_org(org), 'mch', 'tweak' )
                    mch = copy_bone( self.obj, org, name )

                    # CTRL
                    name = get_bone_name( strip_org(org), 'ctrl', 'tweak' )
                    ctrl = copy_bone( self.obj, org, name )

                    eb[ mch  ].length /= self.segments
                    eb[ ctrl ].length /= self.segments

                    # If we have more than one segments, place the head of the
                    # 2nd and onwards at the correct position
                    if j > 0:
                        put_bone(self.obj, mch,  eb[ tweaks['mch' ][-1] ].tail)
                        put_bone(self.obj, ctrl, eb[ tweaks['ctrl'][-1] ].tail)

                    tweaks['ctrl'] += [ ctrl ]
                    tweaks['mch' ] += [ mch  ]

                    # Parenting the tweak ctrls to mchs
                    eb[ mch  ].parent = eb[ org ]
                    eb[ ctrl ].parent = eb[ mch ]

            else: # Last limb bone - is not subdivided
                name = get_bone_name( strip_org(org), 'mch', 'tweak' )
                mch = copy_bone( self.obj, org_bones[i-1], name )
                eb[ mch ].length = eb[org].length / 4
                put_bone(
                    self.obj,
                    mch,
                    eb[org_bones[i-1]].tail
                )

                ctrl = get_bone_name( strip_org(org), 'ctrl', 'tweak' )
                ctrl = copy_bone( self.obj, org, ctrl )
                eb[ ctrl ].length = eb[org].length / 2

                tweaks['mch']  += [ mch  ]
                tweaks['ctrl'] += [ ctrl ]

                # Parenting the tweak ctrls to mchs
                eb[ mch  ].parent = eb[ org ]
                eb[ ctrl ].parent = eb[ mch ]

        # Scale to reduce widget size and maintain conventions!
        for mch, ctrl in zip( tweaks['mch'], tweaks['ctrl'] ):
            eb[ mch  ].length /= 4
            eb[ ctrl ].length /= 2

        # Contraints
        if self.limb_type == 'paw':

            for i,b in enumerate( tweaks['mch'] ):
                first  = 0
                middle = trunc( len( tweaks['mch'] ) / 3 )
                middle1 = middle + self.segments
                last   =  len( tweaks['mch'] ) - 1

                if i == first or i == middle or i == middle1:
                    make_constraint( self, b, {
                        'constraint'  : 'COPY_SCALE',
                        'subtarget'   : 'root'
                    })
                elif i != last:
                    targets       = []
                    factor        = 0
                    if i < middle:
                        dt_target_idx = middle
                        targets = [first,middle]
                    elif i > middle and i < middle1:
                        targets = [middle,middle1]
                        factor = self.segments
                        dt_target_idx = middle1
                    else:
                        targets       = [middle1,last]
                        factor        = self.segments * 2
                        dt_target_idx = last


                    # Use copy transforms constraints to position each bone
                    # exactly in the location respective to its index (between
                    # the two edges)
                    make_constraint( self, b, {
                        'constraint'  : 'COPY_TRANSFORMS',
                        'subtarget'   : tweaks['ctrl'][targets[0]],
                    })
                    make_constraint( self, b, {
                        'constraint'  : 'COPY_TRANSFORMS',
                        'subtarget'   : tweaks['ctrl'][targets[1]],
                        'influence'   : (i - factor) / self.segments
                    })
                    make_constraint( self, b, {
                        'constraint'  : 'DAMPED_TRACK',
                        'subtarget'   : tweaks['ctrl'][ dt_target_idx ],
                    })

        else:
            for i,b in enumerate( tweaks['mch'] ):
                first  = 0
                middle = trunc( len( tweaks['mch'] ) / 2 )
                last   = len( tweaks['mch'] ) - 1

                if i == first or i == middle:
                    make_constraint( self, b, {
                        'constraint'  : 'COPY_SCALE',
                        'subtarget'   : 'root'
                    })
                elif i != last:
                    targets       = []
                    dt_target_idx = middle
                    factor        = 0
                    if i < middle:
                        targets = [first,middle]
                    else:
                        targets       = [middle,last]
                        factor        = self.segments
                        dt_target_idx = last

                    # Use copy transforms constraints to position each bone
                    # exactly in the location respective to its index (between
                    # the two edges)
                    make_constraint( self, b, {
                        'constraint'  : 'COPY_TRANSFORMS',
                        'subtarget'   : tweaks['ctrl'][targets[0]],
                    })
                    make_constraint( self, b, {
                        'constraint'  : 'COPY_TRANSFORMS',
                        'subtarget'   : tweaks['ctrl'][targets[1]],
                        'influence'   : (i - factor) / self.segments
                    })
                    make_constraint( self, b, {
                        'constraint'  : 'DAMPED_TRACK',
                        'subtarget'   : tweaks['ctrl'][ dt_target_idx ],
                    })

        # Ctrl bones Locks and Widgets
        pb = self.obj.pose.bones
        for t in tweaks['ctrl']:
            pb[t].lock_rotation = True, False, True
            pb[t].lock_scale    = False, True, False

            create_sphere_widget(self.obj, t, bone_transform_name=None)

            if self.tweak_layers:
                pb[t].bone.layers = self.tweak_layers

        return tweaks


    def create_def( self, tweaks ):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        def_bones = []
        for i,org in enumerate(org_bones):
            if i < len(org_bones) - 1:
                # Create segments if specified
                for j in range( self.segments ):
                    name = get_bone_name( strip_org(org), 'def' )
                    def_name = copy_bone( self.obj, org, name )

                    eb[ def_name ].length /= self.segments

                    # If we have more than one segments, place the 2nd and
                    # onwards on the tail of the previous bone
                    if j > 0:
                         put_bone(self.obj, def_name, eb[ def_bones[-1] ].tail)

                    def_bones += [ def_name ]
            else:
                name     = get_bone_name( strip_org(org), 'def' )
                def_name = copy_bone( self.obj, org, name )
                def_bones.append( def_name )

        # Parent deform bones
        for i,b in enumerate( def_bones ):
            if i > 0: # For all bones but the first (which has no parent)
                eb[b].parent      = eb[ def_bones[i-1] ] # to previous
                eb[b].use_connect = True

        # Constraint def to tweaks
        for d,t in zip(def_bones, tweaks):
            tidx = tweaks.index(t)

            make_constraint( self, d, {
                'constraint'  : 'COPY_TRANSFORMS',
                'subtarget'   : t
            })

            if tidx != len(tweaks) - 1:
                make_constraint( self, d, {
                    'constraint'  : 'DAMPED_TRACK',
                    'subtarget'   : tweaks[ tidx + 1 ],
                })

                make_constraint( self, d, {
                    'constraint'  : 'STRETCH_TO',
                    'subtarget'   : tweaks[ tidx + 1 ],
                })

        # Create bbone segments
        for bone in def_bones[:-1]:
            self.obj.data.bones[bone].bbone_segments = self.bbones

        self.obj.data.bones[ def_bones[0]  ].bbone_in  = 0.0
        self.obj.data.bones[ def_bones[-2] ].bbone_out = 0.0
        self.obj.data.bones[ def_bones[-1] ].bbone_in  = 0.0
        self.obj.data.bones[ def_bones[-1] ].bbone_out = 0.0


        # Rubber hose drivers
        pb = self.obj.pose.bones
        for i,t in enumerate( tweaks[1:-1] ):
            # Create custom property on tweak bone to control rubber hose
            name = 'rubber_tweak'

            if i == trunc( len( tweaks[1:-1] ) / 2 ):
                pb[t][name] = 0.0
            else:
                pb[t][name] = 1.0

            prop = rna_idprop_ui_prop_get( pb[t], name, create=True )

            prop["min"]         = 0.0
            prop["max"]         = 2.0
            prop["soft_min"]    = 0.0
            prop["soft_max"]    = 1.0
            prop["description"] = name

        for j,d in enumerate(def_bones[:-1]):
            drvs = {}
            if j != 0:
                tidx = j
                drvs[tidx] = self.obj.data.bones[d].driver_add("bbone_in").driver

            if j != len( def_bones[:-1] ) - 1:
                tidx = j + 1
                drvs[tidx] = self.obj.data.bones[d].driver_add("bbone_out").driver

            for d in drvs:
                drv = drvs[d]
                name = 'rubber_tweak'
                drv.type = 'AVERAGE'
                var = drv.variables.new()
                var.name = name
                var.type = "SINGLE_PROP"
                var.targets[0].id = self.obj
                var.targets[0].data_path = pb[tweaks[d]].path_from_id() + \
                                           '[' + '"' + name + '"' + ']'

        return def_bones


    def create_ik( self, parent ):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        ctrl       = get_bone_name( org_bones[0], 'ctrl', 'ik'        )
        mch_ik     = get_bone_name( org_bones[0], 'mch',  'ik'        )
        mch_target = get_bone_name( org_bones[0], 'mch',  'ik_target' )

        for o, ik in zip( org_bones, [ ctrl, mch_ik, mch_target ] ):
            bone = copy_bone( self.obj, o, ik )

            if org_bones.index(o) == len( org_bones ) - 1:
                eb[ bone ].length /= 4

        # Create MCH Stretch
        mch_str = copy_bone(
            self.obj,
            org_bones[0],
            get_bone_name( org_bones[0], 'mch', 'ik_stretch' )
        )

        if self.limb_type != 'paw':
            eb[ mch_str ].tail = eb[ org_bones[-1] ].head
        else:
            eb[ mch_str ].tail = eb[ org_bones[-2] ].head

        # Parenting
        eb[ ctrl    ].parent = eb[ parent ]
        eb[ mch_str ].parent = eb[ parent ]
        eb[ mch_ik  ].parent = eb[ ctrl   ]


        make_constraint( self, mch_ik, {
            'constraint'  : 'IK',
            'subtarget'   : mch_target,
            'chain_count' : 2,
        })

        pb = self.obj.pose.bones
        pb[ mch_ik ].ik_stretch = 0.1
        pb[ ctrl   ].ik_stretch = 0.1

        # IK constraint Rotation locks
        for axis in ['x','y','z']:
            if axis != self.rot_axis:
               setattr( pb[ mch_ik ], 'lock_ik_' + axis, True )

        # Locks and Widget
        pb[ ctrl ].lock_rotation = True, False, True
        create_ikarrow_widget( self.obj, ctrl, bone_transform_name=None )

        return { 'ctrl'       : { 'limb' : ctrl },
                 'mch_ik'     : mch_ik,
                 'mch_target' : mch_target,
                 'mch_str'    : mch_str
        }


    def create_fk( self, parent ):
        org_bones = self.org_bones.copy()

        if self.limb_type == 'paw':  # Paw base chain is one bone longer
            org_bones.pop()

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        ctrls = []

        for o in org_bones:
            bone = copy_bone( self.obj, o, get_bone_name( o, 'ctrl', 'fk' ) )
            ctrls.append( bone )

        # MCH
        mch = copy_bone(
            self.obj, org_bones[-1], get_bone_name( o, 'mch', 'fk' )
        )

        eb[ mch ].length /= 4

        # Parenting
        eb[ ctrls[0] ].parent      = eb[ parent   ]
        eb[ ctrls[1] ].parent      = eb[ ctrls[0] ]
        eb[ ctrls[1] ].use_connect = True
        eb[ ctrls[2] ].parent      = eb[ mch      ]
        eb[ mch      ].parent      = eb[ ctrls[1] ]
        eb[ mch      ].use_connect = True

        # Constrain MCH's scale to root
        make_constraint( self, mch, {
            'constraint'  : 'COPY_SCALE',
            'subtarget'   : 'root'
        })

        # Locks and widgets
        pb = self.obj.pose.bones
        pb[ ctrls[2] ].lock_location = True, True, True

        create_limb_widget( self.obj, ctrls[0] )
        create_limb_widget( self.obj, ctrls[1] )

        create_circle_widget(self.obj, ctrls[2], radius=0.4, head_tail=0.0)

        for c in ctrls:
            if self.fk_layers:
                pb[c].bone.layers = self.fk_layers

        return { 'ctrl' : ctrls, 'mch' : mch }


    def org_parenting_and_switch( self, org, ik, fk, parent ):
        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones
        # re-parent ORGs in a connected chain
        for i,o in enumerate(org):
            if i > 0:
                eb[o].parent = eb[ org[i-1] ]
                if i <= len(org)-1:
                    eb[o].use_connect = True

        bpy.ops.object.mode_set(mode ='OBJECT')
        pb = self.obj.pose.bones
        pb_parent = pb[ parent ]

        # Create ik/fk switch property
        pb_parent['IK/FK']  = 0.0
        prop = rna_idprop_ui_prop_get( pb_parent, 'IK/FK', create=True )
        prop["min"]         = 0.0
        prop["max"]         = 1.0
        prop["soft_min"]    = 0.0
        prop["soft_max"]    = 1.0
        prop["description"] = 'IK/FK Switch'

        # Constrain org to IK and FK bones
        iks =  [ ik['ctrl']['limb'] ]
        iks += [ ik[k] for k in [ 'mch_ik', 'mch_target'] ]

        for o, i, f in zip( org, iks, fk ):
            make_constraint( self, o, {
                'constraint'  : 'COPY_TRANSFORMS',
                'subtarget'   : i
            })
            make_constraint( self, o, {
                'constraint'  : 'COPY_TRANSFORMS',
                'subtarget'   : f
            })

            # Add driver to relevant constraint
            drv = pb[o].constraints[-1].driver_add("influence").driver
            drv.type = 'AVERAGE'

            var = drv.variables.new()
            var.name = prop.name
            var.type = "SINGLE_PROP"
            var.targets[0].id = self.obj
            var.targets[0].data_path = \
                pb_parent.path_from_id() + '['+ '"' + prop.name + '"' + ']'


    def create_terminal( self, limb_type, bones ):
        if   limb_type == 'arm':
            return create_arm( self, bones )
        elif limb_type == 'leg':
            return create_leg( self, bones )
        elif limb_type == 'paw':
            return create_paw( self, bones )


    def generate( self ):
        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # Clear parents for org bones
        for bone in self.org_bones[1:]:
            eb[bone].use_connect = False
            eb[bone].parent      = None

        bones = {}

        # Create mch limb parent
        bones['parent'] = self.create_parent()
        bones['tweak']  = self.create_tweak()
        bones['def']    = self.create_def( bones['tweak']['ctrl'] )
        bones['ik']     = self.create_ik(  bones['parent']        )
        bones['fk']     = self.create_fk(  bones['parent']        )

        self.org_parenting_and_switch(
            self.org_bones, bones['ik'], bones['fk']['ctrl'], bones['parent']
        )

        bones = self.create_terminal( self.limb_type, bones )

        return [ create_script( bones, self.limb_type ) ]

def add_parameters( params ):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """

    items = [
        ('arm', 'Arm', ''),
        ('leg', 'Leg', ''),
        ('paw', 'Paw', '')
    ]
    params.limb_type = bpy.props.EnumProperty(
        items   = items,
        name    = "Limb Type",
        default = 'arm'
    )

    items = [
        ('x', 'X', ''),
        ('y', 'Y', ''),
        ('z', 'Z', '')
    ]
    params.rotation_axis = bpy.props.EnumProperty(
        items   = items,
        name    = "Rotation Axis",
        default = 'x'
    )

    params.segments = bpy.props.IntProperty(
        name        = 'limb segments',
        default     = 2,
        min         = 1,
        description = 'Number of segments'
    )

    params.bbones = bpy.props.IntProperty(
        name        = 'bbone segments',
        default     = 10,
        min         = 1,
        description = 'Number of segments'
    )

    # Setting up extra layers for the FK and tweak
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

    # Setting up extra layers for the FK and tweak
    params.fk_extra_layers = bpy.props.BoolProperty(
        name        = "fk_extra_layers",
        default     = True,
        description = ""
        )

    params.fk_layers = bpy.props.BoolVectorProperty(
        size        = 32,
        description = "Layers for the FK controls to be on",
        default     = tuple( [ i == 1 for i in range(0, 32) ] )
        )


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters."""

    r = layout.row()
    r.prop(params, "limb_type")

    r = layout.row()
    r.prop(params, "rotation_axis")

    r = layout.row()
    r.prop(params, "segments")

    r = layout.row()
    r.prop(params, "bbones")

    for layer in [ 'fk', 'tweak' ]:
        r = layout.row()
        r.prop(params, layer + "_extra_layers")
        r.active = params.tweak_extra_layers

        col = r.column(align=True)
        row = col.row(align=True)

        for i in range(8):
            row.prop(params, layer + "_layers", index=i, toggle=True, text="")

        row = col.row(align=True)

        for i in range(16,24):
            row.prop(params, layer + "_layers", index=i, toggle=True, text="")

        col = r.column(align=True)
        row = col.row(align=True)

        for i in range(8,16):
            row.prop(params, layer + "_layers", index=i, toggle=True, text="")

        row = col.row(align=True)

        for i in range(24,32):
            row.prop(params, layer + "_layers", index=i, toggle=True, text="")

def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('upper_arm.L')
    bone.head[:] = -0.0016, 0.0060, -0.0012
    bone.tail[:] = 0.2455, 0.0678, -0.1367
    bone.roll = 2.0724
    bone.use_connect = False
    bones['upper_arm.L'] = bone.name
    bone = arm.edit_bones.new('forearm.L')
    bone.head[:] = 0.2455, 0.0678, -0.1367
    bone.tail[:] = 0.4625, 0.0285, -0.2797
    bone.roll = 2.1535
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['upper_arm.L']]
    bones['forearm.L'] = bone.name
    bone = arm.edit_bones.new('hand.L')
    bone.head[:] = 0.4625, 0.0285, -0.2797
    bone.tail[:] = 0.5265, 0.0205, -0.3273
    bone.roll = 2.2103
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['forearm.L']]
    bones['hand.L'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['upper_arm.L']]
    pbone.rigify_type = 'pitchipoy.limbs.super_limb'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    try:
        pbone.rigify_parameters.separate_ik_layers = True
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.ik_layers = [
            False, False, False, False, False, False, False, False, True, False,
            False, False, False, False, False, False, False, False, False, False,
            False, False, False, False, False, False, False, False, False, False,
            False, False
        ]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.separate_hose_layers = True
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.hose_layers = [
            False, False, False, False, False, False, False, False, False, True,
            False, False, False, False, False, False, False, False, False, False,
            False, False, False, False, False, False, False, False, False, False,
            False, False
        ]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.tweak_layers = [
            False, False, False, False, False, False, False, False, False, True,
            False, False, False, False, False, False, False, False, False, False,
            False, False, False, False, False, False, False, False, False, False,
            False, False
        ]
    except AttributeError:
        pass
    try:
        pbone.rigify_parameters.fk_layers = [
            False, False, False, False, False, False, False, False, True, False,
            False, False, False, False, False, False, False, False, False, False,
            False, False, False, False, False, False, False, False, False, False,
            False, False
        ]
    except AttributeError:
        pass
    pbone = obj.pose.bones[bones['forearm.L']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone = obj.pose.bones[bones['hand.L']]
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
