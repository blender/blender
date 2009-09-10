import bpy
import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender.slave as slave
import netrender.master as master

from netrender.utils import *

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
@rnaType
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
		col.itemR(scene.network_render, "path")
		
		if scene.network_render.mode == "RENDER_CLIENT":
			col.itemR(scene.network_render, "chunks")
			col.itemR(scene.network_render, "priority")
			col.itemR(scene.network_render, "job_name")
			col.itemO("render.netclientsend", text="send job to server")

@rnaType
class SCENE_PT_network_slaves(RenderButtonsPanel):
	__label__ = "Slaves Status"
	COMPAT_ENGINES = set(['NET_RENDER'])
	
	def poll(self, context):
		scene = context.scene
		return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"

	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		netsettings = scene.network_render

		row = layout.row()
		row.template_list(netsettings, "slaves", netsettings, "active_slave_index", rows=2)

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("render.netclientslaves", icon="ICON_FILE_REFRESH", text="")
		subcol.itemO("render.netclientblacklistslave", icon="ICON_ZOOMOUT", text="")
		
		if len(bpy.data.netrender_slaves) == 0 and len(netsettings.slaves) > 0:
			while(len(netsettings.slaves) > 0):
				netsettings.slaves.remove(0)
		
		if netsettings.active_slave_index >= 0 and len(netsettings.slaves) > 0:
			layout.itemS()
			
			slave = bpy.data.netrender_slaves[netsettings.active_slave_index]

			layout.itemL(text="Name: " + slave.name)
			layout.itemL(text="Address: " + slave.address[0])
			layout.itemL(text="Seen: " + time.ctime(slave.last_seen))
			layout.itemL(text="Stats: " + slave.stats)

@rnaType
class SCENE_PT_network_slaves_blacklist(RenderButtonsPanel):
	__label__ = "Slaves Blacklist"
	COMPAT_ENGINES = set(['NET_RENDER'])
	
	def poll(self, context):
		scene = context.scene
		return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		netsettings = scene.network_render

		row = layout.row()
		row.template_list(netsettings, "slaves_blacklist", netsettings, "active_blacklisted_slave_index", rows=2)

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("render.netclientwhitelistslave", icon="ICON_ZOOMOUT", text="")

		if len(bpy.data.netrender_blacklist) == 0 and len(netsettings.slaves_blacklist) > 0:
			while(len(netsettings.slaves_blacklist) > 0):
				netsettings.slaves_blacklist.remove(0)
		
		if netsettings.active_blacklisted_slave_index >= 0 and len(netsettings.slaves_blacklist) > 0:
			layout.itemS()
			
			slave = bpy.data.netrender_blacklist[netsettings.active_blacklisted_slave_index]

			layout.itemL(text="Name: " + slave.name)
			layout.itemL(text="Address: " + slave.address[0])
			layout.itemL(text="Seen: " + slave.last_seen)
			layout.itemL(text="Stats: " + time.ctime(slave.stats))

@rnaType
class SCENE_PT_network_jobs(RenderButtonsPanel):
	__label__ = "Jobs"
	COMPAT_ENGINES = set(['NET_RENDER'])
	
	def poll(self, context):
		scene = context.scene
		return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"
	
	def draw(self, context):
		layout = self.layout
		
		scene = context.scene
		netsettings = scene.network_render

		row = layout.row()
		row.template_list(netsettings, "jobs", netsettings, "active_job_index", rows=2)

		col = row.column()

		subcol = col.column(align=True)
		subcol.itemO("render.netclientstatus", icon="ICON_FILE_REFRESH", text="")
		subcol.itemO("render.netclientcancel", icon="ICON_ZOOMOUT", text="")
		subcol.itemO("render.netclientdownload", icon='ICON_RENDER_ANIMATION', text="")

		if len(bpy.data.netrender_jobs) == 0 and len(netsettings.jobs) > 0:
			while(len(netsettings.jobs) > 0):
				netsettings.jobs.remove(0)
		
		if netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0:
			layout.itemS()
			
			job = bpy.data.netrender_jobs[netsettings.active_job_index]

			layout.itemL(text="Name: %s" % job.name)
			layout.itemL(text="Length: %04i" % len(job))
			layout.itemL(text="Done: %04i" % job.results[DONE])
			layout.itemL(text="Error: %04i" % job.results[ERROR])

@rnaType
class NetRenderSettings(bpy.types.IDPropertyGroup):
	pass

@rnaType
class NetRenderSlave(bpy.types.IDPropertyGroup):
	pass

@rnaType
class NetRenderJob(bpy.types.IDPropertyGroup):
	pass

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

NetRenderSettings.StringProperty( attr="path",
				name="Path",
				description="Path for temporary files",
				maxlen = 128,
				default = "/tmp/")

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

NetRenderSettings.IntProperty( attr="priority",
				name="Priority",
				description="Priority of the job",
				default = 1,
				min=1,
				max=10)

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

NetRenderJob.StringProperty( attr="name",
				name="Name of the job",
				description="",
				maxlen = 128,
				default = "")
