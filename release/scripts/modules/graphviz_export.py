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

import bpy

header = '''
digraph ancestors           {
graph [fontsize=30 labelloc="t" label="" splines=false overlap=true, rankdir=BT];
ratio = "auto" ;
'''

footer = '''
}
'''


def compat_str(text, line_length=0):

    if line_length:
        text_ls = []
        while len(text) > line_length:
            text_ls.append(text[:line_length])
            text = text[line_length:]

        if text:
            text_ls.append(text)
        text = '\n  '.join(text_ls)

    #text = text.replace('.', '.\n')
    #text = text.replace(']', ']\n')
    text = text.replace("\n", "\\n")
    text = text.replace('"', '\\"')
    return text


def graph_armature(obj, filepath, FAKE_PARENT=True, CONSTRAINTS=True, DRIVERS=True, XTRA_INFO=True):
    CONSTRAINTS = DRIVERS = True

    fileobject = open(filepath, "w")
    fw = fileobject.write
    fw(header)
    fw('label = "%s::%s" ;' % (bpy.data.filepath.split("/")[-1].split("\\")[-1], obj.name))

    arm = obj.data

    bones = [bone.name for bone in arm.bones]
    bones.sort()
    print("")
    for bone in bones:
        b = arm.bones[bone]
        print(">>", bone, ["*>", "->"][b.use_connect], getattr(getattr(b, "parent", ""), "name", ""))
        label = [bone]
        bone = arm.bones[bone]

        for key, value in obj.pose.bones[bone.name].items():
            if key.startswith("_"):
                continue

            if type(value) == float:
                value = "%.3f" % value
            elif type(value) == str:
                value = compat_str(value)

            label.append("%s = %s" % (key, value))

        opts = ["shape=box", "regular=1", "style=filled", "fixedsize=false", 'label="%s"' % compat_str('\n'.join(label))]

        if bone.name.startswith('ORG'):
            opts.append("fillcolor=yellow")
        else:
            opts.append("fillcolor=white")

        fw('"%s" [%s];\n' % (bone.name, ','.join(opts)))

    fw('\n\n# Hierarchy:\n')

    # Root node.
    if FAKE_PARENT:
        fw('"Object::%s" [];\n' % obj.name)

    for bone in bones:
        bone = arm.bones[bone]

        parent = bone.parent
        if parent:
            parent_name = parent.name
            connected = bone.use_connect
        elif FAKE_PARENT:
            parent_name = 'Object::%s' % obj.name
            connected = False
        else:
            continue

        opts = ["dir=forward", "weight=2", "arrowhead=normal"]
        if not connected:
            opts.append("style=dotted")

        fw('"%s" -> "%s" [%s] ;\n' % (bone.name, parent_name, ','.join(opts)))
    del bone

    # constraints
    if CONSTRAINTS:
        fw('\n\n# Constraints:\n')
        for bone in bones:
            pbone = obj.pose.bones[bone]
            # must be ordered
            for constraint in pbone.constraints:
                subtarget = getattr(constraint, "subtarget", "")
                if subtarget:
                    # TODO, not internal links
                    opts = ['dir=forward', "weight=1", "arrowhead=normal", "arrowtail=none", "constraint=false", 'color="red"', 'labelfontsize=4']
                    if XTRA_INFO:
                        label = "%s\n%s" % (constraint.type, constraint.name)
                        opts.append('label="%s"' % compat_str(label))
                    fw('"%s" -> "%s" [%s] ;\n' % (pbone.name, subtarget, ','.join(opts)))

    # Drivers
    if DRIVERS:
        fw('\n\n# Drivers:\n')

        def rna_path_as_pbone(rna_path):
            if not rna_path.startswith("pose.bones["):
                return None

            #rna_path_bone = rna_path[:rna_path.index("]") + 1]
            #return obj.path_resolve(rna_path_bone)
            bone_name = rna_path.split("[")[1].split("]")[0]
            return obj.pose.bones[bone_name[1:-1]]

        animation_data = obj.animation_data
        if animation_data:

            fcurve_drivers = [fcurve_driver for fcurve_driver in animation_data.drivers]
            fcurve_drivers.sort(key=lambda fcurve_driver: fcurve_driver.data_path)

            for fcurve_driver in fcurve_drivers:
                rna_path = fcurve_driver.data_path
                pbone = rna_path_as_pbone(rna_path)

                if pbone:
                    for var in fcurve_driver.driver.variables:
                        for target in var.targets:
                            pbone_target = rna_path_as_pbone(target.data_path)
                            rna_path_target = target.data_path
                            if pbone_target:
                                opts = ['dir=forward', "weight=1", "arrowhead=normal", "arrowtail=none", "constraint=false", 'color="blue"', "labelfontsize=4"]
                                display_source = rna_path.replace("pose.bones", "")
                                display_target = rna_path_target.replace("pose.bones", "")
                                if XTRA_INFO:
                                    label = "%s\\n%s" % (display_source, display_target)
                                    opts.append('label="%s"' % compat_str(label))
                                fw('"%s" -> "%s" [%s] ;\n' % (pbone_target.name, pbone.name, ','.join(opts)))

    fw(footer)
    fileobject.close()

    '''
    print(".", end="")
    import sys
    sys.stdout.flush()
    '''
    print("\nSaved:", filepath)
    return True

if __name__ == "__main__":
    import os
    tmppath = "/tmp/test.dot"
    graph_armature(bpy.context.object, tmppath, CONSTRAINTS=True, DRIVERS=True)
    os.system("dot -Tpng %s > %s; eog %s &" % (tmppath, tmppath + '.png', tmppath + '.png'))
