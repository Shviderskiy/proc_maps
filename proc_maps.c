#include "proc_maps.h"

#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

static int _open_proc_maps(pid_t pid_)
{
    if (pid_ < 0)
        return open("/proc/self/maps", O_RDONLY);

    /* len("/proc/" + str(2**64) + /maps") + 1 = 32 */
    char maps_path[32];
    sprintf(maps_path, "/proc/%lu/maps", (unsigned long int)pid_);
    return open(maps_path, O_RDONLY);
}

typedef struct
{
    int    fd;
    char * buf;
    size_t max_buf_size;
    size_t cur_buf_size;
    size_t cur_buf_pos;
    int    is_eof;
}
_reading_context;

static int _buffered_getc(_reading_context * rctx_, int * result_)
{
    if (rctx_->is_eof)
        goto eof;

    if (rctx_->cur_buf_pos == rctx_->cur_buf_size)
    {
        /* dubious optimization */
        if (rctx_->cur_buf_size != 0 &&
                rctx_->cur_buf_size < rctx_->max_buf_size)
            goto eof;

        ssize_t rres = read(rctx_->fd, rctx_->buf, rctx_->max_buf_size);
        if (rres < 0)
            return -1; /* errno is filled */

        if (rres == 0)
            goto eof;

        rctx_->cur_buf_pos  = 0;
        rctx_->cur_buf_size = (size_t)rres;
    }

    if (result_ != NULL)
        *result_ =  rctx_->buf[rctx_->cur_buf_pos];
    ++rctx_->cur_buf_pos;
    return 0;

eof:
    rctx_->is_eof = 1;
    if (result_ != NULL)
        *result_ = EOF;
    return 0;
}

static int _parse_hex(_reading_context * rctx_, char end_, uint64_t * result_)
{
    uint64_t result = 0;

    int c;
    while (1)
    {
        if (_buffered_getc(rctx_, &c) != 0)
            return -1; /* errno is filled */

        if (c == EOF)
            return -1; /* unexpected end */

        if (c == end_)
            break;

        result *= 16;
        if ('0' <= c && c <= '9')
            result += (uint64_t)(c - '0');
        else if ('a' <= c && c <= 'f')
            result += (uint64_t)(c - 'a' + 10);
        else if ('A' <= c && c <= 'F')
            result += (uint64_t)(c - 'A' + 10);
        else
            return -1; /* invalid char */
    }

    if (result_ != NULL)
        *result_ = result;
    return 0;
}

static int _parse_prot_liter(_reading_context * rctx_, char liter_)
{
    int c;

    if (_buffered_getc(rctx_, &c) != 0)
        return -1; /* errno is filled */

    if (c == EOF)
        return -1; /* unexpected end */

    if (c == liter_)
        return 1;

    if (c == '-') /* OK */
        return 0;

    return -1; /* invalid char */
}

static int _parse_prot(_reading_context * rctx_, int * result_)
{
    int r = _parse_prot_liter(rctx_, 'r');
    if (r < 0)
       return r; /* error */

    int w = _parse_prot_liter(rctx_, 'w');
    if (w < 0)
       return w; /* error */

    int x = _parse_prot_liter(rctx_, 'x');
    if (x < 0)
       return x; /* error */

    if (result_ != NULL)
        *result_ = (r ? PROT_READ  : PROT_NONE) |
                   (w ? PROT_WRITE : PROT_NONE) |
                   (x ? PROT_EXEC  : PROT_NONE);
    return 0;
}

static int _parse_flag(_reading_context * rctx_, int * result_)
{
    int c;

    if (_buffered_getc(rctx_, &c) != 0)
        return -1; /* errno is filled */

    if (c == EOF)
        return -1; /* unexpected end */

    int result;
    if (c == 'p')
        result = MAP_PRIVATE;
    else if (c == 's')
        result = MAP_SHARED;
    else
        return -1; /* invalid char */

    if (result_ != NULL)
        *result_ = result;
    return 0;
}

