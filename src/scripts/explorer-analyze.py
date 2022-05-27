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
    for i in range(L1_ASSOC):
        data[i] = []
    ofs = 0
    for j in range(REPS):
        for i in range(L1_ASSOC):
            data[i].append(u64(rawdata[ofs:ofs+8]))
            ofs += 8
    return data

def mean(dist):
    return sum(dist)/len(dist)

if __name__ == "__main__":
    REPS = 0x100
    L1_ASSOC = 8
    data = parse_file(sys.argv[1])
    kdes = []
    for i in range(L1_ASSOC):
        plt.figure(i)
        plt.hist(data[i], bins=np.linspace(0,10,10))
        plt.savefig("Index{}".format(i))
    
    for i in range(L1_ASSOC):
        print("index{}: {}".format(i, mean(data[i])))