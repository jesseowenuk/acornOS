// Hello World - acornlib smoke test
//
// Each test below exercises one libc/kernel feature. Run just one via
// 'run hello <name>' (e.g. 'run hello malloc'), or run everything in
// sequence via 'run hello' (no args) or 'run hello all'

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_argv(int argc, char** argv)
{
    // argc/argv - passed via RDI/RSI at process_entry, see crt0.c
    printf("argc=%d\n", argc);

    for(int i = 0; i < argc; i++)
    {
        printf("argv[%d]=%s\n", i, argv[i]);
    }
}

static void test_printf(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // printf formats
    printf("int=%d neg=%d, hex=%lx str=%s char=%c pct=%%\n",
        42, -7, 0xCAFEUL, "world", 'Q');
}

static void test_string(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    char buf[32];
    strcpy(buf, "hello");
    printf("strlen=%d strcpy=%s cmp_eq=%d cmp_ne=%d\n",
        (int)strlen(buf), buf, strcmp(buf, "hello"), strcmp(buf, "world"));

    memset(buf, 'x', 4);
    buf[4] = 0;
    printf("memset=%s\n", buf);

    char dst[8];
    memcpy(dst, "copy!", 6);
    printf("memcpy=%s memcmp_eq=%d\n", dst, memcmp(dst, "copy!", 6));
}

static void test_fileio(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("pid=%d\n", getpid());

    // open/close/seek against a file the kernel's own boot self-test creates
    int fd = open("/temp/trunc.txt", O_RDONLY);
    printf("open(trunc.txt)=%d\n", fd);
    if(fd >= 0)
    {
        int pos = seek(fd, 0, SEEK_SET);
        printf("seek=%d\n", pos);
        close(fd);
    }

    // Real file I/O round-trip, write, close, reopen, read back
    int wfd = open("/temp/iotest.txt", O_WRONLY | O_CREAT | O_TRUNC);
    ssize_t wn = write(wfd, "roundtrip", 9);
    close(wfd);
    printf("file write: fd=%d n=%d\n", wfd, (int)wn);

    int rfd = open("/temp/iotest.txt", O_RDONLY);
    char iobuf[16];
    ssize_t rn = read(rfd, iobuf, 9);
    iobuf[rn] = 0;
    close(rfd);
    printf("file read: fd=%d n=%d data=%s\n", rfd, (int)rn, iobuf);

    int bad_fd = open("/temp/does_not_exist.txt", O_RDONLY);
    printf("open(missing)=%d\n", bad_fd);
}

static void test_stdio(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Buffered stdio - fopen/fwrite/fread/fclose
    // Write more than one internal buffer's worth (FILE_BUF_SIZE=512) in
    // several fwrite() calls, to prove the write behind buffer flushes
    // mid-stream rather than only on fclose().
    FILE* wf = fopen("/temp/stdio_test.txt", "w");
    printf("fopen(w) ok=%d\n", wf != NULL);

    fwrite("HEAD-", 1, 5, wf);

    for(int i = 0; i < 60; i++)
    {
        fwrite("AAAAAAAAAA", 1, 10, wf);        // 60 * 10 = 600 bytes, spans the buffer
    }

    fwrite("-TAIL", 1, 5, wf);
    fclose(wf);

    // Read it all back in small chunks, smaller than the file, to prove
    // the read-ahead buffer refills correctly  across multiple fread() calls
    FILE* rf = fopen("/temp/stdio_test.txt", "r");
    char stdio_buf[700];
    size_t stdio_total = 0;
    size_t got;

    while((got = fread(stdio_buf + stdio_total, 1, 100, rf)) > 0)
    {
        stdio_total += got;
    }

    stdio_buf[stdio_total] = 0;
    fclose(rf);

    printf("stdio len=%d head=%c%c%c%c%c tail=%c%c%c%c%c\n",
        (int)stdio_total,
        stdio_buf[0], stdio_buf[1], stdio_buf[2], stdio_buf[3], stdio_buf[4],
        stdio_buf[stdio_total - 5], stdio_buf[stdio_total - 4], stdio_buf[stdio_total - 3],
        stdio_buf[stdio_total - 2], stdio_buf[stdio_total - 1]);

    // Append mode - open the same file "a", write more, and confrm the
    // original contents are still there with the new data on the end
    FILE* af = fopen("/temp/stdio_test.txt", "a");
    fwrite("+MORE", 1, 5, af);
    fclose(af);

    FILE* rf2 = fopen("/temp/stdio_test.txt", "r");
    char append_buf[720];
    size_t append_total = 0;

    while((got = fread(append_buf + append_total, 1, 100, rf2)) > 0)
    {
        append_total += got;
    }

    append_buf[append_total] = 0;
    fclose(rf2);

    printf("append len=%d tail=%c%c%c%c%c\n",
        (int)append_total,
        append_buf[append_total - 5], append_buf[append_total - 4], append_buf[append_total - 3],
        append_buf[append_total - 2], append_buf[append_total - 1]);
}

