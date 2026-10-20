#include <iostream>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include "ui/spreadsheet_ui.h"

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

int main(int argc, char** argv) {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(1200, 800, "Excel - Spreadsheet (C++)", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Load clean modern font if available
    ImFont* font = nullptr;
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\segoeui.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",
        "C:\\Windows\\Fonts\\calibri.ttf"
    };
    for (const char* path : fontPaths) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);
            font = io.Fonts->AddFontFromFileTTF(path, 15.0f);
            if (font) break;
        }
    }
    if (!font) {
        io.Fonts->AddFontDefault();
    }

    // Setup modern light theme matching x-spreadsheet aesthetics
    ImGui::StyleColorsLight();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.96f, 0.96f, 0.97f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.92f, 0.94f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.85f, 0.90f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.78f, 0.85f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.95f, 0.95f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.88f, 0.92f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.86f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.88f, 0.88f, 0.88f, 1.0f);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    xspreadsheet::SpreadsheetUI app;

    // If a JSON file is passed as command line argument, load it
    if (argc > 1) {
        app.book.loadFromFile(argv[1]);
    }

    // Main loop (Hybrid Engine: 60 FPS on scroll, event-driven on cell edit, 0 FPS on idle)
    while (!glfwWindowShouldClose(window)) {
        float waitTimeout = -1.0f; // -1 means sleep indefinitely (0 FPS idle)

        if (app.isFastContinuousNeeded()) {
            waitTimeout = 0.016f; // 60 FPS smooth motion while scrolling or dragging
        } else if (app.isEditingCellActive()) {
            waitTimeout = 0.35f;  // Event-driven cell editing: redraws instantly on keypress, sleeps up to 350ms for cursor blink
        }

        if (waitTimeout > 0.0f) {
            glfwWaitEventsTimeout(waitTimeout);
        } else {
            // 0 FPS: Thread sleeps completely with 0% CPU and 0% GPU until user moves mouse or presses a key
            glfwWaitEvents();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.render(window);

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.95f, 0.95f, 0.96f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
