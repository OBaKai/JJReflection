# JJReflection - 主要作用就在android9.0之后 突破 Hidden Api 限制
 * 支持android9-12
 * 采用策略：System.loadLibrary + native线程 两种方式
 * 优先使用System.loadLibrary方式，如果失败了则追加使用native线程方式

## 依赖
```groovy
   allprojects {
	repositories {
		maven { url 'https://www.jitpack.io' }
	}
   }	

    dependencies {
        implementation 'com.github.OBaKai:JJReflection:1.1'
    }
```

## 使用
```java
    JJReflection.apiExemptions();
```


# Hidden Api限制分析
## Android9+

### 原因

反射的时候增加了方法签名校验机制，如果该方法签名不在 割免列表 中，都会被拒绝访问。（HiddenApi都不在割免列表中）

### 解决方法

将方法签名加入到 割免列表 就能突破限制了。（class的签名都是以L开头的，将L加入到豁免列表，就能所有起飞了。）

**元反射：**

通过反射 getDeclaredMethod 方法，getDeclaredMethod 是 public 的，不存在问题。
反射调用 getDeclardMethod 是以系统身份去反射的系统类，因此元反射方法也被系统类加载的方法；所以我们的元反射方法调用的 getDeclardMethod 会被认为是系统调用的，可以反射任意的方法



### 源码分析

```c
//调用链：
java_lang_Class.cc#Class_getDeclaredMethodInternal 
	-> ShouldBlockAccessToMember
	-> hidden_api.h#GetMemberAction
	-> hidden_api.cc#GetMemberActionImpl

//分析：
Class_getDeclaredMethodInternal：//主要 ShouldBlockAccessToMember 函数返回false，就万事大吉了
	if (result == nullptr || ShouldBlockAccessToMember(result->GetArtMethod(), soa.Self())) {
    	return nullptr;
  	}
  	return soa.AddLocalReference<jobject>(result.Get());


ShouldBlockAccessToMember: //action别是kDeny就行了
	hiddenapi::Action action = hiddenapi::GetMemberAction(member, self, IsCallerTrusted, hiddenapi::kReflection);
  	return action == hiddenapi::kDeny;


GetMemberActionImpl:
	Runtime* runtime = Runtime::Current();
	//GetHiddenApiExemptions：获取豁免的Hidden Api签名。（art/runtime/runtime.h -> dalvik_system_VMRuntime.cc ，VMRuntime_setHiddenApiExemptions 该jni方法，其native函数的声明在VMRuntime.java中）
	//IsExempted：判断该方法签名是否在 豁免列表中。
	if (member_signature.IsExempted(runtime->GetHiddenApiExemptions())) {
	      action = kAllow;
	      MaybeWhitelistMember(runtime, member);
	      return kAllow;
    }

bool MemberSignature::IsExempted(const std::vector<std::string>& exemptions) {
  for (const std::string& exemption : exemptions) {
    if (DoesPrefixMatch(exemption)) {
      return true;
    }
  }
  return false;
}

bool MemberSignature::DoesPrefixMatch(const std::string& prefix) const {
  size_t pos = 0;
  for (const char* part : GetSignatureParts()) {
    size_t count = std::min(prefix.length() - pos, strlen(part));
    if (prefix.compare(pos, count, part, 0, count) == 0) {
      pos += count;
    } else {
      return false;
    }
  }
  return pos == prefix.length();
}
```



## android11+ 

### 原因

元反射失效了，因为增加了 调用者上下文判断 机制。机制给 调用者 跟 被调用的方法 增加了domian，调用者domain要等于或者小于 被调用的方法domian，才能允许访问。

### 解决方法

1、寻找domain小的调用栈，在那里进行 割免列表 的添加。

System.loadLibrary（java.lang.System 类在 /apex/com.android.runtime/javalib/core-oj.jar 里边），所以System.loadLibrary的调用栈的domain很低的。而在System.loadLibrary中最终会调到JNI_OnLoad函数，所以可以在JNI_OnLoad函数里边进行割免操作。

参考：https://bbs.pediy.com/thread-268936.htm



2、让系统无法识别调用栈，系统就会给一个最小的domain我们。

在native层创建一个新线程，然后获取该子线程的JniEnv，通过该JniEnv来进行割免操作。

参考：https://www.jianshu.com/p/6546ce67c8e0



3、利用BootClassLoader -- FreeReflection库（https://github.com/tiann/FreeReflection）

DexFile设置父ClassLoader为null（BootClassLoader，它用于加载一些Android系统框架的类，如果用BootClassLoader它来加载domain肯定很低），然后再进行割免操作。

(个人觉得，直接用还是可以的。对其进行改造确实麻烦，还得生成dex然后再转base64啥的。)



### 源码分析

