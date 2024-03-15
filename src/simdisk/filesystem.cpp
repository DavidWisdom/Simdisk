//
// Created by eric on 10/19/23.
//

#include "filesystem.h"
#include <fstream>
#include <cstring>
// 设置 Inode 的数据块信息，根据需要的块数和分配的块列表
void set_blocks(Inode* inode, const std::vector<uint32_t>& blocks, uint32_t needed_blocks_num) {
    using AutoBlock = Filesystem::AutoBlock;

    // 若块数不超过直接块的数量（6个直接块）
    if (needed_blocks_num <= 6) {
        for (uint32_t i = 0; i < needed_blocks_num; i++) {
            inode->i_block[i] = blocks[i];
        }
    }
        // 若块数在直接块和一级间接块的范围内
    else if (needed_blocks_num <= 6 + POINTERS_PER_BLOCK) {
        // 填充直接块
        for (uint32_t i = 0; i < 6; i++) {
            inode->i_block[i] = blocks[i];
        }

        // 创建一级间接块
        AutoBlock block;
        inode->i_block[6] = block.id();
        memset(block.elem()->pointers, null, sizeof(block.elem()->pointers));

        // 填充一级间接块
        for (uint32_t i = 6; i < needed_blocks_num; ++i) {
            block.elem()->pointers[i - 6] = blocks[i];
        }
    }
        // 若块数在直接块、一级间接块和二级间接块的范围内
    else if (needed_blocks_num <= 6 + POINTERS_PER_BLOCK + POINTERS_PER_BLOCK * POINTERS_PER_BLOCK) {
        // 填充直接块
        for (uint32_t i = 0; i < 6; i++) {
            inode->i_block[i] = blocks[i];
        }

        // 创建一级间接块
        AutoBlock block;
        inode->i_block[6] = block.id();
        memset(block.elem()->pointers, null, sizeof(block.elem()->pointers));

        // 填充一级间接块
        for (uint32_t i = 6; i < 6 + POINTERS_PER_BLOCK; ++i) {
            block.elem()->pointers[i - 6] = blocks[i];
        }

        // 创建二级间接块
        AutoBlock indirect_block;
        inode->i_block[7] = indirect_block.id();
        memset(indirect_block.elem()->pointers, null, sizeof(indirect_block.elem()->pointers));

        uint32_t cnt = needed_blocks_num - 6 - POINTERS_PER_BLOCK;
        bool flag = false;

        // 填充二级间接块
        for (uint32_t i = 0; i < POINTERS_PER_BLOCK; ++i) {
            if (flag) break;
            AutoBlock block;
            memset(block.elem()->pointers, null, sizeof(block.elem()->pointers));
            indirect_block.elem()->pointers[i] = block.id();

            // 填充块
            for (uint32_t j = 0; j < POINTERS_PER_BLOCK; ++j) {
                block.elem()->pointers[j] = blocks[needed_blocks_num - cnt];
                --cnt;
                if (cnt == 0) {
                    flag = true;
                    break;
                }
            }
        }
    }
    // 处理其他情况
    else {
        // 超出二级间接块的情况，此时为三级间接块的情况
        // 填充直接块
        for (uint32_t i = 0; i < 6; i++) {
            inode->i_block[i] = blocks[i];
        }

        // 创建一级间接块
        AutoBlock block;
        inode->i_block[6] = block.id();
        memset(block.elem()->pointers, null, sizeof(block.elem()->pointers));

        // 填充一级间接块
        for (uint32_t i = 6; i < 6 + POINTERS_PER_BLOCK; ++i) {
            block.elem()->pointers[i - 6] = blocks[i];
        }

        // 创建二级间接块
        AutoBlock indirect_block;
        inode->i_block[7] = indirect_block.id();
        memset(indirect_block.elem()->pointers, null, sizeof(indirect_block.elem()->pointers));

        uint32_t cnt = needed_blocks_num - 6 - POINTERS_PER_BLOCK;
        bool flag = false;

        // 填充二级间接块
        for (uint32_t i = 0; i < POINTERS_PER_BLOCK; ++i) {
            if (flag) break;
            AutoBlock block;
            memset(block.elem()->pointers, null, sizeof(block.elem()->pointers));
            indirect_block.elem()->pointers[i] = block.id();

            // 填充块
            for (uint32_t j = 0; j < POINTERS_PER_BLOCK; ++j) {
                block.elem()->pointers[j] = blocks[needed_blocks_num - cnt];
                --cnt;
                if (cnt == 0) {
                    flag = true;
                    break;
                }
            }
        }
    }
}
// 从 Inode 中获取块的信息
std::vector<uint32_t> get_blocks(Inode* inode) {
    // 如果 Inode 无效，返回空向量
    if (!inode->is_valid) {
        return {};
    }

    std::vector<uint32_t> res;

    // 处理直接块
    for (uint32_t i = 0; i <= 5; ++i) {
        // 如果直接块为空，返回当前结果
        if (inode->i_block[i] == null) {
            return res;
        }
        res.push_back(inode->i_block[i]);
    }

    std::vector<uint32_t> temp;

    // 处理一级间接块
    if (inode->i_block[6] != null) {
        temp.push_back(inode->i_block[6]);
    }

    // 处理二级间接块
    if (inode->i_block[7] != null) {
        // 获取二级间接块
        Filesystem::AutoBlock indirect_block(inode->i_block[7]);

        // 遍历填充一级间接块的指针
        for (uint32_t i = 0; i < POINTERS_PER_BLOCK; ++i) {
            // 如果指针为空，跳出循环
            if (indirect_block.elem()->pointers[i] == null) {
                break;
            }
            temp.push_back(indirect_block.elem()->pointers[i]);
        }
    }

    // 处理其他情况，如三级间接块（TODO）

    // 遍历填充块的信息
    for (uint32_t i = 0; i < temp.size(); ++i) {
        // 获取块
        Filesystem::AutoBlock blocks(temp[i]);

        // 遍历填充块的指针
        for (uint32_t j = 0; j < POINTERS_PER_BLOCK; ++j) {
            // 如果指针为空，返回当前结果
            if (blocks.elem()->pointers[j] == null) {
                return res;
            }
            res.push_back(blocks.elem()->pointers[j]);
        }
    }

    return res;
}
// 创建一个新的数据块
std::pair<uint32_t, Block*> Filesystem::new_block() {
    // 从块位图中获取一个新的块索引
    uint32_t i = blocks_bitmap->_new();

    // 保存块位图的状态
    blocks_bitmap->save();

    // 如果块索引为空，返回空指针
    if (i == null) {
        return {null, nullptr};
    } else {
        // 否则，返回块索引和相应的块指针
        return {i, Disk::read_block(i)};
    }
}

// 获取指定索引的数据块
inline Block* Filesystem::get_block(uint32_t i) {
    // 如果索引为空，返回空指针
    if (i == null) {
        return nullptr;
    }

    // 否则，读取并返回相应的数据块指针
    return Disk::read_block(i);
}

