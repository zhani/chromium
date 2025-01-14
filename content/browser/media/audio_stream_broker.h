// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "media/mojo/interfaces/audio_input_stream.mojom.h"
#include "media/mojo/interfaces/audio_output_stream.mojom.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"

namespace audio {
namespace mojom {
class StreamFactory;
}
}  // namespace audio

namespace base {
class UnguessableToken;
}

namespace media {
class AudioParameters;
class UserInputMonitorBase;
}

namespace content {

// An AudioStreamBroker is used to broker a connection between a client
// (typically renderer) and the audio service. It also sets up all objects
// used for monitoring the stream.
class CONTENT_EXPORT AudioStreamBroker {
 public:
  class CONTENT_EXPORT LoopbackSink {
   public:
    LoopbackSink();
    virtual ~LoopbackSink();
    virtual void OnSourceGone() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(LoopbackSink);
  };

  class CONTENT_EXPORT LoopbackSource {
   public:
    LoopbackSource();
    virtual ~LoopbackSource();
    virtual void AddLoopbackSink(LoopbackSink* sink) = 0;
    virtual void RemoveLoopbackSink(LoopbackSink* sink) = 0;
    virtual const base::UnguessableToken& GetGroupID() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(LoopbackSource);
  };

  using DeleterCallback = base::OnceCallback<void(AudioStreamBroker*)>;

  AudioStreamBroker(int render_process_id, int render_frame_id);
  virtual ~AudioStreamBroker();

  virtual void CreateStream(audio::mojom::StreamFactory* factory) = 0;

  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

 protected:
  const int render_process_id_;
  const int render_frame_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioStreamBroker);
};

// Used for dependency injection into ForwardingAudioStreamFactory.
class CONTENT_EXPORT AudioStreamBrokerFactory {
 public:
  static std::unique_ptr<AudioStreamBrokerFactory> CreateImpl();

  AudioStreamBrokerFactory();
  virtual ~AudioStreamBrokerFactory();

  virtual std::unique_ptr<AudioStreamBroker> CreateAudioInputStreamBroker(
      int render_process_id,
      int render_frame_id,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      media::UserInputMonitorBase* user_input_monitor,
      bool enable_agc,
      audio::mojom::AudioProcessingConfigPtr processing_config,
      AudioStreamBroker::DeleterCallback deleter,
      mojom::RendererAudioInputStreamFactoryClientPtr
          renderer_factory_client) = 0;

  virtual std::unique_ptr<AudioStreamBroker> CreateAudioLoopbackStreamBroker(
      int render_process_id,
      int render_frame_id,
      AudioStreamBroker::LoopbackSource* source,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool mute_source,
      AudioStreamBroker::DeleterCallback deleter,
      mojom::RendererAudioInputStreamFactoryClientPtr
          renderer_factory_client) = 0;

  virtual std::unique_ptr<AudioStreamBroker> CreateAudioOutputStreamBroker(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      const base::Optional<base::UnguessableToken>& processing_id,
      AudioStreamBroker::DeleterCallback deleter,
      media::mojom::AudioOutputStreamProviderClientPtr client) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioStreamBrokerFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_STREAM_BROKER_H_
