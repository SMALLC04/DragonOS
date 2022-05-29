#include "cmd.h"
#include <libc/string.h>
#include <libc/stdio.h>
#include <libc/stddef.h>
#include <libsystem/syscall.h>
#include <libc/string.h>
#include <libc/errno.h>
#include <libc/unistd.h>
#include <libc/stdlib.h>

#include <libc/dirent.h>

#include "cmd_help.h"

// 当前工作目录（在main_loop中初始化）
char *shell_current_path = NULL;
/**
 * @brief shell 内建函数的主命令与处理函数的映射表
 *
 */
struct built_in_cmd_t shell_cmds[] =
    {
        {"cd", shell_cmd_cd},
        {"cat", shell_cmd_cat},
        {"exec", shell_cmd_exec},
        {"ls", shell_cmd_ls},
        {"mkdir", shell_cmd_mkdir},
        {"pwd", shell_cmd_pwd},
        {"rm", shell_cmd_rm},
        {"rmdir", shell_cmd_rmdir},
        {"reboot", shell_cmd_reboot},
        {"touch", shell_cmd_touch},
        {"help", shell_help},

};
// 总共的内建命令数量
const static int total_built_in_cmd_num = sizeof(shell_cmds) / sizeof(struct built_in_cmd_t);

/**
 * @brief 寻找对应的主命令编号
 *
 * @param main_cmd 主命令
 * @return int 成功：主命令编号
 *              失败： -1
 */
int shell_find_cmd(char *main_cmd)
{

    for (int i = 0; i < total_built_in_cmd_num; ++i)
    {
        if (strcmp(main_cmd, shell_cmds[i].name) == 0) // 找到对应的命令号
            return i;
    }
    // 找不到该命令
    return -1;
}

/**
 * @brief 运行shell内建的命令
 *
 * @param index 主命令编号
 * @param argc 参数数量
 * @param argv 参数列表
 */
void shell_run_built_in_command(int index, int argc, char **argv)
{
    if (index >= total_built_in_cmd_num)
        return;
    // printf("run built-in command : %s\n", shell_cmds[index].name);

    shell_cmds[index].func(argc, argv);
}

/**
 * @brief cd命令:进入文件夹
 *
 * @param argc
 * @param argv
 * @return int
 */

int shell_cmd_cd(int argc, char **argv)
{

    int current_dir_len = strlen(shell_current_path);
    if (argc < 2)
    {
        shell_help_cd();
        goto done;
    }
    // 进入当前文件夹
    if (!strcmp(".", argv[1]))
        goto done;

    // 进入父目录
    if (!strcmp("..", argv[1]))
    {

        // 当前已经是根目录
        if (!strcmp("/", shell_current_path))
            goto done;

        // 返回到父目录
        int index = current_dir_len - 1;
        for (; index > 1; --index)
        {
            if (shell_current_path[index] == '/')
                break;
        }
        shell_current_path[index] = '\0';

        // printf("switch to \" %s \"\n", shell_current_path);
        goto done;
    }

    int dest_len = strlen(argv[1]);
    // 路径过长
    if (dest_len >= SHELL_CWD_MAX_SIZE - 1)
    {
        printf("ERROR: Path too long!\n");
        goto fail;
    }

    if (argv[1][0] == '/')
    {
        // ======进入绝对路径=====
        int ec = chdir(argv[1]);
        if (ec == -1)
            ec = errno;
        if (ec == 0)
        {
            // 获取新的路径字符串
            char *new_path = (char *)malloc(dest_len + 2);
            memset(new_path, 0, dest_len + 2);
            strncpy(new_path, argv[1], dest_len);

            // 释放原有的路径字符串的内存空间
            free(shell_current_path);

            shell_current_path = new_path;

            shell_current_path[dest_len] = '\0';
            return 0;
        }
        else
            goto fail;
        ; // 出错则直接忽略
    }
    else
    {
        int dest_offset = 0;
        if (dest_len > 2)
        {
            if (argv[1][0] == '.' && argv[1][1] == '/') // 相对路径
                dest_offset = 2;
        }

        int new_len = current_dir_len + dest_len - dest_offset;
        // ======进入相对路径=====
        if (new_len >= SHELL_CWD_MAX_SIZE - 1)
        {
            printf("ERROR: Path too long!\n");
            goto fail;
        }

        // 拼接出新的字符串
        char *new_path = (char *)malloc(new_len + 2);
        memset(new_path, 0, sizeof(new_path));
        strncpy(new_path, shell_current_path, current_dir_len);

        if (current_dir_len > 1)
            new_path[current_dir_len] = '/';
        strcat(new_path, argv[1] + dest_offset);

        if (chdir(new_path) == 0) // 成功切换目录
        {
            free(shell_current_path);
            new_path[new_len] = '\0';
            shell_current_path = new_path;
            goto done;
        }
        else
        {
            printf("ERROR: Cannot switch to directory: %s\n", new_path);
            goto fail;
        }
    }

fail:;
done:;
    // 释放参数所占的内存
    free(argv);
    return 0;
}

/**
 * @brief 查看文件夹下的文件列表
 *
 * @param argc
 * @param argv
 * @return int
 */
// todo:
int shell_cmd_ls(int argc, char **argv)
{
    struct DIR *dir = opendir(shell_current_path);
    
    if (dir == NULL)
        return -1;

    struct dirent *buf = NULL;
    // printf("dir=%#018lx\n", dir);

    while (1)
    {
        buf = readdir(dir);
        if(buf == NULL)
            break;
        
        int color = COLOR_WHITE;
        if(buf->d_type & VFS_ATTR_DIR)
            color = COLOR_YELLOW;
        else if(buf->d_type & VFS_ATTR_FILE)
            color = COLOR_INDIGO;
        
        char output_buf[256] = {0};

        sprintf(output_buf, "%s   ", buf->d_name);
        put_string(output_buf, color, COLOR_BLACK);
        
    }
    printf("\n");
    closedir(dir);
    return 0;
}

/**
 * @brief 显示当前工作目录的命令
 *
 * @param argc
 * @param argv
 * @return int
 */
int shell_cmd_pwd(int argc, char **argv)
{
    if (shell_current_path)
        printf("%s\n", shell_current_path);

    free(argv);
}

/**
 * @brief 查看文件内容的命令
 *
 * @param argc
 * @param argv
 * @return int
 */
// todo:
int shell_cmd_cat(int argc, char **argv) {}

/**
 * @brief 创建空文件的命令
 *
 * @param argc
 * @param argv
 * @return int
 */
// todo:
int shell_cmd_touch(int argc, char **argv) {}

/**
 * @brief 删除命令
 *
 * @param argc
 * @param argv
 * @return int
 */
// todo:
int shell_cmd_rm(int argc, char **argv) {}

/**
 * @brief 创建文件夹的命令
 *
 * @param argc
 * @param argv
 * @return int
 */
// todo:
int shell_cmd_mkdir(int argc, char **argv) {}

/**
 * @brief 删除文件夹的命令
 *
 * @param argc
 * @param argv
 * @return int
 */
// todo:
int shell_cmd_rmdir(int argc, char **argv) {}

/**
 * @brief 执行新的程序的命令
 *
 * @param argc
 * @param argv
 * @return int
 */

// todo:
int shell_cmd_exec(int argc, char **argv) {}

/**
 * @brief 重启命令
 *
 * @param argc
 * @param argv
 * @return int
 */
// todo:
int shell_cmd_reboot(int argc, char **argv)
{
    return syscall_invoke(SYS_REBOOT, 0, 0, 0, 0, 0, 0, 0, 0);
}