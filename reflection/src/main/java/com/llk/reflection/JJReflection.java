package com.llk.reflection;

import android.os.Build;

/**
 * JJReflection - 主要作用就在android9.0之后 突破 Hidden Api 限制
 * 支持android9-12
 * 采用策略：System.loadLibrary + native线程 两种方式
 * 优先使用System.loadLibrary方式，如果失败了则追加使用native线程方式
 *
 * 参考 - FreeReflection  https://github.com/tiann/FreeReflection
 *      1、修改 hidden_api_policy_（android9、10）- 详情可看 art.h art.cpp
 *      2、元反射（android9、10）
 *          通过反射 getDeclaredMethod 方法，getDeclaredMethod 是 public 的，不存在问题。
 *          反射调用 getDeclardMethod是以系统身份去反射的系统类，因此元反射方法也被系统类加载的方法；所以我们的元反射方法调用的 getDeclardMethod 会被认为是系统调用的，可以反射任意的方法
 *      3、DexFile设置父ClassLoader为null（BootClassLoader，它用于加载一些Android系统框架的类，如果用BootClassLoader它来加载domain肯定很低）的方法进行限制绕过（android11）
 *
 * 参考 [安卓hiddenapi访问绝技] https://bbs.pediy.com/thread-268936.htm#msg_header_h2_10
 *      依靠System.loadLibrary进行 setHiddenApiExemptions（android9、10、11）
 *          System.loadLibrary（java.lang.System 类在 /apex/com.android.runtime/javalib/core-oj.jar 里边），所以System.loadLibrary的调用栈的domain很低的
 *          而在System.loadLibrary中最终会调到JNI_OnLoad函数，所以可以在JNI_OnLoad函数里边进行限制绕过
 *          可以通过 cat proc/pid/maps |grep ".jar" 查看jar位置
 *  参考 [Android 11 绕过反射限制] https://www.jianshu.com/p/6546ce67c8e0
 *      native启动子线程进行突破
 */
public class JJReflection {

    private static native boolean unsealFromThread();

    /**
     * 突破 Hidden Api 限制
     * @return true：成功突破
     */
    public static boolean apiExemptions() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return true;
        }

        System.loadLibrary("jj-reflection");

        return unsealFromThread();
     }
}