cdef class Interpolation:
    def __cinit__(self):
        self.clamped = False

    def __call__(self, double x):
        x = min(max(x, 0), 1)
        return self.evaluate(x)

    cdef double evaluate(self, double x):
        raise NotImplementedError()

    cdef double derivative(self, double x):
        cdef double width = 0.01
        if x > width:
            return (self.evaluate(x) - self.evaluate(x - width)) / width
        else:
            return (self.evaluate(x + width) - self.evaluate(x)) / width

    def __repr__(self):
        return "<Interpolation {}>".format(repr(self.__class__.__name__))

    def evaluateList(self, DoubleList positions not None):
        cdef:
            DoubleList output = DoubleList(length = len(positions))
            double x
            int i

        for i in range(len(output)):
            x = positions.data[i]
            if x < 0: output.data[i] = self.evaluate(0)
            elif x > 1: output.data[i] = self.evaluate(1)
            else: output.data[i] = self.evaluate(x)

        return output

    def sample(self, int amount, double minValue = 0, double maxValue = 1):
        if amount < 0:
            raise Exception("amount has to be greater than zero")

        cdef:
            DoubleList output = DoubleList(length = amount)
            double amplitude = maxValue - minValue
            double factor
            int i

        if amount >= 2: factor = 1 / (<double>amount - 1)
        else: factor = 0.5

        for i in range(amount):
            output.data[i] = self.evaluate(i * factor) * amplitude + minValue

        return output
