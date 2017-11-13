/**
 * Copyright (2017) Baidu Inc. All rights reserveed.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "lightduer_coap_defs.h"
#include "com_baidu_lightduer_lib_jni_LightduerOsJni.h"
#include "android/log.h"
#include "lightduer_connagent.h"
#include "lightduer_voice.h"

#define TAG "lightduer_os"
#define PACKAGE "com/baidu/lightduer/lib/jni"

#define LOGE(...) \
      ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

#define LOGI(...) \
      ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))

#define LOGD(...) \
      ((void)__android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__))

static JavaVM *s_javavm;

struct EventCallback {
    jobject obj;
    jmethodID method;
    jobject lightDuerEvent;
    jobject lightDuerEventId;
};

struct VoiceResultCallback {
    jobject obj;
    jmethodID method;
};

static EventCallback s_eventCB;
static VoiceResultCallback s_voiceResultCB;


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void */*reserved*/)
{
    s_javavm = vm;
    JNIEnv* env = NULL;

    if (s_javavm) {
        LOGD("s_javavm init success");
    } else {
        LOGE("s_javavm init failed");
    }

    if (s_javavm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
        LOGE("get env fail!");
        return -1;
    }
    return JNI_VERSION_1_4;
}

JNIEnv* getJNIEnv(int* neesDetach)
{
    JNIEnv* env = NULL;

    if (s_javavm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
        int status = s_javavm->AttachCurrentThread((JNIEnv**)&env, 0);
        if (status < 0) {
            LOGE("failed to attach current thread!!");
            return env;
        }
        *neesDetach = 1;
    }

    return env;
}

JNIEXPORT void JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_initialize(
        JNIEnv */*env*/, jclass /*clazz*/)
{
    duer_initialize();
}

static void duer_event_hook(duer_event_t *event)
{
    if (!event) {
        LOGE("NULL event!!!");
    }

    LOGD("======event: %d", event->_event);
    LOGD("======event code: %d", event->_code);
    int needDetach = 0;
    JNIEnv *jenv = getJNIEnv(&needDetach);
    if (jenv == NULL) {
        LOGE("can't get jenv");
        return;
    }

    do {
        jclass jduereventclass = jenv->GetObjectClass(s_eventCB.lightDuerEvent);
        if (jduereventclass == NULL) {
            LOGE("LightduerEvent not found!");
            break;
        }
        jmethodID constru =
                jenv->GetMethodID(jduereventclass, "<init>",
                                  "(L"PACKAGE"/event/LightduerEventId;ILjava/lang/Object;)V");
        if (constru == NULL) {
            LOGE("get constructor failed");
            break;
        }

        jclass jduereventid = jenv->GetObjectClass(s_eventCB.lightDuerEventId);
        if (jduereventid == NULL) {
            LOGE("get jduereventid failed");
            break;
        }

        jfieldID eventIdField;
        switch (event->_event) {
            case DUER_EVENT_STARTED:
                eventIdField = jenv->GetStaticFieldID(jduereventid, "DUER_EVENT_STARTED",
                                                      "L"PACKAGE"/event/LightduerEventId;");
                break;
            case DUER_EVENT_STOPPED:
                eventIdField = jenv->GetStaticFieldID(jduereventid, "DUER_EVENT_STOPPED",
                                                      "L"PACKAGE"/event/LightduerEventId;");
                break;
        }
        if (eventIdField == NULL) {
            LOGE("get eventIdField failed");
            break;
        }
        jobject eventId = jenv->GetStaticObjectField(jduereventid, eventIdField);
        if (eventId == NULL) {
            LOGE("get eventId failed");
            break;
        }
        if (event->_object != NULL) {
            LOGE("TODO try to transfer the object to java layer!");
        }
        jobject duerevent = jenv->NewObject(jduereventclass, constru, eventId, event->_code, NULL);
        if (duerevent == NULL) {
            LOGE("get duerevent failed");
            break;
        }
        jenv->CallVoidMethod(s_eventCB.obj, s_eventCB.method, duerevent);
    } while (0);

    if (needDetach) {
        s_javavm->DetachCurrentThread();
    }
}