// 删除指定索引的数据块
void Filesystem::delete_block(uint32_t i) {
    // 如果索引为空，直接返回
    if (i == null) {
        return;
    }

    // 从块位图中删除指定索引的块
    blocks_bitmap->_delete(i);

    // 保存块位图的状态
    blocks_bitmap->save();
}

// 保存数据块到指定索引
void Filesystem::save_block(uint32_t i, Block* block) {
    // 将数据块写入磁盘的指定索引位置
    Disk::write_block(i, block);
}

// 释放数据块内存
void Filesystem::release_block(Block* block) {
    // 删除数据块对象，释放内存
    delete block;
}


void Filesystem::_new(std::string name) {
    Disk::disk_name = std::move(name);
    Disk::new_disk();
    super = Disk::read_block(0);
    super->superblock = Superblock();
    blocks_bitmap = new Bitmap(super->superblock.blocks_num, 1);
    inodes_bitmap = new Bitmap(super->superblock.inodes_num, 1 + super->superblock.blocks_bitmap_num);
    inodes_table = new InodesTable(super->superblock.inodes_table_block, 1 + super->superblock.blocks_bitmap_num + super->superblock.inodes_bitmap_num);
    uint32_t offset = 1 + super->superblock.blocks_bitmap_num + super->superblock.inodes_bitmap_num + super->superblock.inodes_table_block;
    for (uint32_t i = 0; i < offset; ++i) {
        blocks_bitmap->set(i);
    }
    // 创建根目录
    auto [root_block_id, root_block] = new_block();
    super->superblock.root_block_id = root_block_id;
    root = root_block;
    for (auto& entry: root->entries) {
        entry.is_valid = false;
    }

    // 根目录 i结点
    auto [root_inode_id, root_inode] = new_inode();
    super->superblock.root_inode_id = root_inode_id;
    root->entries[0].is_valid = true;
    root->entries[0].inode_id = root_inode_id;
    strcpy(root->entries[0].name, "/");

    // 新建block，用于给根目录存放内容
    auto [block_id, block] = new_block();
    for (auto& entry : block->entries) {
        entry.is_valid = false;
    }
    // 根目录的内容
    root_inode->is_valid = true;
    root_inode->link_cnt = 3;
    root_inode->size = sizeof(Entry) * 2;
    root_inode->capacity = 1024;
    root_inode->mode = to_mode("rwxr-xr-x");
    root_inode->type = 'd';
    strcpy(root_inode->owner, "root");
    memset(root_inode->i_block, -1, sizeof(root_inode->i_block));
    root_inode->i_block[0] = block_id;

    // 创建 . 文件夹
    block->entries[0].is_valid = true;
    block->entries[0].inode_id = root_inode_id;
    strcpy(block->entries[0].name, ".");

    // 创建 .. 文件夹
    block->entries[1].is_valid = true;
    block->entries[1].inode_id = root_inode_id;
    strcpy(block->entries[1].name, "..");
    save_block(0, super);
    save_block(super->superblock.root_block_id, root);
    save_block(block_id, block);
    release_block(block);
    blocks_bitmap->save();
    inodes_bitmap->save();
    inodes_table->save();
    current_shell_pid = 0;
    Filesystem::Info info;
    strcpy(info.username, "root");
    pid_map[0] = info;
    pid_map[0].last_entry.set(nullptr);
    pid_map[0].current_entry.set(Entry::clone(&root->entries[0]));
    pid_map[0].root_entry.set(Entry::clone(&root->entries[0]));
    md("/home");
    md("/lost+found");
    md("/proc");
    md("/root");
    md("/usr");
    md("/usr/lock");
    newfile("/usr/user.log");
//    newfile("/usr/system.log");
//    newfile("/usr/lock/lock.log");
    user_log = get_path_entry("/usr/user.log").second;
//    system_log = get_path_entry("/usr/system.log").second;
//    lock_log = get_path_entry("/usr/lock/lock.log").second;
    write_log(user_log, "username    password");
    useradd("root", "root");
    auto only_root_read = [&](const std::string& path) {
        chmod("g-r", path);
        chmod("o-r", path);
        chmod("g-x", path);
        chmod("o-x", path);
        chmod("a-w", path);
    };
//    only_root_read("/usr/user.log");
//    only_root_read("/usr/system.log");
    chmod("a-w", "/");
    system(("zip backup.zip " + Disk::disk_name).c_str());
//    std::cout << "zip backup.zip " + Disk::disk_name << std::endl;
    copy_host("backup.zip", "/lost+found/backup.img");
//    Block* beforeblock = Disk::read_block(6438);
//    std::string content;
//    for (uint32_t i = 0; i < BLOCK_SIZE; ++i) {
//        content.push_back(*((char*)((void*)beforeblock) + i));
//    }
//    std::ofstream test("before.zip", std::ios::binary);
//    test << content;
//    test.close();
//    delete beforeblock;
    system("rm backup.zip");
}

// 加载文件系统
void Filesystem::load(std::string name) {
    // 设置磁盘文件名
    Disk::disk_name = std::move(name);

    // 读取超级块
    super = Disk::read_block(0);

    // 初始化位图和InodesTable
    blocks_bitmap = new Bitmap(super->superblock.blocks_num, 1);
    inodes_bitmap = new Bitmap(super->superblock.inodes_num, 1 + super->superblock.blocks_bitmap_num);
    inodes_table = new InodesTable(super->superblock.inodes_table_block, 1 + super->superblock.blocks_bitmap_num + super->superblock.inodes_bitmap_num);

    // 读取根目录块
    root = Disk::read_block(super->superblock.root_block_id);

    // 初始化当前Shell的信息
    current_shell_pid = 0;
    Filesystem::Info info;
    strcpy(info.username, "root");
    pid_map[0] = info;
    pid_map[0].last_entry.set(nullptr);
    pid_map[0].current_entry.set(Entry::clone(&root->entries[0]));
    pid_map[0].root_entry.set(Entry::clone(&root->entries[0]));

    // 获取系统日志、用户日志、锁日志的Entry
    user_log = get_path_entry("/usr/user.log").second;
//    system_log = get_path_entry("/usr/system.log").second;
//    lock_log = get_path_entry("/usr/lock/lock.log").second;

    // 读取系统日志内容
//    std::string system_log_data = cat_log(system_log);
//    std::stringstream system_log_ss(system_log_data);
//    std::vector<std::string> log_data;
//    std::string temp;
//    while (std::getline(system_log_ss, temp)) {
//        log_data.push_back(temp);
//    }

//    // 根据系统日志内容进行状态恢复或备份
//    if (!load_state) {
//        if (temp.find("is processing") != std::string::npos) {
//            // 如果日志中包含处理中信息，则进行状态恢复
//            copy_to_host("/lost+found/backup.img", "backup.zip");
//            system("unzip -o backup.zip");
//            system("rm backup.zip");
//            printf("The disk image is broken and is currently being repaired...\n");
//            release();
//            load_state = true;
//            load(Disk::disk_name);
//        } else {
//            // 如果没有处理中信息，则进行备份
//            system(("zip backup.zip " + Disk::disk_name).c_str());
//            copy_host("backup.zip", "/lost+found/backup.img");
//            system("rm backup.zip");
//        }
//    } else {
//        // 已经进行过状态恢复，继续备份
//        system(("zip backup.zip " + Disk::disk_name).c_str());
//        copy_host("backup.zip", "/lost+found/backup.img");
//        system("rm backup.zip");
//    }

    // 获取当前时间，并将其格式化为字符串
//    auto now = std::chrono::system_clock::now();
//    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
//    std::stringstream ss;
//    ss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
//    ss << ": Simdisk is rebooting...";
//
//    // 将重启信息写入系统日志
//    write_log(system_log, log_data.empty() ? "" : log_data.back() + "\n" + ss.str());
}

