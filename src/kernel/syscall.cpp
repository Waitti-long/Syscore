#include "syscall.h"
#include "register.h"
#include "../lib/stl/stl.h"
#include "scheduler.h"
#include "posix/posix_structs.h"
#include "fs/file_describer.h"
#include "times.h"

#define get_actual_page(x) (((x)>0x80000000)?(x):(x)+ get_running_elf_page())
#define SYSCALL_LIST_LENGTH (1024)
// 该宏默认所有的路径长度都不会超过512
#define PATH_BUFF_SIZE (512)

/// lazy init syscall list
int (*syscall_list[SYSCALL_LIST_LENGTH])(Context *context);


/// functions declaration
void syscall_register();

void syscall_distribute(int syscall_id, Context *context);


/// utils functions

int sysGetRealFd(int fd) {
    if (file_describer_exists(fd)) {
        fd = (int) file_describer_convert(fd);
    }
    fd = fd_get_origin_fd(fd);
    return fd;
}

/**
 * strpbrk - Find the first occurrence of a set of characters
 * @cs: The string to be searched
 * @ct: The characters to search for
 */
char *strpbrk(const char *cs, const char *ct) {
    const char *sc1, *sc2;

    for (sc1 = cs; *sc1 != '\0'; ++sc1) {
        for (sc2 = ct; *sc2 != '\0'; ++sc2) {
            if (*sc1 == *sc2)
                return (char *) sc1;
        }
    }
    return NULL;
}

/**
 * strsep - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 *
 * strsep() updates @s to point after the token, ready for the next call.
 *
 * It returns empty tokens, too, behaving exactly like the libc function
 * of that name. In fact, it was stolen from glibc2 and de-fancy-fied.
 * Same semantics, slimmer shape. ;)
 */
char *strsep(char **s, const char *ct) {
    char *sbegin = *s;
    char *end;

    if (sbegin == NULL)
        return NULL;

    end = strpbrk(sbegin, ct);
    if (end)
        *end++ = '\0';
    *s = end;
    return sbegin;
}

void test_getAbsolutePath();

/**
 * return absolute path by path & cwd
 * @param path
 * @param cwd
 * @return if valid, return a temp address; otherwise, return NULL.
 */
char *getAbsolutePath(char *path, char *cwd) {
#define PATH_MAX_LEN 512
#define PATH_UNIT_MAX_COUNT 16
    static int getAbsolutePathIsInitialized = 0;
    if (getAbsolutePathIsInitialized == 0) {
        getAbsolutePathIsInitialized = 1;
        test_getAbsolutePath();
    }
    static char str_buff[PATH_MAX_LEN];
    static char units[PATH_UNIT_MAX_COUNT][PATH_MAX_LEN];
    static char res[PATH_UNIT_MAX_COUNT][PATH_MAX_LEN];
    memset(str_buff, 0, sizeof str_buff);
    if (cwd != NULL) {
        // if path start with /, it means absolute path
        if (!(strlen(path) >= 1 && path[0] == '/')) {
            strcpy(str_buff, cwd);
            // if cwd don't end with /, just add /
            if (strlen(cwd) == 0 || cwd[strlen(cwd) - 1] != '/') {
                strcpy(str_buff + strlen(str_buff), "/");
            }
        }
    }

    if (path == NULL) {
        if (cwd == NULL)return NULL;
    } else {
        strcpy(str_buff + strlen(str_buff), path);
    }

    int cur_unit = 0;
    char *buf_pointer = str_buff;
    char *unit;
    while ((unit = strsep(&buf_pointer, "/")) != NULL) {
        if (*unit != '\0') {
            strcpy(units[cur_unit++], unit);
        }
    }

    int unit_count = cur_unit;
    cur_unit = 0;

    int cur = 0;
    while (cur_unit < unit_count) {
        if (strcmp(units[cur_unit], ".") == 0) {
            // do nothing
        } else if (strcmp(units[cur_unit], "..") == 0) {
            cur--;
            if (cur < 0) {
                return NULL;
            }
        } else {
            strcpy(res[cur], units[cur_unit]);
            cur++;
            if (cur >= PATH_UNIT_MAX_COUNT) {
                return NULL;
            }
        }
        cur_unit++;
    }

    for (int i = 0; i < cur; i++) {
        strcpy(str_buff + strlen(str_buff), "/");
        strcpy(str_buff + strlen(str_buff), res[i]);
    }

    // if root, should return /
    if (cur == 0) {
        strcpy(str_buff, "/");
    }
    return str_buff;
#undef PATH_MAX_LEN
#undef PATH_UNIT_MAX_COUNT
}

