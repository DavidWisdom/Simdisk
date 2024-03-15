//
// Created by eric on 10/19/23.
//
#include "filesystem.h"
sem_t semaphore;
struct Message {
    pid_t pid;
    uint32_t id;
    std::string command;
    Option option;
};
bool state = false;
std::queue<Message> message_queue;
std::mutex mtx;
Filesystem fs;
int shmId, semId, parSemId;
SharedMemory* sharedMemory;

static std::string to_string(Option option) {
    switch(option) {
        case Option::NONE: return "NONE";
        case Option::NEW: return "NEW";
        case Option::PWD: return "PWD";
        case Option::GET: return "GET";
        case Option::READ: return "READ";
        case Option::EXEC: return "EXEC";
        case Option::WRITE: return "WRITE";
        case Option::EXIT: return "EXIT";
        case Option::REQUEST: return "REQUEST";
        case Option::RESPONSE: return "RESPONSE";
        case Option::CAT: return "CAT";
        case Option::SWITCH: return "SWITCH";
        case Option::PATCH: return "PATCH";
        case Option::TAB: return "TAB";
        default: return "Unknown Option";
    }
};
bool is_prefix(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
        return false;
    }
    std::string substring = str.substr(0, prefix.length());
    return substring == prefix;
}
#include <chrono>
#include <ctime>
#include <iomanip>
ErrorCode simdisk(const Message& msg) {
    fs.current_shell_pid = msg.pid;
    if (msg.option == Option::NEW) {
        fs.new_shell();
        return ErrorCode::SUCCESS;
    }
    std::vector<std::string> args = split_command(msg.command);
    if (msg.option == Option::PATCH) {
        uint32_t i = std::stoul(args[1]);
        fs.response << fs.pid_map[fs.current_shell_pid].data.substr(i * 1024, 1024);
        return ErrorCode::SUCCESS;
    }
    if (msg.option == Option::TAB) {
        return fs.tab(args.back());
    }
    fs.request_option = msg.option;
    if (args[0] == "cat") {
        return fs.cat(args[1]);
    } else if (args[0] == "cd") {
        if (args.size() == 1) {
            return fs.cd("");
        } else {
            return fs.cd(args[1]);
        }
    } else if (args[0] == "check") {
        return fs.check();
    } else if (args[0] == "copy") {
        if (args.size() == 3) {
            std::string prefix = "<host>";
            if (is_prefix(args[1], prefix)) {
                return fs.copy_host(args[1].substr(prefix.length()), args[2]);
            } else if (is_prefix(args[2], prefix)) {
                return fs.copy_to_host(args[1], args[2].substr(prefix.length()));
            } else {
                return fs.copy(args[1], args[2]);
            }
        }
    } else if (args[0] == "del") {
        for (int i = 1; i < args.size(); ++i) {
            ErrorCode err = fs.del(args[i]);
            if (err == ErrorCode::FAILURE) return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    } else if (args[0] == "dir") {
        if (args.size() == 1) {
            return fs.dir("");
        } else if (args.size() == 2) {
            if (args[1] == "-s") {
                return fs.dir("", true);
            } else {
                return fs.dir(args[1]);
            }
        } else {
            if (args[1] == "-s") {
                return fs.dir(args[2], true);
            } else {
                return fs.dir(args[1], true);
            }
        }
    } else if (args[0] == "info") {
        if (args.size() == 1) {
            return fs.info("");
        } else {
            return fs.info(args[1]);
        }
    } else if (args[0] == "ls"){
        if (args.size() == 1) {
            return fs.ls("");
        } else if (args.size() == 2) {
            if (args[1] == "-s") {
                return fs.ls("", true);
            } else {
                return fs.ls(args[1]);
            }
        } else {
            if (args[1] == "-s") {
                return fs.ls(args[2], true);
            } else {
                return fs.ls(args[1], true);
            }
        }
    } else if (args[0] == "ll") {
        if (args.size() == 1) {
            return fs.ll("");
        } else if (args.size() == 2) {
            if (args[1] == "-s") {
                return fs.ll("", true);
            } else {
                return fs.ll(args[1]);
            }
        } else {
            if (args[1] == "-s") {
                return fs.ll(args[2], true);
            } else {
                return fs.ll(args[1], true);
            }
        }
    } else if (args[0] == "md") {
        for (int i = 1; i < args.size(); ++i) {
            ErrorCode err = fs.md(args[i]);
            if (err == ErrorCode::FAILURE) return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    } else if (args[0] == "newfile") {
        for (int i = 1; i < args.size(); ++i) {
            ErrorCode err = fs.newfile(args[i]);
            if (err == ErrorCode::FAILURE) return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    } else if (args[0] == "rd") {
        for (int i = 1; i < args.size(); ++i) {
            ErrorCode err = fs.rd(args[i]);
            if (err == ErrorCode::FAILURE) return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    } else if (args[0] == "save") {
        system(("zip backup.zip " + Disk::disk_name).c_str());
        fs.copy_host("backup.zip", "/lost+found/backup.img");
        system("rm backup.zip");
    } else if (args[0] == "su") {
        return fs.su(args[1], args[2]);
    } else if (args[0] == "sudo") {
        if (args.size() == 4) {
            if (args[1] == "useradd") {
                return fs.useradd(args[2], args[3]);
            } else if (args[1] == "chmod") {
                return fs.chmod(args[2], args[3]);
            }
        }
    } else if (args[0] == "exit") {
        fs.pid_map.erase(fs.current_shell_pid);
    }
    return ErrorCode::SUCCESS;
}
/**
 * @brief Server 类
 *
 * 该类用于接收请求，实现了在后台运行的主循环。
 */
class Server {
public:
    /**
     * @brief 获取请求
     *
     * 从消息队列中获取请求并进行处理。
     *
     * @return int 操作状态，通常无返回。
     */
    int get_request();

    /**
     * @brief 启动 Server
     *
     * 主循环，无限循环地调用 get_request 处理请求。
     *
     * @return [[noreturn]] 由于是无限循环，没有返回。
     */
    [[noreturn]] void run() {
        while (true) {
            get_request();
        }
    }
};

int Server::get_request() {
    printf("Simdisk: server is waiting for a request\n");
    Semaphore::P(parSemId);
    printf("Simdisk: server receives request\n");
    pid_t pid = sharedMemory->request.pid;
    uint32_t id = sharedMemory->request.id;
    std::string request(sharedMemory->request.data);
    Option option = sharedMemory->request.option;
    mtx.lock();
    message_queue.emplace(pid, id, request, option);
    mtx.unlock();
    sharedMemory->request.type = 'y';
    if (request.empty()) {
        printf("Simdisk: server records request %u\n", id);
    } else {
        printf("Simdisk: server records request %u `%s`\n", id, request.c_str());
    }
    sem_post(&semaphore);
    return 0;
}
/**
 * @brief Cooker 类
 *
 * 该类用于处理请求，实现了在后台运行的主循环。
 */
class Cooker {
public:
    /**
     * @brief 获取请求
     *
     * 从消息队列中获取请求并进行处理。
     *
     * @return int 操作状态，通常无返回。
     */
    int get_request();

    /**
     * @brief 启动 Cooker
     *
     * 主循环，无限循环地调用 get_request 处理请求。
     *
     * @return [[noreturn]] 由于是无限循环，没有返回。
     */
    [[noreturn]] void run() {
        while (true) {
            get_request();
        }
    }
};

int Cooker::get_request() {
    printf("Simdisk: cooker is waiting for a request\n");
    sem_wait(&semaphore);
    mtx.lock();
    Message request = message_queue.front();
    message_queue.pop();
    mtx.unlock();

    if (request.command.empty()) {
        printf("Simdisk: cooker is processing request %u\n", request.id);
    } else {
        printf("Simdisk: cooker is processing request %u `%s`\n", request.id, request.command.c_str());
    }
    // 使用系统时钟获取当前时间点
    {
        auto now = std::chrono::system_clock::now();
        // 将时间点转换为time_t以便输出
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//        std::string data = fs.cat_log(system_log);
        std::stringstream ss;
        if (request.command.empty()) {
            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
            ss << ": Simdisk is processing command `<NULL>` with option `";
            ss << to_string(request.option);
            ss << "` from Shell `";
            ss << std::to_string(request.pid);
            ss << "` User `";
            ss << fs.pid_map[request.pid].username;
            ss << "`";
        } else {
            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
            ss << ": Simdisk is processing command `" + request.command + "` with option `";
            ss << to_string(request.option);
            ss << "` from Shell `";
            ss << std::to_string(request.pid);
            ss << "` User `";
            ss << fs.pid_map[request.pid].username;
            ss << "`";
        }
//        if (data.empty())
//            fs.write_log(system_log, ss.str());
//        else
//            fs.write_log(system_log, data + "\n" + ss.str());
    }
    ErrorCode code = simdisk(request);
    {
//        auto now = std::chrono::system_clock::now();
//        // 将时间点转换为time_t以便输出
//        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//        std::string data = fs.cat_log(system_log);
//        std::stringstream ss;
//        if (request.command.empty()) {
//            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
//            ss << ": Simdisk completes processing command `<NULL>` with option `";
//            ss << to_string(request.option);
//            ss << "` from Shell `";
//            ss << std::to_string(request.pid);
//            ss << "` User `";
//            ss << fs.pid_map[request.pid].username;
//            ss << "`";
//        } else {
//            ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
//            ss << ": Simdisk completes processing command `" + request.command + "` with option `";
//            ss << to_string(request.option);
//            ss << "` from Shell `";
//            ss << std::to_string(request.pid);
//            ss << "` User `";
//            ss << fs.pid_map[request.pid].username;
//            ss << "`";
//        }
//        if (data.empty())
//            fs.write_log(system_log, ss.str());
//        else
//            fs.write_log(system_log, data + "\n" + ss.str());
    }
    Option option = Filesystem::response_option;
    int time = 100000;
    int cnt = 0;
    while (sharedMemory->response.type == 'n') {
        if (cnt >= 5) {
            break;
        }
        usleep(time);
        ++cnt;
    }
    std::string response;
    response = Filesystem::response.str();
    sharedMemory->response.send(response.c_str(), request.id, code, option);
    Filesystem::response.clear();
    Filesystem::response.str("");
    if (request.command.empty()) {
        printf("Simdisk: cooker completes processing request %u\n", request.id);
    } else {
        printf("Simdisk: cooker completes processing request %u `%s`\n", request.id, request.command.c_str());
    }
    return 0;
}

/**
 * @brief 初始化 Simdisk 系统
 *
 * 创建或获取共享内存，初始化信号量，创建并初始化系统日志。
 *
 * 注意: 由于该函数直接涉及操作系统底层的 IPC 和文件 I/O，
 * 具体实现细节可能需要依赖于操作系统的特定情况。
 */
void init() {
    // 创建或获取共享内存
    shmId = shmget(IPC_PRIVATE, sizeof(SharedMemory), IPC_CREAT | 0666);
    sharedMemory = (SharedMemory*)shmat(shmId, nullptr, 0);
    sharedMemory->request.pid = 0;
    memset(sharedMemory->request.data, 0, sizeof(char) * 2048);
    sharedMemory->request.id = 0;
    sharedMemory->request.type = 'y';
    sharedMemory->request.option = Option::NONE;
    memset(sharedMemory->response.data, 0, sizeof(char) * 2048);
    sharedMemory->response.id = 0;
    sharedMemory->response.code = ErrorCode::SUCCESS;
    sharedMemory->response.type = 'y';
    sharedMemory->response.option = Option::NONE;

    // 初始化信号量值为 0
    semId = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    semctl(semId, 0, SETVAL, 1);

    parSemId = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    semctl(parSemId, 0, SETVAL, 0);

    // 创建并初始化记录系统状态的日志文件
    std::ofstream output("ids.txt");
    output << shmId << ' ' << semId << ' ' << parSemId;
    output.close();
}
// 初始化日志信息
Entry* user_log = nullptr;
//Entry* system_log = nullptr;
//Entry* lock_log = nullptr;
/*
 *                        _oo0oo_
 *                       o8888888o
 *                       88" . "88
 *                       (| -_- |)
 *                       0\  =  /0
 *                     ___/`---'\___
 *                   .' \\|     |// '.
 *                  / \\|||  :  |||// \
 *                 / _||||| -:- |||||- \
 *                |   | \\\  - /// |   |
 *                | \_|  ''\---/''  |_/ |
 *                \  .-\__  '-'  ___/-. /
 *              ___'. .'  /--.--\  `. .'___
 *           ."" '<  `.___\_<|>_/___.' >' "".
 *          | | :  `- \`.;`\ _ /`;.`/ - ` : | |
 *          \  \ `_.   \_ __\ /__ _/   .-` /  /
 *      =====`-.____`.___ \_____/___.-`___.-'=====
 *                        `=---='
 *
 *
 *      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *            佛祖保佑       永不宕机     永无BUG
 */
/**
 * @brief Simdisk 主程序入口
 *
 * 检查磁盘镜像文件，创建新文件或载入已有文件，初始化并启动 Simdisk 服务。
 *
 * @return 返回程序执行状态，通常为 0 表示正常退出
 */
int main() {
    // 确保块大小为 1024 字节
    static_assert(sizeof(Block) == 1024);

    begin:
    std::cout << "请输入Simdisk要管理的磁盘镜像文件: ";
    std::string name;
    std::getline(std::cin, name);
    std::fstream img(name);

    if (!img.is_open()) {
        // 磁盘镜像文件不存在，询问是否创建新的磁盘文件
        std::cout << "磁盘镜像文件'" + name + "'不存在，是否创建新的磁盘文件? [Y/n] ";
        std::string option;
        std::getline(std::cin, option);

        if (option == "Y" || option == "y") {
            fs._new(name);
        } else {
            goto begin;
        }
    } else {
        // 磁盘镜像文件存在，询问是否载入已有的磁盘文件
        std::cout << "磁盘镜像文件'" + name + "'已经存在，是否载入已有的磁盘文件? [Y/n] ";
        std::string option;
        std::getline(std::cin, option);

        if (option == "Y" || option == "y") {
            state = true;
            fs.load(name);
        } else if (option == "N" || option == "n") {
            fs._new(name);
        } else {
            goto begin;
        }
    }

    // 初始化共享内存和信号量
    init();

    // 输出提示信息
    printf("Simdisk is currently running...\n");

    // 创建并启动 Simdisk 服务的服务器线程和处理线程
    Server server;
    Cooker cooker;
    std::thread t1(&Server::run, &server);
    std::thread t2(&Cooker::run, &cooker);

    // 等待线程结束
    t1.join();
    t2.join();

    // 释放资源
    fs.release();

    // 返回程序执行状态，通常为 0 表示正常退出
    return 0;
}
