#pragma once

#include <atomic>
#include <vector>
#include <string>

struct WavData {
  std::vector<float> samples; // interleaved floats (frames * channels) or mono if convertToMono==true
  int sampleRate = 0;
  int channels = 0;
  int bitsPerSample = 0;
  std::atomic<int> position = {0};

  int frames() const { return samples.size() / channels; }
};


bool LoadWavRIFF(const std::string& filename, WavData& out, bool convertToMono = true, std::string* err = nullptr);

typedef void PaStream;
PaStream* PlaySoundPortAudio(WavData& wav);
