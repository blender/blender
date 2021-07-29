
class MultiInputDemo(SvMultiInput):
    
    inputs  = [
        ['s', 'Data 0'],
    ]
    
    multi_socket_type = "s"
    base_name = "Data {}"
    
    outputs = [("s", "Data")]
   
    @staticmethod
    def function(args):
        return [[len(arg) for arg in args]]
