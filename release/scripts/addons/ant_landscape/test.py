from numpy import *

if __name__ == '__main__':
    # This only works when this script is loaded as main, or
    # run directly from the ant_landscape directory.
    from stats import Stats

    stats = Stats()

    a = zeros(10000000)
    print(stats.time())
    print(stats.memory())
    a = sin(a)
    print(stats.time())
    print(stats.memory())
    a = cos(a)
    print(stats.time())
    print(stats.memory())
    a = cos(a)**2+sin(a)**2
    print(stats.time())
    print(stats.memory())
