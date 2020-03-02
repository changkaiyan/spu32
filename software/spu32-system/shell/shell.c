#include "../bios/devices.h"
#include "../bios_calls/bios_calls.h"
#include <libtinyc.h>
#include <stdint.h>

char inputbuf[128];

uint32_t strlen(char* buf)
{
    uint32_t len = 0;
    while (buf[len] != 0) {
        len++;
    }
    return len;
}

void clear_buf(char* buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        buf[i] = 0;
    }
}

void read_input()
{
    inputbuf[0] = 0;

    char cwd[64];
    fs_getcwd(cwd, sizeof(cwd));

    uint8_t reprint = 1;
    uint8_t execute = 0;
    uint32_t bufidx = 0;

    while (!execute) {
        if (reprint) {
            printf("%s $ %s", cwd, inputbuf);
            reprint = 0;
        }

        char c;
        stream_read(DEVICE_STDIN, &c, 1);
        if (c == '\n' || c == '\r') {
            execute = 1;
        } else if (c == 127) {
            if (bufidx > 0) {
                inputbuf[--bufidx] = 0;
            }
            reprint = 1;
            printf("\n\r");
        } else if (c < 32) {
            // ignore other control characters
        } else if (bufidx < (sizeof(inputbuf) - 1)) {
            inputbuf[bufidx] = c;
            inputbuf[bufidx + 1] = 0;
            bufidx++;
            stream_write(DEVICE_STDOUT, &c, 1);
        }
    }

    printf("\n\r");
}

char* find_argument(uint32_t n)
{
    uint32_t waswhitespace = 1;
    uint32_t current = 0;

    for (uint32_t i = 0; i < sizeof(inputbuf); ++i) {
        char c = inputbuf[i];
        if (c == 0) {
            return NULL;
        }

        if (waswhitespace && c != ' ') {
            if (current == n) {
                return inputbuf + i;
            }
            waswhitespace = 0;
            current++;
        }

        if (c == ' ') {
            waswhitespace = 1;
        }
    }
}

void get_argument(char* buf, uint32_t buflen, uint32_t narg)
{
    buf[0] = 0;
    char* ptr = find_argument(narg);
    if (ptr == NULL) {
        return;
    }

    for (uint32_t i = 0; i < (buflen - 1); ++i) {
        char c = ptr[i];
        if (c == 0 || c == ' ') {
            return;
        }
        buf[i] = c;
        buf[i + 1] = 0;
    }
}

uint32_t count_arguments()
{
    uint32_t n = 0;
    while (find_argument(n) != NULL) {
        n++;
    }
    return n;
}

void arg1_func(void (*fun)(char*))
{
    char arg1[64];

    get_argument(arg1, sizeof(arg1), 1);
    if (arg1[0] == 0) {
        printf("needs an argument\n\r");
        return;
    }

    (*fun)(arg1);
}

void arg2_func(void (*fun)(char*, char*))
{
    char arg1[64];
    char arg2[64];

    get_argument(arg1, sizeof(arg1), 1);
    get_argument(arg2, sizeof(arg2), 2);
    if (arg1[0] == 0 || arg2[0] == 0) {
        printf("needs two arguments\n\r");
        return;
    }

    (*fun)(arg1, arg2);
}

void do_ls()
{
    result_t res;

    char pattern[16];
    clear_buf(pattern, sizeof(pattern));
    get_argument(pattern, 16, 1);
    if (pattern[0] == 0) {
        // no argument provided, search for *
        pattern[0] = '*';
    }

    // List contents of current dir
    printf("\n\r");
    struct file_info_t finfo;
    res = fs_findfirst(".", pattern, &finfo);
    while (res == RESULT_OK && finfo.name[0] != 0) {
        char padding[16];
        clear_buf(padding, sizeof(padding));
        uint32_t namelen = strlen(finfo.name);
        for (uint32_t i = 0; i < (sizeof(padding) - namelen); ++i) {
            padding[i] = ' ';
        }
        printf("%s", finfo.name);
        printf("%s %s   ", padding, (finfo.attrib & ATTRIB_DIR) != 0 ? "<DIR>" : "     ");
        printf("%d bytes\n\r", finfo.size);
        res = fs_findnext(&finfo);
    }

    uint64_t free;
    res = fs_free(&free);
    if (res == RESULT_OK) {
        uint32_t freekibi = free / 1024;
        uint32_t freemibi = freekibi / 1024;
        uint32_t freegibi = freemibi / 1024;
        printf("---\n\rfree: %d GiB, %d MiB, %d KiB\n\r", freegibi, freemibi, freekibi);
    } else {
        printf("could not determine number of free bytes\n\r");
    }

    printf("\n\r");
}

void do_mkdir(char* arg1)
{
    result_t res = fs_mkdir(arg1);
    if (res != RESULT_OK) {
        printf("could not make directory %s\n\r", arg1);
    }
}

void do_rm(char* arg1)
{
    struct file_info_t finfo;
    result_t res = fs_findfirst(".", arg1, &finfo);
    while (res == RESULT_OK && finfo.name[0] != 0) {
        result_t unlink_res = fs_unlink(finfo.name);
        if (unlink_res != RESULT_OK) {
            printf("could not remove %s\n\r", finfo.name);
        }
        res = fs_findnext(&finfo);
    }
}

