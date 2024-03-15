//
// Created by eric on 10/19/23.
//

#ifndef SIMPLE_OS_FILESYSTEM_H
#define SIMPLE_OS_FILESYSTEM_H
#include "../common/common.h"
#include <iostream>
#include <bitset>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstring>
#include <utility>
#include <sys/types.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#define INODES_PER_BLOCK 16
#define POINTERS_PER_BLOCK 256
#define ENTRY_PER_BLOCK 32
#define DISK_SIZE (100 * 1024 * 1024)
#define BLOCKS_NUM (100 * 1024)
#define BLOCK_SIZE 1024
#define INODES_NUM (100 * 1024)
#define INODE_SIZE 64
#define MAX_LENGTH 24
static constexpr uint32_t null = (uint32_t)-1;
extern bool state;
// Superblock 结构体定义了超级块的一些属性，用于描述文件系统的基础信息。
struct Superblock {
    uint32_t magic_number = 0xffffff; // 魔数是一个用于识别文件系统的标志
    uint32_t blocks_num = BLOCKS_NUM; // 块的数量
    uint32_t used_blocks_num = 0; // 被使用的块的数量，初始值为0。
    uint32_t used_inodes_num = 0; // 被使用的 inode 的数量，初始值为0。
    uint32_t inodes_num = INODES_NUM; // inode 的数量
    uint32_t block_size = BLOCK_SIZE; // 块的大小
    uint32_t inode_size = INODE_SIZE; // inode 的大小
    uint32_t blocks_bitmap_num = BLOCKS_NUM / 8 / BLOCK_SIZE + 1; // 块的位图大小
    uint32_t inodes_bitmap_num = INODES_NUM / 8 / BLOCK_SIZE + 1; // inode 的位图大小
    uint32_t inodes_table_block = INODES_NUM / INODES_PER_BLOCK; // inode 表所需的块数
    uint32_t size = 0; // size 是一个预留字段，初始值为0，可以在后续扩展中用于存储文件系统的大小信息
    uint16_t filesystem_type; // 文件系统类型，是一个16位的短整数
    time_t last_check_time; // 最后检查时间，是一个time_t类型的变量
    time_t last_mount_time; // 最后挂载时间，是一个time_t类型的变量
    time_t last_write_time; // 最后写入时间，是一个time_t类型的变量
    uint32_t root_block_id; // 根目录项所在块ID
    uint32_t root_inode_id; // 根目录项对应inodeID
};
// 一个Inode占用64字节
struct Inode {                // Inode
    bool is_valid = false;    // Inode是否有效
    uint8_t link_cnt = 0;     // Inode链接数
    uint32_t size = 0;        // 文件大小
    uint32_t capacity = 0;    // 文件容量
    mode_t mode{};            // 文件权限
    char type = 'n';          // 文件类型
    char owner[8]{};          // 文件所有者
    uint32_t i_block[9]{null, null, null, null, null, null, null, null, null};
    // 直接指针与间接指针  0-5直接指针  6为一级间接指针 7为二级间接指针 8为三级间接指针
    void set_data(bool _is_valid, uint8_t _link_cnt, uint32_t _size, uint32_t _capacity, mode_t _mode, char _type, const char* _owner) {
        is_valid = _is_valid;
        link_cnt = _link_cnt;
        size = _size;
        capacity = _capacity;
        mode = _mode;
        type = _type;
        strcpy(owner, _owner);
    }
};
struct Entry {
    bool is_valid = false;                              // 目录项是否有效
    uint32_t inode_id = null;                           // 目录项对应的i结点ID
    char name[MAX_LENGTH]{};                            // 目录项名称
    static Entry* clone(Entry* entry) {                 // 深拷贝
        auto* new_entry = new Entry();
        new_entry->is_valid = entry->is_valid;
        new_entry->inode_id = entry->inode_id;
        strcpy(new_entry->name, entry->name);
        return new_entry;
    }
    static void release(Entry* entry) {                 // 释放目录项指针
        delete entry;
    }
};

// 数据块
union Block {
    Superblock superblock;                // 超级块
    Inode inodes[INODES_PER_BLOCK];       // 16个i节点（一个i节点占用64个字节）的块
    Entry entries[ENTRY_PER_BLOCK];       // 32个目录项（一个目录项占用32个字节）
    uint32_t pointers[POINTERS_PER_BLOCK];// 间接指针块
    uint8_t bmp[BLOCK_SIZE];              // 位图数据
    char data[BLOCK_SIZE];                // 纯数据块
    Block() {}
};

struct Disk {
    // 磁盘名字
    inline static std::string disk_name;
    // 创建新的磁盘
    static ErrorCode new_disk();
    // 加载已有磁盘
    static void load_disk();
    // 根据所给定的块号从磁盘中读取相应的块
    static Block* read_block(uint32_t block_num);
    // 根据所给定的块号往磁盘中读取相应的块
    static void write_block(uint32_t block_num, const Block* block);
};

