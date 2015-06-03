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

#dependence: PyYAML (pip install PyYaml)


import os
import sys
import re
import json

import yaml

GEN_DIR = '.'

TYPE_MAP = {
    'int':'TYPE_INT',
    'float':'TYPE_FLOAT',
    'bool':'TYPE_BOOL',
    'uint8_t':'TYPE_UINT8',
    'int8_t':'TYPE_INT8',
    'uint16_t':'TYPE_UINT16',
    'int16_t':'TYPE_INT16',
    'uint32_t':'TYPE_UINT32',
    'int32_t':'TYPE_INT32',
    'const char *':'TYPE_STRING',
    'char *':'TYPE_STRING'
    }

error_msg = ""

def find_grove_in_database (grove_name, json_obj):
    for grove in json_obj:
        if grove['GroveName'] == grove_name:
            return grove
    return {}

def declare_vars (arg_list):
    result = ""
    for arg in arg_list:
        result += arg.replace('*','')
        result += ';\r\n    '
    return result

def build_read_call_args (arg_list):
    result = ""
    for arg in arg_list:
        result += arg.strip().split(' ')[1].replace('*', '&')
        result += ','
    return result.rstrip(',')

def build_read_print (arg_list):
    global error_msg
    arg_list = [arg for arg in arg_list if arg.find("*")>-1]  #find out the ones have "*"
    result = '        writer_print(TYPE_STRING, "{");\r\n'
    cnt = len(arg_list)
    for i in xrange(cnt):
        t = arg_list[i].strip().split(' ')[0]
        name = arg_list[i].strip().split(' ')[1]
        if t in TYPE_MAP.keys():
            result += '        writer_print(TYPE_STRING, "\\"%s\\":");\r\n' %(name.replace('*',''))
            result += "        writer_print(%s, %s%s);\r\n" %(TYPE_MAP[t], name.replace('*', '&'), \
                                                              ', true' if i < (cnt-1) else '')
        else:
            error_msg = 'arg type %s not supported' % t
            #sys.exit()
            return ""
    result += '        writer_print(TYPE_STRING, "}");\r\n'
    return result

def build_unpack_vars (arg_list):
    global error_msg
    result = ""
    arg_list = [arg for arg in arg_list if arg.find("*") < 0]  #find out the ones dont have "*"
    for arg in arg_list:
        t = arg.strip().split(' ')[0]
        name = arg.strip().split(' ')[1]
        result += '    %s = *((%s *)arg_ptr); arg_ptr += sizeof(%s);\r\n' % (name, t, t)
    return result;

def build_read_with_arg (arg_list):
    global error_msg
    result = ""
    arg_list = [arg for arg in arg_list if arg.find("*") < 0]  #find out the ones dont have "*"
    for arg in arg_list:
        t = arg.strip().split(' ')[0]
        name = arg.strip().split(' ')[1]
        result += '/%s_%s' % (t, name)
    return result;

def build_return_values (arg_list):
    result = ""
    arg_list = [arg for arg in arg_list if arg.find("*")>-1]  #find out the ones have "*"
    for arg in arg_list:
        result += arg.strip().replace('*', ': ')
        result += ', '
    return result.rstrip(', ')

def build_reg_write_arg_type (arg_list):
    global error_msg
    result = ""
    arg_list = [arg for arg in arg_list if arg.find("*") < 0]
    length = min(4, len(arg_list))
    for i in xrange(length):
        t = arg_list[i].strip().split(' ')[0]
        if t in TYPE_MAP.keys():
            result += "    arg_types[%d] = %s;\r\n" %(i, TYPE_MAP[t])
        else:
            error_msg = 'arg type %s not supported' % t
            #sys.exit()
            return ""
    return result


