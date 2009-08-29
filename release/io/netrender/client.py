import bpy
import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender.slave as slave
import netrender.master as master
from netrender.utils import *

class NetworkRenderEngine(bpy.types.RenderEngine):
	__idname__ = 'NET_RENDER'
	__label__ = "Network Render"
	def render(self, scene):
		if scene.network_render.mode == "RENDER_CLIENT":
			self.render_client(scene)
		elif scene.network_render.mode == "RENDER_SLAVE":
			self.render_slave(scene)
		elif scene.network_render.mode == "RENDER_MASTER":
			self.render_master(scene)
		else:
			print("UNKNOWN OPERATION MODE")
	
	def render_master(self, scene):
		server_address = (scene.network_render.server_address, scene.network_render.server_port)
		httpd = master.RenderMasterServer(server_address, master.RenderHandler)
		httpd.timeout = 1
		httpd.stats = self.update_stats
		while not self.test_break():
			httpd.handle_request()

	def render_slave(self, scene):
		slave.render_slave(self, scene)
	
	def render_client(self, scene):
		self.update_stats("", "Network render client initiation")
		
		conn = clientConnection(scene)
		
		if conn:
			# Sending file
			
			self.update_stats("", "Network render exporting")
			
			job_id = scene.network_render.job_id
			
			# reading back result
			
			self.update_stats("", "Network render waiting for results")
			
			clientRequestResult(conn, scene, job_id)
			response = conn.getresponse()
			
			if response.status == http.client.NO_CONTENT:
				scene.network_render.job_id = clientSendJob(conn, scene)
				clientRequestResult(conn, scene, job_id)
			
			while response.status == http.client.PROCESSING and not self.test_break():
				print("waiting")
				time.sleep(1)
				clientRequestResult(conn, scene, job_id)
				response = conn.getresponse()
	
			if response.status != http.client.OK:
				conn.close()
				return
			
			r = scene.render_data
			x= int(r.resolution_x*r.resolution_percentage*0.01)
			y= int(r.resolution_y*r.resolution_percentage*0.01)
			
			f = open(PATH_PREFIX + "output.exr", "wb")
			buf = response.read(1024)
			
			while buf:
				f.write(buf)
				buf = response.read(1024)
			
			f.close()
			
			result = self.begin_result(0, 0, x, y)
			result.load_from_file(PATH_PREFIX + "output.exr", 0, 0)
			self.end_result(result)
			
			conn.close()

bpy.types.register(NetworkRenderEngine)

