import bpy
import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

VERSION = b"0.3"

PATH_PREFIX = "/tmp/"

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
	
	if anim:
		job_frame = "%i:%i" % (scene.start_frame, scene.end_frame)
	else:
		job_frame = "%i" % (scene.current_frame, )
		
	blacklist = []
	
	filename = bpy.data.filename
	
	name = scene.network_render.job_name
	
	if name == "[default]":
		path, name = os.path.split(filename)
	
	for slave in scene.network_render.slaves_blacklist:
		blacklist.append(slave.id)
		
	blacklist = " ".join(blacklist)
	
	headers = {"job-frame":job_frame, "job-name":name, "job-chunks": str(chunks), "slave-blacklist": blacklist}
	
	# try to send path first
	conn.request("POST", "job", filename, headers=headers)
	response = conn.getresponse()
	
	# if not found, send whole file
	if response.status == http.client.NOT_FOUND:
		f = open(bpy.data.filename, "rb")
		conn.request("PUT", "file", f, headers=headers)
		f.close()
		response = conn.getresponse()
	
	return response.getheader("job-id")

def clientRequestResult(conn, scene, job_id):
	conn.request("GET", "render", headers={"job-id": job_id, "job-frame":str(scene.current_frame)})
