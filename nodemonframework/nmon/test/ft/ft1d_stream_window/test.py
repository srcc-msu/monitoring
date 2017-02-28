#!/usr/bin/env python

import sys, os
import random, socket, struct, subprocess
sys.path.insert(0, '../common')
from ft_common import testOptionPresent, hexdump, drainSocketIncoming
from ft_common import formV2ControlMessage

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

def sendControlMessage(stage, s_conf, msg):
    drainSocketIncoming(s_conf)
    ntries = 5
    while True:
        if ntries <= 0:
            sys.stderr.write('FATAL: %s: cannot configure tested process\n' \
                    % (stage,))
            raise RuntimeError('test failed')
        ntries -= 1
        s_conf.sendto(msg, ('127.0.0.1', NMON_CTL_PORT))
        s_conf.settimeout(2.0)
        try:
            resp, respaddr = s_conf.recvfrom(65535)
            print 'resplen =', len(resp)
            print 'respaddr =', respaddr
            hexdump(resp)
            if resp[0:8] != 'MMCS.ACR':
                print 'Weird response, ignoring'
                continue
            break
        except Exception, e:
            print 'Exception', e

def waitForDataMessage(stage, s_data, cookie):
    drainSocketIncoming(s_data)
    ntries = 5
    while True:
        if ntries <= 0:
            sys.stderr.write('FATAL: %s: timeout' % (stage,))
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

    print 'prep stage: start target process'
    tproc = ts.tproc = subprocess.Popen( \
            args = ['../../../nmond', '--nohwck', '--foreground', \
                    '--conf', './nmond.conf',
                    '--pretend_host', '127.0.0.1',
                    '--pidfile', './pid'])
    print 'tested process pid:', tproc.pid
    print 'prep stage passed'

    print 'stage 1: configure tested process and wait for response'
    s_conf = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
            socket.IPPROTO_UDP)
    s_conf.bind(('0.0.0.0', 0))
    s_data = socket.socket(socket.AF_INET, socket.SOCK_DGRAM,
            socket.IPPROTO_UDP)
    s_data.bind(('0.0.0.0', 0))
    data_addr = s_data.getsockname()
    print 'Socket addresses: conf -> %r; data -> %r' % \
            (s_conf.getsockname(), data_addr,)
    agrport = data_addr[1]
    if testOptionPresent('stage1_wrong_agrport'):
        agrport ^= 0x8888
    cookie = random.randrange(1, 100)
    params = {'cookie': cookie, 'agrport': agrport, 'window': 1}
    if testOptionPresent('stage1_zero_window'):
        params['window'] = 0
    if testOptionPresent('control_with_bad_host'):
        params['pretend_address'] = '11.0.0.255'
    msg = formV2ControlMessage(params)
    sendControlMessage('stage 1', s_conf, msg)
    print 'stage 1 passed'

    print 'stage 2: wait for single data message and then silence'
    waitForDataMessage('stage 2.1', s_data, cookie)
    seen_exc = False
    try:
        waitForDataMessage('stage2.2', s_data, cookie)
    except socket.timeout:
        seen_exc = True
    if not seen_exc:
        raise RuntimeError('stage 2.2 failed: unexpected data message')
    print 'stage 2 passed'

    print 'stage 3: change cookie and check response'
    cookie = random.randrange(201, 300)
    agrport = data_addr[1]
    if testOptionPresent('stage3_wrong_agrport'):
        agrport ^= 0x8888
    params = {'cookie': cookie, 'agrport': agrport, 'window': 0}
    if testOptionPresent('stage3.1_wrong_window'):
        params[window] = 10
    msg = formV2ControlMessage(params)
    sendControlMessage('stage 3.1', s_conf, msg)
    seen_exc = False
    try:
        waitForDataMessage('stage3.2', s_data, cookie)
    except socket.timeout:
        seen_exc = True
    if not seen_exc:
        raise RuntimeError('stage 3.2 failed: unexpected data message')
    params = {'cookie': cookie, 'agrport': agrport, 'window': 20}
    if testOptionPresent('stage3.3_wrong_window'):
        params[window] = 2
    msg = formV2ControlMessage(params)
    sendControlMessage('stage 3.3', s_conf, msg)
    waitForDataMessage('stage 3.4', s_data, cookie)
    waitForDataMessage('stage 3.5', s_data, cookie)
    waitForDataMessage('stage 3.6', s_data, cookie)
    print 'stage 3 passed'

    print 'test succeeded'

if __name__ == '__main__':

    ts = TestState()

    rc = 0
    try:
        try:
            doTest(ts)
        except Exception, e:
            sys.stderr.write('Exception from test: %r\n' % (e,))
            rc = 1
    finally:
        ts.destroy()
    sys.exit(rc)