// 释放文件系统相关资源
void Filesystem::release() {
    delete super;
    delete blocks_bitmap;
    delete inodes_bitmap;
    delete inodes_table;
    delete root;
    delete user_log;
//    delete system_log;
//    delete lock_log;
}

// 创建一个新的inode，返回inode的索引和指针
std::pair<uint32_t, Inode *> Filesystem::new_inode() {
    uint32_t i = inodes_bitmap->_new();
    inodes_bitmap->save();
    if (i == null) return {null, nullptr};
    uint32_t inodeIndex = i / INODES_PER_BLOCK;
    uint32_t inodeOffset = i % INODES_PER_BLOCK;
    return {i, &inodes_table->inodes_table[inodeIndex]->inodes[inodeOffset]};
}

// 列出目录内容，支持带参数和不带参数两种模式
ErrorCode Filesystem::dir(const std::string &path, bool with_args, const char *user) {
    AutoEntry entry;
    // 如果路径为空，获取当前目录的Entry
    if (path.empty()) {
        entry.set(Entry::clone(pid_map[current_shell_pid].current_entry.elem()));
    } else {
        // 否则，根据路径获取对应的Entry
        entry.set(get_path_entry(path).second);
        if (entry == nullptr) {
            response << "dir: cannot access '" << path << "': No such file or directory" << '\n';
            return ErrorCode::FAILURE;
        }
    }
    // 如果Entry对应的Inode类型为文件，则直接打印文件名
    if (get_inode(entry.elem()->inode_id)->type == 'f') {
        response << entry.elem()->name << '\n';
        return ErrorCode::SUCCESS;
    }
    bool state = false;
    Inode* inode = get_inode(entry.elem()->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
    // 检查对目录的读权限
    ErrorCode err = check_entry(entry.elem(), user, Option::READ);
    if (err == ErrorCode::FAILURE) {
        response << "dir: Permission denied" << '\n';
        return ErrorCode::FAILURE;
    }
    AutoBlock block(0, inode);
    for (const auto& file: block.elem()->entries) {
        if (with_args) {
            // 带参数模式下，只打印目录（排除"."和".."）的名称
            if (file.is_valid && get_inode(file.inode_id)->type == 'd') {
                if (file.name[0] != '.') {
                    response << file.name << "    ";
                    state = true;
                }
            }
        } else {
            // 不带参数模式下，打印所有有效文件和目录的名称
            if (file.is_valid) {
                if (file.name[0] != '.') {
                    response << file.name << "    ";
                    state = true;
                }
            }
        }
    }
    if (state) response << '\n';
    return ErrorCode::SUCCESS;
}
// 创建新的目录
ErrorCode Filesystem::new_directory(Entry *parent, const char *name, const char *user) {
    // 检查名称长度是否超出限制
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;

    // 检查父目录是否有效
    if (!parent->is_valid) return ErrorCode::FAILURE;

    // 获取父目录对应的Inode
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;

    // 获取父目录的数据块
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;

    // 检查是否已存在同名的目录
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            return ErrorCode::EXISTS;
        }
    }

    // 新建子目录的Inode
    auto [child_inode_id, child_inode] = new_inode();

    // 在父目录中添加新目录的Entry
    for (auto& entry: block.elem()->entries) {
        if (!entry.is_valid) {
            entry.is_valid = true;
            entry.inode_id = child_inode_id;
            inode->size += sizeof(Entry);
            strcpy(entry.name, name);
            block.save();
            break;
        }
    }

    // 设置子目录的Inode信息
    child_inode->set_data(true, 2, sizeof(Entry) * 2, 1024, to_mode("rwxr-xr-x"), 'd', user);
    memset(child_inode->i_block, -1, sizeof(child_inode->i_block));

    // 获取子目录的数据块
    AutoBlock child_block;
    child_inode->i_block[0] = child_block.id();

    // 初始化子目录的数据块中的Entries
    for (auto& entry : child_block.elem()->entries) {
        entry.is_valid = false;
    }

    // 设置子目录的特殊Entries（. 和 ..）
    child_block.elem()->entries[0].is_valid = true;
    child_block.elem()->entries[0].inode_id = child_inode_id;
    strcpy(child_block.elem()->entries[0].name, ".");
    child_block.elem()->entries[1].is_valid = true;
    child_block.elem()->entries[1].inode_id = parent->inode_id;
    strcpy(child_block.elem()->entries[1].name, "..");

    // 保存父目录和子目录的Inode信息
    save_inode(parent->inode_id);
    save_inode(child_inode_id);

    return ErrorCode::SUCCESS;
}

// 删除文件
ErrorCode Filesystem::delete_file(Entry *parent, const char *name, const char *user) {
    // 检查父目录是否有效
    if (!parent->is_valid) return ErrorCode::FAILURE;

    // 获取父目录对应的Inode
    Inode* parent_inode = get_inode(parent->inode_id);
    if (parent_inode == nullptr || !parent_inode->is_valid) return ErrorCode::FAILURE;

    // 获取父目录的数据块
    AutoBlock block(0, parent_inode);
    if (block == nullptr) return ErrorCode::FAILURE;

    // 遍历父目录的Entries
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            // 获取待删除文件的Inode
            Inode* inode = get_inode(entry.inode_id);

            // 如果是目录，返回错误
            if (inode->type == 'd') {
                return ErrorCode::FILE_NOT_MATCH;
            }

            // 获取文件的数据块并删除
            std::vector<uint32_t> blocks = get_blocks(inode);
            for (uint32_t i = 0; i < blocks.size(); ++i) {
                delete_block(blocks[i]);
            }

            // 删除文件的索引节点和数据块
            for (uint32_t i = 6; i < 9; ++i) {
                delete_block(inode->i_block[i]);
            }
            delete_inode(entry.inode_id);

            // 标记父目录Entry为无效，并更新父目录的大小
            entry.is_valid = false;
            parent_inode->size -= sizeof(Entry);

            // 保存父目录的Inode和数据块
            save_inode(parent->inode_id);
            block.save();

            return ErrorCode::SUCCESS;
        }
    }

    // 文件未找到
    return ErrorCode::FILE_NOT_FOUND;
}

