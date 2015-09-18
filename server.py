#!/usr/bin/python

#   Copyright (C) 2015 by seeedstudio
#   Author: Jack Shao (jacky.shaoxg@gmail.com)
#
#   The MIT License (MIT)
#
#   Permission is hereby granted, free of charge, to any person obtaining a copy
#   of this software and associated documentation files (the "Software"), to deal
#   in the Software without restriction, including without limitation the rights
#   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#   copies of the Software, and to permit persons to whom the Software is
#   furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included in
#   all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
#   THE SOFTWARE.

#   Dependences: pip install tornado
#                pip install PyJWT
#                pip install pycrypto

from datetime import timedelta
import socket
import json
import sqlite3 as lite
import re
import jwt
import md5
import hashlib
import hmac
import binascii
import threading
from Crypto.Cipher import AES
from Crypto import Random

from tornado.httpserver import HTTPServer
from tornado.tcpserver import TCPServer
from tornado import ioloop
from tornado import gen
from tornado import iostream
from tornado import web
from tornado.options import define, options
from tornado.log import *


from handlers import *

TOKEN_SECRET = "!@#$%^&*RG)))))))JM<==TTTT==>((((((&^HVFT767JJH"
BS = AES.block_size
pad = lambda s: s if (len(s) % BS == 0) else (s + (BS - len(s) % BS) * chr(0) )
unpad = lambda s : s.rstrip(chr(0))

