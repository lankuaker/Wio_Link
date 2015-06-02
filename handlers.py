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

import os
from datetime import timedelta
import socket
import json
import sqlite3 as lite
import re
import jwt
import md5
import httplib
import uuid

from tornado.httpserver import HTTPServer
from tornado.tcpserver import TCPServer
from tornado import ioloop
from tornado import gen
from tornado import iostream
from tornado import web
from tornado.options import define, options
from tornado.log import *

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
        node_key = self.gen_token(email)

        cur = self.application.cur
        try:
            cur.execute("INSERT INTO nodes(user_id,node_sn,name,private_key) VALUES('%s','%s','%s','%s')"%(user_id, node_sn,node_name, node_key))
            self.resp(200, "Node created",{"server_key":TOKEN_SECRET,"node_sn":node_sn,"node_key": node_key})
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
                nodes.append({"name":r["name"], "server_key":TOKEN_SECRET,"node_sn":r["node_sn"],"node_key":r['private_key']})
            self.resp(200, meta={"nodes": nodes})
        except Exception,e:
            self.resp(500,str(e))
            return

    def post(self):
        self.resp(404, "Please get this url\n")


class NodeReadWriteHandler(BaseHandler):

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



    @gen.coroutine
    def get(self, uri):

        uri = uri.split("?")[0]
        print "get:",uri


        node = self.get_node()
        if not node:
            return

        for conn in self.conns:
            if conn.node_token == node['token']:
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
    def post (self, node_id, grove_name, method):
        print "post", node_id, grove_name, method, self.request.body

        if not self.get_node():
            return

        if self.request.body.find("&") > 0 and self.request.body.find(",") > 0:
            self.resp(400, "Bad format of the post body. Allowed format:arg1=value1&arg2=value2 in order.")
            return

        if self.request.body.find("&") > 0:
            arg_list = self.request.body.split('&')
        else:
            arg_list = self.request.body.split(',')

        if len(arg_list) == 1 and arg_list[0] == "":
            arg_list = []

        cmd_args = ""
        for arg in arg_list:
            if arg.find('=') > -1:
                value = arg.split('=')[1]
                cmd_args += value
                cmd_args += "/"
            else:
                cmd_args += arg
                cmd_args += "/"


        for conn in DeviceServer.accepted_conns:
            if conn.node_id == node_id:
                try:
                    cmd = "POST /%s/%s/%s\r\n"%(grove_name, method, cmd_args)
                    cmd = cmd.encode("ascii")
                    ok, resp = yield conn.submit_and_wait_resp (cmd, "resp_post")
                    self.write(resp)
                except Exception,e:
                    print e
                return

        self.resp(404, "Node is offline")


class UserDownloadHandler(BaseHandler):
    """
    post two para, project name  and  yaml file

    """
    def get (self):
        self.resp(404, "Please post to this url\n")

    @web.authenticated
    def post(self):
        node_sn = self.get_argument("node_sn","")
        if not node_sn:
            self.resp(400, "Missing node_sn information\n")
            return

        if self.request.files.get('yaml', None):
            uploadFile = self.request.files['yaml'][0]
            filename = uploadFile['filename']
        else:
            self.resp(400, "Missing Yaml file information\n")
            return

        user = self.current_user
        email = user["email"]

        # create user dir and project subdir
        projectDir = 'users_build' + '/' + email
        if not os.path.exists(projectDir):
            os.makedirs(projectDir) 

        # create yaml file on user dir
        configYaml = open(projectDir + '/' + filename, 'wb')
        configYaml.write(uploadFile['body'])

        # add required file, such as Makefile
        
        # go make

        self.resp(200, "User download",{"node_sn": node_sn})

