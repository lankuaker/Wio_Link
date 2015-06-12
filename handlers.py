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

#   Dependences: see server.py header section

import os
from datetime import timedelta
import socket
import json
import sqlite3 as lite
import re
import jwt
import md5
import base64
import httplib
import uuid
from shutil import copy
from build_firmware import *
import yaml
import threading

from tornado.httpserver import HTTPServer
from tornado.tcpserver import TCPServer
from tornado import ioloop
from tornado import gen
from tornado import iostream
from tornado import web
from tornado import websocket
from tornado.options import define, options
from tornado.log import *
from tornado.concurrent import Future

TOKEN_SECRET = "!@#$%^&*RG)))))))JM<==TTTT==>((((((&^HVFT767JJH"


class BaseHandler(web.RequestHandler):
    def get_current_user(self):
        user = None
        token = self.get_argument("access_token","")
        if not token:
            try:
                token_str = self.request.headers.get("Authorization")
                token = token_str.replace("token ","")
            except:
                token = None

        if token:
            try:
                cur = self.application.cur
                cur.execute('select * from users where token="%s"'%token)
                rows = cur.fetchall()
                if len(rows) > 0:
                    user = rows[0]
            except:
                user = None
        else:
            user = None

        print "get current user", str(user)
        if not user:
            self.resp(403,"Please login to get the token")
        return user



    '''
    400 - bad variables / format
    401 - unauthorized
    403 - forbidden
    404 - not found
    500 - server internal error
    '''
    def resp (self, status, msg="",meta={}):
        response = {"status":status,"msg":msg}
        response = dict(response, **meta);
        self.write(response)

    def write_error(self, status_code, **kwargs):
        #Function to display custom error page defined in the handler.
        #Over written from base handler.
        msg = "Please login to get the token" if status_code == 403 else httplib.responses[status_code]
        self.resp(status_code, msg)

    def get_uuid (self):
        return str(uuid.uuid4())

    def gen_token (self, email):
        return jwt.encode({'email': email,'uuid':self.get_uuid()}, TOKEN_SECRET, algorithm='HS256').split(".")[2]

class IndexHandler(BaseHandler):
    @web.authenticated
    def get(self):
        #DeviceServer.accepted_conns[0].submit_cmd("OTA\r\n")
        self.resp(400,msg = "Please specify the url as this format: /node_id/grove_name/property")

class TestHandler(web.RequestHandler):
    def get(self):
        args = dict(username = 'visitor')
        self.render("test.html", **args)



class UserCreateHandler(BaseHandler):
    def get (self):
        self.resp(404, "Please post to this url\n")

    def post(self):
        email = self.get_argument("email","")
        passwd = self.get_argument("password","")
        if not email:
            self.resp(400, "Missing email information\n")
            return
        if not passwd:
            self.resp(400, "Missing password information\n")
            return
        if not re.match(r'^[_.0-9a-z-]+@([0-9a-z][0-9a-z-]+.)+[a-z]{2,4}$', email):
            self.resp(400, "Bad email address\n")
            return

        cur = self.application.cur
        token = self.gen_token(email)
        try:
            cur.execute('select * from users where email="%s"'%email)
            rows = cur.fetchall()
            if len(rows) > 0:
                self.resp(400, "This email already registered")
                return
            cur.execute("INSERT INTO users(email,pwd,token) VALUES('%s','%s','%s')"%(email, md5.new(passwd).hexdigest(), token))
        except Exception,e:
            self.resp(500,str(e))
            return
        finally:
            self.application.conn.commit()

        self.resp(200, "User created",{"token": token})

class UserChangePasswordHandler(BaseHandler):
    def get (self):
        self.resp(403, "Please post to this url")

    @web.authenticated
    def post(self):
        passwd = self.get_argument("password","")
        if not passwd:
            self.resp(400, "Missing new password information\n")
            return

        email = self.current_user["email"]
        token = self.current_user["token"]
        gen_log.info("%s want to change password with token %s"%(email,token))

        cur = self.application.cur
        try:
            new_token = self.gen_token(email)
            cur.execute('update users set pwd="%s",token="%s" where email="%s"'%(md5.new(passwd).hexdigest(),new_token, email))
            self.resp(200, "Password changed, new token is given",{"token": new_token})
            gen_log.info("%s succeed to change password"%(email))
        except Exception,e:
            self.resp(500,str(e))
            return
        finally:
            self.application.conn.commit()


