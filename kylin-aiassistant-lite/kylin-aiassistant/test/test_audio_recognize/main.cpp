#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    return a.exec();
}

/*
 * Copyright 2024 KylinSoft Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <thread>
#include <stdio.h>
#include <vector>
#include <fstream>

#include <kylin-ai/ai-base/speech.h>
#include <gio/gio.h>
#include <gio/giotypes.h>

std::vector<char> readAudioData(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filePath << std::endl;
        return {};
    }

    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> audioData(fileSize);
    file.read(audioData.data(), fileSize);
    return audioData;
}

int main(int argc, char *argv[]) {

    SpeechSession session;
    speech_create_session(&session);
    speech_init_session(session);

    int a = 10;
    speech_set_recognizing_callback(
                session,
                [](RecognitionResult result, void* userData) {
        std::cout << "---testSpeechApi-RecognitionResult--data---" << result.text_data
                  << "---rate---" << result.error_code
                  << "---userData---" << *(int*)userData << std::endl;

    }, (void*)&a);
    speech_set_recognized_callback(
                session, [](void* userData) {
        std::cout << "---testSpeechApi--Recognition finished---"
                  << "---userData---" << userData << std::endl;
    }, (void*)&a);

    speech_set_synthesizing_callback(
                session,
                [](SynthesisResult result, void* userData){
        std::cout << "---testSpeechApi-SynthesisResult--data---" << result.audio_data
                  << "---rate---" << result.audio_rate
                  << "---userData---" << userData << std::endl;
    }, (void*)&a);

    speech_set_synthesized_callback(
                session, [](void* userData) {
        std::cout << "---testSpeechApi--synthesized finished---"
                  << "---userData---" << userData << std::endl;
    }, (void*)&a);


    auto data = readAudioData("/home/zkx/tmpSubsystem/16k_10.pcm");
    speech_start_continuous_recognition(session, "\{\"rate\": 1600}");
    speech_continuous_recognition_write_audio_data_async(session, data.data(), data.size());

//    sleep(5);
//    speech_stop_continuous_recognition(session);
//    speech_stop_continuous_recognition(session);

//    speech_recognize_once(session, "\{\"rate\": 1600}", data.data(), data.size());

//    speech_start_continuous_synthesis(session, "\{\"speed\": 5}");
//    speech_continuous_synthesize_write_text(session, "你好");
//    speech_continuous_synthesize_write_text(session, "麒麟软件");
//    speech_continuous_synthesize_write_text(session, "你吃饭了么，你吃饭了么，你吃饭了么，你吃饭了么，你吃饭了么，你吃饭了么");

//    sleep(3);
//    speech_stop_continuous_synthesis(session);

//    speech_synthesize_once_async(session, "\{\"speed\": 5}", "你好,麒麟");

//    auto *loop_ = g_main_loop_new(nullptr, false);
//    g_main_loop_run(loop_);

    QCoreApplication a(argc, argv);

    return a.exec();
}

