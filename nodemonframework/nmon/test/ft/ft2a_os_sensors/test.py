#!/usr/bin/env python

import sys, os
import random, socket, struct, subprocess, traceback
sys.path.insert(0, '../common')
from ft_common import testOptionPresent, hexdump, drainSocketIncoming
from ft_common import formV1ControlMessage
from ft_common import DataMessage

NMON_CTL_PORT = 4259

class TestState:
    """The class is invented only to conform to recommended style instead of
    global variables, and to gather destructions to common code.
    """
    tproc = None

    def __del__(self):
        self.destroy()

    def destroy(self):
        print "_: TestState.destroy()"
        if self.tproc is not None:
            self.tproc.terminate()
            self.tproc = None

class ControlTimeoutError(RuntimeError):
    pass

def sendControlMessage(stage, s_conf, msg):
    drainSocketIncoming(s_conf)
    ntries = 8
    while True:
        if ntries <= 0:
            sys.stderr.write('%s: cannot configure tested process\n' \
                    % (stage,))
            raise ControlTimeoutError('timeout waiting for control response')
        ntries -= 1
        s_conf.sendto(msg, ('127.0.0.1', NMON_CTL_PORT))
        s_conf.settimeout(1.0)
        try:
            resp, respaddr = s_conf.recvfrom(65535)
            print 'resplen =', len(resp)
            print 'respaddr =', respaddr
            hexdump(resp)
            if resp[0:8] != 'MMCS.ACR':
                print 'Weird response (bad signature), ignoring'
                continue
            if resp[8:50] != msg[8:50]:
                print 'Weird response (not to my request), ignoring'
                continue
            break
        except Exception, e:
            print 'Exception', e

def waitForDataMessage(stage, s_data, cookie):
    drainSocketIncoming(s_data)
    ntries = 8
    while True:
        if ntries <= 0:
            sys.stderr.write('%s: timeout' % (stage,))
            raise RuntimeError('test failed')
        ntries -= 1
        s_data.settimeout(5.0)
        msg, msgaddr = s_data.recvfrom(65535)
        print 'msglen = ', len(msg)
        print 'msgaddr =', msgaddr
        hexdump(msg[0:54])
        if not msg.startswith('MMCS.NAG'):
            print 'Unknown message, retrying'
            continue
        if len(msg) < 54:
            print 'Too short message, retrying'
            continue
        rck = struct.unpack('>L', msg[38:42])[0]
        if rck != cookie:
            print 'Invalid cookie (wanted %s, see %s), retrying' \
                    % (cookie, rck,)
            continue
        return msg

def doTest(ts):

    params = {}
    s_conf = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
            socket.IPPROTO_UDP)
    s_conf.bind(('0.0.0.0', 0))
    s_data = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
            socket.IPPROTO_UDP)
    s_data.bind(('0.0.0.0', 0))
    data_addr = s_data.getsockname()
    print 'Socket addresses: conf -> %r; data -> %r' % \
            (s_conf.getsockname(), data_addr,)
    params['agrhost'] = '127.0.0.1'
    params['agrport'] = data_addr[1]

    print 'stage 1: start and check for control response'
    tproc = ts.tproc = subprocess.Popen( \
            args = ['../../../nmond', '--nohwck', '--foreground', \
                    '--conf', './nmond.conf',
                    '--cac_mode', 'iface',
                    '--pidfile', './pid'])
    print 'stage 1: tested process pid:', tproc.pid
    cookie = random.randrange(301, 399)
    params['cookie'] = cookie
    sp = params.copy()
    msg = formV1ControlMessage(sp)
    sendControlMessage('stage 1.2', s_conf, msg)
    print 'stage 1 passed'

    print 'stage 2: wait for data message and check it'
    ntries = 4  ## too high:)
    while True:
        if ntries <= 0:
            raise RuntimeError('stage 2: no proper message')
        ntries -= 1
        try:
            databin = waitForDataMessage('stage 2.1', s_data, cookie)
            data = DataMessage(binmsg = databin)
            if data.stream != 1:
                print 'Got message for another stream (%r)' % (data.stream,)
                continue
            ## The sensors we expect in stream 1 in all modes
            for st in (1250, 1251, 1252, 1253, 1254):
                vb = data.sensors_bin.get(st, None)
                if not isinstance(vb, str) or len(vb) != 8:
                    raise RuntimeError('stage 2: wrong sensor %r: %r' % \
                            (st, vb))
                value = struct.unpack('>Q', vb)[0]
                print '%r = %r' % (st, value,)
            break
        except Exception, e:
            print 'Exception from getting data message: %r' % (e,)
            print '-' * 70
            traceback.print_exc(file = sys.stdout)
            print '-' * 70
            continue
    print 'stage 2 passed'

    print 'test succeeded'

if __name__ == '__main__':

    ts = TestState()

    rc = 0
    try:
        try:
            doTest(ts)
        except Exception, e:
            sys.stderr.write('Exception from test: %r\n' % (e,))
            print '-' * 70
            traceback.print_exc(file = sys.stdout)
            print '-' * 70
            rc = 1
    finally:
        ts.destroy()
    sys.exit(rc)
