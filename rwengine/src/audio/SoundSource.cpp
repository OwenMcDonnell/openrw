#include "audio/SoundSource.hpp"
#include <loaders/LoaderSDT.hpp>
#include <rw/types.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

// Rename some functions for older libavcodec/ffmpeg versions (e.g. Ubuntu
// Trusty)
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 28, 1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif

#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(57, 80, 100)
#define avio_context_free av_freep
#endif

constexpr int kNumOutputChannels = 2;
constexpr AVSampleFormat kOutputFMT = AV_SAMPLE_FMT_S16;

void SoundSource::loadFromFile(const rwfs::path& filePath) {
    // Allocate audio frame
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        RW_ERROR("Error allocating the audio frame");
        return;
    }

    // Allocate formatting context
    AVFormatContext* formatContext = nullptr;
    if (avformat_open_input(&formatContext, filePath.string().c_str(), nullptr,
                            nullptr) != 0) {
        av_frame_free(&frame);
        RW_ERROR("Error opening audio file (" << filePath << ")");
        return;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Error finding audio stream info");
        return;
    }

    // Find the audio stream
    int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1,
                                          -1, nullptr, 0);
    if (streamIndex < 0) {
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Could not find any audio stream in the file " << filePath);
        return;
    }

    AVStream* audioStream = formatContext->streams[streamIndex];
    AVCodec* codec = avcodec_find_decoder(audioStream->codecpar->codec_id);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
    AVCodecContext* codecContext = audioStream->codec;
    codecContext->codec = codec;

    // Open the codec
    if (avcodec_open2(codecContext, codecContext->codec, nullptr) != 0) {
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't open the audio codec context");
        return;
    }
#else
    // Initialize codec context for the decoder.
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't allocate a decoding context.");
        return;
    }

    // Fill the codecCtx with the parameters of the codec used in the read file.
    if (avcodec_parameters_to_context(codecContext, audioStream->codecpar) !=
        0) {
        avcodec_close(codecContext);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't find parametrs for context");
    }

    // Initialize the decoder.
    if (avcodec_open2(codecContext, codec, nullptr) != 0) {
        avcodec_close(codecContext);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't open the audio codec context");
        return;
    }
#endif

    // Expose audio metadata
    channels = kNumOutputChannels;
    sampleRate = static_cast<size_t>(codecContext->sample_rate);

    // prepare resampler
    SwrContext* swr = nullptr;

    // Start reading audio packets
    AVPacket readingPacket;
    av_init_packet(&readingPacket);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 37, 100)

    while (av_read_frame(formatContext, &readingPacket) == 0) {
        if (readingPacket.stream_index == audioStream->index) {
            AVPacket decodingPacket = readingPacket;

            while (decodingPacket.size > 0) {
                // Decode audio packet
                int gotFrame = 0;
                int len = avcodec_decode_audio4(codecContext, frame, &gotFrame,
                                                &decodingPacket);

                if (len >= 0 && gotFrame) {
                    // Write samples to audio buffer
                    for (size_t i = 0;
                         i < static_cast<size_t>(frame->nb_samples); i++) {
                        // Interleave left/right channels
                        for (size_t channel = 0; channel < channels;
                             channel++) {
                            int16_t sample = reinterpret_cast<int16_t*>(
                                frame->data[channel])[i];
                            data.push_back(sample);
                        }
                    }

                    decodingPacket.size -= len;
                    decodingPacket.data += len;
                } else {
                    decodingPacket.size = 0;
                    decodingPacket.data = nullptr;
                }
            }
        }
        av_free_packet(&readingPacket);
    }
