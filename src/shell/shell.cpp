#include "../common/common.h"
#include <iostream>
#include <sys/shm.h>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <termios.h>
//
// Created by eric on 10/19/23.
//
// shell.cpp

// 定义方向键代码以进行输入处理
#define UP 65
#define DOWN 66
#define LEFT 68
#define RIGHT 67
// 共享内存
SharedMemory* sharedMemory;
// 共享内存、信号量和父信号量的ID
int shmId, semId, parSemId;
// 用于存储上一次的路径信息
std::vector<std::string> last_path;
// 用于存储当前路径信息
std::vector<std::string> current_path;
// 用于存储当前命令
std::string current_command;
// 用于存储历史执行命令
std::vector<std::string> history;
bool cd_command = true;
bool change_command = false;
// 当前命令的执行状态
ErrorCode current_command_state = ErrorCode::SUCCESS;
// 当前用户名
std::string current_username = "root";
// 当前密码
std::string current_password;
// 用于存储命令的各个参数
std::vector<std::string> args;
// 已定义的命令
std::vector<std::string> defined_command = {
        "cat","cd","check","chmod","clear","copy","del","dir","echo","exit","help","info",
        "ls","ll","md","newfile","rd","su","sudo"
};
// 当前命令匹配的所有相关命令
std::vector<std::string> matches;
// 当前命令匹配的索引
uint32_t matches_id = 0;
// 用于判断当前键是否为tab
bool is_tab = false;
// 实现linux系统上的getch()函数
/**
 * @brief 从终端获取单个字符（无回显）
 *
 * 使用 termios 设置终端属性，关闭标准输入的行缓冲和回显，
 * 从终端获取一个字符后，恢复终端属性。
 *
 * @return 获取到的单个字符的 ASCII 码
 */
int getch() {
    static struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}


class Shell {
public:
    static void cd_path(std::string cd_path) {
        if (cd_path.empty()) {
            current_path.clear();
            return;
        }
        if (cd_path == "-") {
            auto temp = last_path;
            last_path = current_path;
            current_path = temp;
            return;
        }
//        if (cd_path[0] == '~') cd_path = "/home" + cd_path.substr(1);
        if (cd_path[0] == '/') {
            current_path = split_path(cd_path);
        } else {
            auto append_path = split_path(cd_path);
            for (const std::string& path: append_path) {
                current_path.emplace_back(path);
            }
        }
        auto temp_path = current_path;
        current_path.clear();
        for (const std::string& path: temp_path) {
            if (path == ".") continue;
            if (path == "..") {
                if (!current_path.empty()) {
                    current_path.pop_back();
                }
                continue;
            }
            current_path.push_back(path);
        }
    }
/**
 * @brief 判断字符串是否为另一个字符串的前缀
 *
 * @param str 要检查的字符串
 * @param prefix 前缀字符串
 * @return true 如果字符串是前缀
 * @return false 如果字符串不是前缀
 */
    static bool is_prefix(const std::string& str, const std::string& prefix) {
        if (str.length() < prefix.length()) {
            return false;
        }
        std::string substring = str.substr(0, prefix.length());
        return substring == prefix;
    }

/**
 * @brief 获取当前路径字符串
 *
 * @return std::string 当前路径字符串
 */
    static std::string get_path() {
        std::string paths;
        if (current_path.empty()) {
            return "/";
        }
//        if (current_path[0] == "home") {
//            for (uint32_t i = 1; i < current_path.size(); ++i) {
//                paths += "/" + current_path[i];
//            }
//            return "~" + paths;
//        } else {
        for (const auto& path: current_path) {
            paths += "/" + path;
        }
        return paths;
//        }
    }

/**
 * @brief 获取显示的路径字符串，包含用户名、主机名和当前路径信息
 *
 * @param path 当前路径字符串
 * @return std::string 显示的路径字符串
 */
    static std::string get_display_path(const std::string& path) {
        std::ostringstream oss;
        oss << GREEN << current_username << '@' << "VM-SIMDISK" << WHITE << ":";
        oss << BLUE << get_path();
        if (current_username == "root") {
            oss << WHITE << "# ";
        } else {
            oss << WHITE << "$ ";
        }
        return oss.str();
    }
    uint32_t request_id;
/**
 * @brief 发送请求到Simdisk
 *
 * @param command 要发送的命令字符串
 * @param option 请求的选项，默认为 Option::NONE
 * @return int 操作结果，通常为 0 表示成功
 */
    int send_request(const std::string& command, Option option = Option::NONE) {
        Semaphore::P(semId);
        int time = 100000;
        while (sharedMemory->request.type == 'n') {
            usleep(time);
        }
        sharedMemory->request.send(command.c_str(), request_id, option);
        Semaphore::V(semId);
        Semaphore::V(parSemId);
        return 0;
    }

/**
 * @brief 获取Simdisk的响应
 *
 * @param response 存储Simdisk响应的结构体
 * @param state 控制是否等待响应，默认为 true
 * @return int 操作结果，通常为 0 表示成功
 */
    int get_response(Response& response, bool state = true) const {
        int time = 100000;
        while (true) {
            if (sharedMemory->response.id == request_id) {
                strcpy(response.data, sharedMemory->response.data);
                response.id = sharedMemory->response.id;
                response.code = sharedMemory->response.code;
                response.option = sharedMemory->response.option;
                sharedMemory->response.type = 'y';
                break;
            }
            if (state) usleep(time);
        }
        return 0;
    }

