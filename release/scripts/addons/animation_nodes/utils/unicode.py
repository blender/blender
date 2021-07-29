# only use these two functions together
def toValidString(text):
    return str(text.encode())

def fromValidString(text):
    return eval(text).decode()
