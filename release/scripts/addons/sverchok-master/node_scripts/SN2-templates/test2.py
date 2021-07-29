# simple demo script for atomic map
class MyAdd(SvScriptAuto):   
    @staticmethod
    def function(*args):
        return sum(args)
    
    name = "My own add function"
            
    inputs = [("s","A",0),
              ("s","B",0)]
 
    outputs = [("s","C")]
 
