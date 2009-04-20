# Blender.Scene.Render.RenderLayer module and the RenderLayer PyType object

"""
The Blender.Scene.Render.RenderLayer submodule.

Scene.Render.RenderLayer
========================

This module provides access to B{Render Layers} in Blender.

Example::
	import bpy
	sce = bpy.data.scenes.active
	render = sce.render
	layer = render.addRenderLayer()
	render.removeRenderLayer(layer)
"""

class RenderLayer:
	"""
	The RenderLayer object
	======================
	@type name: string
	@ivar name: Get or set the name for the L{RenderLayer}
	@type lightGroup: group
	@ivar lightGroup: group of lights
	@type enable: bool
	@ivar enable: enable this render layer
	@type enableZMask: bool
	@ivar enableZMask: Only render what's in front of the solid z values
	@type enableZMaskAll: bool
	@ivar enableZMaskAll: Fill in Z values for solid faces in invisible layers, for masking
	@type enableSolid: bool
	@ivar enableSolid: Render Solid faces in this Layer
	@type enableZTra: bool
	@ivar enableZTra: Render Z-Transparent faces in this Layer (On top of Solid and Halos)
	@type enableHalo: bool
	@ivar enableHalo: Render Halos in this Layer (on top of Solid)
	@type enableEdge: bool
	@ivar enableEdge: Render Edge-enhance in this Layer (only works for Solid faces)
	@type enableSky: bool
	@ivar enableSky: Render Sky or backbuffer in this Layer
	@type enableStrand: bool
	@ivar enableStrand: Render Strands in this Layer
	@type layerMask: bool
	@ivar layerMask: ...
	@type zLayerMask: bool
	@ivar zLayerMask: ...
	  
	@type passCombined: bool
	@ivar passCombined: Deliver full combined RGBA buffer
	@type passZ: bool
	@ivar passZ: Deliver Z values pass
	@type passSpeed: bool
	@ivar passSpeed: Deliver Speed Vector pass
	@type passNormal: bool
	@ivar passNormal: Deliver Normal pass
	@type passUV: bool
	@ivar passUV: Deliver Texture UV pass
	@type passMist: bool
	@ivar passMist: Deliver Mist factor pass (0-1)
	@type passIndex: bool
	@ivar passIndex: Deliver Object Index pass
	@type passColor: bool
	@ivar passColor: Deliver shade-less Color pass
	@type passDiffuse: bool
	@ivar passDiffuse: Deliver Diffuse pass
	@type passSpecular: bool
	@ivar passSpecular: Deliver Specular pass
	@type passShadow: bool
	@ivar passShadow: Deliver Shadow pass
	@type passAO: bool
	@ivar passAO: Deliver AO pass
	@type passReflect: bool
	@ivar passReflect: Deliver Raytraced Reflection pass
	@type passRefract: bool
	@ivar passRefract: Deliver Raytraced Reflection pass
	@type passRadiosity: bool
	@ivar passRadiosity: Deliver Radiosity pass
	
	
	@type passSpecularXOR: bool
	@ivar passSpecularXOR: Deliver Specular pass XOR
	@type passShadowXOR: bool
	@ivar passShadowXOR: Deliver Shadow pass XOR
	@type passAOXOR: bool
	@ivar passAOXOR: Deliver AO pass XOR
	@type passRefractXOR: bool
	@ivar passRefractXOR: Deliver Raytraced Reflection pass XOR
	@type passRadiosityXOR: bool
	@ivar passRadiosityXOR: Deliver Radiosity pass XOR
	"""