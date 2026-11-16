// A deliberately tiny on-device probe for the cacao client library.
//
// Purpose: settle empirically whether Sony's Pie libcacao_client.so really does fault when its
// stack-allocated android::Parcel meets A16's much larger Parcel, and to give the clean-room
// replacement the same smoke test so the two can be compared directly.
//
// It dlopens whichever library it is pointed at and calls the two entry points that need no
// caller-supplied vtable:
//   cacao::ProcessFactory::createProcess()        - builds Parcels (getService + connect)
//   cacao::ProcessFactory::getJpegBufferSize(...) - asks the service
// getCaps is deliberately NOT called here: it needs a caller-owned Caps object with a real vtable.

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <binder/Parcel.h>
#include <binder/IPCThreadState.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/ProcessState.h>

namespace {
struct ImageSize {
    uint32_t width;
    uint32_t height;
};

// Mirrors cacao::SerializedData exactly (memSize +388, memPtr +392). The trailing 8 bytes are a
// word pair, not a uint64_t, so the struct is 404 on ILP32 and 408 on LP64 - see cacao_abi.h.
struct SerializedData {
    int32_t numFds;
    int32_t fds[96];
    int32_t memSize;
    void* memPtr;
    int32_t reservedTail[2];
};
#if defined(__LP64__)
static_assert(sizeof(SerializedData) == 408, "SerializedData must be 408 bytes on LP64");
#else
static_assert(sizeof(SerializedData) == 404, "SerializedData must be 404 bytes on ILP32");
#endif

// Mirrors cacao::ObjectBase's 4 vtable slots so that the first NEW virtual of a derived class
// lands at vptr+32, which is where Caps::getSerializeSize lives.
class ObjectBaseStub {
public:
    virtual ~ObjectBaseStub() {}       // +0 / +8
    virtual void dump(unsigned int) {} // +16
    virtual void reservedSlot24() {}   // +24
};

// A caller-owned Caps, which is what getCaps expects: it never constructs one, it only calls
// through the vtable at +32 / +40 / +48.
class TestCaps : public ObjectBaseStub {
public:
    uint32_t getSerializeSizeCalls = 0;
    uint32_t serializeCalls = 0;
    uint32_t deserializeCalls = 0;

    virtual uint32_t getSerializeSize() {   // +32
        getSerializeSizeCalls++;
        printf("[caps]   getSerializeSize() -> 256\n");
        fflush(stdout);
        return 256;
    }
    virtual int32_t serialize(SerializedData* d) {   // +40
        serializeCalls++;
        printf("[caps]   serialize(memSize=%d, memPtr=%p, numFds=%d)\n", d->memSize, d->memPtr,
               d->numFds);
        fflush(stdout);
        if (d->memPtr == nullptr || d->memSize <= 0) {
            printf("[caps]   !! shared buffer was not set up\n");
            return -1;
        }
        memset(d->memPtr, 0, static_cast<size_t>(d->memSize));
        d->numFds = 0;
        return 0;
    }
    virtual int32_t deserialize(SerializedData* d) {  // +48
        deserializeCalls++;
        printf("[caps]   deserialize(memSize=%d, memPtr=%p)  <-- SERVICE REPLIED\n", d->memSize,
               d->memPtr);
        fflush(stdout);
        return 0;
    }
};

struct CameraIndex {
    int32_t index;
};

// Mirrors cacao::ProcessModeBase: ObjectBase's four slots, then reserved@+32,
// getSerializeSize@+40, serialize@+48.
class TestMode : public ObjectBaseStub {
public:
    virtual int32_t reservedAt32() { return 0; }        // +32
    virtual uint32_t getSerializeSize() {               // +40
        printf("[mode]   getSerializeSize() -> 36\n");
        fflush(stdout);
        return 36;
    }
    virtual int32_t serialize(SerializedData* d) {      // +48
        printf("[mode]   serialize(memSize=%d, memPtr=%p)\n", d->memSize, d->memPtr);
        fflush(stdout);
        if (d->memPtr == nullptr) return -1;
        memset(d->memPtr, 0, 36);
        d->numFds = 0;
        return 0;
    }
};

// Mirrors cacao::ProcessConfigBase: ObjectBase's four slots, then getType@+32, two reserved,
// getSerializeSize@+56, serialize@+64, deserialize@+72.
class TestConfig : public ObjectBaseStub {
public:
    virtual uint32_t getType() { return 0; }             // +32
    virtual int32_t reservedAt40() { return 0; }         // +40
    virtual int32_t reservedAt48() { return 0; }         // +48
    virtual uint32_t getSerializeSize() {                // +56
        printf("[cfg]    getSerializeSize() -> 64\n");
        fflush(stdout);
        return 64;
    }
    virtual int32_t serialize(SerializedData* d) {       // +64
        printf("[cfg]    serialize(memSize=%d, memPtr=%p)\n", d->memSize, d->memPtr);
        fflush(stdout);
        if (d->memPtr == nullptr) return -1;
        memset(d->memPtr, 0, 64);
        d->numFds = 0;
        return 0;
    }
    virtual int32_t deserialize(SerializedData*) {       // +72
        printf("[cfg]    deserialize() called\n");
        fflush(stdout);
        return 0;
    }
};

// Mirrors cacao::IGatewayProcess. It does NOT derive ObjectBase - D2@0, D0@8, then the methods,
// so subscribeEvents lands at +16 and start at +24 (see cacao_abi.h).
class GatewayMirror {
public:
    virtual ~GatewayMirror() {}
    virtual int32_t subscribeEvents(void*, void*) { return 0; }   // +16
    virtual int32_t start(void*) { return 0; }                    // +24
    virtual int32_t stop() { return 0; }                          // +32
    virtual int32_t setConfig(void*) { return 0; }                // +40
    virtual int32_t getConfig(void*) { return 0; }                // +48
};
}  // namespace

