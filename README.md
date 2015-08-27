#ESP8266 IOT Node


## Intro
ESP8266 IOT Node is a easy to use platform based on esp8266 wifi solution chip from espressif and grove modules from seeedstudio. The key feature of this project is that users never need to program one line of code before he get all the grove properties into Web of Things. 

Features:

* Totally open sourced for both software and hardware
* No need programming
* Config with GUI
* Web of Things for hundreds of grove modules (covering most IOT application domains)
* WiFi enabled with ESP8266 which is not expensive to have
* Security enhenced (AES for session between node & server, https for API calls)
* REST APIs for application 
* Lightweighted forward server (API server) based on Tornado



## Devloping Status

Done:

* Grove driver framework for turning grove properties into Web of Things
* Talk between node and server via AES encryption
* Basic property get/post APIs
* Basic user and nodes management APIs
* Cloud compiling APIs
* OTA control APIs

On the plan:

* GUI: android/ios

## Contributor Call

We're eagerly calling for Android app developers and iOS app developers to build GUI app, because our resources are very limited, and I believe there're masters in the community who loves open source projects too. Please contact xuguang.shao@seeed.cc if you would like to help out. 

And also we need more grove drivers commit from community if you know Arduino and Grove modules well.

## API Documentation

[API documentation](https://github.com/KillingJacky/esp8266_iot_node/wiki/API-Documentation)


## Rules for writing compatible grove drivers(beta, internal use)

* All hardware calls must be made through suli2 api
* **驱动可以分为两层, 第一层是过程式实现(方便移植到C平台),第二层为class封装**
* **第一层如果无法做到无状态(无法避免使用全局变量),则去掉第一层仅实现class封装即可**
* 为跨平台需要,在驱动实现中进行位移运算时,数据类型采用标准库中定义的带宽度类型名,例如int16_t,不建议使用int(各平台宽度不同)
* 规范的格式
  * 完整的header注释
  * 较完整的function注释
  * 函数名及变量名均采用小写
  * 分词采用下划线
* 为自动处理脚本而约定的特殊格式
  * 文件命名格式: grove打头,单词小写,下划线分词, 例如 grove_3axis_acc.h / grove_light_sensor.h / grove_nfc.h
  * 若采用两层实现,class封装层文件名中必须包含class字样, 例如 grove_3axis_acc_class.h
  * 必须在class.h中填加//GROVE_NAME注释行
  * 必须在class.h中填加//IF_TYPE注释行, 接口类型可以是{GPIO, PWM, ANALOG, I2C, UART}
  * 必须在class.h中填加//IMAGE_URL注释行
  * class封装中所有对外开放的读函数必须以read_打头
  * class封装中所有对外开放的写/配置函数必须以write_ 打头
  * class封装中构造函数的参数只能是硬件相关的,例如pin脚, 除硬件外所有配置均通过write写入
  * class封装中构造函数的参数按接口类型不同,分别有固定参数,参下一节"构造函数参数"
  * 第一层及第二层封装中所有非对外开放的函数/内部函数均以下划线打头
  * 所有read/write函数返回类型为bool
  * 所有read函数的输出数值均通过指针向外透出, 参数个数不限
  * 所有write函数参数个数不能超过4个
  * class封装中所有对外开放成员方法的参数数值类型支持{int, float, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t}及他们的指针型，指针类型是输出型参数（如read方法的输出），非指针类型是输入型参数（如read方法的输入参数和write方法的输入参数）
  * class封装中所有对外开放的写方法的参数数据类型还（实验性）支持字符串类型“char *“，但限制为：只能有1个字串参数，且是最后一个参数，字串长度最大256
  * 事件绑定函数的命名格式必须为EVENT_T * attach_event_reporter_for_\[event_name\](CALLBACK_T), 才能被脚本扫到
  * 事件绑定函数EVENT_T * attach_event_reporter_for_\[event_name\]必须返回事件变量的指针
  * 异常原因查询函数格式为：char *get_last_error()，若驱动中实现了此函数，当读写函数调用因异常而返回false时，会将此函数返回值所指向的字符串作为出错信息返回给云端调用，需要注意的是，字符串需要存储在RAM中。
  
* 构造函数参数
  * GPIO类型: 参数为(int pin)
  * ANALOG类型: 参数为(int pin)
  * I2C类型: 参数为(int pinsda, int pinscl)
  * UART类型: 参数为(int pintx, int pinrx)
  
## How to test newly written grove driver? (beta, internal use)

###1. Need to scan all the drivers into a database file. 

Steps:

  * python envirenment is needed
  * cd project root dir (where the script scan_drivers.py lies)
  * run "python ./scan_drivers.py" in your shell / cmd line
  
###2. Test building
  * python envirenment needed
  * gnu make tools needed
  * gnu arm gcc cross-compile tools needed, and modify the path in Makefile if they're not in your PATH env
  * edit the yaml file in users_build_dir/local_user, follow the rule 构造函数参数
  * run "python ./build_firmware.py" in your shell / cmd line - Check if error happens, 如果出错,那一定是没有遵循规则
  * cd users_build_dir/local_user (暂时, build_firmware.py完成后无须此步)
  * run "make" (暂时, build_firmware.py完成后无须此步) - Check if error happens, 如果出错, 可能是任意地方有错误
  