static int _parse_dec(_reading_context * rctx_, char end_, uint64_t * result_)
{
    uint64_t result = 0;

    int c;
    while (1)
    {
        if (_buffered_getc(rctx_, &c) != 0)
            return -1; /* errno is filled */

        if (c == EOF)
            return -1; /* unexpected end */

        if (c == end_)
            break;

        result *= 10;
        if ('0' <= c && c <= '9')
            result += (uint64_t)(c - '0');
        else
            return -1; /* invalid char */
    }

    if (result_ != NULL)
        *result_ = result;
    return 0;
}

static int _skip_spaces_and_parse_str(
        _reading_context * rctx_, char end_, size_t max_len_, char * result_)
{
    size_t i = 0;

    int c = EOF;

    int need_parse = 1;

    while (need_parse)
    {
        if (_buffered_getc(rctx_, &c) != 0)
            return -1; /* errno is filled */

        if (c == EOF || c == end_)
            need_parse = 0;

        if (c != ' ')
            break;
    }

    if (need_parse)
    {
        if (result_ != NULL && i + 1 < max_len_)
            result_[i++] = (char)c;
    }

    while (need_parse)
    {
        if (_buffered_getc(rctx_, &c) != 0)
            return -1; /* errno is filled */

        if (c == EOF || c == end_)
            break;

        if (result_ != NULL && i + 1 < max_len_)
            result_[i++] = (char)c;
    }

    if (result_ != NULL && i + 1 < max_len_)
        result_[i++] = '\0';

    return 0;
}

static int _proc_maps_iterate(
        int(* callback_)(proc_maps_record const * record_, void * context_),
        void * context_,
        _reading_context * rctx_)
{
    char pathname[PATH_MAX];
    proc_maps_record record;
    record.pathname = pathname;

    while (1)
    {
        uint64_t hex, dec;

        if (_parse_hex(rctx_, '-', &hex) != 0)
        {
            if (rctx_->is_eof)
                return 0;
            return -1;
        }
        record.addr_beg = (void *)((uintptr_t)hex);

        if (_parse_hex(rctx_, ' ', &hex) != 0)
            return -1;
        record.addr_end = (void *)((uintptr_t)hex);

        if (_parse_prot(rctx_, &record.prot) != 0)
            return -1;

        if (_parse_flag(rctx_, &record.flag) != 0)
            return -1;

        int c;
        if (_buffered_getc(rctx_, &c) != 0)
            return -1; /* errno is filled */

        if (c == EOF)
            return -1; /* unexpected end */

        if (c != ' ')
            return -1; /* invalid char */

        if (_parse_hex(rctx_, ' ', &hex) != 0)
            return -1;
        record.offset = (off_t)hex;

        if (_parse_dec(rctx_, ':', &dec) != 0)
            return -1;
        record.dev_major = (unsigned int)dec;

        if (_parse_dec(rctx_, ' ', &dec) != 0)
            return -1;
        record.dev_minor = (unsigned int)dec;

        if (_parse_dec(rctx_, ' ', &dec) != 0)
            return -1;
        record.inode = (int)dec;

        if (_skip_spaces_and_parse_str(
                rctx_, '\n', sizeof(pathname), pathname) != 0)
            return -1;

        if (callback_(&record, context_) != 0)
            break;
    }

    return 0;
}

int proc_maps_iterate(
        pid_t pid_,
        int(* callback_)(proc_maps_record const * record_, void * context_),
        void * context_,
        char * rbuf_,
        size_t rbuf_size_)
{
    if (callback_ == NULL)
        return 0;

    _reading_context rctx;
    rctx.fd = _open_proc_maps(pid_);
    if (rctx.fd == -1)
        return -1 /* errno is filled */;

    rctx.cur_buf_size = rctx.cur_buf_pos = 0;
    rctx.is_eof = 0;

    if (rbuf_ != NULL && rbuf_size_ != 0)
    {
        rctx.buf = rbuf_;
        rctx.max_buf_size = rbuf_size_;
        goto execution;
    }

    char buf[64];
    rctx.buf = buf;
    rctx.max_buf_size = sizeof(buf);

execution:
    {
        int result = _proc_maps_iterate(callback_, context_, &rctx);
        int last_errno = errno;
        if (close(rctx.fd) == -1)
        {
            if (result == 0)
                return -1; /* errno is filled */
            errno = last_errno;
        }
        return result;
    }
}
