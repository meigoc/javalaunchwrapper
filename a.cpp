#include <jni.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>


const std::string CONFIG_FILE = "saved.ini";


std::map<std::string, std::string> loadConfig() {
    std::map<std::string, std::string> config;
    std::ifstream file(CONFIG_FILE);
    std::string line;
    
    while (std::getline(file, line)) {
        size_t equalPos = line.find('=');
        if (equalPos != std::string::npos) {
            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);
            config[key] = value;
        }
    }
    return config;
}


void saveConfig(const std::map<std::string, std::string>& config) {
    std::ofstream file(CONFIG_FILE);
    for (const auto& pair : config) {
        file << pair.first << "=" << pair.second << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::map<std::string, std::string> config = loadConfig();
    

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-class" && i + 1 < argc) {
            config["class"] = argv[++i];
        } else if (arg == "-jar" && i + 1 < argc) {
            config["jar"] = argv[++i];
        } else if (arg == "-cp" && i + 1 < argc) {
            config["cp"] = argv[++i];
        }
    }


    saveConfig(config);

    if (config.find("class") == config.end() && config.find("jar") == config.end()) {
        std::cerr << "Ошибка: не указан -class или -jar и нет сохранённых данных." << std::endl;
        return 1;
    }

    std::string classpath = config["cp"].empty() ? "." : config["cp"];
    JavaVMOption options[1];
    std::string classpathOption = "-Djava.class.path=" + classpath;
    options[0].optionString = const_cast<char*>(classpathOption.c_str());

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

    if (!config["jar"].empty()) {
        jclass urlClassLoaderClass = env->FindClass("java/net/URLClassLoader");
        if (!urlClassLoaderClass) {
            env->ExceptionDescribe();
            jvm->DestroyJavaVM();
            return 1;
        }

        jclass systemClass = env->FindClass("java/lang/System");
        jmethodID getProperty = env->GetStaticMethodID(systemClass, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;");
        jstring fileSeparator = env->NewStringUTF("file.separator");
        jstring separatorValue = (jstring)env->CallStaticObjectMethod(systemClass, getProperty, fileSeparator);
        const char* sep = env->GetStringUTFChars(separatorValue, nullptr);
        
        std::string jarPath = "file:" + config["jar"];
        jstring jarUrl = env->NewStringUTF(jarPath.c_str());

        jobjectArray urlArray = env->NewObjectArray(1, env->FindClass("java/net/URL"), nullptr);
        env->SetObjectArrayElement(urlArray, 0, jarUrl);

        jmethodID init = env->GetMethodID(urlClassLoaderClass, "<init>", "([Ljava/net/URL;)V");
        jobject urlClassLoader = env->NewObject(urlClassLoaderClass, init, urlArray);

        jmethodID loadClassMethod = env->GetMethodID(urlClassLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        jstring mainClassName = env->NewStringUTF(config["class"].c_str());
        jclass mainClass = (jclass)env->CallObjectMethod(urlClassLoader, loadClassMethod, mainClassName);

        if (!mainClass) {
            env->ExceptionDescribe();
            jvm->DestroyJavaVM();
            return 1;
        }

        jmethodID mainMethod = env->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
        if (!mainMethod) {
            env->ExceptionDescribe();
            jvm->DestroyJavaVM();
            return 1;
        }

        jclass stringClass = env->FindClass("java/lang/String");
        jobjectArray argsForMain = env->NewObjectArray(0, stringClass, nullptr);
        env->CallStaticVoidMethod(mainClass, mainMethod, argsForMain);

    } else {
        jclass cls = env->FindClass(config["class"].c_str());
        if (!cls) {
            env->ExceptionDescribe();
            jvm->DestroyJavaVM();
            std::cerr << "Класс " << config["class"] << " не найден." << std::endl;
            return 1;
        }

        jmethodID mid = env->GetStaticMethodID(cls, "main", "([Ljava/lang/String;)V");
        if (!mid) {
            env->ExceptionDescribe();
            jvm->DestroyJavaVM();
            std::cerr << "Метод main не найден в классе " << config["class"] << std::endl;
            return 1;
        }

        jclass stringClass = env->FindClass("java/lang/String");
        jobjectArray argsForMain = env->NewObjectArray(0, stringClass, nullptr);
        env->CallStaticVoidMethod(cls, mid, argsForMain);
    }

    jvm->DestroyJavaVM();
    return 0;
}
