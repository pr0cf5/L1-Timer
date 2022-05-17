#!/usr/bin/python3
import matplotlib.pyplot as plt
from scipy.stats import gaussian_kde
import numpy as np
import struct
import os, sys

def u64(x):
    return struct.unpack("<Q", x)[0]

def measure():
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    results = {}
    offset = 0
    for i in range(EVSET_MAX_SZ):
        dist_t1 = []
        dist_t2 = []
        for j in range(REPS):
            t1,t2 = u64(data[offset:offset+8]), u64(data[offset+8:offset+16])
            dist_t1.append(t1)
            dist_t2.append(t2)
            offset += 16
        results[i] = (dist_t1,dist_t2)
    return results

def mean(dist):
    return sum(dist)/len(dist)

if __name__ == "__main__":
    REPS = 0x100
    EVSET_MAX_SZ = 0x40
    res = measure()
    yy1 = []
    yy2 = []
    for i in range(EVSET_MAX_SZ):
        t1d, t2d = res[i]
        yy1.append(mean(t1d))
        yy2.append(mean(t2d))
    xx = [i for i in range(EVSET_MAX_SZ)]
    
    plt.figure(1)
    plt.plot(xx, yy1, color='red')
    plt.plot(xx, yy2, color='green')
    plt.savefig("L1-latency-trend.png")
    
    plt.figure(2)
    t1d, t2d = res[10]
    kde1, kde2 = gaussian_kde(t1d),gaussian_kde(t2d)
    #xx1 = np.linspace(min(t1d), max(t2d))
    xx2 = np.linspace(min(t2d), max(t2d))
    #plt.plot(xx1, kde1(xx1), color='red')
    plt.plot(xx2, kde2(xx2), color='green')
    plt.savefig("L1-latency-distribution (N=10)")

    plt.figure(3)
    t1d, t2d = res[60]
    kde1, kde2 = gaussian_kde(t1d),gaussian_kde(t2d)
    #xx1 = np.linspace(min(t1d), max(t2d))
    xx2 = np.linspace(min(t2d), max(t2d))
    #plt.plot(xx1, kde1(xx1), color='red')
    plt.plot(xx2, kde2(xx2), color='green')
    plt.savefig("L1-latency-distribution (N=60)")

    
    