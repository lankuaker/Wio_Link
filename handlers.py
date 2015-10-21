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
import hashlib
import base64
import httplib
import uuid
from shutil import copy
from build_firmware import *
import yaml
import threading
import time
import smtplib
import config as server_config

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

        gen_log.info("get current user"+ str(user))
        if not user:
            self.resp(403,"Please login to get the token")
        return user



    '''
    205 - custom, get response from node but with error
    400 - bad variables / format
    401 - unauthorized
    403 - forbidden
    404 - not found
    500 - server internal error
    '''
    def resp (self, status, msg="",meta={}):
        response = {"status":status,"msg":msg}
        response = dict(response, **meta)
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

class UserRetrievePasswordHandler(BaseHandler):
    def get(self):
        self.retrieve()

    def post(self):
        self.retrieve()
        
    def retrieve(self):
        email = self.get_argument('email','')
        if not email:
            self.resp(400, "You must specify the email of the account which you want to retrieve password.\n")
            return

        if not re.match(r'^[_.0-9a-z-]+@([0-9a-z][0-9a-z-]+.)+[a-z]{2,4}$', email):
            self.resp(400, "Bad email address\n")
            return

        gen_log.info("%s want to retrieve password"%(email))

        cur = self.application.cur
        try:
            cur.execute('select * from users where email="%s"' % email)
            row = cur.fetchone()
            if not row:
                self.resp(401, "No account registered with this email\n")
                return

            new_password = self.gen_token(email)[0:6]
            cur.execute('update users set pwd="%s" where email="%s"' % (md5.new(new_password).hexdigest(), email))

            #start a thread sending email here
            self.request.connection.stream.io_loop.add_callback(self.start_thread_send_email, email, new_password)

            self.resp(200, "The password has been sent to your email address\n")

        except Exception,e:
            self.resp(500, str(e))
            return
        finally:
            self.application.conn.commit()


    def start_thread_send_email (self, email, new_password):
        thread_name = "email_thread-" + str(email)
        li = threading.enumerate()
        for l in li:
            if l.getName() == thread_name:
                gen_log.info('INFO: Skip same email request!')
                return

        threading.Thread(target=self.email_sending_thread, name=thread_name, 
            args=(email, new_password)).start()
        

    def email_sending_thread (self, email, new_password):
        s = smtplib.SMTP_SSL(server_config.smtp_server)
        try:
            s.login(server_config.smtp_user, server_config.smtp_pwd)
        except Exception,e:
            gen_log.error(e)
            return

        sender = 'no_reply@seeed.cc'
        receiver = email

        message = """From: Pion_One <%s>
To: <%s>
Subject: The password for your account of iot.seeed.cc has been retrieved

Dear User,

Thanks for your interest in iot.seeed.cc, the new password for your account is
%s

Please change it as soon as possible.

Thank you!

IOT Team from Seeed

""" % (sender, receiver, new_password)
        try:
            s.sendmail(sender, receiver, message)
        except Exception,e:
            gen_log.error(e)
            return
        gen_log.info('sent new password %s to %s' % (new_password, email))

        

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
            self.resp(200, "Logged in",{"token": row["token"], "user_id": row["user_id"]})
        except Exception,e:
            self.resp(500,str(e))
            return
        finally:
            self.application.conn.commit()

class DriversHandler(BaseHandler):
    @web.authenticated
    def get (self):
        cur_dir = os.path.split(os.path.realpath(__file__))[0]
        self.write(open(os.path.join(cur_dir, "drivers.json")).read())

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
    def initialize (self, conns):
        self.conns = conns

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
                online = False
                for conn in self.conns:
                    if r['node_sn'] == conn.sn and conn.online_status == True:
                        online = True
                        break
                nodes.append({"name":r["name"],"node_sn":r["node_sn"],"node_key":r['private_key'], "online":online})
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
        node_sn = self.get_argument("node_sn","").strip()
        if not node_sn:
            self.resp(400, "Missing node sn information\n")
            return

        new_node_name = self.get_argument("name","").strip()
        if not new_node_name:
            self.resp(400, "Missing node name information\n")
            return

        cur = self.application.cur
        try:
            cur.execute("UPDATE nodes set name='%s' WHERE node_sn='%s'" %(new_node_name, node_sn))
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

    @web.authenticated
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

    def initialize (self, conns, state_waiters, state_happened):
        self.conns = conns
        self.state_waiters = state_waiters
        self.state_happened = state_happened

    def get_current_user(self):
        return None

    def get_node (self):
        node = None
        token = self.get_argument("access_token","")
        if not token:
            try:
                token_str = self.request.headers.get("Authorization")
                token = token_str.replace("token ","")
            except:
                token = None
        gen_log.debug("node token:"+ str(token))
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

        if not node:
            self.resp(403,"Please attach the valid node token (not the user token)")
        else:
            gen_log.info("get current node, id: %d, name: %s" % (node['node_id'],node["name"]))

        return node



