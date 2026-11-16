# A16 dex2oat rejects these old Sony/Qualcomm prebuilt framework jars during
# dexpreopt ('Offset should be aligned by 4 for maplist item' — stricter than A15).
# Disable AOT preopt for them; they load interpreted/JIT at runtime (as on A15).
DEXPREOPT_DISABLED_MODULES += com.sonymobile.camera.addon_impl \
    com.fingerprints.extension \
    com.qti.snapdragon.sdk.display \
    com.sonymobile.miscta_impl \
    tcmclient \
    com.qti.dpmframework \
    com.qualcomm.qti.imscmservice-V2.0-java \
    com.qualcomm.qti.imscmservice-V2.1-java \
    com.qualcomm.qti.imscmservice-V2.2-java \
    com.qualcomm.qti.uceservice-V2.0-java \
    com.qualcomm.qti.uceservice-V2.1-java \
    com.qualcomm.qti.uceservice-V2.2-java \
    com.qualcomm.qti.uceservice-V2.3-java \
    com.quicinc.cne.api-V1.0-java \
    com.quicinc.cne.api-V1.1-java \
    com.quicinc.cne.constants-V1.0-java \
    com.quicinc.cne.constants-V2.0-java \
    com.quicinc.cne.constants-V2.1-java \
    dpmapi \
    embmslibrary \
    qcrilhook \
    uimlpalibrary \
    vendor.qti.data.factory-V1.0-java \
    vendor.qti.data.factory-V2.0-java \
    vendor.qti.data.factory-V2.1-java \
    vendor.qti.data.factory-V2.2-java \
    vendor.qti.data.factory-V2.3-java \
    vendor.qti.data.mwqem-V1.0-java \
    vendor.qti.data.slm-V1.0-java \
    vendor.qti.hardware.data.cne.internal.api-V1.0-java \
    vendor.qti.hardware.data.cne.internal.constants-V1.0-java \
    vendor.qti.hardware.data.cne.internal.server-V1.0-java \
    vendor.qti.hardware.data.connection-V1.0-java \
    vendor.qti.hardware.data.connection-V1.1-java \
    vendor.qti.hardware.data.dynamicdds-V1.0-java \
    vendor.qti.hardware.data.iwlan-V1.0-java \
    vendor.qti.hardware.data.latency-V1.0-java \
    vendor.qti.hardware.data.lce-V1.0-java \
    vendor.qti.hardware.data.qmi-V1.0-java \
    vendor.qti.hardware.mwqemadapter-V1.0-java \
    vendor.qti.hardware.slmadapter-V1.0-java \
    vendor.qti.ims.callcapability-V1.0-java \
    vendor.qti.ims.callinfo-V1.0-java \
    vendor.qti.ims.factory-V1.0-java \
    vendor.qti.ims.factory-V1.1-java \
    vendor.qti.ims.rcsconfig-V1.0-java \
    vendor.qti.ims.rcsconfig-V1.1-java \
    vendor.qti.ims.rcsconfig-V2.0-java \
    vendor.qti.ims.rcsconfig-V2.1-java \
    vendor.qti.latency-V2.0-java \
    vendor.qti.latency-V2.1-java
