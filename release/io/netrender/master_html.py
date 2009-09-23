import re

from netrender.utils import *


def get(handler):
	def output(text):
		handler.wfile.write(bytes(text, encoding='utf8'))
	
	def link(text, url):
		return "<a href='%s'>%s</a>" % (url, text)
	
	def startTable(border=1):
		output("<table border='%i'>" % border)
	
	def headerTable(*headers):
		output("<thead><tr>")
		
		for c in headers:
			output("<td>" + c + "</td>")
		
		output("</tr></thead>")
	
	def rowTable(*data):
		output("<tr>")
		
		for c in data:
			output("<td>" + str(c) + "</td>")
		
		output("</tr>")
	
	def endTable():
		output("</table>")
	
	handler.send_head(content = "text/html")
	
	if handler.path == "/html" or handler.path == "/":
		output("<html><head><title>NetRender</title></head><body>")
	
		output("<h2>Master</h2>")
	
		output("<h2>Slaves</h2>")
		
		startTable()
		headerTable("id", "name", "address", "stats")
		
		for slave in handler.server.slaves:
			rowTable(slave.id, slave.name, slave.address[0], slave.stats)
		
		endTable()
		
		output("<h2>Jobs</h2>")
		
		startTable()
		headerTable("id", "name", "credits", "time since last", "length", "done", "dispatched", "error", "priority", "exception")

		handler.server.update()
		
		for job in handler.server.jobs:
			results = job.framesStatus()
			rowTable(link(job.id, "/html/job" + job.id), job.name, round(job.credits, 1), int(time.time() - job.last_dispatched), len(job), results[DONE], results[DISPATCHED], results[ERROR], handler.server.balancer.applyPriorities(job), handler.server.balancer.applyExceptions(job))
		
		endTable()
		
		output("</body></html>")
	
	elif handler.path.startswith("/html/job"):
		job_id = handler.path[9:]
		
		output("<html><head><title>NetRender</title></head><body>")
	
		job = handler.server.getJobByID(job_id)
		
		if job:
			output("<h2>Frames</h2>")
		
			startTable()
			headerTable("no", "status", "render time", "slave", "log")
			
			for frame in job.frames:
				rowTable(frame.number, frame.statusText(), "%.1fs" % frame.time, frame.slave.name if frame.slave else "&nbsp;", link("view log", "/html/log%s_%i" % (job_id, frame.number)) if frame.log_path else "&nbsp;")
			
			endTable()
		else:
			output("no such job")
		
		output("</body></html>")
	
	elif handler.path.startswith("/html/log"):
		pattern = re.compile("([a-zA-Z0-9]+)_([0-9]+)")
		
		output("<html><head><title>NetRender</title></head><body>")
		
		match = pattern.match(handler.path[9:])
		if match:
			job_id = match.groups()[0]
			frame_number = int(match.groups()[1])
			
			job = handler.server.getJobByID(job_id)
			
			if job:
				frame = job[frame_number]
				
				if frame:
						f = open(frame.log_path, 'rb')
						
						output("<pre>")
						
						shutil.copyfileobj(f, handler.wfile)
						
						output("</pre>")
						
						f.close()
				else:
					output("no such frame")
			else:
				output("no such job")
		else:
			output("malformed url")
		
		output("</body></html>")
