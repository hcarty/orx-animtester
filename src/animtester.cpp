/**
 * @file animtester.cpp
 * @date 21-Jan-2023
 */

#include <algorithm>
#include <optional>
#include <set>
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

const orxSTRING objectName = "Character";
orxOBJECT *targetObject = orxNULL;
orxBOOL configChanged = orxFALSE;
std::optional<std::string> save = std::nullopt;

namespace animset
{
    std::vector<orxANIM *> GetAnims(const orxANIMSET *animSet, bool sorted = true)
    {
        auto animationCount = orxAnimSet_GetAnimCount(animSet);
        std::vector<orxANIM *> animations{};
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

namespace object
{
    orxANIMSET *GetAnimSet(orxOBJECT *object)
    {
        auto animPointer = orxOBJECT_GET_STRUCTURE(object, ANIMPOINTER);
        orxASSERT(animPointer);
        auto animSet = orxAnimPointer_GetAnimSet(animPointer);
        orxASSERT(animSet);

        return animSet;
    }

    const orxSTRING GetAnimSetName(orxOBJECT *object)
    {
        auto animSet = GetAnimSet(object);
        return orxAnimSet_GetName(animSet);
    }

    std::vector<orxANIM *> GetAnims(orxOBJECT *object, bool sorted = true)
    {
        auto animSet = GetAnimSet(object);
        return animset::GetAnims(animSet);
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

    const orxSTRING GetAnimSetPrefix(const orxSTRING animSetName)
    {
        orxConfig_PushSection(animSetName);
        auto prefix = orxConfig_GetString("Prefix");
        orxConfig_PopSection();
        return prefix;
    }

    void GetAnimSectionName(const orxSTRING animSetName, const orxSTRING animName, orxSTRING buf, size_t bufSize)
    {
        auto prefix = GetAnimSetPrefix(animSetName);
        orxString_NPrint(buf, bufSize, "%s%s", prefix, animName);
    }

    void GetAnimLinkSource(const orxSTRING animName, orxSTRING out, size_t outLen)
    {
        orxString_NPrint(out, outLen, "%s->", animName);
    }

    std::vector<const orxCHAR *> GetAnimLinks(const orxSTRING animSetName, const orxSTRING srcAnim)
    {
        orxConfig_PushSection(animSetName);
        orxCHAR src[64];
        GetAnimLinkSource(srcAnim, src, sizeof(src));
        auto count = orxConfig_GetListCount(src);
        std::vector<const orxCHAR *> dests{};
        dests.resize(count);
        for (size_t i = 0; i < count; i++)
        {
            dests[i] = orxConfig_GetListString(src, i);
        }
        orxConfig_PopSection();
        return dests;
    }

    std::string AnimSourceName(const orxSTRING animName)
    {
        return std::string{animName} + "->";
    }

    void AddAnimLink(const orxSTRING animSetName, const orxSTRING srcAnim, const orxSTRING dstAnim)
    {
        orxConfig_PushSection(animSetName);

        // Source has a -> suffix
        auto src = AnimSourceName(srcAnim);

        orxConfig_AppendListString(src.data(), &dstAnim, 1);
        orxConfig_PopSection();
    }

    void SetAnimLinks(const orxSTRING animSetName, const orxSTRING srcAnim, std::vector<std::string> &dstAnims)
    {
        auto src = AnimSourceName(srcAnim);
        orxConfig_PushSection(animSetName);
        if (dstAnims.size() > 0)
        {
            orxCHAR *links[dstAnims.size()];
            for (int i = 0; i < dstAnims.size(); i++)
                links[i] = dstAnims[i].data();
            orxConfig_SetListString(src.data(), (const orxCHAR **)links, dstAnims.size());
        }
        else
        {
            orxConfig_ClearValue(src.data());
        }
        orxConfig_PopSection();
    }

    void AddStartAnim(const orxSTRING animSetName, const orxSTRING animName)
    {
        orxConfig_PushSection(animSetName);
        orxConfig_AppendListString("StartAnimList", &animName, 1);
        orxConfig_PopSection();
    }

    // Saving animation changes to config
    std::set<std::string> sectionsToSave{};

    orxBOOL SaveCallback(const orxSTRING section, const orxSTRING key, const orxSTRING file, orxBOOL useEncryption)
    {
        return sectionsToSave.contains(std::string{section});
    }

    void Save(const orxSTRING file, orxOBJECT *object)
    {
        // Save the animation set section
        auto animSet = object::GetAnimSet(object);
        sectionsToSave.insert(orxAnimSet_GetName(animSet));

        // Save the section for each individual animation
        auto anims = object::GetAnims(object);
        for (auto anim : anims)
        {
            orxCHAR buf[256];
            GetAnimSectionName(orxAnimSet_GetName(animSet), orxAnim_GetName(anim), buf, sizeof(buf));
            sectionsToSave.insert(buf);
        }

        orxConfig_Save(file, orxFALSE, SaveCallback);

        // Clear the set of sections to save
        sectionsToSave.clear();
    }

}

namespace gui
{
    void AnimWindow(const orxSTRING animSetName, const orxSTRING prefix, const orxSTRING name)
    {
        orxCHAR sectionName[256];
        config::GetAnimSectionName(animSetName, name, sectionName, sizeof(sectionName));

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

    void AnimSetWindow(const orxANIMSET *animSet)
    {
        auto animSetName = orxAnimSet_GetName(animSet);
        static const auto configKey = "FrameSize";

        orxASSERT(orxConfig_PushSection(animSetName));

        orxCHAR title[256];
        orxString_NPrint(title, sizeof(title), "Animation Set: %s", animSetName);

        ImGui::Begin(title);

        // Save changes
        if (ImGui::Button("Save"))
        {
            save = std::string{orxConfig_GetOrigin(animSetName)};
        }

        auto frameSize = orxVECTOR_0;
        orxConfig_GetVector(configKey, &frameSize);

        // Add a new animation
        {
            static orxCHAR newAnimName[64];
            ImGui::InputTextWithHint("", "<new animation name>", newAnimName, sizeof(newAnimName));
            ImGui::SameLine();
            ImGui::SmallButton("Add animation");
            if (ImGui::IsItemActivated())
            {
                configChanged = orxTRUE;

                config::SetAnimFrames(animSetName, newAnimName, 1);
                config::AddStartAnim(animSetName, newAnimName);

                orxCHAR sectionName[64];
                config::GetAnimSectionName(animSetName, newAnimName, sectionName, sizeof(sectionName));
                orxConfig_PushSection(sectionName);
                orxConfig_SetFloat("KeyDuration", 0.1);
                orxConfig_SetVector("TextureOrigin", &orxVECTOR_0);
                orxConfig_PopSection();

                newAnimName[0] = '\0';
            }
        }

        // Show all animations in the set
        if (ImGui::CollapsingHeader("Animations"))
        {
            static const orxSTRING selectedAnimation = orxSTRING_EMPTY;
            auto animations = animset::GetAnims(animSet);
            for (auto anim : animations)
            {
                auto name = orxAnim_GetName(anim);
                bool selected = orxString_Compare(name, selectedAnimation) == 0;
                if (ImGui::Selectable(name, selected))
                {
                    selectedAnimation = name;
                }
                if (selected)
                {
                    // Separate window for viewing/editing config for the selected animation
                    AnimWindow(animSetName, config::GetAnimSetPrefix(animSetName), name);

                    // Track changes to animation links so we can apply them
                    auto changed = false;
                    std::vector<std::string> updatedLinks;

                    ImGui::PushID(name);

                    ImGui::Indent();

                    // Animation links for the selected animation
                    if (ImGui::CollapsingHeader("Links"))
                    {
                        const auto links = config::GetAnimLinks(animSetName, name);
                        for (const auto link : links)
                        {
                            ImGui::PushID(link);

                            std::string originalLink{link};
                            orxCHAR linkText[64];
                            orxString_NCopy(linkText, link, sizeof(linkText));

                            ImGui::InputTextWithHint("", "<animation link>", linkText, sizeof(linkText));
                            ImGui::SameLine();
                            auto apply = ImGui::Button("Apply");
                            ImGui::SameLine();
                            auto remove = ImGui::Button("Remove");

                            if (!remove)
                            {
                                if (!apply)
                                    updatedLinks.push_back(originalLink);
                                else
                                    updatedLinks.push_back(std::string{linkText});
                            }

                            if (apply || remove)
                                changed = true;

                            ImGui::PopID();
                        }

                        // Add a new link
                        ImGui::PushID("New Link Input");
                        static orxCHAR linkText[64] = "";
                        ImGui::InputTextWithHint("", "<animation link>", linkText, sizeof(linkText));
                        ImGui::SameLine();
                        auto add = ImGui::Button("Add");
                        ImGui::PopID();
                        if (add)
                        {
                            changed = true;
                            updatedLinks.push_back(std::string{linkText});
                            linkText[0] = '\0';
                        }
                    }

                    ImGui::Unindent();

                    ImGui::PopID();

                    if (changed)
                    {
                        configChanged = orxTRUE;
                        config::SetAnimLinks(animSetName, name, updatedLinks);
                    }
                }
            }
        }

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
        if (ImGui::CollapsingHeader("Texture"))
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
                float regionX = orxFLOAT_0;
                float regionY = orxFLOAT_0;
                if (snap)
                {
                    regionX = floorf((io.MousePos.x - pos.x) / frameSize.fX) * frameSize.fX;
                    regionY = floorf((io.MousePos.y - pos.y) / frameSize.fY) * frameSize.fY;
                }
                else
                {
                    regionX = io.MousePos.x - pos.x - frameSize.fX * 0.5f;
                    regionY = io.MousePos.y - pos.y - frameSize.fY * 0.5f;
                }
                float zoom = 4.0f;
                if (regionX < 0.0f)
                {
                    regionX = 0.0f;
                }
                else if (regionX > textureWidth - frameSize.fX)
                {
                    regionX = textureWidth - frameSize.fX;
                }
                if (regionY < 0.0f)
                {
                    regionY = 0.0f;
                }
                else if (regionY > textureHeight - frameSize.fY)
                {
                    regionY = textureHeight - frameSize.fY;
                }
                ImGui::Text("Min: (%.2f, %.2f)", regionX, regionY);
                ImGui::Text("Max: (%.2f, %.2f)", regionX + frameSize.fX, regionY + frameSize.fY);
                ImVec2 uv0 = ImVec2((regionX) / textureWidth, (regionY) / textureHeight);
                ImVec2 uv1 = ImVec2((regionX + frameSize.fX) / textureWidth, (regionY + frameSize.fY) / textureHeight);
                ImGui::Image(textureID, ImVec2(frameSize.fX * zoom, frameSize.fY * zoom), uv0, uv1);
                ImGui::EndTooltip();
            }
        }

        orxConfig_PopSection();

        ImGui::End();
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
            orxASSERT(animSetName.length() > 0);

            config::GetAnimSetPrefix(animSetName.data());

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
                    // orxObject_SetTargetAnim(object, animName);
                    orxObject_SetCurrentAnim(object, animName);
                }
                if (active)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void ObjectWindow(orxOBJECT *object)
    {
        orxASSERT(object);
        orxCHAR title[256];
        orxString_NPrint(title, sizeof(title), "Object: %s", orxObject_GetName(object));

        ImGui::Begin(title);

        ScaleInput(object);
        AnimationText(object);
        AnimationRateInput(object);
        ImGui::LabelText("AnimationSet name", "%s", object::GetAnimSetName(targetObject));
        TargetAnimationCombo(object);

        ImGui::End();
    }
}

/** Update function, it has been registered to be called every tick of the core clock
 */
void orxFASTCALL Update(const orxCLOCK_INFO *_pstClockInfo, void *_pContext)
{
    // Re-create the object if configuration has changed
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

    // Save our changes if requested
    if (save.has_value())
    {
        config::Save(save.value().data(), targetObject);
    }

    // Show top level windows
    gui::ObjectWindow(targetObject);
    gui::AnimSetWindow(object::GetAnimSet(targetObject));

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
