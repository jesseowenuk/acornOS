// Hello World - acornlib smoke test

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    printf("=== acornlib test ===\n");

    // printf formats
    printf("int=%d neg=%d, hex=%lx str=%s char=%c pct=%%\n",
        42, -7, 0xCAFEUL, "world", 'Q');

    // string.h
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

    // Process info
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

    printf("=== done ===\n");

    return 0;
}