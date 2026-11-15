#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 SedonaPro contributors
"""Small MQTT 3.1.1 broker for tests: routing with + and # wildcards, retained
messages, QoS 0/1, will on abnormal disconnect, optional user/password.
Every PUBLISH it receives is logged as 'PUBLISH <topic> <payload>' (PUBLISHr =
retained).

  mqtt_broker.py [port] [--user U --password P] [--bind ADDR]

Also a tiny client for tests:
  mqtt_broker.py pub HOST PORT TOPIC PAYLOAD [--retain] [--user U --password P]
"""
import socket
import sys
import threading

lock = threading.Lock()
clients = {}      # socket -> {"subs": {filter: qos}, "will": (topic, payload, retain) or None}
retained = {}     # topic -> payload


def log(*a):
    print(*a, flush=True)


def recv_exact(c, n):
    b = b''
    while len(b) < n:
        chunk = c.recv(n - len(b))
        if not chunk:
            raise ConnectionError
        b += chunk
    return b


def read_packet(c):
    h = recv_exact(c, 1)[0]
    mult, length = 1, 0
    while True:
        b = recv_exact(c, 1)[0]
        length += (b & 127) * mult
        mult *= 128
        if not b & 128:
            break
    return h, recv_exact(c, length)


def enc_len(n):
    out = b''
    while True:
        b = n % 128
        n //= 128
        out += bytes([b | 128 if n else b])
        if not n:
            return out


def s16(s):
    s = s.encode() if isinstance(s, str) else s
    return len(s).to_bytes(2, 'big') + s


def publish_packet(topic, payload, qos=0, retain=False, pid=1):
    body = s16(topic) + (pid.to_bytes(2, 'big') if qos else b'') + payload
    return bytes([0x30 | (qos << 1) | (1 if retain else 0)]) + enc_len(len(body)) + body


def match(f, t):
    if t.startswith('$') and f[:1] in ('+', '#'):
        return False
    fp, tp = f.split('/'), t.split('/')
    for i, p in enumerate(fp):
        if p == '#':
            return True
        if i >= len(tp):
            return False
        if p != '+' and p != tp[i]:
            return False
    return len(fp) == len(tp)


def route(topic, payload, retain):
    if retain:
        if payload:
            retained[topic] = payload
        else:
            retained.pop(topic, None)
    with lock:
        targets = [(c, max((q for f, q in st['subs'].items() if match(f, topic)), default=None))
                   for c, st in clients.items()]
    for c, q in targets:
        if q is None:
            continue
        try:
            c.sendall(publish_packet(topic, payload, min(q, 1), False, 1))
        except OSError:
            pass


def field(body, p):
    n = int.from_bytes(body[p:p + 2], 'big')
    return body[p + 2:p + 2 + n], p + 2 + n


def client(c, user, password):
    st = {'subs': {}, 'will': None}
    clean = False
    try:
        h, body = read_packet(c)
        if h >> 4 != 1:
            return
        flags = body[7]
        cid, p = field(body, 10)
        cid = cid.decode()
        will = u = pw = None
        if flags & 0x04:
            wt, p = field(body, p)
            wm, p = field(body, p)
            will = (wt.decode(), wm, bool(flags & 0x20))
        if flags & 0x80:
            u, p = field(body, p)
            u = u.decode()
        if flags & 0x40:
            pw, p = field(body, p)
            pw = pw.decode()
        if user is not None and (u != user or pw != password):
            log('CONNECT refused', cid)
            c.sendall(b'\x20\x02\x00\x05')
            return
        log('CONNECT', cid, 'user=%s' % u if u else '', 'will=%s' % will[0] if will else '')
        c.sendall(b'\x20\x02\x00\x00')
        st['will'] = will
        with lock:
            clients[c] = st
        while True:
            h, body = read_packet(c)
            kind = h >> 4
            if kind == 3:
                topic, p = field(body, 0)
                topic = topic.decode()
                rest = body[p:]
                if (h >> 1) & 3:
                    c.sendall(b'\x40\x02' + rest[:2])
                    rest = rest[2:]
                log('PUBLISH%s %s %s' % ('r' if h & 1 else '', topic, rest.decode(errors='replace')))
                route(topic, rest, bool(h & 1))
            elif kind == 8:
                pid, p, granted, new = body[:2], 2, b'', []
                while p < len(body):
                    f, p = field(body, p)
                    f = f.decode()
                    q = min(body[p], 1)
                    p += 1
                    st['subs'][f] = q
                    granted += bytes([q])
                    new.append(f)
                    log('SUBSCRIBE', f)
                c.sendall(bytes([0x90]) + enc_len(2 + len(granted)) + pid + granted)
                for t, v in list(retained.items()):
                    if any(match(f, t) for f in new):
                        c.sendall(publish_packet(t, v, 0, True))
            elif kind == 10:
                pid, p = body[:2], 2
                while p < len(body):
                    f, p = field(body, p)
                    st['subs'].pop(f.decode(), None)
                    log('UNSUBSCRIBE', f.decode())
                c.sendall(b'\xb0\x02' + pid)
            elif kind == 12:
                c.sendall(b'\xd0\x00')
            elif kind == 14:
                clean = True
                log('DISCONNECT', cid)
                return
    except (ConnectionError, OSError, IndexError):
        pass
    finally:
        with lock:
            clients.pop(c, None)
        c.close()
        if not clean and st['will']:
            t, v, r = st['will']
            log('WILL %s %s' % (t, v.decode(errors='replace')))
            route(t, v, r)


def pub(host, port, topic, payload, retain, user=None, password=None):
    with socket.create_connection((host, int(port)), timeout=5) as c:
        flags = 0x02 | (0x80 if user else 0) | (0x40 if password else 0)
        body = s16('MQTT') + bytes([4, flags, 0, 30]) + s16('mini-pub')
        body += (s16(user) if user else b'') + (s16(password) if password else b'')
        c.sendall(b'\x10' + enc_len(len(body)) + body)
        h, ack = read_packet(c)
        if ack[1] != 0:
            raise SystemExit('connect refused: %d' % ack[1])
        c.sendall(publish_packet(topic, payload.encode(), 0, retain))
        c.sendall(b'\xe0\x00')


def main():
    args = sys.argv[1:]
    opt = lambda k: args[args.index(k) + 1] if k in args else None
    if args[:1] == ['pub']:
        return pub(args[1], args[2], args[3], args[4], '--retain' in args, opt('--user'), opt('--password'))
    user, password, bind = opt('--user'), opt('--password'), opt('--bind') or '127.0.0.1'
    port = int(args[0]) if args and not args[0].startswith('-') else 18830
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((bind, port))
    s.listen()
    log('listening', port)
    while True:
        c, _ = s.accept()
        threading.Thread(target=client, args=(c, user, password), daemon=True).start()


if __name__ == '__main__':
    main()
