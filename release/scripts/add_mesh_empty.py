#!BPY
"""
Name: 'Empty mesh'
Blender: 243
Group: 'AddMesh'
"""
import BPyAddMesh
import Blender

def main():
	BPyAddMesh.add_mesh_simple('EmptyMesh', [], [], [])

main()