// 修改文件权限
ErrorCode Filesystem::chmod_file(Entry *parent, const char *name, const std::string& option) {
    // 检查父目录是否有效
    if (!parent->is_valid) return ErrorCode::FAILURE;

    // 获取父目录对应的Inode
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;

    // 获取父目录的数据块
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;

    // 遍历父目录的Entries
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            Inode* inode = get_inode(entry.inode_id);
            mode_t& mode = inode->mode;
            if (option.size() < 2) return ErrorCode::FAILURE;
            if (option[0] == 'a') {
                if (option[1] == '+') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode |= S_IRUSR; mode |= S_IRGRP; mode |= S_IROTH; break;
                            case 'w': mode |= S_IWUSR; mode |= S_IWGRP; mode |= S_IWOTH; break;
                            case 'x': mode |= S_IXUSR; mode |= S_IXGRP; mode |= S_IXOTH; break;
                        }
                    }
                } else if (option[1] == '-') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode &= ~S_IRUSR; mode &= ~S_IRGRP; mode &= ~S_IROTH; break;
                            case 'w': mode &= ~S_IWUSR; mode &= ~S_IWGRP; mode &= ~S_IWOTH; break;
                            case 'x': mode &= ~S_IXUSR; mode &= ~S_IXGRP; mode &= ~S_IXOTH; break;
                        }
                    }
                } else {
                    return ErrorCode::FAILURE;
                }
            } else if (option[0] == 'g') {
                if (option[1] == '+') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode |= S_IRGRP; break;
                            case 'w': mode |= S_IWGRP; break;
                            case 'x': mode |= S_IRGRP; break;
                        }
                    }
                } else if (option[1] == '-') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode &= ~S_IRGRP; break;
                            case 'w': mode &= ~S_IWGRP; break;
                            case 'x': mode &= ~S_IXGRP; break;
                        }
                    }
                } else {
                    return ErrorCode::FAILURE;
                }
            } else if (option[0] == 'u') {
                if (option[1] == '+') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode |= S_IRUSR; break;
                            case 'w': mode |= S_IWUSR; break;
                            case 'x': mode |= S_IRUSR; break;
                        }
                    }
                } else if (option[1] == '-') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode &= ~S_IRUSR; break;
                            case 'w': mode &= ~S_IWUSR; break;
                            case 'x': mode &= ~S_IXUSR; break;
                        }
                    }
                } else {
                    return ErrorCode::FAILURE;
                }
            } else if (option[0] == 'o') {
                if (option[1] == '+') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode |= S_IROTH; break;
                            case 'w': mode |= S_IWOTH; break;
                            case 'x': mode |= S_IROTH; break;
                        }
                    }
                } else if (option[1] == '-') {
                    for (uint32_t i = 2; i < option.size(); ++i) {
                        switch(option[i]) {
                            case 'r': mode &= ~S_IROTH; break;
                            case 'w': mode &= ~S_IWOTH; break;
                            case 'x': mode &= ~S_IXOTH; break;
                        }
                    }
                } else {
                    return ErrorCode::FAILURE;
                }
            } else {
                return ErrorCode::FAILURE;
            }
            save_inode(entry.inode_id);
            return ErrorCode::SUCCESS;
        }
    }
    return ErrorCode::FILE_NOT_FOUND;
}
ErrorCode Filesystem::write_log(Entry* log, const std::string& contents) {
    Inode* inode = get_inode(log->inode_id);
    if (inode->type == 'd') {
        return ErrorCode::FILE_NOT_MATCH;
    }
    std::vector<uint32_t> blocks = get_blocks(inode);
    uint32_t needed_blocks_num = (contents.size() + super->superblock.block_size - 1) / super->superblock.block_size;
    if (needed_blocks_num == 0) needed_blocks_num = 1;
    uint32_t blocks_num = blocks.size();
    if (needed_blocks_num < blocks_num) {
        for (uint32_t i = needed_blocks_num; i < blocks_num; ++i) {
            delete_block(blocks[i]);
        }
    } else if (needed_blocks_num > blocks_num) {
        blocks.resize(needed_blocks_num);
        for (uint32_t i = blocks_num; i < needed_blocks_num; ++i) {
            AutoBlock data_block;
            blocks[i] = data_block.id();
        }
    }
    for (uint32_t i = 0; i < needed_blocks_num; ++i) {
        AutoBlock data_block(blocks[i], GET | WRITE_MODE);
        size_t length = std::min((uint32_t)contents.size(), (i + 1) * super->superblock.block_size) - i * super->superblock.block_size;
//        strcpy(data_block.elem()->data, contents.substr(i * super->superblock.block_size, length).c_str());
        memcpy(data_block.elem()->data, contents.substr(i * super->superblock.block_size, length).c_str(), length);
    }
    if (inode->i_block[6] != null) {
        delete_block(inode->i_block[6]);
    }
    if (inode->i_block[7] != null) {
        delete_block(inode->i_block[7]);
    }
    if (inode->i_block[8] != null) {
        delete_block(inode->i_block[8]);
    }
    memset(inode->i_block, null, sizeof(inode->i_block));
    set_blocks(inode, blocks, needed_blocks_num);
    inode->size = contents.size();
    inode->capacity = needed_blocks_num * super->superblock.block_size;
    save_inode(log->inode_id);
    return ErrorCode::SUCCESS;
}
ErrorCode Filesystem::write_file(Entry *parent, const char *name, const char *user) {
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;
    if (!parent->is_valid) return ErrorCode::FAILURE;
    Inode* parent_inode = get_inode(parent->inode_id);
    if (parent_inode == nullptr || !parent_inode->is_valid) return ErrorCode::FAILURE;
    AutoBlock block(0, parent_inode);
    if (block == nullptr) return ErrorCode::FAILURE;
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            Inode* inode = get_inode(entry.inode_id);
            if (inode->type == 'd') {
                return ErrorCode::FILE_NOT_MATCH;
            }
            std::ifstream file(name, std::ios::binary);
            if (!file.is_open()) return ErrorCode::FAILURE;
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            std::string contents = buffer.str();
            std::vector<uint32_t> blocks = get_blocks(inode);
            uint32_t needed_blocks_num = (contents.size() + super->superblock.block_size - 1) / super->superblock.block_size;
            if (needed_blocks_num == 0) needed_blocks_num = 1;
            uint32_t blocks_num = blocks.size();
            if (needed_blocks_num < blocks_num) {
                for (uint32_t i = needed_blocks_num; i < blocks_num; ++i) {
                    delete_block(blocks[i]);
                }
                blocks.resize(needed_blocks_num);
            } else if (needed_blocks_num > blocks_num) {
                blocks.resize(needed_blocks_num);
                for (uint32_t i = blocks_num; i < needed_blocks_num; ++i) {
                    AutoBlock data_block;
                    blocks[i] = data_block.id();
                }
            }
            for (uint32_t i = 0; i < needed_blocks_num; ++i) {
                AutoBlock data_block(blocks[i], GET | WRITE_MODE);
                Block* data = data_block.elem();
                size_t length = std::min((uint32_t)contents.size(), (i + 1) * super->superblock.block_size) - i * super->superblock.block_size;
//                strcpy(data->data, contents.substr(i * super->superblock.block_size, length).c_str());
                memcpy(data->data, contents.substr(i * super->superblock.block_size, length).c_str(), length);
            }
            if (inode->i_block[6] != null) {
                delete_block(inode->i_block[6]);
            }
            if (inode->i_block[7] != null) {
                delete_block(inode->i_block[7]);
            }
            if (inode->i_block[8] != null) {
                delete_block(inode->i_block[8]);
            }
            memset(inode->i_block, null, sizeof(inode->i_block));
            set_blocks(inode, blocks, needed_blocks_num);
            inode->size = contents.size();
            inode->capacity = needed_blocks_num * super->superblock.block_size;
            save_inode(entry.inode_id);
            save_inode(parent->inode_id);
            unlock(entry.inode_id, inode, Lock::WRITE_LOCK);
            return ErrorCode::SUCCESS;
        }
    }
    return ErrorCode::FILE_NOT_FOUND;
}
ErrorCode Filesystem::write_data(Entry *parent, const char* name, const std::string& contents, const char *user) {
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;
    if (!parent->is_valid) return ErrorCode::FAILURE;
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            ErrorCode err = check_entry(&entry, user, Option::WRITE);
            if (err != ErrorCode::SUCCESS) return ErrorCode::PERMISSION_DENIED;
            Inode* inode = get_inode(entry.inode_id);
            if (inode->type == 'd') {
                return ErrorCode::FILE_NOT_MATCH;
            }
            err = lock(entry.inode_id, inode, Lock::WRITE_LOCK);
            if (err != ErrorCode::SUCCESS) return ErrorCode::LOCKED;
            std::vector<uint32_t> blocks = get_blocks(inode);
            uint32_t needed_blocks_num = (contents.size() + super->superblock.block_size - 1) / super->superblock.block_size;
            if (needed_blocks_num == 0) needed_blocks_num = 1;
            uint32_t blocks_num = blocks.size();
            if (needed_blocks_num < blocks_num) {
                for (uint32_t i = needed_blocks_num; i < blocks_num; ++i) {
                    delete_block(blocks[i]);
                }
                blocks.resize(needed_blocks_num);
            } else if (needed_blocks_num > blocks_num) {
                blocks.resize(needed_blocks_num);
                for (uint32_t i = blocks_num; i < needed_blocks_num; ++i) {
                    AutoBlock data_block;
                    blocks[i] = data_block.id();
                }
            }
//            for (uint32_t i = 0; i < blocks.size(); ++i) {
//                if (blocks[i] == 0) {
//                    std::cout << "Error" << std::endl;
//                    assert(false);
//                }
//            }
            for (uint32_t i = 0; i < needed_blocks_num; ++i) {
                AutoBlock data_block(blocks[i], GET | WRITE_MODE);
                Block* data = data_block.elem();
                size_t length = std::min((uint32_t)contents.size(), (i + 1) * super->superblock.block_size) - i * super->superblock.block_size;
                memcpy(data->data, contents.substr(i * super->superblock.block_size, length).c_str(), length);
//                std::string content = contents.substr(i * super->superblock.block_size, length);
//                for (uint32_t j = 0; j < content.size(); ++j) {
//                    data->data[i] = content[i];
//                }
//                strcpy(data->data, contents.substr(i * super->superblock.block_size, length).c_str());
            }
            if (inode->i_block[6] != null) {
                delete_block(inode->i_block[6]);
            }
            if (inode->i_block[7] != null) {
                delete_block(inode->i_block[7]);
            }
            if (inode->i_block[8] != null) {
                delete_block(inode->i_block[8]);
            }
            memset(inode->i_block, null, sizeof(inode->i_block));
            set_blocks(inode, blocks, needed_blocks_num);
            inode->size = contents.size();
//            Block* beforeblock = Disk::read_block(6438);
//            std::string content;
//            for (uint32_t i = 0; i < BLOCK_SIZE; ++i) {
//                content.push_back(*((char*)((void*)beforeblock) + i));
//            }
//            std::ofstream test("before.zip", std::ios::binary);
//            test << content;
//            test.close();
//            delete beforeblock;
//             std::ofstream ofs("mytest.zip", std::ios::binary);
//             ofs << contents;
//             ofs.close();
            inode->capacity = needed_blocks_num * super->superblock.block_size;
            save_inode(entry.inode_id);
            save_inode(parent->inode_id);
            unlock(entry.inode_id, inode, Lock::WRITE_LOCK);
            return ErrorCode::SUCCESS;
        }
    }
    return ErrorCode::FILE_NOT_FOUND;
}
ErrorCode Filesystem::new_file(Entry *parent, const char *name, const char *user) {
    // Inode of parent
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;
    if (!parent->is_valid) return ErrorCode::FAILURE;
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            return ErrorCode::EXISTS;
        }
    }
    // Inode of child
    // 新建文件夹的对应i结点
    auto [child_inode_id, child_inode] = new_inode();
    for (auto& entry: block.elem()->entries) {
        if (!entry.is_valid) {
            entry.is_valid = true;
            entry.inode_id = child_inode_id;
            inode->size += sizeof(Entry);
            strcpy(entry.name, name);
            block.save();
            break;
        }
    }
    child_inode->set_data(true, 1, 0, 1024, to_mode("rwxr-xr-x"), 'f', user);
    memset(child_inode->i_block, -1, sizeof(child_inode->i_block));
    AutoBlock child_block;
    child_inode->i_block[0] = child_block.id();