void test_getAbsolutePath() {
    // positive test
    assert(strcmp(getAbsolutePath(".//mnt/test/../", "/"), "/mnt") == 0);
    assert(strcmp(getAbsolutePath("/", NULL), "/") == 0);
    assert(strcmp(getAbsolutePath(".", NULL), "/") == 0);
    assert(strcmp(getAbsolutePath("../", "/mnt/"), "/") == 0);
    assert(strcmp(getAbsolutePath("../", "/mnt"), "/") == 0);
    assert(strcmp(getAbsolutePath(".", "/"), "/") == 0);
    assert(strcmp(getAbsolutePath("/test", NULL), "/test") == 0);
    assert(strcmp(getAbsolutePath("./test", "/"), "/test") == 0);
    assert(strcmp(getAbsolutePath("././", NULL), "/") == 0);
    assert(strcmp(getAbsolutePath(".", "/mnt/"), "/mnt") == 0);
    assert(strcmp(getAbsolutePath("./", "/mnt/"), "/mnt") == 0);
    assert(strcmp(getAbsolutePath(".", "/mnt"), "/mnt") == 0);
    assert(strcmp(getAbsolutePath("./", "/mnt"), "/mnt") == 0);
    assert(strcmp(getAbsolutePath("/mnt/test_mount", NULL), "/mnt/test_mount") == 0);
    assert(strcmp(getAbsolutePath("/mnt/test_mount", "/"), "/mnt/test_mount") == 0);
    assert(strcmp(getAbsolutePath(NULL, "/"), "/") == 0);

    // negative test
    assert(getAbsolutePath("../../", "/") == NULL);
    assert(getAbsolutePath(NULL, NULL) == NULL);
}

char *atFdCWD(int dir_fd, char *path) {
    static char buff[PATH_BUFF_SIZE];
    if (dir_fd == AT_FDCWD) {
        strcpy(buff, getAbsolutePath(path, get_running_cwd()));
    } else {
        strcpy(buff, getAbsolutePath(path, File_Describer_Get_Path(dir_fd)));
    }
    return buff;
}

/// syscall

int sys_getchar(Context *context) {
    return getchar_blocked();
}

int sys_write(Context *context) {
    // fd：要写入文件的文件描述符。
    // buf：一个缓存区，用于存放要写入的内容。
    // count：要写入的字节数。
    // ssize_t ret = write(fd, buf, count);
    // 返回值：成功执行，返回写入的字节数。错误，则返回-1。
    int fd = sysGetRealFd((int) context->a0);
    char *buf = (char *) get_actual_page(context->a1);
    int count = (int) context->a2;
    int write_bytes = fs->write(File_Describer_Get_Path(fd), buf, count);
    return write_bytes;
}

int sys_execve(Context *context) {
    execute((const char *)get_actual_page(context->a0));
    return context->a0;
}

int sys_gettimeofday(Context *context) {
    get_timespec((TimeVal*)get_actual_page(context->a0));
    return 0;
}

int sys_exit(Context *context) {
    exit_process(context->a0);
    return context->a0;
}

int sys_getppid(Context *context) {
    return get_running_ppid();
}

int sys_getpid(Context *context) {
    return get_running_pid();
}

int sys_openat(Context *context) {
    // fd：文件所在目录的文件描述符
    // filename：要打开或创建的文件名。如为绝对路径，则忽略fd。如为相对路径，且fd是AT_FDCWD，则filename是相对于当前工作目录来说的。如为相对路径，且fd是一个文件描述符，则filename是相对于fd所指向的目录来说的。
    // flags：必须包含如下访问模式的其中一种：O_RDONLY，O_WRONLY，O_RDWR。还可以包含文件创建标志和文件状态标志。
    // mode：文件的所有权描述。详见`man 7 inode `。
    // int ret = openat(fd, filename, flags, mode);
    // 返回值：成功执行，返回新的文件描述符。失败，返回-1。
    // TODO: 暂不支持文件所有权描述
    int dir_fd = sysGetRealFd(context->a0);
    char *filename = (char *) get_actual_page(context->a1);
    int flag = context->a2;
    int fd = fd_search_a_empty_file_describer();
    char* path = atFdCWD(dir_fd, filename);
    int fr = fs->open(path, flag);
    if(fr == 0){
        File_Describer_Data fakeData = {.redirect_fd = 0};
        char* fd_path = (char*)k_malloc(strlen(path) + 1);
        strcpy(fd_path, path);
        File_Describer_Create(fd, FILE_DESCRIBER_REGULAR, FILE_ACCESS_RW, fakeData, fd_path);
        return fd;
    }
    return -1;
}

