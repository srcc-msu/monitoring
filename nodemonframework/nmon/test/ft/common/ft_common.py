import os
import socket, struct

## The simplest variant.
def isprint(ch):
    c = ord(ch)
    return 32 <= c <= 126

def hexdump(msg):
    i = 0
    l = len(msg)
    while i < l:
        out = '%04X' % (i,)
        for j in range(0,16):
            p = i + j
            if p < l:
                out += ' %02X' % (ord(msg[p]),)
            else:
                out += '   '
        out += '  |'
        for j in range(0,16):
            p = i + j
            if p < l:
                c = msg[p]
                if isprint(c):
                    out += c
                else:
                    out += '.'
            else:
                out += ' '
        print out
        i += 16

def testOptionPresent(option):
    return option in os.environ.get('TEST_OPTIONS', '').split(',')

def drainSocketIncoming(s):
    s.settimeout(0)
    while True:
        try:
            s.recvfrom(65535)
            continue
        except socket.error:
            break

def formV1ControlMessage(params):
    msg = "MMCS.ACF\x00\x01\x00\x00"    ## signature, version=1, flags=0
    pretend_address = params.get('pretend_address', '127.0.0.1')
    msg += socket.inet_aton(pretend_address)
    msg += "\x00" * 12                  ## address field filler
    agrhost = params.get('agrhost', '127.0.0.1')
    msg += socket.inet_aton(agrhost)
    msg += "\x00" * 12                  ## address field filler
    msg += struct.pack('>H', params.get('agrport', 0))
    msg += struct.pack('>L', params.get('cookie', 0))
    print 'formV1ControlMessage() formed:'
    if params.get('hexdump', False):
        hexdump(msg)
    return msg

## Aux for formV2ControlMessage()
def _formV2Attr(attrid, payload):
    pl = len(payload)
    if (pl % 2) == 1:
        pl += 1
        payload += '\x00'
    return struct.pack('>HH', pl, attrid) + payload

def _formV2Ip4Attr(attrid, addrtext):
    addrbin = socket.inet_aton(addrtext)
    return _formV2Attr(attrid, '\x00\x00' + addrbin)

def formV2ControlMessage(params):
    ## signature, version=2, flags=0, csumtype=0, resp_code=0
    msg = "MMCS.ACF\x00\x02\x00\x00\x00\x00"
    pretend_address = params.get('pretend_address', None)
    if pretend_address is not None:
        msg += _formV2Ip4Attr(256, pretend_address)
    agrhost = params.get('agrhost', None)
    if agrhost is not None:
        msg += _formV2Ip4Attr(1, agrhost)
    agrport = params.get('agrport', None)
    if agrport is not None:
        msg += _formV2Attr(2, struct.pack('>H', agrport))
    cookie = params.get('cookie', None)
    if cookie is not None:
        msg += _formV2Attr(3, struct.pack('>L', cookie))
    stream = params.get('stream', None)
    if stream is not None:
        msg += _formV2Attr(257, struct.pack('>H', stream))
    window = params.get('window', None)
    if window is not None:
        msg += _formV2Attr(5, struct.pack('>l', window))
    ## Four zero bytes are required to terminate TLV
    msg += '\x00\x00\x00\x00'
    print 'formV2ControlMessage() formed:'
    if params.get('hexdump', False):
        hexdump(msg)
    return msg

def enow_to_unixtime(ts1, ts2, ts3):
    return ts1 * 1000000 + ts2 + ts3 / 1000000.0

class DataMessage:
    version = None
    payload_type = None
    stream = None
    flags = None
    source = None
    ssrc = None
    seq = None
    cookie = None
    sensors_bin = None

    def __init__(self, binmsg = None):
        if binmsg is not None:
            self.initFromBinary(binmsg)

    def initFromBinary(self, binmsg):
        if len(binmsg) < 10:
            raise ValueError('DataMessage: too short message')
        if binmsg[0:8] != 'MMCS.NAG':
            raise ValueError('DataMessage: invalid signature')
        version = struct.unpack('>H', binmsg[8:10])[0]
        ## TODO: expand when version 2 will be added
        if version != 1:
            raise ValueError('DataMessage: unsupported version %r' % \
                    (message,))
        self.version = version
        if len(binmsg) < 54:
            raise ValueError('DataMessage: too short for version 1')
        self.payload_type, self.stream, self.flags, srcbin, \
                self.ssrc, self.seq, self.cookie, ts1, ts2, ts3 = \
                struct.unpack('>BBH16sLLLLLL', binmsg[10:54])
        self.timestamp = enow_to_unixtime(ts1, ts2, ts3)
        if (self.flags & 3) == 0:
            self.source = socket.inet_ntoa(srcbin[0:4])
        else:
            raise ValueError('DataMessage: unsupported address family')
        if self.payload_type != 1:
            raise ValueError('DataMessage: unsupported payload type')
        rest = binmsg[54:]
        self.sensors_bin = {}
        while True:
            if len(rest) < 4:
                raise ValueError('DataMessage: unterminated TLV')
            s_dlen, s_type = struct.unpack('>HH', rest[0:4])
            rest = rest[4:]
            if s_dlen == 0:
                break
            if len(rest) < s_dlen:
                raise ValueError('DataMessage: too short TLV body')
            self.sensors_bin[s_type] = rest[0:s_dlen]
            rest = rest[s_dlen:]