//    strcpy(child_block.elem()->data, "\0");
    memcpy(child_block.elem()->data, "\0", BLOCK_SIZE);
    save_inode(parent->inode_id);
    save_inode(child_inode_id);
    return ErrorCode::SUCCESS;
}

// 获取文件信息
ErrorCode Filesystem::get_file(Entry *parent, const char *name, const char *user) {
    // 检查文件名长度是否超过限制
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;

    // 检查父目录是否有效
    if (!parent->is_valid) return ErrorCode::FAILURE;

    // 获取父目录对应的Inode
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;

    // 获取父目录的数据块
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;

    // 遍历父目录的Entries
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            // 检查目标是否为文件夹
            if (get_inode(entry.inode_id)->type == 'd') {
                return ErrorCode::FILE_NOT_MATCH;
            }
            return ErrorCode::SUCCESS;
        }
    }
    return ErrorCode::FILE_NOT_FOUND;
}

// 释放文件
ErrorCode Filesystem::release_file(Entry *parent, const char *name) {
    // 检查文件名长度是否超过限制
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;

    // 检查父目录是否有效
    if (!parent->is_valid) return ErrorCode::FAILURE;

    // 获取父目录对应的Inode
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;

    // 获取父目录的数据块
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;

    // 遍历父目录的Entries
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            // 检查目标是否为文件夹
            if (get_inode(entry.inode_id)->type == 'd') {
                return ErrorCode::FILE_NOT_MATCH;
            }

            // 释放文件锁
            unlock(entry.inode_id, inode, Lock::READ_LOCK);
            return ErrorCode::SUCCESS;
        }
    }
    return ErrorCode::FILE_NOT_FOUND;
}

