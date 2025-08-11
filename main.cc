#include <portaudio.h>

#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <iostream>

#include "fileio.h"

const int STEP_COUNT = 16;
const int SAMPLE_COUNT = 4;

struct Sample {
  WavData wav;
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

  // if (gl3wInit() != 0) {
  //   std::cerr << "Failed to initialize OpenGL loader\n";
  //   return 1;
  // }

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
    WavData wav;
    LoadWavRIFF(path, wav);
    samples[i] = {wav, name};
  }

  bool sequence[SAMPLE_COUNT][STEP_COUNT] = { false };

  int currentStep = 0;
  double lastStepTime = glfwGetTime();
  const double stepDurationSec = 0.15; // 150ms

  bool play = false;

  // main loop
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();

    // update sequencer
    if (play) {
      double now = glfwGetTime();
      if (now - lastStepTime >= stepDurationSec) {
        // play
        for (int i = 0; i < SAMPLE_COUNT; i++) {
          if (sequence[i][currentStep]) {
            // MultiByteToWideChar(CP_ACP, 0, samples[i].filename, -1, path, 100);
            // PlaySound(path, NULL, SND_FILENAME | SND_ASYNC);
          }
        }
        currentStep = (currentStep + 1) % STEP_COUNT;
        lastStepTime = now;
      }
    }

    // start gui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Simple Sampler + Step Sequencer (GLFW Backend)");

    if (ImGui::Button(play ? "Stop" : "Play")) {
      play = !play;
      if (!play) currentStep = 0;
      lastStepTime = glfwGetTime();
    }
    ImGui::SameLine();
    ImGui::Text("Step: %d / %d", currentStep + 1, STEP_COUNT);

    ImGui::Separator();

    for (int i = 0; i < SAMPLE_COUNT; i++) {
      ImGui::Text("%s", samples[i].name.c_str());
      ImGui::SameLine();
      for (int step = 0; step < STEP_COUNT; step++) {
        ImVec4 col = (step == currentStep && play) ? ImVec4(0.4f, 0.8f, 0.4f, 1.0f) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushID(i * STEP_COUNT + step);
        if (ImGui::Button(sequence[i][step] ? "X" : "_", ImVec2(20, 20))) {
          sequence[i][step] = !sequence[i][step];
        }
        ImGui::PopID();
        ImGui::PopStyleColor();
        ImGui::SameLine();
      }
      ImGui::NewLine();
    }

    ImGui::End();

    // render
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
