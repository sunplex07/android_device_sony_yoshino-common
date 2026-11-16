#define LOG_TAG "libcacao_client"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <vector>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <binder/ProcessState.h>
#include <cutils/properties.h>
#include <log/log.h>
#include <utils/Mutex.h>

#include "ICacao.h"
#include "include/cacao/cacao_abi.h"

namespace android {
namespace {

constexpr int32_t kOk = 0;
constexpr int32_t kErrNoInit = -101;
constexpr int32_t kErrAllocFailed = -102;
constexpr int32_t kErrBadValue = -103;
constexpr int32_t kErrAlreadyExists = -107;
constexpr int32_t kErrTimedOut = -110;
constexpr int32_t kErrFailed = -111;

inline int32_t mapTransportStatus(int32_t rc) {
    return (rc == kErrTimedOut) ? kErrFailed : (rc != 0 ? kErrFailed : kOk);
}

bool allocSharedBuffer(uint32_t size, sp<IMemory>* out, cacao::SerializedData* sd) {
    if (size == 0) {
        return false;
    }
    sp<MemoryHeapBase> heap = new MemoryHeapBase(size, 0, nullptr);
    if (heap->getHeapID() < 0) {
        ALOGE("could not allocate a %u-byte shared heap", size);
        return false;
    }
    sp<MemoryBase> mem = new MemoryBase(heap, 0, size);
    memset(sd, 0, sizeof(*sd));
    sd->memSize = static_cast<int32_t>(mem->size());
    sd->memPtr = mem->unsecurePointer();
    *out = mem;
    return true;
}

}

class CacaoProcessCallback : public BnInterface<ICacaoProcessCallback> {
public:
    CacaoProcessCallback(cacao::ICBProcessResultListener* listener, cacao::ProcessResultBase* result)
          : mListener(listener), mResult(result) {}

    void notifyProgress(const sp<IMemory>&) override {}
    void notifyResult(const sp<IMemory>&) override {}

    status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) override {
        switch (code) {
            case TXN_CB_NOTIFY_PROGRESS:
            case TXN_CB_NOTIFY_RESULT: {
                CHECK_INTERFACE(ICacaoProcessCallback, data, reply);
                sp<IMemory> mem = interface_cast<IMemory>(data.readStrongBinder());
                cacao::SerializedData sd;
                memset(&sd, 0, sizeof(sd));
                const int32_t numFds = data.readInt32();
                sd.numFds = numFds;
                for (int32_t i = 0; i < numFds && i < static_cast<int32_t>(cacao::kMaxSerializedFds);
                     i++) {
                    sd.fds[i] = data.readFileDescriptor();
                }
                if (mem != nullptr) {
                    if (property_get_bool("cacao.cb_skip_mem", false)) {
                        ALOGW("callback: skipping the IMemory round trip (cacao.cb_skip_mem=1)");
                    } else {
                        sd.memSize = static_cast<int32_t>(mem->size());
                        sd.memPtr = mem->unsecurePointer();
                    }
                }
                deliver(code == TXN_CB_NOTIFY_RESULT, &sd);
                return NO_ERROR;
            }
            default:
                return BBinder::onTransact(code, data, reply, flags);
        }
    }

private:
    void deliver(bool isResult, cacao::SerializedData* sd) {
        if (mResult != nullptr) {
            mResult->deserialize(sd);
        }
        if (mListener == nullptr) {
            return;
        }
        if (isResult) {
            mListener->onResult(mResult);
        } else {
            mListener->onProgress(mResult);
        }
    }

    cacao::ICBProcessResultListener* mListener;
    cacao::ProcessResultBase* mResult;
};

class CacaoSession : public BnInterface<ICacaoClient>, public IBinder::DeathRecipient {
public:
    CacaoSession() = default;

    int32_t connect(const sp<ICacaoService>& service) {
        AutoMutex _l(mLock);
        if (service == nullptr) {
            return kErrBadValue;
        }
        if (mCacao != nullptr) {
            return kErrAlreadyExists;
        }
        mCacao = service->connect(this);
        if (mCacao == nullptr) {
            ALOGE("connect: service returned no session");
            return kErrFailed;
        }
        sp<IBinder> binder = IInterface::asBinder(mCacao);
        if (binder != nullptr) {
            binder->linkToDeath(this);
        }
        ALOGI("cacao session established");
        return kOk;
    }