int main(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/system/lib64/libcacao_client.so";

    // A binder thread pool is REQUIRED before any of this works. getCaps hands the service a local
    // binder (the MemoryBase backing the shared buffer), and a process with no thread pool cannot
    // serve it - the driver rejects BINDER_SET_MAX_THREADS with EINVAL and the transaction comes
    // back as UNKNOWN_ERROR (0x80000000). Sony's own ProcessFactory::createProcess starts the pool
    // for the same reason. Without this the probe measures its own harness, not the library.
    // The Pie libcacao_service.so allocates its shared-memory objects with HARDCODED sizes
    // (operator new(0x38) for MemoryHeapBase @0x9520, new(0x24) for MemoryBase @0x953c). If A16
    // lays those classes out larger, the daemon under-allocates and the object's tail - including
    // the RefBase subobject the sp constructor reaches via the vtable's virtual-base offset - lands
    // outside the allocation. That is exactly the SIGSEGV/SEGV_ACCERR-on-write seen in the
    // cacaoserver tombstone. Print the real sizes for this ABI so the theory can be checked rather
    // than assumed.
    printf("[probe] ABI check: sizeof(MemoryHeapBase)=%zu (Pie allocates 56)  "
           "sizeof(MemoryBase)=%zu (Pie allocates 36)\n",
           sizeof(android::MemoryHeapBase), sizeof(android::MemoryBase));

    // BpCacaoProcessCallback::notifyResult in the 32-bit blob stack-allocates two Parcels 52 bytes
    // apart (sp+0x0c and sp+0x40) inside a 0x78-byte frame whose stack guard sits at sp+0x74. If
    // this ABI's Parcel is larger than 52, constructing them overruns the frame - which is what the
    // cacaoserver __stack_chk_fail tombstone shows. Print it rather than assert it.
    printf("[probe] ABI check: sizeof(Parcel)=%zu (the blob's frame allows 52)  -> %s\n",
           sizeof(android::Parcel),
           sizeof(android::Parcel) > 52 ? "OVERRUNS the blob frame" : "fits");
    fflush(stdout);

    // Replicate, on a REAL A16 MemoryHeapBase, the exact arithmetic the Pie blob's
    // sp<IMemoryHeap>::sp<MemoryHeapBase>(MemoryHeapBase*) performs (@0x98ea):
    //     r0 = *(obj);  r0 = *(r0 - 12);  r2 = obj + r0;
    //     r0 = *(obj + r0);  r0 = *(r0 - 12);  r0 = r0 + r2;   -> passed to incStrong
    // If A16's vtable does not carry a virtual-base offset at [vptr-12], this walks off into
    // nonsense - which is what the cacaoserver tombstone shows (object's computed vptr/mRefs both
    // unmapped). Comparing the computed pointer against the object's real address settles it.
    {
        android::sp<android::MemoryHeapBase> heap =
                new android::MemoryHeapBase(4096, 0, "probe-abi-test");
        const uint8_t* obj = reinterpret_cast<const uint8_t*>(heap.get());
        printf("[abi] MemoryHeapBase object = %p  heapID=%d\n", obj, heap->getHeapID());
        {
            // The crashed object's first words were  e522b334 e522b36c 00000000 00000000
            // then  00000008 00001000 e7a14000 - i.e. flags, size 4096 and a real
            // /dev/ashmem/MemoryHeapBase mapping. So it looked CONSTRUCTED. Print the same words
            // from a known-good object to see whether word1 (mRefs) normally sits near the vtable
            // or on the heap.
            const uint32_t* w = reinterpret_cast<const uint32_t*>(obj);
            printf("[abi] healthy object words: %08x %08x %08x %08x | %08x %08x %08x\n",
                   w[0], w[1], w[2], w[3], w[4], w[5], w[6]);
            printf("[abi] word1 - word0 = %d  (crash had 0x38 = 56)\n", (int)(w[1] - w[0]));
        }
        if (obj != nullptr) {
            const intptr_t* vptr = *reinterpret_cast<const intptr_t* const*>(obj);
            printf("[abi] vptr=%p  [vptr-4]=%pd  [vptr-8]=%p  [vptr-12]=%p\n", (void*)vptr,
                   (void*)vptr[-1], (void*)vptr[-2], (void*)vptr[-3]);
            const intptr_t off1 = vptr[-3];                       // *(vptr - 12)
            printf("[abi] step1: vbase-offset candidate = %ld\n", (long)off1);
            if (off1 > -4096 && off1 < 4096) {
                const uint8_t* adj = obj + off1;
                const intptr_t* vptr2 = *reinterpret_cast<const intptr_t* const*>(adj);
                const intptr_t off2 = vptr2[-3];
                const uint8_t* result = adj + off2;
                printf("[abi] step2: second offset = %ld -> computed RefBase* = %p\n", (long)off2,
                       (const void*)result);
                // The REAL question is not "is it inside the object" (too weak - the earlier
                // version of this test passed on that and was meaningless). It is whether the
                // computed pointer equals the address the compiler itself uses for the RefBase
                // subobject. Ask the compiler.
                const android::RefBase* realRefBase =
                        static_cast<const android::RefBase*>(heap.get());
                printf("[abi] computed = %p   compiler's RefBase* = %p   delta = %d\n",
                       (const void*)result, (const void*)realRefBase,
                       (int)(reinterpret_cast<const uint8_t*>(realRefBase) - result));
                printf("[abi] VERDICT: %s\n",
                       (reinterpret_cast<const uint8_t*>(realRefBase) == result)
                               ? "blob arithmetic MATCHES the compiler - not the bug"
                               : "blob arithmetic MISMATCHES - incStrong gets the wrong this");
            } else {
                printf("[abi] step1 value is not a plausible offset -> blob arithmetic BROKEN\n");
            }
        }
        fflush(stdout);
    }

    android::ProcessState::self()->startThreadPool();
    printf("[probe] binder thread pool started\n");
    fflush(stdout);

    printf("[probe] dlopen %s\n", path);
    fflush(stdout);

    void* h = dlopen(path, RTLD_NOW);
    if (h == nullptr) {
        printf("[probe] dlopen FAILED: %s\n", dlerror());
        return 1;
    }
    printf("[probe] dlopen ok\n");
    fflush(stdout);

    using CreateProcessFn = void* (*)();
    using JpegSizeFn = int32_t (*)(ImageSize);

    auto createProcess = reinterpret_cast<CreateProcessFn>(
            dlsym(h, "_ZN5cacao14ProcessFactory13createProcessEv"));
    auto jpegSize = reinterpret_cast<JpegSizeFn>(
            dlsym(h, "_ZN5cacao14ProcessFactory17getJpegBufferSizeENS_9ImageSizeE"));

    printf("[probe] createProcess=%p getJpegBufferSize=%p\n", (void*)createProcess,
           (void*)jpegSize);
    fflush(stdout);

    if (jpegSize != nullptr) {
        printf("[probe] calling getJpegBufferSize(5504x3096)...\n");
        fflush(stdout);
        ImageSize s = {5504u, 3096u};
        int32_t n = jpegSize(s);
        printf("[probe] getJpegBufferSize -> %d\n", n);
        fflush(stdout);
    }

    if (createProcess != nullptr) {
        printf("[probe] calling createProcess()...\n");
        fflush(stdout);
        void* p = createProcess();
        printf("[probe] createProcess -> %p\n", p);
        fflush(stdout);
    }

    // getCaps: the real end-to-end test. It needs a caller-owned Caps with a live vtable, which
    // is why it could not be exercised above.
    using GetCapsFn = int32_t (*)(const CameraIndex&, void*);
    auto getCaps = reinterpret_cast<GetCapsFn>(dlsym(
            h, "_ZN5cacao22ProcessCtrlCapsFactory7getCapsERKNS_15ProcessCtrlCaps11CameraIndexEPNS_4CapsE"));
    printf("[probe] getCaps=%p\n", (void*)getCaps);
    fflush(stdout);

    if (getCaps != nullptr) {
        for (int32_t cam = 0; cam <= 1; cam++) {
            TestCaps caps;
            CameraIndex idx = {cam};
            printf("[probe] calling getCaps(camera=%d)...\n", cam);
            fflush(stdout);
            int32_t rc = getCaps(idx, &caps);
            printf("[probe] getCaps(%d) -> %d   [getSerializeSize=%u serialize=%u deserialize=%u]\n",
                   cam, rc, caps.getSerializeSizeCalls, caps.serializeCalls,
                   caps.deserializeCalls);
            fflush(stdout);
        }
    }

    // Exercise IGatewayProcess::start() in isolation. With CACAO_START_TX=1 the library really
    // sends opcode 2, so the daemon's reaction can be watched here rather than inside the camera
    // app. Without it, start() serializes and returns without transacting.
    if (createProcess != nullptr) {
        printf("[probe] createProcess() for the start test...\n");
        fflush(stdout);
        void* gw = createProcess();
        printf("[probe] gateway = %p\n", gw);
        fflush(stdout);
        if (gw != nullptr) {
            TestMode mode;
            auto* g = reinterpret_cast<GatewayMirror*>(gw);
            // Replicate the JNI's ordering exactly: BypassCameraPhoto_changeToPhotoMode calls
            // gateway slot +32 (stop) and then slot +24 (start). Doing it here means a daemon
            // crash costs a probe run rather than the camera app.
            // Theory under test (section 42.1): the daemon may require a real configuration before
            // it will accept a start and survive the capture that follows. Send one first.
            if (getenv("CACAO_TEST_SETCONFIG") != nullptr) {
                TestConfig cfg;
                printf("[probe] calling setConfig() BEFORE stop/start\n");
                fflush(stdout);
                printf("[probe] setConfig -> %d\n", g->setConfig(&cfg));
                fflush(stdout);
            }

            printf("[probe] calling stop()  (the JNI does this first)\n");
            fflush(stdout);
            int32_t src = g->stop();
            printf("[probe] stop -> %d\n", src);
            fflush(stdout);

            printf("[probe] calling start(mode)  [CACAO_START_TX=%s]\n",
                   getenv("CACAO_START_TX") ? getenv("CACAO_START_TX") : "unset");
            fflush(stdout);
            int32_t rc = g->start(&mode);
            printf("[probe] start -> %d\n", rc);
            fflush(stdout);

            // Give the daemon a moment, then prove it is still alive by doing more work through
            // the same session. If stop+start poisons it, this is where it shows.
            usleep(300000);
            printf("[probe] post-start: calling stop() again to exercise the session\n");
            fflush(stdout);
            printf("[probe] stop -> %d\n", g->stop());
            fflush(stdout);
        }
    }

    printf("[probe] done, no fault\n");
    return 0;
}
