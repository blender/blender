class TaskManager:
    def __init__(self):
        self._tasks = []
        self.taskIndex = 0
        
    def appendTask(self, task):
        self.tasks.append(task)
        
    def appendTasks(self, *tasks):
        self._tasks.extend(tasks)
        
    def execute(self, event):
        if self.isFinished:
            return "FINISHED"
            
        task = self._tasks[self.taskIndex]
        status = task.execute(event)
        if status == "FINISHED":
            self.taskIndex += 1
            
        return "FINISHED" if self.isFinished else "CONTINUE"
            
    @property
    def isFinished(self):
        return self.taskIndex >= len(self._tasks)
        
    @property
    def nextDescription(self):
        for task in self._tasks[self.taskIndex:]:
            if task.description != "":
                return task.description
        return ""
        
    @property
    def percentage(self):
        totalTimeWeigth = self.getTotalTimeWeight()
        if totalTimeWeigth == 0: return 0
        return self.getTimeWeight(end = self.taskIndex) / self.getTotalTimeWeight()
        
    def getTotalTimeWeight(self):
        return self.getTimeWeight(end = len(self._tasks))
        
    def getTimeWeight(self, start = 0, end = 0):
        weight = 0
        for task in self._tasks[start:end]:
            weight += task.timeWeight
        return weight
         
         
class Task:
    def execute(self, event):
        return "FINISHED"
    def __getattr__(self, name):
        if name == "description":
            return ""
        if name == "timeWeight":
            return 1