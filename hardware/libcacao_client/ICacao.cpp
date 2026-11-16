#define LOG_TAG "libcacao_client"

#include "ICacao.h"

#include <unistd.h>

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <log/log.h>
#include <utils/Mutex.h>

namespace android {

void writeSerializedData(Parcel* out, const sp<IMemory>& mem, const int32_t* fds, int32_t numFds) {
    out->writeStrongBinder(IInterface::asBinder(mem));
    out->writeInt32(numFds);
    for (int32_t i = 0; i < numFds; i++) {
        out->writeFileDescriptor(fds[i], false );
    }
}

static Mutex gServiceLock;
static sp<ICacaoService> gService;
static int32_t gServicePid = -1;

sp<ICacaoService> getCacaoService() {
    AutoMutex _l(gServiceLock);
    if (gService != nullptr) {
        return gService;
    }
    static const String16 kServiceName("cacao");
    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) {
        return nullptr;
    }
    for (int retry = 0; retry < 9; retry++) {
        sp<IBinder> binder = sm->checkService(kServiceName);
        if (binder != nullptr) {
            gService = interface_cast<ICacaoService>(binder);
            gServicePid = gService->getServicePid();
            return gService;
        }
        ALOGI("trying to get cacao service <retry:%d>", retry);
        usleep(500000);
    }
    ALOGE("cacao service did not appear");
    return nullptr;
}

int32_t getCacaoServicePid() {
    if (getCacaoService() == nullptr) {
        return -1;
    }
    AutoMutex _l(gServiceLock);
    return gServicePid;
}

class BpCacaoService : public BpInterface<ICacaoService> {
public:
    explicit BpCacaoService(const sp<IBinder>& impl) : BpInterface<ICacaoService>(impl) {}

    sp<ICacao> connect(const sp<ICacaoClient>& client) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacaoService::getInterfaceDescriptor());
        data.writeStrongBinder(IInterface::asBinder(client));
        status_t st = remote()->transact(TXN_SVC_CONNECT, data, &reply);
        if (st != NO_ERROR) {
            ALOGE("connect: transact failed: %d", st);
            return nullptr;
        }
        return interface_cast<ICacao>(reply.readStrongBinder());
    }

    int32_t getServicePid() override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacaoService::getInterfaceDescriptor());
        status_t st = remote()->transact(TXN_SVC_GET_SERVICE_PID, data, &reply);
        if (st != NO_ERROR) {
            ALOGE("getServicePid: transact failed: %d", st);
            return -1;
        }
        return reply.readInt32();
    }

    int32_t allocMemory(const int32_t& value, const sp<IMemory>& mem, cacao::SerializedData sd) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacaoService::getInterfaceDescriptor());
        data.writeInt32(value);
        writeSerializedData(&data, mem, sd.fds, sd.numFds);
        status_t st = remote()->transact(TXN_SVC_ALLOC_MEMORY, data, &reply);
        if (st != NO_ERROR) {
            ALOGE("allocMemory: transact failed: %d", st);
            return st;
        }
        return reply.readInt32();
    }

    int32_t getJpegBufferSize(uint32_t width, uint32_t height) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacaoService::getInterfaceDescriptor());
        data.writeInt32(static_cast<int32_t>(width));
        data.writeInt32(static_cast<int32_t>(height));
        status_t st = remote()->transact(TXN_SVC_GET_JPEG_BUFFER_SIZE, data, &reply);
        if (st != NO_ERROR) {
            ALOGE("getJpegBufferSize: transact failed: %d", st);
            return 0;
        }
        return reply.readInt32();
    }
};

static sp<IMemory> readSerializedData(const Parcel& data, cacao::SerializedData* sd) {
    sp<IMemory> mem = interface_cast<IMemory>(data.readStrongBinder());
    memset(sd, 0, sizeof(*sd));
    const int32_t numFds = data.readInt32();
    sd->numFds = numFds;
    for (int32_t i = 0; i < numFds && i < static_cast<int32_t>(cacao::kMaxSerializedFds); i++) {
        sd->fds[i] = data.readFileDescriptor();
    }
    if (mem != nullptr) {
        sd->memSize = static_cast<int32_t>(mem->size());
        sd->memPtr = mem->unsecurePointer();
    }
    return mem;
}

