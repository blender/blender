import bpy
from mathutils import Vector
from math import pi
from ...utils import copy_bone, flip_bone, put_bone, org, align_bone_y_axis, align_bone_x_axis, align_bone_z_axis
from ...utils import strip_org, make_deformer_name, connected_children_names
from ...utils import create_circle_widget, create_sphere_widget, create_widget, create_chain_widget
from ...utils import MetarigError, make_mechanism_name, create_cube_widget
from rna_prop_ui import rna_idprop_ui_prop_get
from ..limbs.limb_utils import get_bone_name

script = """
controls = [%s]
torso    = '%s'

if is_selected( controls ):
    layout.prop( pose_bones[ torso ], '["%s"]', slider = True )
    layout.prop( pose_bones[ torso ], '["%s"]', slider = True )
"""


class Rig:

    def __init__(self, obj, bone_name, params):
        """ A simplified version of the torso rig. Basically a connected-DEF chain of bones """

        eb = obj.data.edit_bones

        self.obj = obj
        self.org_bones = [bone_name] + connected_children_names(obj, bone_name)
        self.params = params
        self.spine_length = sum([eb[b].length for b in self.org_bones])
        self.bbones = params.bbones

        # Check if user provided the positions of the neck and pivot
        # if params.neck_pos and params.pivot_pos:
        #     self.neck_pos  = params.neck_pos
        #     self.pivot_pos = params.pivot_pos
        # else:
        #     raise MetarigError(
        #         "RIGIFY ERROR: please specify neck and pivot bone positions"
        #     )
        #
        # # Check if neck is lower than pivot
        # if params.neck_pos <= params.pivot_pos:
        #     raise MetarigError(
        #         "RIGIFY ERROR: Neck cannot be below or the same as pivot"
        #     )

        # if params.control_num:
        #     self.control_num = params.control_num
        # else:
        #     raise MetarigError(
        #         "RIGIFY ERROR: please specify number of controls"
        #     )
        #
        # if params.control_num > round(len(eb)/2):
        #     raise MetarigError(
        #         "RIGIFY ERROR: Number of controls must be <= number of bones/2"
        #     )

        # TODO:
        # Limit neck_pos prop  to 1 --> num of bones - 1 (last is head)
        # Limit pivot_pos prop to 2 --> num of bones (must leave place for lower torso)

        # if params.tail_pos:
        #     self.tail_pos = params.tail_pos

        # Assign values to tweak layers props if opted by user
        if params.tweak_extra_layers:
            self.tweak_layers = list(params.tweak_layers)
        else:
            self.tweak_layers = None

        # Report error of user created less than the minimum of 4 bones for rig
        # if len(self.org_bones) <= 4:
        #     raise MetarigError(
        #         "RIGIFY ERROR: invalid rig structure" % (strip_org(bone_name))
        #     )


    # def build_bone_structure( self ):
    #     """ Divide meta-rig into lists of bones according to torso rig anatomy:
    #         Neck --> Upper torso --> Lower torso --> Tail (optional) """
    #
    #     if self.pivot_pos and self.neck_pos:
    #
    #         neck_index  = self.neck_pos  - 1
    #         pivot_index = self.pivot_pos - 1
    #
    #         tail_index = 0
    #         if 'tail_pos' in dir(self):
    #             tail_index  = self.tail_pos  - 1
    #
    #         neck_bones        = self.org_bones[neck_index::]
    #         upper_torso_bones = self.org_bones[pivot_index:neck_index]
    #         lower_torso_bones = self.org_bones[tail_index:pivot_index]
    #
    #         tail_bones        = []
    #         if tail_index:
    #             tail_bones = self.org_bones[::tail_index+1]
    #
    #         return {
    #             'neck'  : neck_bones,
    #             'upper' : upper_torso_bones,
    #             'lower' : lower_torso_bones,
    #             'tail'  : tail_bones
    #         }
    #
    #     else:
    #         return 'ERROR'

    def orient_bone( self, eb, axis, scale, reverse = False ):
        v = Vector((0,0,0))

        setattr(v,axis,scale)

        if reverse:
            tail_vec = v * self.obj.matrix_world
            eb.head[:] = eb.tail
            eb.tail[:] = eb.head + tail_vec
        else:
            tail_vec = v * self.obj.matrix_world
            eb.tail[:] = eb.head + tail_vec

    def create_pivot(self, bones=None, pivot=None):
        """ Create the pivot control and mechanism bones """

        org_bones  = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        if not pivot:
            pivot = int(len(org_bones)/2)

        pivot_name = org_bones[pivot]
        if '.L' in pivot_name:
            prefix = strip_org(org_bones[pivot]).split('.')[0] + '.L'
        elif '.R' in pivot_name:
            prefix = strip_org(org_bones[pivot]).split('.')[0] + '.R'
        else:
            prefix = strip_org(org_bones[pivot]).split('.')[0]

        ctrl_name = get_bone_name(prefix, 'ctrl', 'pivot')
        ctrl_name = copy_bone(self.obj, pivot_name, ctrl_name)
        ctrl_eb = eb[ ctrl_name ]

        self.orient_bone( ctrl_eb, 'y', self.spine_length / 2.5 )

        pivot_loc = eb[pivot_name].head + ((eb[pivot_name].tail - eb[pivot_name].head)/2)*(len(org_bones)%2)

        put_bone( self.obj, ctrl_name, pivot_loc)

        v = eb[org_bones[-1]].tail - eb[org_bones[0]].head  # Create a vector from head of first ORG to tail of last
        v.normalize()
        v_proj = eb[org_bones[0]].y_axis.dot(v)*v   # projection of first ORG to v
        v_point = eb[org_bones[0]].y_axis - v_proj  # a vector co-planar to first ORG and v directed out of the chain

        if v_point.magnitude < eb[org_bones[0]].y_axis.magnitude*1e-03: #if v_point is too small it's not usable
            v_point = eb[org_bones[0]].x_axis

        if self.params.tweak_axis == 'auto':
            align_bone_y_axis(self.obj, ctrl_name, v)
            align_bone_z_axis(self.obj, ctrl_name, -v_point)
        elif self.params.tweak_axis == 'x':
            align_bone_y_axis(self.obj, ctrl_name, Vector((1,0,0)))
            align_bone_x_axis(self.obj, ctrl_name, Vector((0,0,1)))
        elif self.params.tweak_axis == 'y':
            align_bone_y_axis(self.obj, ctrl_name, Vector((0,1,0)))
            align_bone_x_axis(self.obj, ctrl_name, Vector((1,0,0)))
        elif self.params.tweak_axis == 'z':
            align_bone_y_axis(self.obj, ctrl_name, Vector((0,0,1)))
            align_bone_x_axis(self.obj, ctrl_name, Vector((1,0,0)))

        return {
            'ctrl' : ctrl_name
        }#Todo modify following


        org_bones  = self.org_bones
        pivot_name = org_bones[pivot-1]

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # Create torso control bone
        torso_name = 'torso'
        ctrl_name = copy_bone(self.obj, pivot_name, torso_name)
        ctrl_eb = eb[ctrl_name]

        self.orient_bone( ctrl_eb, 'y', self.spine_length / 2.5 )

        # Create mch_pivot
        mch_name = make_mechanism_name( 'pivot' )
        mch_name = copy_bone(self.obj, ctrl_name, mch_name)
        mch_eb = eb[mch_name]

        mch_eb.length /= 4

        # Positioning pivot in a more usable location for animators
        if hasattr(self, 'tail_pos') and self.tail_pos > 0:
            pivot_loc = eb[org_bones[pivot-1]].head
        else:
            pivot_loc = (eb[org_bones[0]].head + eb[org_bones[0]].tail) / 2

        put_bone(self.obj, ctrl_name, pivot_loc)

        return {
            'ctrl': ctrl_name,
            'mch': mch_name
        }

    def create_deform(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        def_bones = []
        for o in org_bones:
            def_name = make_deformer_name(strip_org(o))
            def_name = copy_bone(self.obj, o, def_name)
            def_bones.append(def_name)

        bpy.ops.object.mode_set(mode='POSE')
        # Create bbone segments
        for bone in def_bones:
            self.obj.data.bones[bone].bbone_segments = self.bbones

        if not self.SINGLE_BONE:
            self.obj.data.bones[def_bones[0]].bbone_in = 0.0
            self.obj.data.bones[def_bones[-1]].bbone_out = 0.0
        else:
            self.obj.data.bones[def_bones[0]].bbone_in = 1.0
            self.obj.data.bones[def_bones[-1]].bbone_out = 1.0
        bpy.ops.object.mode_set(mode='EDIT')

        return def_bones

    def create_neck( self, neck_bones ):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # Create neck control
        neck    = copy_bone( self.obj, org(neck_bones[0]), 'neck' )
        neck_eb = eb[ neck ]

        # Neck spans all neck bones (except head)
        neck_eb.tail[:] = eb[ org(neck_bones[-1]) ].head

        # Create head control
        head = copy_bone( self.obj, org(neck_bones[-1]), 'head' )

        # MCH bones
        # Neck MCH stretch
        mch_str = copy_bone( self.obj, neck, make_mechanism_name('STR-neck') )

        # Neck MCH rotation
        mch_neck = copy_bone(
            self.obj, neck, make_mechanism_name('ROT-neck')
        )

        self.orient_bone( eb[mch_neck], 'y', self.spine_length / 10 )

        # Head MCH rotation
        mch_head = copy_bone(
            self.obj, head, make_mechanism_name('ROT-head')
        )

        self.orient_bone( eb[mch_head], 'y', self.spine_length / 10 )

        twk,mch = [],[]

        # Intermediary bones
        for b in neck_bones[1:-1]: # All except 1st neck and (last) head
            mch_name = copy_bone( self.obj, org(b), make_mechanism_name(b) )
            eb[mch_name].length /= 4

            mch += [ mch_name ]

        # Tweak bones
        for b in neck_bones[:-1]: # All except last bone
            twk_name = "tweak_" + b
            twk_name = copy_bone( self.obj, org(b), twk_name )

            eb[twk_name].length /= 2

            twk += [ twk_name ]

        return {
            'ctrl_neck' : neck,
            'ctrl'      : head,
            'mch_str'   : mch_str,
            'mch_neck'  : mch_neck,
            'mch_head'  : mch_head,
            'mch'       : mch,
            'tweak'     : twk
        }

    def create_chest( self, chest_bones ):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # get total spine length

        # Create chest control bone
        chest = copy_bone( self.obj, org( chest_bones[0] ), 'chest' )
        self.orient_bone( eb[chest], 'y', self.spine_length / 3 )

        # create chest mch_wgt
        mch_wgt = copy_bone(
            self.obj, org( chest_bones[-1] ),
            make_mechanism_name( 'WGT-chest' )
        )

        # Create mch and twk bones
        twk,mch = [],[]

        for b in chest_bones:
            mch_name = copy_bone( self.obj, org(b), make_mechanism_name(b) )
            self.orient_bone( eb[mch_name], 'y', self.spine_length / 10 )

            twk_name = "tweak_" + b
            twk_name = copy_bone( self.obj, org(b), twk_name )
            eb[twk_name].length /= 2

            mch += [ mch_name ]
            twk += [ twk_name ]

        return {
            'ctrl'    : chest,
            'mch'     : mch,
            'tweak'   : twk,
            'mch_wgt' : mch_wgt
        }

    def create_hips( self, hip_bones ):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # Create hips control bone
        hips = copy_bone( self.obj, org( hip_bones[-1] ), 'hips' )
        self.orient_bone(
            eb[hips],
            'y',
            self.spine_length / 4,
            reverse = True
        )

        # create hips mch_wgt
        mch_wgt = copy_bone(
            self.obj, org( hip_bones[0] ),
            make_mechanism_name( 'WGT-hips' )
        )

        # Create mch and tweak bones
        twk,mch = [],[]
        for b in hip_bones:
            mch_name = copy_bone( self.obj, org(b), make_mechanism_name(b) )
            self.orient_bone(
                eb[mch_name], 'y', self.spine_length / 10, reverse = True
            )

            twk_name = "tweak_" + b
            twk_name = copy_bone( self.obj, org( b ), twk_name )

            eb[twk_name].length /= 2

            mch += [ mch_name ]
            twk += [ twk_name ]

        return {
            'ctrl'    : hips,
            'mch'     : mch,
            'tweak'   : twk,
            'mch_wgt' : mch_wgt
        }

    def create_tail( self, tail_bones ):
        pass

    def create_chain(self):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode='EDIT')
        eb = self.obj.data.edit_bones

        twk, mch, mch_ctrl, ctrl = [], [], [], []

        suffix = ''
        if '.L' in org_bones[0]:
            suffix = '.L'
        elif '.R' in org_bones[0]:
            suffix = '.R'

        mch_auto = ''
        if not self.SINGLE_BONE:
            mch_name = copy_bone( self.obj, org(org_bones[0]), 'MCH-AUTO-'+strip_org(org_bones[0]).split('.')[0] + suffix)
            eb[mch_name].head = eb[org_bones[0]].head
            eb[mch_name].tail = eb[org_bones[-1]].tail

            mch_auto = mch_name

        # Intermediary bones
        for b in org_bones:     # All

            if self.SINGLE_BONE:
                mch_name = copy_bone(self.obj, org(b), make_mechanism_name(strip_org(b)))
                eb[mch_name].length /= 4
                put_bone(self.obj, mch_name, eb[b].head - (eb[mch_name].tail - eb[mch_name].head))
                align_bone_z_axis(self.obj, mch_name, eb[b].z_axis)
                if '.' in mch_name:
                    mch_rev_name = mch_name.split('.')[0] + '_reverse.' + mch_name.split('.')[1]
                else:
                    mch_rev_name = mch_name + '_reverse'
                mch_rev_name = copy_bone(self.obj, org(b), mch_rev_name)
                eb[mch_rev_name].length /= 4
                eb[mch_rev_name].tail = eb[mch_name].head
                align_bone_z_axis(self.obj, mch_rev_name, eb[b].z_axis)
                mch += [mch_name]
                mch += [mch_rev_name]
                break

            mch_name = copy_bone(self.obj, org(b), make_mechanism_name(strip_org(b)))
            eb[mch_name].length /= 4

            mch += [ mch_name ]

            if b == org_bones[-1]: #Add extra
                mch_name = copy_bone(self.obj, org(b), make_mechanism_name(strip_org(b)))
                eb[mch_name].length /= 4
                put_bone(self.obj, mch_name, eb[b].tail)
                mch += [ mch_name ]

        # Tweak & Ctrl bones
        v = eb[org_bones[-1]].tail - eb[org_bones[0]].head  # Create a vector from head of first ORG to tail of last
        v.normalize()
        v_proj = eb[org_bones[0]].y_axis.dot(v)*v   # projection of first ORG to v
        v_point = eb[org_bones[0]].y_axis - v_proj  # a vector co-planar to first ORG and v directed out of the chain

        if v_point.magnitude < eb[org_bones[0]].y_axis.magnitude*1e-03:
            v_point = eb[org_bones[0]].x_axis

        for b in org_bones:     # All

            suffix = ''
            if '.L' in b:
                suffix = '.L'
            elif '.R' in b:
                suffix = '.R'

            if b == org_bones[0]:
                name = get_bone_name(b.split('.')[0] + suffix, 'ctrl', 'ctrl')
                name = copy_bone(self.obj, org(b), name)
                align_bone_x_axis(self.obj, name, eb[org(b)].x_axis)
                ctrl += [name]
            else:
                name = get_bone_name(b, 'ctrl', 'tweak')
                name = copy_bone(self.obj, org(b), name)
                twk += [name]

            self.orient_bone(eb[name], 'y', eb[name].length / 2)

            if self.params.tweak_axis == 'auto':
                align_bone_y_axis(self.obj, name, v)
                align_bone_z_axis(self.obj, name, -v_point)  # invert?
            elif self.params.tweak_axis == 'x':
                align_bone_y_axis(self.obj, name, Vector((1,0,0)))
                align_bone_x_axis(self.obj, name, Vector((0,0,1)))
            elif self.params.tweak_axis == 'y':
                align_bone_y_axis(self.obj, name, Vector((0,1,0)))
                align_bone_x_axis(self.obj, name, Vector((1,0,0)))
            elif self.params.tweak_axis == 'z':
                align_bone_y_axis(self.obj, name, Vector((0,0,1)))
                align_bone_x_axis(self.obj, name, Vector((1,0,0)))

            if b == org_bones[-1]:      # Add extra
                ctrl_name = get_bone_name(b.split('.')[0] + suffix, 'ctrl', 'ctrl')
                ctrl_name = copy_bone(self.obj, org(b), ctrl_name)

                self.orient_bone(eb[ctrl_name], 'y', eb[ctrl_name].length / 2)

                #TODO check this if else
                if self.params.conv_bone:
                    align_bone_y_axis(self.obj, ctrl_name, eb[org(self.params.conv_bone)].y_axis)
                    align_bone_x_axis(self.obj, ctrl_name, eb[org(self.params.conv_bone)].x_axis)
                    align_bone_z_axis(self.obj, ctrl_name, eb[org(self.params.conv_bone)].z_axis)

                else:
                    if twk:
                        lastname = twk[-1]
                    else:
                        lastname = ctrl[-1]
                    align_bone_y_axis(self.obj, ctrl_name, eb[lastname].y_axis)
                    align_bone_x_axis(self.obj, ctrl_name, eb[lastname].x_axis)
                put_bone(self.obj, ctrl_name, eb[b].tail)

                ctrl += [ctrl_name]

        conv_twk = ''
        # Convergence tweak
        if self.params.conv_bone:
            conv_twk = get_bone_name(self.params.conv_bone, 'ctrl', 'tweak') #"tweak_" + self.params.conv_bone

            if not(conv_twk in eb.keys()):
                conv_twk = copy_bone( self.obj, org(self.params.conv_bone), conv_twk )

        # Mch controls
        suffix = ''
        if '.L' in b:
            suffix = '.L'
        elif '.R' in b:
            suffix = '.R'

        for b in org_bones:
            mch_ctrl_name = "MCH-CTRL-" + strip_org(b).split('.')[0] + suffix
            mch_ctrl_name = copy_bone( self.obj, org(b), mch_ctrl_name )

            self.orient_bone( eb[mch_ctrl_name], 'y', eb[mch_ctrl_name].length/6 )

            if b == org_bones[0] or b == org_bones[-1]:
                name = get_bone_name(b.split('.')[0] + suffix, 'ctrl', 'ctrl') #"ctrl_" + strip_org(b)
            else:
                name = get_bone_name(b, 'ctrl', 'tweak') #"tweak_" + strip_org(b)
            # align_bone_y_axis(self.obj, mch_ctrl_name, eb[name].y_axis)
            # align_bone_x_axis(self.obj, mch_ctrl_name, eb[name].x_axis)

            align_bone_y_axis(self.obj, mch_ctrl_name, eb[name].y_axis)
            align_bone_x_axis(self.obj, mch_ctrl_name, eb[name].x_axis)

            if self.SINGLE_BONE:
                align_bone_z_axis(self.obj, mch_ctrl_name, eb[b].z_axis)

            mch_ctrl += [mch_ctrl_name]

            if b == org_bones[-1]: #Add extra
                mch_ctrl_name = "MCH-CTRL-" + strip_org(b).split('.')[0] + suffix
                mch_ctrl_name = copy_bone( self.obj, org(b), mch_ctrl_name )

                self.orient_bone( eb[mch_ctrl_name], 'y', eb[mch_ctrl_name].length/6 )

                name = get_bone_name(b.split('.')[0] + suffix, 'ctrl', 'ctrl') #"ctrl_" + strip_org(b)
                #align_bone_y_axis(self.obj, mch_ctrl_name, -eb[name].z_axis)
                align_bone_y_axis(self.obj, mch_ctrl_name, eb[name].y_axis)
                align_bone_x_axis(self.obj, mch_ctrl_name, eb[name].x_axis)

                put_bone(self.obj, mch_ctrl_name, eb[b].tail)

                if self.SINGLE_BONE:
                    align_bone_z_axis(self.obj, mch_ctrl_name, eb[b].z_axis)

                mch_ctrl += [mch_ctrl_name]

        return {
            'mch'       : mch,
            'mch_ctrl'  : mch_ctrl,
            'mch_auto'  : mch_auto,
            'tweak'     : twk,
            'ctrl'      : ctrl,
            'conv'      : conv_twk
        }

    def parent_bones(self, bones):
        org_bones = self.org_bones

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        # Parent deform bones
        for i, b in enumerate(bones['def']):
            if i > 0: # For all bones but the first (which has no parent)
                eb[b].parent = eb[bones['def'][i-1]]  # to previous
                eb[b].use_connect = True

        # Todo check case when sup_chain is in bigger rig
        eb[bones['def'][0]].parent = eb[bones['chain']['mch'][0]]

        for i, twk in enumerate(bones['chain']['tweak']):
            eb[ twk ].parent = eb[bones['chain']['mch_ctrl'][i+1]]
            eb[ twk ].use_inherit_scale = False

        eb[bones['chain']['ctrl'][0]].parent = eb[bones['chain']['mch_ctrl'][0]]
        eb[bones['chain']['ctrl'][0]].use_inherit_scale = False
        eb[bones['chain']['ctrl'][1]].parent = eb[bones['chain']['mch_ctrl'][-1]]
        eb[bones['chain']['ctrl'][1]].use_inherit_scale = False


        if 'pivot' in bones.keys():
            eb[ bones['pivot']['ctrl'] ].use_inherit_scale = False

        for i,mch in enumerate( bones['chain']['mch'] ):
            if mch == bones['chain']['mch'][0]:
                eb[ mch ].parent = eb[ bones['chain']['ctrl'][0] ]
            elif mch == bones['chain']['mch'][-1]:
                eb[ mch ].parent = eb[ bones['chain']['ctrl'][1] ]
            else:
                eb[ mch ].parent = eb[ bones['chain']['tweak'][i-1] ]

        if 'parent' in bones.keys():
            eb[bones['chain']['mch_auto']].parent = eb[bones['parent']]
            eb[bones['chain']['mch_ctrl'][0]].parent = eb[bones['parent']]
            eb[bones['chain']['mch_ctrl'][-1]].parent = eb[bones['parent']]

        for i, mch_ctrl in enumerate( bones['chain']['mch_ctrl'][1:-1] ):
            eb[ mch_ctrl ].parent = eb[ bones['chain']['mch_auto'] ]

        if 'pivot' in bones.keys():
            eb[ bones['pivot']['ctrl'] ].parent = eb[ bones['chain']['mch_auto'] ]

        if bones['chain']['conv']:
            eb[ bones['chain']['tweak'][-1] ].parent = eb[ bones['chain']['conv'] ]

        if self.SINGLE_BONE:
            eb[bones['chain']['ctrl'][0]].parent = None
            eb[bones['chain']['ctrl'][-1]].parent = None
            eb[bones['chain']['mch_ctrl'][0]].parent = eb[bones['chain']['ctrl'][0]]
            eb[bones['chain']['mch_ctrl'][-1]].parent = eb[bones['chain']['ctrl'][-1]]
            eb[bones['chain']['mch'][0]].parent = eb[bones['chain']['mch'][1]]
            eb[bones['chain']['mch'][1]].parent = eb[bones['chain']['mch_ctrl'][0]]

        return  #TODO modify what follows

        # Parent control bones
        # Head control => MCH-rotation_head
        eb[ bones['neck']['ctrl'] ].parent = eb[ bones['neck']['mch_head'] ]

        # MCH stretch => neck ctrl
        eb[ bones['neck']['mch_str'] ].parent = eb[ bones['neck']['ctrl_neck'] ]

        # Neck control => MCH-rotation_neck
        eb[ bones['neck']['ctrl_neck'] ].parent = eb[ bones['neck']['mch_neck'] ]

        # Parent hips and chest controls to torso
        eb[ bones['chest']['ctrl'] ].parent = eb[ bones['pivot']['ctrl'] ]
        eb[ bones['hips']['ctrl'] ].parent  = eb[ bones['pivot']['ctrl'] ]

        # Parent mch bones
        # Neck mch
        eb[ bones['neck']['mch_head'] ].parent = eb[ bones['neck']['ctrl_neck'] ]

        parent = eb[ bones['neck']['mch_str'] ]
        for i,b in enumerate([ eb[n] for n in bones['neck']['mch'] ]):
            b.parent = parent

        # Chest mch bones and neck mch
        chest_mch = bones['chest']['mch'] + [ bones['neck']['mch_neck'] ]
        for i,b in enumerate(chest_mch):
            if i == 0:
                eb[b].parent = eb[ bones['pivot']['ctrl'] ]
            else:
                eb[b].parent = eb[ chest_mch[i-1] ]

        # Hips mch bones
        for i,b in enumerate( bones['hips']['mch'] ):
            if i == len(bones['hips']['mch']) - 1:
                eb[b].parent = eb[ bones['pivot']['ctrl'] ]
            else:
                eb[b].parent = eb[ bones['hips']['mch'][i+1] ]

        # mch pivot
        eb[ bones['pivot']['mch'] ].parent = eb[ bones['chest']['mch'][0] ]

        # MCH widgets
        eb[ bones['chest']['mch_wgt'] ].parent = eb[ bones['chest']['mch'][-1] ]
        eb[ bones['hips' ]['mch_wgt'] ].parent = eb[ bones['hips' ]['mch'][0 ] ]

        # Tweaks

        # Neck tweaks
        for i,twk in enumerate( bones['neck']['tweak'] ):
            if i == 0:
                eb[ twk ].parent = eb[ bones['neck']['ctrl_neck'] ]
            else:
                eb[ twk ].parent = eb[ bones['neck']['mch'][i-1] ]

        # Chest tweaks
        for twk,mch in zip( bones['chest']['tweak'], bones['chest']['mch'] ):
            if bones['chest']['tweak'].index( twk ) == 0:
                eb[ twk ].parent = eb[ bones['pivot']['mch'] ]
            else:
                eb[ twk ].parent = eb[ mch ]

        # Hips tweaks
        for i,twk in enumerate(bones['hips']['tweak']):
            if i == 0:
                eb[twk].parent = eb[ bones['hips']['mch'][i] ]
            else:
                eb[twk].parent = eb[ bones['hips']['mch'][i-1] ]

        # Parent orgs to matching tweaks
        tweaks =  bones['hips']['tweak'] + bones['chest']['tweak']
        tweaks += bones['neck']['tweak'] + [ bones['neck']['ctrl'] ]

        if 'tail' in bones.keys():
            tweaks += bones['tail']['tweak']

        for org, twk in zip( org_bones, tweaks ):
            eb[ org ].parent = eb[ twk ]

    def make_constraint(self, bone, constraint):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        owner_pb = pb[bone]
        const = owner_pb.constraints.new(constraint['constraint'])
        const.target = self.obj

        # filter contraint props to those that actually exist in the current
        # type of constraint, then assign values to each
        for p in [k for k in constraint.keys() if k in dir(const)]:
            setattr(const, p, constraint[p])

    def constrain_bones(self, bones):
        # DEF bones

        deform = bones['def']
        mch = bones['chain']['mch']
        mch_ctrl = bones['chain']['mch_ctrl']
        ctrls = bones['chain']['ctrl']
        tweaks = [ ctrls[0]] + bones['chain']['tweak'] + [ ctrls[-1] ]

        # if 'conv' in bones['chain'].keys():
        #     conv_tweak = bones['chain']['conv']

        for i, d in enumerate(deform):

            if len(deform) > 1:
                self.make_constraint(d, {
                    'constraint': 'COPY_TRANSFORMS',
                    'subtarget': mch[i],
                    'owner_space': 'POSE',
                    'target_space': 'POSE'
                })

            self.make_constraint(d, {
                'constraint': 'STRETCH_TO',
                'subtarget': tweaks[i+1]
            })

        if 'pivot' in bones.keys():
            step = 2/(len(self.org_bones))
            for i,b in enumerate(mch_ctrl):
                xval = i*step
                influence = 2*xval - xval**2 #parabolic influence of pivot
                if (i != 0) and (i != len(mch_ctrl)-1):
                    self.make_constraint( b, {
                        'constraint' : 'COPY_TRANSFORMS',
                        'subtarget'  : bones['pivot']['ctrl'],
                        'influence'  : influence,
                        'owner_space'  : 'LOCAL',
                        'target_space' : 'LOCAL'
                    } )

        # MCH-AUTO

        mch_auto = bones['chain']['mch_auto']

        if mch_auto:
            self.make_constraint( mch_auto, {
                'constraint': 'COPY_LOCATION',
                'subtarget' : mch[0],
                'owner_space'  : 'WORLD',
                'target_space' : 'WORLD'
            } )

            self.make_constraint( mch_auto, {
                'constraint'    : 'STRETCH_TO',
                'subtarget'     : tweaks[-1]
            } )

        # PIVOT CTRL

        if 'pivot' in bones.keys():

            pivot = bones['pivot']['ctrl']

            self.make_constraint( pivot, {
                'constraint': 'COPY_ROTATION',
                'subtarget' : tweaks[0],
                'influence' :   0.33,
                'owner_space'  : 'LOCAL',
                'target_space' : 'LOCAL'
            } )

            self.make_constraint( pivot, {
                'constraint': 'COPY_ROTATION',
                'subtarget' : tweaks[-1],
                'influence' :   0.33,
                'owner_space'  : 'LOCAL',
                'target_space' : 'LOCAL'
            } )

        # MCH-CTRL

        # for i, b in enumerate(mch_ctrl):
        #
        #     if i != 0 and mch_ctrl[i] != mch_ctrl[-1]:
        #         self.make_constraint( b, {
        #             'constraint' : 'COPY_TRANSFORMS',
        #             'subtarget'  : tweaks[i-1],
        #             'influence'  : 0.5,
        #             'owner_space'  : 'LOCAL',
        #             'target_space' : 'LOCAL'
        #         } )
        #
        #         self.make_constraint( b, {
        #             'constraint' : 'COPY_TRANSFORMS',
        #             'subtarget'  : tweaks[i+1],
        #             'influence'  : 0.5,
        #             'owner_space'  : 'LOCAL',
        #             'target_space' : 'LOCAL'
        #         } )

        return  #Todo mdify what follows

        # head and neck MCH bones
        for b in [ bones['neck']['mch_head'], bones['neck']['mch_neck'] ]:
            self.make_constraint( b, {
                'constraint' : 'COPY_ROTATION',
                'subtarget'  : bones['pivot']['ctrl'],
            } )
            self.make_constraint( b, {
                'constraint' : 'COPY_SCALE',
                'subtarget'  : bones['pivot']['ctrl'],
            } )

        # Neck MCH Stretch
        self.make_constraint( bones['neck']['mch_str'], {
            'constraint'  : 'DAMPED_TRACK',
            'subtarget'   : bones['neck']['ctrl'],
        })

        self.make_constraint( bones['neck']['mch_str'], {
            'constraint'  : 'STRETCH_TO',
            'subtarget'   : bones['neck']['ctrl'],
        })

        # Intermediary mch bones
        intermediaries = [ bones['neck'], bones['chest'], bones['hips'] ]

        if 'tail' in bones.keys():
            intermediaries += bones['tail']

        for i,l in enumerate(intermediaries):
            mch     = l['mch']
            factor  = float( 1 / len( l['tweak'] ) )

            for j,b in enumerate(mch):
                if i == 0:
                    nfactor = float( (j + 1) / len( mch ) )
                    self.make_constraint( b, {
                        'constraint'   : 'COPY_ROTATION',
                        'subtarget'    : l['ctrl'],
                        'influence'    : nfactor
                    } )
                else:
                    self.make_constraint( b, {
                        'constraint'   : 'COPY_TRANSFORMS',
                        'subtarget'    : l['ctrl'],
                        'influence'    : factor,
                        'owner_space'  : 'LOCAL',
                        'target_space' : 'LOCAL'
                    } )


        # MCH pivot
        self.make_constraint( bones['pivot']['mch'], {
            'constraint'   : 'COPY_TRANSFORMS',
            'subtarget'    : bones['hips']['mch'][-1],
            'owner_space'  : 'LOCAL',
            'target_space' : 'LOCAL'
        })

        # DEF bones
        deform =  bones['def']
        tweaks =  bones['hips']['tweak'] + bones['chest']['tweak']
        tweaks += bones['neck']['tweak'] + [ bones['neck']['ctrl'] ]

        for d,t in zip(deform, tweaks):
            tidx = tweaks.index(t)

            self.make_constraint( d, {
                'constraint'  : 'COPY_TRANSFORMS',
                'subtarget'   : t
            })

            if tidx != len(tweaks) - 1:
                self.make_constraint( d, {
                    'constraint'  : 'DAMPED_TRACK',
                    'subtarget'   : tweaks[ tidx + 1 ],
                })

                self.make_constraint( d, {
                    'constraint'  : 'STRETCH_TO',
                    'subtarget'   : tweaks[ tidx + 1 ],
                })

    def stick_to_bendy_bones(self, bones):
        bpy.ops.object.mode_set(mode='OBJECT')
        deform = bones['def']
        pb = self.obj.pose.bones

        if len(deform) > 1:  # Only for single bone sup chain
            return

        def_pb = pb[deform[0]]
        ctrl_start = pb[bones['chain']['ctrl'][0]]
        ctrl_end = pb[bones['chain']['ctrl'][-1]]
        mch_start = pb[bones['chain']['mch'][0]]
        mch_end = pb[bones['chain']['mch_ctrl'][-1]]

        if 'bbone_custom_handle_start' in dir(def_pb) and 'bbone_custom_handle_end' in dir(def_pb):
            if not self.SINGLE_BONE:
                def_pb.bbone_custom_handle_start = ctrl_start
                def_pb.bbone_custom_handle_end = ctrl_end
            else:
                def_pb.bbone_custom_handle_start = mch_start
                def_pb.bbone_custom_handle_end = mch_end
            def_pb.use_bbone_custom_handles = True

    def create_drivers(self, bones):
        bpy.ops.object.mode_set(mode ='OBJECT')
        pb = self.obj.pose.bones

        # Setting the torso's props
        torso = pb[ bones['pivot']['ctrl'] ]

        props  = [ "head_follow", "neck_follow" ]
        owners = [ bones['neck']['mch_head'], bones['neck']['mch_neck'] ]

        for prop in props:
            if prop == 'neck_follow':
                torso[prop] = 0.5
            else:
                torso[prop] = 0.0

            prop = rna_idprop_ui_prop_get( torso, prop, create=True )
            prop["min"]         = 0.0
            prop["max"]         = 1.0
            prop["soft_min"]    = 0.0
            prop["soft_max"]    = 1.0
            prop["description"] = prop

        # driving the follow rotation switches for neck and head
        for bone, prop, in zip( owners, props ):
            # Add driver to copy rotation constraint
            drv = pb[ bone ].constraints[ 0 ].driver_add("influence").driver
            drv.type = 'AVERAGE'

            var = drv.variables.new()
            var.name = prop
            var.type = "SINGLE_PROP"
            var.targets[0].id = self.obj
            var.targets[0].data_path = \
                torso.path_from_id() + '['+ '"' + prop + '"' + ']'

            drv_modifier = self.obj.animation_data.drivers[-1].modifiers[0]

            drv_modifier.mode            = 'POLYNOMIAL'
            drv_modifier.poly_order      = 1
            drv_modifier.coefficients[0] = 1.0
            drv_modifier.coefficients[1] = -1.0

    def locks_and_widgets(self, bones):
        bpy.ops.object.mode_set(mode='OBJECT')
        pb = self.obj.pose.bones

        #Locks
        mch_ctrl = bones['chain']['mch_ctrl']

        for b in mch_ctrl:
            pb[b].lock_rotation = False, False, False
            pb[b].lock_location = False, False, False
            pb[b].lock_scale = False, False, False

        for b in bones['chain']['tweak']:
            pb[b].lock_rotation = True, False, True

        for b in bones['chain']['ctrl']:
            pb[b].lock_rotation = True, False, True

        if 'pivot' in bones.keys():
            pb[bones['pivot']['ctrl']].lock_rotation = True, False, True

        # Assigning a widget to main ctrl bone
        if 'pivot' in bones.keys():
            create_cube_widget(
                self.obj,
                bones['pivot']['ctrl'],
                radius              = 0.15,
                bone_transform_name = None
            )

        for bone in bones['chain']['tweak']:
            create_cube_widget(
                self.obj,
                bone,
                radius              = 0.2,
                bone_transform_name = None
            )

        create_chain_widget(
            self.obj,
            bones['chain']['ctrl'][0],
            invert              = False,
            radius              = 0.3,
            bone_transform_name = None
        )

        create_chain_widget(
            self.obj,
            bones['chain']['ctrl'][-1],
            invert              = True,
            radius              = 0.3,
            bone_transform_name = None
        )

        if bones['chain']['conv']:
            create_cube_widget(
                self.obj,
                bones['chain']['conv'],
                radius              = 0.5,
                bone_transform_name = None
            )

        # Assigning layers to tweaks and ctrls
        for bone in bones['chain']['tweak']:
            if self.tweak_layers:
                pb[bone].bone.layers = self.tweak_layers

        # for bone in bones['chain']['ctrl']:
        #     if self.tweak_layers:
        #         pb[bone].bone.layers = self.tweak_layers


        return


        # Locks
        tweaks =  bones['neck']['tweak'] + bones['chest']['tweak']
        tweaks += bones['hips']['tweak']

        if 'tail' in bones.keys():
            tweaks += bones['tail']['tweak']

        # Tweak bones locks
        for bone in tweaks:
            pb[bone].lock_rotation = True, False, True
            pb[bone].lock_scale    = False, True, False

        # Widgets

        # Assigning a widget to pivot bone
        if 'pivot' in bones.keys():
            create_cube_widget(
                self.obj,
                bones['pivot']['ctrl'],
                radius              = 0.5,
                bone_transform_name = None
            )

        # Assigning widgets to control bones
        gen_ctrls = [
            bones['neck']['ctrl_neck'],
            bones['chest']['ctrl'],
            bones['hips']['ctrl']
        ]

        if 'tail' in bones.keys():
            gen_ctrls += [ bones['tail']['ctrl'] ]

        for bone in gen_ctrls:
            create_circle_widget(
                self.obj,
                bone,
                radius              = 1.0,
                head_tail           = 0.5,
                with_line           = False,
                bone_transform_name = None
            )

        # Head widget
        create_circle_widget(
            self.obj,
            bones['neck']['ctrl'],
            radius              = 0.75,
            head_tail           = 1.0,
            with_line           = False,
            bone_transform_name = None
        )

        # place widgets on correct bones
        chest_widget_loc = pb[ bones['chest']['mch_wgt'] ]
        pb[bones['chest']['ctrl'] ].custom_shape_transform = chest_widget_loc

        hips_widget_loc = pb[bones['hips']['mch_wgt']]
        if 'tail' in bones.keys():
            hips_widget_loc = bones['def'][self.tail_pos - 1]

        pb[ bones['hips']['ctrl'] ].custom_shape_transform = hips_widget_loc

        # Assigning widgets to tweak bones and layers
        for bone in tweaks:
            create_sphere_widget(self.obj, bone, bone_transform_name=None)

            if self.tweak_layers:
                pb[bone].bone.layers = self.tweak_layers

    def generate(self):

        # Torso Rig Anatomy:
        # Neck: all bones above neck point, last bone is head
        # Upper torso: all bones between pivot and neck start
        # Lower torso: all bones below pivot until tail point
        # Tail: all bones below tail point

        #bone_chains = self.build_bone_structure()

        self.SINGLE_BONE = (len(self.org_bones) == 1)

        bpy.ops.object.mode_set(mode ='EDIT')
        eb = self.obj.data.edit_bones

        bones = {}
        if eb[self.org_bones[0]].parent:
            bones['parent'] = eb[self.org_bones[0]].parent.name

        # Clear parents for org bones
        for bone in self.org_bones[0:]:
            eb[bone].use_connect = False
            eb[bone].parent      = None

        #if bone_chains != 'ERROR':
            #
            # # Create lists of bones and strip "ORG" from their names
            # neck_bones        = [ strip_org(b) for b in bone_chains['neck' ] ]
            # upper_torso_bones = [ strip_org(b) for b in bone_chains['upper'] ]
            # lower_torso_bones = [ strip_org(b) for b in bone_chains['lower'] ]
            # tail_bones        = [ strip_org(b) for b in bone_chains['tail' ] ]

        chain_bones = [strip_org(b) for b in self.org_bones]

            # bones = {}
            #
            # bones['def']   = self.create_deform() # Gets org bones from self
            # bones['pivot'] = self.create_pivot( self.pivot_pos )
            # bones['neck']  = self.create_neck( neck_bones )
            # bones['chest'] = self.create_chest( upper_torso_bones )
            # bones['hips']  = self.create_hips( lower_torso_bones )
            # # TODO: Add create tail
            #
            # if tail_bones:
            #     bones['tail'] = self.create_tail( tail_bones )



        bones['def'] = self.create_deform()
        if len(self.org_bones) > 2:
            bones['pivot'] = self.create_pivot()
        bones['chain'] = self.create_chain()

        # Adjust Roll in SINGLE_BONE case
        #if self.SINGLE_BONE:
        all_bones = bones['chain']['mch'] + bones['chain']['mch_ctrl'] + bones['chain']['ctrl'] + bones['def']
        for b in all_bones:
            eb[b].roll = -pi / 2

        #Todo create pivot-like controls

            # # TEST
            # bpy.ops.object.mode_set(mode ='EDIT')
            # eb = self.obj.data.edit_bones
            #
            # self.parent_bones(      bones )
            # self.constrain_bones(   bones )
            # self.create_drivers(    bones )
            # self.locks_and_widgets( bones )

        self.parent_bones(bones)
        self.constrain_bones(bones)
        self.stick_to_bendy_bones(bones)
        self.locks_and_widgets(bones)

        #Todo invoke the remaining functions

        # controls =  [ bones['neck']['ctrl'],  bones['neck']['ctrl_neck'] ]
        # controls += [ bones['chest']['ctrl'], bones['hips']['ctrl']      ]
        # controls += [ bones['pivot']['ctrl'] ]

        # if 'tail' in bones.keys():
        #     controls += [ bones['tail']['ctrl'] ]

        return  #TODO modify what follows

        # Create UI
        controls_string = ", ".join(["'" + x + "'" for x in controls])  #TODO correct this
        return [script % (
            controls_string,
            bones['pivot']['ctrl'],
            'head_follow',
            'neck_follow'
            )]