static jobject init_lightduerevent(JNIEnv *jenv)
{
    jclass lightduereventclass = jenv->FindClass(PACKAGE"/event/LightduerEvent");
    if (lightduereventclass == NULL) {
        LOGE("lightduereventclass get fail");
        return NULL;
    }
    jmethodID constructor =
            jenv->GetMethodID(lightduereventclass, "<init>", "()V");
    if (constructor == NULL) {
        LOGE("get constructor failed");
        return NULL;
    }
    jobject result = jenv->NewObject(lightduereventclass, constructor);
    if (!result) {
        LOGE("create object failed");
        return NULL;
    }
    return jenv->NewGlobalRef(result);

}

static jobject init_lightduereventid(JNIEnv *jenv)
{
    jclass jduereventid = jenv->FindClass(PACKAGE"/event/LightduerEventId");
    if (jduereventid == NULL) {
        LOGE("get jduereventid failed");
        return NULL;
    }
    jfieldID eventIdField = jenv->GetStaticFieldID(jduereventid, "DUER_EVENT_STARTED",
                                                  "L"PACKAGE"/event/LightduerEventId;");

    if (eventIdField == NULL) {
        LOGE("get eventIdField failed");
        return NULL;
    }
    jobject eventId = jenv->GetStaticObjectField(jduereventid, eventIdField);
    if (eventId == NULL) {
        LOGE("get eventId failed");
        return NULL;
    }
    return jenv->NewGlobalRef(eventId);
}

JNIEXPORT void JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_setEventListener(
        JNIEnv *env, jclass /*cls*/, jobject obj)
{
    jclass event_callback_class = env->GetObjectClass(obj);
    jmethodID callback = env->GetMethodID(event_callback_class, "callback",
                                          "(L"PACKAGE"/event/LightduerEvent;)V");
    if (callback == NULL) {
        LOGE("method callback is not found!!\n");
        return;
    }
    s_eventCB.obj = env->NewGlobalRef(obj);
    s_eventCB.method = callback;
    s_eventCB.lightDuerEvent = init_lightduerevent(env);
    s_eventCB.lightDuerEventId = init_lightduereventid(env);
    duer_set_event_callback(duer_event_hook);
}

static void duer_voice_result(struct baidu_json *result)
{
    char* str_result = baidu_json_PrintUnformatted(result);
    LOGD("duer_voice_result:%s \n", str_result);

    int needDetach = 0;
    JNIEnv *jenv = getJNIEnv(&needDetach);
    jstring jstr_result = jenv->NewStringUTF(str_result);
    jenv->CallVoidMethod(s_voiceResultCB.obj, s_voiceResultCB.method, jstr_result);
    free(str_result);
    if (needDetach) {
        s_javavm->DetachCurrentThread();
    }
}

JNIEXPORT void JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_setVoiceResultListener(
        JNIEnv *env, jclass /*clazz*/, jobject obj)
{
    jclass vocice_result_callback_class = env->GetObjectClass(obj);
    jmethodID callback = env->GetMethodID(vocice_result_callback_class, "callback",
                                          "(Ljava/lang/String;)V");
    if (callback == NULL) {
        LOGE("method callback is not found!!\n");
        return;
    }

    s_voiceResultCB.obj = env->NewGlobalRef(obj);
    s_voiceResultCB.method = callback;
    duer_voice_set_result_callback(duer_voice_result);
}

JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_startServerByProfile(
        JNIEnv *env, jclass /*clazz*/, jbyteArray byteArray)
{
    jbyte *data = env->GetByteArrayElements(byteArray, 0);
    jsize size = env->GetArrayLength(byteArray);
    LOGI("data:%s \n", data);
    LOGI("size:%d \n", size);
    return duer_start(data, size);
}

JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_stopServer(
        JNIEnv */*env*/, jclass /*clazz*/)
{
    return duer_stop();
}

JNIEXPORT void JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_setVoiceMode(
        JNIEnv */*env*/, jclass /*clazz*/, jint mode)
{
    switch (mode) {
        case 1:
            duer_voice_set_mode(DUER_VOICE_MODE_CHINESE_TO_ENGLISH);
            break;
        case 2:
            duer_voice_set_mode(DUER_VOICE_MODE_ENGLISH_TO_CHINESE);
            break;
        default:
            duer_voice_set_mode(DUER_VOICE_MODE_DEFAULT);
            break;
    }
}

JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_startVoice(
        JNIEnv */*env*/, jclass /*clazz*/, jint samplerate)
{
    return duer_voice_start(samplerate);
}

JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_sendVoiceData(
        JNIEnv *env, jclass /*clazz*/, jbyteArray voice_data)
{
    jbyte *data = env->GetByteArrayElements(voice_data, 0);
    jsize size = env->GetArrayLength(voice_data);
    return duer_voice_send(data, size);
}

JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_stopVoice(
        JNIEnv */*env*/, jclass /*clazz*/)
{
    return duer_voice_stop();
}

JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_reportData(
        JNIEnv *env, jclass /*clazz*/, jstring data)
{
    const char* c_data = env->GetStringUTFChars(data, NULL);
    if (c_data == NULL) {
        LOGE("get c char failed");
        return DUER_ERR_FAILED;
    }
    baidu_json *json_data = baidu_json_Parse(c_data);
    if (json_data == NULL) {
        LOGE("get json failed");
        return DUER_ERR_FAILED;
    }
    int result = duer_data_report(json_data);
    env->ReleaseStringUTFChars(data, c_data);
    return result;
}

static jobject s_resource_listener = NULL; //TODO can be initialized in the initialize call
static jclass s_lightduerContextClass = NULL;
static jclass s_lightduerMessageClass = NULL;
static jclass s_lightduerAddressClass = NULL;

