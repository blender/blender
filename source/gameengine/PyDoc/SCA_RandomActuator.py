# $Id$
# Documentation for SCA_RandomActuator
from SCA_IActuator import *

class SCA_RandomActuator(SCA_IActuator):
	"""
	Random Actuator
	"""
	def setSeed(seed):
		"""
		Sets the seed of the random number generator.
		
		Equal seeds produce equal series. If the seed is 0, 
		the generator will produce the same value on every call.
		
		@type seed: integer
		"""
	def getSeed():
		"""
		Returns the initial seed of the generator.
		
		@rtype: integer
		"""
	def getPara1():
		"""
		Returns the first parameter of the active distribution. 
		
		Refer to the documentation of the generator types for the meaning
		of this value.
		
		@rtype: float
		"""
	def getPara2():
		"""
		Returns the second parameter of the active distribution. 
		
		Refer to the documentation of the generator types for the meaning
		of this value.
		
		@rtype: float
		"""
	def getDistribution():
		"""
		Returns the type of random distribution.
		
		@rtype: distribution type
		@return: KX_RANDOMACT_BOOL_CONST, KX_RANDOMACT_BOOL_UNIFORM, KX_RANDOMACT_BOOL_BERNOUILLI,
		        KX_RANDOMACT_INT_CONST, KX_RANDOMACT_INT_UNIFORM, KX_RANDOMACT_INT_POISSON, 
		        KX_RANDOMACT_FLOAT_CONST, KX_RANDOMACT_FLOAT_UNIFORM, KX_RANDOMACT_FLOAT_NORMAL,
		        KX_RANDOMACT_FLOAT_NEGATIVE_EXPONENTIAL
		"""
	def setProperty(property):
		"""
		Set the property to which the random value is assigned. 
		
		If the generator and property types do not match, the assignment is ignored.
		
		@type property: string
		@param property: The name of the property to set.
		"""
	def getProperty():
		"""
		Returns the name of the property to set.
		
		@rtype: string
		"""
	def setBoolConst(value):
		"""
		Sets this generator to produce a constant boolean value.
		
		@param value: The value to return.
		@type value: boolean
		"""
	def setBoolUniform():
		"""
		Sets this generator to produce a uniform boolean distribution.
		
		The generator will generate True or False with 50% chance.
		"""
	def setBoolBernouilli(value):
		"""
		Sets this generator to produce a Bernouilli distribution.
		
		@param value: Specifies the proportion of False values to produce.
				- 0.0: Always generate True
				- 1.0: Always generate False
		@type value: float
		"""
	def setIntConst(value):
		"""
		Sets this generator to always produce the given value.
		
		@param value: the value this generator produces.
		@type value: integer
		"""
	def setIntUniform(lower_bound, upper_bound):
		"""
		Sets this generator to produce a random value between the given lower and
		upper bounds (inclusive).
		
		@type lower_bound: integer
		@type upper_bound: integer
		"""
	def setIntPoisson(value):
		"""
		Generate a Poisson-distributed number. 
		
		This performs a series of Bernouilli tests with parameter value. 
		It returns the number of tries needed to achieve succes.
		
		@type value: float
		"""
	def setFloatConst(value):
		"""
		Always generate the given value.
		
		@type value: float
		"""
	def setFloatUniform(lower_bound, upper_bound):
		"""
		Generates a random float between lower_bound and upper_bound with a
		uniform distribution.
		
		@type lower_bound: float
		@type upper_bound: float
		"""
	def setFloatNormal(mean, standard_deviation):
		"""
		Generates a random float from the given normal distribution.
		
		@type mean: float
		@param mean: The mean (average) value of the generated numbers
		@type standard_deviation: float
		@param standard_deviation: The standard deviation of the generated numbers.
		"""
	def setFloatNegativeExponential(half_life):
		"""
		Generate negative-exponentially distributed numbers. 
		
		The half-life 'time' is characterized by half_life.
		
		@type half_life: float
		"""