def add_parameters(params):
    """ Add the parameters of this rig type to the
        RigifyParameters PropertyGroup
    """

    items = [
        ('auto', 'Auto', ''),
        ('x', 'X', ''),
        ('y', 'Y', ''),
        ('z', 'Z', '')
    ]

    params.tweak_axis = bpy.props.EnumProperty(
        items   = items,
        name    = "Tweak Axis",
        default = 'auto'
    )

    params.conv_bone = bpy.props.StringProperty(
        name = 'Convergence bone',
        default = ''
    )

    params.bbones = bpy.props.IntProperty(
        name        = 'bbone segments',
        default     = 10,
        min         = 1,
        description = 'Number of segments'
    )

    # params.neck_pos = bpy.props.IntProperty(
    #     name        = 'neck_position',
    #     default     = 6,
    #     min         = 0,
    #     description = 'Neck start position'
    # )
    #
    # params.pivot_pos = bpy.props.IntProperty(
    #     name         = 'pivot_position',
    #     default      = 3,
    #     min          = 0,
    #     description  = 'Position of the torso control and pivot point'
    # )
    #
    # params.tail_pos = bpy.props.IntProperty(
    #     name        = 'tail_position',
    #     default     = 0,
    #     min         = 0,
    #     description = 'Where the tail starts (change from 0 to enable)'
    # )

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