class DeviceConnection(object):


    def __init__ (self, device_server, stream, address):
        self.recv_msg_queue = []
        self.send_msg_queue = []
        self.device_server = device_server
        self.stream = stream
        self.address = address
        self.stream.set_nodelay(True)
        self.idle_time = 0;
        self.killed = False
        self.sn = ""
        self.private_key = ""
        self.node_id = 0
        self.iv = None
        self.cipher = None

        self.state_waiters = []
        self.state_happened = []

        self.event_waiters = []
        self.event_happened = []

        self.ota_ing = False
        self.post_ota = False

    def secure_write (self, data):
        if self.cipher:
            cipher_text = self.cipher.encrypt(pad(data))
            self.stream.write(cipher_text)
            return cipher_text

    @gen.coroutine
    def wait_hello (self):
        try:
            self._wait_hello_future = self.stream.read_bytes(64) #read 64bytes: 32bytes SN + 32bytes signature signed with private key
            str = yield gen.with_timeout(timedelta(seconds=10), self._wait_hello_future,
                                         io_loop=self.stream.io_loop)
            self.idle_time = 0  #reset the idle time counter

            if len(str) != 64:
                self.stream.write("sorry\r\n")
                yield gen.sleep(0.1)
                self.kill_myself()
                gen_log.debug("receive length != 64")
                raise gen.Return(100) # length not match 64

            sn = str[0:32]
            sig = str[32:64]

            gen_log.info("accepted sn: "+ sn)

            #query the sn from database
            node = None
            cur = self.device_server.cur
            cur.execute('select * from nodes where node_sn="%s"'%sn)
            rows = cur.fetchall()
            if len(rows) > 0:
                node = rows[0]

            if not node:
                self.stream.write("sorry\r\n")
                yield gen.sleep(0.1)
                self.kill_myself()
                gen_log.info("node sn not found")
                raise gen.Return(101) #node not found

            key = node['private_key']
            key = key.encode("ascii")

            sig0 = hmac.new(key, msg=sn, digestmod=hashlib.sha256).digest()
            gen_log.debug("sig:     "+ binascii.hexlify(sig))
            gen_log.debug("sig calc:"+ binascii.hexlify(sig0))

            if sig0 == sig:
                #send IV + AES Key
                self.sn = sn
                self.private_key = key
                self.node_id = node['node_id']
                gen_log.info("valid hello packet from node %d" % self.node_id)
                #remove the junk connection of the same sn
                self.stream.io_loop.add_callback(self.device_server.remove_junk_connection, self)
                #init aes
                self.iv = Random.new().read(AES.block_size)
                self.cipher = AES.new(key, AES.MODE_CFB, self.iv, segment_size=128)
                cipher_text = self.iv + self.cipher.encrypt(pad("hello"))
                gen_log.debug("cipher text: "+ cipher_text.encode('hex'))
                self.stream.write(cipher_text)
                raise gen.Return(0)
            else:
                self.stream.write("sorry\r\n")
                yield gen.sleep(0.1)
                self.kill_myself()
                gen_log.error("signature not match: %s %s" % (sig, sig0))
                raise gen.Return(102) #sig not match
        except gen.TimeoutError:
            self.kill_myself()
            raise gen.Return(1)
        except iostream.StreamClosedError:
            self.kill_myself()
            raise gen.Return(2)

        #self.stream.io_loop.add_future(self._serving_future, lambda future: future.result())

    @gen.coroutine
    def _loop_reading_input (self):
        line = ""
        piece = ""
        while not self.killed:
            msg = ""
            try:
                msg = yield self.stream.read_bytes(16)
                msg = unpad(self.cipher.decrypt(msg))

                self.idle_time = 0 #reset the idle time counter

                line += msg

                while line.find('\r\n') > -1:
                    index = line.find('\r\n')
                    piece = line[:index+2]
                    line = line[index+2:]
                    piece = piece.strip("\r\n")
                    json_obj = json.loads(piece)
                    gen_log.info('Node %d: recv json: %s' % (self.node_id, str(json_obj)))

                    try:
                        state = None
                        event = None
                        if json_obj['msg_type'] == 'ota_trig_ack':
                            state = ('going', 'Node has been notified...')
                            self.ota_ing = True
                        elif json_obj['msg_type'] == 'ota_status':
                            if json_obj['msg'] == 'started':
                                state = ('going', 'Node is downloading the binary...')
                            else:
                                state = ('error', 'Node failed to start the downloading.')
                                self.post_ota = True
                        elif json_obj['msg_type'] == 'ota_result':
                            if json_obj['msg'] == 'success':
                                state = ('done', 'Firmware updated.')
                            else:
                                state = ('error', 'Update failed. Please reboot the node and retry.')
                            self.post_ota = True
                            self.ota_ing = False
                        elif json_obj['msg_type'] == 'event':
                            event = json_obj
                            event.pop('msg_type')
                        gen_log.debug(state)
                        gen_log.debug(event)
                        if state:
                            if len(self.state_waiters) == 0:
                                self.state_happened.append(state)
                            else:
                                self.state_waiters.pop(0).set_result(state)
                        elif event:
                            if len(self.event_waiters) == 0:
                                self.event_happened.append(event)
                            else:
                                for future in self.event_waiters:
                                    future.set_result(event)
                                self.event_waiters = []
                        else:
                            self.recv_msg_queue.append(json_obj)
                    except Exception,e:
                        gen_log.warn("Node %d: %s" % (self.node_id ,str(e)))

            except iostream.StreamClosedError:
                gen_log.error("StreamClosedError when write to node %d" % self.node_id)
                self.kill_myself()
                return
            except ValueError:
                gen_log.warn("Node %d: %s can not be decoded into json" % (self.node_id, piece))
            except Exception,e:
                gen_log.error("Node %d: %s" % (self.node_id ,str(e)))
                self.kill_myself()
                return

            yield gen.moment

            if self.post_ota:
                gen_log.error("Node %d OTA finished, will reboot, kill the connection" % self.node_id )
                self.kill_myself()
                return

    @gen.coroutine
    def _loop_sending_cmd (self):
        while not self.killed:
            if len(self.send_msg_queue) > 0:
                try:
                    cmd = self.send_msg_queue.pop(0)
                    self.secure_write(cmd)
                except Exception, e:
                    yield gen.moment
            else:
                yield gen.sleep(0.1)

    @gen.coroutine
    def _online_check (self):
        while not self.killed:
            yield gen.sleep(1)
            if self.ota_ing:
                continue 
            self.idle_time += 1
            if self.idle_time == 60:
                gen_log.info("heartbeat sent to node %d" % self.node_id)
                try:
                    self.secure_write("##PING##")
                except iostream.StreamClosedError:
                    gen_log.error("StreamClosedError when send ping to node %d" % self.node_id)
                    self.kill_myself()
            if self.idle_time == 70:
                gen_log.error("no answer from node %d, kill" % self.node_id)
                self.kill_myself()



    @gen.coroutine
    def start_serving (self):
        ret = yield self.wait_hello()
        if ret == 0:
            #gen_log.info("waited hello")
            pass
        elif ret == 1:
            gen_log.info("timeout waiting hello, kill this connection")
            return
        elif ret == 2:
            gen_log.info("connection is closed by client")
            return
        elif ret >= 100:
            return

        ## loop reading the stream input
        self._loop_reading_input()
        self._loop_sending_cmd()
        self._online_check()

    def kill_myself (self):
        if self.killed:
            return
        self.sn = ""
        self.stream.io_loop.add_callback(self.device_server.remove_connection, self)
        self.stream.close()
        self.killed = True

    def submit_cmd (self, cmd):
        self.send_msg_queue.append(cmd)

    @gen.coroutine
    def submit_and_wait_resp (self, cmd, target_resp, timeout_sec=10):
        self.submit_cmd(cmd)
        timeout = 0
        while True:
            try:
                for msg in self.recv_msg_queue:
                    if msg['msg_type'] == target_resp:
                        tmp = msg
                        self.recv_msg_queue.remove(msg)
                        del tmp['msg_type']
                        raise gen.Return((True, tmp))
                yield gen.sleep(0.1)
                timeout += 0.1
                if timeout > timeout_sec:
                    raise gen.Return((False, "timeout when waiting response from node %d" % self.node_id))
            except gen.Return:
                raise
            except Exception,e:
                gen_log.error(e)