class NodeReadWriteHandler(NodeBaseHandler):

    @gen.coroutine
    def get(self, ignore, uri):

        gen_log.debug(ignore)

        uri = uri.split("?")[0]
        gen_log.debug("get:"+ str(uri))


        node = self.get_node()
        if not node:
            return

        for conn in self.conns:
            if conn.sn == node['node_sn'] and not conn.killed:
                try:
                    cmd = "GET /%s\r\n"%(uri)
                    cmd = cmd.encode("ascii")
                    ok, resp = yield conn.submit_and_wait_resp (cmd, "resp_get")
                    if 'status' in resp:
                        self.resp(resp['status'],resp['msg'])
                    else:
                        self.resp(200,resp['msg'])
                except Exception,e:
                    gen_log.error(e)
                return
        self.resp(404, "Node is offline")

    @gen.coroutine
    def post (self, ignore, uri):

        uri = uri.split("?")[0].rstrip("/")
        gen_log.info("post to:"+ str(uri))

        node = self.get_node()
        if not node:
            return

        try:
            json_obj = json.loads(self.request.body)
        except:
            json_obj = None

        cmd_args = ""

        if json_obj:
            gen_log.info("post request:"+ str(json_obj))

            if not isinstance(json_obj, dict):
                self.resp(400, "Bad format of json: must be key/value pair.")
                return
            for k,v in json_obj.items():
                cmd_args += str(v)
                cmd_args += "/"
        else:

            arg_list = self.request.body.split('&')

            gen_log.info("post request:"+ str(arg_list))

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
                    cmd = ("/%s/%s"%(uri, cmd_args)).rstrip("/")
                    cmd = "POST %s\r\n"%(cmd)
                    cmd = cmd.encode("ascii")
                    ok, resp = yield conn.submit_and_wait_resp (cmd, "resp_post")
                    if 'status' in resp:
                        self.resp(resp['status'],resp['msg'])
                    else:
                        self.resp(200,resp['msg'])
                except Exception,e:
                    gen_log.error(e)
                return

        self.resp(404, "Node is offline")

class NodeEventHandler(websocket.WebSocketHandler):
    def initialize (self, conns):
        self.conns = conns
        self.cur_conn = None
        self.node_key = None
        self.connected = False
        self.future = None

    def check_origin(self, origin):
        return True

    def open(self):
        gen_log.info("websocket open")
        self.connected = True

    def on_close(self):
        gen_log.info("websocket close")
        if self.connected and self.cur_conn:
            cur_waiters = self.cur_conn.event_waiters
            if self.future in cur_waiters:
                self.future.set_result(None)
                cur_waiters.remove(self.future)
                # cancel yield
                pass

        self.connected = False

    @gen.coroutine
    def on_message(self, message):
        self.node_key = message
        for conn in self.conns:
            if conn.private_key == self.node_key and not conn.killed:
                self.cur_conn = conn
                break

        if not self.cur_conn:
            message = {"Node":"offline"}
            self.write_message(message)
            self.connected = False
            self.close()
            return

        while self.connected:
            self.future = self.wait_event_post()
            event = yield self.future
            if event:
                self.write_message(event)
            yield gen.moment

    def wait_event_post(self):
        result_future = Future()

        if len(self.cur_conn.event_happened) > 0:
            result_future.set_result(self.cur_conn.event_happened.pop(0))
        else:
            self.cur_conn.event_waiters.append(result_future)

        return result_future


class NodeGetConfigHandler(NodeBaseHandler):

    def get(self):
        node = self.get_node()
        if not node:
            return

        user_id = node["user_id"]
        node_name = node["name"]
        node_sn = node["node_sn"]

        cur_dir = os.path.split(os.path.realpath(__file__))[0]
        user_build_dir = cur_dir + '/users_build/' + str(user_id)  + '_' + node_sn
        if not os.path.exists(user_build_dir):
            self.resp(404, "Config not found\n")
            return

        try:
            yaml_file = open("%s/connection_config.yaml"%user_build_dir, 'r')
            self.resp(200, yaml_file.read())
        except Exception,e:
            gen_log.error("Exception when reading yaml file:"+ str(e))
            self.resp(404, "Config not found\n")


    def post(self):
        self.resp(404, "Please get this url\n")


