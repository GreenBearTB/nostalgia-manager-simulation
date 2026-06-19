// GLFW + OpenGL3 entry point. Used for the portable (Linux/macOS) build and for
// automated visual testing. The Windows build uses main_win32.cpp (DX11).
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include "../ui/App.h"

namespace {
std::string findDataDir(int argc, char** argv) {
    if (argc > 1 && argv[1][0] != '-') return argv[1];
    const char* candidates[] = {"data", "../data", "../../data",
                                "NostalgiaManager/data", "../NostalgiaManager/data"};
    for (const char* c : candidates) {
        std::ifstream a(std::string(c) + "/TeamsDB.csv");
        std::ifstream b(std::string(c) + "/datasources.cfg");
        if (a.is_open() || b.is_open()) return c;
    }
    return "data";
}

void glfwError(int e, const char* desc) {
    std::fprintf(stderr, "GLFW error %d: %s\n", e, desc);
}
}  // namespace

int main(int argc, char** argv) {
    std::string dataDir = findDataDir(argc, argv);

    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) {
        std::fprintf(stderr, "Failed to init GLFW\n");
        return 1;
    }
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800,
                                          "Nostalgia Manager Simulation", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().FontGlobalScale = 1.30f;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    nm::App app;
    app.init(dataDir);

    while (!glfwWindowShouldClose(window) && !app.wantQuit()) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.render();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.06f, 0.10f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
