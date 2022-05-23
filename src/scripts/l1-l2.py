#!/usr/bin/python3
import matplotlib.pyplot as plt
import numpy as np
from scipy.stats import gaussian_kde
import struct
import os, sys

def p32(x):
    return struct.pack("<I",x)

def u64(x):
    return struct.unpack("<Q", x)[0]

def mean(arr):
    return sum(arr)/len(arr)

def parse_file(file_name):
    with open(file_name, "rb") as f:
        data = f.read()
    ofs = 0
    # first read l2
    l2_time = []
    l1_time = []
    for i in range(REPS):
        l2_time.append(u64(data[ofs:ofs+8]))
        ofs += 8
    # second read l1
    for i in range(REPS):
        l1_time.append(u64(data[ofs:ofs+8]))
        ofs += 8
    return (l1_time, l2_time)

if __name__ == "__main__":
    REPS = 0x1000
    l1,l2 = parse_file(sys.argv[1])
    kde1 = gaussian_kde(l1)
    kde2 = gaussian_kde(l2)
    xx = np.linspace(0, 200, 10000)
    plt.figure(1)
    plt.hist(l1, bins=np.linspace(0,200,200), color='red')
    plt.savefig("L1.png")

    plt.figure(2)
    plt.hist(l2, bins=np.linspace(0,200,200), color='green')
    plt.savefig("L2.png")

    print("L1: {}".format(mean(l1)))
    print("L2: {}".format(mean(l2)))