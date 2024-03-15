//
// Created by eric on 10/19/23.
//

#ifndef SIMPLE_OS_COMMON_H
#define SIMPLE_OS_COMMON_H

#include <iostream>
#include <cstring>
#include <fstream>
#include <map>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include <queue>
#include <mutex>
#include <utility>
// 颜色宏定义
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[1;34m"
#define WHITE   "\033[1;37m"

// 错误码枚举
enum class ErrorCode {
    SUCCESS,                // 操作成功
    FAILURE,                // 操作失败
    EXISTS,                 // 已存在
    EXCEEDED,               // 超出限制
    WAIT_REQUEST,           // 等待请求
    FILE_NOT_FOUND,         // 文件未找到
    FILE_NOT_MATCH,         // 文件不匹配
    PERMISSION_DENIED,      // 权限被拒绝
    LOCKED,                 // 被锁定
};

// 选项枚举
enum class Option {
    NONE,           // 无操作
    NEW,            // 新建
    PWD,            // 获取当前路径
    GET,            // 获取
    READ,           // 读取
    EXEC,           // 执行
    WRITE,          // 写入
    EXIT,           // 退出
    REQUEST,        // 请求
    RESPONSE,       // 响应
    CAT,            // 查看文件内容
    SWITCH,         // 切换
    PATCH,          // 补丁
    TAB             // 制表
};

// 字符串分割函数，用于解析命令
std::vector<std::string> split_command(const std::string& command);

// 路径字符串分割函数，用于解析路径
std::vector<std::string> split_path(std::string path);

// 请求结构体
struct Request {
    pid_t pid;               // 进程ID
    char data[2048];         // 数据
    uint32_t id;             // ID
    char type;               // 类型
    Option option;           // 选项

    // 发送请求
    void send(const char _data[2048], uint32_t& _id, Option _option = Option::NONE) {
        pid = getpid();
        strcpy(data, _data);
        id += 1;
        _id = id;
        option = _option;
        type = 'n';
    }
};

// 响应结构体
struct Response {
    char data[2048];         // 数据
    uint32_t id;             // ID
    ErrorCode code;          // 错误码
    char type;               // 类型
    Option option;           // 选项

    // 发送响应
    void send(const char _data[2048], uint32_t _id, ErrorCode _code, Option _option) {
        strcpy(data, _data);
        code = _code;
        option = _option;
        type = 'n';
        id = _id;
    }
};

// 共享内存结构体
struct SharedMemory {
    Request request;         // 请求
    Response response;       // 响应
};

// 信号量类
class Semaphore {
public:
    // P操作
    static void P(int semId) {
        sembuf waitOp{};
        waitOp.sem_num = 0;
        waitOp.sem_op = -1;
        waitOp.sem_flg = SEM_UNDO;
        semop(semId, &waitOp, 1);
    }

    // V操作
    static void V(int semId) {
        sembuf signalOp{};
        signalOp.sem_num = 0;
        signalOp.sem_op = 1;
        signalOp.sem_flg = SEM_UNDO;
        semop(semId, &signalOp, 1);
    }
};
#endif //SIMPLE_OS_COMMON_H
