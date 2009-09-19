import time

from netrender.utils import *
import netrender.model

class RatingRule:
	def rate(self, job):
		return 0

class ExclusionRule:
	def test(self, job):
		return False

class PriorityRule:
	def test(self, job):
		return False

class Balancer:
	def __init__(self):
		self.rules = []
		self.priorities = []
		self.exceptions = []
	
	def addRule(self, rule):
		self.rules.append(rule)
	
	def addPriority(self, priority):
		self.priorities.append(priority)
		
	def addException(self, exception):
		self.exceptions.append(exception)
	
	def applyRules(self, job):
		return sum((rule.rate(job) for rule in self.rules))
	
	def applyPriorities(self, job):
		for priority in self.priorities:
			if priority.test(job):
				return True # priorities are first
		
		return False
	
	def applyExceptions(self, job):
		for exception in self.exceptions:
			if exception.test(job):
				return True # exceptions are last
			
		return False
	
	def sortKey(self, job):
		return (1 if self.applyExceptions(job) else 0, # exceptions after
						0 if self.applyPriorities(job) else 1, # priorities first
						self.applyRules(job))
	
	def balance(self, jobs):
		if jobs:
			jobs.sort(key=self.sortKey)
			return jobs[0]
		else:
			return None
	
# ==========================


class RatingCredit(RatingRule):
	def rate(self, job):
		return -job.credits # more credit is better (sort at first in list)

class NewJobPriority(PriorityRule):
	def test(self, job):
		return job.countFrames(status = DISPATCHED) == 0

class ExcludeQueuedEmptyJob(ExclusionRule):
	def test(self, job):
		return job.status != JOB_QUEUED or job.countFrames(status = QUEUED) == 0
