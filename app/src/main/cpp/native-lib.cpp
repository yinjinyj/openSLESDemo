#include <jni.h>

#include <string>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>
#include <SLES/OpenSLES_AndroidMetadata.h>
#include <jni.h>
#include <android/log.h>

// for opensles
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

// for native asset manager
#include <sys/types.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>

#include <stdio.h>
#include <malloc.h>

//打印日志
#include <android/log.h>

#define LOGI(FORMAT, ...) __android_log_print(ANDROID_LOG_INFO,"ywl5320",FORMAT,##__VA_ARGS__);
#define LOGE(FORMAT, ...) __android_log_print(ANDROID_LOG_ERROR,"ywl5320",FORMAT,##__VA_ARGS__);

// 引擎对象
SLObjectItf engineObject = NULL;
//引擎接口
SLEngineItf engineEngine = NULL;

//混音器
SLObjectItf outputMixObject = NULL;
SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

//assets播放器(播放器对象)
SLObjectItf fdPlayerObject = NULL;
//播放接口
SLPlayItf fdPlayerPlay = NULL;
//声音控制接口
SLVolumeItf fdPlayerVolume = NULL;

//uri播放器
SLObjectItf uriPlayerObject = NULL;
//播放接口
SLPlayItf uriPlayerPlay = NULL;
//声音控制接口
SLVolumeItf uriPlayerVolume = NULL;

//pcm
SLObjectItf pcmPlayerObject = NULL;
//播放接口
SLPlayItf pcmPlayerPlay = NULL;
//声音控制接口
SLVolumeItf pcmPlayerVolume = NULL;

//缓冲器队列接口
SLAndroidSimpleBufferQueueItf pcmBufferQueue;
SLuint32 playerState;
FILE *pcmFile;
void *buffer;

uint8_t *out_buffer;

void release();

//获取引擎接口（创建引擎）
void createEngine();

const SLInterfaceID ids[] = {SL_IID_VOLUME};
const SLboolean req[] = {SL_BOOLEAN_FALSE};
extern "C"
JNIEXPORT jstring

JNICALL
Java_com_yinjin_expandtextview_openslesdemo_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}


//重置播放器
void release() {
    if (pcmPlayerObject != NULL) {
        (*pcmPlayerObject)->Destroy(pcmPlayerObject);
        pcmPlayerObject = NULL;
        pcmPlayerPlay = NULL;
        pcmPlayerVolume = NULL;
        pcmBufferQueue = NULL;
        pcmFile = NULL;
        buffer = NULL;
        out_buffer = NULL;
    }
    // destroy file descriptor audio player object, and invalidate all associated interfaces
    if (fdPlayerObject != NULL) {
        (*fdPlayerObject)->Destroy(fdPlayerObject);
        fdPlayerObject = NULL;
        fdPlayerPlay = NULL;
        fdPlayerVolume = NULL;
    }
    // destroy URI audio player object, and invalidate all associated interfaces
    if (uriPlayerObject != NULL) {
        (*uriPlayerObject)->Destroy(uriPlayerObject);
        uriPlayerObject = NULL;
        uriPlayerPlay = NULL;
        uriPlayerVolume = NULL;
    }
    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
}

