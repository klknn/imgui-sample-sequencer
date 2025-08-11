cc_binary(
    name = "main",
    srcs = glob(["main.cc"]),
    deps = [
        "@com_github_imgui//:core",
        "@com_github_imgui//:backends_glfw",
        "@com_github_imgui//:backends_opengl3",
    ],
    copts = ["/utf-8"],
    linkopts = ["Winmm.lib"],
)
