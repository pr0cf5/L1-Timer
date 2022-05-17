#!/usr/bin/python3
import os
import struct

def p32(x):
    return struct.pack("<I",x)

def u64(x):
    return struct.unpack("<Q", x)[0]

def read_result(file_name):
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

def run(path):
    pathdata = b""
    pathdata += p32(len(path))
    for x in path:
        pathdata += p32(x)
    with open("path.txt", "wb") as f:
        f.write(pathdata)
    os.system("./explorer")
    result = read_result("result.txt")

if __name__ == "__main__":
    REPS = 0x1000
    L1_ASSOC = 8
    run([5,6,3,1])