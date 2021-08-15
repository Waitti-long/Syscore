#include "kernel/interrupt.h"
#include "kernel/register.h"
#include "kernel/scheduler.h"
#include "kernel/self_test.h"
#include "kernel/memory/kernel_stack.h"
#include "lib/stl/string.h"
#include "kernel/fs/IFS.h"
#include "kernel/fs/VFS.h"
#include "kernel/Test.h"
#include "lib/stl/vector.h"
#include "kernel/memory/memory.h"
#include "kernel/memory/Heap.h"
#include "kernel/fs/file_describer.h"
#include "lib/stl/Trie.h"
#include "lib/stl/PathUtil.h"
#include "lib/stl/RefCountPtr.h"
#include "lib/stl/UniquePtr.h"
#include "lib/stl/PageTableUtil.h"
#include "kernel/time/time.h"
#include "kernel/posix/pselect.h"

void vfs_init();
void fix_kernel_page_table();

extern "C" {
#include "driver/interface.h"
#include "driver/fatfs/ff.h"
#include "driver/sysctl.h"
#include "driver/sleep/sleep.h"
#include "driver/rtc/rtc.h"
#include "driver/plic/plic.h"
}

extern "C" void __cxa_pure_virtual() {
    // Do nothing or print an error message.
}

void test_lib() {
    TestString t = TestString();
    t.test();

    TestTrie testTrie;
    TestPathUtil testPathUtil;
    TestFile testFile;
}

void simple_test(){
//    add_test("/yield");
//    add_test("/fork");
//    add_test("/clone");
//    add_test("/write");
//    add_test("/uname");
//    add_test("/times");
//    add_test("/getpid");
//    add_test("/getppid");
//    add_test("/open");
//    add_test("/read");
//    add_test("/close");
//    add_test("/openat");
//    add_test("/getcwd");
//    add_test("/dup");
//    add_test("/getdents");
//    add_test("/dup2");
//    add_test("/wait");
//    add_test("/exit");
//    add_test("/execve");
//    add_test("/gettimeofday");
//    add_test("/mkdir_");
//    add_test("/chdir");
//    add_test("/waitpid");
//    add_test("/sleep");
//    add_test("/unlink");
//    add_test("/mount");
//    add_test("/umount");
//    add_test("/fstat");
//    add_test("/pipe");
}

void busybox_test(){

    // tested
//    add_test("busybox_new ash -c exit");
//    add_test("busybox_new sh -c exit");
//    add_test("busybox_new du");
//    add_test("busybox_new expr 1 + 1");
//    add_test("busybox_new false");
//    add_test("busybox_new true");
//    add_test("busybox_new clear");
//    add_test("busybox_new uname");
//    add_test("busybox_new printf abc\n");
//    add_test("busybox_new basename /aaa/bbb");
//    add_test("busybox_new dirname /aaa/bbb");
//    add_test("busybox_new echo 123");
//    add_test("busybox_new printf \"abc\n\"");
//    add_test("busybox_new echo \"#### independent command test\"");
//    add_test("busybox_new sleep 1");
//    add_test("busybox_new cal");
//    add_test("busybox_new date");
//    add_test("busybox_new kill 10");
//    add_test("busybox_new pwd");

    // TODO: misc tests
//    add_test("busybox_new df");         // too complex, skip
//    add_test("busybox_new dmesg");      // too complex, skip
//    add_test("busybox_new uptime");     // too complex, skip
//    add_test("busybox_new free");       // need /proc/meminfo
//    add_test("busybox_new ps");         // need /proc
//    add_test("busybox_new hwclock");    // need /etc/localtime

    // passed fs tests
//    add_test("busybox_new sh -c \"echo ccccccc > test.txt \"");
//    add_test("busybox_new touch test.txt");
//    add_test("busybox_new cat test.txt");
//    add_test("busybox_new cut -c 3 test.txt");

    // TODO: file system tests
//    add_test("busybox_new which ls");
//    add_test("busybox_new ls");
//    add_test("busybox_new sh -c \"echo ccccccc > test.txt \"");
//    add_test("busybox_new cat test.txt");
//    add_test("busybox_new cut -c 3 test.txt");
//    add_test("busybox_new od test.txt");
//    add_test("busybox_new head test.txt");
//    add_test("busybox_new tail test.txt");
//    add_test("busybox_new hexdump -C test.txt");
//    add_test("busybox_new md5sum test.txt");
//    add_test("busybox_new sort test.txt | ./busybox uniq");
//    add_test("busybox_new stat test.txt");
//    add_test("busybox_new strings test.txt");
//    add_test("busybox_new wc test.txt");
//    add_test("busybox_new [ -f test.txt ]");
//    add_test("busybox_new more test.txt");
//    add_test("busybox_new rm test.txt");
//    add_test("busybox_new mkdir test_dir");
//    add_test("busybox_new mv test_dir test");
//    add_test("busybox_new rmdir test");
//    add_test("busybox_new grep hello busybox_cmd.txt");
//    add_test("busybox_new cp busybox_cmd.txt busybox_cmd.bak");
//    add_test("busybox_new rm busybox_cmd.bak");
//    add_test("busybox_new find -name \"busybox_cmd.txt\"");
}