    int32_t disconnect() {
        sp<ICacao> session;
        {
            AutoMutex _l(mLock);
            if (mCacao == nullptr) {
                return kErrNoInit;
            }
            session = mCacao;
            mCacao = nullptr;
        }
        const int32_t rc = session->disconnect();
        sp<IBinder> binder = IInterface::asBinder(session);
        if (binder != nullptr) {
            binder->unlinkToDeath(this);
        }
        return mapTransportStatus(rc);
    }

    void binderDied(const wp<IBinder>&) override {
        ALOGW("cacao service died; tearing the session down");
        disconnect();
    }

    void notifyEvent(int32_t eventId, const sp<IMemory>& mem) override {
        (void)mem;
        AutoMutex _l(mLock);
        ALOGI("notifyEvent(%d)%s", eventId,
              mEventListener != nullptr ? "" : " (no listener registered)");
    }

    int32_t subscribeEvents(cacao::ICBProcessEventListener* listener, void* userData) {
        AutoMutex _l(mLock);
        mEventListener = listener;
        mEventUserData = userData;
        return kOk;
    }

    int32_t stop() {
        AutoMutex _l(mLock);
        if (mCacao == nullptr) {
            return kErrNoInit;
        }
        return mapTransportStatus(mCacao->stop());
    }

    int32_t start(const cacao::ProcessModeBase* mode) {
        AutoMutex _l(mLock);
        if (mCacao == nullptr) {
            return kErrNoInit;
        }
        if (mode == nullptr) {
            return kErrBadValue;
        }
        cacao::ProcessModeBase* m = const_cast<cacao::ProcessModeBase*>(mode);
        cacao::SerializedData sd;
        sp<IMemory> mem;
        const uint32_t modeSize = m->getSerializeSize();
        ALOGI("start: ProcessModeBase::getSerializeSize() [slot+40] -> %u", modeSize);
        if (!allocSharedBuffer(modeSize, &mem, &sd)) {
            ALOGW("start: could not allocate %u bytes for the mode", modeSize);
            return kErrAllocFailed;
        }
        const int32_t rc = m->serialize(&sd);
        ALOGI("start: ProcessModeBase::serialize() [slot+48] -> %d (numFds=%d)", rc, sd.numFds);
        if (rc < 0) {
            return rc;
        }
        const bool kTransmit = property_get_bool("cacao.start_tx", true);
        if (!kTransmit) {
            ALOGW("start: mode serialized, transmit disabled by cacao.start_tx=0");
            return kOk;
        }
        ALOGI("start: TRANSMITTING opcode 2 with a %u-byte mode...", modeSize);
        const int32_t txrc = mCacao->setConfig(mem, sd);
        ALOGI("start: opcode 2 returned %d", txrc);
        return mapTransportStatus(txrc);
    }

    int32_t setConfig(const cacao::ProcessConfigBase* config) {
        AutoMutex _l(mLock);
        if (mCacao == nullptr) {
            return kErrNoInit;
        }
        if (config == nullptr) {
            return kErrBadValue;
        }
        cacao::ProcessConfigBase* c = const_cast<cacao::ProcessConfigBase*>(config);
        cacao::SerializedData sd;
        sp<IMemory> mem;
        if (!allocSharedBuffer(c->getSerializeSize(), &mem, &sd)) {
            return kErrAllocFailed;
        }
        const int32_t rc = c->serialize(&sd);
        if (rc < 0) {
            return rc;
        }
        (void)c->getType();
        return mapTransportStatus(mCacao->setConfig(mem, sd));
    }

    int32_t getConfig(cacao::ProcessConfigBase* config) {
        AutoMutex _l(mLock);
        if (mCacao == nullptr) {
            return kErrNoInit;
        }
        if (config == nullptr) {
            return kErrBadValue;
        }
        cacao::SerializedData sd;
        sp<IMemory> mem;
        if (!allocSharedBuffer(config->getSerializeSize(), &mem, &sd)) {
            return kErrAllocFailed;
        }
        int32_t rc = config->serialize(&sd);
        if (rc < 0) {
            return rc;
        }
        const int32_t type = static_cast<int32_t>(config->getType());
        rc = mCacao->getConfig(type, mem, sd);
        if (rc != 0) {
            return mapTransportStatus(rc);
        }
        return config->deserialize(&sd);
    }