def gen_wrapper_registration (instance_name, info, arg_list):
    global error_msg
    grove_name = info['GroveName'].lower()
    gen_header_file_name = grove_name+"_gen.h"
    gen_cpp_file_name = grove_name+"_gen.cpp"
    fp_wrapper_h = open(os.path.join(GEN_DIR, gen_header_file_name),'w')
    fp_wrapper_cpp = open(os.path.join(GEN_DIR, gen_cpp_file_name), 'w')
    str_reg_include = ""
    str_reg_method = ""
    str_wellknown = ""
    error_msg = ""

    #leading part
    fp_wrapper_h.write('#include "%s"\r\n\r\n' % info['ClassFile'])
    fp_wrapper_cpp.write('#include "%s"\r\n#include "%s"\r\n#include "%s"\r\n\r\n' % (gen_header_file_name, "rpc_server.h", "rpc_stream.h"))
    str_reg_include += '#include "%s"\r\n\r\n' % gen_header_file_name
    args_in_string = ""
    for arg in info['ConstructArgList']:
        arg_name = arg.strip().split(' ')[1]
        if arg_name in arg_list.keys():
            args_in_string += ","
            args_in_string += str(arg_list[arg_name])
        else:
            error_msg = "ERR: no construct arg name in config file matchs %s" % arg_name
            #sys.exit()
            return (False, "", "", "")
    str_reg_method += '    %s *%s = new %s(%s);\r\n' % (info['ClassName'], instance_name+"_ins", info['ClassName'], args_in_string.lstrip(","))


    #loop part
    #read functions
    str_reg_method += '    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);\r\n'

    for fun in info['Outputs'].items():
        fp_wrapper_h.write('void __%s_%s(void *class_ptr, void *input);\r\n' % (grove_name, fun[0]))

        fp_wrapper_cpp.write('void __%s_%s(void *class_ptr, void *input)\r\n' % (grove_name, fun[0]))
        fp_wrapper_cpp.write('{\r\n')
        fp_wrapper_cpp.write('    %s *grove = (%s *)class_ptr;\r\n' % (info['ClassName'], info['ClassName']))
        fp_wrapper_cpp.write('    uint8_t *arg_ptr = (uint8_t *)input;\r\n')
        fp_wrapper_cpp.write('    %s\r\n'%declare_vars(fun[1]))
        fp_wrapper_cpp.write(build_unpack_vars(fun[1]))
        fp_wrapper_cpp.write('\r\n')
        fp_wrapper_cpp.write('    if(grove->%s(%s))\r\n'%(fun[0],build_read_call_args(fun[1])))
        fp_wrapper_cpp.write('    {\r\n')
        fp_wrapper_cpp.write(build_read_print(fun[1]))
        fp_wrapper_cpp.write('    }else\r\n')
        fp_wrapper_cpp.write('    {\r\n')
        fp_wrapper_cpp.write('        writer_print(TYPE_STRING, "null");\r\n')
        fp_wrapper_cpp.write('    }\r\n')
        fp_wrapper_cpp.write('}\r\n\r\n')

        str_reg_method += '    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);\r\n'
        str_reg_method += build_reg_write_arg_type(fun[1])
        str_reg_method += '    rpc_server_register_method("%s", "%s", METHOD_READ, %s, %s, arg_types);\r\n' % \
                          (instance_name, fun[0].replace('read_',''), '__'+grove_name+'_'+fun[0], instance_name+"_ins")

        str_wellknown += '    writer_print(TYPE_STRING, "\\"GET " OTA_SERVER_URL_PREFIX "/%s/%s%s -> %s\\",");\r\n' % \
                            (instance_name, fun[0].replace('read_',''), build_read_with_arg(fun[1]), build_return_values(fun[1]))

    str_reg_method += '\r\n'

    #write functions
    for fun in info['Inputs'].items():
        fp_wrapper_h.write('void __%s_%s(void *class_ptr, void *input);\r\n' % (grove_name, fun[0]))

        fp_wrapper_cpp.write('void __%s_%s(void *class_ptr, void *input)\r\n' % (grove_name, fun[0]))
        fp_wrapper_cpp.write('{\r\n')
        fp_wrapper_cpp.write('    %s *grove = (%s *)class_ptr;\r\n' % (info['ClassName'], info['ClassName']))
        fp_wrapper_cpp.write('    uint8_t *arg_ptr = (uint8_t *)input;\r\n')
        fp_wrapper_cpp.write('    %s\r\n' % declare_vars(fun[1]))
        fp_wrapper_cpp.write(build_unpack_vars(fun[1]))
        fp_wrapper_cpp.write('\r\n')
        fp_wrapper_cpp.write('    if(grove->%s(%s))\r\n'%(fun[0],build_read_call_args(fun[1])))
        fp_wrapper_cpp.write('        writer_print(TYPE_STRING, "\\"OK\\"");\r\n')
        fp_wrapper_cpp.write('    else\r\n')
        fp_wrapper_cpp.write('        writer_print(TYPE_STRING, "\\"Failed\\"");\r\n')
        fp_wrapper_cpp.write('}\r\n\r\n')

        str_reg_method += '    memset(arg_types, TYPE_NONE, MAX_INPUT_ARG_LEN);\r\n'
        str_reg_method += build_reg_write_arg_type(fun[1])
        str_reg_method += '    rpc_server_register_method("%s", "%s", METHOD_WRITE, %s, %s, arg_types);\r\n' % \
                          (instance_name, fun[0].replace('write_',''), '__'+grove_name+'_'+fun[0], instance_name+"_ins")

        str_wellknown += '    writer_print(TYPE_STRING, "\\"POST " OTA_SERVER_URL_PREFIX "/%s/%s%s\\",");\r\n' % \
                            (instance_name, fun[0].replace('write_',''), build_read_with_arg(fun[1]))

    str_wellknown = str_wellknown[:-6]+'");\r\n'

    # event attachment
    if info['HasEvent']:
        str_reg_method += '\r\n    %s->attach_event_handler(rpc_server_event_report);\r\n' % instance_name

    fp_wrapper_h.close()
    fp_wrapper_cpp.close()
    if error_msg:
        return (False, "","")

    return (True, str_reg_include, str_reg_method, str_wellknown)



