#include <jni.h>
#include <string>
#include <android/log.h>
#include <unistd.h>
#include <pthread.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <asm-generic/mman.h>
#include <sys/mman.h>

#include "ESP.h"
#include "ImGui/imgui.h"
#include "Roboto-Regular.h"
#include "ImGui/backends/imgui_impl_opengl3.h"
#include "ImGui/backends/imgui_impl_android.h"
#include "ImGui/backends/android_native_app_glue.h"

#include "../Dobby/dobby.h"
#include "../Utils/Utils.h"
#include "Offsets.h"

#define LOG_TAG "Debug"
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)


namespace States{

    static bool     speedHack{false};
    static bool     spinBot{false};
    static bool     silentAim{false};
    static bool     esp{false};
    static float    fov{180.0f};
    static float    aimSpeed{10.0f};
}


/** --------------------------------ImGui Begin-------------------------------------------------- */
/** --------------------------------------------------------------------------------------------- */

// To check if ImGui has initialized or not.
static bool isInitialized{false};

/**
 * @brief Initializes and sets up the ImGui menu with the specified width and height.
 */
void setupMenu(int width, int height) {

    if (isInitialized)
        return;

    ImGuiContext* ImGuiContext = ImGui::CreateContext();
    if (!ImGuiContext) {
        LOGD("Failed to create ImGuiContext");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename = nullptr;

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init("#version 300 es");

    int systemScale = (1.0 / width) * width;

    ImFontConfig imFontConfig;
    imFontConfig.SizePixels = systemScale * 22.0f;
    io.Fonts->AddFontFromMemoryTTF(Roboto_Regular, systemScale * 30.0, 40.0f);

    ImGui::GetStyle().ScaleAllSizes(1.0f);

    // Тёмная тема
    ImGui::StyleColorsDark();

    // Красные акценты
    ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::GetStyle().Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::GetStyle().Colors[ImGuiCol_Button] = ImVec4(0.3f, 0.0f, 0.0f, 1.0f);
    ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered] = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);
    ImGui::GetStyle().Colors[ImGuiCol_ButtonActive] = ImVec4(0.7f, 0.0f, 0.0f, 1.0f);
    ImGui::GetStyle().Colors[ImGuiCol_Header] = ImVec4(0.2f, 0.0f, 0.0f, 1.0f);
    ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered] = ImVec4(0.4f, 0.0f, 0.0f, 1.0f);
    ImGui::GetStyle().Colors[ImGuiCol_HeaderActive] = ImVec4(0.6f, 0.0f, 0.0f, 1.0f);

    isInitialized = true;
    LOGD("Setup done.");
}


/**
 * @brief Design and Draws the ImGui menu for the mod.
 */
void DesignAndDrawMenu() {

    ImGui::SetNextWindowSize(ImVec2(450, 350));
    ImGui::Begin("Meow Client");

    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Meow Client v1.0");
    ImGui::Separator();

    if (ImGui::BeginTabBar("Tabs")) {

        if (ImGui::BeginTabItem("Aimbot")) {
            ImGui::Checkbox("Silent Aim", &States::silentAim);
            ImGui::SliderFloat("FOV", &States::fov, 1.0f, 360.0f);
            ImGui::SliderFloat("Speed", &States::aimSpeed, 1.0f, 30.0f);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("ESP")) {
            ImGui::Checkbox("ESP", &States::esp);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Misc")) {
            ImGui::Checkbox("Speed Hack", &States::speedHack);
            ImGui::Checkbox("SpinBot 360", &States::spinBot);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Save")) {
            ImGui::Text("Config save coming soon");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}


/**
 * @brief Draws the ImGui menu internally with the specified width and height.
 */
void drawImGuiMenuInternally(int width, int height) {

    if (!isInitialized)
        return;

    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(width, height);
    ImGui::NewFrame();

    DesignAndDrawMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

/**
 * @brief Function pointer type for the original eglSwapBuffers function.
 */
EGLBoolean (*eglSwapBuffersOrigin)(EGLDisplay eglDisplay, EGLSurface eglSurface);

/**
 * @brief Replacement function for eglSwapBuffers that integrates ImGui rendering.
 */
EGLBoolean eglSwapBuffersReplace(EGLDisplay eglDisplay, EGLSurface eglSurface) {

    EGLint width, height;
    eglQuerySurface(eglDisplay, eglSurface, EGL_WIDTH, &width);
    eglQuerySurface(eglDisplay, eglSurface, EGL_HEIGHT, &height);

    setupMenu(width, height);
    drawImGuiMenuInternally(width, height);

    return eglSwapBuffersOrigin(eglDisplay, eglSurface);
}

/**
 * android::InputConsumer::initializeMotionEvent(android::MotionEvent*, android::InputMessage const*)
 */
void (*inputOrigin)(void* thiz, void* event, void* msg);

/**
 * @brief Custom replacement function for Android input event handling.
 */
void inputReplace(void *thiz, void *event, void *msg) {
    inputOrigin(thiz, event, msg);
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
}

/**
 * @brief Initializes ImGui hooks by hooking functions and symbols.
 */
void initializeImGuiHooks() {

    do {
        sleep(1);
    } while (!isLibraryLoaded("libEGL.so"));

    auto eglSwapBuffers = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    DobbyHook((void *)eglSwapBuffers, (void *)eglSwapBuffersReplace, (void **)&eglSwapBuffersOrigin);

    auto input = DobbySymbolResolver("libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");
    DobbyHook((void *)input, (void *)inputReplace, (void **)&inputOrigin);

    LOGD("ImGui Hooks initialized");
}


/** --------------------------------------------------------------------------------------------- */
/** --------------------------------ImGui End-------------------------------------------------- */


void (*UpdateOrigin)(void* instance);
void UpdateReplace(void* instance){

    if( instance != NULL ){
      // Do Something
    }
    UpdateOrigin(instance);
}

void initializeUnityHooks(){

    do {
        sleep(1);
    } while (!isLibraryLoaded("libil2cpp.so"));

    unsigned long libBase = getLibraryBase("libil2cpp.so");
    LOGD("Base of libil2cpp.so : %p ", (void*) libBase );

    DobbyHook( (void*) getRealOffset(libBase,CharacterPlayer::Update          ), (void*) UpdateReplace            , (void**) &UpdateOrigin            );
}

void* hackThread(void* ){

    // initializeUnityHooks(); // Пока отключено
    initializeImGuiHooks();

    return  NULL;
}

__attribute__((constructor))
int main(){

    LOGD("Server: I am loaded in the address space");

    pthread_t ptid;
    pthread_create(&ptid, NULL, hackThread, NULL);
}
