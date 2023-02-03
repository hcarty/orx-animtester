/**
 * @file animtester.cpp
 * @date 21-Jan-2023
 */

#include <algorithm>
#include <optional>
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

const orxSTRING objectName = "Knight";
orxOBJECT *targetObject = orxNULL;
orxBOOL configChanged = orxFALSE;

namespace object
{
    std::optional<orxANIMSET *> GetAnimSet(orxOBJECT *object)
    {
        auto animPointer = orxOBJECT_GET_STRUCTURE(object, ANIMPOINTER);
        if (!animPointer)
            return std::nullopt;

        auto animSet = orxAnimPointer_GetAnimSet(animPointer);
        if (!animSet)
            return std::nullopt;

        return animSet;
    }

    const orxSTRING GetAnimSetName(orxOBJECT *object)
    {
        auto animSet = GetAnimSet(object);
        if (animSet.has_value())
        {
            return orxAnimSet_GetName(animSet.value());
        }
        else
        {
            return orxSTRING_EMPTY;
        }
    }

    std::vector<orxANIM *> GetAnims(orxOBJECT *object, bool sorted = true)
    {
        std::vector<orxANIM *> animations{};

        auto maybeAnim = GetAnimSet(object);
        if (!maybeAnim.has_value())
        {
            return animations;
        }
        auto animSet = maybeAnim.value();

        auto animationCount = orxAnimSet_GetAnimCount(animSet);
        animations.resize(animationCount);
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

namespace config
{
    int GetAnimFrames(const orxSTRING animSetName, const orxSTRING animName)
    {
        // Get number of frames in the animation from the animation set
        orxConfig_PushSection(animSetName);
        auto frames = orxConfig_GetU32(animName);
        orxConfig_PopSection();
        return frames;
    }

    void SetAnimFrames(const orxSTRING animSetName, const orxSTRING animName, const orxU32 frames)
    {
        // Set number of frames in the animation in the animation set
        orxConfig_PushSection(animSetName);
        orxConfig_SetU32(animName, frames);
        orxConfig_PopSection();
    }
}

namespace gui
{
    void AnimSetWindow(const orxSTRING animSetName)
    {
        static const auto configKey = "FrameSize";

        if (orxConfig_PushSection(animSetName))
        {
            orxCHAR title[256];
            orxString_NPrint(title, sizeof(title), "Animation Set: %s", animSetName);

            ImGui::Begin(title);

            auto frameSize = orxVECTOR_0;
            orxConfig_GetVector(configKey, &frameSize);

            // Set frame size
            {
                int x = frameSize.fX;
                int y = frameSize.fY;
                auto setX = ImGui::InputInt("X Frame Size", &x, 1, 8);
                auto setY = ImGui::InputInt("Y Frame Size", &y, 1, 8);
                if (setX || setY)
                {
                    configChanged = orxTRUE;
                    frameSize.fX = x;
                    frameSize.fY = y;
                    orxConfig_SetVector(configKey, &frameSize);
                }
            }

            // Show source texture
            {
                static auto snap = false;
                ImGui::Checkbox("Snap tooltip to frame size", &snap);

                // Capture IO (mouse) information
                ImGuiIO &io = ImGui::GetIO();
                ImVec2 pos = ImGui::GetCursorScreenPos();

                auto texture = orxTexture_Get(orxConfig_GetString("Texture"));
                float textureWidth, textureHeight;
                orxTexture_GetSize(texture, &textureWidth, &textureHeight);
                auto textureID = (ImTextureID)orxTexture_GetBitmap(texture);
                ImGui::Image(textureID, {textureWidth, textureHeight});

                // Zoomed in tooltip - based on imgui_demo.cpp
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    float region_x = io.MousePos.x - pos.x - frameSize.fX * 0.5f;
                    float region_y = io.MousePos.y - pos.y - frameSize.fY * 0.5f;
                    if (snap)
                    {
                        region_x = floorf(region_x / frameSize.fX) * frameSize.fX;
                        region_y = floorf(region_y / frameSize.fY) * frameSize.fY;
                    }
                    float zoom = 4.0f;
                    if (region_x < 0.0f)
                    {
                        region_x = 0.0f;
                    }
                    else if (region_x > textureWidth - frameSize.fX)
                    {
                        region_x = textureWidth - frameSize.fX;
                    }
                    if (region_y < 0.0f)
                    {
                        region_y = 0.0f;
                    }
                    else if (region_y > textureHeight - frameSize.fY)
                    {
                        region_y = textureHeight - frameSize.fY;
                    }
                    ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
                    ImGui::Text("Max: (%.2f, %.2f)", region_x + frameSize.fX, region_y + frameSize.fY);
                    ImVec2 uv0 = ImVec2((region_x) / textureWidth, (region_y) / textureHeight);
                    ImVec2 uv1 = ImVec2((region_x + frameSize.fX) / textureWidth, (region_y + frameSize.fY) / textureHeight);
                    ImGui::Image(textureID, ImVec2(frameSize.fX * zoom, frameSize.fY * zoom), uv0, uv1);
                    ImGui::EndTooltip();
                }
            }

            orxConfig_PopSection();

            ImGui::End();
        }
    }

    void AnimWindow(const orxSTRING animSetName, const orxSTRING prefix, const orxSTRING name)
    {
        orxCHAR sectionName[256];
        orxString_NPrint(sectionName, sizeof(sectionName), "%s%s", prefix, name);
        if (orxConfig_PushSection(sectionName))
        {
            orxCHAR buffer[256];
            orxString_NPrint(buffer, sizeof(buffer), "Animation: %s", name);
            ImGui::Begin(buffer);

            // Number of frames
            auto frames = config::GetAnimFrames(animSetName, name);
            auto setFrames = ImGui::InputInt("Frames", &frames, 1, 2);
            if (setFrames)
            {
                configChanged = orxTRUE;
                config::SetAnimFrames(animSetName, name, frames);
            }

            // Frame duration
            auto duration = orxConfig_GetFloat("KeyDuration");
            auto setDuration = ImGui::InputFloat("Key Duration", &duration, 0.01, 0.05);
            if (setDuration)
            {
                configChanged = orxTRUE;
                orxConfig_SetFloat("KeyDuration", duration);
            }

            // Texture origin
            orxVECTOR origin = orxVECTOR_0;
            orxConfig_GetVector("TextureOrigin", &origin);
            int x = origin.fX;
            int y = origin.fY;
            auto setX = ImGui::InputInt("X Origin", &x, 1, 8);
            auto setY = ImGui::InputInt("Y Origin", &y, 1, 8);
            if (setX || setY)
            {
                configChanged = orxTRUE;
                origin.fX = x;
                origin.fY = y;
                orxConfig_SetVector("TextureOrigin", &origin);
            }

            ImGui::End();
            orxConfig_PopSection();
        }
    }

    void ScaleInput(orxOBJECT *object)
    {
        // Object scale
        auto scale = orxVECTOR_0;
        orxObject_GetScale(object, &scale);
        ImGui::InputFloat("Scale", &scale.fX, 1, 2);
        scale.fX = orxCLAMP(scale.fX, 0.0, 64.0);
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

    void AnimationRateInput(orxOBJECT *object)
    {
        // Animation rate
        auto animationRate = orxObject_GetAnimFrequency(object);
        ImGui::InputFloat("Animation rate", &animationRate, 0.1, 1.0);
        orxObject_SetAnimFrequency(object, animationRate);
    }

    void TargetAnimationCombo(orxOBJECT *object)
    {
        auto currentAnimation = orxObject_GetCurrentAnim(object);
        auto targetAnimation = orxObject_GetTargetAnim(object);

        static std::string selectedAnimation{};
        static std::string animSetName{};
        static std::string prefix{};

        // Target animation
        if (ImGui::BeginCombo("Target animation", targetAnimation))
        {
            // Get animation section prefix, if there is one
            animSetName = object::GetAnimSetName(object);
            orxASSERT(orxString_GetLength(animSetName) > 0);

            orxConfig_PushSection(animSetName.data());
            prefix = orxConfig_GetString("Prefix");
            orxConfig_PopSection();

            // Get the animation set for the object
            auto animations = object::GetAnims(object);

            for (auto anim : animations)
            {
                // Add a selector for each available target animation
                auto animName = orxAnim_GetName(anim);
                auto active = selectedAnimation == animName;
                if (ImGui::Selectable(animName, active))
                {
                    selectedAnimation = animName;
                    orxObject_SetTargetAnim(object, animName);
                }
                if (active)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selectedAnimation.length() > 0)
        {
            AnimWindow(animSetName.data(), prefix.data(), selectedAnimation.data());
        }
    }

    void ObjectWindow(orxOBJECT *object)
    {
        orxASSERT(object);
        orxCHAR title[256];
        orxString_NPrint(title, sizeof(title), "Object %s", orxObject_GetName(object));

        ImGui::Begin(title);

        ScaleInput(object);
        AnimationText(object);
        AnimationRateInput(object);
        TargetAnimationCombo(object);

        ImGui::End();
    }
}

/** Update function, it has been registered to be called every tick of the core clock
 */
void orxFASTCALL Update(const orxCLOCK_INFO *_pstClockInfo, void *_pContext)
{
    if (configChanged)
    {
        configChanged = orxFALSE;
        // Get some animation information for the current object so we can
        // propagate it to the replacement object.
        auto currentAnimation = orxObject_GetCurrentAnim(targetObject);
        auto targetAnimation = orxObject_GetTargetAnim(targetObject);
        auto animationTime = orxObject_GetAnimTime(targetObject);

        // Delete to current object so that the associated animset is freed
        // now, rather than potentially delaying the deletion until the next
        // frame. Then we can create a new object using the update config
        // values.
        orxObject_Delete(targetObject);

        // Create a new object and align its animation and animation time to
        // the values for the previous target object.
        targetObject = orxObject_CreateFromConfig(objectName);
        orxObject_SetCurrentAnim(targetObject, currentAnimation);
        orxObject_SetTargetAnim(targetObject, targetAnimation);
        orxObject_SetAnimTime(targetObject, animationTime);
        orxASSERT(targetObject);
    }

    // Show top level windows
    gui::ObjectWindow(targetObject);
    gui::AnimSetWindow(object::GetAnimSetName(targetObject));

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
    targetObject = orxObject_CreateFromConfig(objectName);
    orxASSERT(targetObject);

    // Register the Update function to the core clock
    orxClock_Register(orxClock_Get(orxCLOCK_KZ_CORE), Update, orxNULL, orxMODULE_ID_MAIN, orxCLOCK_PRIORITY_NORMAL);

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