int sys_read(Context *context) {
    // fd: 文件描述符，buf: 用户空间缓冲区，count：读多少
    // ssize_t ret = read(fd, buf, count)
    // ret: 返回的字节数
    int fd = sysGetRealFd(context->a0);
    char *buf = (char *) get_actual_page(context->a1);
    int count = (int)context->a2;
    return fs->read(File_Describer_Get_Path(fd), buf, count);
}

int sys_close(Context *context) {
    // fd：要关闭的文件描述符。
    // int ret = close(fd);
    // 返回值：成功执行，返回0。失败，返回-1。
    int fd = sysGetRealFd(context->a0);
    File_Describer_Reduce((int) fd);
    return (0);
}

int sys_getcwd(Context *context) {
    // char *buf：一块缓存区，用于保存当前工作目录的字符串。当buf设为NULL，由系统来分配缓存区。
    // size：buf缓存区的大小。
    // long ret = getcwd(buf, size);
    // 返回值：成功执行，则返回当前工作目录的字符串的指针。失败，则返回NULL。
    size_t buf = context->a0;
    size_t size = context->a1;
    char *ret;
    char *current_work_dir = get_running_cwd();
    if (buf == 0) {
        // 系统分配缓冲区
        ret = (char *) k_malloc(size);
        bind_kernel_heap((size_t) ret);
    } else {
        ret = (char *) get_actual_page(buf);
    }
    strcpy(ret, current_work_dir);
    // TODO: 如果用户使用虚拟地址此处会返回真实地址，这不合理
    return (int) ((size_t) ret);
}

int sys_dup(Context *context) {
    // fd：被复制的文件描述符。
    // int ret = dup(fd);
    // 返回值：成功执行，返回新的文件描述符。失败，返回-1。
    int fd = sysGetRealFd(context->a0);
    int new_fd = fd_search_a_empty_file_describer();
    file_describer_bind(new_fd, new_fd);
    File_Describer_Plus(fd);
    File_Describer_Data data = {.redirect_fd = fd};
    File_Describer_Create(new_fd, FILE_DESCRIBER_REDIRECT, FILE_ACCESS_READ, data, NULL);
    return (new_fd);
}

int sys_dup3(Context *context) {
    // old：被复制的文件描述符。
    // new：新的文件描述符。
    // int ret = dup3(old, new, 0);
    // 返回值：成功执行，返回新的文件描述符。失败，返回-1。
    size_t old_fd = sysGetRealFd(context->a0);
    size_t new_fd = context->a1;
    // TODO: dup3暂不支持flag参数
    size_t flags = context->a2;

    if (old_fd == new_fd) {
        return (-1);
    }
    int actual_fd = fd_search_a_empty_file_describer();

    File_Describer_Data data = {.redirect_fd = (int) old_fd};
    File_Describer_Create(actual_fd, FILE_DESCRIBER_REDIRECT, FILE_ACCESS_WRITE, data, NULL);
    File_Describer_Plus((int) old_fd);
    file_describer_bind(new_fd, actual_fd);

    return (int) (new_fd);
}

int sys_chdir(Context *context) {
    // path：需要切换到的目录。
    // int ret = chdir(path);
    // 返回值：成功执行，返回0。失败，返回-1。
    char *path = (char *) get_actual_page(context->a0);
    char *current_cwd = get_running_cwd();
    strcpy(current_cwd, getAbsolutePath(path, get_running_cwd()));
    return (0);
}

int sys_getdents64(Context *context) {
    // fd：所要读取目录的文件描述符。
    // buf：一个缓存区，用于保存所读取目录的信息。
    // len：buf的大小。
    // int ret = getdents64(fd, buf, len);
    // 返回值：成功执行，返回读取的字节数。当到目录结尾，则返回0。失败，则返回-1。
    size_t fd = sysGetRealFd(context->a0);
    char *buf = (char *) get_actual_page(context->a1);
    size_t len = context->a2;
    return fs->read_dir(file_describer_array[fd].path, buf, len);
}

int sys_mkdirat(Context *context) {
    // dirfd：要创建的目录所在的目录的文件描述符。
    // path：要创建的目录的名称。
    //      如果path是相对路径，则它是相对于dirfd目录而言的。
    //      如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。
    //      如果path是绝对路径，则dirfd被忽略。
    // mode：文件的所有权描述。详见`man 7 inode `。
    // int ret = mkdirat(dirfd, path, mode);
    // 返回值：成功执行，返回0。失败，返回-1。
    // TODO: mode不支持
    int dir_fd = sysGetRealFd(context->a0);
    char *path = (char *) get_actual_page(context->a1);
    int mode = (int) context->a2;
    fs->mkdir(atFdCWD(dir_fd, path), mode);
    return 0;
}