struct Bitmap {
    uint32_t size;                    // 位图大小
    uint32_t offset;                  // 位图在磁盘位置中的偏移量
    uint32_t counter;                 // 位图有效位个数
    std::vector<uint8_t> bitmap;      // 位图数据
    std::vector<Block*> blocks;       // 位图在磁盘中对应的块
    Bitmap(uint32_t size, uint32_t offset): size(size), offset(offset), counter(0) {
        bitmap.resize(size / 8);
        blocks.resize(size / 8 / BLOCK_SIZE + 1);
        if (state) {
            for (uint32_t i = 0; i < blocks.size(); ++i) {
                blocks[i] = Disk::read_block(i + offset);
            }
            for (uint32_t i = 0; i < blocks.size(); ++i) {
                for (uint32_t j = 0; j < BLOCK_SIZE; ++j) {
                    if (i * BLOCK_SIZE + j >= size / 8) {
                        break;
                    }
                    bitmap[i * BLOCK_SIZE + j] = (uint8_t)(blocks[i]->data[j]);
                    std::bitset<8> bt(bitmap[i * BLOCK_SIZE + j]);
                    counter += bt.count();
                }
            }
        } else {
            Block block;
            for (uint32_t i = 0; i < blocks.size(); ++i) {
                blocks[i] = Disk::read_block(i + offset);
            }
        }
    }
    /**
    * @brief 设置位图中指定位置的位为1
    *
    * 根据给定的位置，将位图中对应位置的位设置为1，并更新计数器。
    *
    * @param i 位图中的位置
    */
    void set(uint32_t i) {
        uint32_t byteIndex = i / 8;
        uint32_t bitOffset = i % 8;
        bitmap[byteIndex] |= (1 << bitOffset);
        keep(i, bitmap[byteIndex]);
        ++counter;
    }

/**
 * @brief 重置位图中指定位置的位为0
 *
 * 根据给定的位置，将位图中对应位置的位重置为0，并更新计数器。
 *
 * @param i 位图中的位置
 */
    void reset(uint32_t i) {
        uint32_t byteIndex = i / 8;
        uint32_t bitOffset = i % 8;
        bitmap[byteIndex] &= ~(1 << bitOffset);
        keep(i, bitmap[byteIndex]);
        --counter;
    }

/**
 * @brief 保持位图中指定位置的值
 *
 * 根据给定的位置和值，将位图中对应位置的值保持一致。
 *
 * @param i   位图中的位置
 * @param val 位图中指定位置的值
 */
    inline void keep(uint32_t i, uint8_t val) {
        uint32_t blockIndex = i / (8 * BLOCK_SIZE);
        uint32_t bmpIndex = (i % (8 * BLOCK_SIZE)) / 8;
        blocks[blockIndex]->bmp[bmpIndex] = val;
    }

/**
 * @brief 分配一个新的位置
 *
 * 遍历位图，找到第一个为0的位置，将其设置为1，并返回该位置。
 *
 * @return uint32_t 分配的位置，如果没有可用位置，返回-1
 */
    uint32_t _new() {
        for (uint32_t i = 0; i < bitmap.size(); ++i) {
            uint8_t byte = bitmap[i];
            if (byte != 0xFF) {
                for (uint32_t j = 0; j < 8; ++j) {
                    if ((byte & (1 << j)) == 0) {
                        set(i * 8 + j);
                        return i * 8 + j;    // 返回位置
                    }
                }
            }
        }
        return -1;
    }

/**
 * @brief 释放指定位置
 *
 * 根据给定的位置，将位图中对应位置的位重置为0。
 *
 * @param i 位图中的位置
 */
    void _delete(uint32_t i) {
        reset(i);
    }

    void save() {
        for (uint32_t i = 0; i < blocks.size(); ++i) {
            Disk::write_block(i + offset, blocks[i]);
        }
    }
    void save(uint32_t i) {
        if (i == null) return;
        i = i / (8 * BLOCK_SIZE);
        Disk::write_block(i + offset, blocks[i]);
    }
    ~Bitmap() {
        for (auto& block: blocks) {
            delete block;
        }
    }
};
/**
 * @brief InodesTable 结构体
 *
 * 用于表示磁盘上的 inode 表结构，包括在磁盘上的偏移量和对应的块列表。
 */
struct InodesTable {
    uint32_t offset;                    // inode表在磁盘位置中的偏移量
    std::vector<Block*> inodes_table;   // inode表在磁盘中对应的块

    /**
     * @brief 构造函数
     *
     * 根据给定的大小和偏移量，初始化 InodesTable 结构体。
     *
     * @param size    inode表的大小
     * @param offset  inode表在磁盘中的偏移量
     */
    InodesTable(uint32_t size, uint32_t offset): offset(offset) {
        inodes_table.resize(size);
        for (uint32_t i = 0; i < inodes_table.size(); ++i) {
            inodes_table[i] = Disk::read_block(i + offset);
        }
    }

    /**
     * @brief 析构函数
     *
     * 释放 InodesTable 结构体中的块资源。
     */
    ~InodesTable() {
        for (auto& block: inodes_table) {
            delete block;
        }
    }

    /**
     * @brief 保存整个 inode 表到磁盘
     */
    void save() {
        for (uint32_t i = 0; i < inodes_table.size(); ++i) {
            Disk::write_block(i + offset, inodes_table[i]);
        }
    }

    /**
     * @brief 保存 inode 表中指定位置的块到磁盘
     *
     * @param i inode 表中的位置
     */
    void save(uint32_t i) {
        uint32_t inodeIndex = i / INODES_PER_BLOCK;
        Disk::write_block(i + offset, inodes_table[inodeIndex]);
    }
};

extern Entry* user_log;
//extern Entry* system_log;
//extern Entry* lock_log;
struct Filesystem {
    static std::pair<uint32_t, Block*> new_block();
    static Block* get_block(uint32_t i);
    static void save_block(uint32_t i, Block* block);
    static void delete_block(uint32_t i);
    static void release_block(Block* block);
    typedef int Mode;
    static constexpr int NEW = 1;
    static constexpr int GET = 2;
    static constexpr int WRITE_MODE = 4;
    static constexpr int READ_MODE = 8;
/**
 * @brief AutoBlock 结构体
 *
 * 该结构体用于自动管理块的读写和释放，以及提供方便的块操作接口。
 */
    struct AutoBlock {
        uint32_t pos;   ///< 块在磁盘中的位置
        Mode mode;      ///< 块的操作模式
        Block* block;    ///< 块的实际数据

        /**
         * @brief 默认构造函数
         *
         * 在构造时创建一个新块，并设置为写入模式。
         */
        AutoBlock() {
            mode = NEW | WRITE_MODE;
            std::tie(pos, block) = new_block();
        }

        /**
         * @brief 通过位置构造函数
         *
         * 根据给定位置读取一个块，并设置为获取模式。
         *
         * @param pos 块在磁盘中的位置
         */
        AutoBlock(uint32_t pos): pos(pos) {
            mode = GET;
            block = Disk::read_block(pos);
        }

        /**
         * @brief 通过位置和模式构造函数
         *
         * 根据给定位置和模式读取一个块。
         *
         * @param pos   块在磁盘中的位置
         * @param mode  操作模式
         */
        AutoBlock(uint32_t pos, Mode mode): pos(pos), mode(mode) {
            mode |= GET;
            block = Disk::read_block(pos);
        }

        /**
         * @brief 通过 Inode 构造函数
         *
         * 根据给定 Inode 的 id 和 Inode 指针读取一个块，并设置为获取和读取模式。
         *
         * @param id    Inode 中块的索引
         * @param inode 指向 Inode 的指针
         */
        AutoBlock(uint32_t id, Inode* inode): pos(inode->i_block[id]), block(get_block(inode->i_block[id])), mode(GET | READ_MODE) {}