static duer_status_t coap_dynamic_resource_callback_for_java(
        duer_context ctx, duer_msg_t *msg, duer_addr_t *addr)
{
    LOGD(" in coap_dynamic_resource_callback_for_java");
    if (s_resource_listener == NULL) {
        LOGE("s_resource_listener is NULL!");
        return DUER_OK;
    }
    if (s_lightduerContextClass == NULL) {
        LOGE("s_lightduerContextClass is NULL!");
        return DUER_OK;
    }
    if (s_lightduerMessageClass == NULL) {
        LOGE("s_lightduerMessageClass is NULL!");
        return DUER_OK;
    }
    if (s_lightduerAddressClass == NULL) {
        LOGE("s_lightduerAddressClass is NULL!");
        return DUER_OK;
    }

    int needDetach = 0;
    JNIEnv *jenv = getJNIEnv(&needDetach);
    if (jenv == NULL) {
        LOGE("can't get jenv");
        return DUER_OK;
    }

    char *c_host = NULL;
    char *c_path = NULL;
    char *c_query = NULL;

    do {
        jmethodID constructorContext =
                jenv->GetMethodID(s_lightduerContextClass, "<init>", "(J)V");
        if (constructorContext == NULL) {
            LOGE("get constructorContext failed");
            break;
        }
        jobject context = jenv->NewObject(s_lightduerContextClass, constructorContext, (jlong) ctx);
        if (context == NULL) {
            LOGE("create context fail!");
            break;
        }

        jmethodID constructorAddress =
                jenv->GetMethodID(s_lightduerAddressClass, "<init>", "(SILjava/lang/String;)V");
        if (constructorAddress == NULL) {
            LOGE("get constructorAddress failed");
            break;
        }

        c_host = (char*)malloc(addr->host_size + 1);
        if (c_host == NULL) {
            LOGE("alloc host fail!");
            break;
        }
        //LOGD("host size: %lu \n", addr->host_size);
        memcpy(c_host, addr->host, addr->host_size);
        c_host[addr->host_size] = 0;
        LOGD("host: %s \n", c_host);
        jstring host = jenv->NewStringUTF(c_host);
        jobject address = jenv->NewObject(s_lightduerAddressClass, constructorAddress,
                                          addr->type, addr->port, host);
        if (address == NULL) {
            LOGE("create address fail!");
            break;
        }

        jmethodID constructorMessage =
                jenv->GetMethodID(
                        s_lightduerMessageClass,
                        "<init>",
                        "(SSIIIII[BLjava/lang/String;Ljava/lang/String;[B)V");
        if (constructorMessage == NULL) {
            LOGE("get constructorMessage failed");
            break;
        }

        jbyte *token_byte = (jbyte*)msg->token;
        jbyteArray token_array = jenv->NewByteArray(msg->token_len);
        jenv->SetByteArrayRegion(token_array, 0, msg->token_len, token_byte);
        LOGD("token_len:%d \n", msg->token_len);
        //for (int i =0; i < msg->token_len; ++i) {
        //    LOGD("token[%d]: 0x%x \n", i, msg->token[i]);
        //}

        c_path = (char*)malloc(msg->path_len + 1);
        if (c_path == NULL) {
            LOGE("alloc c_path fail!");
            break;
        }
        LOGD("path_len:%d \n", msg->path_len);
        memcpy(c_path, msg->path, msg->path_len);
        c_path[msg->path_len] = 0;
        LOGD("c_path:%s \n", c_path);
        jstring path = jenv->NewStringUTF(c_path);

        c_query = (char*)malloc(msg->query_len + 1);
        if (c_query == NULL) {
            LOGE("alloc c_query fail!");
            break;
        }
        LOGD("query_len:%d\n", msg->query_len);
        memcpy(c_query, msg->query, msg->query_len);
        c_query[msg->query_len] = 0;
        LOGD("query:%s\n", c_query);
        jstring query = jenv->NewStringUTF(c_query);

        jbyte *payload_byte = (jbyte*)msg->payload;
        jbyteArray payload_array = jenv->NewByteArray(msg->payload_len);
        jenv->SetByteArrayRegion(payload_array, 0, msg->payload_len, payload_byte);

        jobject message = jenv->NewObject(s_lightduerMessageClass, constructorMessage,
                                          msg->msg_type, msg->msg_code, msg->msg_id,
                                          msg->token_len, msg->path_len, msg->query_len, msg->payload_len,
                                          token_array, path, query, payload_array);
        if (message == NULL) {
            LOGE("create message fail!");
            break;
        }
        LOGI("msg_type:%d, msg_code:%d, msg_id:%d \n", msg->msg_type, msg->msg_code, msg->msg_id);
        jclass resourceListenerClass = jenv->GetObjectClass(s_resource_listener);
        jmethodID callback = jenv->GetMethodID(resourceListenerClass, "callback",
                                               "(Lcom/baidu/lightduer/lib/jni/utils/LightduerContext;Lcom/baidu/lightduer/lib/jni/utils/LightduerMessage;Lcom/baidu/lightduer/lib/jni/utils/LightduerAddress;)I");

        jint result = jenv->CallIntMethod(s_resource_listener, callback, context, message, address);
        LOGI("result %d", result);
    } while (0);

    if (c_host) {
        free(c_host);
        c_host = NULL;
    }
    if (c_path) {
        free(c_path);
        c_path = NULL;
    }
    if (c_query) {
        free(c_query);
        c_query = NULL;
    }
    if (needDetach) {
        s_javavm->DetachCurrentThread();
    }

    return DUER_OK;
}

JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_addResource(
        JNIEnv *env, jclass clazz, jobjectArray objArray)
{
    int result = -1;
    jsize arraylength = env->GetArrayLength(objArray);
    if (arraylength <= 0) {
        LOGE("no element in array");
        return result;
    }

    jobject lightduerResource = env->GetObjectArrayElement(objArray, 0);
    if (lightduerResource == NULL) {
        LOGE("get element 0 fail!");
        return result;
    }
    jclass lightduerResourceClass = env->GetObjectClass(lightduerResource);
    if (lightduerResourceClass == NULL) {
        LOGE("get class for LightduerResource fail!");
        return result;
    }
    jfieldID fieldMode = env->GetFieldID(lightduerResourceClass, "mode", "I");
    if (fieldMode == NULL) {
        LOGE("get field mode failed!");
        return result;
    }
    jfieldID fieldAllowed = env->GetFieldID(lightduerResourceClass, "allowed", "I");
    if (fieldAllowed == NULL) {
        LOGE("get field allowed failed!");
        return result;
    }
    jfieldID fieldPath = env->GetFieldID(lightduerResourceClass, "path", "Ljava/lang/String;");
    if (fieldPath == NULL) {
        LOGE("get field path failed!");
        return result;
    }
    jfieldID  fieldStaticData = env->GetFieldID(lightduerResourceClass, "staticData", "[B");
    if (fieldStaticData == NULL) {
        LOGE("get field staticData failed!");
        return result;
    }
    jfieldID fieldDynamicCallback =
            env->GetFieldID(lightduerResourceClass,
                            "dynamicCallback",
                            "Lcom/baidu/lightduer/lib/jni/controlpoint/LightduerResourceListener;");
    if (fieldDynamicCallback == NULL) {
        LOGE("get field dynamicCallback failed");
        return result;
    }

    if (s_lightduerAddressClass == NULL) {
        jclass lightduerAddressClass = env->FindClass(PACKAGE"/utils/LightduerAddress");
        if (lightduerAddressClass == NULL) {
            LOGE("lightduerAddressClass find fail!");
            return result;
        }
        s_lightduerAddressClass = (jclass) env->NewGlobalRef(lightduerAddressClass);
    }

    if (s_lightduerContextClass == NULL) {
        jclass lightduerContextClass = env->FindClass(PACKAGE"/utils/LightduerContext");
        if (lightduerContextClass == NULL) {
            LOGE("lightduerContextClass find fail!");
            return result;
        }
        s_lightduerContextClass = (jclass) env->NewGlobalRef(lightduerContextClass);
    }

    if (s_lightduerMessageClass == NULL) {
        jclass lightduerMessageClass = env->FindClass(PACKAGE"/utils/LightduerMessage");
        if (lightduerMessageClass == NULL) {
            LOGE("lightduerMessageClass find fail!");
            return result;
        }
        s_lightduerMessageClass = (jclass) env->NewGlobalRef(lightduerMessageClass);
    }

    duer_res_t* resources = (duer_res_t*)malloc(sizeof(duer_res_t) * arraylength);
    if (!resources) {
        LOGE("resources alloc fail!!");
        return result;
    }
    memset(resources, 0, sizeof(duer_res_t) * arraylength);

    bool error_happen = false;
    for (int i = 0; i < arraylength; ++i) {
        jobject resource = env->GetObjectArrayElement(objArray, i);
        jint mode = env->GetIntField(resource, fieldMode);
        resources[i].mode = mode;
        jint allowed = env->GetIntField(resource, fieldAllowed);
        resources[i].allowed = allowed;
        jstring path = (jstring) env->GetObjectField(resource, fieldPath);
        //const char* c_path = env->GetStringUTFChars(path, NULL);
        //LOGI("c_path:%s", c_path);
        //env->ReleaseStringUTFChars(path, c_path);
        jsize pathLength = env->GetStringUTFLength(path);
        char *newPath = (char *) malloc(pathLength + 1);
        if (newPath == NULL) {
            LOGE("alloc fail! %d", i);
            error_happen = true;
            break;
        }

        env->GetStringUTFRegion(path, 0, pathLength, newPath);
        newPath[pathLength] = 0;
        resources[i].path = newPath;
        LOGD("newData: %d, path:%s", i, resources[i].path);
        if (mode == DUER_RES_MODE_STATIC) {
            jbyteArray  staticData = (jbyteArray) env->GetObjectField(resource, fieldStaticData);
            jsize dataLength = env->GetArrayLength(staticData);
            jbyte *newData = (jbyte *) malloc(dataLength);
            if (newData == NULL) {
                LOGE("newData alloc fail! %d", i);
                error_happen = true;
                break;
            }
            LOGD("newData: %d, p:%p", i, newData);
            env->GetByteArrayRegion(staticData, 0, dataLength, newData);
            resources[i].res.s_res.data = newData;
            resources[i].res.s_res.size = dataLength;
        } else if (mode == DUER_RES_MODE_DYNAMIC) {
            resources[i].res.f_res = &coap_dynamic_resource_callback_for_java;
            if (s_resource_listener == NULL) {
                LOGD("set the java resource callback for dynamic resource!!");
                jobject cb = env->GetObjectField(resource, fieldDynamicCallback);
                s_resource_listener = env->NewGlobalRef(cb);
            }
        }

    }

    if (!error_happen) {
        result = duer_add_resources(resources, arraylength);
    }
    for (int i = 0; i < arraylength; ++i) {
        if (resources[i].path) {
            free(resources[i].path);
        }
        if (resources[i].mode == DUER_RES_MODE_STATIC) {
            if (resources[i].res.s_res.data) {
                free(resources[i].res.s_res.data);
            }
        }
    }
    free(resources);
    return result;
}


