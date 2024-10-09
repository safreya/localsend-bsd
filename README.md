# localsend-bsd localsend 协议的简单实现

## 用法

### 发送文件

```
$ localsend send file1 file2 ...
```

### 接收文件

```
$ localsend recv dir
```

## 编译安装

### 依赖的库

- cxxopts        用于命令行解析
- libcurl        用于传输文件
- cpp-httplib    用于接收模式 http 服务器
- nlohmann-json  用于解析 json
- openssl        用于接收模式服务器的 https 连接

```
# pkg install cxxopts libcurl cpp-httplib nlohmann-json openssl
```

### 编译

```
$ cd localsend-bsd
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make
```
