// Tencent is pleased to support the open source community by making Mars available.
// Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

// Licensed under the MIT License (the "License"); you may not use this file except in 
// compliance with the License. You may obtain a copy of the License at
// http://opensource.org/licenses/MIT

// Unless required by applicable law or agreed to in writing, software distributed under the License is
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions and
// limitations under the License.

// mars/mars/comm/unix/thread/mutex.h

#ifndef BUBBLEFS_PLATFORM_MARS_MUTEX_H_
#define BUBBLEFS_PLATFORM_MARS_MUTEX_H_

#include <errno.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>

#include "platform/macros.h"
#include "platform/mars_time_utils.h"

namespace bubblefs {
namespace mymars {

class Mutex {
  public:
    typedef pthread_mutex_t handle_type;
    Mutex(bool _recursive = false)
        : magic_(reinterpret_cast<uintptr_t>(this)), mutex_(), attr_() {
        //禁止重复加锁
        int ret = pthread_mutexattr_init(&attr_);

        if (ENOMEM == ret) ASSERT(0 == ENOMEM);
        else if (0 != ret) ASSERT(0 == ret);

        ret = pthread_mutexattr_settype(&attr_, _recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_ERRORCHECK);

        if (EINVAL == ret) ASSERT(0 == EINVAL);
        else if (0 != ret) ASSERT(0 == ret);

        ret = pthread_mutex_init(&mutex_, &attr_);

        if (EAGAIN == ret) ASSERT(0 == EAGAIN);
        else if (ENOMEM == ret) ASSERT(0 == ENOMEM);
        else if (EPERM == ret) ASSERT(0 == EPERM);
        else if (EBUSY == ret) ASSERT(0 == EBUSY);
        else if (EINVAL == ret) ASSERT(0 == EINVAL);
        else if (0 != ret) ASSERT(0 == ret);
    }

    ~Mutex() {
        magic_ = 0;
        int ret = pthread_mutex_destroy(&mutex_);

        if (EBUSY == ret) ASSERT(0 == EBUSY);
        else if (EINVAL == ret) ASSERT(0 == EINVAL);
        else if (0 != ret) ASSERT(0 == ret);

        ret = pthread_mutexattr_destroy(&attr_);

        if (EINVAL == ret) ASSERT(0 == EINVAL);
        else if (0 != ret) ASSERT(0 == ret);
    }

    bool lock() {
        // 成功返回0，失败返回错误码
        ASSERT(reinterpret_cast<uintptr_t>(this) == magic_ && 0 != magic_); // "this:%p != mageic:%p", this, (void*)magic_);

        if (reinterpret_cast<uintptr_t>(this) != magic_) return false;

        int ret = pthread_mutex_lock(&mutex_);

        if (EINVAL == ret) ASSERT(0 == EINVAL);
        else if (EAGAIN == ret) ASSERT(0 == EAGAIN);
        else if (EDEADLK == ret) ASSERT(0 == EDEADLK);
        else if (0 != ret) ASSERT(0 == ret);

        return 0 == ret;
    }

    bool unlock() {
        ASSERT(reinterpret_cast<uintptr_t>(this) == magic_ && 0 != magic_); // "this:%p != mageic:%p", this, (void*)magic_);
        //        if (reinterpret_cast<uintptr_t>(this)!=m_magic) return false;

        int ret = pthread_mutex_unlock(&mutex_);

        if (EINVAL == ret) ASSERT(0 == EINVAL);
        else if (EAGAIN == ret) ASSERT(0 == EAGAIN);
        else if (EPERM == ret) ASSERT(0 == EPERM);
        else if (0 != ret) ASSERT(0 == ret);

        return 0 == ret;
    }

    bool trylock() {
        ASSERT(reinterpret_cast<uintptr_t>(this) == magic_ && 0 != magic_); // "this:%p != mageic:%p", this, (void*)magic_);

        if (reinterpret_cast<uintptr_t>(this) != magic_) return false;

        int ret = pthread_mutex_trylock(&mutex_);

        if (EBUSY == ret) return false;

        if (EINVAL == ret) ASSERT(0 == EINVAL);
        else if (EAGAIN == ret) ASSERT(0 == EAGAIN);
        else if (EDEADLK == ret) ASSERT(0 == EDEADLK);
        else if (0 != ret) ASSERT(0 == ret);

        return 0 == ret;
    }

    bool timedlock(long _millisecond) {
        ASSERT(reinterpret_cast<uintptr_t>(this) == magic_ && 0 != magic_); // "this:%p != mageic:%p", this, (void*)magic_);

        if (reinterpret_cast<uintptr_t>(this) != magic_) return false;

        struct timespec ts;
        MakeTimeout(&ts, _millisecond);

        int ret = pthread_mutex_timedlock(&mutex_, &ts);

        switch (ret) {
        case 0: return true;

        case ETIMEDOUT: return false;

        case EAGAIN: ASSERT(false); return false;

        case EDEADLK: ASSERT("EDEADLK"); return false;

        case EINVAL: ASSERT("EINVAL"); return false;

        default: ASSERT(false); return false;
        }

        return false;
    }

    bool islocked() {
        ASSERT(reinterpret_cast<uintptr_t>(this) == magic_);

        int ret = pthread_mutex_trylock(&mutex_);

        if (0 == ret) unlock();

        return 0 != ret;
    }

    handle_type& internal() { return mutex_; }

  private:
    Mutex(const Mutex&);
    Mutex& operator = (const Mutex&);

  private:
    static void MakeTimeout(struct timespec* pts, long millisecond) {
        struct timeval tv;
        gettimeofday(&tv, 0);
        pts->tv_sec = millisecond / 1000 + tv.tv_sec;
        pts->tv_nsec = (millisecond % 1000) * 1000 * 1000 + tv.tv_usec * 1000;

        pts->tv_sec += pts->tv_nsec / (1000 * 1000 * 1000);
        pts->tv_nsec = pts->tv_nsec % (1000 * 1000 * 1000);
    }

  private:
    uintptr_t    magic_;  // Dangling pointer will dead lock, so check it!!!
    pthread_mutex_t mutex_;
    pthread_mutexattr_t attr_;
};

} // namespace mymars
} // namespace bubblefs

#endif // BUBBLEFS_PLATFORM_MARS_MUTEX_H_ 