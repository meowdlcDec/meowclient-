#include <jni.h>
#include <string>
#include <vector>
#include <utility>
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

    static bool     showMenu{true};
    static float    menuAlpha{1.0f};

    static bool     speedHack{false};
    static bool     spinBot{false};
    static bool     silentAim{true};
    static bool     esp{true};
    static float    fovValue{185.0f};
    static float    aimSpeed{12.0f};
    static int      aimMode{0};
    static int      targetBone{0};

    static std::vector<std::pair<std::string, float>> notifications;
}


/** --------------------------------ImGui Begin-------------------------------------------------- */
/** --------------------------------------------------------------------------------------------- */

static bool isInitialized{false};


void AddNotification(const char* msg) {
    States::notifications.push_back({msg, ImGui::GetTime()});
}


void ApplyAndroidRedTheme() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 12.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.PopupRounding = 10.0f;
    style.ScrollbarRounding = 10.0f;

    style.TouchExtraPadding = ImVec2(4.0f, 4.0f);
    style.FramePadding = ImVec2(10.0f, 8.0f);
    style.ItemSpacing = ImVec2(12.0f, 10.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.07f, 0.07f, 0.08f, 0.98f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.10f, 0.10f, 0.11f, 0.50f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.13f, 0.13f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);

    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.10f, 0.10f, 0.11f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.85f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.85f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.95f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.15f, 0.15f, 0.16f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.85f, 0.15f, 0.15f, 0.80f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.95f, 0.25f, 0.25f, 1.00f);

    colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.85f, 0.15f, 0.15f, 0.60f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.85f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.85f, 0.15f, 0.15f, 0.70f);
}


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

    io.Fonts->AddFontFromMemoryTTF(Roboto_Regular, sizeof(Roboto_Regular), 28.0f);

    ApplyAndroidRedTheme();

    isInitialized = true;
    LOGD("Setup done.");
}