#else

    AVFrame* resampled = av_frame_alloc();

    while (av_read_frame(formatContext, &readingPacket) == 0) {
        if (readingPacket.stream_index == audioStream->index) {
            int sendPacket = avcodec_send_packet(codecContext, &readingPacket);
            av_packet_unref(&readingPacket);
            int receiveFrame = 0;

            while ((receiveFrame =
                        avcodec_receive_frame(codecContext, frame)) == 0) {
                if (!swr) {
                    if (frame->channels == 1 || frame->channel_layout == 0) {
                        frame->channel_layout =
                            av_get_default_channel_layout(1);
                    }
                    swr = swr_alloc_set_opts(
                        nullptr,
                        AV_CH_LAYOUT_STEREO,    // output channel layout
                        kOutputFMT,             // output format
                        frame->sample_rate,     // output sample rate
                        frame->channel_layout,  // input channel layout
                        static_cast<AVSampleFormat>(
                            frame->format),  // input format
                        frame->sample_rate,  // input sample rate
                        0, nullptr);
                    if (!swr) {
                        RW_ERROR(
                            "Resampler has not been successfully allocated.");
                        return;
                    }
                    swr_init(swr);
                    if (!swr_is_initialized(swr)) {
                        RW_ERROR(
                            "Resampler has not been properly initialized.");
                        return;
                    }
                }

                // Decode audio packet
                if (receiveFrame == 0 && sendPacket == 0) {
                    // Write samples to audio buffer
                    resampled->channel_layout = AV_CH_LAYOUT_STEREO;
                    resampled->sample_rate = frame->sample_rate;
                    resampled->format = kOutputFMT;
                    resampled->channels = kNumOutputChannels;

                    swr_config_frame(swr, resampled, frame);

                    if (swr_convert_frame(swr, resampled, frame) < 0) {
                        RW_ERROR("Error resampling " << filePath << '\n');
                    }

                    for (size_t i = 0;
                         i <
                         static_cast<size_t>(resampled->nb_samples) * channels;
                         i++) {
                        data.push_back(
                            reinterpret_cast<int16_t*>(resampled->data[0])[i]);
                    }
                    av_frame_unref(resampled);
                }
            }
        }
    }

    /// Free all data used by the resampled frame.
    av_frame_free(&resampled);

#endif

    // Cleanup
    /// Free all data used by the frame.
    av_frame_free(&frame);

    /// Free resampler
    swr_free(&swr);

    /// Close the context and free all data associated to it, but not the
    /// context itself.
    avcodec_close(codecContext);

    /// Free the context itself.
    avcodec_free_context(&codecContext);

    /// We are done here. Close the input.
    avformat_close_input(&formatContext);
}

/// Structure for input data
struct InputData {
    uint8_t* ptr = nullptr;
    size_t size{};  ///< size left in the buffer
};

/// Low level function for copying data from handler (opaque)
/// to buffer.
static int read_packet(void* opaque, uint8_t* buf, int buf_size) {
    auto* input = reinterpret_cast<InputData*>(opaque);
    buf_size = FFMIN(buf_size, input->size);
    /* copy internal data to buf */
    memcpy(buf, input->ptr, buf_size);
    input->ptr += buf_size;
    input->size -= buf_size;
    return buf_size;
}

void SoundSource::loadSfx(LoaderSDT& sdt, size_t index, bool asWave) {
    // Allocate audio frame
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        RW_ERROR("Error allocating the audio frame");
        return;
    }

    /// Now we need to prepare "custom" format context
    /// We need sdt loader for that purpose
    std::unique_ptr<char[]> raw_sound = sdt.loadToMemory(index, asWave);
    if (!raw_sound) {
        av_frame_free(&frame);
        RW_ERROR("Error loading sound");
        return;
    }

    /// Prepare input
    InputData input{};
    input.size = sizeof(WaveHeader) + sdt.assetInfo.size;
    /// Store start ptr of data to be able freed memory later
    auto inputDataStart = std::make_unique<uint8_t[]>(
        input.size);
    input.ptr = inputDataStart.get();

    /// Alocate memory for buffer
    /// Memory freeded at the end
    static constexpr size_t ioBufferSize = 4096;
    auto ioBuffer = static_cast<uint8_t*>(av_malloc(ioBufferSize));

    /// Cast pointer, in order to match required layout for ffmpeg
    input.ptr = reinterpret_cast<uint8_t*>(raw_sound.get());

    /// Finally prepare our "custom" format context
    AVIOContext* avioContext = avio_alloc_context(
        ioBuffer, ioBufferSize, 0, &input, &read_packet, nullptr, nullptr);
    AVFormatContext* formatContext = avformat_alloc_context();
    formatContext->pb = avioContext;

    if (avformat_open_input(&formatContext, "nothint", nullptr, nullptr) != 0) {
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        av_frame_free(&frame);
        RW_ERROR("Error opening audio file (" << index << ")");
        return;
    }

    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Error finding audio stream info");
        return;
    }

    // Find the audio stream
    // AVCodec* codec = nullptr;
    int streamIndex = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1,
                                          -1, nullptr, 0);
    if (streamIndex < 0) {
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Could not find any audio stream in the file ");
        return;
    }

    AVStream* audioStream = formatContext->streams[streamIndex];
    AVCodec* codec = avcodec_find_decoder(audioStream->codecpar->codec_id);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 5, 0)
    AVCodecContext* codecContext = audioStream->codec;
    codecContext->codec = codec;

    // Open the codec
    if (avcodec_open2(codecContext, codecContext->codec, nullptr) != 0) {
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't open the audio codec context");
        return;
    }
