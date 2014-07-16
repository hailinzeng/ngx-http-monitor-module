#!/usr/bin/python

import logging
import httplib
import time
import sys

from datetime import datetime

# A simple http client simulator to generate 200/non-200 http response

def initLog(name):
    formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
    # logging.basicConfig(format=formatter)

    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)

    fh = logging.FileHandler(name)
    fh.setFormatter(formatter)
    logger.addHandler(fh)

    # handler = logging.StreamHandler(sys.stdout)
    # handler.setFormatter(formatter)
    # logger.addHandler(handler)

    return logger

logger = initLog('httpclient.log')

begintime = datetime.now()

for i in range(1):
    if i % 10 == 0 :
        time.sleep(1)

    try:
        conn = httplib.HTTPConnection("127.0.0.1:80")

        # 1) request page that exist
        conn.request("GET", "/index.html")

        r1 = conn.getresponse()
        logger.info("status:%d, reason:%s", r1.status, r1.reason)

        data1 = r1.read()
        # print data1

        # 2) request page that not exist
        conn.request("GET", "/parrot.spam")

        r2 = conn.getresponse()
        logger.info("status:%d, reason:%s", r2.status, r2.reason)

        data2 = r2.read()
        # print data2

        conn.close()

    except:
        print "error:", sys.exc_info()[0]
        logger.info("error:%s" % sys.exc_info()[0])

# 
# endtime = datetime.now()
# diftime = endtime - begintime
# logger.info("diftime:%ds %dus", diftime.seconds, diftime.microseconds)
