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

import os
import sys
import re
import json
from datetime import *

def parse_one_driver_dir (driver_dir):
    files = []
    for f in os.listdir(driver_dir):
        full_path = os.path.join(driver_dir,f)
        if os.path.isfile(full_path) and (full_path.endswith(".h") or full_path.endswith(".cpp")):
            files.append(f)
    return files

def get_class_header_file (files):
    for f in files:
        if f.endswith(".h") and f.find("class") > -1:
            return f
    for f in files:
        if f.endswith(".h"):
            return f

    return ""

def parse_class_header_file (file):
    patterns = {}
    doc = {}
    print file
    content = open(file, 'r').read()

    json_dump = ""
    try:
        json_dump = json.dumps(content);
    except Exception,e:
        print e

    if not json_dump:
        try:
            content = unicode(content,"ISO-8859-1")
            json_dump = json.dumps(content)
        except Exception,e:
            print e
    if not json_dump:
        try:
            content = unicode(content,"GB2312")
            json_dump = json.dumps(content)
        except Exception,e:
            print e

    if not json_dump:
        return ("Encoding of source file is not one of: utf8,iso-8859-1,gb2312", {}, {})


    ##grove name
    grove_name = re.findall(r'^//GROVE_NAME\s+"(.+)"', content, re.M)
    print grove_name
    if grove_name:
        patterns["GroveName"] = grove_name[0].rstrip('\r')
    else:
        return ("can not find GROVE_NAME in %s"%file, {},{})

    ##SKU
    sku = re.findall(r'^//SKU\s+([a-zA-z0-9\-_]+)', content, re.M)
    print sku
    if sku:
        patterns["SKU"] = sku[0].rstrip('\r')
    else:
        return ("can not find SKU in %s"%file, {},{})


    ##interface type
    if_type = re.findall(r'^//IF_TYPE\s+([a-zA-z0-9]+)', content, re.M)
    print if_type
    if if_type:
        patterns["InterfaceType"] = if_type[0]
    else:
        return ("can not find IF_TYPE in %s"%file,{}, {})
    ##image url
    image_url = re.findall(r'^//IMAGE_URL\s+(.+)', content, re.M)
    print image_url
    if image_url:
        patterns["ImageURL"] = image_url[0].rstrip('\r')
    else:
        return ("can not find IMAGE_URL in %s"%file,{}, {})
    ##class name
    class_name = re.findall(r'^class\s+([a-zA-z0-9_]+)', content, re.M)
    print class_name
    if class_name:
        patterns["ClassName"] = class_name[0]
    else:
        return ("can not find class name in %s"%file,{}, {})
    ##construct function arg list
    arg_list = re.findall(r'%s\((.*)\);'%class_name[0], content, re.M)
    print arg_list
    if arg_list:
        patterns["ConstructArgList"] = [x.strip(" ") for x in arg_list[0].split(',')]
    else:
        return ("can not find construct arg list in %s"%file,{}, {})

    ## read functions
    read_functions = re.findall(r'^\s+bool\s+(read_[a-zA-z0-9_]+)\((.*)\).*$', content, re.M)
    print read_functions
    reads = {}
    for func in read_functions:
        args = func[1].split(',')
        args = [x.strip() for x in args]
        if 'void' in args:
            args.remove('void')
        reads[func[0]] = args
    patterns["Outputs"] = reads

    read_functions_with_doc = re.findall(r'(\/\*\*[\s\S]*?\*\/)[\s\S]*?bool\s+(read_[a-zA-z0-9_]+)\((.*)\).*$', content, re.M)
    print read_functions_with_doc
    for func in read_functions_with_doc:
        paras = re.findall(r'@param (\w*)[ ]?[-:]?[ ]?(.*)$', func[0], re.M)
        dict_paras = {}
        for p in paras:
            dict_paras[p[0]] = p[1]

        briefs = re.findall(r'(?!\* @)\* (.*)', func[0], re.M)
        brief = '\n'.join(briefs)
        dict_paras['@brief@'] = brief if brief.strip().strip('\n') else ""
        doc[func[1]] = dict_paras

    ## write functions
    write_functions = re.findall(r'^\s+bool\s+(write_[a-zA-z0-9_]+)\((.*)\).*$', content, re.M)
    print write_functions
    writes = {}
    for func in write_functions:
        args = func[1].split(',')
        args = [x.strip() for x in args]
        if 'void' in args:
            args.remove('void')
        writes[func[0]] = args
    patterns["Inputs"] = writes

    write_functions_with_doc = re.findall(r'(\/\*\*[\s\S]*?\*\/)[\s\S]*?bool\s+(write_[a-zA-z0-9_]+)\((.*)\).*$', content, re.M)
    print write_functions_with_doc
    for func in write_functions_with_doc:
        paras = re.findall(r'@param (\w*)[ ]?[-:]?[ ]?(.*)$', func[0], re.M)
        dict_paras = {}
        for p in paras:
            dict_paras[p[0]] = p[1]

        briefs = re.findall(r'(?!\* @)\* (.*)', func[0], re.M)
        brief = '\n'.join(briefs)
        dict_paras['@brief@'] = brief if brief.strip().strip('\n') else ""
        doc[func[1]] = dict_paras

    ## event
    #    EVENT_T * attach_event_reporter_for_ir_approached(CALLBACK_T reporter);
    event_attachments = re.findall(r'^\s+EVENT_T\s*\*\s*attach_event_reporter_for_(.*)\((.*)\).*$', content, re.M)
    print event_attachments
    events = []
    for ev in event_attachments:
        events.append(ev[0])

    patterns["Events"] = events

    if len(event_attachments) > 0:
        patterns["HasEvent"] = True
    else:
        patterns["HasEvent"] = False

    ## get last error
    get_last_error_func = re.findall(r'^\s+char\s*\*\s*get_last_error\(\s*\).*$', content, re.M)
    if len(get_last_error_func) > 0:
        patterns["CanGetLastError"] = True
    else:
        patterns["CanGetLastError"] = False

    return ("OK",patterns, doc)