ErrorCode Filesystem::cat_data(Entry *parent, const char *name, std::string& data, const char *user) {
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;
    if (!parent->is_valid) return ErrorCode::FAILURE;
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            ErrorCode err = check_entry(&entry, user, Option::READ);
            if (err != ErrorCode::SUCCESS) return ErrorCode::PERMISSION_DENIED;
            if (get_inode(entry.inode_id)->type == 'd') {
                return ErrorCode::FILE_NOT_MATCH;
            }
            err = lock(entry.inode_id, inode, Lock::READ_LOCK);
            if (err != ErrorCode::SUCCESS) return ErrorCode::LOCKED;
            data = cat_log(&entry);
            return ErrorCode::SUCCESS;
        }
    }
    return ErrorCode::FILE_NOT_FOUND;
}
std::string Filesystem::cat_log(Entry *log) {
    Inode* inode = get_inode(log->inode_id);
    if (inode->size == 0) return "";
    std::vector<uint32_t> blocks = get_blocks(inode);
    std::string contents;
    for (uint32_t i = 0; i < blocks.size(); ++i) {
        AutoBlock data_block(blocks[i]);
        contents += data_block.data();
    }
    contents = contents.substr(0, inode->size);
    return contents;
}
ErrorCode Filesystem::cat_file(Entry *parent, const char *name, Option option, const char *user) {
    // Inode of parent
    if (strlen(name) > MAX_LENGTH) return ErrorCode::EXCEEDED;
    if (!parent->is_valid) return ErrorCode::FAILURE;
    Inode* inode = get_inode(parent->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
    AutoBlock block(0, inode);
    if (block == nullptr) return ErrorCode::FAILURE;
    for (auto& entry: block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            ErrorCode err = check_entry(&entry, user, option);
            if (err != ErrorCode::SUCCESS) return ErrorCode::PERMISSION_DENIED;
            Inode* inode = get_inode(entry.inode_id);
            if (inode->type == 'd') {
                return ErrorCode::FILE_NOT_MATCH;
            }
            if (option == Option::READ) {
                err = lock(entry.inode_id, inode, Lock::READ_LOCK);
                if (err != ErrorCode::SUCCESS) return ErrorCode::LOCKED;
            } else if (option == Option::WRITE) {
                err = lock(entry.inode_id, inode, Lock::WRITE_LOCK);
                if (err != ErrorCode::SUCCESS) return ErrorCode::LOCKED;
            }
            std::vector<uint32_t> blocks = get_blocks(inode);
            if (blocks.empty()) return ErrorCode::FAILURE;
            std::string contents;
//            char* data = new char[BLOCK_SIZE * blocks.size()];
//            char* data_pos = data;
            for (uint32_t i = 0; i < blocks.size(); ++i) {
                AutoBlock data_block(blocks[i]);
                contents += data_block.data();
//                memcpy(data_pos, data_block.data(), BLOCK_SIZE);
//                data_pos += BLOCK_SIZE;
            }
//            contents.assign(data, inode->size);
//            delete[] data;
            contents = contents.substr(0, inode->size);
            std::ofstream file(name, std::ios::binary);
            if (!file.is_open()) return ErrorCode::FAILURE;
            file << contents;
            file.close();
            return ErrorCode::SUCCESS;
        }
    }
    return ErrorCode::FILE_NOT_FOUND;
}

// 将给定路径分割为目录和文件名，返回一个pair，包含目录和文件名
std::pair<std::string, std::string> Filesystem::split_path_and_name(std::string path) {
    // 如果路径为空，返回上级目录和当前目录
    if (path.empty()) { return {"..", "."}; }

    // 如果路径末尾为'/'，去除该字符
    if (path.back() == '/') { path.pop_back(); }

    // 如果路径为空，返回根目录和当前目录
    if (path.empty()) { return {"/", "."}; }

    // 寻找最后一个'/'的位置
    auto last_slash_pos = path.find_last_of('/');

    // 如果未找到'/'，说明路径为文件名，返回当前目录和文件名
    if (last_slash_pos == std::string::npos) {
        return {".", path};
    } else {
        // 否则，分割路径为目录和文件名
        std::string directory = path.substr(0, last_slash_pos);
        if (directory.empty()) directory = "/"; // 如果目录为空，表示为根目录
        std::string filename = path.substr(last_slash_pos + 1);
        return {directory, filename};
    }
}


// 获取给定路径的Entry，返回错误码和Entry指针的pair
std::pair<ErrorCode, Entry*> Filesystem::get_path_entry(std::string path) {
    // 如果路径为空，返回当前目录的Entry的克隆
    if (path.empty()) {
        return {ErrorCode::SUCCESS, Entry::clone(pid_map[current_shell_pid].current_entry.elem())};
    }

    // 如果路径以'~'开头，将其替换为"/home"
    if (path[0] == '~') path = "/home" + path.substr(1);

    Entry* res;

    // 如果路径以'/'开头，从根目录开始搜索
    if (path[0] == '/') {
        res = Entry::clone(pid_map[current_shell_pid].root_entry.elem());
    } else {
        // 否则从当前目录开始搜索
        res = Entry::clone(pid_map[current_shell_pid].current_entry.elem());
    }

    // 将路径分割为各个子路径
    std::vector<std::string> paths = split_path(path);

    // 逐级查找路径对应的Entry
    for (const auto& subpath: paths) {
        // 如果当前Entry无效，返回文件未找到的错误码
        if (!res->is_valid) return {ErrorCode::FILE_NOT_FOUND, nullptr};

        // 获取当前Entry对应的Inode
        Inode* inode = get_inode(res->inode_id);

        // 如果Inode无效，返回文件未找到的错误码
        if (inode == nullptr || !inode->is_valid) return {ErrorCode::FILE_NOT_FOUND, nullptr};

        // 获取Inode对应的数据块
        AutoBlock block(0, inode);

        // 如果数据块无效，返回文件未找到的错误码
        if (block == nullptr) return {ErrorCode::FILE_NOT_FOUND, nullptr};

        bool state = false;

        // 遍历数据块中的所有Entry
        for (auto& entry: block.elem()->entries) {
            // 如果Entry有效且与子路径名匹配，更新当前Entry
            if (entry.is_valid && strcmp(entry.name, subpath.c_str()) == 0) {
                delete res;
                res = Entry::clone(&entry);
                state = true;
                break;
            }
        }

        // 如果未找到匹配的Entry，返回文件未找到的错误码
        if (!state) {
            return {ErrorCode::FILE_NOT_FOUND, nullptr};
        }
    }

    // 返回成功，及对应路径的Entry指针
    return {ErrorCode::SUCCESS, res};
}

