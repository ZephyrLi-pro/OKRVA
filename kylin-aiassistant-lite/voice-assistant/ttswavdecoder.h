#ifndef TTSWAVDECODER_H
#define TTSWAVDECODER_H

#include <QByteArray>
#include <QtEndian>

namespace TtsWav {
inline bool decode(const QByteArray &wav, QByteArray *pcm, int *sampleRate, int *channels)
{
    if (!pcm || !sampleRate || !channels || wav.size() < 44
            || wav.left(4) != "RIFF" || wav.mid(8, 4) != "WAVE") {
        return false;
    }
    const auto read16 = [&wav](int pos) {
        return qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(wav.constData() + pos));
    };
    const auto read32 = [&wav](int pos) {
        return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(wav.constData() + pos));
    };
    const quint64 declaredSize = static_cast<quint64>(read32(4)) + 8;
    if (declaredSize != static_cast<quint64>(wav.size())) {
        return false;
    }
    bool haveFormat = false;
    QByteArray decoded;
    int rate = 0;
    int count = 0;
    for (int pos = 12; pos + 8 <= wav.size();) {
        const QByteArray id = wav.mid(pos, 4);
        const quint32 length = read32(pos + 4);
        const quint64 end = static_cast<quint64>(pos) + 8 + length;
        if (end > declaredSize) {
            return false;
        }
        if (id == "fmt ") {
            if (length < 16 || read16(pos + 8) != 1 || read16(pos + 22) != 16) {
                return false;
            }
            count = read16(pos + 10);
            rate = static_cast<int>(read32(pos + 12));
            if ((count != 1 && count != 2) || rate < 8000 || rate > 48000
                    || read16(pos + 20) != count * 2
                    || read32(pos + 16) != static_cast<quint32>(rate * count * 2)) {
                return false;
            }
            haveFormat = true;
        } else if (id == "data") {
            decoded.append(wav.constData() + pos + 8, static_cast<int>(length));
        }
        pos = static_cast<int>(end + (length & 1));
    }
    if (!haveFormat || decoded.isEmpty() || decoded.size() % (count * 2) != 0) {
        return false;
    }
    *pcm = decoded;
    *sampleRate = rate;
    *channels = count;
    return true;
}
}

#endif
