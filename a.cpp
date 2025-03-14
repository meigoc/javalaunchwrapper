#include <jni.h>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Использование: " << argv[0] << " -class <ИмяКласса>" << std::endl;
        return 1;
    }

    std::string className;
  
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-class" && i + 1 < argc) {
            className = argv[i + 1];
            break;
        }
    }

    if (className.empty()) {
        std::cerr << "Не указано имя класса!" << std::endl;
        return 1;
    }

    JavaVMOption options[1];
    options[0].optionString = const_cast<char*>("-Djava.class.path=.");

    JavaVMInitArgs vm_args;
    vm_args.version = JNI_VERSION_1_8;
    vm_args.nOptions = 1;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = false;

    JavaVM* jvm = nullptr;
    JNIEnv* env = nullptr;

    jint res = JNI_CreateJavaVM(&jvm, reinterpret_cast<void**>(&env), &vm_args);
    if (res != JNI_OK) {
        std::cerr << "Не удалось создать JVM, код ошибки: " << res << std::endl;
        return res;
    }

    jclass cls = env->FindClass(className.c_str());
    if (cls == nullptr) {
        env->ExceptionDescribe();
        jvm->DestroyJavaVM();
        std::cerr << "Класс " << className << " не найден." << std::endl;
        return 1;
    }

    jmethodID mid = env->GetStaticMethodID(cls, "main", "([Ljava/lang/String;)V");
    if (mid == nullptr) {
        env->ExceptionDescribe();
        jvm->DestroyJavaVM();
        std::cerr << "Метод main не найден в классе " << className << std::endl;
        return 1;
    }

    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray argsForMain = env->NewObjectArray(0, stringClass, nullptr);


    env->CallStaticVoidMethod(cls, mid, argsForMain);


    jvm->DestroyJavaVM();

    return 0;
}
