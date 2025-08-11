#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include <windows.h>  // PlaySound API
#include <iostream>
#include <vector>
#include <deque>
#include <string>

const int STEP_COUNT = 16;
const int SAMPLE_COUNT = 4;
const int MAX_PATTERNS = 8;

struct Sample {
  const char* filename;
  const char* name;
};

int main(int, char**)
{
  if (!glfwInit()) {
    std::cerr << "Failed to initialize GLFW\n";
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

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
  Sample samples[SAMPLE_COUNT] = {
    {"samples/kick.wav", "Kick"},
    {"samples/snare.wav", "Snare"},
    {"samples/hihat.wav", "Hi-Hat"},
    {"samples/clap.wav", "Clap"}
  };
  wchar_t path[100];

  bool sequence[SAMPLE_COUNT][STEP_COUNT] = { false };
  /*
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
    MultiByteToWideChar(CP_ACP, 0, samples[i].filename, -1, path, 100);
    PlaySound(path, NULL, SND_FILENAME | SND_ASYNC);
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
    ImGui::Text("%s", samples[i].name);
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
  */
  // パターンリスト：各パターンは bool[SAMPLE_COUNT][STEP_COUNT]
  std::deque<bool> patterns[MAX_PATTERNS];
  for (int i = 0; i < MAX_PATTERNS; i++) {
    patterns[i].resize(SAMPLE_COUNT * STEP_COUNT, false);
  }

  // 現在の編集パターンID
  int currentPattern = 0;

  // プレイリストはパターンIDの順番
  std::vector<int> playlist = {0, 1};  // 例：最初はパターン0→パターン1

  // プレイ中のプレイリスト再生状態
  bool play = false;
  int currentPlaylistIndex = 0;
  int currentStep = 0;
  double lastStepTime = glfwGetTime();
  const double stepDurationSec = 0.15;

  while (!glfwWindowShouldClose(window))
  {
    // イベント処理
    glfwPollEvents();

    // 再生処理
    if (play && !playlist.empty()) {
      double now = glfwGetTime();
      if (now - lastStepTime >= stepDurationSec) {
        // 現在のパターンを取り出す
        int patId = playlist[currentPlaylistIndex];
        auto& pattern = patterns[patId];

        // 今のステップで鳴らす音をPlaySound
        for (int i = 0; i < SAMPLE_COUNT; i++) {
          bool on = pattern[i * STEP_COUNT + currentStep];
          if (on) {
            MultiByteToWideChar(CP_ACP, 0, samples[i].filename, -1, path, 100);
            PlaySound(path, NULL, SND_FILENAME | SND_ASYNC);
            // PlaySound(samples[i].filename, NULL, SND_FILENAME | SND_ASYNC);
          }
        }

        currentStep++;
        if (currentStep >= STEP_COUNT) {
          currentStep = 0;
          // プレイリストの次パターンへ
          currentPlaylistIndex++;
          if (currentPlaylistIndex >= (int)playlist.size()) {
            currentPlaylistIndex = 0; // ループ再生
          }
        }
        lastStepTime = now;
      }
    }

    // ImGuiフレーム開始
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // パターン編集ウィンドウ
    ImGui::Begin("Pattern Editor");

    // パターン切替ボタン
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

    // 編集対象のパターンを参照
    auto& editPattern = patterns[currentPattern];

    // ステップシーケンサーのグリッド描画
    for (int i = 0; i < SAMPLE_COUNT; i++) {
      ImGui::Text("%s", samples[i].name);
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

    // プレイリスト編集ウィンドウ
    ImGui::Begin("Playlist Editor");

    ImGui::Text("Playlist (pattern IDs):");

    // プレイリストを並べて表示＋順序変更（簡易）
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

    // パターン追加ボタン
    if (ImGui::Button("Add current pattern to playlist")) {
      playlist.push_back(currentPattern);
    }

    ImGui::End();

    // プレイコントロール
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

    // 描画処理
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // clean up
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