class UserLoginHandler(BaseHandler):
    def get (self):
        self.resp(403, "Please login to get the token\n")

    def post(self):
        email = self.get_argument("email","")
        passwd = self.get_argument("password","")
        if not email:
            self.resp(400, "Missing email information\n")
            return
        if not passwd:
            self.resp(400, "Missing password information\n")
            return
        if not re.match(r'^[_.0-9a-z-]+@([0-9a-z][0-9a-z-]+.)+[a-z]{2,4}$', email):
            self.resp(400, "Bad email address\n")
            return

        cur = self.application.cur
        try:
            cur.execute('select * from users where email="%s" and pwd="%s"'%(email, md5.new(passwd).hexdigest()))
            row = cur.fetchone()
            if not row:
                self.resp(401, "Login failed - invalid email or password")
                return
            self.resp(200, "Logged in",{"token": row["token"]})
        except Exception,e:
            self.resp(500,str(e))
            return
        finally:
            self.application.conn.commit()

class DriversHandler(BaseHandler):
    @web.authenticated
    def get (self):
        cur_dir = os.path.split(os.path.realpath(__file__))[0]
        self.write(open(os.path.join(cur_dir, "database.json")).read())

    @web.authenticated
    def post(self):
        self.resp(403, "Please get this url")

class DriversStatusHandler(BaseHandler):
    @web.authenticated
    def get (self):
        cur_dir = os.path.split(os.path.realpath(__file__))[0]
        self.write(open(os.path.join(cur_dir, "scan_status.json")).read())

    @web.authenticated
    def post(self):
        self.resp(403, "Please get this url")



class NodeCreateHandler(BaseHandler):
    def get (self):
        self.resp(404, "Please post to this url\n")

    @web.authenticated
    def post(self):
        node_name = self.get_argument("name","").strip()
        if not node_name:
            self.resp(400, "Missing node name information\n")
            return

        user = self.current_user
        email = user["email"]
        user_id = user["user_id"]
        node_sn = md5.new(email+self.get_uuid()).hexdigest()
        node_key = md5.new(self.gen_token(email)).hexdigest()  #we need the key to be 32bytes long too

        cur = self.application.cur
        try:
            cur.execute("INSERT INTO nodes(user_id,node_sn,name,private_key) VALUES('%s','%s','%s','%s')"%(user_id, node_sn,node_name, node_key))
            self.resp(200, "Node created",{"node_sn":node_sn,"node_key": node_key})
        except Exception,e:
            self.resp(500,str(e))
            return
        finally:
            self.application.conn.commit()


class NodeListHandler(BaseHandler):
    @web.authenticated
    def get (self):
        user = self.current_user
        email = user["email"]
        user_id = user["user_id"]

        cur = self.application.cur
        try:
            cur.execute("select node_sn, name, private_key from nodes where user_id='%s'"%(user_id))
            rows = cur.fetchall()
            nodes = []
            for r in rows:
                nodes.append({"name":r["name"],"node_sn":r["node_sn"],"node_key":r['private_key']})
            self.resp(200, meta={"nodes": nodes})
        except Exception,e:
            self.resp(500,str(e))
            return

    def post(self):
        self.resp(404, "Please get this url\n")

class NodeRenameHandler(BaseHandler):
    def get (self):
        self.resp(404, "Please post to this url\n")

    @web.authenticated
    def post(self):
        new_node_name = self.get_argument("name","").strip()
        if not new_node_name:
            self.resp(400, "Missing node name information\n")
            return

        user = self.current_user
        user_id = user["user_id"]

        cur = self.application.cur
        try:
            cur.execute("UPDATE nodes set name='%s' WHERE user_id='%s'" %(new_node_name, user_id))
            self.resp(200, "Node renamed",{"name": new_node_name})
        except Exception,e:
            self.resp(500,str(e))
            return
        finally:
            self.application.conn.commit()

class NodeDeleteHandler(BaseHandler):
    @web.authenticated
    def get (self):
        self.resp(404, "Please post to this url\n")


    def post(self):
        node_sn = self.get_argument("node_sn","").strip()
        if not node_sn:
            self.resp(400, "Missing node sn information\n")
            return

        user = self.current_user
        user_id = user["user_id"]

        cur = self.application.cur
        try:
            cur.execute("delete from nodes where user_id=%d and node_sn='%s'"%(user_id, node_sn))
            if cur.rowcount > 0:
                self.resp(200, "node deleted")
            else:
                self.resp(200, "node not exist")
        except Exception,e:
            self.resp(500,str(e))
            return
        finally:
            self.application.conn.commit()

class NodeBaseHandler(BaseHandler):

    def initialize (self, conns):
        self.conns = conns

    def get_node (self):
        node = None
        token = self.get_argument("access_token","")
        if not token:
            try:
                token_str = self.request.headers.get("Authorization")
                token = token_str.replace("token ","")
            except:
                token = None
        print token
        if token:
            try:
                cur = self.application.cur
                cur.execute('select * from nodes where private_key="%s"'%token)
                rows = cur.fetchall()
                if len(rows) > 0:
                    node = rows[0]
            except:
                node = None
        else:
            node = None

        print "get current node:", str(node)
        if not node:
            self.resp(403,"Please attach the valid node token (not the user token)")
        return node