void lua_test(){
    add_test("/lua-s sin30.lua");
}

void create_XXX_testfile(){
    fs->mkdir("/var", 0, fs->root->fs);
    fs->mkdir("/var/tmp", 0, fs->root->fs);
    fs->open("/var/tmp/XXX", O_CREATE | O_RDWR);
    char buf[1024];
    memset(buf, 0, sizeof(buf));
    for (int i = 0;i < 1024 * 7; i++) {
        fs->write("/var/tmp/XXX", buf, sizeof(buf));
    }
    fs->close("/var/tmp/XXX");
    printf("Create XXX test_file successfully.\n");
}

/**
 * 此处打算通过 sret 进入u-mode
 * 中断后硬件会执行以下动作：
 * 异常指令的pc被保存在sepc中;pc被设置为stvec;
 * scause被设置为异常原因;stval被设置为出错地址或其他信息;
 * sie置零以禁止中断;之前的sie被保存到spie;
 * 之前的权限模式被保存到spp中;
 *
 * 之后的流程是我们手动完成的：保存现场;跳到中断处理函数;恢复现场;
 *
 * 所以这里需要恢复现场+将模拟硬件自动完成的动作。
 */
void init_thread() {
    printf("[OS] test library\n");
    test_lib();
    printf("[FS] fs init.\n");
    vfs_init();
    printf("[OS] Interrupt & Timer Interrupt Open.\n");
    interrupt_timer_init();
    printf("[OS] init scheduler.\n");
    init_scheduler();
    FD::InitializeFileDescriber();
    init_self_tests();
#ifdef QEMU
    create_XXX_testfile();
#endif

//    simple_test();
//    busybox_test();
//    lua_test();
add_test("/lmbench_new lat_syscall -P 1 null");
//add_test("/lmbench_new lat_syscall -P 1 read");
//add_test("/lmbench_new lat_syscall -P 1 write");
//add_test("/busybox_new mkdir -p /var/tmp");
//add_test("/busybox_new touch /var/tmp/lmbench");
//add_test("/lmbench_new lat_syscall -P 1 stat /var/tmp/lmbench");
//add_test("/lmbench_new lat_syscall -P 1 fstat /var/tmp/lmbench");
//add_test("/lmbench_new lat_syscall -P 1 open /var/tmp/lmbench");
//add_test("/lmbench_new lat_pipe -P 1");
//add_test("/lmbench_new bw_file_rd -P 1 512k io_only /var/tmp/XXX");
//add_test("/lmbench_new bw_file_rd -P 1 512k open2close /var/tmp/XXX");
//add_test("/lmbench_all bw_mmap_rd -P 1 512k mmap_only /var/tmp/XXX");

    schedule();
}

void vfs_init() {
    File::init();
    auto *ifs = new FS;
    ifs->init();
    fs = new VFS(ifs);

    auto* stdout = new StdoutFs;
    auto* zero_fs = new ZeroFs;
    auto* null_fs = new NullFs;
    auto* pipe_fs = new PipeFs;

    /** /dev **/
    fs->mkdir("/dev", 0, ifs);
    fs->mkdir("/dev/console", 0, stdout, false);
    fs->mkdir("/dev/zero", 0, zero_fs, false);
    fs->mkdir("/dev/null", 0, null_fs, false);

    /** /sys/pipe **/
    fs->mkdir("/sys", 0, ifs);
    fs->mkdir("/sys/pipe", 0, pipe_fs, false);

    /** /proc/self/exe **/
    fs->mkdir("/proc", 0, ifs);
    fs->mkdir("/proc/self", 0, ifs);
    fs->mkdir("/proc/self/exe", 0, ifs);
}

int main() {
    printf("   _____            _____               \n"
           "  / ____|          / ____|              \n"
           " | (___  _   _ ___| |     ___  _ __ ___ \n"
           "  \\___ \\| | | / __| |    / _ \\| '__/ _ \\\n"
           "  ____) | |_| \\__ \\ |___| (_) | | |  __/\n"
           " |_____/ \\__, |___/\\_____\\___/|_|  \\___|\n"
           "          __/ |                         \n"
           "         |___/                          \n");

    lty(__kernel_stack_base);
    printf("[OS] bsp init.\n");
    driver_init();
    printf("[OS] Memory Init.\n");
    init_memory();
//    init_kernel_heap();
    init_heap();
    kernelContext.kernel_satp = register_read_satp() | (8LL << 60);lty(kernelContext.kernel_satp);
    kernelContext.kernel_handle_interrupt = (size_t) handle_interrupt;
    kernelContext.kernel_restore = (size_t) __restore;

    init_thread();
    // unreachable
    panic("Unreachable code!");
    return 0;
}