status_t BnCacaoService::onTransact(uint32_t code, const Parcel& data, Parcel* reply,
                                    uint32_t flags) {
    switch (code) {
        case TXN_SVC_CONNECT: {
            CHECK_INTERFACE(ICacaoService, data, reply);
            sp<ICacaoClient> client = interface_cast<ICacaoClient>(data.readStrongBinder());
            reply->writeStrongBinder(IInterface::asBinder(connect(client)));
            return NO_ERROR;
        }
        case TXN_SVC_GET_SERVICE_PID: {
            CHECK_INTERFACE(ICacaoService, data, reply);
            reply->writeInt32(getServicePid());
            return NO_ERROR;
        }
        case TXN_SVC_ALLOC_MEMORY: {
            CHECK_INTERFACE(ICacaoService, data, reply);
            const int32_t value = data.readInt32();
            cacao::SerializedData sd;
            sp<IMemory> mem = readSerializedData(data, &sd);
            reply->writeInt32(allocMemory(value, mem, sd));
            return NO_ERROR;
        }
        case TXN_SVC_GET_JPEG_BUFFER_SIZE: {
            CHECK_INTERFACE(ICacaoService, data, reply);
            const uint32_t width = static_cast<uint32_t>(data.readInt32());
            const uint32_t height = static_cast<uint32_t>(data.readInt32());
            reply->writeInt32(getJpegBufferSize(width, height));
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

class BpCacao : public BpInterface<ICacao> {
public:
    explicit BpCacao(const sp<IBinder>& impl) : BpInterface<ICacao>(impl) {}

    int32_t stop() override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        status_t st = remote()->transact(TXN_CACAO_STOP, data, &reply);
        return st == NO_ERROR ? 0 : st;
    }

    int32_t setConfig(const sp<IMemory>& mem, cacao::SerializedData sd) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        writeSerializedData(&data, mem, sd.fds, sd.numFds);
        status_t st = remote()->transact(TXN_CACAO_START, data, &reply);
        if (st != NO_ERROR) return st;
        return reply.readInt32();
    }

    int32_t disconnect() override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        status_t st = remote()->transact(TXN_CACAO_DISCONNECT, data, &reply);
        if (st != NO_ERROR) return st;
        return reply.readInt32();
    }

    int32_t getConfig(int32_t id, const sp<IMemory>& mem, cacao::SerializedData sd) override {
        (void)id;
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        writeSerializedData(&data, mem, sd.fds, sd.numFds);
        status_t st = remote()->transact(TXN_CACAO_GET_CONFIG, data, &reply);
        if (st != NO_ERROR) return st;
        return reply.readInt32();
    }

    int32_t process(int32_t token, const sp<IMemory>& paramMem,
                    cacao::SerializedData sd) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        data.writeInt32(token);
        writeSerializedData(&data, paramMem, sd.fds, sd.numFds);
        status_t st = remote()->transact(TXN_CACAO_PROCESS, data, &reply);
        if (st != NO_ERROR) return st;
        return reply.readInt32();
    }

    intptr_t processAsync(const sp<IMemory>& paramMem, cacao::SerializedData paramSd,
                          Vector<cacao::ImageBuf*> images, const sp<IMemory>& resultMem,
                          cacao::SerializedData resultSd, int32_t token,
                          const sp<ICacaoProcessCallback>& callback) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        writeSerializedData(&data, paramMem, paramSd.fds, paramSd.numFds);
        data.writeInt32(static_cast<int32_t>(images.size()));
        for (size_t i = 0; i < images.size(); i++) {
            cacao::ImageBuf* b = images[i];
            if (b == nullptr) continue;
            const cacao::ImageSize sz = b->getImageSize();
            data.writeUint32(sz.width);
            data.writeUint32(sz.height);
            data.writeInt32(static_cast<int32_t>(b->getDataSpace()));
            data.writeNativeHandle(static_cast<const native_handle_t*>(b->mHandle));
        }
        writeSerializedData(&data, resultMem, resultSd.fds, resultSd.numFds);
        data.writeInt32(token);
        data.writeStrongBinder(IInterface::asBinder(callback));
        status_t st = remote()->transact(TXN_CACAO_PROCESS_ASYNC, data, &reply);
        if (st != NO_ERROR) {
            ALOGE("processAsync: transact failed: %d", st);
            return 0;
        }
        return static_cast<intptr_t>(reply.readInt64());
    }

    intptr_t processAsyncWithBinder(const sp<IMemory>& paramMem, cacao::SerializedData paramSd,
                                    const sp<IBinder>& extra, const sp<IMemory>& resultMem,
                                    cacao::SerializedData resultSd, int32_t token,
                                    const sp<ICacaoProcessCallback>& callback) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        writeSerializedData(&data, paramMem, paramSd.fds, paramSd.numFds);
        data.writeStrongBinder(extra);
        writeSerializedData(&data, resultMem, resultSd.fds, resultSd.numFds);
        data.writeInt32(token);
        data.writeStrongBinder(IInterface::asBinder(callback));
        status_t st = remote()->transact(TXN_CACAO_PROCESS_ASYNC_WITH_BINDER, data, &reply);
        if (st != NO_ERROR) {
            ALOGE("processAsyncWithBinder: transact failed: %d", st);
            return 0;
        }
        return static_cast<intptr_t>(reply.readInt64());
    }

    int32_t cancel(intptr_t id) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacao::getInterfaceDescriptor());
        data.writeInt64(static_cast<int64_t>(id));
        status_t st = remote()->transact(TXN_CACAO_CANCEL, data, &reply);
        if (st != NO_ERROR) return st;
        return reply.readInt32();
    }
};

