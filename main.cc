#include <portaudio.h>

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <iostream>

#include "fileio.h"

#include <iostream>
#include <vector>
#include <deque>
#include <string>

const int STEP_COUNT = 16;
const int SAMPLE_COUNT = 4;
const int MAX_PATTERNS = 8;

struct Sample {
  WavData wav;
  std::string path;
  std::string name;
};

int main(int, char**)
{
  auto pa_err = Pa_Initialize();
  if (pa_err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(pa_err);
    return 1;
}

  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow* window = glfwCreateWindow(800, 600, "Simple DAW with ImGui + GLFW", NULL, NULL);
  if (!window) {
    std::cerr << "Failed to create GLFW window\n";
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // VSync

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  ImGui::StyleColorsDark();

  // ImGui GLFW + OpenGL3 init
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // samples
  Sample samples[SAMPLE_COUNT];
  std::vector<std::string> sample_names = {"kick", "snare", "hihat", "clap"};
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    std::string name = sample_names[i];
    std::string path = "samples/" + name + ".wav";
    samples[i].name = name;
    LoadWavRIFF(path, samples[i].wav);
  }

  bool sequence[SAMPLE_COUNT][STEP_COUNT] = { false };
  std::deque<bool> patterns[MAX_PATTERNS];
  for (int i = 0; i < MAX_PATTERNS; i++) {
    patterns[i].resize(SAMPLE_COUNT * STEP_COUNT, false);
  }

  // States over loop.
  int currentPattern = 0;
  std::vector<int> playlist = {0};
  bool play = false;
  int currentPlaylistIndex = 0;
  int currentStep = 0;
  double lastStepTime = glfwGetTime();
  const double stepDurationSec = 0.15;
  PaStream* mainStream = nullptr;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Audio block.
    if (play && !playlist.empty()) {
      double now = glfwGetTime();
      if (now - lastStepTime >= stepDurationSec) {
        int patId = playlist[currentPlaylistIndex];
        auto& pattern = patterns[patId];


        for (int i = 0; i < SAMPLE_COUNT; i++) {
          bool on = pattern[i * STEP_COUNT + currentStep];
          if (on) {
            if (mainStream) Pa_CloseStream(mainStream);
            mainStream = PlaySoundPortAudio(samples[i].wav);
            if (mainStream) Pa_StartStream(mainStream);
          }
        }

        currentStep++;
        if (currentStep >= STEP_COUNT) {
          currentStep = 0;
          currentPlaylistIndex++;
          if (currentPlaylistIndex >= (int)playlist.size()) {
            currentPlaylistIndex = 0; // loop.
          }
        }
        lastStepTime = now;
      }
    }

    // ImGui block.
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // pattern editor UI.
    ImGui::Begin("Pattern Editor");

    // pattern switch button.
    for (int p = 0; p < MAX_PATTERNS; p++) {
      ImGui::PushID(p);
      if (ImGui::RadioButton(std::to_string(p).c_str(), currentPattern == p)) {
        currentPattern = p;
        currentStep = 0;
        currentPlaylistIndex = 0;
      }
      ImGui::PopID();
      ImGui::SameLine();
    }
    ImGui::NewLine();

    auto& editPattern = patterns[currentPattern];

    // step sequencer UI.
    for (int i = 0; i < SAMPLE_COUNT; i++) {
      ImGui::Text("%s", samples[i].name.c_str());
      ImGui::SameLine();
      for (int step = 0; step < STEP_COUNT; step++) {
        ImVec4 col = (step == currentStep && play) ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushID(i * STEP_COUNT + step);
        bool& val = editPattern[i * STEP_COUNT + step];
        if (ImGui::Button(val ? "X" : "_", ImVec2(20, 20))) {
          val = !val;
        }
        ImGui::PopID();
        ImGui::PopStyleColor();
        ImGui::SameLine();
      }
      ImGui::NewLine();
    }
    ImGui::End();

    // playlist UI.
    ImGui::Begin("Playlist Editor");
    ImGui::Text("Playlist (pattern IDs):");
    for (int i = 0; i < (int)playlist.size(); i++) {
      ImGui::PushID(i);
      ImGui::Text("%d", playlist[i]);
      ImGui::SameLine();
      if (ImGui::SmallButton("Up") && i > 0) {
        std::swap(playlist[i], playlist[i - 1]);
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Down") && i < (int)playlist.size() - 1) {
        std::swap(playlist[i], playlist[i + 1]);
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Remove")) {
        playlist.erase(playlist.begin() + i);
        ImGui::PopID();
        break;
      }
      ImGui::PopID();
    }
    if (ImGui::Button("Add current pattern to playlist")) {
      playlist.push_back(currentPattern);
    }
    ImGui::End();

    // playback window.
    ImGui::Begin("Playback Control");
    if (ImGui::Button(play ? "Stop" : "Play")) {
      play = !play;
      if (!play) {
        currentStep = 0;
        currentPlaylistIndex = 0;
      }
      lastStepTime = glfwGetTime();
    }
    ImGui::Text("Current playlist index: %d / %d", currentPlaylistIndex + 1, (int)playlist.size());
    ImGui::End();

    // render.
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // imgui clean up
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  // pa clean up
  pa_err = Pa_Terminate();
  if(pa_err != paNoError) {
    std::cerr << "PortAudio error: " << Pa_GetErrorText(pa_err);
  }
  return 0;
}