        /**
         * @brief 析构函数
         *
         * 在析构时，如果是写入模式，将块写回磁盘，并释放块内存。
         */
        ~AutoBlock() {
            if (mode & WRITE_MODE) {
                Disk::write_block(pos, block);
            }
            delete block;
        }

        /**
         * @brief 获取块在磁盘中的位置
         *
         * @return uint32_t 块在磁盘中的位置
         */
        uint32_t id() const {
            return pos;
        }

        /**
         * @brief 保存块
         *
         * 将块写回磁盘。
         */
        void save() const {
            Disk::write_block(pos, block);
        }

        /**
         * @brief 设置操作模式
         *
         * 添加操作模式的标志。
         *
         * @param mask 要添加的模式标志
         */
        void mask(Mode mask) {
            mode |= mask;
        }

        /**
         * @brief 等于运算符重载
         *
         * 判断块是否为空指针。
         *
         * @param nullptr_t 空指针
         * @return true     块为空指针
         * @return false    块不为空指针
         */
        bool operator==(std::nullptr_t) const {
            return block == nullptr;
        }

        /**
         * @brief 获取块指针
         *
         * @return Block* 块指针
         */
        Block* elem() const {
            return block;
        }

        /**
         * @brief 获取块数据
         *
         * 将块的数据转换为字符串返回。
         *
         * @return std::string 块的数据
         */
        std::string data() const {
            std::string contents;
            for (size_t i = 0; i < BLOCK_SIZE; ++i) {
                contents.push_back(*((char*)((void*)block) + i));
            }
            return contents;
        }
    };

/**
 * @brief AutoEntry 结构体
 *
 * 该结构体用于自动管理 Entry 的创建、释放和交换操作，提供方便的 Entry 操作接口。
 */
    struct AutoEntry {
        Entry* entry; ///< Entry 指针

        /**
         * @brief 默认构造函数
         *
         * 创建一个空的 AutoEntry。
         */
        AutoEntry(): entry(nullptr) {}

        /**
         * @brief 显式构造函数
         *
         * 根据给定 Entry 指针创建 AutoEntry。
         *
         * @param _entry 要管理的 Entry 指针
         */
        explicit AutoEntry(Entry* _entry): entry(_entry) {}

        /**
         * @brief 析构函数
         *
         * 在析构时释放 Entry 内存。
         */
        ~AutoEntry() { delete entry; }

        /**
         * @brief 设置 Entry
         *
         * 释放当前 Entry，并设置为新的 Entry。
         *
         * @param _entry 新的 Entry 指针
         */
        void set(Entry* _entry) {
            delete entry;
            entry = _entry;
        }

        /**
         * @brief 获取 Entry 指针
         *
         * @return Entry* Entry 指针
         */
        Entry* elem() const {
            return entry;
        }

        /**
         * @brief 等于运算符重载
         *
         * 判断 Entry 是否为空指针。
         *
         * @param nullptr_t 空指针
         * @return true     Entry 为空指针
         * @return false    Entry 不为空指针
         */
        bool operator==(std::nullptr_t) const {
            return entry == nullptr;
        }

        /**
         * @brief 交换两个 AutoEntry
         *
         * @param entry1 要交换的 AutoEntry1
         * @param entry2 要交换的 AutoEntry2
         */
        static void swap(AutoEntry& entry1, AutoEntry& entry2) {
            Entry* entry = entry1.entry;
            entry1.entry = entry2.entry;
            entry2.entry = entry;
        }
    };


    inline static Option request_option = Option::NONE;
    inline static Option response_option = Option::NONE;
    inline static std::stringstream response;
    void _new(std::string name);
    bool load_state = false;
    void load(std::string name);
    void release();
    struct Info {
        AutoEntry last_entry;
        AutoEntry current_entry;
        AutoEntry root_entry;
        char username[8];
        std::string data;
    };
    inline static pid_t current_shell_pid;
    inline static std::map<pid_t, Info> pid_map;

    inline static Block* super = nullptr;
    inline static Bitmap* blocks_bitmap = nullptr;
    inline static Bitmap* inodes_bitmap = nullptr;
    inline static InodesTable* inodes_table = nullptr;
    inline static Block* root = nullptr;
    static std::pair<uint32_t, Inode*> new_inode();
    static Inode* get_inode(uint32_t i);
    static void save_inode(uint32_t i);
    static void delete_inode(uint32_t i);
/**
 * @brief 将权限码转换为文件模式
 *
 * 根据给定的权限码，生成相应的文件模式。
 *
 * @param permissions_code 权限码，例如 "rwxr-xr--"
 * @return mode_t 文件模式
 */
    constexpr mode_t to_mode(const char* permissions_code) {
        mode_t mode = 0;
        if (permissions_code[0] == 'r') mode |= S_IRUSR;
        if (permissions_code[1] == 'w') mode |= S_IWUSR;
        if (permissions_code[2] == 'x') mode |= S_IXUSR;
        if (permissions_code[3] == 'r') mode |= S_IRGRP;
        if (permissions_code[4] == 'w') mode |= S_IWGRP;
        if (permissions_code[5] == 'x') mode |= S_IXGRP;
        if (permissions_code[6] == 'r') mode |= S_IROTH;
        if (permissions_code[7] == 'w') mode |= S_IWOTH;
        if (permissions_code[8] == 'x') mode |= S_IXOTH;
        return mode;
    }

/**
 * @brief 在指定父目录下创建新目录
 *
 * 在指定的父目录下创建一个新目录。
 *
 * @param parent 父目录的Entry指针
 * @param name 新目录的名称
 * @param user 用户名，默认为当前Shell的用户名
 * @return ErrorCode 操作结果的错误码
 */
    ErrorCode new_directory(Entry* parent, const char* name, const char* user = pid_map[current_shell_pid].username);

/**
 * @brief 在指定父目录下创建新文件
 *
 * 在指定的父目录下创建一个新文件。
 *
 * @param parent 父目录的Entry指针
 * @param name 新文件的名称
 * @param user 用户名，默认为当前Shell的用户名
 * @return ErrorCode 操作结果的错误码
 */
    ErrorCode new_file(Entry* parent, const char* name, const char* user = pid_map[current_shell_pid].username);

/**
 * @brief 写入文件内容
 *
 * 向指定文件写入内容。
 *
 * @param parent 文件所在目录的Entry指针
 * @param name 文件名
 * @param user 用户名，默认为当前Shell的用户名
 * @return ErrorCode 操作结果的错误码
 */
    ErrorCode write_file(Entry* parent, const char* name, const char* user = pid_map[current_shell_pid].username);

/**
 * @brief 写入日志内容
 *
 * 向指定日志写入内容。
 *
 * @param log 日志的Entry指针
 * @param contents 要写入的内容
 * @return ErrorCode 操作结果的错误码
 */
    ErrorCode write_log(Entry* log, const std::string& contents);

/**
 * @brief 读取日志内容
 *
 * 读取指定日志的内容。
 *
 * @param log 日志的Entry指针
 * @return std::string 日志内容
 */
    std::string cat_log(Entry* log);

/**
 * @brief 删除文件
 *
 * 删除指定目录下的文件。
 *
 * @param parent 文件所在目录的Entry指针
 * @param name 文件名
 * @param user 用户名，默认为当前Shell的用户名
 * @return ErrorCode 操作结果的错误码
 */
    ErrorCode delete_file(Entry* parent, const char* name, const char* user = pid_map[current_shell_pid].username);

/**
 * @brief 读取文件内容
 *
 * 读取指定文件的内容。
 *
 * @param parent 文件所在目录的Entry指针
 * @param name 文件名
 * @param option 读取选项
 * @param user 用户名，默认为当前Shell的用户名
 * @return ErrorCode 操作结果的错误码
 */
    ErrorCode cat_file(Entry* parent, const char* name, Option option, const char* user = pid_map[current_shell_pid].username);

/**
 * @brief 获取文件
 *
 * 获取指定目录下的文件。
 *
 * @param parent 文件所在目录的Entry指针
 * @param name 文件名
 * @param user 用户名，默认为当前Shell的用户名
 * @return ErrorCode 操作结果的错误码
 */
    ErrorCode get_file(Entry* parent, const char* name, const char* user = pid_map[current_shell_pid].username);

/**
 * @brief 拆分路径和文件名
 *
 * 将给定的路径拆分为目录路径和文件名。
 *
 * @param path 待拆分的路径
 * @return std::pair<std::string, std::string> 目录路径和文件名的pair
 */
    std::pair<std::string, std::string> split_path_and_name(std::string path);

/**
 * @brief 获取指定路径的Entry
 *
 * 获取指定路径下的Entry和操作结果的错误码。
 *
 * @param path 指定路径
 * @return std::pair<ErrorCode, Entry*> 操作结果的错误码和Entry指针的pair
 */
    std::pair<ErrorCode, Entry*> get_path_entry(std::string path);