void createEngine() {
    SLresult result;
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
}
//播放asset文件
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_playAsset(JNIEnv *env, jobject instance,
                                                                jobject assetManager,
                                                                jstring filename) {
    release();
    const char *utf8 = env->GetStringUTFChars(filename, JNI_FALSE);
    // use asset manager to open asset by filename
    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    AAsset *asset = AAssetManager_open(mgr, utf8, AASSET_MODE_UNKNOWN);
    (*env).ReleaseStringUTFChars(filename, utf8);
    // open asset as file descriptor
    off_t start, length;
    int fd = AAsset_openFileDescriptor(asset, &start, &length);
    AAsset_close(asset);
    SLresult result;
    //第一步，创建引擎
    createEngine();
    //第二步，创建混音器
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, mids, mreq);
    (void) result;
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    (void) result;
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }
    //第三步，设置播放器参数和创建播放器
    // 1、 配置 audio source
    SLDataLocator_AndroidFD loc_fd = {SL_DATALOCATOR_ANDROIDFD, fd, start, length};
    SLDataFormat_MIME format_mime = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
    SLDataSource audioSrc = {&loc_fd, &format_mime};
    // 2、 配置 audio sink
    SLDataLocator_OutputMix locator_outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSink = {&locator_outputMix, NULL};
    // 创建播放器
    const SLInterfaceID ids[3] = {SL_IID_SEEK, SL_IID_MUTESOLO, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &fdPlayerObject, &audioSrc,
                                                &audioSink, 3, ids, req);
    (void) result;
    // 实现播放器
    result = (*fdPlayerObject)->Realize(fdPlayerObject, SL_BOOLEAN_FALSE);
    (void) result;
    // 得到播放器接口
    result = (*fdPlayerObject)->GetInterface(fdPlayerObject, SL_IID_PLAY, &fdPlayerPlay);
    (void) result;
    // 得到声音控制接口
    result = (*fdPlayerObject)->GetInterface(fdPlayerObject, SL_IID_PLAY, &fdPlayerVolume);
    (void) result;
    // 设置播放状态
    if (NULL != fdPlayerPlay) {
        result = (*fdPlayerPlay)->SetPlayState(fdPlayerPlay, SL_PLAYSTATE_PLAYING);
        (void) result;
    }
    //设置播放音量 （100 * -50：静音 ）
    (*fdPlayerVolume)->SetVolumeLevel(fdPlayerVolume, 0 * -50);
}
//播放URL地址
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_playURL(JNIEnv *env, jobject instance,
                                                              jstring uri) {
    SLresult result;
    const char *utf8 = env->GetStringUTFChars(uri, JNI_FALSE);
    //第一步，创建引擎
    createEngine();
    //第二步，创建混音器
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, mids, mreq);
    (void) result;
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    (void) result;
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_OUTPUTMIX,
                                              &outputMixEnvironmentalReverb);
    if (result == SL_RESULT_SUCCESS) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }
    //第三步，设置播放器参数和创建播放器
    // configure audio source
    SLDataLocator_URI loc_uri = {SL_DATALOCATOR_URI, (SLchar *) utf8};
    SLDataFormat_MIME format_ming = {SL_DATAFORMAT_MIME, NULL, SL_CONTAINERTYPE_UNSPECIFIED};
    SLDataSource slDataSource = {&loc_uri, &format_ming};
    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids[3] = {SL_IID_SEEK, SL_IID_MUTESOLO, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &uriPlayerObject, &slDataSource,
                                                &audioSnk, 3, ids, req);

    (void) result;

    // release the Java string and UTF-8
    (*env).ReleaseStringUTFChars(uri, utf8);

    // realize the player
    result = (*uriPlayerObject)->Realize(uriPlayerObject, SL_BOOLEAN_FALSE);
    // this will always succeed on Android, but we check result for portability to other platforms
    if (SL_RESULT_SUCCESS != result) {
        (*uriPlayerObject)->Destroy(uriPlayerObject);
        uriPlayerObject = NULL;
        return;
    }

    // get the play interface
    result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_PLAY, &uriPlayerPlay);
    (void) result;


    // get the volume interface
    result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_VOLUME, &uriPlayerVolume);
    (void) result;

    if (NULL != uriPlayerPlay) {

        // set the player's state
        result = (*uriPlayerPlay)->SetPlayState(uriPlayerPlay, SL_PLAYSTATE_PLAYING);
        (void) result;
    }

    //设置播放音量 （100 * -50：静音 ）
    (*uriPlayerVolume)->SetVolumeLevel(uriPlayerVolume,  1* -50);

}

void getPcmData(void **pcm) {
    while (!feof(pcmFile)) {
        fread(out_buffer, 44100 * 2 * 2, 1, pcmFile);
        if (out_buffer == NULL) {
            LOGI("%s", "read end");
            break;
        } else {
            LOGI("%s", "reading");
        }
        *pcm = out_buffer;
        break;
    }
}

void *pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void *context) {
    //assert(NULL == context);
    getPcmData(&buffer);
    // for streaming playback, replace this test by logic to find and fill the next buffer
    if (NULL != buffer) {
        SLresult result;
        // enqueue another buffer
        result = (*pcmBufferQueue)->Enqueue(pcmBufferQueue, buffer, 44100 * 2 * 2);
        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        // which for this code example would indicate a programming error
    }
}

