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

import bpy
import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender
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
	bl_space_type = "PROPERTIES"
	bl_region_type = "WINDOW"
	bl_context = "render"
	# COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here
	
	def poll(self, context):
		rd = context.scene.render_data
		return (rd.use_game_engine==False) and (rd.engine in self.COMPAT_ENGINES)

# Setting panel, use in the scene for now.
@rnaType
class RENDER_PT_network_settings(RenderButtonsPanel):
	bl_label = "Network Settings"
	COMPAT_ENGINES = set(['NET_RENDER'])

	def draw(self, context):
		layout = self.layout

		scene = context.scene
		rd = scene.render_data
		
		layout.active = True
		
		split = layout.split()
		
		col = split.column()
		col.prop(scene.network_render, "mode")
		col.prop(scene.network_render, "path")
		col.prop(scene.network_render, "server_address")
		col.prop(scene.network_render, "server_port")
		
		if scene.network_render.mode == "RENDER_MASTER":
			col.prop(scene.network_render, "server_broadcast")
		else:
			col.operator("render.netclientscan", icon='FILE_REFRESH', text="")

@rnaType
class RENDER_PT_network_job(RenderButtonsPanel):
	bl_label = "Job Settings"
	COMPAT_ENGINES = set(['NET_RENDER'])
	
	def poll(self, context):
		scene = context.scene
		return super().poll(context) and scene.network_render.mode == "RENDER_CLIENT"

	def draw(self, context):
		layout = self.layout

		scene = context.scene
		rd = scene.render_data
		
		layout.active = True
		
		split = layout.split()
		
		col = split.column()
		col.operator("render.netclientanim", icon='RENDER_ANIMATION')
		col.operator("render.netclientsend", icon='FILE_BLEND')
		col.operator("render.netclientweb", icon='QUESTION')
		col.prop(scene.network_render, "job_name")
		col.prop(scene.network_render, "job_category")
		row = col.row()
		row.prop(scene.network_render, "priority")
		row.prop(scene.network_render, "chunks")

@rnaType
class RENDER_PT_network_slaves(RenderButtonsPanel):
	bl_label = "Slaves Status"
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

		sub = row.column(align=True)
		sub.operator("render.netclientslaves", icon='FILE_REFRESH', text="")
		sub.operator("render.netclientblacklistslave", icon='ZOOMOUT', text="")
		
		if len(netrender.slaves) == 0 and len(netsettings.slaves) > 0:
			while(len(netsettings.slaves) > 0):
				netsettings.slaves.remove(0)
		
		if netsettings.active_slave_index >= 0 and len(netsettings.slaves) > 0:
			layout.separator()
			
			slave = netrender.slaves[netsettings.active_slave_index]

			layout.label(text="Name: " + slave.name)
			layout.label(text="Address: " + slave.address[0])
			layout.label(text="Seen: " + time.ctime(slave.last_seen))
			layout.label(text="Stats: " + slave.stats)

@rnaType
class RENDER_PT_network_slaves_blacklist(RenderButtonsPanel):
	bl_label = "Slaves Blacklist"
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

		sub = row.column(align=True)
		sub.operator("render.netclientwhitelistslave", icon='ZOOMOUT', text="")

		if len(netrender.blacklist) == 0 and len(netsettings.slaves_blacklist) > 0:
			while(len(netsettings.slaves_blacklist) > 0):
				netsettings.slaves_blacklist.remove(0)
		
		if netsettings.active_blacklisted_slave_index >= 0 and len(netsettings.slaves_blacklist) > 0:
			layout.separator()
			
			slave = netrender.blacklist[netsettings.active_blacklisted_slave_index]

			layout.label(text="Name: " + slave.name)
			layout.label(text="Address: " + slave.address[0])
			layout.label(text="Seen: " + time.ctime(slave.last_seen))
			layout.label(text="Stats: " + slave.stats)

@rnaType
class RENDER_PT_network_jobs(RenderButtonsPanel):
	bl_label = "Jobs"
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

		sub = row.column(align=True)
		sub.operator("render.netclientstatus", icon='FILE_REFRESH', text="")
		sub.operator("render.netclientcancel", icon='ZOOMOUT', text="")
		sub.operator("render.netclientcancelall", icon='PANEL_CLOSE', text="")
		sub.operator("render.netclientdownload", icon='RENDER_ANIMATION', text="")

		if len(netrender.jobs) == 0 and len(netsettings.jobs) > 0:
			while(len(netsettings.jobs) > 0):
				netsettings.jobs.remove(0)
		
		if netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0:
			layout.separator()
			
			job = netrender.jobs[netsettings.active_job_index]

			layout.label(text="Name: %s" % job.name)
			layout.label(text="Length: %04i" % len(job))
			layout.label(text="Done: %04i" % job.results[DONE])
			layout.label(text="Error: %04i" % job.results[ERROR])

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
				default = "[default]")

NetRenderSettings.IntProperty( attr="server_port",
				name="Server port",
				description="port of the master render server",
				default = 8000,
				min=1,
				max=65535)

NetRenderSettings.BoolProperty( attr="server_broadcast",
				name="Broadcast server address",
				description="broadcast server address on local network",
				default = True)

default_path = os.environ.get("TEMP", None)

if not default_path:
	if os.name == 'nt':
		default_path = "c:/tmp/"
	else:
		default_path = "/tmp/"
elif not default_path.endswith(os.sep):
	default_path += os.sep

NetRenderSettings.StringProperty( attr="path",
				name="Path",
				description="Path for temporary files",
				maxlen = 128,
				default = default_path)

NetRenderSettings.StringProperty( attr="job_name",
				name="Job name",
				description="Name of the job",
				maxlen = 128,
				default = "[default]")

NetRenderSettings.StringProperty( attr="job_category",
				name="Job category",
				description="Category of the job",
				maxlen = 128,
				default = "")

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
						name="Network mode",
						description="Mode of operation of this instance",
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
