#include "painter.h"
#include <obsidian/s_scene.h>
#include "paint.h"
#include "common.h"
#include "render.h"
#include "obsidian/f_file.h"
#include "obsidian/r_geo.h"
#include "undo.h"

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdbool.h>
#include <obsidian/v_video.h>
#include <obsidian/v_swapchain.h>
#include <obsidian/r_raytrace.h>
#include <obsidian/u_ui.h>
#include <obsidian/v_private.h>
#include <obsidian/dtags.h>
#include <hell/input.h>
#include <hell/window.h>
#include <hell/cmd.h>
#include <hell/common.h>
#include <hell/debug.h>
#include <hell/locations.h>
#include <hell/len.h>
#include "g_api.h"

#define NS_TARGET 16666666 // 1 / 60 seconds

#define DEF_WINDOW_WIDTH  666
#define DEF_WINDOW_HEIGHT 666

static void getMemorySizes4k(Obdn_V_MemorySizes* ms) __attribute__ ((unused));
static void getMemorySizes8k(Obdn_V_MemorySizes* ms) __attribute__ ((unused));
static void getMemorySizes16k(Obdn_V_MemorySizes* ms) __attribute__ ((unused));
static void painter_FullClean(void);

static void getMemorySizes4k(Obdn_V_MemorySizes* ms)
{
    *ms = (Obdn_V_MemorySizes){
    .hostGraphicsBufferMemorySize          = OBDN_1_GiB,
    .deviceGraphicsBufferMemorySize        = OBDN_256_MiB,
    .deviceGraphicsImageMemorySize         = OBDN_1_GiB,
    .hostTransferBufferMemorySize          = OBDN_1_GiB * 2,
    .deviceExternalGraphicsImageMemorySize = OBDN_100_MiB };
}

static void getMemorySizes8k(Obdn_V_MemorySizes* ms)
{
    *ms = (Obdn_V_MemorySizes){
    .hostGraphicsBufferMemorySize          = OBDN_1_GiB * 2,
    .deviceGraphicsBufferMemorySize        = OBDN_256_MiB,
    .deviceGraphicsImageMemorySize         = OBDN_1_GiB * 2,
    .hostTransferBufferMemorySize          = OBDN_1_GiB * 4,
    .deviceExternalGraphicsImageMemorySize = OBDN_100_MiB };
}

static void getMemorySizes16k(Obdn_V_MemorySizes* ms)
{
    *ms = (Obdn_V_MemorySizes){
    .hostGraphicsBufferMemorySize          = OBDN_1_GiB * 6,
    .deviceGraphicsBufferMemorySize        = OBDN_256_MiB * 2,
    .deviceGraphicsImageMemorySize         = OBDN_1_GiB * 6,
    .hostTransferBufferMemorySize          = OBDN_1_GiB * 8,
    .deviceExternalGraphicsImageMemorySize = OBDN_100_MiB };
}

static Obdn_S_Scene renderScene;
static PaintScene   paintScene;
static G_Export     ge;
static Parms        parms;

static void* gameModule;

static const char* debugFilterTags[] = {
    //OBDN_DEBUG_TAG_MEM, 
    //OBDN_DEBUG_TAG_PIPE, 
    //OBDN_DEBUG_TAG_SHADE,
    //OBDN_DEBUG_TAG_GRAPHIC_PIPE,
    //OBDN_DEBUG_TAG_UI,
};

