#!/usr/bin/env python3
import random
import math
import subprocess
import time
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("-t", "--test",
                    help="Which test to run",
                    default=1, type=int)
parser.add_argument("-a", "--aes",
                    help="Aesthetic arguments to pass to the program. E.g., -a\"-t lines -c red\"",
                    default="")

def to_str(x):
    return ", ".join(map(str, x))

def test1(aes):
    x = [i/10 for i in range(0, 100)]
    y = [math.sin(i) for i in x]
    print(f"len(x)={len(x)}, len(y)={len(y)}")
    return ["-x", to_str(x), "-y", to_str(y), *aes]


def test2(aes):
    x = [i for i in range(5)]
    y = [i for i in range(8)]
    y.extend(y)
    print(", ".join(map(str, x)))
    print(", ".join(map(str, y)))
    return ["-x", to_str(x), "-y", to_str(y), *aes]


def test3(aes):
    x = [i/5 for i in range(0, 5)]
    y = [math.sin(i) for i in x]
    print(f"len(x)={len(x)}, len(y)={len(y)}")
    return ["-x", to_str(x), "-y", to_str(y), *aes]


def test4(aes):
    args = """-P -x "1,2,3" -y "1,4,9" -t lines -c red""" + " " + \
           """-P -x "1,2,3" -y "2,3,4" -t points -c blue"""
    args = args.split(" ")
    print(args)
    return args


def test5(aes):
    args = """-P --file test.dat -x1 -y2 -t lines -c red""" + " " + \
           """-P -x "1,2,3,4" -y "3,6,9,12" -t points -c blue"""
    args = args.split(" ")
    print(args)
    return args

def test6(aes):
    args = """-P -x "1,2,3,4" -y "3,6,9,12" -t points -c blue""" + " " + \
           """-P --file test.dat -x1 -y2 -t lines -c red"""
    args = args.split(" ")
    print(args)
    return args

test_cases = {1: test1, 2: test2, 3: test3, 4: test4, 5: test5, 6: test6}

if __name__ == "__main__":
    args = parser.parse_args()
    if (not args.test in test_cases):
        print(f"Test {args.test} not found")
        exit(1)
    aes = args.aes
    aes = aes.strip().split(" ")

    # get data
    proc_args = test_cases[args.test](aes)

    # run program
    print("Running C program...\n")
    proc = subprocess.Popen(["./main", *proc_args])

    try:
        while proc.poll() is None:
            time.sleep(0.1)
    except KeyboardInterrupt:
        proc.terminate()
        print("  Program terminated")