    int32_t cancel(void* id) {
        AutoMutex _l(mLock);
        if (mCacao == nullptr) {
            return kErrNoInit;
        }
        return mapTransportStatus(mCacao->cancel(reinterpret_cast<intptr_t>(id)));
    }

    int32_t process(const cacao::ProcessParamBase* param, cacao::ICBProcessResultListener* listener,
                    cacao::ProcessResultBase* result) {
        AutoMutex _l(mLock);
        if (mCacao == nullptr) {
            return kErrNoInit;
        }
        if (param == nullptr || result == nullptr) {
            return kErrBadValue;
        }
        cacao::ProcessParamBase* p = const_cast<cacao::ProcessParamBase*>(param);

        cacao::SerializedData paramSd, resultSd;
        sp<IMemory> paramMem, resultMem;
        if (!allocSharedBuffer(p->getSerializeSize(), &paramMem, &paramSd)) {
            return kErrAllocFailed;
        }
        int32_t rc = p->serialize(&paramSd);
        if (rc < 0) {
            return rc;
        }
        if (!allocSharedBuffer(result->getSerializeSize(), &resultMem, &resultSd)) {
            return kErrAllocFailed;
        }
        rc = result->serialize(&resultSd);
        if (rc < 0) {
            return rc;
        }
        ALOGI("process: param=%d result=%d bytes", paramSd.memSize, resultSd.memSize);
        sp<CacaoProcessCallback> cb = new CacaoProcessCallback(listener, result);
        rc = mCacao->process(1, paramMem, paramSd);
        return mapTransportStatus(rc);
    }

    intptr_t processAsync(const cacao::ProcessParamBase* param, Vector<cacao::ImageBuf*>* bufs,
                          cacao::ICBProcessResultListener* listener,
                          cacao::ProcessResultBase* result) {
        AutoMutex _l(mLock);
        if (mCacao == nullptr || param == nullptr || result == nullptr || listener == nullptr) {
            return 0;
        }
        cacao::ProcessParamBase* p = const_cast<cacao::ProcessParamBase*>(param);

        cacao::SerializedData paramSd, resultSd;
        sp<IMemory> paramMem, resultMem;
        if (!allocSharedBuffer(p->getSerializeSize(), &paramMem, &paramSd)) {
            return 0;
        }
        if (p->serialize(&paramSd) < 0) {
            return 0;
        }
        if (!allocSharedBuffer(result->getSerializeSize(), &resultMem, &resultSd)) {
            return 0;
        }
        if (result->serialize(&resultSd) < 0) {
            return 0;
        }

        if (bufs != nullptr) {
            const uintptr_t* w = reinterpret_cast<const uintptr_t*>(bufs);
            ALOGI("processAsync: vector=%p  vptr=%p mStorage=%p mCount=%zu mFlags=%zu itemSize=%zu"
                  "  | size()=%zu",
                  bufs, (void*)w[0], (void*)w[1], (size_t)w[2], (size_t)w[3], (size_t)w[4],
                  bufs->size());
        } else {
            ALOGI("processAsync: vector is NULL (Sony allows this)");
        }
        Vector<cacao::ImageBuf*> images;
        if (bufs != nullptr) {
            for (size_t i = 0; i < bufs->size(); i++) {
                if ((*bufs)[i] != nullptr) images.add((*bufs)[i]);
            }
        }

        ALOGI("processAsync: %zu image buffers, param=%u result=%u bytes", images.size(),
              paramSd.memSize, resultSd.memSize);
        sp<CacaoProcessCallback> cb = new CacaoProcessCallback(listener, result);
        mPendingCallbacks.push_back(cb);
        return mCacao->processAsync(paramMem, paramSd, images, resultMem, resultSd, 0, cb);
    }

