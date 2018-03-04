import os
import re
import sys
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

def main():
    if len(sys.argv) != 3:
        print("Не верное количество параметров! Укажите расположение "
              "двух файлов относительно папки result")
        print("Например: fault.py Kirill/euler.txt Kirill/implicit.txt")
        return

    settingPath = os.path.join(os.path.pardir, "initial", "setting.ini")
    with open(settingPath, 'r') as file:
        pattern = re.compile('[A-Za-z]+=-?\d+')
        setting = { line.split('=')[0] : float(line.split('=')[1])
                    for line in file if pattern.match(line) }

    xStart = setting['XSTART']
    xFinish = setting['XEND']
    NX = setting['NX']

    x = np.linspace(xStart, xFinish, NX)

    pathRes1 = os.path.join(os.pardir, "result", sys.argv[1])
    pathRes2 = os.path.join(os.pardir, "result", sys.argv[2])

    try:
        res1 = np.loadtxt(pathRes1)
        res2 = np.loadtxt(pathRes2)

        assert len(res1) == len(res2)

        yAbs = [xi-xj for xi, xj in zip(res1, res2)]
        yRelat = [(xi-xj)/max(xi, xj) for xi, xj in zip(res1, res2)]

        np.savetxt('test.txt',yAbs, fmt='%.15e')
        absoluteFault = max(map(abs,yAbs))
        relativeFault = max(map(abs,yRelat))
        print("абсолютная:\t%.15f" % absoluteFault)
        print("относительная:\t%.15f" % relativeFault)

        ####################################################################
        #                    Рисование графиков                            #
        ####################################################################

        # assert len(x) == len(yAbs)
        #
        # plt.figure(num = 'FAULT', facecolor = (1, 1, .54))
        #
        # plt.subplot(221)
        # plt.plot(x, yAbs, label ='absolut', color = 'green')
        #
        # plt.legend(loc = 2)
        # plt.xlabel('x', fontsize = 14)
        # plt.grid(True)
        # ax = plt.gca()
        # ax.yaxis.set_major_formatter(FuncFormatter(lambda x,y: '{:.0e}'.format(x)))
        # # plt.yscale('log')
        #
        # plt.subplot(222)
        # plt.plot(x, yRelat, label = 'relative', color = 'red')
        # plt.legend(loc = 2)
        # plt.xlabel('x', fontsize=14)
        # plt.grid(True)
        #
        # plt.subplot(223)
        # plt.plot(x, res1, label = sys.argv[1], color = 'blue')
        # plt.legend(loc = 2)
        # plt.xlabel('x', fontsize = 14)
        # plt.grid(True)
        #
        # plt.subplot(224)
        # plt.plot(x, res2, label = sys.argv[2], color = 'brown')
        # plt.legend(loc = 2)
        # plt.xlabel('x', fontsize = 14)
        # plt.grid(True)
        #
        # plt.show()
    except AssertionError:
        print ('ERROR! Не совпадают размерности! ' + str(len(res1)) + "!=" + str(len(res2)))


if __name__ == '__main__':
    main()
