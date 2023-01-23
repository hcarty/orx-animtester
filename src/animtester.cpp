/**
 * @file animtester.cpp
 * @date 21-Jan-2023
 */

#include <algorithm>
#include <string>
#include <vector>
#include "orx.h"

#define orxIMGUI_IMPL
#include "orxImGui.h"
#undef orxIMGUI_IMPL

#ifdef __orxMSVC__

/* Requesting high performance dedicated GPU on hybrid laptops */
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#endif // __orxMSVC__

namespace object
{
    std::vector<orxANIM *> GetAnims(orxOBJECT *object, bool sorted = true)
    {
        auto animPointer = orxOBJECT_GET_STRUCTURE(object, ANIMPOINTER);
        orxASSERT(animPointer);
        auto animSet = orxAnimPointer_GetAnimSet(animPointer);
        orxASSERT(animSet);
        auto animationCount = orxAnimSet_GetAnimCount(animSet);
        auto animations = std::vector<orxANIM *>(animationCount);
        for (orxU32 i = 0; i < animationCount; i++)
        {
            animations[i] = orxAnimSet_GetAnim(animSet, i);
        }
        if (sorted)
            std::sort(animations.begin(), animations.end(), [](auto a, auto b)
                      { return std::string{orxAnim_GetName(a)} < std::string{orxAnim_GetName(b)}; });
        return animations;
    }
}

namespace gui
{
    void ScaleSlider(orxOBJECT *object)
    {
        // Object scale
        auto scale = orxVECTOR_0;
        orxObject_GetScale(object, &scale);
        ImGui::SliderFloat("Scale", &scale.fX, 0.0, 10.0);
        scale.fY = scale.fX;
        orxObject_SetScale(object, &scale);
    }

    void AnimationText(orxOBJECT *object)
    {
        // Current animation
        auto currentAnimation = orxObject_GetCurrentAnim(object);
        ImGui::LabelText("Current animation", "%s", currentAnimation);

        // Target animation
        auto targetAnimation = orxObject_GetTargetAnim(object);
        ImGui::LabelText("Target animation", "%s", targetAnimation);
    }

    void AnimationRateSlider(orxOBJECT *object)
    {
        // Animation rate
        auto animationRate = orxObject_GetAnimFrequency(object);
        ImGui::SliderFloat("Animation rate", &animationRate, 0.0, 1.0);
        orxObject_SetAnimFrequency(object, animationRate);
    }

    void AnimationTargetTree(orxOBJECT *object)
    {
        auto currentAnimation = orxObject_GetCurrentAnim(object);

        // Target animation
        if (ImGui::TreeNode("Target animation"))
        {
            // Get the animation set for the object
            auto animations = object::GetAnims(object);
            for (auto anim : animations)
            {
                // Add a selector for each available target animation
                auto animName = orxAnim_GetName(anim);
                auto active = false;
                ImGui::Selectable(animName, &active);
                if (active)
                    orxObject_SetTargetAnim(object, animName);
            }
            ImGui::TreePop();
        }
    }

    void AnimationWindow(orxOBJECT *object)
    {
        orxASSERT(object);
        ImGui::Begin("Animation Control");

        ScaleSlider(object);
        AnimationText(object);
        AnimationRateSlider(object);
        AnimationTargetTree(object);

        ImGui::End();
    }
}

/** Update function, it has been registered to be called every tick of the core clock
 */
void orxFASTCALL Update(const orxCLOCK_INFO *_pstClockInfo, void *_pContext)
{
    auto targetObject = (orxOBJECT *)_pContext;

    // Show animation control window
    gui::AnimationWindow(targetObject);

    // Should quit?
    if (orxInput_IsActive("Quit"))
    {
        // Send close event
        orxEvent_SendShort(orxEVENT_TYPE_SYSTEM, orxSYSTEM_EVENT_CLOSE);
    }
}

/** Init function, it is called when all orx's modules have been initialized
 */
orxSTATUS orxFASTCALL Init()
{
    // Initialize Dear ImGui
    orxImGui_Init();

    // Create the viewport
    orxViewport_CreateFromConfig("MainViewport");

    // Create the scene
    auto targetObject = orxObject_CreateFromConfig("Knight");
    orxASSERT(targetObject);

    // Register the Update function to the core clock
    orxClock_Register(orxClock_Get(orxCLOCK_KZ_CORE), Update, (void *)targetObject, orxMODULE_ID_MAIN, orxCLOCK_PRIORITY_NORMAL);

    // Done!
    return orxSTATUS_SUCCESS;
}

/** Run function, it should not contain any game logic
 */
orxSTATUS orxFASTCALL Run()
{
    // Return orxSTATUS_FAILURE to instruct orx to quit
    return orxSTATUS_SUCCESS;
}

/** Exit function, it is called before exiting from orx
 */
void orxFASTCALL Exit()
{
    // Exit from Dear ImGui
    orxImGui_Exit();

    // Let orx clean all our mess automatically. :)
}

/** Bootstrap function, it is called before config is initialized, allowing for early resource storage definitions
 */
orxSTATUS orxFASTCALL Bootstrap()
{
    // Add config storage to find the initial config file
    orxResource_AddStorage(orxCONFIG_KZ_RESOURCE_GROUP, "../data/config", orxFALSE);

    // Return orxSTATUS_FAILURE to prevent orx from loading the default config file
    return orxSTATUS_SUCCESS;
}

/** Main function
 */
int main(int argc, char **argv)
{
    // Set the bootstrap function to provide at least one resource storage before loading any config files
    orxConfig_SetBootstrap(Bootstrap);

    // Execute our game
    orx_Execute(argc, argv, Init, Run, Exit);

    // Done!
    return EXIT_SUCCESS;
}
