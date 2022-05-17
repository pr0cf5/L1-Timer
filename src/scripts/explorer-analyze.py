#!/usr/bin/python3
import matplotlib.pyplot as plt
from scipy.stats import gaussian_kde
import numpy as np
import struct
import os, sys

def u64(x):
    return struct.unpack("<Q", x)[0]

def parse_file(file_name):
    with open(file_name, "rb") as f:
        rawdata = f.read()
    data = {}
    ofs = 0
    for j in range(L1_ASSOC):
        temp = []
        for i in range(REPS):
            temp.append(u64(rawdata[ofs:ofs+8]))
            ofs += 8
        data[j] = temp
    return data

def mean(dist):
    return sum(dist)/len(dist)

if __name__ == "__main__":
    REPS = 0x1000
    L1_ASSOC = 8
    data = parse_file(sys.argv[1])
    kdes = []
    for i in range(L1_ASSOC):
        kdes.append(gaussian_kde(data[i]))
    for i in range(L1_ASSOC):
        xx = np.linspace(0,60,REPS)
        plt.figure(i)
        plt.plot(xx, kdes[i](xx))
        plt.savefig("Index{}".format(i))
    