    intptr_t processAsyncWithBinder(const cacao::ProcessParamBase* param, const sp<IBinder>& extra,
                                    cacao::ICBProcessResultListener* listener,
                                    cacao::ProcessResultBase* result) {
        AutoMutex _l(mLock);
        if (mCacao == nullptr || param == nullptr || result == nullptr || listener == nullptr) {
            return 0;
        }
        cacao::ProcessParamBase* p = const_cast<cacao::ProcessParamBase*>(param);

        cacao::SerializedData paramSd, resultSd;
        sp<IMemory> paramMem, resultMem;
        if (!allocSharedBuffer(p->getSerializeSize(), &paramMem, &paramSd)) return 0;
        if (p->serialize(&paramSd) < 0) return 0;
        if (!allocSharedBuffer(result->getSerializeSize(), &resultMem, &resultSd)) return 0;
        if (result->serialize(&resultSd) < 0) return 0;

        sp<CacaoProcessCallback> cb = new CacaoProcessCallback(listener, result);
        mPendingCallbacks.push_back(cb);
        return mCacao->processAsyncWithBinder(paramMem, paramSd, extra, resultMem, resultSd, 0,
                                              cb);
    }

private:
    Mutex mLock;
    sp<ICacao> mCacao;
    cacao::ICBProcessEventListener* mEventListener = nullptr;
    void* mEventUserData = nullptr;
    std::vector<sp<CacaoProcessCallback>> mPendingCallbacks;
};

class Cacao : public cacao::IGatewayProcess {
public:
    ~Cacao() override {
        if (mSession != nullptr) {
            mSession->disconnect();
        }
    }

    int32_t connect() {
        mSession = new CacaoSession();
        sp<ICacaoService> svc = getCacaoService();
        const int32_t rc = mSession->connect(svc);
        if (rc < 0) {
            mSession->disconnect();
            mSession = nullptr;
        }
        return rc;
    }

    int32_t subscribeEvents(cacao::ICBProcessEventListener* l, void* u) override {
        return mSession != nullptr ? mSession->subscribeEvents(l, u) : kErrNoInit;
    }
    int32_t start(const cacao::ProcessModeBase* m) override {
        return mSession != nullptr ? mSession->start(m) : kErrNoInit;
    }
    int32_t stop() override {
        return mSession != nullptr ? mSession->stop() : kErrNoInit;
    }
    int32_t setConfig(const cacao::ProcessConfigBase* c) override {
        return mSession != nullptr ? mSession->setConfig(c) : kErrNoInit;
    }
    int32_t getConfig(cacao::ProcessConfigBase* c) override {
        return mSession != nullptr ? mSession->getConfig(c) : kErrNoInit;
    }
    int32_t cancel(void* id) override {
        return mSession != nullptr ? mSession->cancel(id) : kErrNoInit;
    }

    int32_t process(const cacao::ProcessParamBase* p, cacao::ICBProcessResultListener* l,
                    cacao::ProcessResultBase* r) override {
        return mSession != nullptr ? mSession->process(p, l, r) : kErrNoInit;
    }
    int32_t processAsync(const cacao::ProcessParamBase* p, Vector<cacao::ImageBuf*>* v,
                         cacao::ICBProcessResultListener* l,
                         cacao::ProcessResultBase* r) override {
        return mSession != nullptr
                       ? static_cast<int32_t>(mSession->processAsync(p, v, l, r))
                       : 0;
    }
    int32_t processAsyncWithBinder(const cacao::ProcessParamBase* p, const sp<IBinder>& b,
                                   cacao::ICBProcessResultListener* l,
                                   cacao::ProcessResultBase* r) override {
        return mSession != nullptr
                       ? static_cast<int32_t>(mSession->processAsyncWithBinder(p, b, l, r))
                       : 0;
    }

private:
    sp<CacaoSession> mSession;
};

cacao::IGatewayProcess* createCacao() {
    ProcessState::self()->startThreadPool();

    sp<ICacaoService> svc = getCacaoService();
    if (svc == nullptr) {
        ALOGI("createProcess: no cacao service");
        return nullptr;
    }
    if (getCacaoServicePid() == static_cast<int32_t>(getpid())) {
        ALOGI("createProcess: caller IS the daemon");
        return nullptr;
    }

    Cacao* obj = new Cacao();
    if (obj->connect() < 0) {
        delete obj;
        return nullptr;
    }
    return obj;
}

}

namespace cacao {

IGatewayProcess::~IGatewayProcess() {}

IGatewayProcess* ProcessFactory::createProcess() {
    return android::createCacao();
}

}
