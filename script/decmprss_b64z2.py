#! /usr/bin/python

import sys
import base64
import bz2
from textwrap import wrap

with open(sys.argv[1], 'rb') as fin:
    data = fin.read(4096)
    
    ## message start from BF=
    bf = data.find(b'BF=')
    raw_msg = data[bf+3:-1]
    msg_decomposed = False
    
    try:
        #print('Raw msg [{:}]'.format(repr(raw_msg)))
        u64_msg = base64.b64decode(raw_msg)
        # u64_msg = decode_base64(raw_msg)
        ubz2 = bz2.decompress(u64_msg)
        for line in wrap(repr(ubz2), 80): print(line)
        msg_decomposed = True
    except:
        print('Failed to decompose message')
        msg_decomposed = False

sys.exit(msg_decomposed)