void DesignAndDrawMenu() {

    if (States::showMenu && States::menuAlpha < 1.0f) States::menuAlpha += 0.15f;
    if (!States::showMenu && States::menuAlpha > 0.0f) States::menuAlpha -= 0.15f;

    if (States::menuAlpha <= 0.05f) {
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowBgAlpha(0.4f);
        ImGui::Begin("##ToggleBtn", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize);

        if (ImGui::IsKeyPressed(ImGuiKey_RightShift)) {
            States::showMenu = true;
        }

        if (ImGui::Button("MENU", ImVec2(120, 60))) {
            States::showMenu = true;
        }
        ImGui::End();
    } else {
        ImGui::SetNextWindowPos(ImVec2(10, 10));
        ImGui::SetNextWindowBgAlpha(0.4f);
        ImGui::Begin("##CloseBtn", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize);
        if (ImGui::Button("X", ImVec2(60, 60))) {
            States::showMenu = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightShift)) {
            States::showMenu = false;
        }
        ImGui::End();

        ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(States::menuAlpha * 0.98f);

        ImGui::Begin("Meow Client", nullptr);
        {
            if (ImGui::BeginTabBar("NavigationTabs")) {

                // ============ AIMBOT ============
                if (ImGui::BeginTabItem("Aimbot")) {
                    ImGui::Spacing();

                    ImGui::Text("Silent Aim");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    ImGui::Checkbox("##Silent", &States::silentAim);

                    ImGui::Separator();
                    ImGui::Spacing();

                    const char* modes[] = { "Track", "Predict", "Direct" };
                    ImGui::Text("Aim Mode");
                    ImGui::Combo("##AimMode", &States::aimMode, modes, IM_ARRAYSIZE(modes));

                    ImGui::Spacing();

                    const char* bones[] = { "Head", "Neck", "Chest", "Body" };
                    ImGui::Text("Target Bone");
                    ImGui::Combo("##TargetBone", &States::targetBone, bones, IM_ARRAYSIZE(bones));

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("FOV");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
                    ImGui::Text("%.1f", States::fovValue);
                    ImGui::SliderFloat("##FOV", &States::fovValue, 1.0f, 360.0f);

                    ImGui::Spacing();

                    ImGui::Text("Speed");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 80);
                    ImGui::Text("%.1f", States::aimSpeed);
                    ImGui::SliderFloat("##Speed", &States::aimSpeed, 1.0f, 30.0f);

                    ImGui::EndTabItem();
                }

                // ============ ESP ============
                if (ImGui::BeginTabItem("ESP")) {
                    ImGui::Spacing();

                    ImGui::Text("ESP");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    ImGui::Checkbox("##ESP", &States::esp);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("Box");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    static bool box = true;
                    ImGui::Checkbox("##Box", &box);

                    ImGui::Spacing();

                    ImGui::Text("Line");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    static bool line = true;
                    ImGui::Checkbox("##Line", &line);

                    ImGui::Spacing();

                    ImGui::Text("Health");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    static bool health = true;
                    ImGui::Checkbox("##Health", &health);

                    ImGui::EndTabItem();
                }

                // ============ MISC ============
                if (ImGui::BeginTabItem("Misc")) {
                    ImGui::Spacing();

                    ImGui::Text("Speed Hack");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    ImGui::Checkbox("##SpeedHack", &States::speedHack);

                    ImGui::Spacing();

                    ImGui::Text("SpinBot 360");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    ImGui::Checkbox("##SpinBot", &States::spinBot);

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    ImGui::Text("BunnyHop");
                    ImGui::SameLine(ImGui::GetWindowWidth() - 60);
                    static bool bhop = false;
                    ImGui::Checkbox("##BHOP", &bhop);

                    ImGui::EndTabItem();
                }

                // ============ SAVE ============
                if (ImGui::BeginTabItem("Save")) {
                    ImGui::Spacing(); ImGui::Spacing();

                    if (ImGui::Button("SAVE CONFIG", ImVec2(ImGui::GetWindowWidth() - 20, 60))) {
                        AddNotification("Config saved");
                    }

                    ImGui::Spacing(); ImGui::Spacing();

                    if (ImGui::Button("LOAD CONFIG", ImVec2(ImGui::GetWindowWidth() - 20, 60))) {
                        AddNotification("Config loaded");
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    // HUD уведомления
    if (!States::notifications.empty()) {
        ImGui::SetNextWindowPos(ImVec2(10, 80));
        ImGui::SetNextWindowBgAlpha(0.3f);
        ImGui::Begin("##Notifications", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize);
        float now = ImGui::GetTime();
        for (auto it = States::notifications.begin(); it != States::notifications.end(); ) {
            float age = now - it->second;
            if (age > 4.0f) {
                it = States::notifications.erase(it);
                continue;
            }
            float alpha = 1.0f - (age / 4.0f);
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, alpha), "%s", it->first.c_str());
            ++it;
        }
        ImGui::End();
    }
}


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


EGLBoolean (*eglSwapBuffersOrigin)(EGLDisplay eglDisplay, EGLSurface eglSurface);

EGLBoolean eglSwapBuffersReplace(EGLDisplay eglDisplay, EGLSurface eglSurface) {

    EGLint width, height;
    eglQuerySurface(eglDisplay, eglSurface, EGL_WIDTH, &width);
    eglQuerySurface(eglDisplay, eglSurface, EGL_HEIGHT, &height);

    setupMenu(width, height);
    drawImGuiMenuInternally(width, height);

    return eglSwapBuffersOrigin(eglDisplay, eglSurface);
}


void (*inputOrigin)(void* thiz, void* event, void* msg);

void inputReplace(void *thiz, void *event, void *msg) {
    // Сначала Unity
    inputOrigin(thiz, event, msg);

    // Потом ImGui с задержкой
    usleep(50000); // 50 мс

    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
}

void initializeImGuiHooks() {

    do {
        sleep(1);
    } while (!isLibraryLoaded("libEGL.so"));

    auto eglSwapBuffers = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    DobbyHook((void *)eglSwapBuffers, (void *)eglSwapBuffersReplace, (void **)&eglSwapBuffersOrigin);

    auto input = DobbySymbolResolver("libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");
    if (input) {
        DobbyHook((void *)input, (void *)inputReplace, (void **)&inputOrigin);
        LOGD("Input hooked");
    }

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