class BnCacaoClientBase : public BnInterface<ICacaoClient> {
public:
    status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) override {
        switch (code) {
            case TXN_CLIENT_NOTIFY_EVENT: {
                CHECK_INTERFACE(ICacaoClient, data, reply);
                int32_t eventId = data.readInt32();
                sp<IMemory> mem = interface_cast<IMemory>(data.readStrongBinder());
                int32_t numFds = data.readInt32();
                for (int32_t i = 0; i < numFds; i++) {
                    (void)data.readFileDescriptor();
                }
                notifyEvent(eventId, mem);
                return NO_ERROR;
            }
            default:
                return BBinder::onTransact(code, data, reply, flags);
        }
    }
};

class BpCacaoClient : public BpInterface<ICacaoClient> {
public:
    explicit BpCacaoClient(const sp<IBinder>& impl) : BpInterface<ICacaoClient>(impl) {}

    void notifyEvent(int32_t eventId, const sp<IMemory>& mem) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacaoClient::getInterfaceDescriptor());
        data.writeInt32(eventId);
        writeSerializedData(&data, mem, nullptr, 0);
        remote()->transact(TXN_CLIENT_NOTIFY_EVENT, data, &reply, IBinder::FLAG_ONEWAY);
    }
};

class BpCacaoProcessCallback : public BpInterface<ICacaoProcessCallback> {
public:
    explicit BpCacaoProcessCallback(const sp<IBinder>& impl)
          : BpInterface<ICacaoProcessCallback>(impl) {}

    void notifyProgress(const sp<IMemory>& mem) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacaoProcessCallback::getInterfaceDescriptor());
        writeSerializedData(&data, mem, nullptr, 0);
        remote()->transact(TXN_CB_NOTIFY_PROGRESS, data, &reply, IBinder::FLAG_ONEWAY);
    }

    void notifyResult(const sp<IMemory>& mem) override {
        Parcel data, reply;
        data.writeInterfaceToken(ICacaoProcessCallback::getInterfaceDescriptor());
        writeSerializedData(&data, mem, nullptr, 0);
        remote()->transact(TXN_CB_NOTIFY_RESULT, data, &reply, IBinder::FLAG_ONEWAY);
    }
};