//播放pcm流
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_playPCM(JNIEnv *env, jobject instance,
                                                              jstring pamPath_) {
    release();
    const char *pamPath = (env)->GetStringUTFChars(pamPath_, JNI_FALSE);
    pcmFile = fopen(pamPath, "r");
    if (pcmFile == NULL) {
        LOGE("%s", "fopen file error");
        return;
    }
    out_buffer = (uint8_t *) malloc(44100 * 2 * 2);
    SLresult result;
    // TODO
    //第一步，创建引擎
    createEngine();

    //第二步，创建混音器
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, mids, mreq);
    (void) result;
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    (void) result;
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, NULL};


    // 第三步，配置PCM格式信息
    SLDataLocator_AndroidSimpleBufferQueue android_queue = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
                                                            2};
    SLDataFormat_PCM pcm = {
            SL_DATAFORMAT_PCM,//播放pcm格式的数据
            2,//2个声道（立体声）
            SL_SAMPLINGRATE_44_1,//44100hz的频率
            SL_PCMSAMPLEFORMAT_FIXED_16,//位数 16位
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致就行
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声（前左前右）
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    SLDataSource slDataSource = {&android_queue, &pcm};


    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &pcmPlayerObject, &slDataSource,
                                                &audioSnk, 3, ids, req);
    //初始化播放器
    (*pcmPlayerObject)->Realize(pcmPlayerObject, SL_BOOLEAN_FALSE);

//    得到接口后调用  获取Player接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_PLAY, &pcmPlayerPlay);

//    注册回调缓冲区 获取缓冲队列接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_BUFFERQUEUE, &pcmBufferQueue);
    //缓冲接口回调
    (*pcmBufferQueue)->RegisterCallback(pcmBufferQueue,
                                        (slAndroidSimpleBufferQueueCallback) pcmBufferCallBack,
                                        NULL);
//    获取音量接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_VOLUME, &pcmPlayerVolume);

//    获取播放状态接口
    (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay, SL_PLAYSTATE_PLAYING);

//    主动调用回调函数开始工作
    pcmBufferCallBack(pcmBufferQueue, NULL);

    (env)->ReleaseStringUTFChars(pamPath_, pamPath);
}
//停止播放，是在播放asset基础上停止播放的
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_stop(JNIEnv *env, jobject instance) {
    SLresult result;
    if (NULL != fdPlayerPlay) {
        result = (*fdPlayerPlay)->GetPlayState(fdPlayerPlay, &playerState);
        (void) result;
        if (playerState == SL_PLAYSTATE_PLAYING) {
            (*fdPlayerPlay)->SetPlayState(fdPlayerPlay, SL_PLAYSTATE_STOPPED);
            release();
        }
    }

}
//暂停播放，是在播放asset基础上停止暂停的
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_pause(JNIEnv *env, jobject instance) {

    SLresult result;
    if (NULL != fdPlayerPlay) {
        result = (*fdPlayerPlay)->GetPlayState(fdPlayerPlay, &playerState);
        (void) result;
        if (playerState == SL_PLAYSTATE_PLAYING) {
            (*fdPlayerPlay)->SetPlayState(fdPlayerPlay, SL_PLAYSTATE_PAUSED);
        }
    }

}
//重新播放，是在播放asset基础上停止重新播放的
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_replay(JNIEnv *env, jobject instance) {

    SLresult result;
    if (NULL != fdPlayerPlay) {
        result = (*fdPlayerPlay)->GetPlayState(fdPlayerPlay, &playerState);
        (void) result;

        if (playerState == SL_PLAYSTATE_PAUSED) {
            (*fdPlayerPlay)->SetPlayState(fdPlayerPlay, SL_PLAYSTATE_PLAYING);
        }
    }

}
//音量增加
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_volumeAdd(JNIEnv *env, jobject instance) {
    SLresult lresult;
    SLmillibel volume = NULL;
    if (fdPlayerVolume != NULL) {
        (*fdPlayerVolume)->GetVolumeLevel(fdPlayerVolume, &volume);
        LOGE("%S",volume)
        (*fdPlayerVolume)->SetVolumeLevel(fdPlayerVolume, (volume + 50));
        LOGE("%S",volume)
    }

}
//音量减少
extern "C"
JNIEXPORT void JNICALL
Java_com_yinjin_expandtextview_openslesdemo_VoiceUtil_volumeReduce(JNIEnv *env, jobject instance) {
    SLresult lresult;
    SLmillibel volume = NULL;
    if (fdPlayerVolume != NULL) {
        (*fdPlayerVolume)->GetVolumeLevel(fdPlayerVolume, &volume);
        LOGE("%S",volume)
        (*fdPlayerVolume)->SetVolumeLevel(fdPlayerVolume,(volume - 50));
        LOGE("%S",volume)
    }

}