int sys_unlinkat(Context *context) {
    // dirfd：要删除的链接所在的目录。
    // path：要删除的链接的名字。如果path是相对路径，则它是相对于dirfd目录而言的。如果path是相对路径，且dirfd的值为AT_FDCWD，则它是相对于当前路径而言的。如果path是绝对路径，则dirfd被忽略。
    // flags：可设置为0或AT_REMOVEDIR。
    // int ret = unlinkat(dirfd, path, flags);
    // 返回值：成功执行，返回0。失败，返回-1。
    // TODO: 没管flag
    int dir_fd = sysGetRealFd(context->a0);
    char *path = (char *) get_actual_page(context->a1);
    // TODO: we can't really delete it
    fs->unlink(atFdCWD(dir_fd, path));
    return 0;
}

int sys_times(Context *context) {
    struct ES_tms *tms = (struct ES_tms *)get_actual_page(context->a0);
    tms->tms_utime = 1;
    tms->tms_stime = 1;
    tms->tms_cutime = 1;
    tms->tms_cstime = 1;
    return (1000);
}

int sys_uname(Context *context) {
    struct ES_utsname *required_uname = (struct ES_utsname *)get_actual_page(context->a0);
    memcpy(required_uname, &ES_uname, sizeof(ES_uname));
    return (0);
}

int sys_wait4(Context *context) {
    return (wait((int *)get_actual_page(context->a1)));
}

int sys_nanosleep(Context *context) {
    TimeVal *timeVal = (TimeVal *)get_actual_page(context->a0);
    time_seconds += timeVal->sec;
    time_macro_seconds += timeVal->usec;
    return (0);
}

int sys_clone(Context *context) {
    clone(context->a0, context->a1, context->a2);
    return context->a0;
}

int sys_sched_yield(Context *context) {
    yield();
    return (0);
}

int sys_mount(Context *context) {
    return (0);
}

int sys_umount2(Context *context) {
    return 0;
}

int sys_fstat(Context* context){
    // fd: 文件句柄；
    // kst: 接收保存文件状态的指针；
    // int ret = fstat(fd, &kst);
    // 返回值：成功返回0，失败返回-1；
    int fd = sysGetRealFd(context->a0);
    auto* stat = (kstat*) get_actual_page(context->a1);
    return fs->fstat(file_describer_array[fd].path, stat);
}

/// syscall int & register & distribute

void syscall_init() {
    for (int i = 0; i < SYSCALL_LIST_LENGTH; i++) {
        syscall_list[i] = NULL;
    }
}

void syscall_unhandled(Context *context) {
    printf("[SYSCALL] Unhandled Syscall: %d\n", context->a7);
}

void syscall_distribute(int syscall_id, Context *context) {
    static int syscall_is_initialized = 0;
    if (syscall_is_initialized != 1) {
        syscall_init();
        syscall_register();
        syscall_is_initialized = 1;
    }

    assert(syscall_id >= 0 && syscall_id < SYSCALL_LIST_LENGTH);
    assert(context != NULL);

    if (syscall_list[syscall_id] != NULL) {
        context->a0 = syscall_list[syscall_id](context);
    } else {
        syscall_unhandled(context);
    }
}

Context *syscall(Context *context) {
    syscall_distribute((int) context->a7, context);
    context->sepc += 4;
    return context;
}


void syscall_register() {
    syscall_list[SYS_getchar] = sys_getchar;
    syscall_list[SYS_write] = sys_write;
    syscall_list[SYS_execve] = sys_execve;
    syscall_list[SYS_gettimeofday] = sys_gettimeofday;
    syscall_list[SYS_exit] = sys_exit;
    syscall_list[SYS_getppid] = sys_getppid;
    syscall_list[SYS_getpid] = sys_getpid;
    syscall_list[SYS_openat] = sys_openat;
    syscall_list[SYS_read] = sys_read;
    syscall_list[SYS_close] = sys_close;
    syscall_list[SYS_getcwd] = sys_getcwd;
    syscall_list[SYS_dup] = sys_dup;
    syscall_list[SYS_dup3] = sys_dup3;
    syscall_list[SYS_chdir] = sys_chdir;
    syscall_list[SYS_getdents64] = sys_getdents64;
    syscall_list[SYS_mkdirat] = sys_mkdirat;
    syscall_list[SYS_unlinkat] = sys_unlinkat;
    syscall_list[SYS_times] = sys_times;
    syscall_list[SYS_uname] = sys_uname;
    syscall_list[SYS_wait4] = sys_wait4;
    syscall_list[SYS_nanosleep] = sys_nanosleep;
    syscall_list[SYS_clone] = sys_clone;
    syscall_list[SYS_sched_yield] = sys_sched_yield;
    syscall_list[SYS_mount] = sys_mount;
    syscall_list[SYS_umount2] = sys_umount2;
    syscall_list[SYS_fstat] = sys_fstat;
}
