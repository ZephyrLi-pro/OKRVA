#ifndef SPEECHPCMEDGES_H
#define SPEECHPCMEDGES_H
#include <QByteArray>
#include <QtEndian>
#include <algorithm>

namespace SpeechPcm {
// Remove only near-digital silence at the edges. Keep a guard before/after
// speech; quiet consonants are not trimmed using a loudness/RMS threshold.
inline QByteArray trimEdges(const QByteArray &pcm, int rate, int channels, int tailMs)
{
    if (rate <= 0 || channels <= 0 || pcm.size() % (channels * 2)) return pcm;
    const int stride = channels * 2;
    const int frames = pcm.size() / stride;
    const int hop = std::max(1, rate / 100);
    auto quiet = [&](int begin, int end) {
        for (int frame = begin; frame < end; ++frame)
            for (int ch = 0; ch < channels; ++ch) {
                const int value = qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(pcm.constData()) + frame * stride + ch * 2);
                if (value < -16 || value > 16) return false;
            }
        return true;
    };
    int begin = 0, end = frames;
    while (begin + hop <= frames && quiet(begin, begin + hop)) begin += hop;
    if (begin == frames) return pcm;
    while (end - hop >= begin && quiet(end - hop, end)) end -= hop;
    begin = std::max(0, begin - rate * 60 / 1000);
    end = std::min(frames, end + rate * tailMs / 1000);
    return pcm.mid(begin * stride, (end - begin) * stride);
}
}
#endif
