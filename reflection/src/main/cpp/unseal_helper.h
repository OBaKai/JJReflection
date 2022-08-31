#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "android_log.h"

typedef union {
    JNIEnv* env;
    void* v_env;
} EnvToVoid;

/**
 * 通过VMRuntime类的方法 进行割免操作
 * 带@libcore.api.CorePlatformApi注解了，domain应该是被修饰成 corePlatform ？？？？
 */
bool api_exemptions_from_VMRuntime(JNIEnv* env) {
    ALOGD("exe api_exemptions_from_VMRuntime.");

    //android11之后 VMRuntime 带@libcore.api.CorePlatformApi注解了，domain应该是被修饰成 corePlatform 了
    jclass vmRuntime_Class = env->FindClass("dalvik/system/VMRuntime");
    if (vmRuntime_Class == nullptr) {
        ALOGE("VMRuntime class not found.");
        env->ExceptionClear();
        return false;
    }

//    jfieldID THE_ONE_field = env->GetStaticFieldID(vmRuntime_Class,
//                                                           "THE_ONE",
//                                                           "Ldalvik/system/VMRuntime;");
//    if (THE_ONE_field == nullptr) {
//        ALOGE("THE_ONE field not found.");
//        return false;
//    }
//    jobject vmRuntime_Object = env->GetStaticObjectField(vmRuntime_Class, THE_ONE_field);
//    if (vmRuntime_Object == nullptr) {
//        ALOGE("VMRuntime object not found.");
//        return false;
//    }

    jmethodID getRuntime_Method = env->GetStaticMethodID(vmRuntime_Class,
                                                   "getRuntime",
                                                   "()Ldalvik/system/VMRuntime;");
    if (getRuntime_Method == nullptr) {
        ALOGE("getRuntime method not found.");
        return false;
    }
    jobject vmRuntime_Object = env->CallStaticObjectMethod(vmRuntime_Class, getRuntime_Method);
    if (vmRuntime_Object == nullptr) {
        ALOGE("VMRuntime object not found.");
        return false;
    }

    jmethodID setHiddenApiExemptions_Method = env->GetMethodID(vmRuntime_Class,
                                                        "setHiddenApiExemptions",
                                                        "([Ljava/lang/String;)V");
    if (setHiddenApiExemptions_Method == nullptr) {
        ALOGE("setHiddenApiExemptions method not found.");
        return false;
    }

    //设置豁免的Hidden Api签名。class的签名都是以L开头的，所以这里全部进行豁免
    jstring freeTargetClass = env->NewStringUTF("L");
    jobjectArray freeTargetArray = env->NewObjectArray(1,
                                                       env->FindClass("java/lang/String"),
                                                       NULL);
    env->SetObjectArrayElement(freeTargetArray, 0, freeTargetClass);

    env->CallVoidMethod(vmRuntime_Object, setHiddenApiExemptions_Method, freeTargetArray);

    env->DeleteLocalRef(freeTargetClass);
    env->DeleteLocalRef(freeTargetArray);
    return true;
}

/**
 * 通过ZygoteInit类的方法 进行割免操作
 */
bool api_exemptions_from_ZygoteInit(JNIEnv* env) {
    ALOGD("exe api_exemptions_from_ZygoteInit.");

    //不知道为什么android12 ZygoteInit#setApiDenylistExemptions 获取会崩溃
    //有空再看看吧，这里直接返回false，然后让VMRuntime进行割免
    int android_api_ = android_get_device_api_level();
    if (android_api_ > __ANDROID_API_R__){
        ALOGI("api over android11，don't exe api_exemptions_from_ZygoteInit.");
        return false;
    }

    //这里使用ZygoteInit里的方法，内部也是调用 VMRuntime#setHiddenApiExemptions
    jclass zygoteInitClass = env->FindClass("com/android/internal/os/ZygoteInit");
    if (zygoteInitClass == nullptr) {
        ALOGE("ZygoteInit class not found.");
        env->ExceptionClear();
        return false;
    }

    //ZygoteInit#setApiBlacklistExemptions（android9-11）
    jmethodID apiExemptionsMethod = env->GetStaticMethodID(zygoteInitClass,
                                                           "setApiBlacklistExemptions",
                                                           "([Ljava/lang/String;)V");
//    if (apiExemptionsMethod == nullptr) {
//        ALOGE("setApiBlacklistExemptions method not found.");
//
//        //ZygoteInit#setApiDenylistExemptions（android12）
//        apiExemptionsMethod = env->GetStaticMethodID(zygoteInitClass,
//                                                     "setApiDenylistExemptions",
//                                                     "([Ljava/lang/String;)V");
//    }

    if (apiExemptionsMethod == nullptr) {
        ALOGE("setApiDenylistExemptions method not found.");
        return false;
    }

    //设置豁免的Hidden Api签名。class的签名都是以L开头的，所以这里全部进行豁免
    jstring freeTargetClass = env->NewStringUTF("L");
    jobjectArray freeTargetArray = env->NewObjectArray(1,
                                                       env->FindClass("java/lang/String"),
                                                       NULL);
    env->SetObjectArrayElement(freeTargetArray, 0, freeTargetClass);

    env->CallStaticVoidMethod(zygoteInitClass, apiExemptionsMethod, freeTargetArray);

    env->DeleteLocalRef(freeTargetClass);
    env->DeleteLocalRef(freeTargetArray);
    return true;
}

bool perform_api_exemptions(JNIEnv* env){
    if (!api_exemptions_from_ZygoteInit(env)){
        return api_exemptions_from_VMRuntime(env);
    }
    return true;
}

/**
 * 在子线程中执行割免
 * @param arg - java_vm
 * @return 是否割免成功
 */
void *thread_unseal_fun(void *arg) {
    JavaVM *java_vm = static_cast<JavaVM *>(arg);
    if (java_vm == nullptr){
        ALOGE("thread_unseal_fun fail, java_vm is NULL.");
        return reinterpret_cast<void *>(false);
    }

    bool isSuccess;

    JNIEnv *env;
    java_vm->AttachCurrentThread(&env, NULL);

    isSuccess = perform_api_exemptions(env);

    java_vm->DetachCurrentThread();

    return reinterpret_cast<void *>(isSuccess);
}

/**
 * 通过切线程进行割免
 */
bool perform_unseal_from_Thread(JNIEnv *env){
    JavaVM *vm;
    env->GetJavaVM(&vm);

    if (vm == nullptr){
        ALOGE("GetJavaVM fail.");
        return false;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, thread_unseal_fun, vm);

    bool isSuccess;
    pthread_join(tid, reinterpret_cast<void **>(&isSuccess));
    ALOGI("perform_unseal_from_Thread isSuccess=%d.", isSuccess);

    return isSuccess;
}

/**
 * 通过JNI_OnLoad进行割免
 */
bool perform_unseal_from_JNI_OnLoad(JavaVM *vm){
    EnvToVoid envToVoid;
    envToVoid.v_env = NULL;
    if (vm->GetEnv(&envToVoid.v_env, JNI_VERSION_1_6) != JNI_OK) {
        ALOGE("GetEnv fail.");
        return false;
    }

    bool isSuccess = perform_api_exemptions(envToVoid.env);
    ALOGI("perform_unseal_from_JNI_OnLoad isSuccess=%d.", isSuccess);

    return isSuccess;
}