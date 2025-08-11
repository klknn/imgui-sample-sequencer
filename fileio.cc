#include "fileio.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>
#include <thread>

#include <portaudio.h>

/*
  LoadWavRIFF:
  - filename: 読み込む wav ファイルパス
  - out: 読み出した波形データとメタデータを格納
  - convertToMono: true の場合 channels>1 の時点でモノラルに平均化して返す
  - err: nullptr でなければエラーメッセージを入れる
  return: 成功なら true
*/
bool LoadWavRIFF(const std::string& filename, WavData& out, bool convertToMono, std::string* err) {
  auto set_err = [&](const std::string &s){
    if (err) *err = s;
  };

  std::ifstream ifs(filename, std::ios::binary);
  if (!ifs) {
    set_err("failed to open file: " + filename);
    return false;
  }

  auto read_u32 = [&](uint32_t &v)->bool{
    unsigned char b[4];
    ifs.read(reinterpret_cast<char*>(b), 4);
    if (!ifs) return false;
    v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
  };
  auto read_u16 = [&](uint16_t &v)->bool{
    unsigned char b[2];
    ifs.read(reinterpret_cast<char*>(b), 2);
    if (!ifs) return false;
    v = (uint16_t)b[0] | ((uint16_t)b[1] << 8);
    return true;
  };
  auto read_bytes = [&](char* buf, std::streamsize n)->bool{
    ifs.read(buf, n);
    return static_cast<std::streamsize>(ifs.gcount()) == n;
  };

  // RIFF header
  char riff[4];
  if (!read_bytes(riff, 4)) { set_err("can't read RIFF"); return false; }
  if (std::memcmp(riff, "RIFF", 4) != 0) { set_err("not a RIFF file"); return false; }

  uint32_t riffSize;
  if (!read_u32(riffSize)) { set_err("can't read RIFF size"); return false; }

  char wave[4];
  if (!read_bytes(wave, 4)) { set_err("can't read WAVE id"); return false; }
  if (std::memcmp(wave, "WAVE", 4) != 0) { set_err("not a WAVE file"); return false; }

  // variables to fill
  uint16_t audioFormat = 0;
  uint16_t numChannels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataChunkPos = 0;
  uint32_t dataChunkSize = 0;

  // iterate chunks
  while (ifs.good()) {
    char chunkId[4];
    if (!read_bytes(chunkId, 4)) break; // no more chunks
    uint32_t chunkSize;
    if (!read_u32(chunkSize)) { set_err("unexpected EOF reading chunk size"); return false; }

    if (std::memcmp(chunkId, "fmt ", 4) == 0) {
      // parse fmt chunk (minimum 16 bytes for PCM)
      uint16_t fmt_audioFormat;
      uint16_t fmt_numChannels;
      uint32_t fmt_sampleRate;
      uint32_t fmt_byteRate;
      uint16_t fmt_blockAlign;
      uint16_t fmt_bitsPerSample;

      if (!read_u16(fmt_audioFormat) ||
          !read_u16(fmt_numChannels) ||
          !read_u32(fmt_sampleRate) ||
          !read_u32(fmt_byteRate) ||
          !read_u16(fmt_blockAlign) ||
          !read_u16(fmt_bitsPerSample)) {
        set_err("failed to read fmt chunk");
        return false;
      }

      audioFormat = fmt_audioFormat;
      numChannels = fmt_numChannels;
      sampleRate = fmt_sampleRate;
      bitsPerSample = fmt_bitsPerSample;

      // fmt chunk may have extra bytes (cbSize etc.), skip remainder
      std::streamoff remaining = (std::streamoff)chunkSize - 16;
      if (remaining > 0) {
        ifs.seekg(remaining, std::ios::cur);
        if (!ifs) { set_err("failed to skip extra fmt bytes"); return false; }
      }
      // pad byte if chunkSize odd
      if (chunkSize & 1) ifs.seekg(1, std::ios::cur);
    }
    else if (std::memcmp(chunkId, "data", 4) == 0) {
      // data chunk: remember position & size, then break to parse
      dataChunkPos = static_cast<uint32_t>(ifs.tellg());
      dataChunkSize = chunkSize;
      // seek past data (we'll come back by seeking to dataChunkPos)
      ifs.seekg(chunkSize, std::ios::cur);
      if (!ifs) { set_err("failed to skip data chunk"); return false; }
      if (chunkSize & 1) ifs.seekg(1, std::ios::cur);
      // continue (we may have other chunks after data, but we already got data info)
    }
    else {
      // skip unknown chunk (with padding)
      std::streamoff skip = chunkSize;
      ifs.seekg(skip, std::ios::cur);
      if (!ifs) { set_err("failed to skip unknown chunk"); return false; }
      if (chunkSize & 1) ifs.seekg(1, std::ios::cur);
    }
  }

  if (dataChunkPos == 0 || dataChunkSize == 0) { set_err("no data chunk found"); return false; }
  if (sampleRate == 0 || numChannels == 0 || bitsPerSample == 0) {
    set_err("fmt chunk not found or invalid");
    return false;
  }

  // read data chunk
  ifs.clear();
  ifs.seekg(dataChunkPos);
  if (!ifs) { set_err("seek to data failed"); return false; }

  std::vector<unsigned char> rawData;
  try {
    rawData.resize(dataChunkSize);
  } catch (...) {
    set_err("out of memory allocating rawData");
    return false;
  }
  if (!read_bytes(reinterpret_cast<char*>(rawData.data()), (std::streamsize)dataChunkSize)) {
    set_err("failed to read data chunk bytes");
    return false;
  }

  // parse samples
  const size_t bytesPerSample = (bitsPerSample + 7) / 8;
  const size_t frameBytes = bytesPerSample * numChannels;
  if (frameBytes == 0) { set_err("invalid frame size"); return false; }
  const size_t frameCount = dataChunkSize / frameBytes;

  // prepare output
  std::vector<float> interleaved;
  interleaved.resize(frameCount * numChannels);

  const unsigned char* ptr = rawData.data();
  size_t offset = 0;

  auto read_i24_to_int32 = [&](const unsigned char* p)->int32_t {
    // little endian 3 bytes -> sign extend to 32
    int32_t v = (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16);
    if (v & 0x00800000) v |= 0xFF000000; // sign extend
    return v;
  };

  for (size_t f = 0; f < frameCount; ++f) {
    for (int ch = 0; ch < (int)numChannels; ++ch) {
      float sample = 0.0f;
      const unsigned char* sPtr = ptr + offset;

      if (audioFormat == 3 /*IEEE float*/ && bitsPerSample == 32) {
        // read float32 little-endian
        uint32_t u = (uint32_t)sPtr[0] | ((uint32_t)sPtr[1] << 8) | ((uint32_t)sPtr[2] << 16) | ((uint32_t)sPtr[3] << 24);
        float fval;
        std::memcpy(&fval, &u, sizeof(float));
        sample = fval;
      }
      else if (audioFormat == 1 /*PCM integer*/) {
        if (bitsPerSample == 8) {
          // unsigned 8-bit PCM (0..255) -> [-1,1]
          uint8_t u8 = sPtr[0];
          sample = (static_cast<int>(u8) - 128) / 128.0f;
        }
        else if (bitsPerSample == 16) {
          int16_t v = (int16_t)( (int16_t)sPtr[0] | ((int16_t)sPtr[1] << 8) );
          sample = v / 32768.0f;
        }
        else if (bitsPerSample == 24) {
          int32_t v = read_i24_to_int32(sPtr);
          sample = v / 8388608.0f; // 2^23
        }
        else if (bitsPerSample == 32) {
          int32_t v = (int32_t)( (int32_t)sPtr[0] | ((int32_t)sPtr[1] << 8) | ((int32_t)sPtr[2] << 16) | ((int32_t)sPtr[3] << 24) );
          sample = v / 2147483648.0f; // 2^31
        }
        else {
          set_err("unsupported bitsPerSample for PCM: " + std::to_string(bitsPerSample));
          return false;
        }
      }
      else {
        set_err("unsupported audio format / bitsPerSample combo");
        return false;
      }

      interleaved[f * numChannels + ch] = sample;
      offset += bytesPerSample;
    }
  }

  // optional mono conversion
  if (convertToMono && numChannels > 1) {
    out.samples.resize(frameCount);
    for (size_t f = 0; f < frameCount; ++f) {
      double sum = 0.0;
      for (int ch = 0; ch < (int)numChannels; ++ch) sum += interleaved[f * numChannels + ch];
      out.samples[f] = static_cast<float>(sum / numChannels);
    }
    out.channels = 1;
  } else {
    out.samples = std::move(interleaved);
    out.channels = numChannels;
  }

  out.sampleRate = static_cast<int>(sampleRate);
  out.bitsPerSample = static_cast<int>(bitsPerSample);

  return true;
}


