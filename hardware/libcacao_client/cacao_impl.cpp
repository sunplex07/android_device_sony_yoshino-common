#define LOG_TAG "libcacao_client"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <binder/IMemory.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <log/log.h>

#include "ICacao.h"
#include "include/cacao/cacao_abi.h"

using android::IMemory;
using android::MemoryBase;
using android::MemoryHeapBase;
using android::sp;

namespace cacao {

ObjectBase::ObjectBase() {}
ObjectBase::~ObjectBase() {}
void ObjectBase::dump(unsigned int) {}
const char* ObjectBase::typeName() {
    return "ObjectBase";
}

void* ObjectBase::operator new(size_t size) {
    return ::malloc(size);
}

void ObjectBase::operator delete(void* p) {
    ::free(p);
}

int32_t ProcessFactory::getJpegBufferSize(ImageSize size) {
    sp<android::ICacaoService> svc = android::getCacaoService();
    if (svc == nullptr) {
        return 0;
    }
    if (android::getCacaoServicePid() == static_cast<int32_t>(getpid())) {
        return 0;
    }
    return svc->getJpegBufferSize(size.width, size.height);
}

int32_t ProcessCtrlCapsFactory::getCaps(const ProcessCtrlCaps::CameraIndex& index, Caps* caps) {
    sp<android::ICacaoService> svc = android::getCacaoService();
    if (svc == nullptr) {
        ALOGI("getCaps(camera=%d): no cacao service - returning defaults", index.index);
        return 0;
    }
    if (android::getCacaoServicePid() == static_cast<int32_t>(getpid())) {
        ALOGI("getCaps(camera=%d): caller IS the daemon - returning defaults", index.index);
        return 0;
    }
    ALOGI("getCaps(camera=%d): cacao service found, doing the real round-trip", index.index);

    SerializedData serdata;
    memset(&serdata, 0, sizeof(serdata));

    if (caps == nullptr) {
        ALOGE("getCaps: null Caps");
        return kErrNullCaps;
    }

    uint32_t size = caps->getSerializeSize();

    sp<MemoryHeapBase> heap = new MemoryHeapBase(size, 0, nullptr);
    if (heap->getHeapID() < 0) {
        ALOGE("getCaps: could not allocate a %u-byte shared heap", size);
        return kErrTransactFailed;
    }
    sp<MemoryBase> mem = new MemoryBase(heap, 0, size);

    serdata.memSize = static_cast<int32_t>(mem->size());
    serdata.memPtr = mem->unsecurePointer();

    int32_t rc = caps->serialize(&serdata);
    if (rc < 0) {
        ALOGE("getCaps: Caps::serialize failed: %d", rc);
        return rc;
    }

    SerializedData wire;
    memcpy(&wire, &serdata, sizeof(wire));

    if (wire.numFds < 0 || wire.numFds > static_cast<int32_t>(kMaxSerializedFds)) {
        ALOGE("getCaps: Caps::serialize produced an out-of-range fd count: %d", wire.numFds);
        return kErrTransactFailed;
    }
    rc = svc->allocMemory(index.index, mem, wire);

    if (rc != 0) {
        ALOGE("getCaps: allocMemory failed: %d", rc);
        return rc == kErrTimedOut ? kErrTimedOut : kErrTransactFailed;
    }

    serdata.reservedTail[0] = 0;
    serdata.reservedTail[1] = 0;
    const int32_t drc = caps->deserialize(&serdata);
    ALOGI("getCaps(camera=%d): SERVICE ACCEPTED, deserialize -> %d", index.index, drc);
    return drc;
}

}
