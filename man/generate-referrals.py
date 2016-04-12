#!/usr/bin/python
import sys
for name in sys.argv[1:]:
    for line in open(name, 'r'):
        words = line.split()
        if words[0] == '.so':
            print name, words[1]
            break