class NodeReadWriteHandler(NodeBaseHandler):

    @gen.coroutine
    def get(self, uri):

        uri = uri.split("?")[0]
        print "get:",uri


        node = self.get_node()
        if not node:
            return

        for conn in self.conns:
            if conn.sn == node['node_sn'] and not conn.killed:
                try:
                    cmd = "GET /%s\r\n"%(uri)
                    cmd = cmd.encode("ascii")
                    ok, resp = yield conn.submit_and_wait_resp (cmd, "resp_get")
                    self.resp(200,resp)
                except Exception,e:
                    print e
                return
        self.resp(404, "Node is offline")

    @gen.coroutine
    def post (self, uri):

        uri = uri.split("?")[0].rstrip("/")
        print "post to:",uri

        node = self.get_node()
        if not node:
            return

        try:
            json_obj = json.loads(self.request.body)
        except:
            json_obj = None

        cmd_args = ""

        if json_obj:
            print "post request:", json_obj

            if not isinstance(json_obj, dict):
                self.resp(400, "Bad format of json: must be key/value pair.")
                return
            for k,v in json_obj.items():
                cmd_args += str(v)
                cmd_args += "/"
        else:

            arg_list = self.request.body.split('&')

            print "post request:", arg_list

            if len(arg_list) == 1 and arg_list[0] == "":
                arg_list = []

            for arg in arg_list:
                if arg.find('=') > -1:
                    value = arg.split('=')[1]
                    cmd_args += value
                    cmd_args += "/"
                else:
                    cmd_args += arg
                    cmd_args += "/"


        for conn in self.conns:
            if conn.sn == node['node_sn'] and not conn.killed:
                try:
                    cmd = "POST /%s/%s\r\n"%(uri, cmd_args)
                    cmd = cmd.encode("ascii")
                    ok, resp = yield conn.submit_and_wait_resp (cmd, "resp_post")
                    self.resp(200,resp)
                except Exception,e:
                    print e
                return

        self.resp(404, "Node is offline")

class NodeEventHandler(websocket.WebSocketHandler):
    def __init__(self):
        self.cur_conn = None
        self.node_key = None
        self.connected = False

    def open(self):
        self.connected = True
        print "open websocket"

        pass

    def on_close(self):
        self.connected = False

    @gen.coroutine
    def on_message(self, message):
        self.node_key = message
        for conn in self.conns:
            if conn.private_key == self.node_key and not conn.killed:
                cur_conn = conn
                break

        while self.connected
            event = yield self.wait_event_post()
            print "enent:", event
            self.write_message(event)
            yield gen.moment

    def wait_event_post(self):
        result_future = Future()
        if len(self.cur_conn.event_queue) > 0:
            result_future.set_result(self.cur_conn.event_queue.pop(0))

        return result_future


class UserDownloadHandler(NodeBaseHandler):
    """
    post two para, node_token and yaml

    """

    def get (self):
        self.resp(404, "Please post to this url\n")

    @gen.coroutine
    def post(self):
        node = self.get_node()
        if not node:
            return

        cur_conn = None

        for conn in self.conns:
            if conn.private_key == node['private_key'] and not conn.killed:
                cur_conn = conn
                break

        if not cur_conn:
            self.resp(404, "Node is offline")
            return

        self.cur_conn = cur_conn

        yaml = self.get_argument("yaml","")
        if not yaml:
            self.resp(400, "Missing yaml information\n")
            return

        user_id = node["user_id"]
        node_name = node["name"]

        pass # test node id is valid?

        try:
            yaml = base64.b64decode(yaml)
        except:
            yaml = ""

        print yaml
        if not yaml:
            gen_log.error("no valid yaml provided")
            self.resp(400, "no valid yaml provided")
            return

        cur_dir = os.path.split(os.path.realpath(__file__))[0]
        user_build_dir = cur_dir + '/users_build/' + str(user_id)
        if not os.path.exists(user_build_dir):
            os.makedirs(user_build_dir)

        yaml_file = open("%s/connection_config.yaml"%user_build_dir, 'wr')
        yaml_file.write(yaml)

        yaml_file.close()

        copy('%s/users_build/local_user/Makefile'%cur_dir, user_build_dir)

        self.request.connection.stream.io_loop.add_callback(self.ota_process, user_id, node_name, node['node_sn'])

        self.resp(200,"",meta={'ota_status': "going", "ota_msg": "Building firmware.."})


    def ota_process (self, user_id, node_name, node_sn):
        thread_name = "build_thread-" + str(user_id)
        li = threading.enumerate()
        for l in li:
            if l.getName() == thread_name:
                print 'INFO: Skip same request!'
                return

        threading.Thread(target=self.build_thread, name=thread_name, 
            args=(user_id, node_name, node_sn)).start()
        

    # @gen.coroutine
    def build_thread (self, user_id, node_name, node_sn):
        if not self.cur_conn:
            self.resp(404, "Node is offline")
            return

        if not gen_and_build(str(user_id), node_name):
            error_msg = get_error_msg()
            gen_log.error(error_msg)
            print error_msg
            #save state
            state = ("error", "build error: "+error_msg)
            if len(self.cur_conn.state_waiters) == 0:
                self.cur_conn.state_happened.append(state)
            else:
                self.cur_conn.state_waiters.pop(0).set_result(state)

            return

        #save state
        state = ("going", "Notifing the node...")
        print state
        if len(self.cur_conn.state_waiters) == 0:
            self.cur_conn.state_happened.append(state)
        else:
            self.cur_conn.state_waiters.pop(0).set_result(state)

        # go OTA
        try:
            cmd = "OTA\r\n"
            cmd = cmd.encode("ascii")
            self.cur_conn.submit_cmd (cmd)
        except Exception,e:
            print e
            #save state
            state = ("error", "notify error: "+str(e))
            if len(self.cur_conn.state_waiters) == 0:
                self.cur_conn.state_happened.append(state)
            else:
                self.cur_conn.state_waiters.pop(0).set_result(state)
        return