class NodeGetResourcesHandler(NodeBaseHandler):

    vhost_url_base = 'https://iot.seeed.cc'

    def prepare_data_for_template(self, node, grove_instance_name, grove, grove_doc):

        data = []
        events = []

        methods_doc = grove_doc['Methods']
        for fun in grove['Outputs'].items():
            item = {}
            item['type'] = 'GET'
            #build read arg
            arguments = []
            method_name = fun[0].replace('read_','')
            url = self.vhost_url_base + '/v1/node/' + grove_instance_name + '/' + method_name
            arg_list = [arg for arg in fun[1] if arg.find("*") < 0]  #find out the ones dont have "*"
            for arg in arg_list:
                if not arg:
                    continue
                t = arg.strip().split(' ')[0]
                name = arg.strip().split(' ')[1]
                
                if fun[0] in methods_doc and name in methods_doc[fun[0]]:
                    comment = ", " + methods_doc[fun[0]][name]
                else:
                    comment = ""

                arguments.append('[%s]: %s value%s' % (name, t, comment))
                url += ('/[%s]' % name)

            item['url'] = url + '?access_token=' + node['private_key']
            item['arguments'] = arguments

            item['brief'] = ""
            if fun[0] in methods_doc and '@brief@' in methods_doc[fun[0]]: 
                item['brief'] = methods_doc[fun[0]]['@brief@']


            #build returns
            returns = ""
            return_docs = []
            arg_list = [arg for arg in fun[1] if arg.find("*")>-1]  #find out the ones have "*"
            for arg in arg_list:
                if not arg:
                    continue
                t = arg.strip().replace('*', '').split(' ')[0]
                name = arg.strip().replace('*', '').split(' ')[1]
                returns += ('"%s": [%s value],' % (name, t))

                if fun[0] in methods_doc and name in methods_doc[fun[0]]:
                    comment = ", " + methods_doc[fun[0]][name]
                    return_docs.append('%s: %s value%s' % (name, t, comment))


            returns = returns.rstrip(',')
            item['returns'] = returns
            item['return_docs'] = return_docs

            data.append(item)

        for fun in grove['Inputs'].items():
            item = {}
            item['type'] = 'POST'
            #build write arg
            arguments = []
            method_name = fun[0].replace('write_','')
            url = self.vhost_url_base + '/v1/node/' + grove_instance_name + '/' + method_name
            #arg_list = [arg for arg in fun[1] if arg.find("*")<0]  #find out the ones havent "*"
            arg_list = fun[1]
            for arg in arg_list:
                if not arg:
                    continue
                string_type = True if arg.find("char *") >= 0 else False
                if string_type: 
                    t = "char *"
                else:
                    t = arg.strip().replace('*', '').split(' ')[0]
                name = arg.strip().replace('*', '').split(' ')[1]

                if fun[0] in methods_doc and name in methods_doc[fun[0]]:
                    comment = ", " + methods_doc[fun[0]][name]
                else:
                    comment = ""

                arguments.append('[%s]: %s value%s' % (name, t, comment))
                url += ('/[%s]' % name)
            item['url'] = url + '?access_token=' + node['private_key']
            item['arguments'] = arguments

            item['brief'] = ""
            if fun[0] in methods_doc and '@brief@' in methods_doc[fun[0]]: 
                item['brief'] = methods_doc[fun[0]]['@brief@']

            data.append(item)

        if grove['HasEvent']:
            events += grove['Events']

        return (data, events)


    def get(self):
        node = self.get_node()
        if not node:
            return

        user_id = node["user_id"]
        node_name = node["name"]
        node_id = node['node_id']
        node_sn = node['node_sn']

        cur_dir = os.path.split(os.path.realpath(__file__))[0]
        user_build_dir = cur_dir + '/users_build/' + str(user_id) + '_' + node_sn
        if not os.path.exists(user_build_dir):
            self.resp(404, "Configuration file not found\n")
            return

        #open the yaml file for reading
        try:
            config_file = open('%s/connection_config.yaml'%user_build_dir,'r')
        except Exception,e:
            gen_log.error("Exception when reading yaml file:"+ str(e))
            self.resp(404, "No resources, the node has not been configured jet.\n")
            return

        #open the json file for reading
        try:
            drv_db_file = open('%s/drivers.json' % cur_dir,'r')
            drv_doc_file= open('%s/driver_docs.json' % cur_dir,'r')
        except Exception,e:
            gen_log.error("Exception when reading grove drivers database file:"+ str(e))
            self.resp(404, "Internal error, the grove drivers database file is corrupted.\n")
            return

        #calculate the checksum of 2 file
        sha1 = hashlib.sha1()
        sha1.update(config_file.read())
        chksum_config = sha1.hexdigest()
        sha1 = hashlib.sha1()
        sha1.update(drv_db_file.read() + drv_doc_file.read())
        chksum_drv_db = sha1.hexdigest()

        #query the database, if 2 file not changed, echo cached html
        resource = None
        try:
            cur = self.application.cur
            cur.execute('select * from resources where node_id="%s"'%node_id)
            rows = cur.fetchall()
            if len(rows) > 0:
                resource = rows[0]
        except:
            resource = None

        if resource:
            if chksum_config == resource['chksum_config'] and chksum_drv_db == resource['chksum_dbjson']:
                gen_log.info("echo the cached page for node_id: %d" % node_id)
                self.write(resource['render_content'])
                return

        gen_log.info("re-render the resource page for node_id: %d" % node_id)

        #else render new resource page
        #load the yaml file into object
        config_file.seek(0)
        drv_db_file.seek(0)
        drv_doc_file.seek(0)
        try:
            config = yaml.load(config_file)
        except yaml.YAMLError, err:
            gen_log.error("Error in parsing yaml file:"+ str(err))
            self.resp(404, "No resources, the configuration file is corrupted.\n")
            return
        except Exception,e:
            gen_log.error("Error in loading yaml file:"+ str(e))
            self.resp(404, "No resources, the configuration file is corrupted.\n")
            return

        #load the json file into object
        try:
            drv_db = json.load(drv_db_file)
            drv_docs = json.load(drv_doc_file)
        except Exception,e:
            gen_log.error("Error in parsing grove drivers database file:"+ str(e))
            self.resp(404, "Internal error, the grove drivers database file is corrupted.\n")
            return

        #prepare resource data structs
        data = []
        events = []
        self.vhost_url_base = server_config.vhost_url_base.rstrip('/')
        #self.vhost_url_base = self.get_argument("data_server", server_config.vhost_url_base.rstrip('/'))
        #if self.vhost_url_base.find('http') < 0:
        #    self.vhost_url_base = 'https://'+self.vhost_url_base
        #print self.vhost_url_base

        if config:
            for grove_instance_name in config.keys():
                grove = find_grove_in_database(config[grove_instance_name]['name'], drv_db)
                if grove:
                    grove_doc = find_grove_in_docs_db(grove['ID'], drv_docs)
                    if grove_doc:
                        d, e = self.prepare_data_for_template(node,grove_instance_name,grove,grove_doc)
                        data += d
                        events += e

                    else:  #if grove_doc
                        error_msg = "Error, cannot find %s in grove driver documentation database file."%grove_instance_name
                        gen_log.error(error_msg)
                        self.resp(404, error_msg)
                        return
                    
                else:  #if grove
                    error_msg = "Error, cannot find %s in grove drivers database file."%grove_instance_name
                    gen_log.error(error_msg)
                    self.resp(404, error_msg)
                    return

        #render the template
        domain = self.vhost_url_base.replace('https:', 'wss:')
        domain = domain.replace('http:', 'ws:')
        page = self.render_string('resources.html', node_name = node_name, events = events, data = data, 
                                  node_sn = node['node_sn'] , url_base = self.vhost_url_base, domain=domain)

        #store the page html into database
        try:
            cur = self.application.cur
            if resource:
                cur.execute('update resources set chksum_config=?,chksum_dbjson=?,render_content=? where node_id=?',
                            (chksum_config, chksum_drv_db, page, node_id))
            else:
                cur.execute('insert into resources (node_id, chksum_config, chksum_dbjson, render_content) values(?,?,?,?)',
                            (node_id, chksum_config, chksum_drv_db, page))
        except:
            resource = None
        finally:
            self.application.conn.commit()

        #echo out
        self.write(page)



    def post(self):
        self.resp(404, "Please get this url\n")