#else
    // Initialize codec context for the decoder.
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        av_frame_free(&frame);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't allocate a decoding context.");
        return;
    }

    // Fill the codecCtx with the parameters of the codec used in the read file.
    if (avcodec_parameters_to_context(codecContext, audioStream->codecpar) !=
        0) {
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        avcodec_close(codecContext);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't find parametrs for context");
        return;
    }

    // Initialize the decoder.
    if (avcodec_open2(codecContext, codec, nullptr) != 0) {
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        avcodec_close(codecContext);
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        RW_ERROR("Couldn't open the audio codec context");
        return;
    }
#endif

    // Expose audio metadata
    channels = static_cast<size_t>(codecContext->channels);
    sampleRate = sdt.assetInfo.sampleRate;

    // OpenAL only supports mono or stereo, so error on more than 2 channels
    if (channels > 2) {
        RW_ERROR("Audio has more than two channels");
        av_free(formatContext->pb->buffer);
        avio_context_free(&formatContext->pb);
        av_frame_free(&frame);
        avcodec_close(codecContext);
        avformat_close_input(&formatContext);
        return;
    }

    // Start reading audio packets
    AVPacket readingPacket;
    av_init_packet(&readingPacket);

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(57, 37, 100)

    while (av_read_frame(formatContext, &readingPacket) == 0) {
        if (readingPacket.stream_index == audioStream->index) {
            AVPacket decodingPacket = readingPacket;

            while (decodingPacket.size > 0) {
                // Decode audio packet
                int gotFrame = 0;
                int len = avcodec_decode_audio4(codecContext, frame, &gotFrame,
                                                &decodingPacket);

                if (len >= 0 && gotFrame) {
                    // Write samples to audio buffer

                    for (size_t i = 0;
                         i < static_cast<size_t>(frame->nb_samples); i++) {
                        // Interleave left/right channels
                        for (size_t channel = 0; channel < channels;
                             channel++) {
                            int16_t sample = reinterpret_cast<int16_t*>(
                                frame->data[channel])[i];
                            data.push_back(sample);
                        }
                    }

                    decodingPacket.size -= len;
                    decodingPacket.data += len;
                } else {
                    decodingPacket.size = 0;
                    decodingPacket.data = nullptr;
                }
            }
        }
        av_free_packet(&readingPacket);
    }
#else

    while (av_read_frame(formatContext, &readingPacket) == 0) {
        if (readingPacket.stream_index == audioStream->index) {
            AVPacket decodingPacket = readingPacket;

            int sendPacket = avcodec_send_packet(codecContext, &decodingPacket);
            int receiveFrame = 0;

            while ((receiveFrame =
                        avcodec_receive_frame(codecContext, frame)) == 0) {
                // Decode audio packet

                if (receiveFrame == 0 && sendPacket == 0) {
                    // Write samples to audio buffer

                    for (size_t i = 0;
                         i < static_cast<size_t>(frame->nb_samples); i++) {
                        // Interleave left/right channels
                        for (size_t channel = 0; channel < channels;
                             channel++) {
                            int16_t sample = reinterpret_cast<int16_t*>(
                                frame->data[channel])[i];
                            data.push_back(sample);
                        }
                    }
                }
            }
        }
        av_packet_unref(&readingPacket);
    }

#endif

    // Cleanup
    /// Free all data used by the frame.
    av_frame_free(&frame);

    /// Close the context and free all data associated to it, but not the
    /// context itself.
    avcodec_close(codecContext);

    /// Free the context itself.
    avcodec_free_context(&codecContext);

    /// Free our custom AVIO.
    av_free(formatContext->pb->buffer);
    avio_context_free(&formatContext->pb);

    /// We are done here. Close the input.
    avformat_close_input(&formatContext);
}
