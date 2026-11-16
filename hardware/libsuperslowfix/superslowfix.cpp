// Injects the super-slow notification the vendor plugin never sends.
//
// Slow motion on this device runs the entire 960fps burst correctly - the sensor arms, captures, and
// reports done - but the app dies on a 10s watchdog waiting for a callback that never arrives
// (CACAO_RESTORATION.md sections 78-90). The reason is not a broken pipeline; it is a call that is
// simply absent from this firmware:
//
//   excalibur::ProcessCtrl::notifySuperSlowEvt(PAL_Err_t, SuperSlowType)   @0x14a55
//     - the ONLY function that constructs a ProcessCtrlSuperSlowEvt, which is the event that carries
//       a super-slow result back to the cacao client and on to the app
//     - it has ZERO call sites anywhere in libexcal_process_ctrl_plugin.so (verified by direct bl
//       scan, veneer scan, and PLT xref)
//     - doSuperSlowReady() calls notifyStreamingReadyEvt() instead - the ORDINARY streaming notifier
//     - doSuperSlowDone() only reaches trySendSuperSlowDoneEvt(), which builds a DoneEvt, not this
//
// So the machinery is all present and wired; nothing invokes it. This shim interposes the two
// do*() entry points, lets the original run untouched, and then makes the missing call on the same
// `this`.
//
// SAFETY: this loads into cald, the vendor camera HAL daemon that EVERY camera depends on, including
// the stock Camera2 path. It is therefore OPT-IN: nothing happens unless
// /data/local/tmp/cacao_superslow_fix exists. With the flag absent this library is a pass-through.
//
// The SuperSlowType enum values are not known from the binaries. The flag file may contain
// "<ready>,<done>" (e.g. "0,1") to override the defaults; see kReadyType/kDoneType below.

#define LOG_TAG "superslowfix"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <log/log.h>

namespace {

constexpr const char* kFlag = "/data/local/tmp/cacao_superslow_fix";

// Defaults, overridable from the flag file.
int gReadyType = 0;
int gDoneType = 1;
bool gLoaded = false;

bool enabled() {
    if (access(kFlag, F_OK) != 0) return false;
    if (!gLoaded) {
        gLoaded = true;
        FILE* f = fopen(kFlag, "r");
        if (f) {
            int a = 0, b = 0;
            if (fscanf(f, "%d,%d", &a, &b) == 2) {
                gReadyType = a;
                gDoneType = b;
                ALOGI("type overrides from flag file: ready=%d done=%d", a, b);
            }
            fclose(f);
        }
    }
    return true;
}

using DoFn = void (*)(void*, void*);
using NotifyFn = void (*)(void*, int /*PAL_Err_t*/, int /*SuperSlowType*/);

const char* kSym =
        "_ZN9excalibur11ProcessCtrl18notifySuperSlowEvtE9PAL_Err_tNS_13SuperSlowTypeE";
const char* kPlugin = "/vendor/lib/camera/libexcal_process_ctrl_plugin.so";

// The plugin is dlopened into a LOCAL scope, so RTLD_DEFAULT cannot see it even though the symbol
// is exported GLOBAL DEFAULT. Resolve against the library's own handle instead; RTLD_NOLOAD gets a
// handle to the copy that is already mapped without loading a second one. The last resort anchors
// on `anchor` - a pointer we already hold INTO the plugin (from RTLD_NEXT) - so dladdr gives us its
// load base and we can apply the known symbol offset.
NotifyFn resolveNotify(void* anchor) {
    static NotifyFn fn = nullptr;
    static bool tried = false;
    if (tried) return fn;
    tried = true;

    fn = reinterpret_cast<NotifyFn>(dlsym(RTLD_DEFAULT, kSym));
    if (fn != nullptr) { ALOGI("notify resolved via RTLD_DEFAULT"); return fn; }

    void* h = dlopen(kPlugin, RTLD_NOLOAD | RTLD_NOW);
    if (h == nullptr) h = dlopen(kPlugin, RTLD_NOW);
    if (h != nullptr) {
        fn = reinterpret_cast<NotifyFn>(dlsym(h, kSym));
        if (fn != nullptr) { ALOGI("notify resolved via plugin handle %p", h); return fn; }
    }
    ALOGE("dlopen/dlsym failed: %s", dlerror() ? dlerror() : "(none)");

    if (anchor != nullptr) {
        Dl_info info;
        if (dladdr(anchor, &info) != 0 && info.dli_fbase != nullptr) {
            fn = reinterpret_cast<NotifyFn>(
                    reinterpret_cast<uintptr_t>(info.dli_fbase) + 0x14a55u);
            ALOGI("notify resolved via dladdr base=%p -> %p (%s)",
                  info.dli_fbase, reinterpret_cast<void*>(fn), info.dli_fname);
        }
    }
    return fn;
}

}  // namespace

extern "C" {

// excalibur::ProcessCtrl::doSuperSlowReady(MsgBase*)  @0xf7c5, 16 bytes.
// Original body: loads [msg+0xb4]/[msg+0xb8] and tail-calls notifyStreamingReadyEvt(result).
void _ZN9excalibur11ProcessCtrl16doSuperSlowReadyEPNS_7MsgBaseE(void* thiz, void* msg) {
    static DoFn real = reinterpret_cast<DoFn>(
            dlsym(RTLD_NEXT, "_ZN9excalibur11ProcessCtrl16doSuperSlowReadyEPNS_7MsgBaseE"));
    if (real != nullptr) real(thiz, msg);

    if (!enabled()) return;
    NotifyFn notify = resolveNotify(reinterpret_cast<void*>(real));
    if (notify == nullptr) {
        ALOGE("doSuperSlowReady: notifySuperSlowEvt not resolvable");
        return;
    }
    ALOGI("doSuperSlowReady(this=%p): injecting notifySuperSlowEvt(0, %d)", thiz, gReadyType);
    notify(thiz, 0, gReadyType);
}

// excalibur::ProcessCtrl::doSuperSlowDone(MsgBase*)  @0xf7d5, 18 bytes.
// Original body: sets flag [this+0x16e0]=1 and tail-calls trySendSuperSlowDoneEvt().
void _ZN9excalibur11ProcessCtrl15doSuperSlowDoneEPNS_7MsgBaseE(void* thiz, void* msg) {
    static DoFn real = reinterpret_cast<DoFn>(
            dlsym(RTLD_NEXT, "_ZN9excalibur11ProcessCtrl15doSuperSlowDoneEPNS_7MsgBaseE"));
    if (real != nullptr) real(thiz, msg);

    if (!enabled()) return;
    NotifyFn notify = resolveNotify(reinterpret_cast<void*>(real));
    if (notify == nullptr) {
        ALOGE("doSuperSlowDone: notifySuperSlowEvt not resolvable");
        return;
    }
    ALOGI("doSuperSlowDone(this=%p): injecting notifySuperSlowEvt(0, %d)", thiz, gDoneType);
    notify(thiz, 0, gDoneType);
}

}  // extern "C"
