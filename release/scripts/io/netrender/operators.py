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
import http, http.client, http.server, urllib, socket
import webbrowser

from netrender.utils import *
import netrender.client as client
import netrender.model

@rnaOperator
class RENDER_OT_netclientanim(bpy.types.Operator):
	'''Start rendering an animation on network'''
	bl_idname = "render.netclientanim"
	bl_label = "Animation on network"
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		scene = context.scene
		netsettings = scene.network_render
		
		conn = clientConnection(netsettings.server_address, netsettings.server_port)
		
		if conn:
			# Sending file
			scene.network_render.job_id = client.clientSendJob(conn, scene, True)
			conn.close()
		
		bpy.ops.screen.render('INVOKE_AREA', animation=True)
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class RENDER_OT_netclientsend(bpy.types.Operator):
	'''Send Render Job to the Network'''
	bl_idname = "render.netclientsend"
	bl_label = "Send job"
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		scene = context.scene
		netsettings = scene.network_render
		
		conn = clientConnection(netsettings.server_address, netsettings.server_port)
		
		if conn:
			# Sending file
			scene.network_render.job_id = client.clientSendJob(conn, scene, True)
			conn.close()
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class RENDER_OT_netclientstatus(bpy.types.Operator):
	'''Refresh the status of the current jobs'''
	bl_idname = "render.netclientstatus"
	bl_label = "Client Status"
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(netsettings.server_address, netsettings.server_port)

		if conn:
			conn.request("GET", "/status")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			jobs = (netrender.model.RenderJob.materialize(j) for j in eval(str(response.read(), encoding='utf8')))
			
			while(len(netsettings.jobs) > 0):
				netsettings.jobs.remove(0)
			
			bpy.netrender_jobs = []
			
			for j in jobs:
				bpy.netrender_jobs.append(j)
				netsettings.jobs.add()
				job = netsettings.jobs[-1]
				
				j.results = j.framesStatus() # cache frame status
				
				job.name = j.name
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class RENDER_OT_netclientblacklistslave(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	bl_idname = "render.netclientblacklistslave"
	bl_label = "Client Blacklist Slave"
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		
		if netsettings.active_slave_index >= 0:
			
			# deal with data
			slave = bpy.netrender_slaves.pop(netsettings.active_slave_index)
			bpy.netrender_blacklist.append(slave)
			
			# deal with rna
			netsettings.slaves_blacklist.add()
			netsettings.slaves_blacklist[-1].name = slave.name
			
			netsettings.slaves.remove(netsettings.active_slave_index)
			netsettings.active_slave_index = -1
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class RENDER_OT_netclientwhitelistslave(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	bl_idname = "render.netclientwhitelistslave"
	bl_label = "Client Whitelist Slave"
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		
		if netsettings.active_blacklisted_slave_index >= 0:
			
			# deal with data
			slave = bpy.netrender_blacklist.pop(netsettings.active_blacklisted_slave_index)
			bpy.netrender_slaves.append(slave)
			
			# deal with rna
			netsettings.slaves.add()
			netsettings.slaves[-1].name = slave.name
			
			netsettings.slaves_blacklist.remove(netsettings.active_blacklisted_slave_index)
			netsettings.active_blacklisted_slave_index = -1
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)


@rnaOperator
class RENDER_OT_netclientslaves(bpy.types.Operator):
	'''Refresh status about available Render slaves'''
	bl_idname = "render.netclientslaves"
	bl_label = "Client Slaves"
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(netsettings.server_address, netsettings.server_port)
		
		if conn:
			conn.request("GET", "/slaves")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			slaves = (netrender.model.RenderSlave.materialize(s) for s in eval(str(response.read(), encoding='utf8')))
			
			while(len(netsettings.slaves) > 0):
				netsettings.slaves.remove(0)
			
			bpy.netrender_slaves = []
			
			for s in slaves:
				for i in range(len(bpy.netrender_blacklist)):
					slave = bpy.netrender_blacklist[i]
					if slave.id == s.id:
						bpy.netrender_blacklist[i] = s
						netsettings.slaves_blacklist[i].name = s.name
						break
				else:
					bpy.netrender_slaves.append(s)
					
					netsettings.slaves.add()
					slave = netsettings.slaves[-1]
					slave.name = s.name
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class RENDER_OT_netclientcancel(bpy.types.Operator):
	'''Cancel the selected network rendering job.'''
	bl_idname = "render.netclientcancel"
	bl_label = "Client Cancel"
	
	def poll(self, context):
		netsettings = context.scene.network_render
		return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0
		
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(netsettings.server_address, netsettings.server_port)
		
		if conn:
			job = bpy.netrender_jobs[netsettings.active_job_index]
			
			conn.request("POST", "/cancel", headers={"job-id":job.id})
			
			response = conn.getresponse()
			print( response.status, response.reason )

			netsettings.jobs.remove(netsettings.active_job_index)
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)
	
@rnaOperator
class RENDER_OT_netclientcancelall(bpy.types.Operator):
	'''Cancel all running network rendering jobs.'''
	bl_idname = "render.netclientcancelall"
	bl_label = "Client Cancel All"
	
	def poll(self, context):
		return True
		
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(netsettings.server_address, netsettings.server_port)
		
		if conn:
			conn.request("POST", "/clear")
			
			response = conn.getresponse()
			print( response.status, response.reason )
		
			while(len(netsettings.jobs) > 0):
				netsettings.jobs.remove(0)

		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class netclientdownload(bpy.types.Operator):
	'''Download render results from the network'''
	bl_idname = "render.netclientdownload"
	bl_label = "Client Download"
	
	def poll(self, context):
		netsettings = context.scene.network_render
		return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0
		
	def execute(self, context):
		netsettings = context.scene.network_render
		rd = context.scene.render_data
		
		conn = clientConnection(netsettings.server_address, netsettings.server_port)
		
		if conn:
			job = bpy.netrender_jobs[netsettings.active_job_index]
			
			for frame in job.frames:
				client.requestResult(conn, job.id, frame.number)
				response = conn.getresponse()
		
				if response.status != http.client.OK:
					print("missing", frame.number)
					continue
				
				print("got back", frame.number)
				
				f = open(netsettings.path + "%06d" % frame.number + ".exr", "wb")
				buf = response.read(1024)
				
				while buf:
					f.write(buf)
					buf = response.read(1024)
				
				f.close()
			
			conn.close()
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class netclientscan(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	bl_idname = "render.netclientscan"
	bl_label = "Client Scan"
	
	def poll(self, context):
		return True
		
	def execute(self, context):
		address, port = clientScan()

		if address:
			scene = context.scene
			netsettings = scene.network_render
			netsettings.server_address = address
			netsettings.server_port = port
		
		return ('FINISHED',)

	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class netclientweb(bpy.types.Operator):
	'''Open new window with information about running rendering jobs'''
	bl_idname = "render.netclientweb"
	bl_label = "Open Master Monitor"
	
	def poll(self, context):
		return True
		
	def execute(self, context):
		netsettings = context.scene.network_render
		
		
		# open connection to make sure server exists
		conn = clientConnection(netsettings.server_address, netsettings.server_port)
		
		if conn:
			conn.close()
			
			webbrowser.open("http://%s:%i" % (netsettings.server_address, netsettings.server_port))
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)
