#include <QCoreApplication>
#include <QAudioFormat>
#include <QAudioDeviceInfo>
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QAudioInput>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QAudioFormat audioFormat;

    audioFormat.setByteOrder(QAudioFormat::LittleEndian);
    audioFormat.setChannelCount(1);
    audioFormat.setCodec("audio/pcm");
    audioFormat.setSampleRate(16000);
    audioFormat.setSampleSize(16);
    audioFormat.setSampleType(QAudioFormat::SignedInt);

    // 判断设备，查看是否存在
    QAudioDeviceInfo devInfo = QAudioDeviceInfo::defaultInputDevice();
    if (devInfo.isNull()) {
        qWarning() << "No recording device was found!";
    }

    // 不支持格式，使用最接近格式
    if (!devInfo.isFormatSupported(audioFormat)) {  // 当前使用设备是否支持
        qWarning() << "Automatic conversion is made to the nearest format!";
        audioFormat = devInfo.nearestFormat(audioFormat);  // 转换为最接近格式
    }

    QAudioInput *audioInput = new QAudioInput(devInfo, audioFormat, nullptr);

    QFile destinationFile;
    destinationFile.setFileName("./test_record.flv");
    destinationFile.open( QIODevice::ReadWrite | QIODevice::Truncate );

    audioInput->start(&destinationFile);

    QTimer *timer = new QTimer();
    QObject::connect(timer, &QTimer::timeout, [&destinationFile, audioInput](){
        // 处理超时的逻辑
        qDebug() << "Timer timeout!";
        audioInput->stop();
        destinationFile.close();
        delete audioInput;
    });
    timer->start(5000);

    return a.exec();
}
