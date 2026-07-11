// Hello World - acornlib smoke test

#include <stdio.h>
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