def parameters_ui(layout, params):
    """ Create the ui for the rig parameters."""

    # r = layout.row()
    # r.prop(params, "neck_pos")
    #
    # r = layout.row()
    # r.prop(params, "pivot_pos")
    #
    # r = layout.row()
    # r.prop(params, "tail_pos")

    # r = layout.row()
    # r.prop(params, "control_num")

    pb = bpy.context.object.pose

    r = layout.row()
    col = r.column(align=True)
    row = col.row(align=True)
    row.prop(params, "tweak_axis", expand=True)
    # for i,axis in enumerate( [ 'x', 'y', 'z' ] ):
    #     row.prop(params, "tweak_axis", index=i, toggle=True, text=axis)

    r = layout.row()
    r.prop(params, "bbones")

    r = layout.row()
    r.prop_search(params, 'conv_bone', pb, "bones", text="Convergence Bone")

    r = layout.row()
    r.prop(params, "tweak_extra_layers")
    r.active = params.tweak_extra_layers

    col = r.column(align=True)
    row = col.row(align=True)

    bone_layers = bpy.context.active_pose_bone.bone.layers[:]

    for i in range(8):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    row = col.row(align=True)

    for i in range(16,24):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    col = r.column(align=True)
    row = col.row(align=True)

    for i in range(8,16):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)

    row = col.row(align=True)

    for i in range(24,32):
        icon = "NONE"
        if bone_layers[i]:
            icon = "LAYER_ACTIVE"
        row.prop(params, "tweak_layers", index=i, toggle=True, text="", icon=icon)


