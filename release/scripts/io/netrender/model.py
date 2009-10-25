import sys, os
import http, http.client, http.server, urllib
import subprocess, shutil, time, hashlib

from netrender.utils import *

class LogFile:
	def __init__(self, job_id = 0, frames = []):
		self.job_id = job_id
		self.frames = frames
	
	def serialize(self):
		return 	{
							"job_id": self.job_id,
							"frames": self.frames
						}
	
	@staticmethod
	def materialize(data):
		if not data:
			return None
		
		logfile = LogFile()
		logfile.job_id = data["job_id"]
		logfile.frames = data["frames"]
		
		return logfile

class RenderSlave:
	_slave_map = {}
	
	def __init__(self):
		self.id = ""
		self.name = ""
		self.address = ("",0)
		self.stats = ""
		self.total_done = 0
		self.total_error = 0
		self.last_seen = 0.0
		
	def serialize(self):
		return 	{
							"id": self.id,
							"name": self.name,
							"address": self.address,
							"stats": self.stats,
							"total_done": self.total_done,
							"total_error": self.total_error,
							"last_seen": self.last_seen
						}
	
	@staticmethod
	def materialize(data):
		if not data:
			return None
		
		slave_id = data["id"]
		
		if slave_id in RenderSlave._slave_map:
			return RenderSlave._slave_map[slave_id]
		else:
			slave = RenderSlave()
			slave.id = slave_id
			slave.name = data["name"]
			slave.address = data["address"]
			slave.stats = data["stats"]
			slave.total_done = data["total_done"]
			slave.total_error = data["total_error"]
			slave.last_seen = data["last_seen"]
			
			RenderSlave._slave_map[slave_id] = slave
			
			return slave

JOB_BLENDER = 1
JOB_PROCESS = 2

JOB_TYPES = {
							JOB_BLENDER: "Blender",
							JOB_PROCESS: "Process"
						}

class RenderJob:
	def __init__(self):
		self.id = ""
		self.type = JOB_BLENDER
		self.name = ""
		self.files = []
		self.frames = []
		self.chunks = 0
		self.priority = 0
		self.usage = 0.0
		self.blacklist = []
		self.last_dispatched = 0.0
	
	def addFile(self, file_path, start=-1, end=-1):
		self.files.append((file_path, start, end))
	
	def addFrame(self, frame_number, command = ""):
		frame = RenderFrame(frame_number, command)
		self.frames.append(frame)
		return frame
	
	def __len__(self):
		return len(self.frames)
	
	def countFrames(self, status=QUEUED):
		total = 0
		for f in self.frames:
			if f.status == status:
				total += 1
		
		return total
	
	def countSlaves(self):
		return len(set((frame.slave for frame in self.frames if frame.status == DISPATCHED)))
	
	def framesStatus(self):
		results = {
								QUEUED: 0,
								DISPATCHED: 0,
								DONE: 0,
								ERROR: 0
							}
		
		for frame in self.frames:
			results[frame.status] += 1
			
		return results
	
	def __contains__(self, frame_number):
		for f in self.frames:
			if f.number == frame_number:
				return True
		else:
			return False
	
	def __getitem__(self, frame_number):
		for f in self.frames:
			if f.number == frame_number:
				return f
		else:
			return None
		
	def serialize(self, frames = None):
		min_frame = min((f.number for f in frames)) if frames else -1
		max_frame = max((f.number for f in frames)) if frames else -1
		return 	{
							"id": self.id,
							"type": self.type,
							"name": self.name,
							"files": [f for f in self.files if f[1] == -1 or not frames or (f[1] <= max_frame and f[2] >= min_frame)],
							"frames": [f.serialize() for f in self.frames if not frames or f in frames],
							"chunks": self.chunks,
							"priority": self.priority,
							"usage": self.usage,
							"blacklist": self.blacklist,
							"last_dispatched": self.last_dispatched
						}

	@staticmethod
	def materialize(data):
		if not data:
			return None
		
		job = RenderJob()
		job.id = data["id"]
		job.type = data["type"]
		job.name = data["name"]
		job.files = data["files"]
		job.frames = [RenderFrame.materialize(f) for f in data["frames"]]
		job.chunks = data["chunks"]
		job.priority = data["priority"]
		job.usage = data["usage"]
		job.blacklist = data["blacklist"]
		job.last_dispatched = data["last_dispatched"]

		return job

class RenderFrame:
	def __init__(self, number = 0, command = ""):
		self.number = number
		self.time = 0
		self.status = QUEUED
		self.slave = None
		self.command = command

	def statusText(self):
		return STATUS_TEXT[self.status]

	def serialize(self):
		return 	{
							"number": self.number,
							"time": self.time,
							"status": self.status,
							"slave": None if not self.slave else self.slave.serialize(),
							"command": self.command
						}
						
	@staticmethod
	def materialize(data):
		if not data:
			return None
		
		frame = RenderFrame()
		frame.number = data["number"]
		frame.time = data["time"]
		frame.status = data["status"]
		frame.slave = RenderSlave.materialize(data["slave"])
		frame.command = data["command"]

		return frame
