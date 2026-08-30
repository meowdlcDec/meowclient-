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

    static bool     setInvincibility{false};
    static float    invincibilityDuration{3.0};
    static bool     avoidCollision{false};
}


/** --------------------------------ImGui Begin-------------------------------------------------- */
/** --------------------------------------------------------------------------------------------- */

// To check if ImGui has initialized or not.
static bool isInitialized{false};

/**
 * @brief Initializes and sets up the ImGui menu with the specified width and height.
 *
 * @param width  The width of the display.
 * @param height The height of the display.
 */
void setupMenu(int width, int height) {

    if (isInitialized)
        return;

    // Create a new ImGui context
    ImGuiContext* ImGuiContext = ImGui::CreateContext();
    if (!ImGuiContext) {
        LOGD("Failed to create ImGuiContext");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Set the display size
    io.DisplaySize = ImVec2((float)width, (float)height);
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.IniFilename = nullptr;

    // Setup Platform/Renderer backends
    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init("#version 300 es");

    // Calculate the system scale
    int systemScale = (1.0 / width) * width;

    // Configure font settings
    ImFontConfig imFontConfig;
    imFontConfig.SizePixels = systemScale * 22.0f;
    io.Fonts->AddFontFromMemoryTTF(Roboto_Regular, systemScale * 30.0, 40.0f);

    // Scale ImGui style
    ImGui::GetStyle().ScaleAllSizes(1.0f);

    isInitialized = true;
    LOGD("Setup done.");
}


/**
 * @brief Design and Draws the ImGui menu for the mod.
 */
void DesignAndDrawMenu() {

    {
        ImGui::Begin("Meow Client");

        ImGui::Text("Meow Client v1.0");
        ImGui::Separator();

        ImGui::Checkbox("Speed Hack", &States::setInvincibility);
        ImGui::Checkbox("SpinBot 360", &States::avoidCollision);

        ImGui::End();
    }
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

    // Call the function that implements the Menu
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

    // Setup ImGui (once)
    setupMenu(width, height);

    // Draw Menu (every frame)
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
    // Call the original input handling function
    inputOrigin(thiz, event, msg);

    // Forward the input event to ImGui for processing
    ImGui_ImplAndroid_HandleInputEvent((AInputEvent *)thiz);
}

/**
 * @brief Initializes ImGui hooks by hooking functions and symbols.
 */
void initializeImGuiHooks() {

    // Wait until the "libEGL.so" library is loaded
    do {
        sleep(1);
    } while (!isLibraryLoaded("libEGL.so"));

    // Hook eglSwapBuffers
    auto eglSwapBuffers = DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    DobbyHook((void *)eglSwapBuffers, (void *)eglSwapBuffersReplace, (void **)&eglSwapBuffersOrigin);

    // Hook Input
    auto input = DobbySymbolResolver("libinput.so", "_ZN7android13InputConsumer21initializeMotionEventEPNS_11MotionEventEPKNS_12InputMessageE");
    DobbyHook((void *)input, (void *)inputReplace, (void **)&inputOrigin);

    LOGD("ImGui Hooks initialized");
}


/** --------------------------------------------------------------------------------------------- */
/** --------------------------------ImGui End-------------------------------------------------- */


void (*StartInvcibilityOrigin)(void* instance, float duration);
void StartInvcibilityReplace(void* instance, float duration){

    if ( instance != NULL and States::setInvincibility )
        StartInvcibilityOrigin(instance, States::invincibilityDuration);
    else
        StartInvcibilityOrigin(instance, duration);
}

void (*HandleCollisionWithObstaclesOrigin)(void* instance);
void HandleCollisionWithObstaclesReplace(void* instance){

    if ( instance != NULL and States::avoidCollision ){
        return;
    }else{
        HandleCollisionWithObstaclesOrigin(instance);
    }
}

void (*UpdateOrigin)(void* instance);
void UpdateReplace(void* instance){

    if( instance != NULL ){
      // Do Something
    }
    UpdateOrigin(instance);
}

void initializeUnityHooks(){

    // Wait until the "libil2cpp.so" library is loaded
    do {
        sleep(1);
    } while (!isLibraryLoaded("libil2cpp.so"));

    unsigned long libBase = getLibraryBase("libil2cpp.so");
    LOGD("Base of libil2cpp.so : %p ", (void*) libBase );


    DobbyHook( (void*) getRealOffset(libBase,CharacterPlayer::Update          ), (void*) UpdateReplace            , (void**) &UpdateOrigin            );
    DobbyHook( (void*) getRealOffset(libBase,CharacterPlayer::StartInvcibility), (void*) StartInvcibilityReplace  , (void**) &StartInvcibilityOrigin  );

    DobbyHook( (void*) getRealOffset(libBase,GameController::HandleCollisionWithObstacles), (void*) HandleCollisionWithObstaclesReplace , (void**) &HandleCollisionWithObstaclesOrigin  );

}

void* hackThread(void* ){

    initializeUnityHooks();
    initializeImGuiHooks();

    return  NULL;
}

__attribute__((constructor))
int main(){

    LOGD("Server: I am loaded in the address space");

    pthread_t ptid;
    pthread_create(&ptid, NULL, hackThread, NULL);
}
