#include <dlfcn.h>
#include <stdint.h>
#include <log/log.h>

static const char* SYM_REGISTER =
    "_ZN8gralloc113BufferManager20RegisterHandleLockedEPK16private_handle_tii";
static const char* SYM_GETBUF =
    "_ZN8gralloc113BufferManager25GetBufferFromHandleLockedEPK16private_handle_t";

typedef void (*RegisterFn)(void* bm, const void* handle, int ionfd, int metafd);
typedef void (*GetBufFn)(uint32_t* out2, void* bm, const void* handle);

static RegisterFn s_realRegister = nullptr;
static GetBufFn   s_realGetBuf   = nullptr;
static volatile int s_triedResolve = 0;

__attribute__((constructor)) static void gd_loaded() {
    ALOGI("grallocdedup: LOADED (constructor) - interposer present");
}

static void resolve_real() {
    if (__atomic_load_n(&s_triedResolve, __ATOMIC_ACQUIRE)) return;
    s_realRegister = (RegisterFn)dlsym(RTLD_NEXT, SYM_REGISTER);
    s_realGetBuf   = (GetBufFn)  dlsym(RTLD_NEXT, SYM_GETBUF);
    __atomic_store_n(&s_triedResolve, 1, __ATOMIC_RELEASE);
    if (s_realRegister && s_realGetBuf)
        ALOGI("grallocdedup: resolved register=%p getbuf=%p",
              (void*)s_realRegister, (void*)s_realGetBuf);
    else
        ALOGE("grallocdedup: resolve FAILED reg=%p get=%p (inert)",
              (void*)s_realRegister, (void*)s_realGetBuf);
}

extern "C" __attribute__((visibility("default")))
void _ZN8gralloc113BufferManager20RegisterHandleLockedEPK16private_handle_tii(
        void* bm, const void* handle, int ionfd, int metafd) {
    resolve_real();
    if (s_realGetBuf) {
        uint32_t found[2] = {0, 0};
        s_realGetBuf(found, bm, handle);
        if (found[0]) {
            uint32_t* buf  = (uint32_t*)found[0];
            uint32_t* ctrl = (uint32_t*)found[1];
            __atomic_add_fetch(&buf[1], 1, __ATOMIC_RELAXED);
            if (ctrl) __atomic_sub_fetch(&ctrl[1], 1, __ATOMIC_ACQ_REL);
            ALOGI("grallocdedup: dedup hit handle=%p buf=%p refcount++", handle, buf);
            return;
        }
    }
    if (s_realRegister) {
        s_realRegister(bm, handle, ionfd, metafd);
    }
}
