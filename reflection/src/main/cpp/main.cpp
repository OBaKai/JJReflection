#include <jni.h>
#include "android_log.h"
#include "unseal_helper.h"

bool is_unseal_in_JNI_OnLoad;

jint JNI_OnLoad(JavaVM *vm, void* reserved) {
    jint result = -1;

    ALOGD("===========JNI_OnLoad===========");

    if (!perform_unseal_from_JNI_OnLoad(vm)) {
        goto bail;
    } else{
        is_unseal_in_JNI_OnLoad = true;
    }
    result = JNI_VERSION_1_6;

    bail:
    return result;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_com_llk_reflection_JJReflection_unsealFromThread(JNIEnv *env, jclass clazz) {
    bool isSuccess;
    if (!is_unseal_in_JNI_OnLoad){
        isSuccess = perform_unseal_from_Thread(env);
    } else{
        isSuccess = true;

    }

    return isSuccess;
}