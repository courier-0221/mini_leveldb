# mini_leveldb

## 基础组件

### 1.slice

字符串处理类

成员变量

size_t size_;				//数据长度

const char* data_;	//数据地址

成员函数

remove_prefix			//从头部移除N个字节

starts_with				  //是否为某个字符或字符串开头

### 2.status

用于记录leveldb中状态信息，保存错误码和对应的字符串错误信息，不支持自定义