```c
//调用链：
java_lang_Class.cc#Class_getDeclaredMethodInternal 
	-> ShouldDenyAccessToMember 
	-> hidden_api.h#ShouldDenyAccessToMember
	-> hidden_api.cc#ShouldDenyAccessToMemberImpl

//分析：
Class_getDeclaredMethodInternal：//主要 ShouldDenyAccessToMember 函数返回false，就万事大吉了
	 ...
   if (result == nullptr || ShouldDenyAccessToMember(result->GetArtMethod(), soa.Self())) {
    	return nullptr;
  	}
  	return soa.AddLocalReference<jobject>(result.Get());


enum class Domain : char { //Domain范围
	kCorePlatform = 0, // 0
	kPlatform,  	   // 1 
	kApplication, 	   // 2
};


ShouldDenyAccessToMember：
  ...
	//caller上下文：调用者的上下文
	//fn_get_access_context() -> GetHiddenapiAccessContextFunction -> GetReflectionCaller
	const AccessContext caller_context = fn_get_access_context(); 
	//callee上下文：被调用的这个方法的上下文
	const AccessContext callee_context(member->GetDeclaringClass());

	//调用者是否允许调用该方法，返回true就证明允许调用。也就是说Domain越小越牛逼
	//CanAlwaysAccess函数：return callerDomain <= calleeDomain
	if (caller_context.CanAlwaysAccess(callee_context)) { 
	    return false; //返回false，Class_getDeclaredMethodInternal才能继续执行。
	}


GetReflectionCaller：
	static hiddenapi::AccessContext GetReflectionCaller(Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Walk the stack and find the first frame not from java.lang.Class,
  // java.lang.invoke or java.lang.reflect. This is very expensive.
  // Save this till the last.
  //翻译：遍历堆栈并找到不是来自 java.lang.Class、java.lang.invoke 或 java.lang.reflect 的第一帧。这是非常昂贵的。保存到最后。
  struct FirstExternalCallerVisitor : public StackVisitor {
    explicit FirstExternalCallerVisitor(Thread* thread)
        : StackVisitor(thread, nullptr, StackVisitor::StackWalkKind::kIncludeInlinedFrames),
          caller(nullptr) {
    }

    bool VisitFrame() override REQUIRES_SHARED(Locks::mutator_lock_) {
      ArtMethod *m = GetMethod();
      if (m == nullptr) {
        caller = nullptr;
        // Attached native thread. Assume this is *not* boot class path.
        //这里的注释说，native的线程会获取堆栈？下边也有注释说了这一点
        //在native开线程去反射的话，应该是从这里返回吧。
        return false; //return了，caller为空
      } else if (m->IsRuntimeMethod()) {
        return true;
      }

      ObjPtr<mirror::Class> declaring_class = m->GetDeclaringClass();
      //猜测：这个判断是不是用来判断 这个类是由BootClassLoader加载的
      //如果是的话，是不是FreeReflection库是通过这种方式来让domain降低的？？？
      if (declaring_class->IsBootStrapClassLoaded()) {
        if (declaring_class->IsClassClass()) {
          return true; //return了，caller为空
        }
        ObjPtr<mirror::Class> lookup_class = GetClassRoot<mirror::MethodHandlesLookup>();
        if ((declaring_class == lookup_class || declaring_class->IsInSamePackage(lookup_class))
            && !m->IsClassInitializer()) {
          return true;
        }
        ObjPtr<mirror::Class> proxy_class = GetClassRoot<mirror::Proxy>();
        if (declaring_class->IsInSamePackage(proxy_class) && declaring_class != proxy_class) {
          if (Runtime::Current()->isChangeEnabled(kPreventMetaReflectionBlacklistAccess)) {
            return true; //return了，caller为空
          }
        }
      }

      caller = m; //caller在这里赋值了
      return false; //这里return，caller是不为空的
    }

    ArtMethod* caller;
  };

  FirstExternalCallerVisitor visitor(self);
  visitor.WalkStack();

  // Construct AccessContext from the calling class found on the stack.
  // If the calling class cannot be determined, e.g. unattached threads,
  // we conservatively assume the caller is trusted.
  //翻译：从堆栈上找到的调用类构造 AccessContext。如果无法确定调用类，例如native的线程，我们保守地假设调用者是可信的。

  //caller为空：domain直接设置为 Domain::kCorePlatform
  //caller不为空：获取DexFile（GetDexCache()），然后获取DexFile的domain（GetHiddenapiDomain()）
  ObjPtr<mirror::Class> caller = (visitor.caller == nullptr)
      ? nullptr : visitor.caller->GetDeclaringClass();
  return caller.IsNull() ? hiddenapi::AccessContext(/* is_trusted= */ true)
                         : hiddenapi::AccessContext(caller);
}


DexFile domain赋值逻辑：
	hidden_api.cc#InitializeDexFileDomain：
		Domain dex_domain = DetermineDomainFromLocation(dex_file.GetLocation(), class_loader);
		dex_file.SetHiddenapiDomain(dex_domain); //赋值domian

	// cat /proc/pid/maps |grep "/apex/.*.jar" 可查看进程运行时有哪些/apex/的jar
	hidden_api.cc#DetermineDomainFromLocation：
		if (ArtModuleRootDistinctFromAndroidRoot()) { //1.判断/system、/apex/com.android.art 这些目录是否存在
		    if (LocationIsOnArtModule(dex_location.c_str()) //2.是否在 /apex/com.android.art
		    	|| LocationIsOnConscryptModule(dex_location.c_str())) { //3.否是在 /apex/com.android.conscrypt
		      return Domain::kCorePlatform;
		    }
		    if (LocationIsOnApex(dex_location.c_str())) { //4.是否在 /apex/
		      return Domain::kPlatform;
		    }
	  	}
	    if (LocationIsOnSystemFramework(dex_location.c_str())) { //5.是否在 /system/framework
	      	return Domain::kPlatform;
	  	}
	    if (class_loader.IsNull()) {
	    	return Domain::kPlatform;
	  	}
	  	return Domain::kApplication;
```


## 参考

[FreeReflection](https://github.com/tiann/FreeReflection)

[Android 11 绕过反射限制](https://www.jianshu.com/p/6546ce67c8e0)

[安卓hiddenapi访问绝技](https://bbs.pediy.com/thread-268936.htm)