int PlayWavDataPaCallback(
    const void* input,
    void* output,
    unsigned long frameCount,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData) {
  auto& state = *static_cast<WavData*>(userData);
  float* out = static_cast<float*>(output);
  int pos = state.position.load();

  for (unsigned long i = 0; i < frameCount; ++i) {
    if (pos < state.frames()) {
      for (int ch = 0; ch < state.channels; ++ch) {
        *out++ = state.samples[pos * state.channels + ch];
      }
      pos++;
    } else {
      // pad silence.
      for (int ch = 0; ch < state.channels; ++ch) {
        *out++ = 0.0f;
      }
    }
  }
  state.position.store(pos);

  if (pos >= state.frames()) return paComplete;

  return paContinue;
}

PaStream* PlaySoundPortAudio(WavData& wav) {
  std::string err;
  PaStream* stream;
  wav.position.store(0);
  auto errPa = Pa_OpenDefaultStream(
      &stream,
      0, // no input.
      wav.channels,
      paFloat32,
      wav.sampleRate,
      paFramesPerBufferUnspecified,
      PlayWavDataPaCallback,
      &wav);
  if (errPa != paNoError) {
    std::cerr << "OpenDefaultStream error: " << Pa_GetErrorText(errPa) << "\n";
    return nullptr;
  }
  return stream;
}
