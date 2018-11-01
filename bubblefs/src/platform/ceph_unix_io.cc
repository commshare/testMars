/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2011 New Dream Network
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

// ceph/src/common/safe_io.c
// Pebble/src/common/dir_util.cpp

#include "platform/ceph_unix_io.h"
#include <linux/limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "platform/base_error.h"

// in linux/limits.h
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

namespace bubblefs {
namespace myceph {

ssize_t safe_read(int fd, void *buf, size_t count)
{
        size_t cnt = 0;

        while (cnt < count) {
                ssize_t r = read(fd, buf, count - cnt);
                if (r <= 0) {
                        if (r == 0) {
                                // EOF
                                return cnt;
                        }
                        if (errno == EINTR)
                                continue;
                        return -errno;
                }
                cnt += r;
                buf = (char *)buf + r;
        }
        return cnt;
}

ssize_t safe_read_exact(int fd, void *buf, size_t count)
{
        ssize_t ret = safe_read(fd, buf, count);
        if (ret < 0)
                return ret;
        if ((size_t)ret != count)
                return -EDOM;
        return 0;
}
 
ssize_t safe_write(int fd, const void *buf, size_t count)
{
        while (count > 0) {
                ssize_t r = write(fd, buf, count);
                if (r < 0) {
                        if (errno == EINTR)
                                continue;
                        return -errno;
                }
                count -= r;
                buf = (char *)buf + r;
        }
        return 0;
}

ssize_t safe_pread(int fd, void *buf, size_t count, off_t offset)
{
        size_t cnt = 0;
        char *b = (char*)buf;

        while (cnt < count) {
                ssize_t r = pread(fd, b + cnt, count - cnt, offset + cnt);
                if (r <= 0) {
                        if (r == 0) {
                                // EOF
                                return cnt;
                        }
                        if (errno == EINTR)
                                continue;
                        return -errno;
                }

                cnt += r;
        }
        return cnt;
}

ssize_t safe_pread_exact(int fd, void *buf, size_t count, off_t offset)
{
        ssize_t ret = safe_pread(fd, buf, count, offset);
        if (ret < 0)
                return ret;
        if ((size_t)ret != count)
                return -EDOM;
        return 0;
}

ssize_t safe_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
        while (count > 0) {
                ssize_t r = pwrite(fd, buf, count, offset);
                if (r < 0) {
                        if (errno == EINTR)
                                continue;
                        return -errno;
                }
                count -= r;
                buf = (char *)buf + r;
                offset += r;
        }
        return 0;
}

ssize_t safe_splice(int fd_in, off_t *off_in, int fd_out, off_t *off_out,
                    size_t len, unsigned int flags)
{
  size_t cnt = 0;

  while (cnt < len) {
    ssize_t r = splice(fd_in, off_in, fd_out, off_out, len - cnt, flags);
    if (r <= 0) {
      if (r == 0) {
        // EOF
        return cnt;
      }
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN)
        break;
      return -errno;
    }
    cnt += r;
  }
  return cnt;
}

ssize_t safe_splice_exact(int fd_in, off_t *off_in, int fd_out,
                          off_t *off_out, size_t len, unsigned int flags)
{
  ssize_t ret = safe_splice(fd_in, off_in, fd_out, off_out, len, flags);
  if (ret < 0)
    return ret;
  if ((size_t)ret != len)
    return -EDOM;
  return 0;
}

int safe_write_file(const char *base, const char *file,
                    const char *val, size_t vallen)
{
  int ret;
  char fn[PATH_MAX];
  char tmp[PATH_MAX];
  int fd;

  // does the file already have correct content?
  char oldval[80];
  ret = safe_read_file(base, file, oldval, sizeof(oldval));
  if (ret == (int)vallen && memcmp(oldval, val, vallen) == 0)
    return 0;  // yes.

  snprintf(fn, sizeof(fn), "%s/%s", base, file);
  snprintf(tmp, sizeof(tmp), "%s/%s.tmp", base, file);
  fd = open(tmp, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  if (fd < 0) {
    ret = errno;
    return -ret;
  }
  ret = safe_write(fd, val, vallen);
  if (ret) {
    VOID_TEMP_FAILURE_RETRY(close(fd));
    return ret;
  }

  ret = fsync(fd);
  if (ret < 0) ret = -errno;
  VOID_TEMP_FAILURE_RETRY(close(fd));
  if (ret < 0) {
    unlink(tmp);
    return ret;
  }
  ret = rename(tmp, fn);
  if (ret < 0) {
    ret = -errno;
    unlink(tmp);
    return ret;
  }

  fd = open(base, O_RDONLY);
  if (fd < 0) {
    ret = -errno;
    return ret;
  }
  ret = fsync(fd);
  if (ret < 0) ret = -errno;
  VOID_TEMP_FAILURE_RETRY(close(fd));

  return ret;
}

int safe_read_file(const char *base, const char *file,
                   char *val, size_t vallen)
{
  char fn[PATH_MAX];
  int fd, len;

  snprintf(fn, sizeof(fn), "%s/%s", base, file);
  fd = open(fn, O_RDONLY);
  if (fd < 0) {
    return -errno;
  }
  len = safe_read(fd, val, vallen);
  if (len < 0) {
    VOID_TEMP_FAILURE_RETRY(close(fd));
    return len;
  }
  // close sometimes returns errors, but only after write()
  VOID_TEMP_FAILURE_RETRY(close(fd));

  return len;
}

char DirUtil::m_last_error[256] = {0};

int DirUtil::MakeDir(const std::string& path) {
    if (path.empty()) {
        _LOG_LAST_ERROR("param path is NULL");
        return -1;
    }

    if (access(path.c_str(), F_OK | W_OK) == 0) {
        return 0;
    }

    if (mkdir(path.c_str(), 0755) != 0) {
        _LOG_LAST_ERROR("mkdir %s failed(%s)", path.c_str(), strerror(errno));
        return -1;
    }

    return 0;
}

int DirUtil::MakeDirP(const std::string& path) {
    if (path.empty()) {
        _LOG_LAST_ERROR("param path is NULL");
        return -1;
    }
    if (path.size() > PATH_MAX) {
        _LOG_LAST_ERROR("path length %zu > PATH_MAX(%d)", path.size(), PATH_MAX);
        return -1;
    }

    int len = path.length();
    char tmp[PATH_MAX] = {0};
    snprintf(tmp, sizeof(tmp), "%s", path.c_str());

    for (int i = 1; i < len; i++) {
        if (tmp[i] != '/') {
            continue;
        }

        tmp[i] = '\0';
        if (MakeDir(tmp) != 0) {
            return -1;
        }
        tmp[i] = '/';
    }

    return MakeDir(path);
}

} // namespace myceph
} // namespace bubblefs