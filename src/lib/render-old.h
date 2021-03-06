#ifndef VIEWER_R_COMMANDS_H
#define VIEWER_R_COMMANDS_H

#include <obsidian/r_geo.h>
#include <obsidian/s_scene.h>
#include <obsidian/v_swapchain.h>
#include "engine.h"
#include "common.h"

void r_InitRenderer(const Obdn_S_Scene* scene, const PaintScene* pScene,
               const bool copyToHost, const Obdn_Swapchain* swapchain);
void         r_Render(const Obdn_Swapchain* swapchain, uint32_t fi, VkSemaphore waitSemaphore, VkSemaphore imgAcquiredSemaphore);
void         r_CleanUp(uint32_t swapviewCount);

void r_GetColorDepthExternal(uint32_t* width, uint32_t* height, uint32_t* elementSize, 
        uint64_t* colorOffset, uint64_t* depthOffset);
void r_GetSwapBufferData(uint32_t* width, uint32_t* height, uint32_t* elementSize, 
        void** colorData, void** depthData);
bool r_GetExtMemoryFd(int* fd, uint64_t* size);
bool r_GetSemaphoreFds(int* obdnFrameDoneFD_0, int* obdnFrameDoneFD_1, int* extTextureReadFD);
void r_SetExtFastPath(bool isFast);
void r_OnRecreateSwapchain(uint32_t width, uint32_t height, uint32_t viewCount, const VkImageView views[viewCount]);

#endif /* end of include guard: R_COMMANDS_H */
