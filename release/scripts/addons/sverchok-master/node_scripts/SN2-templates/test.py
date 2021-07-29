
class MyScript( SvScript):
    inputs = [("s" , "List")]
    outputs = [("s", "Sum")]
    
    def process(self):
        data = self.node.inputs[0].sv_get()
        out = []
        for obj in data:
            out.append(sum(obj))
            
        self.node.outputs[0].sv_set(out)