class FirmwareBuildingHandler(NodeBaseHandler):
    """
    post two para, node_token and yaml

    """

    def get (self):
        self.resp(404, "Please post to this url\n")

    @gen.coroutine
    def post(self):
        gen_log.info("FirmwareBuildingHandler")
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
        node_id = node["node_id"]
        node_sn = node["node_sn"]
        self.node_sn = node_sn

        try:
            yaml = base64.b64decode(yaml)
        except:
            yaml = ""

        gen_log.debug(yaml)
        if not yaml:
            gen_log.error("no valid yaml provided")
            self.resp(400, "no valid yaml provided")
            return

        cur_dir = os.path.split(os.path.realpath(__file__))[0]
        user_build_dir = cur_dir + '/users_build/' + str(user_id) + '_' + node_sn
        if not os.path.exists(user_build_dir):
            os.makedirs(user_build_dir)

        yaml_file = open("%s/connection_config.yaml"%user_build_dir, 'wr')
        yaml_file.write(yaml)

        yaml_file.close()

        copy('%s/Makefile.template'%cur_dir, '%s/Makefile'%user_build_dir)

        #try to get the current running app number
        app_num = 'ALL'
        try:
            cmd = "APP\r\n"
            ok, resp = yield conn.submit_and_wait_resp (cmd, "resp_app")
            if ok and resp['msg'] in [1,2,'1','2']:
                app_num = '1' if resp['msg'] in [2,'2'] else '2'
                gen_log.info('Get to know node %d is running app %s' % (node_id, resp['msg']))
            else:
                gen_log.warn('Failed while getting app number for node %d: %s' % (node_id, str(resp)))
        except Exception,e:
            gen_log.error(e)

        server_ip = server_config.server_ip.replace('.', ',')

        self.ioloop = self.request.connection.stream.io_loop
        self.request.connection.stream.io_loop.add_callback(self.ota_process, app_num, user_id, node_name, node_sn, server_ip)

        #clear the possible old state recv during last ota process
        try:
            self.state_happened[self.node_sn] = []
            self.state_waiters[self.node_sn] = []
        except Exception,e:
            pass

        #log the building
        try:
            cur = self.application.cur
            cur.execute("insert into builds (node_id, build_date, build_starttime, build_endtime) \
                        values(?,date('now'),datetime('now'),datetime('now'))", (node_id, ))
        except Exception,e:
            gen_log.error("Failed to log the building record: %s" % str(e))
        finally:
            self.application.conn.commit()

        #echo the response first
        self.resp(200,"",meta={'ota_status': "going", "ota_msg": "Building the firmware.."})


    def ota_process (self, app_num, user_id, node_name, node_sn, server_ip):
        thread_name = "build_thread_" + node_sn
        li = threading.enumerate()
        for l in li:
            if l.getName() == thread_name:
                msg = 'Skip the building request from the same node, sn %s' % node_sn
                gen_log.info(msg)
                state = ("error", msg)
                self.send_notification(state)
                return

        threading.Thread(target=self.build_thread, name=thread_name, 
            args=(app_num, user_id, node_name, node_sn, server_ip)).start()
        

    @gen.coroutine
    def build_thread (self, app_num, user_id, node_name, node_sn, server_ip):

        if not gen_and_build(app_num, str(user_id), node_sn, node_name, server_ip):
            error_msg = get_error_msg()
            gen_log.error(error_msg)
            #save state
            state = ("error", "Build error: "+error_msg)
            self.send_notification(state)

            return

        #query the connection status again
        if self.cur_conn not in self.conns:
            cur_conn = None

            for conn in self.conns:
                if conn.sn == node_sn and not conn.killed:
                    cur_conn = conn
                    break
            self.cur_conn = cur_conn

        if not self.cur_conn or self.cur_conn not in self.conns:
            gen_log.info('Node is offline, sn: %s, name: %s' % (node_sn, node_name))
            state = ("error", "Node is offline")
            self.send_notification(state)
            return

        # go OTA
        retries = 0
        while(retries < 6 and not self.cur_conn.killed):
            try:
                state = ("going", "Notifying the node...[%d]" % retries)
                self.send_notification(state)

                self.cur_conn.ota_notify_done_future = Future()

                cmd = "OTA\r\n"
                cmd = cmd.encode("ascii")
                self.cur_conn.submit_cmd (cmd)
                
                yield gen.with_timeout(timedelta(seconds=10), self.cur_conn.ota_notify_done_future, io_loop=self.ioloop)
                break
            except gen.TimeoutError:
                pass
            except Exception,e:
                gen_log.error(e)
                #save state
                state = ("error", "notify error: "+str(e))
                self.send_notification(state)
                return
            retries = retries + 1
        
        if retries >= 6:  
            state = ("error", "Time out in notifying the node.")
            gen_log.info(state[1])
            self.send_notification(state)
        else:
            gen_log.info("Succeed in notifying node %d." % self.cur_conn.node_id)

    def send_notification(self, state):
        if self.state_waiters and self.node_sn in self.state_waiters and len(self.state_waiters[self.node_sn]) > 0:
            f = self.state_waiters[self.node_sn].pop(0)
            f.set_result(state)
            if len(self.state_waiters[self.node_sn]) == 0:
                del self.state_waiters[self.node_sn]
        elif self.state_happened and self.node_sn in self.state_happened:
            self.state_happened[self.node_sn].append(state)
        else:
            self.state_happened[self.node_sn] = [state]