// 获取指定Inode编号对应的Inode指针
Inode* Filesystem::get_inode(uint32_t i) {
    if (i == null) return nullptr;
    uint32_t inodeIndex = i / INODES_PER_BLOCK;
    uint32_t inodeOffset = i % INODES_PER_BLOCK;
    return &inodes_table->inodes_table[inodeIndex]->inodes[inodeOffset];
}

// 保存指定Inode编号对应的Inode
void Filesystem::save_inode(uint32_t i) {
    // TODO: 添加保存Inode的实现
    if (i == null) return;
    inodes_table->save();
}

// 删除指定Inode编号对应的Inode
void Filesystem::delete_inode(uint32_t i) {
    if (i == null) return;
    inodes_bitmap->_delete(i);
    inodes_bitmap->save();
    uint32_t inodeIndex = i / INODES_PER_BLOCK;
    uint32_t inodeOffset = i % INODES_PER_BLOCK;
    inodes_table->inodes_table[inodeIndex]->inodes[inodeOffset].is_valid = false;
}


void Filesystem::new_shell() {
    Info info;
    strcpy(info.username, "root");
    pid_map[current_shell_pid] = info;
    pid_map[current_shell_pid].last_entry.set(nullptr);
    pid_map[current_shell_pid].current_entry.set(Entry::clone(&root->entries[0]));
    pid_map[current_shell_pid].root_entry.set(Entry::clone(&root->entries[0]));
}