    std::string get_string(const std::string& path = "") {
        std::string command;
        char ch;
        int curr = 0;
        uint32_t pos = 0;
        while (true) {
            ch = getch();
            if (ch != 9) {
                is_tab = false;
            }
            if (ch == 27) {
                ch = getch();
                if (ch == 91) {
                    ch = getch();
                    switch(ch) {
                        case UP: {
                            if (curr + 1 <= (int)history.size()) {
                                ++curr;
                            }
                            printf("\r\033[K");
                            std::string lastCommand = ((int)history.size() >= curr && curr > 0) ? history[history.size() - curr] : "";
                            command = lastCommand;
                            pos = lastCommand.size();
                            printf("%s", (path + lastCommand).c_str());
                        } break;
                        case DOWN: {
                            if (curr - 1 >= 0) {
                                --curr;
                            }
                            printf("\r\033[K");
                            std::string lastCommand = ((int)history.size() >= curr && curr > 0) ? history[history.size() - curr] : "";
                            command = lastCommand;
                            pos = lastCommand.size();
                            printf("%s", (path + lastCommand).c_str());
                        } break;
                        case LEFT: {
                            if (pos > 0) {
                                --pos;
                                printf("\033[1D");
                            }
                        } break;
                        case RIGHT: {
                            if (pos < command.size()) {
                                ++pos;
                                printf("\033[1C");
                            }
                        } break;
                    }
                }
            } else if (ch == 127) {
                if (!command.empty()) {
                    if (pos > 0) {
                        command.erase(command.begin() + pos - 1);
                        --pos;
                        if (command.size() == pos) {
                            printf("\b \b");
                        } else {
                            printf("\b \b");
                            printf("\033[0K");
                            for (auto i = pos; i < command.size(); ++i) {
                                printf("%c", command[i]);
                            }
                            for (auto i = pos; i < command.size(); ++i) {
                                printf("\033[1D");
                            }
                        }
                    }
                }
            } else if (ch == 10) {
                printf("\n");
                if (!command.empty()) history.push_back(command);
                return command;
            } else if (ch == 9) {
                if (is_tab) {
                    printf("\r\033[K");
                    matches_id = (matches_id + 1) % matches.size();
                    command = matches[matches_id];
                    pos = matches[matches_id].size();
                    printf("%s", (path + matches[matches_id]).c_str());
                } else {
                    std::vector<std::string> commands = split_command(command);
                    if (commands.size() == 1) {
                        matches.clear();
                        for (const auto& defined: defined_command) {
                            if (is_prefix(defined, commands[0])) {
                                matches.push_back(defined);
                            }
                        }
                        if (!matches.empty()) {
                            printf("\r\033[K");
                            is_tab = true;
                            matches_id = 0;
                            command = matches[matches_id];
                            pos = matches[matches_id].size();
                            printf("%s", (path + matches[matches_id]).c_str());
                        }
                    }
                    if (commands.size() == 2) {
                        if (commands[0] == "sudo") {
                            matches.clear();
                            for (const auto& defined: defined_command) {
                                if (is_prefix(defined, commands[1])) {
                                    matches.push_back(defined);
                                }
                            }
                            if (!matches.empty()) {
                                is_tab = true;
                                matches_id = 0;
                                printf("\r\033[K");
                                command = matches[matches_id];
                                pos = matches[matches_id].size();
                                printf("%s", (path + matches[matches_id]).c_str());
                            }
                        } else {
                            send_request(command, Option::TAB);
                            Response response{};
                            get_response(response);
                            std::string match = response.data;
                            std::istringstream iss(match);
                            std::vector<std::string> results;
                            std::string result;
                            matches.clear();
                            while (iss >> result) {
                                results.push_back(result);
                            }
                            if (!results.empty()) {
                                is_tab = true;
                                matches_id = 0;
                                printf("\r\033[K");
                                while (!command.empty()) {
                                    if (command.back() == '/' || command.back() == ' ') {
                                        for (const auto& res: results) {
                                            matches.push_back(command + res);
                                        }
                                        break;
                                    } else {
                                        command.pop_back();
                                    }
                                }
                                command = matches[0];
                                pos = command.size();
                                printf("%s", (path + command).c_str());
                            }
                        }
                    }
                    if (commands.size() > 2) {
                        send_request(command, Option::TAB);
                        Response response{};
                        get_response(response);
                        std::string match = response.data;
                        std::istringstream iss(match);
                        std::vector<std::string> results;
                        std::string result;
                        matches.clear();
                        while (iss >> result) {
                            results.push_back(result);
                        }
                        if (!results.empty()) {
                            is_tab = true;
                            matches_id = 0;
                            printf("\r\033[K");
                            while (!command.empty()) {
                                if (command.back() == '/' || command.back() == ' ') {
                                    for (const auto& res: results) {
                                        matches.push_back(command + res);
                                    }
                                    break;
                                } else {
                                    command.pop_back();
                                }
                            }
                            command = matches[0];
                            pos = command.size();
                            printf("%s", (path + command).c_str());
                        }
                    }
                }
            } else {
                command.insert(pos, 1, ch);
                ++pos;
                if (command.size() == pos) {
                    printf("%c", ch);
                } else {
                    printf("%c", ch);
                    printf("\033[0K");
                    for (auto i = pos; i < command.size(); ++i) {
                        printf("%c", command[i]);
                    }
                    for (auto i = pos; i < command.size(); ++i) {
                        printf("\033[1D");
                    }
                }
            }
        }
    }
    static void invisible(const std::string& message, std::string& password) {
        struct termios oldTermSettings{}, newTermSettings{};
        tcgetattr(STDIN_FILENO, &oldTermSettings);
        newTermSettings = oldTermSettings;
        newTermSettings.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &newTermSettings);
        printf("%s", message.c_str());
        // 从输入流中读取用户输入，此时输入内容将不可见
        std::getline(std::cin, password);
        // 恢复终端设置
        tcsetattr(STDIN_FILENO, TCSANOW, &oldTermSettings);
    }
    void shell() {
    begin:
        std::string path = get_display_path(get_path());
        std::cout << path;
        current_command = get_string(path);
        args = split_command(current_command);
        current_command.clear();
        for (const auto & arg : args) {
            current_command += arg + ' ';
        }
        if (!current_command.empty()) current_command.pop_back();
        if (args.empty()) goto begin;
        if (args.empty()) {
            goto begin;
        } else if (args[0] == "cat") {
            if (args.size() == 1) {
                printf("cat: missing operand\n");
                goto begin;
            }
            if (args.size() > 3) {
                printf("cat: too many arguments\n");
                goto begin;
            }
            if (args.size() == 2) {
                send_request("cat " + args[1], Option::CAT);
                Response response{};
                get_response(response);
                if (response.code == ErrorCode::SUCCESS) {
                    if (response.option == Option::PATCH) {
                        uint32_t size = (std::stoul(std::string(response.data)) + 1023) / 1024;
                        std::string result;
                        for (uint32_t i = 0; i < size; ++i) {
                            send_request("cat " + std::to_string(i), Option::PATCH);
                            get_response(response, false);
                            printf("%s", response.data);
                        }
                    } else {
                        printf("%s", response.data);
                    }
                } else {
                    printf("%s", response.data);
                }
                return;
            } else {
                send_request("cat " + args[2], Option::NONE);
                Response response{};
                get_response(response);
                if (response.code == ErrorCode::FAILURE) {
                    printf("%s", response.data);
                    return;
                }
                if (args[1] == "-w") {
                    send_request("cat " + args[2], Option::GET);
                    get_response(response);
                    if (response.code == ErrorCode::FAILURE) {
                        printf("%s", response.data);
                        return;
                    }
                    pid_t pid = fork();
                    std::string name = response.data;
                    if (pid == -1) {
                        printf("\n");
                        return;
                    } else if (pid == 0) {
                        name = response.data;
                        system(("nano " + name).c_str());
                        exit(0);
                    } else {
                        int status;
                        waitpid(pid, &status, 0);
                        if (WIFEXITED(status)) {
                            send_request("cat " + args[2], Option::WRITE);
                            get_response(response);
                        }
                    }
                } else if (args[1] == "-r") {
                    send_request("cat " + args[2], Option::READ);
                    get_response(response);
                    if (response.code == ErrorCode::FAILURE) {
                        printf("%s", response.data);
                        return;
                    }
                    pid_t pid = fork();
                    std::string name = response.data;
                    if (pid == -1) {
                        printf("\n");
                        return;
                    } else if (pid == 0) {
                        name = response.data;
                        system(("less -N " + name).c_str());
                        exit(0);
                    } else {
                        int status;
                        waitpid(pid, &status, 0);
                        if (WIFEXITED(status)) {
                            send_request("cat " + args[2], Option::EXIT);
                            get_response(response);
                        }
                    }
                } else {
                    goto begin;
                }
            }
            return;
        } else if (args[0] == "cd") {
            if (args.size() > 2) {
                printf("cd: too many arguments\n");
                goto begin;
            }
            cd_command = true;
        } else if (args[0] == "check") {

        } else if (args[0] == "copy") {
            if (args.size() < 3) {
                printf("copy: missing operand\n");
                goto begin;
            }
            if (args.size() > 3) {
                printf("copy: too many arguments\n");
                goto begin;
            }
        } else if (args[0] == "del") {

        } else if (args[0] == "dir") {

        } else if (args[0] == "echo") {

        } else if (args[0] == "exec") {

        } else if (args[0] == "help") {
            std::cout << "These shell commands are defined internally.  Type 'help' to see this list." << std::endl;
            std::cout << "Type 'help name' to find out more about the function 'name'." << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;
            std::cout << std::right << std::setw(7) << "Command" << std::setw(60) << "Information" << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;
            std::cout << std::right << std::setw(7) << "cat" << std::setw(60) << "Display the contents of a file" << std::endl;
            std::cout << std::right << std::setw(7) << "cd" << std::setw(60) << "Change the current working directory" << std::endl;
            std::cout << std::right << std::setw(7) << "check" << std::setw(60) << "Check the file system" << std::endl;
            std::cout << std::right << std::setw(7) << "chmod" << std::setw(60) << "Change the permissions of a file or directory" << std::endl;
            std::cout << std::right << std::setw(7) << "clear" << std::setw(60) << "Clear the screen" << std::endl;            std::cout << std::right << std::setw(7) << "copy" << std::setw(60) << "Copy a file or directory to a specified location" << std::endl;
            std::cout << std::right << std::setw(7) << "del" << std::setw(60) << "Remove an existing file" << std::endl;
            std::cout << std::right << std::setw(7) << "dir" << std::setw(60) << "List files and directories in the current directory" << std::endl;
            std::cout << std::right << std::setw(7) << "echo" << std::setw(60) << "Print a message to the console" << std::endl;
            std::cout << std::right << std::setw(7) << "exit" << std::setw(60) << "Exit the shell" << std::endl;
            std::cout << std::right << std::setw(7) << "help" << std::setw(60) << "Display a list of available commands and their descriptions" << std::endl;
            std::cout << std::right << std::setw(7) << "info" << std::setw(60) << "Show information about the file system" << std::endl;
            std::cout << std::right << std::setw(7) << "ls" << std::setw(60) << "List files and directories in the current directory" << std::endl;
            std::cout << std::right << std::setw(7) << "ll" << std::setw(60) << "List files and directories with detailed information" << std::endl;
            std::cout << std::right << std::setw(7) << "md" << std::setw(60) << "Create a new directory" << std::endl;
            std::cout << std::right << std::setw(7) << "newfile" << std::setw(60) << "Create a new file" << std::endl;
            std::cout << std::right << std::setw(7) << "rd" << std::setw(60) << "Remove an existing directory" << std::endl;
            std::cout << std::right << std::setw(7) << "su" << std::setw(60) << "Switch to another user account" << std::endl;
            std::cout << std::right << std::setw(7) << "sudo" << std::setw(60) << "Execute a command with superuser privileges" << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;
            std::cout << std::left;
        } else if (args[0] == "info") {
            // TODO:
        } else if (args[0] == "ls") {

        } else if (args[0] == "ll") {

        } else if (args[0] == "md") {
            if (args.size() == 1) {
                printf("md: missing operand\n");
                goto begin;
            }
        } else if (args[0] == "newfile") {
            if (args.size() == 1) {
                printf("newfile: missing operand\n");
                goto begin;
            }
        } else if (args[0] == "rd") {
            if (args.size() == 1) {
                printf("rd: missing operand\n");
                goto begin;
            }
            for (int i = 1; i < args.size(); ++i) {
                std::string sub_command = "rd " + args[i];
                send_request(sub_command);
                Response response{};
                get_response(response);
                current_command_state = response.code;
                if (response.code == ErrorCode::SUCCESS && strlen(response.data) > 0 && args.size() > 2) {
                    printf("rd %s: %s ", args[i].c_str(), response.data);
                } else {
                    if (strlen(response.data) > 0) printf("%s ", response.data);
                }
                if (response.option == Option::REQUEST) {
                    std::string option;
                    std::getline(std::cin, option);
                    if (option == "Y" || option == "y") {
                        send_request(sub_command, Option::RESPONSE);
                        get_response(response);
                        current_command_state = response.code;
                        printf("%s", response.data);
                    }
                }
            }
            return;
        } else if (args[0] == "scp") {

        } else if (args[0] == "save") {

        } else if (args[0] == "su") {
            if (args.size() == 1) {
                printf("su: missing operand\n");
                goto begin;
            }
            if (args.size() > 2) {
                printf("su: too many arguments\n");
                goto begin;
            }
            invisible("su: password for " + args[1] + ": ", current_password);
            printf("\n");
            send_request(current_command + ' ' + current_password, Option::SWITCH);
            Response response{};
            get_response(response);
            printf("%s", response.data);
            current_command_state = response.code;
            change_command = true;
            current_username = args[1];
            return;
        } else if (args[0] == "sudo") {
            if (current_username != "root") {
                // 禁用终端回显
                invisible("[sudo] password for root: ", current_password);
                printf("\n");
                send_request("su root " + current_password, Option::NONE);
                Response response{};
                get_response(response);
                if (response.code == ErrorCode::FAILURE) {
                    printf("[sudo]: Authentication failure\n");
                    goto begin;
                }
            }
            if (args.size() == 3) {
                if (args[1] == "useradd") {
                    std::string password1;
                    invisible("[sudo] setting password for " + args[2] + ": ", password1);
                    printf("\n");
                    std::string password2;
                    invisible("[sudo] setting password for " + args[2] + " again: ", password2);
                    printf("\n");
                    if (password1 == password2) {
                        current_password = password1;
                        current_command += ' ' + current_password;
                    } else {
                        printf("[sudo] Sorry, passwords do not match.\n");
                        goto begin;
                    }
                }
            }
        } else if (args[0] == "clear") {
            system("clear");
            goto begin;
        } else if (args[0] == "exit") {
            std::cout << "Do you want to exit the file system? [Y/n] ";
            std::string option;
            std::getline(std::cin, option);
            if (option == "y" || option == "Y") {
                send_request(current_command);
                return;
            } else {
                goto begin;
            }
        } else {
            printf("%s: command not found\n", args[0].c_str());
            goto begin;
        }
        send_request(current_command);
        Response response{};
        get_response(response);
        printf("%s", response.data);
        current_command_state = response.code;
    }
