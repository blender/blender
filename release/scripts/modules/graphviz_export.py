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
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

header = '''
digraph ancestors           {
graph [fontsize=30 labelloc="t" label="" splines=false overlap=true, rankdir=BT];
ratio = "auto" ;
'''

footer = '''
}
'''

def compat_str(text):
    text = text.replace("\n", "\\n")
    text = text.replace('"', '\\"')
    return text

def graph_armature(obj, path):
    
    file = open("/tmp/test.dot", "w")
    fw = file.write
    fw(header)
    fw('label = "%s::%s" ;' % (bpy.data.filename.split("/")[-1].split("\\")[-1], obj.name))
    
    arm = obj.data
    
    for bone in arm.bones:
        label = [bone.name]
        for key, value in obj.pose.bones[bone.name].items():
            if key.startswith("_"):
                continue
            
            if type(value) == float:
                value = "%.3f" % value
            elif type(value) == str:
                value = compat_str(value)
            
            label.append("%s = %s" % (key, value))
        
        opts = ["shape=box", "regular=1", "style=filled", "fillcolor=white", 'width="2.33"', 'height="0.35"', "fixedsize=false", 'label="%s"' % ("\\n".join(label))]
        
        fw('"%s" [%s];\n' % (bone.name, ','.join(opts)))
    
    for bone in arm.bones:
        parent = bone.parent
        if parent:
            opts = ["dir=forward", "weight=2", "arrowhead=normal"]
            if not bone.connected:
                opts.append("style=dotted")
            
            fw('"%s" -> "%s" [%s] ;\n' % (bone.name, parent.name, ','.join(opts)))
    del bone    
    
    # constraints
    for pbone in obj.pose.bones:
        for constraint in pbone.constraints:
            subtarget = constraint.subtarget
            if subtarget:
                # TODO, not internal links
                opts = ['dir=forward', "weight=1", "arrowhead=normal", "arrowtail=none", "constraint=false", 'color="red"'] # , 
                label = "%s\n%s" % (constraint.type, constraint.name)
                opts.append('label="%s"' % compat_str(label))
                fw('"%s" -> "%s" [%s] ;\n' % (subtarget, pbone.name, ','.join(opts)))
    
    # Drivers
    def rna_path_as_pbone(rna_path):
        if not rna_path.startswith("pose.bones["):
            return None

        #rna_path_bone = rna_path[:rna_path.index("]") + 1]
        #return obj.path_resolve(rna_path_bone)
        bone_name = rna_path.split("[")[1].split("]")[0]
        return obj.pose.bones[bone_name[1:-1]]
    
    for fcurve_driver in obj.animation_data.drivers:
        rna_path = fcurve_driver.rna_path
        pbone = rna_path_as_pbone(rna_path)
            
        if pbone:
            for target in fcurve_driver.driver.targets:
                pbone_target = rna_path_as_pbone(target.rna_path)
                rna_path_target = target.rna_path
                if pbone_target:
                    opts = ['dir=forward', "weight=1", "arrowhead=normal", "arrowtail=none", "constraint=false", 'color="blue"'] # , 
                    display_source = rna_path.replace("pose.bones", "")
                    display_target = rna_path_target.replace("pose.bones", "")
                    label = "%s\\n%s" % (display_source, display_target)
                    opts.append('label="%s"' % compat_str(label))
                    fw('"%s" -> "%s" [%s] ;\n' % (pbone_target.name, pbone.name, ','.join(opts)))
    
    fw(footer)
    file.close()
    
    print(".", end='')
    import sys
    sys.stdout.flush()

if __name__ == "__main__":
    import bpy
    import os
    path ="/tmp/test.dot"
    graph_armature(bpy.context.object, path)
    os.system("dot -Tpng %s > %s; eog %s &" % (path, path + '.png', path + '.png'))