class OTAStatusReportingHandler(NodeBaseHandler):

    def get (self):
        self.resp(404, "Please post to this url\n")

    @gen.coroutine
    def post(self):
        gen_log.info("request ota status")

        node = self.get_node()
        if not node:
            return

        #cur_conn = None
        #for conn in self.conns:
        #    if conn.private_key == node['private_key'] and not conn.killed:
        #        cur_conn = conn
        #        break
        #
        #if not cur_conn:
        #    self.resp(404, "Node is offline or lost its connection.")
        #    return
        #
        #self.cur_conn = cur_conn
        self.node_sn = node['node_sn']

        #state = yield self.wait_ota_status_change()
        #state_future = self.wait_ota_status_change()
        state = None
        state_future = None
        if self.state_happened and self.node_sn in self.state_happened and len(self.state_happened[self.node_sn]) > 0:
            state = self.state_happened[self.node_sn].pop(0)
            if len(self.state_happened[self.node_sn]) == 0:
                del self.state_happened[self.node_sn]
        else:
            state_future = Future()
            if self.state_waiters and self.node_sn in self.state_waiters:
                self.state_waiters[self.node_sn].append(state_future)
            else:
                self.state_waiters[self.node_sn] = [state_future]


        if not state and state_future:
            #print state_future
            try:
                state = yield gen.with_timeout(timedelta(seconds=300), state_future, io_loop=self.request.connection.stream.io_loop)
            except gen.TimeoutError:
                state = ("error", "Time out.")
            except:
                pass

        gen_log.info("+++post state to app:"+ str(state))

        if self.request.connection.stream.closed():
            return

        self.resp(200,"",{'ota_status': state[0], 'ota_msg': state[1]})

    def on_connection_close(self):
        # global_message_buffer.cancel_wait(self.future)
        gen_log.info("on_connection_close")