/**
 * @brief 运行Simdisk Shell
 *
 * 发送 NEW 请求，获取Simdisk的初始化响应，然后进入Shell循环。
 * 在循环中，根据当前命令的执行状态和类型，执行相应的操作。
 *
 * @note 当前实现中的具体操作逻辑需要根据实际代码来填写注释。
 */
    void run() {
        // 发送 NEW 请求，告知Simdisk创建新的Shell
        send_request("", Option::NEW);
        Response response{};
        // 获取Simdisk的初始化响应
        get_response(response);
        // 如果初始化失败，则直接返回
        if (response.code == ErrorCode::FAILURE) return;

        while (true) {
            // 根据当前命令的执行状态和类型执行相应的操作
            if (current_command_state == ErrorCode::SUCCESS) {
                if (cd_command) {
                    // 如果是 cd 命令，则根据当前命令执行 cd_path 操作
                    if (current_command.empty()) cd_path("");
                    else cd_path(args[1]);
                }
                if (change_command) cd_path("");
            }
            cd_command = false; change_command = false;
            // 执行Shell逻辑，具体操作需要根据实际代码来填写注释
            shell();
            // 如果当前命令是 "exit"，则跳出循环
            if (current_command == "exit") break;
        }

        // 释放共享内存
        shmdt(sharedMemory);
    }

};
/**
 * @brief Simdisk Shell的入口函数
 *
 * 从文件 "ids.txt" 中读取共享内存、信号量等标识符，
 * 然后将共享内存附加到进程中，创建 Shell 实例并运行。
 *
 * @return 程序执行成功返回 0
 */
int main() {
    // 从文件 "ids.txt" 读取共享内存、信号量等标识符
    std::ifstream input("ids.txt");
    input >> shmId >> semId >> parSemId;
    input.close();

    // 将共享内存附加到进程中
    sharedMemory = (SharedMemory*)shmat(shmId, nullptr, 0);

    // 创建 Shell 实例并运行
    Shell shell{};
    shell.run();

    return 0;
}
