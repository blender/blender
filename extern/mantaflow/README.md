# Mantaflow #

Mantaflow is an open-source framework targeted at fluid simulation research in Computer Graphics.
Its parallelized C++ solver core, python scene definition interface and plugin system allow for quickly prototyping and testing new algorithms.

In addition, it provides a toolbox of examples for deep learning experiments with fluids. E.g., it contains examples
how to build convolutional neural network setups in conjunction with the [tensorflow framework](https://www.tensorflow.org).

For more information on how to install, run and code with Mantaflow, please head over to our home page at
[http://mantaflow.com](http://mantaflow.com)

## Debugging ##

You could export openVDB volume into mantaflow, by running Blender with:

    blender --debug-value 3001

And then select `Domain` -> `Fluid` -> `Cache` - > `Advanced` -> `Export Mantaflow Script`.
