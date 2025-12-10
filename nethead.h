#pragma once
enum MSG_TYPE
{
    //视频帧发送/接收
    IMG_SEND = 0,
    IMG_RECV,

    //音频发送/接收
    AUDIO_SEND,
    AUDIO_RECV,

    //文本发送/接收
    TEXT_SEND,
    TEXT_RECV,

    CREATE_MEETING,
    EXIT_MEETING,
    JOIN_MEETING,
    //关闭摄像头 开启摄像头不需要，用IMG_SEND发送视频帧到服务器即可
    CLOSE_CAMERA,

    //服务器回响
    CREATE_MEETING_RESPONSE = 20,
    PARTNER_EXIT,
    PARTNER_JOIN_OTHER,//告诉当前会议里面的所有人新伙伴加入
    JOIN_MEETING_RESPONSE,
    PARTNER_JOIN_SELF,//将会议内其他人的信息告诉自己，便于添加partner	
};
