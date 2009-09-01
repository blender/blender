import bpy
import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

import netrender.model

VERSION = b"0.3"

QUEUED = 0
DISPATCHED = 1
DONE = 2
ERROR = 3

def clientConnection(scene):
		netrender = scene.network_render
		
		conn = http.client.HTTPConnection(netrender.server_address, netrender.server_port)
		
		if clientVerifyVersion(conn):
			return conn
		else:
			conn.close()
			return None

def clientVerifyVersion(conn):
	conn.request("GET", "version")
	response = conn.getresponse()
	
	if response.status != http.client.OK:
		conn.close()
		return False
	
	server_version = response.read()
	
	if server_version != VERSION:
		print("Incorrect server version!")
		print("expected", VERSION, "received", server_version)
		return False
	
	return True

def clientSendJob(conn, scene, anim = False, chunks = 5):
	netsettings = scene.network_render
	job = netrender.model.RenderJob()
	
	if anim:
		for f in range(scene.start_frame, scene.end_frame + 1):
			job.addFrame(f)
	else:
		job.addFrame(scene.current_frame)
	
	filename = bpy.data.filename
	job.files.append(filename)
	
	name = netsettings.job_name
	if name == "[default]":
		path, name = os.path.split(filename)
	
	job.name = name
	
	for slave in scene.network_render.slaves_blacklist:
		job.blacklist.append(slave.id)
	
	job.chunks = netsettings.chunks
	job.priority = netsettings.priority
	
	# try to send path first
	conn.request("POST", "job", repr(job.serialize()))
	response = conn.getresponse()
	
	job_id = response.getheader("job-id")
	
	# if not found, send whole file
	if response.status == http.client.NOT_FOUND:
		f = open(bpy.data.filename, "rb")
		conn.request("PUT", "file", f, headers={"job-id": job_id})
		f.close()
		response = conn.getresponse()
	
	# server will reply with NOT_FOUD until all files are found
	
	return job_id

def clientRequestResult(conn, scene, job_id):
	conn.request("GET", "render", headers={"job-id": job_id, "job-frame":str(scene.current_frame)})