def gen_and_build (user_id, node_name):
    global error_msg
    global GEN_DIR
    ###generate rpc wrapper and registration files

    cur_dir = os.path.split(os.path.realpath(__file__))[0]

    try:
        f_database = open('%s/database.json' % cur_dir,'r')
        js = json.load(f_database)
    except Exception,e:
        error_msg = str(e)
        return False

    user_build_dir = cur_dir + '/users_build/' + user_id
    os.chdir(user_build_dir)

    try:
        f_config = open('%s/connection_config.yaml'%user_build_dir,'r')
        config = yaml.load(f_config)
    except Exception,e:
        error_msg = str(e)
        return False

    GEN_DIR = user_build_dir

    if not os.path.exists(user_build_dir):
        os.mkdir(user_build_dir)

    fp_reg_cpp = open(os.path.join(user_build_dir, "rpc_server_registration.cpp"),'w')
    str_reg_include = ""
    str_reg_method = ""
    str_reg_event = ""
    str_well_known = ""
    grove_list = ""

    for grove_instance_name in config.keys():
        grove = find_grove_in_database(config[grove_instance_name]['name'], js)
        if grove:
            ret, inc, method, wellknown = \
            gen_wrapper_registration(grove_instance_name, grove, config[grove_instance_name]['construct_arg_list'])
            if(ret == False):
                return False
            str_reg_include += inc
            str_reg_method  += method
            str_well_known  += wellknown
            grove_list += (grove["IncludePath"].replace("./grove_drivers/", " "))
        else:
            error_msg = "can not find %s in database"%grove_instance_name
            return False

    fp_reg_cpp.write('#include "suli2.h"\r\n')
    fp_reg_cpp.write('#include "rpc_server.h"\r\n')
    fp_reg_cpp.write('#include "rpc_stream.h"\r\n\r\n')
    #print str_reg_include
    #print str_reg_method
    fp_reg_cpp.write(str_reg_include)
    fp_reg_cpp.write('\r\n')
    fp_reg_cpp.write('void rpc_server_register_resources()\r\n')
    fp_reg_cpp.write('{\r\n')
    fp_reg_cpp.write('    uint8_t arg_types[MAX_INPUT_ARG_LEN];\r\n')
    fp_reg_cpp.write('    \r\n')
    fp_reg_cpp.write('    //%s\r\n'%grove_instance_name)
    fp_reg_cpp.write(str_reg_method)
    fp_reg_cpp.write('}\r\n')
    fp_reg_cpp.write('\r\n')
    fp_reg_cpp.write('void print_well_known()\r\n')
    fp_reg_cpp.write('{\r\n')
    fp_reg_cpp.write('    writer_print(TYPE_STRING, "[");\r\n')
    fp_reg_cpp.write(str_well_known)
    fp_reg_cpp.write('    writer_print(TYPE_STRING, "]");\r\n')
    fp_reg_cpp.write('}\r\n')

    fp_reg_cpp.close()

    ### make
    grove_list = '%s' % grove_list.lstrip(" ")


    if not os.path.exists(user_build_dir):
        os.mkdir(user_build_dir)

    find_cpp = False
    find_makefile = False
    for f in os.listdir(user_build_dir):
        if f.find(".cpp") > -1:
            find_cpp = True
            break
    for f in os.listdir(user_build_dir):
        if f.find("Makefile") > -1:
            find_makefile = True
            break
    if not find_cpp:
        os.system('cd %s;cp -f ../../Demo.cpp.template ./Demo.cpp ' % user_build_dir)
    if not find_makefile:
        os.system('cd %s;cp -f ../../Makefile.template ./Makefile ' % user_build_dir)

    os.putenv("APP","1")
    os.putenv("SPI_SPEED","40")
    os.putenv("SPI_MODE","QIO")
    os.putenv("SPI_SIZE_MAP","4")
    os.putenv("GROVES",grove_list)
    os.putenv("NODE_NAME",node_name)
    cmd = 'cd %s;make clean;make > build.log 2>error.log' % (user_build_dir)
    print cmd
    os.system(cmd)

    content = open(user_build_dir+"/error.log", 'r').readlines()
    for line in content:
        if line.find("error:") > -1 or line.find("make:") > -1:
            error_msg = line
            return False

    os.putenv("APP","2")

    cmd = 'cd %s;make clean;make >> build.log 2>>error.log' % (user_build_dir)
    print cmd
    os.system(cmd)

    content = open(user_build_dir+"/error.log", 'r').readlines()
    for line in content:
        if line.find("error:") > -1 or line.find("make:") > -1:
            error_msg = line
            return False

    os.system('cd %s;rm -rf *.S;rm -rf *.dump;rm -rf *.d;rm -rf *.o'%user_build_dir)



def get_error_msg ():
    global error_msg
    return error_msg

if __name__ == '__main__':

    user_id = "local_user" if len(sys.argv) < 2 else sys.argv[1]
    node_name = "esp8266_node" if len(sys.argv) < 3 else sys.argv[2]
    if not gen_and_build(user_id, node_name):
        print get_error_msg()



