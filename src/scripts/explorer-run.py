#!/usr/bin/python3
import os
import struct

def p32(x):
    return struct.pack("<I",x)

def u64(x):
    return struct.unpack("<Q", x)[0]

def run(path):
    pathdata = b""
    pathdata += p32(len(path))
    for x in path:
        pathdata += p32(x)
    with open("path.txt", "wb") as f:
        f.write(pathdata)
    os.system("./explorer")

if __name__ == "__main__":
    REPS = 0x1000
    L1_ASSOC = 8
    # initial
    seq = [0,1,2,3,4,5,6,7]
    # X
    run(seq)