JNIEXPORT jint JNICALL Java_com_baidu_lightduer_lib_jni_LightduerOsJni_sendResponse(
        JNIEnv *env, jclass type, jobject message, jshort msgCode, jbyteArray payload_) {
    duer_msg_t duer_message;

    if (s_lightduerMessageClass == NULL) {
        LOGE("s_lightduerMessageClass is NULL!");
        return DUER_ERR_FAILED;
    }

    jfieldID fieldtype = env->GetFieldID(s_lightduerMessageClass, "msgType", "S");
    if (fieldtype == NULL) {
        LOGE("get field type failed!");
        return DUER_ERR_FAILED;
    }
    jshort typeValue = env->GetShortField(message, fieldtype);
    duer_message.msg_type = typeValue;

    jfieldID fieldcode = env->GetFieldID(s_lightduerMessageClass, "msgCode", "S");
    if (fieldcode == NULL) {
        LOGE("get field code failed!");
        return DUER_ERR_FAILED;
    }
    jshort codeValue = env->GetShortField(message, fieldcode);
    duer_message.msg_code = codeValue;

    jfieldID fieldId = env->GetFieldID(s_lightduerMessageClass, "msgId", "I");
    if (fieldId == NULL) {
        LOGE("get field id failed!");
        return DUER_ERR_FAILED;
    }
    jint idValue = env->GetIntField(message, fieldId);
    duer_message.msg_id = idValue;

    jfieldID fieldtokenLen = env->GetFieldID(s_lightduerMessageClass, "tokenLen", "I");
    if (fieldtokenLen == NULL) {
        LOGE("get field tokenLen failed!");
        return DUER_ERR_FAILED;
    }
    LOGI("response msg id:%d, code:%d, type:%d", duer_message.msg_id, duer_message.msg_code, duer_message.msg_type);
    jint tokenLenValue = env->GetIntField(message, fieldtokenLen);
    duer_message.token_len = tokenLenValue;
    jfieldID fieldpathLen = env->GetFieldID(s_lightduerMessageClass, "pathLen", "I");
    if (fieldpathLen == NULL) {
        LOGE("get field pathLen failed!");
        return DUER_ERR_FAILED;
    }
    jint pathLenValue = env->GetIntField(message, fieldpathLen);
    duer_message.path_len = pathLenValue;
    jfieldID fieldquerylen = env->GetFieldID(s_lightduerMessageClass, "querylen", "I");
    if (fieldquerylen == NULL) {
        LOGE("get field querylen failed!");
        return DUER_ERR_FAILED;
    }
    jint queryLenValue = env->GetIntField(message, fieldquerylen);
    duer_message.query_len = queryLenValue;
    jfieldID fieldpayloadLen = env->GetFieldID(s_lightduerMessageClass, "payloadLen", "I");
    if (fieldpayloadLen == NULL) {
        LOGE("get field payloadLen failed!");
        return DUER_ERR_FAILED;
    }
    jint payloadLenValue = env->GetIntField(message, fieldpayloadLen);
    duer_message.payload_len = payloadLenValue;
    LOGI("response: tokenlen:%d, query_len:%d, path_len:%d, payload_len:%d \n",
         duer_message.token_len, duer_message.query_len, duer_message.path_len, duer_message.payload_len);

    jfieldID fieldPath = env->GetFieldID(s_lightduerMessageClass, "path", "Ljava/lang/String;");
    if (fieldPath == NULL) {
        LOGE("get field path failed!");
        return DUER_ERR_FAILED;
    }
    jstring path = (jstring) env->GetObjectField(message, fieldPath);
    //const char* c_path = env->GetStringUTFChars(path, NULL);
    //LOGI("c_path:%s", c_path);
    //env->ReleaseStringUTFChars(path, c_path);
    jsize pathLength = env->GetStringUTFLength(path);
    char *newPath = (char *) malloc(pathLength + 1);
    if (newPath == NULL) {
        LOGE("alloc path fail!");
        return DUER_ERR_FAILED;
    }
    env->GetStringUTFRegion(path, 0, pathLength, newPath);
    newPath[pathLength] = 0;
    duer_message.path = (duer_u8_t *)newPath;
    LOGD("path:%s, len1:%d, len2:%d;(len1 == len2)", newPath, pathLength, duer_message.path_len);
    jfieldID fieldQuery= env->GetFieldID(s_lightduerMessageClass, "query", "Ljava/lang/String;");
    if (fieldQuery == NULL) {
        LOGE("get field query failed!");
        return DUER_ERR_FAILED;
    }
    jstring query = (jstring) env->GetObjectField(message, fieldQuery);
    jsize queryLength = env->GetStringUTFLength(query);
    char *newQuery = (char *) malloc(queryLength + 1);
    if (newQuery == NULL) {
        LOGE("alloc query fail!");
        return DUER_ERR_FAILED;
    }
    env->GetStringUTFRegion(query, 0, queryLength, newQuery);
    newQuery[queryLength] = 0;
    duer_message.query = (duer_u8_t *)newQuery;
    LOGD("query:%s, len1:%d, len2:%d;(len1 == len2)", newQuery, queryLength, duer_message.query_len);

    jfieldID  fieldToken = env->GetFieldID(s_lightduerMessageClass, "token", "[B");
    if (fieldToken == NULL) {
        LOGE("get field token failed!");
        return DUER_ERR_FAILED;
    }
    jbyteArray  token = (jbyteArray) env->GetObjectField(message, fieldToken);
    jsize tokenLength = env->GetArrayLength(token);
    jbyte *newToken = (jbyte *) malloc(tokenLength);
    if (newToken == NULL) {
        LOGE("newToken alloc fail!");
        return DUER_ERR_FAILED;
    }
    LOGD("newToken: %p, tokenLength:%d, token_len:%d (tokenLength == token_len)",
         newToken, tokenLength, duer_message.token_len);
    env->GetByteArrayRegion(token, 0, tokenLength, newToken);
    duer_message.token = (duer_u8_t *)newToken;
    //for(int i =0; i < tokenLength; ++i) {
    //    LOGD("token[%d]: 0x%x \n", i, duer_message.token[i]);
    //}
    jfieldID  fieldPayload = env->GetFieldID(s_lightduerMessageClass, "payload", "[B");
    if (fieldPayload == NULL) {
        LOGE("get field payload failed!");
        return DUER_ERR_FAILED;
    }
    jbyteArray  payload = (jbyteArray) env->GetObjectField(message, fieldPayload);
    jsize payloadLength = env->GetArrayLength(payload);
    jbyte *newPayload = (jbyte *) malloc(payloadLength);
    if (newPayload == NULL) {
        LOGE("newPayload alloc fail!");
        return DUER_ERR_FAILED;
    }
    LOGD("newPayload: %p, payloadLength:%d, payload_len:%d (payloadLength == payload_len)",
         newPayload, payloadLength, duer_message.payload_len);
    env->GetByteArrayRegion(payload, 0, payloadLength, newPayload);
    duer_message.payload = (duer_u8_t *)newPayload;

    LOGI("msgCode: %d", msgCode);
    if (payload_) {
        jbyte *payload = env->GetByteArrayElements(payload_, NULL);
        jsize payload_len = env->GetArrayLength(payload_);
        duer_response(&duer_message, msgCode, payload, payload_len);
        env->ReleaseByteArrayElements(payload_, payload, 0);
    } else {
        duer_response(&duer_message, msgCode, NULL, 0);
    }

    if (newPath) {
        free(newPath);
        newPath = NULL;
    }
    if (newQuery) {
        free(newQuery);
        newQuery = NULL;
    }
    if(newToken) {
        free(newToken);
        newToken = NULL;
    }
    if (newPayload) {
        free(newPayload);
        newPayload = NULL;
    }

    return DUER_OK;
}
