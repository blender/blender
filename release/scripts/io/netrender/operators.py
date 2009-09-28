import bpy
import sys, os
import http, http.client, http.server, urllib, socket
import webbrowser

from netrender.utils import *
import netrender.client as client
import netrender.model

@rnaOperator
class RENDER_OT_netclientanim(bpy.types.Operator):
	'''
	Operator documentation text, will be used for the operator tooltip and python docs.
	'''
	__idname__ = "render.netclientanim"
	__label__ = "Net Render Client Anim"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		scene = context.scene
		
		conn = clientConnection(scene)
		
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
	'''
	Operator documentation text, will be used for the operator tooltip and python docs.
	'''
	__idname__ = "render.netclientsend"
	__label__ = "Net Render Client Send"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		scene = context.scene
		
		conn = clientConnection(scene)
		
		if conn:
			# Sending file
			scene.network_render.job_id = client.clientSendJob(conn, scene, True)
			conn.close()
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class RENDER_OT_netclientstatus(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientstatus"
	__label__ = "Net Render Client Status"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(context.scene)

		if conn:
			conn.request("GET", "/status")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			jobs = (netrender.model.RenderJob.materialize(j) for j in eval(str(response.read(), encoding='utf8')))
			
			while(len(netsettings.jobs) > 0):
				netsettings.jobs.remove(0)
			
			bpy.data.netrender_jobs = []
			
			for j in jobs:
				bpy.data.netrender_jobs.append(j)
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
	__idname__ = "render.netclientblacklistslave"
	__label__ = "Net Render Client Blacklist Slave"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		
		if netsettings.active_slave_index >= 0:
			
			# deal with data
			slave = bpy.data.netrender_slaves.pop(netsettings.active_slave_index)
			bpy.data.netrender_blacklist.append(slave)
			
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
	__idname__ = "render.netclientwhitelistslave"
	__label__ = "Net Render Client Whitelist Slave"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		
		if netsettings.active_blacklisted_slave_index >= 0:
			
			# deal with data
			slave = bpy.data.netrender_blacklist.pop(netsettings.active_blacklisted_slave_index)
			bpy.data.netrender_slaves.append(slave)
			
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
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientslaves"
	__label__ = "Net Render Client Slaves"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
	
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(context.scene)
		
		if conn:
			conn.request("GET", "/slaves")
			
			response = conn.getresponse()
			print( response.status, response.reason )
			
			slaves = (netrender.model.RenderSlave.materialize(s) for s in eval(str(response.read(), encoding='utf8')))
			
			while(len(netsettings.slaves) > 0):
				netsettings.slaves.remove(0)
			
			bpy.data.netrender_slaves = []
			
			for s in slaves:
				for i in range(len(bpy.data.netrender_blacklist)):
					slave = bpy.data.netrender_blacklist[i]
					if slave.id == s.id:
						bpy.data.netrender_blacklist[i] = s
						netsettings.slaves_blacklist[i].name = s.name
						break
				else:
					bpy.data.netrender_slaves.append(s)
					
					netsettings.slaves.add()
					slave = netsettings.slaves[-1]
					slave.name = s.name
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class RENDER_OT_netclientcancel(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientcancel"
	__label__ = "Net Render Client Cancel"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		netsettings = context.scene.network_render
		return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0
		
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(context.scene)
		
		if conn:
			job = bpy.data.netrender_jobs[netsettings.active_job_index]
			
			conn.request("POST", "/cancel", headers={"job-id":job.id})
			
			response = conn.getresponse()
			print( response.status, response.reason )

			netsettings.jobs.remove(netsettings.active_job_index)
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)
	
@rnaOperator
class RENDER_OT_netclientcancelall(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientcancelall"
	__label__ = "Net Render Client Cancel All"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
		
	def execute(self, context):
		netsettings = context.scene.network_render
		conn = clientConnection(context.scene)
		
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
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientdownload"
	__label__ = "Net Render Client Download"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		netsettings = context.scene.network_render
		return netsettings.active_job_index >= 0 and len(netsettings.jobs) > 0
		
	def execute(self, context):
		netsettings = context.scene.network_render
		rd = context.scene.render_data
		
		conn = clientConnection(context.scene)
		
		if conn:
			job = bpy.data.netrender_jobs[netsettings.active_job_index]
			
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
	__idname__ = "render.netclientscan"
	__label__ = "Net Render Client Scan"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
		
	def execute(self, context):
		netsettings = context.scene.network_render
		
		try:
			s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
			s.settimeout(30)
	
			s.bind(('', 8000))
			
			buf, address = s.recvfrom(64)
			
			print("received:", buf)
			
			netsettings.server_address = address[0]
			netsettings.server_port = int(str(buf, encoding='utf8'))
		except socket.timeout:
			print("no server info")
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)

@rnaOperator
class netclientweb(bpy.types.Operator):
	'''Operator documentation text, will be used for the operator tooltip and python docs.'''
	__idname__ = "render.netclientweb"
	__label__ = "Net Render Client Web"
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.
	
	__props__ = []
	
	def poll(self, context):
		return True
		
	def execute(self, context):
		netsettings = context.scene.network_render
		
		
		# open connection to make sure server exists
		conn = clientConnection(context.scene)
		
		if conn:
			conn.close()
			
			webbrowser.open("http://%s:%i" % (netsettings.server_address, netsettings.server_port))
		
		return ('FINISHED',)
	
	def invoke(self, context, event):
		return self.execute(context)
