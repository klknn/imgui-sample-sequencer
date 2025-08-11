cc_library(
    name = "fileio",
    srcs = ["fileio.cc"],
    hdrs = ["fileio.h"],
    deps = ["@portaudio//:portaudio"],
)

cc_binary(
    name = "main",
    srcs = glob(["main.cc"]),
    deps = [
        ":fileio",
        "@com_github_imgui//:core",
        "@com_github_imgui//:backends_glfw",
        "@com_github_imgui//:backends_opengl3",
        "@portaudio//:portaudio",
    ],
)
