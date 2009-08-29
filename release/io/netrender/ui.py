import bpy
import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender.slave as slave
import netrender.master as master

VERSION = b"0.3"

PATH_PREFIX = "/tmp/"

QUEUED = 0
DISPATCHED = 1
DONE = 2
ERROR = 3

class RenderButtonsPanel(bpy.types.Panel):
	__space_type__ = "PROPERTIES"
	__region_type__ = "WINDOW"
	__context__ = "scene"
	# COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here
	
	def poll(self, context):
		rd = context.scene.render_data
		return (rd.use_game_engine==False) and (rd.engine in self.COMPAT_ENGINES)

# Setting panel, use in the scene for now.
class SCENE_PT_network_settings(RenderButtonsPanel):
	__label__ = "Network Settings"
	COMPAT_ENGINES = set(['NET_RENDER'])

	def draw_header(self, context):
		layout = self.layout
		scene = context.scene

	def draw(self, context):
		layout = self.layout
		scene = context.scene
		rd = scene.render_data
		
		layout.active = True
		
		split = layout.split()
		
		col = split.column()
		
		col.itemR(scene.network_render, "mode")
		col.itemR(scene.network_render, "server_address")
		col.itemR(scene.network_render, "server_port")
		
		if scene.network_render.mode == "RENDER_CLIENT":
			col.itemR(scene.network_render, "chunks")
			col.itemR(scene.network_render, "job_name")
			col.itemO("render.netclientsend", text="send job to server")
bpy.types.register(SCENE_PT_network_settings)

class SCENE_PT_network_slaves(RenderButtonsPanel):
	__label__ = "Slaves Status"
	COMPAT_ENGINES = set(['NET_RENDER'])
	
	def poll(self, context):
		scene = context.scene
		return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		netrender = scene.network_render

		row = layout.row()
		row.template_list(netrender, "slaves", netrender, "active_slave_index", rows=2)

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("render.netclientslaves", icon="ICON_FILE_REFRESH", text="")
		subcol.itemO("render.netclientblacklistslave", icon="ICON_ZOOMOUT", text="")
		
		if netrender.active_slave_index >= 0 and len(netrender.slaves) > 0:
			layout.itemS()
			
			slave = netrender.slaves[netrender.active_slave_index]

			layout.itemL(text="Name: " + slave.name)
			layout.itemL(text="Adress: " + slave.adress)
			layout.itemL(text="Seen: " + slave.last_seen)
			layout.itemL(text="Stats: " + slave.stats)

bpy.types.register(SCENE_PT_network_slaves)

class SCENE_PT_network_slaves_blacklist(RenderButtonsPanel):
	__label__ = "Slaves Blacklist"
	COMPAT_ENGINES = set(['NET_RENDER'])
	
	def poll(self, context):
		scene = context.scene
		return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		netrender = scene.network_render

		row = layout.row()
		row.template_list(netrender, "slaves_blacklist", netrender, "active_blacklisted_slave_index", rows=2)

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("render.netclientwhitelistslave", icon="ICON_ZOOMOUT", text="")

		
		if netrender.active_blacklisted_slave_index >= 0 and len(netrender.slaves_blacklist) > 0:
			layout.itemS()
			
			slave = netrender.slaves_blacklist[netrender.active_blacklisted_slave_index]

			layout.itemL(text="Name: " + slave.name)
			layout.itemL(text="Adress: " + slave.adress)
			layout.itemL(text="Seen: " + slave.last_seen)
			layout.itemL(text="Stats: " + slave.stats)
			
bpy.types.register(SCENE_PT_network_slaves_blacklist)

