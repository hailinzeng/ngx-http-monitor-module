from datetime import datetime
import logging
import socket
import struct
import sys

# A simple monitor server to receive Message report from nginx

def initLog(name):
    formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')

    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)

    fh = logging.FileHandler(name)
    fh.setFormatter(formatter)
    logger.addHandler(fh)

    return logger

def udplog():
    logger = initLog("httplog_serv.log")

    multicast_group = '127.0.0.1'
    server_address = ('', 10000)

    # Create the socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    # Bind to the server address
    sock.bind(server_address)

    # Tell the operating system to add the socket to the multicast group
    # on all interfaces.
    group = socket.inet_aton(multicast_group)
    mreq = struct.pack('4sL', group, socket.INADDR_ANY)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

    # Receive/respond loop
    while True:
        logger.info('waiting to receive message')
        data, address = sock.recvfrom(1024)
      
        logger.info('received %s bytes from %s' % (len(data), address))
        logger.info(data)

        hexinfo = ":".join(c.encode('hex') for c in data)
        logger.info(hexinfo)

    return

if "__main__" == __name__:
    udplog()