class OTAUpdatesHandler(NodeBaseHandler):

    def get (self):
        self.resp(404, "Please post to this url\n")

    @gen.coroutine
    def post(self):
        print "request ota status"

        node = self.get_node()
        if not node:
            return

        cur_conn = None

        for conn in self.conns:
            if conn.private_key == node['private_key'] and not conn.killed:
                cur_conn = conn
                break

        if not cur_conn:
            self.resp(404, "Node is offline")
            return

        self.cur_conn = cur_conn

        state = yield self.wait_ota_status_change()
        print "+++post state to app:", state

        if self.request.connection.stream.closed():
            return

        self.resp(200,"",{'ota_status': state[0], 'ota_msg': state[1]})

    def on_connection_close(self):
        # global_message_buffer.cancel_wait(self.future)
        print "on_connection_close"

    def wait_ota_status_change(self):
        result_future = Future()

        # get OTA status
        if len(self.cur_conn.state_happened) > 0:
            result_future.set_result(self.cur_conn.state_happened.pop(0))
        else:
            self.cur_conn.state_waiters.append(result_future)

        return result_future






class OTAHandler(BaseHandler):

    def initialize (self, conns):
        self.conns = conns

    def get_node (self):
        node = None
        sn = self.get_argument("sn","")
        if not sn:
            gen_log.error("ota bin request has no sn provided")
            return


        sn = sn[:-4]
        try:
            sn = base64.b64decode(sn)
        except:
            sn = ""

        if len(sn) != 32:
            gen_log.error("ota bin request has no valid sn provided")
            return

        if sn:
            try:
                cur = self.application.cur
                cur.execute('select * from nodes where node_sn="%s"'%sn)
                rows = cur.fetchall()
                if len(rows) > 0:
                    node = rows[0]
            except:
                node = None
        else:
            node = None

        print "get current node:", str(node)
        if not node:
            gen_log.error("can not find the specified node for ota bin request")
        return node


    @gen.coroutine
    def get(self):
        app = self.get_argument("app","")
        if not app or app not in [1,2,"1","2"]:
            gen_log.error("ota bin request has no app number provided")
            return

        node = self.get_node()
        if not node:
            return

        #get the user dir
        user_dir = os.path.join("users_build/",str(node['user_id']))

        #put user*.bin out
        self.write(open(os.path.join(user_dir, "user%s.bin"%str(app))).read())


    def post (self, uri):
        self.resp(404, "Please get this url.")


'''
for test
'''
class OTATrigHandler(BaseHandler):

    def initialize (self, conns):
        self.conns = conns


    @gen.coroutine
    def get (self):

        sn = self.get_argument("sn","")
        if not sn:
            gen_log.error("ota bin request has no sn provided")
            return
        print sn

        for conn in self.conns:
            if conn.sn == sn and not conn.killed:
                try:
                    cmd = "OTA\r\n"
                    cmd = cmd.encode("ascii")
                    ok, resp = yield conn.submit_and_wait_resp (cmd, "ota_trig_ack")
                    self.resp(200,resp)
                except Exception,e:
                    print e
                return
        self.resp(404, "Node is offline")






