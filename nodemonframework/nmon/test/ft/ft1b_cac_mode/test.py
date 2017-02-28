#!/usr/bin/env python

import sys, os
import random, socket, struct, subprocess, traceback
sys.path.insert(0, '../common')
from ft_common import testOptionPresent, hexdump, drainSocketIncoming
from ft_common import formV1ControlMessage

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
        break

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

    print 'stage 1: start target process with cac_mode=any and check successful configuring for 127.0.0.1'
    tproc = ts.tproc = subprocess.Popen( \
            args = ['../../../nmond', '--nohwck', '--foreground', \
                    '--conf', './nmond.conf',
                    '--cac_mode', 'any',
                    '--pidfile', './pid'])
    print 'stage 1: tested process pid:', tproc.pid
    cookie = random.randrange(101, 199)
    params['cookie'] = cookie
    sp = params.copy()
    msg = formV1ControlMessage(sp)
    sendControlMessage('stage 1.2', s_conf, msg)
    waitForDataMessage('stage 1.3', s_data, cookie)
    print 'stage 1 passed'

    print 'stage 2: with cac_mode=any, check weird address'
    sp = params.copy()
    sp['pretend_address'] = '127.0.0.128'
    msg = formV1ControlMessage(sp)
    sendControlMessage('stage 2.1', s_conf, msg)
    waitForDataMessage('stage 2.2', s_data, cookie)
    print 'stage 2 passed'
    tproc.terminate()
    tproc = ts.tproc = None

    print 'stage 3: start with cac_mode=iface and check for 127.0.0.1'
    tproc = ts.tproc = subprocess.Popen( \
            args = ['../../../nmond', '--nohwck', '--foreground', \
                    '--conf', './nmond.conf',
                    '--cac_mode', 'iface',
                    '--pidfile', './pid'])
    print 'stage 3: tested process pid:', tproc.pid
    cookie = random.randrange(301, 399)
    params['cookie'] = cookie
    sp = params.copy()
    msg = formV1ControlMessage(sp)
    sendControlMessage('stage 3.2', s_conf, msg)
    waitForDataMessage('stage 3.3', s_data, cookie)
    print 'stage 3 passed'

    print 'stage 4: with cac_mode=iface, check weird address'
    sp = params.copy()
    sp['pretend_address'] = '127.0.0.138'
    msg = formV1ControlMessage(sp)
    try:
        sendControlMessage('stage 4.1', s_conf, msg)
        raise RuntimeError('stage 4.1: agent responded when shall not')
    except ControlTimeoutError:
        pass
    waitForDataMessage('stage 4.2', s_data, cookie)
    print 'stage 4 passed'
    tproc.terminate()
    tproc = ts.tproc = None

    print 'stage 5: with cac_mode=exact, control with expected address'
    tproc = ts.tproc = subprocess.Popen( \
            args = ['../../../nmond', '--nohwck', '--foreground', \
                    '--conf', './nmond.conf',
                    '--cac_mode', 'exact',
                    '--pretend_host', '127.0.0.2',
                    '--pidfile', './pid'])
    print 'stage 5: tested process pid:', tproc.pid
    cookie = random.randrange(501, 599)
    params['cookie'] = cookie
    sp = params.copy()
    sp['pretend_address'] = '127.0.0.2'
    if testOptionPresent('stage5_wrong_host'):
        sp['pretend_address'] = '10.9.8.7'
    msg = formV1ControlMessage(sp)
    sendControlMessage('stage 5.2', s_conf, msg)
    waitForDataMessage('stage 5.3', s_data, cookie)
    print 'stage 5 passed'

    print 'stage 6: with cac_mode=exact, control with unknown address'
    sp = params.copy()
    ## NB: here it shall form for 127.0.0.1
    msg = formV1ControlMessage(sp)
    try:
        sendControlMessage('stage 6.1', s_conf, msg)
        raise RuntimeError('stage 6.1: agent responded when shall not')
    except ControlTimeoutError:
        pass
    waitForDataMessage('stage 6.2', s_data, cookie)
    print 'stage 6 passed'
    tproc.terminate()
    tproc = ts.tproc = None

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