class OTAFirmwareSendingHandler(BaseHandler):

    def initialize (self, conns, state_waiters, state_happened):
        self.conns = conns
        self.state_waiters = state_waiters
        self.state_happened = state_happened

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

        gen_log.info("get current node:"+ str(node))
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

        node_sn = node['node_sn']
        user_id = node['user_id']
        node_id = node['node_id']
        self.node_sn = node_sn

        #get the user dir and path of bin
        bin_path = os.path.join("users_build/",str(user_id) + '_' + str(node_sn), "user%s.bin"%str(app))

        #put user*.bin out
        self.set_header("Content-Type","application/octet-stream")
        self.set_header("Content-Transfer-Encoding", "binary")
        self.set_header("Content-Length", os.path.getsize(bin_path))

        with open(bin_path,"rb") as file:
            while True:
                try:
                    chunk_size = 64 * 1024
                    chunk = file.read(chunk_size)
                    if chunk:
                        self.write(chunk)
                        yield self.flush()
                    else:
                        gen_log.info("firmware bin sent done.")
                        break  
                except Exception,e:
                    gen_log.error('node %d error when sending binary file: %s' % (node_id, str(e)))
                    state = ('error', 'Error when sending binary file.')
                    self.send_notification(state)
                    self.clear()
                    break


    def post (self, uri):
        self.resp(404, "Please get this url.")

    def send_notification(self, state):
        if self.state_waiters and self.node_sn in self.state_waiters and len(self.state_waiters[self.node_sn]) > 0:
            f = self.state_waiters[self.node_sn].pop(0)
            f.set_result(state)
            if len(self.state_waiters[self.node_sn]) == 0:
                del self.state_waiters[self.node_sn]
        elif self.state_happened and self.node_sn in self.state_happened:
            self.state_happened[self.node_sn].append(state)
        else:
            self.state_happened[self.node_sn] = [state]








