
import numpy as np
from math import cos, pi, exp
import os

# Функция в начальный момент времени
def function(x, y, z):
    return cos(x*pi)*cos(y*pi)*cos(z*pi)

# Аналитическое решение в момент времени t
def expected(x, y, z, t):
    return exp(-3*pi*pi*t)*cos(x*pi)*cos(y*pi)*cos(z*pi)

def main():
    pathSetting = os.path.join(os.pardir, "initial", "setting3.ini")
    with open(pathSetting) as file:
        setting = {line.split('=')[0] : float(line.split('=')[1]) for line in file}

    xStart = setting['XSTART']
    xEnd = setting['XEND']
    NX = setting['NX']

    yStart = setting['YSTART']
    yEnd = setting['YEND']
    NY = setting['NY']

    zStart = setting['ZSTART']
    zEnd = setting['ZEND']
    NZ = setting['NZ']

    t = setting['TFINISH'] - setting['TSTART']

    X = np.linspace(xStart, xEnd, NX, dtype = float)
    Y = np.linspace(yStart, yEnd, NY, dtype = float)
    Z = np.linspace(zStart, zEnd, NZ, dtype = float)

    U = np.array([function(x, y, z) for z in Z for y in Y for x in X])
    expect = np.array([expected(x, y, z, t) for z in Z for y in Y for x in X])

    pathOutputU = os.path.join(os.pardir, "initial", 'function3_8.txt')
    #pathOutputExp = os.path.join(os.pardir, "initial", 'expected3.txt')

    np.savetxt(pathOutputU, U, fmt='%.15e',delimiter='\n')
    #np.savetxt(pathOutputExp, expect, fmt='%.15e',delimiter='\n')

if __name__ == "__main__":
    print("Start")
    main()
    print("Finish")
