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

from datetime import timedelta
import socket
import json
import sqlite3 as lite
import re
import jwt
import md5

from tornado.httpserver import HTTPServer
from tornado.tcpserver import TCPServer
from tornado import ioloop
from tornado import gen
from tornado import iostream
from tornado import web
from tornado.options import define, options

from handlers import *

TOKEN_SECRET = "!@#$%^&*RG)))))))JM<==TTTT==>((((((&^HVFT767JJH"

class DeviceConnection(object):

    recv_msg_queue = []
    send_msg_queue = []

    def __init__ (self, device_server, stream, address):
        self.device_server = device_server
        self.stream = stream
        self.address = address
        self.stream.set_nodelay(True)
        self.node_token = ""

    @gen.coroutine
    def wait_hello (self):
        try:
            self._wait_hello_future = self.stream.read_until("hello\r\n")
            str = yield gen.with_timeout(timedelta(seconds=10), self._wait_hello_future,
                                         io_loop=self.stream.io_loop)
            self.stream.write("hello\r\n")
            raise gen.Return(0)
        except gen.TimeoutError:
            self.kill_myself()
            raise gen.Return(1)
        except iostream.StreamClosedError:
            self.kill_myself()
            raise gen.Return(2)

        #self.stream.io_loop.add_future(self._serving_future, lambda future: future.result())
    @gen.coroutine
    def wait_node_token (self):
        try:
            msg = yield self.stream.read_until("\r\n")
            msg = msg.strip("\r").strip("\n").strip("\r\n")
            self.node_token = msg
            print "node token:", msg
            raise gen.Return(0)
        except iostream.StreamClosedError:
            self.kill_myself()
            raise gen.Return(1)

    @gen.coroutine
    def _loop_reading_input (self):
        while True:
            msg = ""
            try:
                msg = yield self.stream.read_until("\r\n")
                msg = msg.strip("\r\n")
                json_obj = json.loads(msg)
                print json_obj
                self.recv_msg_queue.append(json_obj)
                print self.recv_msg_queue
            except iostream.StreamClosedError:
                self.kill_myself()
                return
            except ValueError:
                print msg, " can not be decoded into json"
                pass

            yield gen.moment

    @gen.coroutine
    def _loop_sending_cmd (self):
        while True:
            if len(self.send_msg_queue) > 0:
                try:
                    cmd = self.send_msg_queue.pop()
                    yield self.stream.write(cmd)
                except Exception, e:
                    yield gen.moment
            else:
                yield gen.sleep(0.1)

    @gen.coroutine
    def start_serving (self):
        ret = yield self.wait_hello()
        if ret == 0:
            print "waited hello"
        elif ret == 1:
            print "timeout waiting hello, kill this connection"
            return
        elif ret == 2:
            print "connection is closed by client"
            return

        ret = yield self.wait_node_token()
        if ret == 1:
            print "connection is closed by client"
            return

        ## check node_id if exists in the database
        ## assume true

        ## loop reading the stream input
        self._loop_reading_input()
        self._loop_sending_cmd()

    def kill_myself (self):
        self.stream.io_loop.add_callback(self.device_server.remove_connection, self)
        self.stream.close()

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
                        tmp = msg['msg']
                        self.recv_msg_queue.remove(msg)
                        raise gen.Return((True, tmp))
                yield gen.sleep(0.1)
                timeout += 0.1
                if timeout > timeout_sec:
                    raise gen.Return((False, "timeout when waiting response from node"))
            except gen.Return:
                raise
            except Exception,e:
                print e



class DeviceServer(TCPServer):

    accepted_conns = []

    def handle_stream(self, stream, address):
        conn = DeviceConnection(self, stream,address)
        self.accepted_conns.append(conn)
        conn.start_serving()
        print "accepted conns: ", len(self.accepted_conns)

    def remove_connection (self, conn):
        print "will remove connection: ", conn
        self.accepted_conns.remove(conn)


class myApplication(web.Application):
    def __init__(self):
        handlers = [
        (r"/", IndexHandler),
        (r"/user/create[/]?", UserCreateHandler),
        (r"/user/changepassword[/]?", UserChangePasswordHandler),
        (r"/user/login[/]?", UserLoginHandler),
        (r"/scan/drivers[/]?", DriversHandler),
        (r"/scan/status[/]?", DriversStatusHandler),
        (r"/nodes/create[/]?", NodeCreateHandler),
        (r"/nodes/list[/]?", NodeListHandler),
        (r"/node/(.+)", NodeReadWriteHandler, dict(conns=DeviceServer.accepted_conns)),
        (r"/user/download[/]?", UserDownloadHandler),
        ]
        self.conn = None
        self.cur = None

        try:
            self.conn = lite.connect('database.db')
            self.conn.row_factory = lite.Row
            self.cur = self.conn.cursor()
            self.cur.execute('SELECT SQLITE_VERSION()')
            data = self.cur.fetchone()
            print "SQLite version: %s" % data[0]
        except lite.Error, e:
            print e
            sys.exit(1)

        web.Application.__init__(self, handlers, debug=True, login_url="/user/login")

def main():

    ###--log_file_prefix=./server.log
    options.parse_command_line()
    http_server = HTTPServer(myApplication())
    http_server.listen(8080)

    tcp_server = DeviceServer()
    tcp_server.listen(8081)

    ioloop.IOLoop.current().start()

if __name__ == '__main__':
    main()