class SCENE_PT_network_jobs(RenderButtonsPanel):
	__label__ = "Jobs"
	COMPAT_ENGINES = set(['NET_RENDER'])
	
	def poll(self, context):
		scene = context.scene
		return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		netrender = scene.network_render

		row = layout.row()
		row.template_list(netrender, "jobs", netrender, "active_job_index", rows=2)

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("render.netclientstatus", icon="ICON_FILE_REFRESH", text="")
		subcol.itemO("render.netclientcancel", icon="ICON_ZOOMOUT", text="")

		
		if netrender.active_job_index >= 0 and len(netrender.jobs) > 0:
			layout.itemS()
			
			job = netrender.jobs[netrender.active_job_index]

			layout.itemL(text="Name: %s" % job.name)
			layout.itemL(text="Length: %04i" % job.length)
			layout.itemL(text="Done: %04i" % job.done)
			layout.itemL(text="Error: %04i" % job.error)
			
bpy.types.register(SCENE_PT_network_jobs)

class NetRenderSettings(bpy.types.IDPropertyGroup):
	pass

class NetRenderSlave(bpy.types.IDPropertyGroup):
	pass

class NetRenderJob(bpy.types.IDPropertyGroup):
	pass

bpy.types.register(NetRenderSettings)
bpy.types.register(NetRenderSlave)
bpy.types.register(NetRenderJob)

bpy.types.Scene.PointerProperty(attr="network_render", type=NetRenderSettings, name="Network Render", description="Network Render Settings")

NetRenderSettings.StringProperty( attr="server_address",
				name="Server address",
				description="IP or name of the master render server",
				maxlen = 128,
				default = "127.0.0.1")

NetRenderSettings.IntProperty( attr="server_port",
				name="Server port",
				description="port of the master render server",
				default = 8000,
				min=1,
				max=65535)

NetRenderSettings.StringProperty( attr="job_name",
				name="Job name",
				description="Name of the job",
				maxlen = 128,
				default = "[default]")

NetRenderSettings.IntProperty( attr="chunks",
				name="Chunks",
				description="Number of frame to dispatch to each slave in one chunk",
				default = 5,
				min=1,
				max=65535)

NetRenderSettings.StringProperty( attr="job_id",
				name="Network job id",
				description="id of the last sent render job",
				maxlen = 64,
				default = "")

NetRenderSettings.IntProperty( attr="active_slave_index",
				name="Index of the active slave",
				description="",
				default = -1,
				min= -1,
				max=65535)

NetRenderSettings.IntProperty( attr="active_blacklisted_slave_index",
				name="Index of the active slave",
				description="",
				default = -1,
				min= -1,
				max=65535)

NetRenderSettings.IntProperty( attr="active_job_index",
				name="Index of the active job",
				description="",
				default = -1,
				min= -1,
				max=65535)

NetRenderSettings.EnumProperty(attr="mode",
						items=(
										("RENDER_CLIENT", "Client", "Act as render client"),
										("RENDER_MASTER", "Master", "Act as render master"),
										("RENDER_SLAVE", "Slave", "Act as render slave"),
									),
						name="network mode",
						description="mode of operation of this instance",
						default="RENDER_CLIENT")

NetRenderSettings.CollectionProperty(attr="slaves", type=NetRenderSlave, name="Slaves", description="")
NetRenderSettings.CollectionProperty(attr="slaves_blacklist", type=NetRenderSlave, name="Slaves Blacklist", description="")
NetRenderSettings.CollectionProperty(attr="jobs", type=NetRenderJob, name="Job List", description="")

NetRenderSlave.StringProperty( attr="name",
				name="Name of the slave",
				description="",
				maxlen = 64,
				default = "")

NetRenderSlave.StringProperty( attr="adress",
				name="Adress of the slave",
				description="",
				maxlen = 64,
				default = "")

NetRenderJob.StringProperty( attr="id",
				name="ID of the job",
				description="",
				maxlen = 64,
				default = "")


NetRenderJob.StringProperty( attr="name",
				name="Name of the job",
				description="",
				maxlen = 128,
				default = "")

NetRenderJob.IntProperty( attr="length",
				name="Number of frames",
				description="",
				default = 0,
				min= 0,
				max=65535)

NetRenderJob.IntProperty( attr="done",
				name="Number of frames rendered",
				description="",
				default = 0,
				min= 0,
				max=65535)

NetRenderJob.IntProperty( attr="error",
				name="Number of frames in error",
				description="",
				default = 0,
				min= 0,
				max=65535)
