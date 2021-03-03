#! /usr/bin/python

##
##  Generate random fits files for testing the main source code
##  see test/test_fits_filename.cpp
##

import datetime
import random

"""
    '.{1}_.{1}_.{8}_[0-9]+_[0-9]+_[0-9]+_[0-9]+(.*)'
       |    |    |    |      |      |      |
   instCode |    date |   runNumber |  plProcessing
       exposureType multRunNumber windowNumber
    Example filename : 'c_e_20070830_11_10_1_0.fits'
"""

info = {}

def make_random_fits(**kwargs):
    if 'instrument' not in kwargs: kwargs['instrument'] = 'c'
    if 'exposure' not in kwargs: kwargs['exposure'] = 'e'
    if 'date' not in kwargs: kwargs['date'] = datetime.datetime.now()
    if 'multRun' not in kwargs: kwargs['multRun'] = random.randrange(10)
    if 'run' not in kwargs: kwargs['run'] = random.randrange(101)
    if 'window' not in kwargs: kwargs['window'] = 1
    if 'plproc' not in kwargs: kwargs['plproc'] = 0
    if 'ext' not in kwargs: kwargs['ext'] = "fits"

    date_str = kwargs['date'].strftime("%Y%m%d")
    fn = "{:1s}_{:1s}_{:8s}_{:}_{:}_{:}_{:}.{:}".format(kwargs['instrument'], 
        kwargs['exposure'], date_str, kwargs['multRun'], kwargs['run'], 
        kwargs['window'], kwargs['plproc'], kwargs['ext'])
    
    if date_str in info:
        if kwargs['multRun'] > info[date_str]['max_mr']:
            info[date_str]['max_mr'] = kwargs['multRun']
            info[date_str]['max_r'] = kwargs['run']
        elif kwargs['multRun'] == info[date_str]['max_mr']:
            if kwargs['run'] > info[date_str]['max_r']:
                info[date_str]['max_r'] = kwargs['run']
    else:
        info[date_str] = {}
        info[date_str]['max_mr'] = kwargs['multRun']
        info[date_str]['max_r'] = kwargs['run']

    return fn

for d in [datetime.datetime.now(), 
            datetime.datetime.now()-datetime.timedelta(1),
            datetime.datetime.now()-datetime.timedelta(2)]:
    fns = [ make_random_fits(date=d) for i in range(20) ]
    for f in fns: 
        with open(f, 'w') as f: pass

for k, v in info.items():
    print("Max identifiers for date {:}".format(k))
    for key, val in v.items():
        print("\t{:} -> {:}".format(key, val))