    ErrorCode list_directory(Entry* parent, std::vector<std::string>& names, const char* user = pid_map[current_shell_pid].username) const {
        if (!parent->is_valid) return ErrorCode::FAILURE;
        Inode* inode = get_inode(parent->inode_id);
        if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
        AutoBlock block(0, inode);
        if (block == nullptr) return ErrorCode::FAILURE;
        for (auto& entry: block.elem()->entries) {
            if (entry.is_valid) {
                names.emplace_back(entry.name);
            }
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode delete_directory(Entry* entry, const char* name, Option option, const char* user = pid_map[current_shell_pid].username);
    ErrorCode info(const std::string& args) {
        if (args.empty()) {
            /* TODO */
            response << std::left << std::setw(10) << "Filesystem";
            response << std::left << std::setw(13) << "    1K-blocks";
            response << std::left << std::setw(8) << "    Used";
            response << std::left << std::setw(9) << "    Avail";
            response << std::left << std::setw(8) << "    Use%";
            response << std::left << "  Mounted on\n";
            response << "------------------------------------------------------------\n";
            response << std::left << std::setw(10) << "simdisk";
            response << std::right << std::setw(13) << super->superblock.blocks_num;
            response << std::right << std::setw(8) << blocks_bitmap->counter;
            response << std::right << std::setw(9) << super->superblock.blocks_num - blocks_bitmap->counter;
            response << std::right << std::setw(7) << std::fixed << std::setprecision(2) << blocks_bitmap->counter * 100. / super->superblock.blocks_num << "%";
            response << std::left << "  /\n";
            response << "------------------------------------------------------------\n";
        }
        else if (args == "-h") {
            response << std::left << std::setw(10) << "Filesystem";
            response << std::left << std::setw(6) << "  Size";
            response << std::left << std::setw(8) << "    Used";
            response << std::left << std::setw(9) << "    Avail";
            response << std::left << std::setw(8) << "    Use%";
            response << std::left << "  Mounted on\n";
            response << "---------------------------------------------------\n";
            response << std::left << std::setw(10) << "simdisk";
            response << std::right << std::setw(6) << "100M";
            if (blocks_bitmap->counter < 1024) {
                response << std::right << std::setw(7) << blocks_bitmap->counter << "K";
            }
            else {
                response << std::right << std::setw(7) << std::fixed << std::setprecision(2) << blocks_bitmap->counter / 1024. << "M";
            }
            response << std::right << std::setw(8) << std::fixed << std::setprecision(2) << 100. - blocks_bitmap->counter / 1024. << "M";
            response << std::right << std::setw(7) << std::fixed << std::setprecision(2) << blocks_bitmap->counter / 1024. <<  "%";
            response << std::left << "  /\n";
            response << "---------------------------------------------------\n";
        } else if (args == "-i") {
            response << std::left << std::setw(10) << "Filesystem";
            response << std::left << std::setw(10) << "    Inodes";
            response << std::left << std::setw(9) << "    IUsed";
            response << std::left << std::setw(9) << "    IFree";
            response << std::left << std::setw(9) << "    IUse%";
            response << std::left << "  Mounted on\n";
            response << "------------------------------------------------------------\n";
            response << std::left << std::setw(10) << "simdisk";
            response << std::right << std::setw(10) << super->superblock.inodes_num;
            response << std::right << std::setw(9) << inodes_bitmap->counter;
            response << std::right << std::setw(9) << super->superblock.inodes_num - inodes_bitmap->counter;
            response << std::right << std::setw(8) << std::fixed << std::setprecision(2) << inodes_bitmap->counter * 100. / super->superblock.inodes_num << "%";
            response << std::left << "  /\n";
            response << "------------------------------------------------------------\n";
        } else {
            response << "info: invalid option" << std::endl;
            return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode md(const std::string& path, const char *user = pid_map[current_shell_pid].username) {
        auto [directory, filename] = split_path_and_name(path);
        if (filename == "." || filename == "..") {
            response << "md: cannot create directory '" << path << "': File exists\n";
            return ErrorCode::FAILURE;
        }
        AutoEntry entry(get_path_entry(directory).second);
        ErrorCode err = check_entry(entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "md: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        if (entry == nullptr) {
            response << "md: cannot create directory '" << path << "': No such file or directory\n";
            return ErrorCode::FAILURE;
        }
        err = new_directory(entry.elem(), filename.c_str());
        if (err == ErrorCode::FAILURE) {
            response << "md: cannot create directory '" << path << "': No such file or directory\n";
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::EXISTS) {
            response << "md: cannot create directory '" << path << "': File exists\n";
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::EXCEEDED) {
            response << "md: cannot create directory '" << path << "': Exceeded the maximum name length (24 characters)\n";
            return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode rd(const std::string& path, const char *user = pid_map[current_shell_pid].username) {
        auto [directory, filename] = split_path_and_name(path);
        AutoEntry entry(get_path_entry(directory).second);
        if (entry == nullptr) {
            response << "rd: cannot delete directory '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        ErrorCode err = check_entry(entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "rd: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        err = delete_directory(entry.elem(), filename.c_str(), request_option);
        if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
            response << "rd: cannot remove directory '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::FILE_NOT_MATCH) {
            response << "rd: cannot remove directory '" << path << "': Not a directory" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::WAIT_REQUEST) {
            response << "The directory is not empty. Are you sure you want to delete it and all its contents? [Y/n]";;
            response_option = Option::REQUEST;
            return ErrorCode::SUCCESS;
        }
        response_option = Option::NONE;
        return ErrorCode::SUCCESS;
    }
    ErrorCode cd(const std::string& path, const char *user = pid_map[current_shell_pid].username) {
        auto& last_entry = pid_map[current_shell_pid].last_entry;
        auto& curr_entry = pid_map[current_shell_pid].current_entry;
        auto& root_entry = pid_map[current_shell_pid].root_entry;
        if (path.empty()) {
            last_entry.set(Entry::clone(curr_entry.elem()));
            curr_entry.set(Entry::clone(root_entry.elem()));
            return ErrorCode::SUCCESS;
        }
        if (path == "-") {
            if (last_entry == nullptr) {
                response << "cd: OLDPWD not set\n";
                return ErrorCode::FAILURE;
            }
            AutoEntry::swap(curr_entry, last_entry);
            return ErrorCode::SUCCESS;
        }
        AutoEntry cd_entry(get_path_entry(path).second);
        if (cd_entry == nullptr) {
            response << "cd: '" << path << "': No such file or directory\n";
            return ErrorCode::FAILURE;
        }
        ErrorCode err = check_entry(cd_entry.elem(), user, Option::EXEC);
        if (err == ErrorCode::FAILURE) {
            response << "cd: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        if (get_inode(cd_entry.elem()->inode_id)->type == 'f') {
            response << "cd: '" << path << "': Not a directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        last_entry.set(Entry::clone(curr_entry.elem()));
        curr_entry.set(Entry::clone(cd_entry.elem()));
        return ErrorCode::SUCCESS;
    }
    ErrorCode dir(const std::string& path, bool with_args = false, const char *user = pid_map[current_shell_pid].username);
    static bool is_prefix(const std::string& str, const std::string& prefix) {
        if (str.length() < prefix.length()) {
            return false;
        }
        std::string substring = str.substr(0, prefix.length());
        return substring == prefix;
    }
    ErrorCode tab(std::string tab_path, const char *user = pid_map[current_shell_pid].username) {
        auto [path, name] = split_path_and_name(tab_path);
        AutoEntry entry;
        if (path.empty()) {
            entry.set(Entry::clone(pid_map[current_shell_pid].current_entry.elem()));
        } else {
            entry.set(get_path_entry(path).second);
            if (entry == nullptr) {
                return ErrorCode::FAILURE;
            }
        }
        Inode* inode = get_inode(entry.elem()->inode_id);
        if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
        ErrorCode err = check_entry(entry.elem(), user, Option::READ);
        if (err == ErrorCode::FAILURE) return ErrorCode::FAILURE;
        AutoBlock block(0, inode);
        for (const auto& file: block.elem()->entries) {
            if (file.is_valid) {
                if (is_prefix(file.name, name)) {
                    if (get_inode(file.inode_id)->type == 'd') {
                        response << file.name << "/ ";
                    } else {
                        response << file.name << " ";
                    }
                }
            }
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode ls(const std::string& path, bool with_args = false, const char *user = pid_map[current_shell_pid].username);
    ErrorCode ll(const std::string& path, bool with_args = false, const char *user = pid_map[current_shell_pid].username);
    ErrorCode cat_data(Entry *parent, const char *name, std::string& data, const char *user = pid_map[current_shell_pid].username);
    ErrorCode su(const std::string &username, const std::string &password) {
        if (users.find(username) == users.end()) {
            response << "su: user `" << username << "` does not exist or the user entry does not contain all the required fields" << std::endl;
            return ErrorCode::FAILURE;
        }
        if (users[username] != password) {
            response << "su: Authentication failure" << std::endl;
            return ErrorCode::FAILURE;
        }
        if (request_option == Option::SWITCH) {
            strcpy(pid_map[current_shell_pid].username, username.c_str());
        }
        Info info;
        strcpy(info.username, username.c_str());
        pid_map[current_shell_pid] = info;
        pid_map[current_shell_pid].last_entry.set(nullptr);
        pid_map[current_shell_pid].current_entry.set(Entry::clone(&root->entries[0]));
        pid_map[current_shell_pid].root_entry.set(Entry::clone(&root->entries[0]));
        return ErrorCode::SUCCESS;
    }
    ErrorCode cat(const std::string& path, const char* user = pid_map[current_shell_pid].username) {
        auto [directory, filename] = split_path_and_name(path);
        AutoEntry entry(get_path_entry(directory).second);
        if (entry == nullptr) {
            response << "cat: cannot catch file '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }  
        ErrorCode err;
        if (request_option == Option::NONE) {
            err = get_file(entry.elem(), filename.c_str());
            if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
                response << "cat: cannot catch file '" << path << "': No such file or directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::FILE_NOT_MATCH) {
                response << "cat: '" << path << "': Is a directory" << std::endl;
                return ErrorCode::FAILURE;
            }
            return ErrorCode::SUCCESS;
        }
        if (request_option == Option::CAT) {
            std::string data;
            err = cat_data(entry.elem(), filename.c_str(), data);
            if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
                response << "cat: cannot catch file '" << path << "': No such file or directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::FILE_NOT_MATCH) {
                response << "cat: '" << path << "': Is a directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::PERMISSION_DENIED) {
                response << "cat: Permission denied" << std::endl;
                return ErrorCode::FAILURE;
            }
            // TODO:
            if (!data.empty()) {
                if (data.back() != '\n') data.push_back('\n');
            }
            if (data.size() > 1024) {
                response_option = Option::PATCH;
                pid_map[current_shell_pid].data = data;
                response << data.size();
            } else {
                response << data;
            }
            release_file(entry.elem(), filename.c_str());
            return ErrorCode::SUCCESS;
        }
        if (request_option == Option::READ) {
            err = cat_file(entry.elem(), filename.c_str(), Option::READ);
            if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
                response << "cat: cannot catch file '" << path << "': No such file or directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::FILE_NOT_MATCH) {
                response << "cat: '" << path << "': Is a directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::PERMISSION_DENIED) {
                response << "cat: Permission denied" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::LOCKED) {
                response << "cat: cannot get the read lock of file '" << path << "'" << std::endl;
                return ErrorCode::FAILURE;
            }
            response << filename;
        } else if (request_option == Option::GET) {
            err = cat_file(entry.elem(), filename.c_str(), Option::WRITE);
            if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
                response << "cat: cannot catch file '" << path << "': No such file or directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::FILE_NOT_MATCH) {
                response << "cat: '" << path << "': Is a directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::PERMISSION_DENIED) {
                response << "cat: Permission denied" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::LOCKED) {
                response << "cat: cannot get the write lock of file '" << path << "'" << std::endl;
                return ErrorCode::FAILURE;
            }
            response << filename;
        } else if (request_option == Option::WRITE) {
            err = write_file(entry.elem(), filename.c_str());
            if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
                response << "cat: cannot catch file '" << path << "': No such file or directory" << std::endl;
                return ErrorCode::FAILURE;
            } else if (err == ErrorCode::FILE_NOT_MATCH) {
                response << "cat: '" << path << "': Is a directory" << std::endl;
                return ErrorCode::FAILURE;
            }
            system(("rm " + filename).c_str());
        } else if (request_option == Option::EXIT) {
            release_file(entry.elem(), filename.c_str());
            system(("rm " + filename).c_str());
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode write_data(Entry *parent, const char* name, const std::string& contents, const char *user = pid_map[current_shell_pid].username);
    ErrorCode release_file(Entry *parent, const char *name);
    ErrorCode newfile(const std::string& path, const char *user = pid_map[current_shell_pid].username) {
        auto [directory, filename] = split_path_and_name(path);
        AutoEntry entry(get_path_entry(directory).second);
        if (entry == nullptr) {
            response << "newfile: cannot create file '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        ErrorCode err = check_entry(entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "newfile: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        err = new_file(entry.elem(), filename.c_str());
        if (err == ErrorCode::FAILURE) {
            response << "newfile: cannot create file '" << path << "': No such file or directory\n";
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::EXISTS) {
            response << "newfile: cannot create file '" << path << "': File exists\n";
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::EXCEEDED) {
            response << "newfile: cannot create file '" << path << "': Exceeded the maximum name length (24 characters)\n";
            return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode del(const std::string& path, const char *user = pid_map[current_shell_pid].username) {
        auto [directory, filename] = split_path_and_name(path);
        AutoEntry entry(get_path_entry(directory).second);
        if (entry == nullptr) {
            response << "del: cannot delete file '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        ErrorCode err = check_entry(entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "del: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        err = delete_file(entry.elem(), filename.c_str());
        if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
            response << "del: cannot delete file '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::FILE_NOT_MATCH) {
            response << "del: cannot delete file '" << path << "': Is a directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode copy_to_host(const std::string& src_path, const std::string& dst_path, const char* user = pid_map[current_shell_pid].username) {
        // TODO:
        auto [src_directory, src_filename] = split_path_and_name(src_path);
        AutoEntry src_entry(get_path_entry(src_directory).second);
        if (src_entry == nullptr) {
            response << "copy: cannot stat file '" << src_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        ErrorCode err = get_file(src_entry.elem(), src_filename.c_str());
        if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
            response << "copy: cannot stat file '" << src_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::FILE_NOT_MATCH) {
            response << "copy: '" << src_path << "': Is a directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        err = check_entry(src_entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "copy: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        src_entry.set(get_path_entry(src_path).second);
        err = lock(src_entry.elem()->inode_id, get_inode(src_entry.elem()->inode_id), Lock::READ_LOCK);
        if (err != ErrorCode::SUCCESS)  {
            response << "copy: cannot get the read lock of file '" << src_path << "'" << std::endl;
            return ErrorCode::FAILURE;
        }
        std::string data = cat_log(src_entry.elem());
        std::ofstream ofs(dst_path, std::ios::binary);
        if (!ofs.is_open()) {
            response << "copy: cannot stat file '" << dst_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        ofs << data;
        ofs.close();
        return ErrorCode::SUCCESS;
    }
    ErrorCode copy_host(const std::string& src_path, std::string dst_path, const char *user = pid_map[current_shell_pid].username) {
        auto [src_directory, src_filename] = split_path_and_name(src_path);
        std::ifstream src_file(src_path, std::ios::binary);
        if (!src_file.is_open()) {
            response << "copy: cannot stat file '" << src_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        std::string temp_path = dst_path;
        if (dst_path.back() == '/') {
            dst_path += src_filename;
        } else {
            auto [dst_directory, dst_filename] = split_path_and_name(dst_path);
            AutoEntry dst_entry(get_path_entry(dst_directory).second);
            if (dst_entry == nullptr) {
                response << "copy: cannot stat file '" << dst_path << "': No such file or directory" << std::endl;
                return ErrorCode::FAILURE;
            } else {
                AutoEntry temp_entry(get_path_entry(dst_directory + "/" + dst_filename).second);
                if (temp_entry != nullptr) {
                    if (get_inode(temp_entry.elem()->inode_id)->type == 'd') {
                        dst_path += "/" + src_filename;
                    }
                }
            }
        }
        auto [dst_directory, dst_filename] = split_path_and_name(dst_path);
        AutoEntry dst_entry(get_path_entry(dst_directory).second);
        if (dst_entry == nullptr) {
            response << "copy: cannot stat file '" << temp_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        } else {
            AutoEntry dst_entry(get_path_entry(dst_path).second);
            if (dst_entry == nullptr) {
                newfile(dst_path);
            }
        }
        ErrorCode err = check_entry(dst_entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "copy: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        std::stringstream buffer;
        buffer << src_file.rdbuf();
        std::string contents = buffer.str();
        src_file.close();
        err = write_data(dst_entry.elem(), dst_filename.c_str(), contents);
//        Block* beforeblock = Disk::read_block(6438);
//        std::string content;
//        for (uint32_t i = 0; i < BLOCK_SIZE; ++i) {
//            content.push_back(*((char*)((void*)beforeblock) + i));
//        }
//        std::ofstream test("before.zip", std::ios::binary);
//        test << content;
//        test.close();
//        delete beforeblock;
        if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
            response << "copy: cannot stat file '" << temp_path << "': Exceeded the maximum name length (24 characters)" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::PERMISSION_DENIED) {
            response << "copy: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::LOCKED) {
            response << "copy: cannot get the write lock of file '" << temp_path << "'" << std::endl;
            return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode copy(const std::string& src_path, std::string dst_path, const char *user = pid_map[current_shell_pid].username) {
        auto [src_directory, src_filename] = split_path_and_name(src_path);
        AutoEntry src_entry(get_path_entry(src_directory).second);
        if (src_entry == nullptr) {
            response << "copy: cannot stat file '" << src_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        ErrorCode err = get_file(src_entry.elem(), src_filename.c_str());
        if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
            response << "copy: cannot stat file '" << src_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::FILE_NOT_MATCH) {
            response << "copy: '" << src_path << "': Is a directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        err = check_entry(src_entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "copy: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        std::string temp_path = dst_path;
        if (dst_path.back() == '/') {
            dst_path += src_filename;
        } else {
            auto [dst_directory, dst_filename] = split_path_and_name(dst_path);
            AutoEntry dst_entry(get_path_entry(dst_directory).second);
            if (dst_entry == nullptr) {
                response << "copy: cannot stat file '" << dst_path << "': No such file or directory" << std::endl;
                return ErrorCode::FAILURE;
            } else {
                AutoEntry temp_entry(get_path_entry(dst_directory + "/" + dst_filename).second);
                if (temp_entry != nullptr) {
                    if (get_inode(temp_entry.elem()->inode_id)->type == 'd') {
                        dst_path += "/" + src_filename;
                    }
                }
            }
        }
        auto [dst_directory, dst_filename] = split_path_and_name(dst_path);
        AutoEntry dst_entry(get_path_entry(dst_directory).second);
        if (dst_entry == nullptr) {
            response << "copy: cannot stat file '" << temp_path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        } else {
            AutoEntry dst_entry(get_path_entry(dst_path).second);
            if (dst_entry == nullptr) {
                newfile(dst_path);
            }
        }
        err = check_entry(dst_entry.elem(), user, Option::WRITE);
        if (err == ErrorCode::FAILURE) {
            response << "copy: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        }
        src_entry.set(get_path_entry(src_path).second);
        err = lock(src_entry.elem()->inode_id, get_inode(src_entry.elem()->inode_id), Lock::READ_LOCK);
        if (err != ErrorCode::SUCCESS)  {
            response << "copy: cannot get the read lock of file '" << src_path << "'" << std::endl;
            return ErrorCode::FAILURE;
        }
        std::string data = cat_log(src_entry.elem());
        unlock(src_entry.elem()->inode_id, get_inode(src_entry.elem()->inode_id), Lock::READ_LOCK);
        err = write_data(dst_entry.elem(), dst_filename.c_str(), data);
        if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
            response << "copy: cannot stat file '" << temp_path << "': Exceeded the maximum name length (24 characters)" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::PERMISSION_DENIED) {
            response << "copy: Permission denied" << std::endl;
            return ErrorCode::FAILURE;
        } else if (err == ErrorCode::LOCKED) {
            response << "copy: cannot get the write lock of file '" << temp_path << "'" << std::endl;
            return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode useradd(const std::string &username, const std::string &password) {
        if (users.find(username) != users.end()) {
            response << "sudo: cannot add user `" << username << "`: User exists" << std::endl;
            return ErrorCode::FAILURE;
        }
        users[username] = password;
        std::ostringstream user_oss;
        user_oss << std::setw(8) << username << std::setfill(' ');
        std::ostringstream pwd_oss;
        pwd_oss << "    " << std::setw(8) << password << std::setfill(' ');
        std::string data = cat_log(user_log) + '\n' + user_oss.str() + pwd_oss.str();
        write_log(user_log, data.c_str());
        return ErrorCode::SUCCESS;
    }
    inline static std::map<std::string, std::string> users;
    void new_shell();
    std::string to_string(mode_t mode) {
        std::string result;
        // 用户权限
        result += (mode & S_IRUSR) ? "r" : "-";
        result += (mode & S_IWUSR) ? "w" : "-";
        result += (mode & S_IXUSR) ? "x" : "-";
        // 组权限
        result += (mode & S_IRGRP) ? "r" : "-";
        result += (mode & S_IWGRP) ? "w" : "-";
        result += (mode & S_IXGRP) ? "x" : "-";
        // 其他人权限
        result += (mode & S_IROTH) ? "r" : "-";
        result += (mode & S_IWOTH) ? "w" : "-";
        result += (mode & S_IXOTH) ? "x" : "-";
        return result;
    }
    ErrorCode check_entry(Entry* entry, const char* user, Option option) {
        Inode* inode = get_inode(entry->inode_id);
        mode_t mode = inode->mode;
        if (strcmp(inode->owner, user) == 0) {
            if (option == Option::WRITE) {
                if ((mode & S_IWUSR)) return ErrorCode::SUCCESS;
                else return ErrorCode::FAILURE;
            } else if (option == Option::READ) {
                if (mode & S_IRUSR) return ErrorCode::SUCCESS;
                else return ErrorCode::FAILURE;
            } else if (option == Option::EXEC) {
                if (mode & S_IXUSR) return ErrorCode::SUCCESS;
                else return ErrorCode::FAILURE;
            }
        }
        else {
            if (option == Option::WRITE) {
                if ((mode & S_IWOTH)) return ErrorCode::SUCCESS;
                else return ErrorCode::FAILURE;
            } else if (option == Option::READ) {
                if (mode & S_IROTH) return ErrorCode::SUCCESS;
                else return ErrorCode::FAILURE;
            } else if (option == Option::EXEC) {
                if (mode & S_IXOTH) return ErrorCode::SUCCESS;
                else return ErrorCode::FAILURE;
            }
        }
        return ErrorCode::FAILURE;
    }
    ErrorCode chmod(const std::string& option, const std::string& path) {
        auto [directory, filename] = split_path_and_name(path);
        AutoEntry entry(get_path_entry(directory).second);
        if (entry == nullptr) {
            response << "chmod: cannot access file '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        ErrorCode err = chmod_file(entry.elem(), filename.c_str(), option);
        if (err == ErrorCode::FAILURE || err == ErrorCode::FILE_NOT_FOUND || err == ErrorCode::EXCEEDED) {
            response << "chmod: cannot access file '" << path << "': No such file or directory" << std::endl;
            return ErrorCode::FAILURE;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode chmod_file(Entry *parent, const char *name, const std::string& option);
    enum class Lock {
        WRITE_LOCK,
        READ_LOCK,
    };
    ErrorCode check() {
//        std::string lock_log_data = cat_log(lock_log);
//        std::stringstream ss(lock_log_data);
//        std::vector<std::string> check_logs;
//        std::string temp;
//        while (ss >> temp) {
//            check_logs.push_back(temp);
//        }
//        uint32_t cnt = lock_cnt;
//        for (const auto& check_log: check_logs) {
//            if (check_log.find("unlock") != std::string::npos) {
//                --cnt;
//                continue;
//            }
//            if (check_log.find("lock") != std::string::npos) {
//                ++cnt;
//            }
//        }
//        if (cnt == lock_cnt) {
          response << "Simple OS is functioning properly.\n";
//        } else {
//            response << "Simple OS appears to be malfunctioning.\n";
//            response << "Initiating repair process...\n";
//        }
        return ErrorCode::SUCCESS;
    }
    uint32_t lock_cnt = 0;
    ErrorCode lock(uint32_t i, Inode* inode, Lock lock) {
        switch(lock) {
            case Lock::WRITE_LOCK: {
                auto [err, entry] = get_path_entry("/usr/lock/" + std::to_string(i) + ".rlock");
                AutoEntry autoEntry(entry);
                if (err == ErrorCode::SUCCESS) {
                    return ErrorCode::FAILURE;
                }
                std::tie(err, entry) = get_path_entry("/usr/lock/" + std::to_string(i) + ".wlock");
                autoEntry.set(entry);
                if (err == ErrorCode::SUCCESS) {
                    return ErrorCode::FAILURE;
                }
            } break;
            case Lock::READ_LOCK: {
                auto [err, entry] = get_path_entry("/usr/lock/" + std::to_string(i) + ".rlock");
                AutoEntry autoEntry(entry);
                if (err == ErrorCode::SUCCESS) {
                    uint32_t cnt = std::stol(cat_log(entry));
                    ++cnt;
                    write_log(entry, std::to_string(cnt));
                    return ErrorCode::SUCCESS;
                }
                std::tie(err, entry) = get_path_entry("/usr/lock/" + std::to_string(i) + ".wlock");
                autoEntry.set(entry);
                if (err == ErrorCode::SUCCESS) {
                    return ErrorCode::FAILURE;
                }
            } break;
        }
        switch (lock) {
            case Lock::WRITE_LOCK:
            {
//                std::string data = cat_log(lock_log);
//                std::stringstream ss;
//                auto now = std::chrono::system_clock::now();
                // 将时间点转换为time_t以便输出
//                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//                ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
//                ss << ": Write Lock: lock inode " + std::to_string(i);
//                write_log(lock_log, data + ss.str() + "\n");
                newfile("/usr/lock/" + std::to_string(i) + ".wlock");
                ++lock_cnt;
            }
                break;
            case Lock::READ_LOCK: {
//                std::string data = cat_log(lock_log);
//                std::stringstream ss;
//                auto now = std::chrono::system_clock::now();
//                // 将时间点转换为time_t以便输出
//                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//                ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
//                ss << ": Read Lock: lock inode " + std::to_string(i);
//                write_log(lock_log, data + ss.str() + "\n");
                newfile("/usr/lock/" + std::to_string(i) + ".rlock");
                ++lock_cnt;
                auto [err, entry] = get_path_entry("/usr/lock/" + std::to_string(i) + ".rlock");
                AutoEntry autoEntry(entry);
                if (err == ErrorCode::SUCCESS) {
                    write_log(entry, "1");
                    return ErrorCode::SUCCESS;
                }
            } break;
        }
        return ErrorCode::SUCCESS;
    }
    ErrorCode unlock(uint32_t i, Inode* inode, Lock lock) {
        switch (lock) {
            case Lock::WRITE_LOCK:{
                del("/usr/lock/" + std::to_string(i) + ".wlock");
                --lock_cnt;
//                std::string data = cat_log(lock_log);
//                std::stringstream ss;
//                auto now = std::chrono::system_clock::now();
//                // 将时间点转换为time_t以便输出
//                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//                ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
//                ss << ": Write Lock: unlock inode " + std::to_string(i);
//                write_log(lock_log, data + ss.str() + "\n");
            } break;
            case Lock::READ_LOCK: {
                auto [err, entry] = get_path_entry("/usr/lock/" + std::to_string(i) + ".rlock");
                AutoEntry autoEntry(entry);
                if (err == ErrorCode::SUCCESS) {
                    uint32_t cnt = std::stol(cat_log(entry));
                    --cnt;
                    if (cnt > 0) write_log(entry, std::to_string(cnt));
                    else {
                        del("/usr/lock/" + std::to_string(i) + ".rlock");
                        --lock_cnt;
//                        std::string data = cat_log(lock_log);
//                        std::stringstream ss;
//                        auto now = std::chrono::system_clock::now();
//                        // 将时间点转换为time_t以便输出
//                        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//                        ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
//                        ss << ": Read Lock: unlock inode " + std::to_string(i);
//                        write_log(lock_log, data + ss.str() + "\n");
                    }
                }
            } break;
        }
        return ErrorCode::SUCCESS;
    }
};

#endif //SIMPLE_OS_FILESYSTEM_H
