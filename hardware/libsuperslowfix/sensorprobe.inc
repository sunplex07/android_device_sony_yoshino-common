
// ---- gyro/EIS probe ---------------------------------------------------------------------------
// The camera HAL reaches sensors through the NDK ASensorManager path, which rides A16's
// libbinder_ndk on top of a Pie-generation VNDK libbinder - that mixed stack is the confirmed cause
// of "Gyro data were not acquired". The proposed fix is to bridge onto the HIDL
// android.frameworks.sensorservice@1.0 ISensorManager, which lshal shows alive on hwbinder with the
// client glue present-but-unloaded in this very process. Before writing that bridge, prove the one
// thing it depends on: that getService() actually returns an interface FROM INSIDE this process.
//
// ABI note: tryGetService returns sp<ISensorManager> - a non-trivial class - so on ARM32 it is
// returned through a hidden first pointer argument (sret), and the std::string argument is passed by
// const reference. Hence the shape below.
#include <string>
namespace {
constexpr const char* kSensorFlag = "/data/local/tmp/cacao_sensor_probe";
using TryGetFn = void (*)(void* /*sret sp<>*/, const std::string* /*name*/, bool /*getStub*/);

void sensorProbe() {
    if (access(kSensorFlag, F_OK) != 0) return;
    const char* kLib = "/vendor/lib/android.frameworks.sensorservice@1.0.so";
    void* h = dlopen(kLib, RTLD_NOW);
    if (h == nullptr) { ALOGE("sensorprobe: dlopen failed: %s", dlerror()); return; }
    const char* kSym =
        "_ZN7android10frameworks13sensorservice4V1_014ISensorManager13tryGetServiceERKNSt3__1"
        "12basic_stringIcNS4_11char_traitsIcEENS4_9allocatorIcEEEEb";
    TryGetFn fn = reinterpret_cast<TryGetFn>(dlsym(h, kSym));
    if (fn == nullptr) { ALOGE("sensorprobe: dlsym(tryGetService) failed: %s", dlerror()); return; }
    void* sret[4] = {nullptr, nullptr, nullptr, nullptr};
    std::string name("default");
    ALOGI("sensorprobe: calling ISensorManager::tryGetService(\"default\") from pid %d", getpid());
    fn(sret, &name, false);
    ALOGI("sensorprobe: RESULT sp=%p  (non-null => the HIDL bridge is viable)", sret[0]);
    if (sret[0] != nullptr) {
        const void* const* vt = *reinterpret_cast<const void* const* const*>(sret[0]);
        ALOGI("sensorprobe: interface vptr=%p slot0=%p", (const void*)vt, vt ? vt[0] : nullptr);
    }
}
}  // namespace
