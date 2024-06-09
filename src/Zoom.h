#ifndef MEETING_SDK_LINUX_SAMPLE_ZOOM_H
#define MEETING_SDK_LINUX_SAMPLE_ZOOM_H

#include <iostream>
#include <chrono>
#include <string>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <future>
#include <thread>

#include <jwt-cpp/jwt.h>

#include "Config.h"
#include "util/Singleton.h"
#include "util/Log.h"

#include "zoom_sdk.h"
#include "rawdata/zoom_rawdata_api.h"
#include "rawdata/rawdata_renderer_interface.h"

#include "meeting_service_components/meeting_audio_interface.h"
#include "meeting_service_components/meeting_participants_ctrl_interface.h"
#include "setting_service_interface.h"
#include "meeting_service_components/meeting_chat_interface.h"

#include "events/AuthServiceEvent.h"
#include "events/MeetingServiceEvent.h"
#include "events/MeetingReminderEvent.h"
#include "events/MeetingRecordingCtrlEvent.h"

#include "raw_record/ZoomSDKRendererDelegate.h"
#include "raw_record/ZoomSDKAudioRawDataDelegate.h"

using namespace std;
using namespace jwt;
using namespace ZOOMSDK;

typedef chrono::time_point<chrono::system_clock> time_point;

class Zoom : public Singleton<Zoom> {
    friend class Singleton<Zoom>;

    Config m_config;
    string m_jwt;
    time_point m_iat;
    time_point m_exp;
    IMeetingService* m_meetingService;
    ISettingService* m_settingService;
    IAuthService* m_authService;
    IZoomSDKRenderer* m_videoHelper;
    ZoomSDKRendererDelegate* m_videoSource;
    IZoomSDKAudioRawDataHelper* m_audioHelper;
    ZoomSDKAudioRawDataDelegate* m_audioSource;
    unordered_set<string> m_consentingUsers;

    unordered_set<string> participants; // Add this declaration
    unordered_map<string, bool> consentStatus; // Add this declaration
    bool recordingStarted = false; // Add this declaration

    SDKError createServices();
    void generateJWT(const string& key, const string& secret);
    SDKError sendConsentRequest(IMeetingChatController* chatCtrl);
    SDKError startRecordingIfAllConsented();
    void checkConsentStatus();

    function<void()> onAuth = [&]() {
        auto e = isMeetingStart() ? start() : join();
        string action = isMeetingStart() ? "start" : "join";
        if (hasError(e, action + " a meeting")) exit(e);
    };

    function<void()> onJoin = [&]() {
        auto* reminderController = m_meetingService->GetMeetingReminderController();
        reminderController->SetEvent(new MeetingReminderEvent());
        if (m_config.useRawRecording()) {
            auto recordingCtrl = m_meetingService->GetMeetingRecordingController();
            function<void(bool)> onRecordingPrivilegeChanged = [&](bool canRec) {
                if (canRec) startRecordingIfAllConsented();
                else stopRawRecording();
            };
            auto recordingEvent = new MeetingRecordingCtrlEvent(onRecordingPrivilegeChanged);
            recordingCtrl->SetEvent(recordingEvent);
            checkConsentStatus();
        }
    };

public:
    SDKError init();
    SDKError auth();
    SDKError config(int ac, char** av);
    SDKError join();
    SDKError start();
    SDKError startRawRecording();
    SDKError stopRawRecording();
    void fetchParticipants();
    void sendMessage(const std::string& message);
    void onConsentUpdate(const std::vector<std::string>& consentingUsers);
    void sendConsentReminder();
    void startConsentCheck();
    SDKError leave();
    SDKError clean();
    bool isMeetingStart();
    static bool hasError(SDKError e, const string& action = "");
};

#endif // MEETING_SDK_LINUX_SAMPLE_ZOOM_H