def create_sample(obj):
    # generated by rigify.utils.write_metarig
    bpy.ops.object.mode_set(mode='EDIT')
    arm = obj.data

    bones = {}

    bone = arm.edit_bones.new('spine')
    bone.head[:] = 0.0000, 0.0000, 0.0000
    bone.tail[:] = 0.0000, 0.5000/8, 1.0000/8
    bone.roll = 0.0000
    bone.use_connect = False
    bones['spine'] = bone.name
    bone = arm.edit_bones.new('spine.001')
    bone.head[:] = 0.0000, 0.5000/8, 1.0000/8
    bone.tail[:] = 0.0000, 0.7500/8, 2.0000/8
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine']]
    bones['spine.001'] = bone.name
    bone = arm.edit_bones.new('spine.002')
    bone.head[:] = 0.0000, 0.7500/8, 2.0000/8
    bone.tail[:] = 0.0000, 0.5000/8, 3.0000/8
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.001']]
    bones['spine.002'] = bone.name
    bone = arm.edit_bones.new('spine.003')
    bone.head[:] = 0.0000, 0.5000/8, 3.0000/8
    bone.tail[:] = 0.0000, 0.0000, 4.0000/8
    bone.roll = 0.0000
    bone.use_connect = True
    bone.parent = arm.edit_bones[bones['spine.002']]
    bones['spine.003'] = bone.name

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones[bones['spine']]
    pbone.rigify_type = 'experimental.super_chain'
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone.bone.layers = [True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    pbone = obj.pose.bones[bones['spine.001']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone.bone.layers = [True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    pbone = obj.pose.bones[bones['spine.002']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone.bone.layers = [True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]
    pbone = obj.pose.bones[bones['spine.003']]
    pbone.rigify_type = ''
    pbone.lock_location = (False, False, False)
    pbone.lock_rotation = (False, False, False)
    pbone.lock_rotation_w = False
    pbone.lock_scale = (False, False, False)
    pbone.rotation_mode = 'QUATERNION'
    pbone.bone.layers = [True, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False, False]

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

    arm.layers = [(x in [0]) for x in range(32)]