class DeviceServer(TCPServer):

    accepted_conns = []

    def __init__ (self, db_conn, cursor):
        self.conn = db_conn
        self.cur = cursor
        TCPServer.__init__(self)


    def handle_stream(self, stream, address):
        conn = DeviceConnection(self, stream,address)
        self.accepted_conns.append(conn)
        conn.start_serving()
        gen_log.info("accepted conns: %d"% len(self.accepted_conns))

    def remove_connection (self, conn):
        gen_log.info("will remove connection: "+ str(conn))
        try:
            while len(conn.state_waiters) > 0:
                gen_log.debug("xxxxxxxxxxxxxxxx")
                conn.state_waiters.pop(0).set_result(('error', 'Node lost its current connection.'))
            self.accepted_conns.remove(conn)
            del conn
        except:
            pass

    def remove_junk_connection (self, conn):
        try:
            for c in self.accepted_conns:
                if c.sn == conn.sn and c != conn :
                    c.killed = True
                    #clear waiting futures
                    while len(c.state_waiters) > 0:
                        gen_log.debug("rrrrrrrrrrrrrrrr")
                        c.state_waiters.pop(0).set_result(('error', 'Node lost its current connection.'))
                    gen_log.info("removed one junk connection of same sn: "+ c.sn)
                    self.accepted_conns.remove(c)
                    del c
                    break
        except:
            pass



class myApplication(web.Application):

    def __init__(self,db_conn,cursor):
        handlers = [
        (r"/v1[/]?", IndexHandler),
        (r"/v1/test[/]?", TestHandler),
        (r"/v1/user/create[/]?", UserCreateHandler),
        (r"/v1/user/changepassword[/]?", UserChangePasswordHandler),
        (r"/v1/user/retrievepassword[/]?", UserRetrievePasswordHandler),
        (r"/v1/user/login[/]?", UserLoginHandler),
        (r"/v1/scan/drivers[/]?", DriversHandler),
        (r"/v1/scan/status[/]?", DriversStatusHandler),
        (r"/v1/nodes/create[/]?", NodeCreateHandler),
        (r"/v1/nodes/list[/]?", NodeListHandler, dict(conns=DeviceServer.accepted_conns)),
        (r"/v1/nodes/rename[/]?", NodeRenameHandler),
        (r"/v1/nodes/delete[/]?", NodeDeleteHandler),
        (r"(?!.*(event|config|resources))/v1/node/(.+)", NodeReadWriteHandler, dict(conns=DeviceServer.accepted_conns)),
        (r"/v1/node/event[/]?", NodeEventHandler,dict(conns=DeviceServer.accepted_conns)),
        (r"/v1/node/config[/]?", NodeGetConfigHandler,dict(conns=DeviceServer.accepted_conns)),
        (r"/v1/node/resources[/]?", NodeGetResourcesHandler,dict(conns=DeviceServer.accepted_conns)),
        (r"/v1/user/download[/]?", FirmwareBuildingHandler, dict(conns=DeviceServer.accepted_conns)),
        (r"/v1/ota/bin", OTAFirmwareSendingHandler, dict(conns=DeviceServer.accepted_conns)),
        (r"/v1/ota/status[/]?", OTAStatusReportingHandler, dict(conns=DeviceServer.accepted_conns)),
        ]
        self.conn = db_conn
        self.cur = cursor

        web.Application.__init__(self, handlers, debug=True, login_url="/v1/user/login",
            template_path = 'templates',)

def main():

    ###--log_file_prefix=./server.log
    ###--logging=debug
    enable_pretty_logging()
    options.parse_command_line()

    conn = None
    cur = None
    try:
        conn = lite.connect('database.db')
        conn.row_factory = lite.Row
        cur = conn.cursor()
        cur.execute('SELECT SQLITE_VERSION()')
        data = cur.fetchone()
        gen_log.info("SQLite version: %s" % data[0])
    except lite.Error, e:
        gen_log.error(e)
        sys.exit(1)

    app = myApplication(conn, cur)
    http_server = HTTPServer(app)
    http_server.listen(8080)

    tcp_server = DeviceServer(conn, cur)
    tcp_server.listen(8000)

    ioloop.IOLoop.current().start()

if __name__ == '__main__':
    main()