void do_cd(char* arg1)
{
    result_t res = fs_chdir(arg1);
    if (res != RESULT_OK) {
        printf("could not change directory to %s\n\r", arg1);
    }
}

void do_mv(char* arg1, char* arg2)
{
    result_t res = fs_rename(arg1, arg2);
    if (res != RESULT_OK) {
        printf("could not rename %s\n\r", arg1);
    }
}

void do_print(char* arg1)
{
    filehandle_t fh;
    result_t res = fs_open(&fh, arg1, MODE_READ);
    if (res != RESULT_OK) {
        printf("could not open input file\n\r");
        return;
    }

    uint32_t read;
    char buf[512];
    clear_buf(buf, sizeof(buf));
    res = fs_read(fh, buf, sizeof(buf), &read);
    while (res == RESULT_OK && read > 0) {
        printf("%s", buf);
        clear_buf(buf, sizeof(buf));
        res = fs_read(fh, buf, sizeof(buf), &read);
    }

    res = fs_close(fh);
    if (res != RESULT_OK) {
        printf("could not close file\n\r");
    }
}

void do_run(char* arg1)
{

    uint32_t arglen = strlen(arg1);
    char prgfile[arglen + 5];
    strcpy(prgfile, arg1);

    // append suffix for executable files and null-terminate
    prgfile[arglen] = '.';
    prgfile[arglen + 1] = 'b';
    prgfile[arglen + 2] = 'i';
    prgfile[arglen + 3] = 'n';
    prgfile[arglen + 4] = 0;

    uint32_t error = 0;
    filehandle_t fh;
    result_t res = fs_open(&fh, prgfile, MODE_READ);
    if (res != RESULT_OK) {
        printf("could not open %s\n\r", prgfile);
        return;
    }

    uint32_t maxbytes = (512 - 64) * 1024;

    uint32_t size;
    res = fs_size(fh, &size);
    if (size > maxbytes) {
        printf("%s is %d bytes, only up to %d bytes allowed\n\r", prgfile, size, maxbytes);
        fs_close(fh);
        return;
    }

    uint32_t read;
    void* loadaddr = (void*)0x0;
    res = fs_read(fh, loadaddr, maxbytes, &read);
    if (res != RESULT_OK) {
        printf("error reading file %s\n\r", prgfile);
        error = 1;
    }

    res = fs_close(fh);
    if (res != RESULT_OK) {
        printf("could not close file\n\r");
        error = 1;
    }

    if (!error) {
        uint32_t (*program)(char**, uint32_t nargs) = (void*)loadaddr;

        uint32_t nargs = count_arguments();

        // create array of pointers to program argument strings
        char* programargs[nargs];
        for (uint32_t i = 0; i < nargs; ++i) {
            programargs[i] = find_argument(i);
        }

        // null-terminate arguments in buffer
        for (uint32_t i = 0; i < sizeof(inputbuf); ++i) {
            if (inputbuf[i] == ' ') {
                inputbuf[i] = 0;
            }
        }

        uint32_t exitcode = (*program)(programargs, nargs);

        printf("\n\rexit code: %d\n\r", exitcode);
    }
}

void do_cp(char* arg1, char* arg2)
{
    filehandle_t fh1;
    result_t res = fs_open(&fh1, arg1, MODE_READ);
    if (res != RESULT_OK) {
        printf("could not open input file\n\r");
        return;
    }

    filehandle_t fh2;
    res = fs_open(&fh2, arg2, MODE_WRITE | MODE_CREATE_ALWAYS);
    if (res != RESULT_OK) {
        printf("could not open output file\n\r");
        fs_close(fh1);
        return;
    }

    uint32_t read;
    char buf[8192];
    clear_buf(buf, sizeof(buf));
    res = fs_read(fh1, buf, sizeof(buf), &read);
    while (res == RESULT_OK && read > 0) {
        uint32_t written;
        result_t write_res = fs_write(fh2, buf, read, &written);
        if (write_res != RESULT_OK) {
            printf("error writing to output file\n\r");
            break;
        }
        res = fs_read(fh1, buf, sizeof(buf), &read);
    }

    fs_close(fh1);
    fs_close(fh2);
}

void execute_input()
{
    char arg0[16];
    uint32_t narg = count_arguments();
    if (narg == 0) {
        return;
    }
    get_argument(arg0, sizeof(arg0), 0);

    if (strcmp(arg0, "ls") == 0 || strcmp(arg0, "dir") == 0) {
        do_ls();
    } else if (strcmp(arg0, "mkdir") == 0) {
        arg1_func(&do_mkdir);
    } else if (strcmp(arg0, "rm") == 0) {
        arg1_func(&do_rm);
    } else if (strcmp(arg0, "cd") == 0) {
        arg1_func(&do_cd);
    } else if (strcmp(arg0, "mv") == 0) {
        arg2_func(&do_mv);
    } else if (strcmp(arg0, "print") == 0) {
        arg1_func(&do_print);
    } else if (strcmp(arg0, "cp") == 0) {
        arg2_func(&do_cp);
    } else {
        do_run(arg0);
    }
}

/**
 * The main entry point to the shell
 */
int main()
{
    printf("\n\r\n\rSPU32 Shell 0.0.1\n\r");

    while (1) {
        read_input();
        execute_input();
    }

    return 0;
}