void painter_Init(uint32_t texSize, bool houdiniMode, const char* gModuleName)
{
    hell_AddFilterTags(LEN(debugFilterTags), debugFilterTags);
    hell_c_SetVar("debug_silent", "1", HELL_C_VAR_ARCHIVE_BIT);
    assert(texSize == IMG_4K || texSize == IMG_8K || texSize == IMG_16K);
    Obdn_V_Config config = {};
    config.rayTraceEnabled = true;
#ifndef NDEBUG
    config.validationEnabled = true;
#else
    config.validationEnabled = false;
#endif
    parms.copySwapToHost = houdiniMode;
    const VkImageLayout finalUILayout = parms.copySwapToHost ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    const Hell_Window* window = NULL;
#if defined(UNIX)
    const bool initConsole = true;
#elif defined(WINDOWS)
    const bool initConsole = false;
#endif
    if (!parms.copySwapToHost)
        hell_Init(initConsole, painter_Frame, painter_FullClean, &window);
    else
        hell_Init(initConsole, painter_Frame, painter_FullClean, NULL);
    hell_c_SetVar("maxFps", "1000000", 0); // basically don't throttle us
    const char* exnames[] = {
        #if 1
        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME 
        #endif
        };
    switch (texSize)
    {
        case IMG_4K:  getMemorySizes4k(&config.memorySizes);  break;
        case IMG_8K:  getMemorySizes8k(&config.memorySizes);  break;
        case IMG_16K: getMemorySizes16k(&config.memorySizes); break;
    }
    obdn_v_Init(&config, LEN(exnames), exnames);
    obdn_v_InitSwapchain(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, window);
    obdn_u_Init(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, finalUILayout);
    const Hell_C_Var* varW = hell_c_GetVar("d_width", "666", HELL_C_VAR_ARCHIVE_BIT);
    const Hell_C_Var* varH = hell_c_GetVar("d_height", "666", HELL_C_VAR_ARCHIVE_BIT);
    obdn_s_Init(&renderScene, varW->value, varH->value, 0.01, 1000);

#define DL_PATH_LEN 256

    hell_DPrint("ROOT %s\n", ROOT);

    char gmodbuf[DL_PATH_LEN];
    assert(strnlen(gModuleName, DL_PATH_LEN) < DL_PATH_LEN - 3);
    strcpy(gmodbuf, ROOT);
    strcat(gmodbuf, "/");
    strcat(gmodbuf, gModuleName);
    #ifdef UNIX
    strcat(gmodbuf, ".so");
    #elif defined(WINDOWS)
    strcat(gmodbuf, ".dll");
    #else
    #error
    #endif

    hell_Print("Painter: Loading game module: %s\n", gmodbuf);
    gameModule = hell_LoadLibrary(gmodbuf);
    assert(gameModule);
    hell_DPrint("Game module imported successfully.\n");
    void* g_entry = hell_LoadSymbol(gameModule, "handshake");
    assert(g_entry);
    hell_DPrint("Game handshake function found.\n");
    G_Handshake handshake = g_entry;

    G_Import gi = {
        .createLayer        = l_CreateLayer,
        .decrementLayer     = l_DecrementLayer,
        .incrementLayer     = l_IncrementLayer,
        .copyTextureToLayer = l_CopyTextureToLayer,
        .parms              = &parms
    };
    ge = handshake(gi);

    if (!parms.copySwapToHost)
        painter_LocalInit(texSize);

    renderScene.dirt = ~0;
    paintScene.dirt  = ~0;
}

void painter_LocalInit(uint32_t texSize)
{
    hell_Print("PAINTER: Local init\n");

    ge.init(&renderScene, &paintScene);
    hell_Print("PAINTER: Game Initialized.\n");
    p_Init(&renderScene, &paintScene, texSize);
    hell_Print("PAINTER: Paint Engine Initialized.\n");
    r_InitRenderer(&renderScene, &paintScene, parms.copySwapToHost);
    hell_Print("PAINTER: Renderer Initialized.\n");
}

void painter_Frame(void)
{
//    hell_i_PumpEvents();
//    hell_i_DrainEvents();
//    hell_c_Execute();
//
    ge.update();
    u_Update(&paintScene);
    VkSemaphore s = VK_NULL_HANDLE;
    s = p_Paint(s);
    uint32_t i = obdn_v_RequestFrame(&renderScene.dirt, renderScene.window);
    r_Render(i, s);

    paintScene.dirt  = 0;
    renderScene.dirt = 0;
}

void painter_StartLoop(void)
{
    parms.shouldRun = true;

    while( parms.shouldRun ) 
    {
        painter_Frame();
    }

    painter_LocalCleanUp();
    painter_ShutDown();
}

void painter_StopLoop(void)
{
    parms.shouldRun = false;
}

void painter_ShutDown(void)
{
    p_CleanUp();
    hell_Print("PAINTER: painter engine shut down.\n");
    obdn_v_CleanUpSwapchain();
    obdn_u_CleanUp();
    obdn_v_CleanUp();
    hell_Print("PAINTER: shutdown.\n");
}

void painter_LocalCleanUp(void)
{
    vkDeviceWaitIdle(device);
    ge.cleanUp();
    hell_Print("PAINTER: game shutdown.\n");
    r_CleanUp();
    hell_Print("PAINTER: renderer shutdown.\n");
}

void painter_LoadFprim(Obdn_F_Primitive* fprim)
{
    Obdn_R_Primitive prim = obdn_f_CreateRPrimFromFPrim(fprim);
    obdn_f_FreePrimitive(fprim);
    Obdn_S_PrimId primId = obdn_s_AddRPrim(&renderScene, prim, NULL);
    hell_Print("PAINTER: Loaded prim. Id %d\n", primId);
}

void* painter_GetGame(void)
{
    assert(gameModule);
    return gameModule;
}

static void painter_FullClean(void)
{
    painter_LocalCleanUp();
    painter_ShutDown();
}