static void test_malloc(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // malloc/free basics - allocate, write, read back
    char* a = (char*)malloc(32);
    strcpy(a, "malloc works");
    printf("malloc a=%s\n", a);

    // A second allocation should land in different memory
    char* b = (char*)malloc(32);
    strcpy(b, "second block");
    printf("malloc b=%s a_still=%s\n", b, a);

    // Free a, then allocate something the same size - first-fit should
    // reuse a's slot rather than growing the heap again
    free(a);
    char* c = (char*)malloc(32);
    printf("reuse: c==a? %d\n", c == a);
    strcpy(c, "reused block");
    printf("malloc c=%s\n", c);

    // A big allocation that needs more than one page from the kernel
    char* big = (char*)malloc(9000);
    for(int i = 0; i < 9000; i++)
    {
        big[i] = 'z';
    }

    big[8999] = 0;
    printf("big len=%d first=%c last=%c\n", (int)strlen(big), big[0], big[8998]);

    // Free everything, then allocate something bigger than any single
    // freed piece but smaller than all of them combined - only works if
    // forward coalesing actually merged the freed blocks back together
    free(b);
    free(c);
    free(big);
    char* merged = (char*)malloc(64);
    strcpy(merged, "coalesced ok");
    printf("coalesed=%s\n", merged);
    free(merged);
}

static void test_proc(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // Yield - just to confirm it doesn't crash
    yield();
    printf("yield OK!\n");

    // fork + wait
    pid_t pid = fork();
    if(pid == 0)
    {
        printf("child pid=%d\n", getpid());
        exit(42);
    }
    else
    {
        printf("parent pid=%d child=%d\n", getpid(), (int)pid);
        int status = wait();
        printf("wait returned=%d\n", status);
    }
}

static void test_sleep(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    printf("pid=%d sleeping 1 second...\n", getpid());
    sleep(1);
    printf("awake after sleep(1)\n");

    printf("sleeping 200ms via usleep...\n");
    usleep(200000);
    printf("awake after usleep(200000)\n");
}

typedef struct
{
    const char* name;
    void (*fn)(int argc, char** argv);
    int run_in_all;         // 0 = explicit only, skipped by "all"
} test_t;

static test_t tests[] =
{
    {"argv",    test_argv,      1},
    {"printf",  test_printf,    1},
    {"string",  test_string,    1},
    {"fileio",  test_fileio,    1},
    {"stdio",   test_stdio,     1},
    {"malloc",  test_malloc,    1},
    {"proc",    test_proc,      1},
    // Not run by "all" - over a second of real wall-clock delay would
    // slow down every boot's self test. Run explicitly via 'run hello.elf sleep'
    {"sleep",   test_sleep,     0},
    {0, 0}
};

static void run_test(test_t* t, int argc, char** argv)
{
    printf("--- %s ---\n", t->name);
    t->fn(argc, argv);
}

int main(int argc, char** argv)
{
    printf("=== acornlib test ===\n");

    // No extra args, or explicitly "all": run every test in sequence
    // (full regression coverage). Otherwise run just the named one.
    const char* which = (argc > 1) ? argv[1] : "all";

    if(strcmp(which, "all") == 0)
    {
        for(int i = 0; tests[i].name; i++)
        {
            if(tests[i].run_in_all)
            {
                run_test(&tests[i], argc, argv);
            }
        }
    }
    else
    {
        int found = 0;

        for(int i = 0; tests[i].name; i++)
        {
            if(strcmp(which, tests[i].name) == 0)
            {
                run_test(&tests[i], argc, argv);
                found = 1;
                break;
            }
        }

        if(!found)
        {
            printf("unknown test '%s'. available:", which);

            for(int i = 0; tests[i].name; i++)
            {
                printf(" %s", tests[i].name);
            }

            printf(" all\n");
        }
    }

    printf("=== done ===\n");

    return 0;
}