status_t BnCacao::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) {
    switch (code) {
        case TXN_CACAO_DISCONNECT: {
            CHECK_INTERFACE(ICacao, data, reply);
            disconnect();
            return NO_ERROR;
        }
        case TXN_CACAO_STOP: {
            CHECK_INTERFACE(ICacao, data, reply);
            stop();
            return NO_ERROR;
        }
        case TXN_CACAO_START: {
            CHECK_INTERFACE(ICacao, data, reply);
            cacao::SerializedData sd;
            sp<IMemory> mem = readSerializedData(data, &sd);
            setConfig(mem, sd);
            return NO_ERROR;
        }
        case TXN_CACAO_PROCESS: {
            CHECK_INTERFACE(ICacao, data, reply);
            const int32_t token = data.readInt32();
            cacao::SerializedData sd;
            sp<IMemory> mem = readSerializedData(data, &sd);
            reply->writeInt32(process(token, mem, sd));
            return NO_ERROR;
        }
        case TXN_CACAO_GET_CONFIG: {
            CHECK_INTERFACE(ICacao, data, reply);
            const int32_t id = data.readInt32();
            cacao::SerializedData sd;
            sp<IMemory> mem = readSerializedData(data, &sd);
            reply->writeInt32(getConfig(id, mem, sd));
            return NO_ERROR;
        }
        case TXN_CACAO_PROCESS_ASYNC: {
            CHECK_INTERFACE(ICacao, data, reply);
            cacao::SerializedData paramSd;
            sp<IMemory> paramMem = readSerializedData(data, &paramSd);

            Vector<cacao::ImageBuf*> images;
            const int32_t numBufs = data.readInt32();
            for (int32_t i = 0; i < numBufs; i++) {
                cacao::ImageSize sz;
                sz.width = data.readUint32();
                sz.height = data.readUint32();
                const int32_t dataSpace = data.readInt32();
                native_handle_t* h = data.readNativeHandle();
                images.add(new cacao::ImageBuf(h, sz, static_cast<cacao::DataSpace>(dataSpace),
                                               static_cast<cacao::ImageFormat>(0)));
            }

            cacao::SerializedData resultSd;
            sp<IMemory> resultMem = readSerializedData(data, &resultSd);
            const int32_t token = data.readInt32();
            sp<ICacaoProcessCallback> cb =
                    interface_cast<ICacaoProcessCallback>(data.readStrongBinder());
            const intptr_t cookie =
                    processAsync(paramMem, paramSd, images, resultMem, resultSd, token, cb);
            reply->writeInt64(static_cast<int64_t>(cookie));
            return NO_ERROR;
        }
        case TXN_CACAO_PROCESS_ASYNC_WITH_BINDER: {
            CHECK_INTERFACE(ICacao, data, reply);
            cacao::SerializedData paramSd;
            sp<IMemory> paramMem = readSerializedData(data, &paramSd);
            sp<IBinder> extra = data.readStrongBinder();
            cacao::SerializedData resultSd;
            sp<IMemory> resultMem = readSerializedData(data, &resultSd);
            const int32_t token = data.readInt32();
            sp<ICacaoProcessCallback> cb =
                    interface_cast<ICacaoProcessCallback>(data.readStrongBinder());
            const intptr_t cookie = processAsyncWithBinder(paramMem, paramSd, extra, resultMem,
                                                           resultSd, token, cb);
            reply->writeInt64(static_cast<int64_t>(cookie));
            return NO_ERROR;
        }
        case TXN_CACAO_CANCEL: {
            CHECK_INTERFACE(ICacao, data, reply);
            const intptr_t id = static_cast<intptr_t>(data.readInt64());
            reply->writeInt32(cancel(id));
            return NO_ERROR;
        }
        default:
            return BBinder::onTransact(code, data, reply, flags);
    }
}

IMPLEMENT_META_INTERFACE(CacaoService, "com.sonymobile.cacao.ICacaoService")
IMPLEMENT_META_INTERFACE(Cacao, "com.sonymobile.cacao.ICacao")
IMPLEMENT_META_INTERFACE(CacaoClient, "com.sonymobile.cacao.ICacaoClient")
IMPLEMENT_META_INTERFACE(CacaoProcessCallback, "com.sonymobile.cacao.ICacaoProcessCallback")

}