ErrorCode Filesystem::ls(const std::string &path, bool with_args, const char *user) {
    AutoEntry entry;
    if (path.empty()) {
        entry.set(Entry::clone(pid_map[current_shell_pid].current_entry.elem()));
    } else {
        entry.set(get_path_entry(path).second);
        if (entry == nullptr) {
            response << "ls: cannot access '" << path << "': No such file or directory" << '\n';
            return ErrorCode::FAILURE;
        }
    }
    if (get_inode(entry.elem()->inode_id)->type == 'f') {
        response << entry.elem()->name << '\n';
        return ErrorCode::SUCCESS;
    }
    bool state = false;
    Inode* inode = get_inode(entry.elem()->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
    ErrorCode err = check_entry(entry.elem(), user, Option::READ);
    if (err == ErrorCode::FAILURE) {
        response << "ls: Permission denied" << '\n';
        return ErrorCode::FAILURE;
    }
    AutoBlock block(0, inode);
    for (const auto& file: block.elem()->entries) {
        if (with_args) {
            if (file.is_valid && get_inode(file.inode_id)->type == 'd') {
                if (file.name[0] != '.') {
                    response << BLUE << file.name << "    ";
                    state = true;
                }
            }
        } else {
            if (file.is_valid) {
                if (file.name[0] != '.') {
                    if (get_inode(file.inode_id)->type == 'd') response << BLUE << file.name << "    ";
                    else response << WHITE << file.name << "    ";
                    state = true;
                }
            }
        }
    }
    response << WHITE;
    if (state) response << '\n';
    return ErrorCode::SUCCESS;
}

ErrorCode Filesystem::ll(const std::string &path, bool with_args, const char* user) {
    AutoEntry entry;
    if (path.empty()) {
        entry.set(Entry::clone(pid_map[current_shell_pid].current_entry.elem()));
    } else {
        entry.set(get_path_entry(path).second);
        if (entry == nullptr) {
            response << "ll: cannot access '" << path << "': No such file or directory" << '\n';
            return ErrorCode::FAILURE;
        }
    }
    Inode* inode = get_inode(entry.elem()->inode_id);
    if (inode == nullptr || !inode->is_valid) return ErrorCode::FAILURE;
    if (inode->type == 'f') {
        response << std::left << std::setw(10) << "Permission";
        response << std::left << std::setw(11) << "      Owner";
        response << std::left << std::setw(11) << "      Group";
        response << std::left << std::setw(11) << "    Address";
        response << std::left << std::setw(10) << "      Size";
        response << std::left << std::setw(10) << "  Capacity";
        response << std::left << "  File\n";
        response << "---------------------------------------------------------------------\n";
        response << std::right << '-' << std::setw(9) << to_string(inode->mode);
        response << std::right << std::setw(11) << inode->owner;
        response << std::right << std::setw(11) << inode->owner;
        response << "  ";
        response << std::right << "0x" << std::hex << std::setw(7) << std::setfill('0') << inode->i_block[0] * 1024;
        response << std::dec;
        response << std::setfill(' ');
        if (inode->size < 1024) {
            response << std::right << std::setw(9) << inode->size << "B";
        } else if (inode->size < 1024 * 1024) {
            response << std::right << std::setw(9) << std::fixed << std::setprecision(2) << inode->size / 1024. << "K";
        } else {
            response << std::right << std::setw(9) << std::fixed << std::setprecision(2) << inode->size / (1024. * 1024) << "M";
        }
        if (inode->capacity < 1024 * 1024) response << std::right << std::setw(9) << inode->capacity / 1024 << "K";
        else response << std::right << std::setw(9) << std::fixed << std::setprecision(2) << inode->capacity / (1024. * 1024) << "M";
        response << "  " << std::left << entry.elem()->name << '\n';
        response << WHITE;
        response << "---------------------------------------------------------------------\n";
        return ErrorCode::SUCCESS;
    }
    ErrorCode err = check_entry(entry.elem(), user, Option::READ);
    if (err == ErrorCode::FAILURE) {
        response << "ll: Permission denied" << '\n';
        return ErrorCode::FAILURE;
    }
    response << std::left << std::setw(10) << "Permission";
    response << std::left << std::setw(11) << "      Owner";
    response << std::left << std::setw(11) << "      Group";
    response << std::left << std::setw(11) << "    Address";
    response << std::left << std::setw(10) << "      Size";
    response << std::left << std::setw(10) << "  Capacity";
    response << std::left << "  File\n";
    response << "---------------------------------------------------------------------\n";
    AutoBlock block(0, inode);
    for (const auto& file: block.elem()->entries) {
        if (file.is_valid) {
            Inode* inode = get_inode(file.inode_id);
            if (inode->type == 'd') {
                response << 'd';
            } else {
                response << '-';
            }
            response << std::right << std::setw(9) << to_string(inode->mode);
            response << std::right << std::setw(11) << inode->owner;
            response << std::right << std::setw(11) << inode->owner;
            response << "  ";
            response << std::right << "0x" << std::hex << std::setw(7) << std::setfill('0') << inode->i_block[0] * 1024;
            response << std::dec;
            response << std::setfill(' ');
            if (inode->size < 1024) {
                response << std::right << std::setw(9) << inode->size << "B";
            } else if (inode->size < 1024 * 1024) {
                response << std::right << std::setw(9) << std::fixed << std::setprecision(2) << inode->size / 1024. << "K";
            } else {
                response << std::right << std::setw(9) << std::fixed << std::setprecision(2) << inode->size / (1024. * 1024) << "M";
            }
            if (inode->capacity < 1024 * 1024) response << std::right << std::setw(9) << inode->capacity / 1024 << "K";
            else response << std::right << std::setw(9) << std::fixed << std::setprecision(2) << inode->capacity / (1024. * 1024) << "M";
            if (inode->type == 'd') {
                response << BLUE;
            } else if (inode->type == 'x') {
                response << GREEN;
            } else {
                response << WHITE;
            }
            response << "  " << std::left << file.name << '\n';
            response << WHITE;
        }
    }
    response << "---------------------------------------------------------------------\n";
    return ErrorCode::SUCCESS;
}
// 删除目录或文件对应的Entry及其关联的Inode和数据块
ErrorCode delete_entry(Entry* current) {
    // 获取Entry对应的Inode
    Inode* inode = Filesystem::get_inode(current->inode_id);

    // 如果是文件类型
    if (inode->type == 'f') {
        // 获取文件的所有数据块
        std::vector<uint32_t> blocks = get_blocks(inode);

        // 删除文件的所有数据块
        for (uint32_t i = 0; i < blocks.size(); ++i) {
            Filesystem::delete_block(blocks[i]);
        }

        // 删除文件的索引节点的间接索引块
        for (uint32_t i = 6; i < 9; ++i) {
            Filesystem::delete_block(inode->i_block[i]);
        }

        // 删除文件的索引节点
        Filesystem::delete_inode(current->inode_id);
        Filesystem::save_inode(current->inode_id);

        // 将Entry标记为无效
        current->is_valid = false;

        // 返回成功
        return ErrorCode::SUCCESS;
    }

    // 如果是目录类型
    Filesystem::AutoBlock block(0, inode);

    // 递归删除目录下的所有文件和子目录
    for (auto& entry : block.elem()->entries) {
        if (entry.is_valid && Filesystem::get_inode(entry.inode_id)->type == 'f') {
            delete_entry(&entry);
            block.save();
        }
    }

    // 删除目录下的所有子目录
    for (auto& entry : block.elem()->entries) {
        if (entry.is_valid && strcmp(entry.name, ".") != 0 && strcmp(entry.name, "..") != 0) {
            delete_entry(&entry);
            block.save();
        }
    }

    // 删除目录的数据块和索引节点
    Filesystem::delete_block(inode->i_block[0]);
    Filesystem::delete_inode(current->inode_id);
    Filesystem::save_inode(current->inode_id);

    // 将Entry标记为无效
    current->is_valid = false;

    // 返回成功
    return ErrorCode::SUCCESS;
}

// 删除目录，包括递归删除其下的所有文件和子目录
ErrorCode Filesystem::delete_directory(Entry* parent, const char* name, Option option, const char* user) {
    // 检查父目录是否有效
    if (!parent->is_valid) {
        return ErrorCode::FAILURE;
    }

    // 获取父目录的Inode
    Inode* parent_inode = get_inode(parent->inode_id);

    // 如果获取Inode失败或父目录Inode无效，返回失败
    if (parent_inode == nullptr || !parent_inode->is_valid) {
        return ErrorCode::FAILURE;
    }

    // 创建AutoBlock对象，用于访问父目录的数据块
    AutoBlock block(0, parent_inode);

    // 如果创建AutoBlock失败，返回失败
    if (block == nullptr) {
        return ErrorCode::FAILURE;
    }

    // 遍历父目录的所有Entry
    for (auto& entry : block.elem()->entries) {
        // 如果Entry有效且名称匹配
        if (entry.is_valid && strcmp(entry.name, name) == 0) {
            // 获取对应Inode
            Inode* inode = get_inode(entry.inode_id);

            // 如果是文件而非目录，返回文件不匹配错误
            if (inode->type == 'f') {
                return ErrorCode::FILE_NOT_MATCH;
            }

            // 如果不是RESPONSE操作，检查目录是否为空
            if (option != Option::RESPONSE) {
                if (inode->size > 2 * sizeof(Entry)) {
                    return ErrorCode::WAIT_REQUEST;
                }
            }

            // 删除Entry，更新父目录Inode的大小，保存Inode和数据块
            delete_entry(&entry);
            parent_inode->size -= sizeof(Entry);
            save_inode(parent->inode_id);
            block.save();

            // 返回成功
            return ErrorCode::SUCCESS;
        }
    }

    // 如果未找到匹配的Entry，返回文件未找到错误
    return ErrorCode::FILE_NOT_FOUND;
}

// 新建磁盘文件
ErrorCode Disk::new_disk() {
    // 以二进制、读写方式创建磁盘文件
    std::fstream disk_file(disk_name, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);

    // 如果文件打开失败，返回失败
    if (!disk_file) {
        return ErrorCode::FAILURE;
    }

    // 创建数据缓冲区
    char* data_buffer = new char[BLOCK_SIZE];

    // 循环写入数据块
    for (int i = 0; i < BLOCKS_NUM; ++i) {
        disk_file.seekp(i * BLOCK_SIZE);
        disk_file.write(reinterpret_cast<const char*>(data_buffer), BLOCK_SIZE);
    }

    // 关闭文件，释放内存，返回成功
    disk_file.close();
    delete[] data_buffer;
    return ErrorCode::SUCCESS;
}
void Disk::load_disk() {

}

// 从磁盘读取指定块号的数据块
Block* Disk::read_block(uint32_t block_num) {
    // 以二进制、读写方式打开磁盘文件
    std::fstream disk_file(disk_name, std::ios::binary | std::ios::in | std::ios::out);

    // 如果文件未成功打开，返回空指针
    if (!disk_file.is_open()) {
        return nullptr;
    }

    // 创建一个字符数组来存储读取的块数据
    char* block = new char[BLOCK_SIZE];

    // 定位到指定块号的位置
    disk_file.seekg(block_num * BLOCK_SIZE);

    // 读取块数据到字符数组
    disk_file.read(block, BLOCK_SIZE);

    // 关闭文件
    disk_file.close();

    // 将字符数组的地址转换为块对象的指针并返回
    return reinterpret_cast<Block*>(block);
}

// 将数据块写入磁盘的指定块号位置
void Disk::write_block(uint32_t block_num, const Block* block) {
    // 以二进制、读写方式打开磁盘文件
    std::fstream disk_file(disk_name, std::ios::binary | std::ios::in | std::ios::out);

    // 定位到指定块号的位置
    disk_file.seekp(block_num * BLOCK_SIZE);

    // 将块对象的内容以字符数组形式写入文件
    disk_file.write(reinterpret_cast<const char*>(block), BLOCK_SIZE);

    // 关闭文件
    disk_file.close();
}