## main ##

if __name__ == '__main__':

    print __file__
    cur_dir = os.path.split(os.path.realpath(__file__))[0]
    grove_drivers_abs_dir = os.path.abspath(cur_dir + "/grove_drivers")
    grove_database = []
    grove_docs = []
    failed = False
    failed_msg = ""
    grove_id = 0
    for f in os.listdir(grove_drivers_abs_dir):
        full_dir = os.path.join(grove_drivers_abs_dir, f)
        grove_info = {}
        grove_doc = {}
        if os.path.isdir(full_dir):
            print full_dir
            files = parse_one_driver_dir(full_dir)
            class_file = get_class_header_file(files)
            if class_file:
                result, patterns, doc = parse_class_header_file(os.path.join(full_dir,class_file))
                if patterns:
                    grove_info['ID'] = grove_id
                    grove_info['IncludePath'] = full_dir.replace(cur_dir, ".")
                    grove_info['Files'] = files
                    grove_info['ClassFile'] = class_file
                    grove_info = dict(grove_info, **patterns)
                    print grove_info
                    grove_database.append(grove_info)
                    grove_doc['ID'] = grove_id
                    grove_doc['Methods'] = doc
                    grove_doc['GroveName'] = grove_info['GroveName']
                    print grove_doc
                    grove_docs.append(grove_doc)
                    grove_id = grove_id + 1
                else:
                    failed_msg = "ERR: parse class file: %s"%result
                    failed = True
                    break
            else:
                failed_msg = "ERR: can not find class file of %s" % full_dir
                failed = True
                break

    #print grove_database

    print "========="
    print grove_docs
    print ""

    if not failed:
        open("%s/drivers.json"%cur_dir,"w").write(json.dumps(grove_database))
        open("%s/driver_docs.json"%cur_dir,"w").write(json.dumps(grove_docs))
        open("%s/scan_status.json"%cur_dir,"w").write('{"status":"OK", "msg":"scanned %d grove drivers at %s"}' % (len(grove_database), str(datetime.now())))
    else:
        print failed_msg
        open("%s/scan_status.json"%cur_dir,"w").write('{"status":"Failed", "msg":"%s"}' % (failed_msg))

