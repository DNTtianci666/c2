#include <array>
#include <cwctype>
#include <fstream>
#include <functional>
#include <string>
#include <ShlObj.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_stdlib.h"

#include "imguiCustom.h"

#include "Hacks/AntiAim.h"
#include "Hacks/Backtrack.h"
#include "Hacks/Glow.h"
#include "Hacks/Misc.h"
#include "Hacks/SkinChanger.h"
#include "Hacks/Sound.h"
#include "Hacks/Visuals.h"

#include "GUI.h"
#include "Config.h"
#include "Helpers.h"
#include "Hooks.h"
#include "Interfaces.h"

#include "SDK/InputSystem.h"

constexpr auto windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
| ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

static ImFont* addFontFromVFONT(const std::string& path, float size, const ImWchar* glyphRanges, bool merge) noexcept
{
    auto file = Helpers::loadBinaryFile(path);
    if (!Helpers::decodeVFONT(file))
        return nullptr;

    ImFontConfig cfg;
    cfg.FontData = file.data();
    cfg.FontDataSize = file.size();
    cfg.FontDataOwnedByAtlas = false;
    cfg.MergeMode = merge;
    cfg.GlyphRanges = glyphRanges;
    cfg.SizePixels = size;

    return ImGui::GetIO().Fonts->AddFont(&cfg);
}

GUI::GUI() noexcept
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.ScrollbarSize = 11.5f;

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImFontConfig cfg;
    cfg.SizePixels = 15.0f;

    if (PWSTR pathToFonts; SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Fonts, 0, nullptr, &pathToFonts))) {
        const std::filesystem::path path{ pathToFonts };
        CoTaskMemFree(pathToFonts);

        fonts.normal15px = io.Fonts->AddFontFromFileTTF((path / "msyh.ttf").string().c_str(), 15.0f, &cfg, Helpers::getFontGlyphRanges());
        if (!fonts.normal15px)
            io.Fonts->AddFontDefault(&cfg);

        fonts.tahoma34 = io.Fonts->AddFontFromFileTTF((path / "msyh.ttf").string().c_str(), 34.0f, &cfg, Helpers::getFontGlyphRanges());
        if (!fonts.tahoma34)
            io.Fonts->AddFontDefault(&cfg);

        fonts.tahoma28 = io.Fonts->AddFontFromFileTTF((path / "msyh.ttf").string().c_str(), 28.0f, &cfg, Helpers::getFontGlyphRanges());
        if (!fonts.tahoma28)
            io.Fonts->AddFontDefault(&cfg);

        cfg.MergeMode = true;
        static constexpr ImWchar symbol[]{
            0x2605, 0x2605, // ★
            0
        };
        io.Fonts->AddFontFromFileTTF((path / "msyh.ttf").string().c_str(), 15.0f, &cfg, symbol);
        cfg.MergeMode = false;
    }

    if (!fonts.normal15px)
        io.Fonts->AddFontDefault(&cfg);
    if (!fonts.tahoma28)
        io.Fonts->AddFontDefault(&cfg);
    if (!fonts.tahoma34)
        io.Fonts->AddFontDefault(&cfg);
    addFontFromVFONT("csgo/panorama/fonts/notosanskr-regular.vfont", 15.0f, io.Fonts->GetGlyphRangesKorean(), true);
    addFontFromVFONT("csgo/panorama/fonts/notosanssc-regular.vfont", 15.0f, io.Fonts->GetGlyphRangesChineseFull(), true);
    constexpr auto unicodeFontSize = 16.0f;
    fonts.unicodeFont = addFontFromVFONT("csgo/panorama/fonts/notosans-bold.vfont", unicodeFontSize, Helpers::getFontGlyphRanges(), false);
}

void GUI::render() noexcept
{
    renderGuiStyle();
}

#include "InputUtil.h"

static void hotkey3(const char* label, KeyBind& key, float samelineOffset = 0.0f, const ImVec2& size = { 100.0f, 0.0f }) noexcept
{
    const auto id = ImGui::GetID(label);
    ImGui::PushID(label);

    ImGui::TextUnformatted(label);
    ImGui::SameLine(samelineOffset);

    if (ImGui::GetActiveID() == id) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_ButtonActive));
        ImGui::Button("...", size);
        ImGui::PopStyleColor();

        ImGui::GetCurrentContext()->ActiveIdAllowOverlap = true;
        if ((!ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[0]) || key.setToPressedKey())
            ImGui::ClearActiveID();
    }
    else if (ImGui::Button(key.toString(), size)) {
        ImGui::SetActiveID(id, ImGui::GetCurrentWindow());
    }

    ImGui::PopID();
}

ImFont* GUI::getTahoma28Font() const noexcept
{
    return fonts.tahoma28;
}

ImFont* GUI::getUnicodeFont() const noexcept
{
    return fonts.unicodeFont;
}

void GUI::handleToggle() noexcept
{
    if (config->misc.menuKey.isPressed()) {
        open = !open;
        if (!open)
            interfaces->inputSystem->resetInputState();
    }
}

static void menuBarItem(const char* name, bool& enabled) noexcept
{
    if (ImGui::MenuItem(name)) {
        enabled = true;
        ImGui::SetWindowFocus(name);
        ImGui::SetWindowPos(name, { 100.0f, 100.0f });
    }
}

void GUI::renderLegitbotWindow() noexcept
{
    static const char* hitboxes[]{ "头","胸部","肚子","手臂","腿" };
    static bool hitbox[ARRAYSIZE(hitboxes)] = { false, false, false, false, false };
    static std::string previewvalue = "";
    bool once = false;

    ImGui::PushID("Key");
    ImGui::hotkey2("按键", config->legitbotKey);
    ImGui::PopID();
    ImGui::Separator();
    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);
    ImGui::Combo("", &currentCategory, "全部\0手枪\0重型武器\0冲锋枪\0步枪\0");
    ImGui::PopID();
    ImGui::SameLine();
    static int currentWeapon{ 0 };
    ImGui::PushID(1);

    switch (currentCategory) {
    case 0:
        currentWeapon = 0;
        ImGui::NewLine();
        break;
    case 1: {
        static int currentPistol{ 0 };
        static constexpr const char* pistols[]{ "全部", "格洛克18", "P2000", "USP-S", "双持贝雷塔", "P250", "Tec-9", "FN57", "CZ-75", "沙漠之鹰", "R8左轮" };

        ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
            if (config->legitbot[idx ? idx : 35].enabled) {
                static std::string name;
                name = pistols[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = pistols[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(pistols));

        currentWeapon = currentPistol ? currentPistol : 35;
        break;
    }
    case 2: {
        static int currentHeavy{ 0 };
        static constexpr const char* heavies[]{ "全部", "新星", "XM1014", "截短霰弹枪", "MAG-7", "M249", "内格夫" };

        ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
            if (config->legitbot[idx ? idx + 10 : 36].enabled) {
                static std::string name;
                name = heavies[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = heavies[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(heavies));

        currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
        break;
    }
    case 3: {
        static int currentSmg{ 0 };
        static constexpr const char* smgs[]{ "全部", "Mac-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP野牛" };

        ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
            if (config->legitbot[idx ? idx + 16 : 37].enabled) {
                static std::string name;
                name = smgs[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = smgs[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(smgs));

        currentWeapon = currentSmg ? currentSmg + 16 : 37;
        break;
    }
    case 4: {
        static int currentRifle{ 0 };
        static constexpr const char* rifles[]{ "全部", "加利尔AR", "法玛斯", "AK-47", "M4A4", "M4A1-S", "SSG-08", "SG-553", "AUG", "AWP", "G3SG1", "SCAR-20" };

        ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
            if (config->legitbot[idx ? idx + 23 : 38].enabled) {
                static std::string name;
                name = rifles[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = rifles[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(rifles));

        currentWeapon = currentRifle ? currentRifle + 23 : 38;
        break;
    }
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("开启", &config->legitbot[currentWeapon].enabled);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 220.0f);
    ImGui::Checkbox("自瞄锁定", &config->legitbot[currentWeapon].aimlock);
    ImGui::Checkbox("静默瞄准", &config->legitbot[currentWeapon].silent);
    ImGuiCustom::colorPicker("自瞄线圈", config->legitbotFov);
    ImGui::Checkbox("不分敌我", &config->legitbot[currentWeapon].friendlyFire);
    ImGui::Checkbox("仅可见自瞄", &config->legitbot[currentWeapon].visibleOnly);
    ImGui::Checkbox("仅开镜自瞄", &config->legitbot[currentWeapon].scopedOnly);
    ImGui::Checkbox("无视闪光自瞄", &config->legitbot[currentWeapon].ignoreFlash);
    ImGui::Checkbox("无视烟雾自瞄", &config->legitbot[currentWeapon].ignoreSmoke);
    ImGui::Checkbox("自动开镜", &config->legitbot[currentWeapon].autoScope);

    for (size_t i = 0; i < ARRAYSIZE(hitbox); i++)
    {
        hitbox[i] = (config->legitbot[currentWeapon].hitboxes & 1 << i) == 1 << i;
    }
    if (ImGui::BeginCombo("瞄准位置", previewvalue.c_str()))
    {
        previewvalue = "";
        for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++)
        {
            ImGui::Selectable(hitboxes[i], &hitbox[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
        }
        ImGui::EndCombo();
    }
    for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++)
    {
        if (!once)
        {
            previewvalue = "";
            once = true;
        }
        if (hitbox[i])
        {
            previewvalue += previewvalue.size() ? std::string(", ") + hitboxes[i] : hitboxes[i];
            config->legitbot[currentWeapon].hitboxes |= 1 << i;
        }
        else
        {
            config->legitbot[currentWeapon].hitboxes &= ~(1 << i);
        }
    }

    ImGui::NextColumn();
    ImGui::PushItemWidth(240.0f);
    ImGui::SliderFloat("自瞄范围", &config->legitbot[currentWeapon].fov, 0.0f, 255.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("自瞄平滑度", &config->legitbot[currentWeapon].smooth, 1.0f, 100.0f, "%.2f");
    ImGui::SliderInt("自瞄速度", &config->legitbot[currentWeapon].reactionTime, 0, 300, "%d");
    ImGui::SliderInt("最小伤害", &config->legitbot[currentWeapon].minDamage, 0, 101, "%d");
    config->legitbot[currentWeapon].minDamage = std::clamp(config->legitbot[currentWeapon].minDamage, 0, 250);
    ImGui::Checkbox("一击必杀", &config->legitbot[currentWeapon].killshot);
    ImGui::Checkbox("允许多次急停射击", &config->legitbot[currentWeapon].betweenShots);
    ImGui::Checkbox("自动压枪", &config->recoilControlSystem.enabled);
    ImGui::SameLine();
    ImGui::Checkbox("静默压枪", &config->recoilControlSystem.silent);
    ImGui::SliderInt("压枪延迟", &config->recoilControlSystem.shotsFired, 0, 150, "%d");
    ImGui::SliderFloat("最大压枪水平误差", &config->recoilControlSystem.horizontal, 0.0f, 1.0f, "%.5f");
    ImGui::SliderFloat("最大压枪垂直误差", &config->recoilControlSystem.vertical, 0.0f, 1.0f, "%.5f");
    ImGui::Columns(1);
}

void GUI::renderRagebotWindow() noexcept
{
    static const char* hitboxes[]{ "头","胸部","肚子","手臂","腿" };
    static bool hitbox[ARRAYSIZE(hitboxes)] = { false, false, false, false, false };
    static std::string previewvalue = "";
    bool once = false;

    ImGui::PushID("Ragebot Key");
    ImGui::hotkey2("按键", config->ragebotKey);
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Separator();
    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);
    ImGui::Combo("", &currentCategory, "全部\0手枪\0重型武器\0冲锋枪\0步枪\0");
    ImGui::PopID();
    ImGui::SameLine();
    static int currentWeapon{ 0 };
    ImGui::PushID(1);

    switch (currentCategory) {
    case 0:
        currentWeapon = 0;
        ImGui::NewLine();
        break;
    case 1: {
        static int currentPistol{ 0 };
        static constexpr const char* pistols[]{ "全部", "格洛克18", "P2000", "USP-S", "双持贝雷塔", "P250", "Tec-9", "FN57", "CZ-75", "沙漠之鹰", "R8左轮" };

        ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
            if (config->ragebot[idx ? idx : 35].enabled) {
                static std::string name;
                name = pistols[idx];
                *out_text = name.append(" *").c_str();
            }
            else {
                *out_text = pistols[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(pistols));

        currentWeapon = currentPistol ? currentPistol : 35;
        break;
    }
    case 2: {
        static int currentHeavy{ 0 };
        static constexpr const char* heavies[]{ "全部", "新星", "XM1014", "截短霰弹枪", "MAG-7", "M249", "内格夫" };

        ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
            if (config->ragebot[idx ? idx + 10 : 36].enabled) {
                static std::string name;
                name = heavies[idx];
                *out_text = name.append(" *").c_str();
            }
            else {
                *out_text = heavies[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(heavies));

        currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
        break;
    }
    case 3: {
        static int currentSmg{ 0 };
        static constexpr const char* smgs[]{ "全部", "Mac-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP野牛" };

        ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
            if (config->ragebot[idx ? idx + 16 : 37].enabled) {
                static std::string name;
                name = smgs[idx];
                *out_text = name.append(" *").c_str();
            }
            else {
                *out_text = smgs[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(smgs));

        currentWeapon = currentSmg ? currentSmg + 16 : 37;
        break;
    }
    case 4: {
        static int currentRifle{ 0 };
        static constexpr const char* rifles[]{ "全部", "加利尔AR", "法玛斯", "AK-47", "M4A4", "M4A1-S", "SSG-08", "SG-553", "AUG", "AWP", "G3SG1", "SCAR-20" };

        ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
            if (config->ragebot[idx ? idx + 23 : 38].enabled) {
                static std::string name;
                name = rifles[idx];
                *out_text = name.append(" *").c_str();
            }
            else {
                *out_text = rifles[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(rifles));

        currentWeapon = currentRifle ? currentRifle + 23 : 38;
        break;
    }
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("开启", &config->ragebot[currentWeapon].enabled);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 220.0f);
    ImGui::Checkbox("自瞄锁定", &config->ragebot[currentWeapon].aimlock);
    ImGui::Checkbox("静默瞄准", &config->ragebot[currentWeapon].silent);
    ImGui::Checkbox("不分敌我", &config->ragebot[currentWeapon].friendlyFire);
    ImGui::Checkbox("仅可见自瞄", &config->ragebot[currentWeapon].visibleOnly);
    ImGui::Checkbox("仅开镜自瞄", &config->ragebot[currentWeapon].scopedOnly);
    ImGui::Checkbox("无视闪光自瞄", &config->ragebot[currentWeapon].ignoreFlash);
    ImGui::Checkbox("无视烟雾自瞄", &config->ragebot[currentWeapon].ignoreSmoke);
    ImGui::Checkbox("自动开枪", &config->ragebot[currentWeapon].autoShot);
    ImGui::Checkbox("自动开镜", &config->ragebot[currentWeapon].autoScope);
    ImGui::Checkbox("自动急停", &config->ragebot[currentWeapon].autoStop);
    ImGui::SameLine();
    ImGui::Checkbox("允许多次急停射击", &config->ragebot[currentWeapon].betweenShots);
    ImGui::Checkbox("低FPS时禁用多点", &config->ragebot[currentWeapon].disableMultipointIfLowFPS);
    ImGui::Checkbox("低FPS时禁用回溯", &config->ragebot[currentWeapon].disableBacktrackIfLowFPS);
    ImGui::Combo("优先攻击", &config->ragebot[currentWeapon].priority, "基于生命值\0基于距离\0基于范围\0");

    for (size_t i = 0; i < ARRAYSIZE(hitbox); i++)
    {
        hitbox[i] = (config->ragebot[currentWeapon].hitboxes & 1 << i) == 1 << i;
    }
    if (ImGui::BeginCombo("自瞄部位", previewvalue.c_str()))
    {
        previewvalue = "";
        for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++)
        {
            ImGui::Selectable(hitboxes[i], &hitbox[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
        }
        ImGui::EndCombo();
    }
    for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++)
    {
        if (!once)
        {
            previewvalue = "";
            once = true;
        }
        if (hitbox[i])
        {
            previewvalue += previewvalue.size() ? std::string(", ") + hitboxes[i] : hitboxes[i];
            config->ragebot[currentWeapon].hitboxes |= 1 << i;
        }
        else
        {
            config->ragebot[currentWeapon].hitboxes &= ~(1 << i);
        }
    }

    ImGui::NextColumn();
    ImGui::PushItemWidth(240.0f);
    ImGui::SliderFloat("自瞄范围", &config->ragebot[currentWeapon].fov, 0.0f, 255.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderInt("命中率", &config->ragebot[currentWeapon].hitChance, 0, 100, "%d");
    ImGui::SliderInt("DT最小伤害", &config->ragebot[currentWeapon].multiPoint, 0, 100, "%d");
    ImGui::SliderInt("最小伤害", &config->ragebot[currentWeapon].minDamage, 0, 101, "%d");
    config->ragebot[currentWeapon].minDamage = std::clamp(config->ragebot[currentWeapon].minDamage, 0, 250);
    ImGui::PushID("Min damage override Key");
    ImGui::hotkey2("最小伤害覆盖按键", config->minDamageOverrideKey);
    ImGui::PopID();
    ImGui::SliderInt("最小伤害覆盖", &config->ragebot[currentWeapon].minDamageOverride, 0, 101, "%d");
    config->ragebot[currentWeapon].minDamageOverride = std::clamp(config->ragebot[currentWeapon].minDamageOverride, 0, 250);

    ImGui::PushID("Doubletap");
    ImGui::hotkey2("双击（DT）", config->tickbase.doubletap);
    ImGui::PopID();
    ImGui::PushID("Hideshots");
    ImGui::hotkey2("不抬头射击", config->tickbase.hideshots);
    ImGui::PopID();
    ImGui::Checkbox("射击后闪现", &config->tickbase.teleport);

    ImGui::Columns(1);
}

void GUI::renderTriggerbotWindow() noexcept
{
    static const char* hitboxes[]{ "头","胸部","肚子","手臂","腿" };
    static bool hitbox[ARRAYSIZE(hitboxes)] = { false, false, false, false, false };
    static std::string previewvalue = "";

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 300.0f);

    ImGui::hotkey2("按键", config->triggerbotKey, 80.0f);
    ImGui::Separator();

    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);
    ImGui::Combo("", &currentCategory, "全部\0手枪\0重型武器\0冲锋枪\0步枪\0电击枪\0");
    ImGui::PopID();
    ImGui::SameLine();
    static int currentWeapon{ 0 };
    ImGui::PushID(1);
    switch (currentCategory) {
    case 0:
        currentWeapon = 0;
        ImGui::NewLine();
        break;
    case 5:
        currentWeapon = 39;
        ImGui::NewLine();
        break;

    case 1: {
        static int currentPistol{ 0 };
        static constexpr const char* pistols[]{ "全部", "格洛克18", "P2000", "USP-S", "双持贝雷塔", "P250", "Tec-9", "FN57", "CZ-75", "沙漠之鹰", "R8左轮" };

        ImGui::Combo("", &currentPistol, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx : 35].enabled) {
                static std::string name;
                name = pistols[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = pistols[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(pistols));

        currentWeapon = currentPistol ? currentPistol : 35;
        break;
    }
    case 2: {
        static int currentHeavy{ 0 };
        static constexpr const char* heavies[]{ "All", "新星", "XM1014", "截短霰弹枪", "MAG-7", "M249", "Negev" };

        ImGui::Combo("", &currentHeavy, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx + 10 : 36].enabled) {
                static std::string name;
                name = heavies[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = heavies[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(heavies));

        currentWeapon = currentHeavy ? currentHeavy + 10 : 36;
        break;
    }
    case 3: {
        static int currentSmg{ 0 };
        static constexpr const char* smgs[]{ "全部", "Mac-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP野牛" };

        ImGui::Combo("", &currentSmg, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx + 16 : 37].enabled) {
                static std::string name;
                name = smgs[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = smgs[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(smgs));

        currentWeapon = currentSmg ? currentSmg + 16 : 37;
        break;
    }
    case 4: {
        static int currentRifle{ 0 };
        static constexpr const char* rifles[]{ "全部", "加利尔AR", "法玛斯", "AK-47", "M4A4", "M4A1-S", "SSG-08", "SG-553", "AUG", "AWP", "G3SG1", "SCAR-20" };

        ImGui::Combo("", &currentRifle, [](void* data, int idx, const char** out_text) {
            if (config->triggerbot[idx ? idx + 23 : 38].enabled) {
                static std::string name;
                name = rifles[idx];
                *out_text = name.append(" *").c_str();
            } else {
                *out_text = rifles[idx];
            }
            return true;
            }, nullptr, IM_ARRAYSIZE(rifles));

        currentWeapon = currentRifle ? currentRifle + 23 : 38;
        break;
    }
    }
    ImGui::PopID();
    ImGui::SameLine();
    ImGui::Checkbox("开启", &config->triggerbot[currentWeapon].enabled);
    ImGui::Checkbox("不分敌我", &config->triggerbot[currentWeapon].friendlyFire);
    ImGui::Checkbox("仅开镜开火", &config->triggerbot[currentWeapon].scopedOnly);
    ImGui::Checkbox("无视闪光开火", &config->triggerbot[currentWeapon].ignoreFlash);
    ImGui::Checkbox("无视烟雾开火", &config->triggerbot[currentWeapon].ignoreSmoke);
    ImGui::SetNextItemWidth(85.0f);

    for (size_t i = 0; i < ARRAYSIZE(hitbox); i++)
    {
        hitbox[i] = (config->triggerbot[currentWeapon].hitboxes & 1 << i) == 1 << i;
    }

    if (ImGui::BeginCombo("部位", previewvalue.c_str()))
    {
        previewvalue = "";
        for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++)
        {
            ImGui::Selectable(hitboxes[i], &hitbox[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
        }
        ImGui::EndCombo();
    }
    for (size_t i = 0; i < ARRAYSIZE(hitboxes); i++)
    {
        if (i == 0)
            previewvalue = "";

        if (hitbox[i])
        {
            previewvalue += previewvalue.size() ? std::string(", ") + hitboxes[i] : hitboxes[i];
            config->triggerbot[currentWeapon].hitboxes |= 1 << i;
        }
        else
        {
            config->triggerbot[currentWeapon].hitboxes &= ~(1 << i);
        }
    }
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("开火间隔", &config->triggerbot[currentWeapon].hitChance, 0, 100, "%d");
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("开火延迟", &config->triggerbot[currentWeapon].shotDelay, 0, 250, "%d ms");
    ImGui::SliderInt("最小伤害", &config->triggerbot[currentWeapon].minDamage, 0, 101, "%d");
    config->triggerbot[currentWeapon].minDamage = std::clamp(config->triggerbot[currentWeapon].minDamage, 0, 250);
    ImGui::Checkbox("一击必杀", &config->triggerbot[currentWeapon].killshot);
    ImGui::NextColumn();
    ImGui::Columns(1);
}

void GUI::renderFakelagWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 300.f);
    ImGui::Checkbox("开启", &config->fakelag.enabled);
    ImGui::Combo("模式", &config->fakelag.mode, "静态\0自适应\0随机\0");
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("限度", &config->fakelag.limit, 1, 16, "%d");
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    ImGui::Columns(1);
}

void GUI::renderLegitAntiAimWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 300.f);
    ImGui::hotkey2("按键", config->legitAntiAim.invert, 80.0f);
    ImGui::Checkbox("开启", &config->legitAntiAim.enabled);
    ImGui::Checkbox("在空闲时间禁用", &config->disableInFreezetime);
    ImGui::Checkbox("延伸", &config->legitAntiAim.extend);
    ImGui::NextColumn();
    ImGui::Columns(1);
}

void GUI::renderRageAntiAimWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 300.f);
    ImGui::Checkbox("开启", &config->rageAntiAim.enabled);
    ImGui::Checkbox("在空闲时间禁用", &config->disableInFreezetime);
    ImGui::Combo("头部朝向", &config->rageAntiAim.pitch, "无\0向下\0中间\0向上\0");
    ImGui::Combo("身体朝向", reinterpret_cast<int*>(&config->rageAntiAim.yawBase), "无\0向前\0向后\0向右\0向左\0旋转\0");
    ImGui::Combo("身体朝向改变", reinterpret_cast<int*>(&config->rageAntiAim.yawModifier), "无\0抖动\0");
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("朝向角度", &config->rageAntiAim.yawAdd, -180, 180, "%d");
    ImGui::PopItemWidth();

    if (config->rageAntiAim.yawModifier == 1) //Jitter
        ImGui::SliderInt("抖动范围", &config->rageAntiAim.jitterRange, 0, 90, "%d");

    if (config->rageAntiAim.yawBase == Yaw::spin)
    {
        ImGui::PushItemWidth(220.0f);
        ImGui::SliderInt("旋转角度", &config->rageAntiAim.spinBase, -180, 180, "%d");
        ImGui::PopItemWidth();
    }

    ImGui::Checkbox("自动面向敌人", &config->rageAntiAim.atTargets);

    ImGui::hotkey2("身体朝向前按键", config->rageAntiAim.manualForward, 60.f);
    ImGui::hotkey2("身体朝向后按键", config->rageAntiAim.manualBackward, 60.f);
    ImGui::hotkey2("身体朝向右按键", config->rageAntiAim.manualRight, 60.f);
    ImGui::hotkey2("身体朝向左按键", config->rageAntiAim.manualLeft, 60.f);

    ImGui::NextColumn();
    ImGui::Columns(1);
}

void GUI::renderFakeAngleWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 300.f);
    ImGui::hotkey2("按键", config->fakeAngle.invert, 80.0f);
    ImGui::Checkbox("开启", &config->fakeAngle.enabled);

    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("左侧角度限制", &config->fakeAngle.leftLimit, 0, 60, "%d");
    ImGui::PopItemWidth();

    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("右侧角度限制", &config->fakeAngle.rightLimit, 0, 60, "%d");
    ImGui::PopItemWidth();

    ImGui::Combo("Peek类型", &config->fakeAngle.peekMode, "无\0Peek真身\0Peek假身\0随机\0");
    ImGui::Combo("Lby类型", &config->fakeAngle.lbyMode, "正常\0逆向（静态）\0摇摆（动态）\0");

    ImGui::NextColumn();
    ImGui::Columns(1);
}

void GUI::renderBacktrackWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 300.f);
    ImGui::Checkbox("开启", &config->backtrack.enabled);
    ImGui::Checkbox("无视烟雾", &config->backtrack.ignoreSmoke);
    ImGui::Checkbox("无视闪光", &config->backtrack.ignoreFlash);
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("回溯时间", &config->backtrack.timeLimit, 1, 200, "%d ms");
    ImGui::PopItemWidth();
    ImGui::NextColumn();
    ImGui::Checkbox("开启假延迟", &config->backtrack.fakeLatency);
    ImGui::PushItemWidth(220.0f);
    ImGui::SliderInt("Ping", &config->backtrack.fakeLatencyAmount, 1, 200, "%d ms");
    ImGui::PopItemWidth();
    ImGui::Columns(1);
}

void GUI::renderChamsWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 300.0f);
    ImGui::hotkey2("按键", config->chamsKey, 80.0f);
    ImGui::Separator();

    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);

    static int material = 1;

    if (ImGui::Combo("", &currentCategory, "队友\0敌人\0下包\0拆除\0本地玩家\0武器\0手\0回溯\0袖子\0垂直\0布娃娃\0假滞后\0"))
        material = 1;

    ImGui::PopID();

    ImGui::SameLine();

    if (material <= 1)
        ImGuiCustom::arrowButtonDisabled("##left", ImGuiDir_Left);
    else if (ImGui::ArrowButton("##left", ImGuiDir_Left))
        --material;

    ImGui::SameLine();
    ImGui::Text("%d", material);

   constexpr std::array categories{ "Allies", "Enemies", "Planting", "Defusing", "Local player", "Weapons", "Hands", "Backtrack", "Sleeves", "Desync", "Ragdolls", "Fake lag" };
   constexpr std::array categoriesBSK{ "队友", "敌人", "下包", "拆除", "本地玩家", "武器", "手", "回溯", "袖子", "垂直", "布娃娃", "假滞后"};

    ImGui::SameLine();

    if (material >= int(config->chams[categories[currentCategory]].materials.size()))
        ImGuiCustom::arrowButtonDisabled("##right", ImGuiDir_Right);
    else if (ImGui::ArrowButton("##right", ImGuiDir_Right))
        ++material;

    ImGui::SameLine();

    auto& chams{ config->chams[categories[currentCategory]].materials[material - 1] };

    ImGui::Checkbox("开启", &chams.enabled);
    ImGui::Separator();
    ImGui::Checkbox("基于血量", &chams.healthBased);
    ImGui::Checkbox("人物闪烁", &chams.blinking);
    ImGui::Combo("材质", &chams.material, "经典的\0扁平\0钝化\0铂\0玻璃\0铬\0晶体\0银\0金\0塑料\0夜光\0珍珠\0金属\0");
    ImGui::Checkbox("人物框架", &chams.wireframe);
    ImGui::Checkbox("覆盖", &chams.cover);
    ImGui::Checkbox("墙后透视", &chams.ignorez);
    ImGuiCustom::colorPicker("颜色", chams);
    ImGui::NextColumn();
    ImGui::Columns(1);
}

void GUI::renderGlowWindow() noexcept
{
    ImGui::hotkey2("按键", config->glowKey, 80.0f);
    ImGui::Separator();

    static int currentCategory{ 0 };
    ImGui::PushItemWidth(110.0f);
    ImGui::PushID(0);
    constexpr std::array categories{ "Allies", "Enemies", "Planting", "Defusing", "Local Player", "Weapons", "C4", "Planted C4", "Chickens", "Defuse Kits", "Projectiles", "Hostages", "Ragdolls" };
   constexpr std::array categoriesBSK{ "队友", "敌人", "下包", "拆除", "本地玩家", "武器", "C4", "已经安放的C4", "小鸡", "拆弹器", "投掷物", "人质", "布娃娃" };
    ImGui::Combo("", &currentCategory, categoriesBSK.data(), categoriesBSK.size());
    ImGui::PopID();
    Config::GlowItem* currentItem;
    if (currentCategory <= 3) {
        ImGui::SameLine();
        static int currentType{ 0 };
        ImGui::PushID(1);
        ImGui::Combo("", &currentType, "全部\0可见的\0遮挡的\0");
        ImGui::PopID();
        auto& cfg = config->playerGlow[categories[currentCategory]];
        switch (currentType) {
        case 0: currentItem = &cfg.all; break;
        case 1: currentItem = &cfg.visible; break;
        case 2: currentItem = &cfg.occluded; break;
        }
    }
    else {
        currentItem = &config->glow[categories[currentCategory]];
    }

    ImGui::SameLine();
    ImGui::Checkbox("开启", &currentItem->enabled);
    ImGui::Separator();
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 150.0f);
    ImGui::Checkbox("基于血量", &currentItem->healthBased);

    ImGuiCustom::colorPicker("颜色", *currentItem);

    ImGui::NextColumn();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("风格", &currentItem->style, "默认\0边缘3D\0边缘\0边缘脉冲\0");

    ImGui::Columns(1);
}

void GUI::renderStreamProofESPWindow() noexcept
{
    ImGui::hotkey2("按键", config->streamProofESP.key, 80.0f);
    ImGui::Separator();

    static std::size_t currentCategory;
    static auto currentItem = "All";

    constexpr auto getConfigShared = [](std::size_t category, const char* item) noexcept -> Shared& {
        switch (category) {
        case 0: default: return config->streamProofESP.enemies[item];
        case 1: return config->streamProofESP.allies[item];
        case 2: return config->streamProofESP.weapons[item];
        case 3: return config->streamProofESP.projectiles[item];
        case 4: return config->streamProofESP.lootCrates[item];
        case 5: return config->streamProofESP.otherEntities[item];
        }
    };

    constexpr auto getConfigPlayer = [](std::size_t category, const char* item) noexcept -> Player& {
        switch (category) {
        case 0: default: return config->streamProofESP.enemies[item];
        case 1: return config->streamProofESP.allies[item];
        }
    };

    if (ImGui::BeginListBox("##list", { 170.0f, 300.0f })) {
        constexpr std::array categories{ "敌人", "队友", "武器", "投掷物", "头号箱子", "其他实物" };

        for (std::size_t i = 0; i < categories.size(); ++i) {
            if (ImGui::Selectable(categories[i], currentCategory == i && std::string_view{ currentItem } == "All")) {
                currentCategory = i;
                currentItem = "All";
            }

            if (ImGui::BeginDragDropSource()) {
                switch (i) {
                case 0: case 1: ImGui::SetDragDropPayload("玩家", &getConfigPlayer(i, "All"), sizeof(Player), ImGuiCond_Once); break;
                case 2: ImGui::SetDragDropPayload("武器", &config->streamProofESP.weapons["All"], sizeof(Weapon), ImGuiCond_Once); break;
                case 3: ImGui::SetDragDropPayload("投掷物", &config->streamProofESP.projectiles["All"], sizeof(Projectile), ImGuiCond_Once); break;
                default: ImGui::SetDragDropPayload("实物", &getConfigShared(i, "All"), sizeof(Shared), ImGuiCond_Once); break;
                }
                ImGui::EndDragDropSource();
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("玩家")) {
                    const auto& data = *(Player*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("武器")) {
                    const auto& data = *(Weapon*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("投掷物")) {
                    const auto& data = *(Projectile*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }

                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("实物")) {
                    const auto& data = *(Shared*)payload->Data;

                    switch (i) {
                    case 0: case 1: getConfigPlayer(i, "All") = data; break;
                    case 2: config->streamProofESP.weapons["All"] = data; break;
                    case 3: config->streamProofESP.projectiles["All"] = data; break;
                    default: getConfigShared(i, "All") = data; break;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::PushID(i);
            ImGui::Indent();

            const auto items = [](std::size_t category) noexcept -> std::vector<const char*> {
                switch (category) {
                case 0:
                case 1: return { "可见的", "遮挡的" };
                case 2: return { "手枪", "冲锋枪", "步枪", "狙击枪", "散弹枪", "机枪", "投掷物", "近战", "其他" };
                case 3: return { "闪光弹", "榴弹手榴弹", "燃烧弹", "弹射炸弹", "诱饵弹", "燃烧瓶", "手雷", "烟雾弹", "雪球" };
                case 4: return { "手枪箱", "轻型武器箱", "重型武器箱", "投掷物箱", "工具箱", "钱提包" };
                case 5: return { "拆弹工具", "小鸡", "已安放C4", "人质", "哨兵", "现金", "弹药箱", "雷达干扰器", "雪球堆", "可收集硬币" };
                default: return { };
                }
            }(i);

            const auto categoryEnabled = getConfigShared(i, "All").enabled;

            for (std::size_t j = 0; j < items.size(); ++j) {
                static bool selectedSubItem;
                if (!categoryEnabled || getConfigShared(i, items[j]).enabled) {
                    if (ImGui::Selectable(items[j], currentCategory == i && !selectedSubItem && std::string_view{ currentItem } == items[j])) {
                        currentCategory = i;
                        currentItem = items[j];
                        selectedSubItem = false;
                    }

                    if (ImGui::BeginDragDropSource()) {
                        switch (i) {
                        case 0: case 1: ImGui::SetDragDropPayload("Player", &getConfigPlayer(i, items[j]), sizeof(Player), ImGuiCond_Once); break;
                        case 2: ImGui::SetDragDropPayload("Weapon", &config->streamProofESP.weapons[items[j]], sizeof(Weapon), ImGuiCond_Once); break;
                        case 3: ImGui::SetDragDropPayload("Projectile", &config->streamProofESP.projectiles[items[j]], sizeof(Projectile), ImGuiCond_Once); break;
                        default: ImGui::SetDragDropPayload("Entity", &getConfigShared(i, items[j]), sizeof(Shared), ImGuiCond_Once); break;
                        }
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                            const auto& data = *(Player*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                            const auto& data = *(Weapon*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                            const auto& data = *(Projectile*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                            const auto& data = *(Shared*)payload->Data;

                            switch (i) {
                            case 0: case 1: getConfigPlayer(i, items[j]) = data; break;
                            case 2: config->streamProofESP.weapons[items[j]] = data; break;
                            case 3: config->streamProofESP.projectiles[items[j]] = data; break;
                            default: getConfigShared(i, items[j]) = data; break;
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                if (i != 2)
                    continue;

                ImGui::Indent();

                const auto subItems = [](std::size_t item) noexcept -> std::vector<const char*> {
                    switch (item) {
                    case 0: return { "格洛克18", "P2000", "USP-S", "双持贝雷塔", "P250", "Tec-9", "FN57", "CZ75", "沙漠之鹰", "R8左轮" };
                    case 1: return { "MAC-10", "MP9", "MP7", "MP5-SD", "UMP-45", "P90", "PP野牛" };
                    case 2: return { "加利尔AR", "法玛斯", "AK-47", "M4A4", "M4A1-S", "SG 553", "AUG" };
                    case 3: return { "SSG 08", "AWP", "G3SG1", "SCAR-20" };
                    case 4: return { "新星", "XM1014", "锯短型散弹", "MAG-7" };
                    case 5: return { "M249", "内格夫" };
                    case 6: return { "闪光弹", "榴弹", "烟雾弹", "燃烧瓶", "诱饵弹", "燃烧弹", "手雷", "火炸弹", "弹射地雷", "碎片榴弹", "雪球" };
                    case 7: return { "斧头", "锤子", "扳手" };
                    case 8: return { "C4", "医疗针", "跳跃炸弹", "信号干扰器", "盾" };
                    default: return { };
                    }
                }(j);

                const auto itemEnabled = getConfigShared(i, items[j]).enabled;

                for (const auto subItem : subItems) {
                    auto& subItemConfig = config->streamProofESP.weapons[subItem];
                    if ((categoryEnabled || itemEnabled) && !subItemConfig.enabled)
                        continue;

                    if (ImGui::Selectable(subItem, currentCategory == i && selectedSubItem && std::string_view{ currentItem } == subItem)) {
                        currentCategory = i;
                        currentItem = subItem;
                        selectedSubItem = true;
                    }

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("Weapon", &subItemConfig, sizeof(Weapon), ImGuiCond_Once);
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Player")) {
                            const auto& data = *(Player*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Weapon")) {
                            const auto& data = *(Weapon*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Projectile")) {
                            const auto& data = *(Projectile*)payload->Data;
                            subItemConfig = data;
                        }

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("Entity")) {
                            const auto& data = *(Shared*)payload->Data;
                            subItemConfig = data;
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                ImGui::Unindent();
            }
            ImGui::Unindent();
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    ImGui::SameLine();
    if (ImGui::BeginChild("##child", { 400.0f, 0.0f })) {
        auto& sharedConfig = getConfigShared(currentCategory, currentItem);

        ImGui::Checkbox("启用", &sharedConfig.enabled);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 260.0f);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("字体", config->getSystemFonts()[sharedConfig.font.index].c_str())) {
            for (size_t i = 0; i < config->getSystemFonts().size(); i++) {
                bool isSelected = config->getSystemFonts()[i] == sharedConfig.font.name;
                if (ImGui::Selectable(config->getSystemFonts()[i].c_str(), isSelected, 0, { 250.0f, 0.0f })) {
                    sharedConfig.font.index = i;
                    sharedConfig.font.name = config->getSystemFonts()[i];
                    config->scheduleFontLoad(sharedConfig.font.name);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        constexpr auto spacing = 250.0f;
        ImGuiCustom::colorPicker("线条", sharedConfig.snapline);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::Combo("##1", &sharedConfig.snapline.type, "底部\0顶部\0准星\0");
        ImGui::SameLine(spacing);
        ImGuiCustom::colorPicker("方框", sharedConfig.box);
        ImGui::SameLine();

        ImGui::PushID("Box");

        if (ImGui::Button("..."))
            ImGui::OpenPopup("");

        if (ImGui::BeginPopup("")) {
            ImGui::SetNextItemWidth(95.0f);
            ImGui::Combo("类型", &sharedConfig.box.type, "2D\0" "2D四角\0" "3D\0" "3D四角\0");
            ImGui::SetNextItemWidth(275.0f);
            ImGui::SliderFloat3("比例", sharedConfig.box.scale.data(), 0.0f, 0.50f, "%.2f");
            ImGuiCustom::colorPicker("填充", sharedConfig.box.fill);
            ImGui::EndPopup();
        }

        ImGui::PopID();

        ImGuiCustom::colorPicker("名字", sharedConfig.name);
        ImGui::SameLine(spacing);

        if (currentCategory < 2) {
            auto& playerConfig = getConfigPlayer(currentCategory, currentItem);

            ImGuiCustom::colorPicker("显示武器", playerConfig.weapon);
            ImGuiCustom::colorPicker("闪光持续时间", playerConfig.flashDuration);
            ImGui::SameLine(spacing);
            ImGuiCustom::colorPicker("骨骼", playerConfig.skeleton);
            ImGui::Checkbox("仅显示发出声音的", &playerConfig.audibleOnly);
            ImGui::SameLine(spacing);
            ImGui::Checkbox("仅显示发现的", &playerConfig.spottedOnly);

            ImGuiCustom::colorPicker("头部方框", playerConfig.headBox);
            ImGui::SameLine();

            ImGui::PushID("Head Box");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                ImGui::SetNextItemWidth(95.0f);
                ImGui::Combo("类型", &playerConfig.headBox.type, "2D\0" "2D四角\0" "3D\0" "3D四角\0");
                ImGui::SetNextItemWidth(275.0f);
                ImGui::SliderFloat3("比例填充", playerConfig.headBox.scale.data(), 0.0f, 0.50f, "%.2f");
                ImGuiCustom::colorPicker("填充", playerConfig.headBox.fill);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        
            ImGui::SameLine(spacing);
            ImGui::Checkbox("血量显示", &playerConfig.healthBar.enabled);
            ImGui::SameLine();

            ImGui::PushID("Health Bar");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                ImGui::SetNextItemWidth(95.0f);
                ImGui::Combo("类型", &playerConfig.healthBar.type, "渐变色\0固定\0基于血量\0");
                if (playerConfig.healthBar.type == HealthBar::Solid) {
                    ImGui::SameLine();
                    ImGuiCustom::colorPicker("", static_cast<Color4&>(playerConfig.healthBar));
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();

            ImGuiCustom::colorPicker("脚步声", config->visuals.footsteps.footstepBeams);
            ImGui::SliderInt("幅度", &config->visuals.footsteps.footstepBeamThickness, 0, 30, "幅度: %d%%");
            ImGui::SliderInt("半径", &config->visuals.footsteps.footstepBeamRadius, 0, 230, "半径: %d%%");
            
            ImGuiCustom::colorPicker("准星线条", playerConfig.lineOfSight);

        } else if (currentCategory == 2) {
            auto& weaponConfig = config->streamProofESP.weapons[currentItem];
            ImGuiCustom::colorPicker("显示弹药", weaponConfig.ammo);
        } else if (currentCategory == 3) {
            auto& trails = config->streamProofESP.projectiles[currentItem].trails;

            ImGui::Checkbox("显示轨迹", &trails.enabled);
            ImGui::SameLine(spacing + 77.0f);
            ImGui::PushID("Trails");

            if (ImGui::Button("..."))
                ImGui::OpenPopup("");

            if (ImGui::BeginPopup("")) {
                constexpr auto trailPicker = [](const char* name, Trail& trail) noexcept {
                    ImGui::PushID(name);
                    ImGuiCustom::colorPicker(name, trail);
                    ImGui::SameLine(150.0f);
                    ImGui::SetNextItemWidth(95.0f);
                    ImGui::Combo("", &trail.type, "线\0圈\0实心圆\0");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(95.0f);
                    ImGui::InputFloat("时间", &trail.time, 0.1f, 0.5f, "%.1fs");
                    trail.time = std::clamp(trail.time, 1.0f, 60.0f);
                    ImGui::PopID();
                };

                trailPicker("本地玩家", trails.localPlayer);
                trailPicker("队友", trails.allies);
                trailPicker("敌人", trails.enemies);
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::SetNextItemWidth(95.0f);
        ImGui::InputFloat("文字间隔距离", &sharedConfig.textCullDistance, 0.4f, 0.8f, "%.1fm");
        sharedConfig.textCullDistance = std::clamp(sharedConfig.textCullDistance, 0.0f, 999.9f);
    }

    ImGui::EndChild();
}

void GUI::renderVisualsWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 280.0f);
    constexpr auto playerModels = "Default\0Special Agent Ava | FBI\0Operator | FBI SWAT\0Markus Delrow | FBI HRT\0Michael Syfers | FBI Sniper\0B Squadron Officer | SAS\0Seal Team 6 Soldier | NSWC SEAL\0Buckshot | NSWC SEAL\0Lt. Commander Ricksaw | NSWC SEAL\0Third Commando Company | KSK\0'Two Times' McCoy | USAF TACP\0Dragomir | Sabre\0Rezan The Ready | Sabre\0'The Doctor' Romanov | Sabre\0Maximus | Sabre\0Blackwolf | Sabre\0The Elite Mr. Muhlik | Elite Crew\0Ground Rebel | Elite Crew\0BSK | Elite Crew\0Prof. Shahmat | Elite Crew\0Enforcer | Phoenix\0Slingshot | Phoenix\0Soldier | Phoenix\0Pirate\0Pirate Variant A\0Pirate Variant B\0Pirate Variant C\0Pirate Variant D\0Anarchist\0Anarchist Variant A\0Anarchist Variant B\0Anarchist Variant C\0Anarchist Variant D\0Balkan Variant A\0Balkan Variant B\0Balkan Variant C\0Balkan Variant D\0Balkan Variant E\0Jumpsuit Variant A\0Jumpsuit Variant B\0Jumpsuit Variant C\0GIGN\0GIGN Variant A\0GIGN Variant B\0GIGN Variant C\0GIGN Variant D\0Street Soldier | Phoenix\0'Blueberries' Buckshot | NSWC SEAL\0'Two Times' McCoy | TACP Cavalry\0Rezan the Redshirt | Sabre\0Dragomir | Sabre Footsoldier\0Cmdr. Mae 'Dead Cold' Jamison | SWAT\0001st Lieutenant Farlow | SWAT\0John 'Van Healen' Kask | SWAT\0Bio-Haz Specialist | SWAT\0Sergeant Bombson | SWAT\0Chem-Haz Specialist | SWAT\0Sir Bloody Miami Darryl | The Professionals\0Sir Bloody Silent Darryl | The Professionals\0Sir Bloody Skullhead Darryl | The Professionals\0Sir Bloody Darryl Royale | The Professionals\0Sir Bloody Loudmouth Darryl | The Professionals\0Safecracker Voltzmann | The Professionals\0Little Kev | The Professionals\0Number K | The Professionals\0Getaway Sally | The Professionals\0";
    ImGui::Combo("T阵营探员", &config->visuals.playerModelT, playerModels);
    ImGui::Combo("CT阵营探员", &config->visuals.playerModelCT, playerModels);
    ImGui::InputText("自定义探员模型", config->visuals.playerModel, sizeof(config->visuals.playerModel));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("模型文件必须放在csgo/models/目录中 必须以models/...开头");
    ImGui::Checkbox("禁用后处理", &config->visuals.disablePostProcessing);
    ImGui::Checkbox("禁用骨骼抖动", &config->visuals.disableJiggleBones);
    ImGui::Checkbox("死后上天", &config->visuals.inverseRagdollGravity);
    ImGui::Checkbox("在范围内保持视野", &config->visuals.keepFov);
    ImGui::Checkbox("移除雾气", &config->visuals.noFog);

    ImGuiCustom::colorPicker("雾气控制器", config->visuals.fog);
    ImGui::SameLine();

    ImGui::PushID("Fog controller");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {

        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(7);
        ImGui::SliderFloat("开始", &config->visuals.fogOptions.start, 0.0f, 5000.0f, "Start: %.2f");
        ImGui::PopID();
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(8);
        ImGui::SliderFloat("结束", &config->visuals.fogOptions.end, 0.0f, 5000.0f, "End: %.2f");
        ImGui::PopID();
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(9);
        ImGui::SliderFloat("密集度", &config->visuals.fogOptions.density, 0.001f, 1.0f, "Density: %.3f");
        ImGui::PopID();

        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::Checkbox("移除天空", &config->visuals.no3dSky);
    ImGui::Checkbox("移除武器抖动", &config->visuals.noAimPunch);
    ImGui::Checkbox("移除视觉抖动", &config->visuals.noViewPunch);
    ImGui::Checkbox("移除风景摆动", &config->visuals.noViewBob);
    ImGui::Checkbox("移除手臂", &config->visuals.noHands);
    ImGui::Checkbox("移除袖子", &config->visuals.noSleeves);
    ImGui::Checkbox("移除武器", &config->visuals.noWeapons);
    ImGui::Checkbox("移除烟雾", &config->visuals.noSmoke);
    ImGui::SameLine();
    ImGui::Checkbox("线条烟雾", &config->visuals.wireframeSmoke);
    ImGui::Checkbox("移除火焰", &config->visuals.noMolotov);
    ImGui::SameLine();
    ImGui::Checkbox("线条火焰", &config->visuals.wireframeMolotov);
    ImGui::Checkbox("移除模糊", &config->visuals.noBlur);
    ImGui::Checkbox("移除开镜黑边", &config->visuals.noScopeOverlay);
    ImGui::Checkbox("移除草丛", &config->visuals.noGrass);
    ImGui::Checkbox("移除阴影", &config->visuals.noShadows);

    ImGui::Checkbox("阴影控制器", &config->visuals.shadowsChanger.enabled);
    ImGui::SameLine();

    ImGui::PushID("Shadow changer");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(5);
        ImGui::SliderInt("", &config->visuals.shadowsChanger.x, 0, 360, "x轴旋转: %d");
        ImGui::PopID();
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(6);
        ImGui::SliderInt("", &config->visuals.shadowsChanger.y, 0, 360, "y轴旋转: %d");
        ImGui::PopID();
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("运动模糊", &config->visuals.motionBlur.enabled);
    ImGui::SameLine();

    ImGui::PushID("Motion Blur");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {

        ImGui::Checkbox("手动调节", &config->visuals.motionBlur.forwardEnabled);

        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(10);
        ImGui::SliderFloat("最小模糊值", &config->visuals.motionBlur.fallingMin, 0.0f, 50.0f, "Falling min: %.2f");
        ImGui::PopID();
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(11);
        ImGui::SliderFloat("最大模糊值", &config->visuals.motionBlur.fallingMax, 0.0f, 50.0f, "Falling max: %.2f");
        ImGui::PopID();
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(12);
        ImGui::SliderFloat("旋转模糊值", &config->visuals.motionBlur.fallingIntensity, 0.0f, 8.0f, "Falling intensity: %.2f");
        ImGui::PopID();
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(12);
        ImGui::SliderFloat("旋转模糊强度", &config->visuals.motionBlur.rotationIntensity, 0.0f, 8.0f, "Rotation intensity: %.2f");
        ImGui::PopID();
        ImGui::PushItemWidth(290.0f);
        ImGui::PushID(12);
        ImGui::SliderFloat("模糊值强度", &config->visuals.motionBlur.strength, 0.0f, 8.0f, "Strength: %.2f");
        ImGui::PopID();

        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("地图高亮", &config->visuals.fullBright);
    ImGui::NextColumn();
    ImGui::Checkbox("视野放大", &config->visuals.zoom);
    ImGui::SameLine();
    ImGui::PushID("Zoom Key");
    ImGui::hotkey2("", config->visuals.zoomKey);
    ImGui::PopID();
    ImGui::Checkbox("第三人称", &config->visuals.thirdperson);
    ImGui::SameLine();
    ImGui::PushID("Thirdperson Key");
    ImGui::hotkey2("", config->visuals.thirdpersonKey);
    ImGui::PopID();
    ImGui::PushItemWidth(290.0f);
    ImGui::PushID(0);
    ImGui::SliderInt("", &config->visuals.thirdpersonDistance, 0, 1000, "第三人距离: %d");
    ImGui::PopID();
    ImGui::Checkbox("灵魂出窍", &config->visuals.freeCam);
    ImGui::SameLine();
    ImGui::PushID("Freecam Key");
    ImGui::hotkey2("", config->visuals.freeCamKey);
    ImGui::PopID();
    ImGui::PushItemWidth(290.0f);
    ImGui::PushID(1);
    ImGui::SliderInt("", &config->visuals.freeCamSpeed, 1, 10, "灵魂移动速度: %d");
    ImGui::PopID();
    ImGui::PushID(2);
    ImGui::SliderInt("", &config->visuals.fov, -60, 60, "视角范围: %d");
    ImGui::PopID();
    ImGui::PushID(3);
    ImGui::SliderInt("", &config->visuals.farZ, 0, 2000, "渲染距离: %d");
    ImGui::PopID();
    ImGui::PushID(4);
    ImGui::SliderInt("", &config->visuals.flashReduction, 0, 100, "闪光祛除度: %d%%");
    ImGui::PopID();
    ImGui::Combo("天空盒", &config->visuals.skybox, Visuals::skyboxList.data(), Visuals::skyboxList.size());
     if (config->visuals.skybox == 26) {
        ImGui::InputText("自定义天空盒", &config->visuals.customSkybox);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("天空盒文件必须放在csgo/materials/skybox/中 ");
    }
    ImGuiCustom::colorPicker("世界颜色", config->visuals.world);
    ImGuiCustom::colorPicker("道具颜色", config->visuals.props);
    ImGuiCustom::colorPicker("天空颜色", config->visuals.sky);
    ImGui::PushID(13);
    ImGui::SliderInt("", &config->visuals.asusWalls, 0, 100, "墙体透明度: %d");
    ImGui::PopID();
    ImGui::PushID(14);
    ImGui::SliderInt("", &config->visuals.asusProps, 0, 100, "其他透明度: %d");
    ImGui::PopID();
    ImGui::Checkbox("沙鹰旋转动作", &config->visuals.deagleSpinner);
    ImGui::Combo("屏幕效果", &config->visuals.screenEffect, "无\0黑白电视\0复古黑白电视\0水底\0治疗针\0被击中\0");
    ImGui::Combo("命中效果", &config->visuals.hitEffect, "无\0黑白电视\0复古黑白电视\0水底\0治疗针\0被击中\0");
    ImGui::SliderFloat("命中效果时间", &config->visuals.hitEffectTime, 0.1f, 1.5f, "%.2fs");
    ImGui::Combo("命中标记", &config->visuals.hitMarker, "无\0默认（交叉）\0");
    ImGui::SliderFloat("命中标记时间", &config->visuals.hitMarkerTime, 0.1f, 1.5f, "%.2fs");
    ImGuiCustom::colorPicker("弹道轨迹", config->visuals.bulletTracers.color.data(), &config->visuals.bulletTracers.color[3], nullptr, nullptr, &config->visuals.bulletTracers.enabled);
    ImGuiCustom::colorPicker("弹痕效果", config->visuals.bulletImpacts.color.data(), &config->visuals.bulletImpacts.color[3], nullptr, nullptr, &config->visuals.bulletImpacts.enabled);
    ImGui::SliderFloat("子弹效果持续时间", &config->visuals.bulletImpactsTime, 0.1f, 5.0f, "子弹效果持续时间: %.2fs");
    ImGuiCustom::colorPicker("命中信息", config->visuals.onHitHitbox.color.color.data(), &config->visuals.onHitHitbox.color.color[3], nullptr, nullptr, &config->visuals.onHitHitbox.color.enabled);
    ImGui::SliderFloat("命中时间", &config->visuals.onHitHitbox.duration, 0.1f, 60.0f, "命中时间: % .2fs");
    ImGuiCustom::colorPicker("燃烧弹范围", config->visuals.molotovHull);
    ImGuiCustom::colorPicker("烟雾范围", config->visuals.smokeHull);
    ImGui::Checkbox("火焰标记", &config->visuals.molotovPolygon.enabled);
    ImGui::SameLine();
    if (ImGui::Button("...##molotov_polygon"))
        ImGui::OpenPopup("popup_molotovPolygon");

    if (ImGui::BeginPopup("popup_molotovPolygon"))
    {
        ImGuiCustom::colorPicker("自己", config->visuals.molotovPolygon.self.color.data(), &config->visuals.molotovPolygon.self.color[3], &config->visuals.molotovPolygon.self.rainbow, &config->visuals.molotovPolygon.self.rainbowSpeed, nullptr);
        ImGuiCustom::colorPicker("团队", config->visuals.molotovPolygon.team.color.data(), &config->visuals.molotovPolygon.team.color[3], &config->visuals.molotovPolygon.team.rainbow, &config->visuals.molotovPolygon.team.rainbowSpeed, nullptr);
        ImGuiCustom::colorPicker("敌人", config->visuals.molotovPolygon.enemy.color.data(), &config->visuals.molotovPolygon.enemy.color[3], &config->visuals.molotovPolygon.enemy.rainbow, &config->visuals.molotovPolygon.enemy.rainbowSpeed, nullptr);
        ImGui::EndPopup();
    }

   ImGui::Checkbox("烟雾弹计时器", &config->visuals.smokeTimer);
    ImGui::SameLine();
    if (ImGui::Button("...##smoke_timer"))
        ImGui::OpenPopup("popup_smokeTimer");

    if (ImGui::BeginPopup("popup_smokeTimer"))
    {
        ImGuiCustom::colorPicker("背景颜色", config->visuals.smokeTimerBG);
        ImGuiCustom::colorPicker("文本颜色", config->visuals.smokeTimerText);
        ImGuiCustom::colorPicker("计时器颜色", config->visuals.smokeTimerTimer);
        ImGui::EndPopup();
    }
    ImGui::Checkbox("燃烧弹计时器", &config->visuals.molotovTimer);
    ImGui::SameLine();
    if (ImGui::Button("...##molotov_timer"))
        ImGui::OpenPopup("popup_molotovTimer");

    if (ImGui::BeginPopup("popup_molotovTimer"))
    {
        ImGuiCustom::colorPicker("背景颜色", config->visuals.molotovTimerBG);
        ImGuiCustom::colorPicker("文本颜色", config->visuals.molotovTimerText);
        ImGuiCustom::colorPicker("计时器颜色", config->visuals.molotovTimerTimer);
        ImGui::EndPopup();
    }
    ImGuiCustom::colorPicker("弹道扩散范围", config->visuals.spreadCircle);

    ImGui::Checkbox("武器位置", &config->visuals.viewModel.enabled);
    ImGui::SameLine();

    if (bool ccPopup = ImGui::Button("编辑"))
        ImGui::OpenPopup("##viewmodel");

    if (ImGui::BeginPopup("##viewmodel"))
    {
        ImGui::PushItemWidth(290.0f);
        ImGui::SliderFloat("##x", &config->visuals.viewModel.x, -20.0f, 20.0f, "X: %.4f");
        ImGui::SliderFloat("##y", &config->visuals.viewModel.y, -20.0f, 20.0f, "Y: %.4f");
        ImGui::SliderFloat("##z", &config->visuals.viewModel.z, -20.0f, 20.0f, "Z: %.4f");
        ImGui::SliderInt("##fov", &config->visuals.viewModel.fov, -60, 60, "视觉范围: %d");
        ImGui::SliderFloat("##roll", &config->visuals.viewModel.roll, -90.0f, 90.0f, "视觉角度: %.2f");
        ImGui::PopItemWidth();
        ImGui::EndPopup();
    }

    ImGui::Columns(1);
}

void GUI::renderSkinChangerWindow() noexcept
{
    static auto itemIndex = 0;

    ImGui::PushItemWidth(110.0f);
    ImGui::Combo("##1", &itemIndex, [](void* data, int idx, const char** out_text) {
        *out_text = SkinChanger::weapon_names[idx].name;
        return true;
        }, nullptr, SkinChanger::weapon_names.size(), 5);
    ImGui::PopItemWidth();

    auto& selected_entry = config->skinChanger[itemIndex];
    selected_entry.itemIdIndex = itemIndex;

    constexpr auto rarityColor = [](int rarity) {
        constexpr auto rarityColors = std::to_array<ImU32>({
            IM_COL32(0,     0,   0,   0),
            IM_COL32(176, 195, 217, 255),
            IM_COL32( 94, 152, 217, 255),
            IM_COL32( 75, 105, 255, 255),
            IM_COL32(136,  71, 255, 255),
            IM_COL32(211,  44, 230, 255),
            IM_COL32(235,  75,  75, 255),
            IM_COL32(228, 174,  57, 255)
        });
        return rarityColors[static_cast<std::size_t>(rarity) < rarityColors.size() ? rarity : 0];
    };

    constexpr auto passesFilter = [](const std::wstring& str, std::wstring filter) {
        constexpr auto delimiter = L" ";
        wchar_t* _;
        wchar_t* token = std::wcstok(filter.data(), delimiter, &_);
        while (token) {
            if (!std::wcsstr(str.c_str(), token))
                return false;
            token = std::wcstok(nullptr, delimiter, &_);
        }
        return true;
    };

    {
        ImGui::SameLine();
        ImGui::Checkbox("开启", &selected_entry.enabled);
        ImGui::Separator();
        ImGui::Columns(2, nullptr, false);
        ImGui::InputInt("图案模板", &selected_entry.seed);
        ImGui::InputInt("StatTrak\u2122", &selected_entry.stat_trak);
        selected_entry.stat_trak = (std::max)(selected_entry.stat_trak, -1);
        ImGui::SliderFloat("磨损值", &selected_entry.wear, FLT_MIN, 1.f, "%.10f", ImGuiSliderFlags_Logarithmic);

        const auto& kits = itemIndex == 1 ? SkinChanger::getGloveKits() : SkinChanger::getSkinKits();

        if (ImGui::BeginCombo("皮肤涂装", kits[selected_entry.paint_kit_vector_index].name.c_str())) {
            ImGui::PushID("Paint Kit");
            ImGui::PushID("Search");
            ImGui::SetNextItemWidth(-1.0f);
            static std::array<std::string, SkinChanger::weapon_names.size()> filters;
            auto& filter = filters[itemIndex];
            ImGui::InputTextWithHint("", "搜索", &filter);
            if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                ImGui::SetKeyboardFocusHere(-1);
            ImGui::PopID();

            const std::wstring filterWide = Helpers::toUpper(Helpers::toWideString(filter));
            if (ImGui::BeginChild("##scrollarea", { 0, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
                for (std::size_t i = 0; i < kits.size(); ++i) {
                    if (filter.empty() || passesFilter(kits[i].nameUpperCase, filterWide)) {
                        ImGui::PushID(i);
                        const auto selected = i == selected_entry.paint_kit_vector_index;
                        if (ImGui::SelectableWithBullet(kits[i].name.c_str(), rarityColor(kits[i].rarity), selected)) {
                            selected_entry.paint_kit_vector_index = i;
                            ImGui::CloseCurrentPopup();
                        }

                        if (ImGui::IsItemHovered()) {
                            if (const auto icon = SkinChanger::getItemIconTexture(kits[i].iconPath)) {
                                ImGui::BeginTooltip();
                                ImGui::Image(icon, { 200.0f, 150.0f });
                                ImGui::EndTooltip();
                            }
                        }
                        if (selected && ImGui::IsWindowAppearing())
                            ImGui::SetScrollHereY();
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
            ImGui::PopID();
            ImGui::EndCombo();
        }

        ImGui::Combo("品质", &selected_entry.entity_quality_vector_index, [](void* data, int idx, const char** out_text) {
            *out_text = SkinChanger::getQualities()[idx].name.c_str(); // safe within this lamba
            return true;
            }, nullptr, SkinChanger::getQualities().size(), 5);

        if (itemIndex == 0) {
            ImGui::Combo("刀", &selected_entry.definition_override_vector_index, [](void* data, int idx, const char** out_text) {
                *out_text = SkinChanger::getKnifeTypes()[idx].name.c_str();
                return true;
                }, nullptr, SkinChanger::getKnifeTypes().size(), 5);
        } else if (itemIndex == 1) {
            ImGui::Combo("手套", &selected_entry.definition_override_vector_index, [](void* data, int idx, const char** out_text) {
                *out_text = SkinChanger::getGloveTypes()[idx].name.c_str();
                return true;
                }, nullptr, SkinChanger::getGloveTypes().size(), 5);
        } else {
            static auto unused_value = 0;
            selected_entry.definition_override_vector_index = 0;
            ImGui::Combo("不可用", &unused_value, "仅用于刀或手套\0");
        }

        ImGui::InputText("名称标签", selected_entry.custom_name, 32);
    }

    ImGui::NextColumn();

    {
        ImGui::PushID("sticker");

        static std::size_t selectedStickerSlot = 0;

        ImGui::PushItemWidth(-1);
        ImVec2 size;
        size.x = 0.0f;
        size.y = ImGui::GetTextLineHeightWithSpacing() * 5.25f + ImGui::GetStyle().FramePadding.y * 2.0f;

        if (ImGui::BeginListBox("", size)) {
            for (int i = 0; i < 5; ++i) {
                ImGui::PushID(i);

                const auto kit_vector_index = config->skinChanger[itemIndex].stickers[i].kit_vector_index;
                const std::string text = '#' + std::to_string(i + 1) + "  " + SkinChanger::getStickerKits()[kit_vector_index].name;

                if (ImGui::Selectable(text.c_str(), i == selectedStickerSlot))
                    selectedStickerSlot = i;

                ImGui::PopID();
            }
            ImGui::EndListBox();
        }

        ImGui::PopItemWidth();

        auto& selected_sticker = selected_entry.stickers[selectedStickerSlot];

        const auto& kits = SkinChanger::getStickerKits();
        if (ImGui::BeginCombo("印花", kits[selected_sticker.kit_vector_index].name.c_str())) {
            ImGui::PushID("Sticker");
            ImGui::PushID("Search");
            ImGui::SetNextItemWidth(-1.0f);
            static std::array<std::string, SkinChanger::weapon_names.size()> filters;
            auto& filter = filters[itemIndex];
            ImGui::InputTextWithHint("", "搜索", &filter);
            if (ImGui::IsItemHovered() || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                ImGui::SetKeyboardFocusHere(-1);
            ImGui::PopID();

            const std::wstring filterWide = Helpers::toUpper(Helpers::toWideString(filter));
            if (ImGui::BeginChild("##scrollarea", { 0, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
                for (std::size_t i = 0; i < kits.size(); ++i) {
                    if (filter.empty() || passesFilter(kits[i].nameUpperCase, filterWide)) {
                        ImGui::PushID(i);
                        const auto selected = i == selected_sticker.kit_vector_index;
                        if (ImGui::SelectableWithBullet(kits[i].name.c_str(), rarityColor(kits[i].rarity), selected)) {
                            selected_sticker.kit_vector_index = i;
                            ImGui::CloseCurrentPopup();
                        }
                        if (ImGui::IsItemHovered()) {
                            if (const auto icon = SkinChanger::getItemIconTexture(kits[i].iconPath)) {
                                ImGui::BeginTooltip();
                                ImGui::Image(icon, { 200.0f, 150.0f });
                                ImGui::EndTooltip();
                            }
                        }
                        if (selected && ImGui::IsWindowAppearing())
                            ImGui::SetScrollHereY();
                        ImGui::PopID();
                    }
                }
            }
            ImGui::EndChild();
            ImGui::PopID();
            ImGui::EndCombo();
        }

        ImGui::SliderFloat("印花磨损", &selected_sticker.wear, FLT_MIN, 1.0f, "%.10f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("印花大小", &selected_sticker.scale, 0.1f, 5.0f);
        ImGui::SliderFloat("印花旋转角度", &selected_sticker.rotation, 0.0f, 360.0f);

        ImGui::PopID();
    }
    selected_entry.update();

    ImGui::Columns(1);

    ImGui::Separator();

    if (ImGui::Button("刷新皮肤", { 130.0f, 30.0f }))
        SkinChanger::scheduleHudUpdate();
}

void GUI::renderMiscWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 230.0f);
    hotkey3("菜单键", config->misc.menuKey);
    ImGui::Checkbox("挂机防踢", &config->misc.antiAfkKick);
    ImGui::Checkbox("广告拦截", &config->misc.adBlock);
    ImGui::Combo("国家修改", &config->misc.forceRelayCluster, "无\0澳大利亚\0奥地利\0巴西\0智利\0迪拜\0法国\0德国\0香港\0印度 (钦奈)\0印度 (孟买)\0日本\0卢森堡\0荷兰\0秘鲁\0菲律宾\0波兰\0新加坡\0南非\0西班牙\0瑞典\0英国\0美国 (亚特兰大)\0美国 (西雅图)\0美国 (芝加哥)\0美国 (洛杉矶)\0美国 (摩西湖)\0美国 (奥克拉荷马)\0美国 (西雅图)\0美国 (华盛顿特区)\0");
    ImGui::Checkbox("自动转向", &config->misc.autoStrafe);
    ImGui::Checkbox("连跳", &config->misc.bunnyHop);
    ImGui::Checkbox("快速蹲起", &config->misc.fastDuck);
    ImGui::Checkbox("滑步", &config->misc.moonwalk);
    ImGui::Checkbox("自动刀", &config->misc.knifeBot);
    ImGui::SameLine();
    ImGui::Combo("模式", &config->misc.knifeBotMode, "普通模式\0愤怒模式\0");

    ImGui::Checkbox("自动战术", &config->misc.blockBot);
    ImGui::SameLine();
    ImGui::PushID("Block bot Key");
    ImGui::hotkey2("", config->misc.blockBotKey);
    ImGui::PopID();

    ImGui::Checkbox("边缘跳跃", &config->misc.edgeJump);
    ImGui::SameLine();
    ImGui::PushID("Edge Jump Key");
    ImGui::hotkey2("", config->misc.edgeJumpKey);
    ImGui::PopID();

    ImGui::Checkbox("自动蹲跳", &config->misc.miniJump);
    ImGui::SameLine();
    ImGui::PushID("Mini jump Key");
    ImGui::hotkey2("", config->misc.miniJumpKey);
    ImGui::PopID();
    if (config->misc.miniJump) {
        ImGui::SliderInt("下蹲幅度", &config->misc.miniJumpCrouchLock, 0, 12, "%d 幅度");
    }

    ImGui::Checkbox("跳跃Bug", &config->misc.jumpBug);
    ImGui::SameLine();
    ImGui::PushID("Jump Bug Key");
    ImGui::hotkey2("", config->misc.jumpBugKey);
    ImGui::PopID();

    ImGui::Checkbox("边缘Bug", &config->misc.edgeBug);
    ImGui::SameLine();
    ImGui::PushID("Edge Bug Key");
    ImGui::hotkey2("", config->misc.edgeBugKey);
    ImGui::PopID();
    if (config->misc.edgeBug) {
        ImGui::SliderInt("调节", &config->misc.edgeBugPredAmnt, 0, 128, "%d 幅度");
    }

    ImGui::Checkbox("自动像素冲浪", &config->misc.autoPixelSurf);
    ImGui::SameLine();
    ImGui::PushID("Auto pixel surf Key");
    ImGui::hotkey2("", config->misc.autoPixelSurfKey);
    ImGui::PopID();
    if (config->misc.autoPixelSurf) {
        ImGui::SliderInt("预言调节", &config->misc.autoPixelSurfPredAmnt, 2, 4, "%d 幅度");
    }

    ImGui::Checkbox("移速显示", &config->misc.velocity.enabled);
    ImGui::SameLine();

    ImGui::PushID("Draw velocity");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::SliderFloat("移速显示位置", &config->misc.velocity.position, 0.0f, 1.0f);
        ImGui::SliderFloat("透明度", &config->misc.velocity.alpha, 0.0f, 1.0f);
        ImGuiCustom::colorPicker("字体颜色", config->misc.velocity.color.color.data(), nullptr, &config->misc.velocity.color.rainbow, &config->misc.velocity.color.rainbowSpeed, &config->misc.velocity.color.enabled);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("键盘显示", &config->misc.keyBoardDisplay.enabled);
    ImGui::SameLine();

    ImGui::PushID("Keyboard display");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::SliderFloat("按键显示位置", &config->misc.keyBoardDisplay.position, 0.0f, 1.0f);
        ImGui::Checkbox("显示关键方框", &config->misc.keyBoardDisplay.showKeyTiles);
        ImGuiCustom::colorPicker("颜色", config->misc.keyBoardDisplay.color);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("慢走", &config->misc.slowwalk);
    ImGui::SameLine();
    ImGui::PushID("Slowwalk Key");
    ImGui::hotkey2("", config->misc.slowwalkKey);
    ImGui::PopID();
    if (config->misc.slowwalk) {
        ImGui::SliderInt("慢走幅度", &config->misc.slowwalkAmnt, 0, 50, config->misc.slowwalkAmnt ? "%d u/s" : "无");
    }
    ImGui::Checkbox("持续蹲起", &config->misc.fakeduck);
    ImGui::SameLine();
    ImGui::PushID("Fakeduck Key");
    ImGui::hotkey2("", config->misc.fakeduckKey);
    ImGui::PopID();
    ImGuiCustom::colorPicker("自动Peek", config->misc.autoPeek.color.data(), &config->misc.autoPeek.color[3], &config->misc.autoPeek.rainbow, &config->misc.autoPeek.rainbowSpeed, &config->misc.autoPeek.enabled);
    ImGui::SameLine();
    ImGui::PushID("Auto peek Key");
    ImGui::hotkey2("", config->misc.autoPeekKey);
    ImGui::PopID();
    ImGuiCustom::colorPicker("盲狙十字准线", config->misc.noscopeCrosshair);
    ImGuiCustom::colorPicker("压枪十字线", config->misc.recoilCrosshair);
    ImGui::Checkbox("手枪连发", &config->misc.autoPistol);
    ImGui::Checkbox("自动换弹", &config->misc.autoReload);
    ImGui::Checkbox("自动接受", &config->misc.autoAccept);
    ImGui::Checkbox("显示雷达", &config->misc.radarHack);
    ImGui::Checkbox("显示段位", &config->misc.revealRanks);
    ImGui::Checkbox("显示金钱", &config->misc.revealMoney);
    ImGui::Checkbox("嫌疑人名单", &config->misc.revealSuspect);
    ImGui::Checkbox("显示选票", &config->misc.revealVotes);

    ImGui::Checkbox("观战者列表", &config->misc.spectatorList.enabled);
    ImGui::SameLine();

    ImGui::PushID("Spectator list");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("无边框", &config->misc.spectatorList.noTitleBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("密钥绑定列表", &config->misc.keybindList.enabled);
    ImGui::SameLine();

    ImGui::PushID("Keybinds list");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("无边框", &config->misc.keybindList.noTitleBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::PushID("Player List");
    ImGui::Checkbox("本地玩家列表", &config->misc.playerList.enabled);
    ImGui::SameLine();

    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("Steam ID", &config->misc.playerList.steamID);
        ImGui::Checkbox("段位", &config->misc.playerList.rank);
        ImGui::Checkbox("胜场", &config->misc.playerList.wins);
        ImGui::Checkbox("生命值", &config->misc.playerList.health);
        ImGui::Checkbox("护甲值", &config->misc.playerList.armor);
        ImGui::Checkbox("金钱", &config->misc.playerList.money);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::PushID("Jump stats");
    ImGui::Checkbox("跳跃信息统计器", &config->misc.jumpStats.enabled);
    ImGui::SameLine();

    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("显示失败", &config->misc.jumpStats.showFails);
        ImGui::Checkbox("显示颜色失败", &config->misc.jumpStats.showColorOnFail);
        ImGui::Checkbox("简化命名", &config->misc.jumpStats.simplifyNaming);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("显示延迟", &config->misc.watermark.enabled);
    ImGuiCustom::colorPicker("屏幕外的敌人", config->misc.offscreenEnemies, &config->misc.offscreenEnemies.enabled);
    ImGui::SameLine();
    ImGui::PushID("Offscreen Enemies");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("基于生命值", &config->misc.offscreenEnemies.healthBar.enabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(95.0f);
        ImGui::Combo("类型", &config->misc.offscreenEnemies.healthBar.type, "渐变\0立体\0基于血量\0");
        if (config->misc.offscreenEnemies.healthBar.type == HealthBar::Solid) {
            ImGui::SameLine();
            ImGuiCustom::colorPicker("", static_cast<Color4&>(config->misc.offscreenEnemies.healthBar));
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
    ImGui::Checkbox("禁用模型遮挡", &config->misc.disableModelOcclusion);
    ImGui::SliderFloat("纵横比", &config->misc.aspectratio, 0.0f, 5.0f, "%.2f");
    ImGui::NextColumn();
    ImGui::Checkbox("禁用HUD模糊", &config->misc.disablePanoramablur);
    ImGui::Checkbox("动画氏族标记", &config->misc.animatedClanTag);
    ImGui::Checkbox("时钟标签", &config->misc.clocktag);
    ImGui::Checkbox("自定义组名", &config->misc.customClanTag);
    ImGui::SameLine();
    ImGui::PushItemWidth(120.0f);
    ImGui::PushID(0);

    if (ImGui::InputText("", config->misc.clanTag, sizeof(config->misc.clanTag)))
        Misc::updateClanTag(true);
    ImGui::PopID();

    ImGui::Checkbox("自定义名称", &config->misc.customName);
    ImGui::SameLine();
    ImGui::PushItemWidth(120.0f);
    ImGui::PushID("Custom name change");

    if (ImGui::InputText("", config->misc.name, sizeof(config->misc.name)) && config->misc.customName)
        Misc::changeName(false, (std::string{ config->misc.name } + '\x1').c_str(), 0.0f);
    ImGui::PopID();

    ImGui::Checkbox("击杀信息", &config->misc.killMessage);
    ImGui::SameLine();
    ImGui::PushItemWidth(120.0f);
    ImGui::PushID(1);
    ImGui::InputText("", &config->misc.killMessageString);
    ImGui::PopID();
    ImGui::Checkbox("窃取名称", &config->misc.nameStealer);
    ImGui::Checkbox("快速安放炸弹", &config->misc.fastPlant);
    ImGui::Checkbox("快速急停", &config->misc.fastStop);
    ImGuiCustom::colorPicker("炸弹计时器", config->misc.bombTimer);
    ImGuiCustom::colorPicker("伤害指示器", config->misc.hurtIndicator);
    ImGui::Checkbox("自动捏左轮", &config->misc.prepareRevolver);
    ImGui::SameLine();
    ImGui::PushID("Prepare revolver Key");
    ImGui::hotkey2("", config->misc.prepareRevolverKey);
    ImGui::PopID();
    ImGui::Combo("击中音效", &config->misc.hitSound, "无\0金属\0游戏感\0钟\0玻璃\0自定义\0");
    if (config->misc.hitSound == 5) {
        ImGui::InputText("击中音效文件名", &config->misc.customHitSound);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("音频文件必须放在csgo/sound/目录中");
    }
    ImGui::PushID(2);
    ImGui::Combo("击杀音效", &config->misc.killSound, "无\0金属\0游戏感\0钟\0玻璃\0自定义\0");
    if (config->misc.killSound == 5) {
        ImGui::InputText("击杀音效文件名", &config->misc.customKillSound);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("音频文件必须放在csgo/sound/目录中");
    }
    ImGui::PopID();
    /*
    ImGui::Text("Quick healthshot");
    ImGui::SameLine();
    hotkey(config->misc.quickHealthshotKey);
    */
    ImGui::Checkbox("投掷物预测", &config->misc.nadePredict);
    ImGui::SameLine();
    ImGui::PushID("Grenade Prediction");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGuiCustom::colorPicker("爆炸伤害预测", config->misc.nadeDamagePredict);
        ImGuiCustom::colorPicker("轨迹", config->misc.nadeTrailPredict);
        ImGuiCustom::colorPicker("范围", config->misc.nadeCirclePredict);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("特训助手预测", &config->misc.fixTabletSignal);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("最大视角变化", &config->misc.maxAngleDelta, 0.0f, 255.0f, "%.2f");
    ImGui::Checkbox("刀枪反手", &config->misc.oppositeHandKnife);
    ImGui::Checkbox("反VAC系统", &config->misc.svPureBypass);
    ImGui::Checkbox("解锁库存访问", &config->misc.inventoryUnlocker);
    ImGui::Checkbox("解锁隐藏控制台指令", &config->misc.unhideConvars);
    ImGui::Checkbox("保留击杀", &config->misc.preserveKillfeed.enabled);
    ImGui::SameLine();

    ImGui::PushID("Preserve Killfeed");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("仅保留爆头", &config->misc.preserveKillfeed.onlyHeadshots);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("击杀信息修改器", &config->misc.killfeedChanger.enabled);
    ImGui::SameLine();

    ImGui::PushID("Killfeed changer");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::Checkbox("爆头击杀", &config->misc.killfeedChanger.headshot);
        ImGui::Checkbox("控制击杀", &config->misc.killfeedChanger.dominated);
        ImGui::SameLine();
        ImGui::Checkbox("复仇击杀", &config->misc.killfeedChanger.revenge);
        ImGui::Checkbox("穿透击杀", &config->misc.killfeedChanger.penetrated);
        ImGui::Checkbox("盲锯击杀", &config->misc.killfeedChanger.noscope);
        ImGui::Checkbox("穿烟击杀", &config->misc.killfeedChanger.thrusmoke);
        ImGui::Checkbox("致盲击杀", &config->misc.killfeedChanger.attackerblind);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("购买记录", &config->misc.purchaseList.enabled);
    ImGui::SameLine();

    ImGui::PushID("Purchase List");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::SetNextItemWidth(75.0f);
        ImGui::Combo("模式", &config->misc.purchaseList.mode, "详细\0总共\0");
        ImGui::Checkbox("仅在冻结期间", &config->misc.purchaseList.onlyDuringFreezeTime);
        ImGui::Checkbox("显示价格", &config->misc.purchaseList.showPrices);
        ImGui::Checkbox("无边框", &config->misc.purchaseList.noTitleBar);
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("自动举报", &config->misc.reportbot.enabled);
    ImGui::SameLine();
    ImGui::PushID("Reportbot");

    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {
        ImGui::PushItemWidth(80.0f);
        ImGui::Combo("目标", &config->misc.reportbot.target, "敌人\0队友\0全部\0");
        ImGui::InputInt("延迟 (s)", &config->misc.reportbot.delay);
        config->misc.reportbot.delay = (std::max)(config->misc.reportbot.delay, 1);
        ImGui::InputInt("次数", &config->misc.reportbot.rounds);
        config->misc.reportbot.rounds = (std::max)(config->misc.reportbot.rounds, 1);
        ImGui::PopItemWidth();
        ImGui::Checkbox("滥用通信", &config->misc.reportbot.textAbuse);
        ImGui::Checkbox("骚扰", &config->misc.reportbot.griefing);
        ImGui::Checkbox("穿墙作弊", &config->misc.reportbot.wallhack);
        ImGui::Checkbox("自瞄作弊", &config->misc.reportbot.aimbot);
        ImGui::Checkbox("其他作弊", &config->misc.reportbot.other);
        if (ImGui::Button("重置"))
            Misc::resetReportbot();
        ImGui::EndPopup();
    }
    ImGui::PopID();

    ImGui::Checkbox("自动购买", &config->misc.autoBuy.enabled);
    ImGui::SameLine();

    ImGui::PushID("Autobuy");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) 
    {
        ImGui::Combo("主武器", &config->misc.autoBuy.primaryWeapon, "无\0MAC-10 | MP9\0MP7 | MP5-SD\0UMP-45\0P90\0PP野牛\0加利尔AR | 法玛斯\0AK-47 | M4A4 | M4A1-S\0SSG 08\0SG553 |AUG\0AWP\0G3SG1 | SCAR-20\0新星\0XM1014\0锯短型散弹 | MAG-7\0M249\0内格夫\0");
        ImGui::Combo("手枪", &config->misc.autoBuy.secondaryWeapon, "无\0格洛克18 | P2000 | USP-S\0双持贝雷塔\0P250\0CZ75 | FN57 | Tec-9\0沙漠之鹰 | R8左轮\0");
        ImGui::Combo("护甲", &config->misc.autoBuy.armor, "无\0护甲\0头盔 + 护甲\0");

        static bool utilities[2]{ false, false };
        static const char* utility[]{ "盾牌","电击枪" };
        static std::string previewvalueutility = "";
        for (size_t i = 0; i < ARRAYSIZE(utilities); i++)
        {
            utilities[i] = (config->misc.autoBuy.utility & 1 << i) == 1 << i;
        }
        if (ImGui::BeginCombo("装备", previewvalueutility.c_str()))
        {
            previewvalueutility = "";
            for (size_t i = 0; i < ARRAYSIZE(utilities); i++)
            {
                ImGui::Selectable(utility[i], &utilities[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
            }
            ImGui::EndCombo();
        }
        for (size_t i = 0; i < ARRAYSIZE(utilities); i++)
        {
            if (i == 0)
                previewvalueutility = "";

            if (utilities[i])
            {
                previewvalueutility += previewvalueutility.size() ? std::string(", ") + utility[i] : utility[i];
                config->misc.autoBuy.utility |= 1 << i;
            }
            else
            {
                config->misc.autoBuy.utility &= ~(1 << i);
            }
        }

        static bool nading[5]{ false, false, false, false, false };
        static const char* nades[]{ "手雷","烟雾弹","燃烧瓶","闪光弹","诱饵弹" };
        static std::string previewvaluenades = "";
        for (size_t i = 0; i < ARRAYSIZE(nading); i++)
        {
            nading[i] = (config->misc.autoBuy.grenades & 1 << i) == 1 << i;
        }
        if (ImGui::BeginCombo("投掷物", previewvaluenades.c_str()))
        {
            previewvaluenades = "";
            for (size_t i = 0; i < ARRAYSIZE(nading); i++)
            {
                ImGui::Selectable(nades[i], &nading[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
            }
            ImGui::EndCombo();
        }
        for (size_t i = 0; i < ARRAYSIZE(nading); i++)
        {
            if (i == 0)
                previewvaluenades = "";

            if (nading[i])
            {
                previewvaluenades += previewvaluenades.size() ? std::string(", ") + nades[i] : nades[i];
                config->misc.autoBuy.grenades |= 1 << i;
            }
            else
            {
                config->misc.autoBuy.grenades &= ~(1 << i);
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();


    ImGuiCustom::colorPicker("记录仪", config->misc.logger);
    ImGui::SameLine();

    ImGui::PushID("Logger");
    if (ImGui::Button("..."))
        ImGui::OpenPopup("");

    if (ImGui::BeginPopup("")) {

        static bool modes[2]{ false, false };
        static const char* mode[]{ "控制台", "事件日志" };
        static std::string previewvaluemode = "";
        for (size_t i = 0; i < ARRAYSIZE(modes); i++)
        {
            modes[i] = (config->misc.loggerOptions.modes & 1 << i) == 1 << i;
        }
        if (ImGui::BeginCombo("日志输出", previewvaluemode.c_str()))
        {
            previewvaluemode = "";
            for (size_t i = 0; i < ARRAYSIZE(modes); i++)
            {
                ImGui::Selectable(mode[i], &modes[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
            }
            ImGui::EndCombo();
        }
        for (size_t i = 0; i < ARRAYSIZE(modes); i++)
        {
            if (i == 0)
                previewvaluemode = "";

            if (modes[i])
            {
                previewvaluemode += previewvaluemode.size() ? std::string(", ") + mode[i] : mode[i];
                config->misc.loggerOptions.modes |= 1 << i;
            }
            else
            {
                config->misc.loggerOptions.modes &= ~(1 << i);
            }
        }

        static bool events[4]{ false, false, false, false };
        static const char* event[]{ "造成伤害", "受到伤害", "劫持人质", "爆炸伤害" };
        static std::string previewvalueevent = "";
        for (size_t i = 0; i < ARRAYSIZE(events); i++)
        {
            events[i] = (config->misc.loggerOptions.events & 1 << i) == 1 << i;
        }
        if (ImGui::BeginCombo("日志事件", previewvalueevent.c_str()))
        {
            previewvalueevent = "";
            for (size_t i = 0; i < ARRAYSIZE(events); i++)
            {
                ImGui::Selectable(event[i], &events[i], ImGuiSelectableFlags_::ImGuiSelectableFlags_DontClosePopups);
            }
            ImGui::EndCombo();
        }
        for (size_t i = 0; i < ARRAYSIZE(events); i++)
        {
            if (i == 0)
                previewvalueevent = "";

            if (events[i])
            {
                previewvalueevent += previewvalueevent.size() ? std::string(", ") + event[i] : event[i];
                config->misc.loggerOptions.events |= 1 << i;
            }
            else
            {
                config->misc.loggerOptions.events &= ~(1 << i);
            }
        }

        ImGui::EndPopup();
    }
    ImGui::PopID();

    if (ImGui::Button("卸载辅助"))
        hooks->uninstall();

    ImGui::Columns(1);
}

void GUI::renderConfigWindow() noexcept
{
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnOffset(1, 170.0f);

    static bool incrementalLoad = false;
    ImGui::Checkbox("叠加加载参数", &incrementalLoad);

    ImGui::PushItemWidth(160.0f);

    auto& configItems = config->getConfigs();
    static int currentConfig = -1;

    static std::string buffer;

    timeToNextConfigRefresh -= ImGui::GetIO().DeltaTime;
    if (timeToNextConfigRefresh <= 0.0f) {
        config->listConfigs();
        if (const auto it = std::find(configItems.begin(), configItems.end(), buffer); it != configItems.end())
            currentConfig = std::distance(configItems.begin(), it);
        timeToNextConfigRefresh = 0.1f;
    }

    if (static_cast<std::size_t>(currentConfig) >= configItems.size())
        currentConfig = -1;

    if (ImGui::ListBox("", &currentConfig, [](void* data, int idx, const char** out_text) {
        auto& vector = *static_cast<std::vector<std::string>*>(data);
        *out_text = vector[idx].c_str();
        return true;
        }, &configItems, configItems.size(), 5) && currentConfig != -1)
            buffer = configItems[currentConfig];

        ImGui::PushID(0);
        if (ImGui::InputTextWithHint("", "参数名称", &buffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (currentConfig != -1)
                config->rename(currentConfig, buffer.c_str());
        }
        ImGui::PopID();
        ImGui::NextColumn();

        ImGui::PushItemWidth(100.0f);

        if (ImGui::Button("打开参数目录"))
            config->openConfigDir();

        if (ImGui::Button("创建参数", { 100.0f, 25.0f }))
            config->add(buffer.c_str());

        if (ImGui::Button("重置参数", { 100.0f, 25.0f }))
            ImGui::OpenPopup("要重置的参数");

        if (ImGui::BeginPopup("Config to reset")) {
            static constexpr const char* names[]{ "全部", "演技选项", "演技反自瞄", "暴力选项", "暴力反自瞄", "假角度", "假卡", "回溯", "自动扳机", "发光", "上色", "透视", "视觉选项", "皮肤修改器", "声音选项", "杂项" };
            for (int i = 0; i < IM_ARRAYSIZE(names); i++) {
                if (i == 1) ImGui::Separator();

                if (ImGui::Selectable(names[i])) {
                    switch (i) {
                    case 0: config->reset(); Misc::updateClanTag(true); SkinChanger::scheduleHudUpdate(); break;
                    case 1: config->legitbot = { }; config->legitbotKey.reset(); break;
                    case 2: config->legitAntiAim = { }; break;
                    case 3: config->ragebot = { }; config->ragebotKey.reset();  break;
                    case 4: config->rageAntiAim = { };  break;
                    case 5: config->fakeAngle = { }; break;
                    case 6: config->fakelag = { }; break;
                    case 7: config->backtrack = { }; break;
                    case 8: config->triggerbot = { }; config->triggerbotKey.reset(); break;
                    case 9: Glow::resetConfig(); break;
                    case 10: config->chams = { }; config->chamsKey.reset(); break;
                    case 11: config->streamProofESP = { }; break;
                    case 12: config->visuals = { }; break;
                    case 13: config->skinChanger = { }; SkinChanger::scheduleHudUpdate(); break;
                    case 14: Sound::resetConfig(); break;
                    case 15: config->misc = { };  Misc::updateClanTag(true); break;
                    }
                }
            }
            ImGui::EndPopup();
        }
        if (currentConfig != -1) {
            if (ImGui::Button("加载参数", { 100.0f, 25.0f })) {
                config->load(currentConfig, incrementalLoad);
                SkinChanger::scheduleHudUpdate();
                Misc::updateClanTag(true);
            }
            if (ImGui::Button("保存参数", { 100.0f, 25.0f }))
                ImGui::OpenPopup("##reallySave");
            if (ImGui::BeginPopup("##reallySave"))
            {
                ImGui::TextUnformatted("是否保存?");
                if (ImGui::Button("取消", { 45.0f, 0.0f }))
                    ImGui::CloseCurrentPopup();
                ImGui::SameLine();
                if (ImGui::Button("确定保存", { 45.0f, 0.0f }))
                {
                    config->save(currentConfig);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (ImGui::Button("删除参数", { 100.0f, 25.0f }))
                ImGui::OpenPopup("##reallyDelete");
            if (ImGui::BeginPopup("##reallyDelete"))
            {
                ImGui::TextUnformatted("是否删除?");
                if (ImGui::Button("取消", { 45.0f, 0.0f }))
                    ImGui::CloseCurrentPopup();
                ImGui::SameLine();
                if (ImGui::Button("确定", { 45.0f, 0.0f }))
                {
                    config->remove(currentConfig);
                    if (static_cast<std::size_t>(currentConfig) < configItems.size())
                        buffer = configItems[currentConfig];
                    else
                        buffer.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::Columns(1);
}

void Active() { ImGuiStyle* Style = &ImGui::GetStyle(); Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 34); Style->Colors[ImGuiCol_ButtonActive] = ImColor(25, 30, 34); Style->Colors[ImGuiCol_ButtonHovered] = ImColor(25, 30, 34); }
void Hovered() { ImGuiStyle* Style = &ImGui::GetStyle(); Style->Colors[ImGuiCol_Button] = ImColor(19, 22, 27); Style->Colors[ImGuiCol_ButtonActive] = ImColor(19, 22, 27); Style->Colors[ImGuiCol_ButtonHovered] = ImColor(19, 22, 27); }



void GUI::renderGuiStyle() noexcept
{
    ImGuiStyle* Style = &ImGui::GetStyle();
    Style->WindowRounding = 5.5;
    Style->WindowBorderSize = 2.5;
    Style->ChildRounding = 5.5;
    Style->FrameBorderSize = 2.5;
    Style->Colors[ImGuiCol_WindowBg] = ImColor(0, 0, 0, 0);
    Style->Colors[ImGuiCol_ChildBg] = ImColor(31, 31 ,31);
    Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 34);
    Style->Colors[ImGuiCol_ButtonHovered] = ImColor(25, 30, 34);
    Style->Colors[ImGuiCol_ButtonActive] = ImColor(19, 22, 27);

    Style->Colors[ImGuiCol_ScrollbarGrab] = ImColor(25, 30, 34);
    Style->Colors[ImGuiCol_ScrollbarGrabActive] = ImColor(25, 30, 34);
    Style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(25, 30, 34);

    static auto Name = "Menu";
    static auto Flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;

    static int activeTab = 1;
    static int activeSubTabLegitbot = 1;
    static int activeSubTabRagebot = 1;
    static int activeSubTabVisuals = 1;
    static int activeSubTabMisc = 1;
    static int activeSubTabConfigs = 1;

    if (ImGui::Begin(Name, NULL, Flags))
    {
        Style->Colors[ImGuiCol_ChildBg] = ImColor(25, 30, 34);

        ImGui::BeginChild("##Back", ImVec2{ 704, 434 }, false);
        {
            ImGui::SetCursorPos(ImVec2{ 2, 2 });

            Style->Colors[ImGuiCol_ChildBg] = ImColor(19, 22, 27);

            ImGui::BeginChild("##Main", ImVec2{ 700, 430 }, false);
            {
                ImGui::BeginChild("##UP", ImVec2{ 700, 45 }, false);
                {
                    ImGui::SetCursorPos(ImVec2{ 10, 6 });
                    ImGui::PushFont(fonts.tahoma34); ImGui::Text("必胜客BSK"); ImGui::PopFont();

                    float pos = 305;
                    ImGui::SetCursorPos(ImVec2{ pos, 0 });
                    if (activeTab == 1) Active(); else Hovered();
                    if (ImGui::Button("演技选项", ImVec2{ 75, 45 }))
                        activeTab = 1;
                    
                    pos += 80;

                    ImGui::SetCursorPos(ImVec2{ pos, 0 });
                    if (activeTab == 2) Active(); else Hovered();
                    if (ImGui::Button("暴力选项", ImVec2{ 75, 45 }))
                        activeTab = 2;

                    pos += 80;

                    ImGui::SetCursorPos(ImVec2{ pos, 0 });
                    if (activeTab == 3) Active(); else Hovered();
                    if (ImGui::Button("视觉选项", ImVec2{ 75, 45 }))
                        activeTab = 3;

                    pos += 80;

                    ImGui::SetCursorPos(ImVec2{ pos, 0 });
                    if (activeTab == 4) Active(); else Hovered();
                    if (ImGui::Button("杂项", ImVec2{ 75, 45 }))
                        activeTab = 4;

                    pos += 80;

                    ImGui::SetCursorPos(ImVec2{ pos, 0 });
                    if (activeTab == 5) Active(); else Hovered();
                    if (ImGui::Button("配置选项", ImVec2{ 75, 45 }))
                        activeTab = 5;
                }
                ImGui::EndChild();

                ImGui::SetCursorPos(ImVec2{ 0, 45 });
                Style->Colors[ImGuiCol_ChildBg] = ImColor(25, 30, 34);
                Style->Colors[ImGuiCol_Button] = ImColor(25, 30, 34);
                Style->Colors[ImGuiCol_ButtonHovered] = ImColor(25, 30, 34);
                Style->Colors[ImGuiCol_ButtonActive] = ImColor(19, 22, 27);
                ImGui::BeginChild("##Childs", ImVec2{ 700, 365 }, false);
                {
                    ImGui::SetCursorPos(ImVec2{ 15, 5 });
                    Style->ChildRounding = 0;
                    ImGui::BeginChild("##Left", ImVec2{ 155, 320 }, false);
                    {
                        switch (activeTab)
                        {
                        case 1: //演技选项
                            ImGui::SetCursorPosY(10);
                            if (ImGui::Button("自瞄                    ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 1;
                            if (ImGui::Button("回溯               ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 2;
                            if (ImGui::Button("自动扳机              ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 3;
                            if (ImGui::Button("反自瞄                 ", ImVec2{ 80, 20 })) activeSubTabLegitbot = 4;
                            break;
                        case 2: //暴力选项
                            ImGui::SetCursorPosY(10);
                            if (ImGui::Button("自瞄                    ", ImVec2{ 80, 20 })) activeSubTabRagebot = 1;
                            if (ImGui::Button("回溯               ", ImVec2{ 80, 20 })) activeSubTabRagebot = 2;
                            if (ImGui::Button("反自瞄                 ", ImVec2{ 80, 20 })) activeSubTabRagebot = 3;
                            if (ImGui::Button("假角度              ", ImVec2{ 80, 20 })) activeSubTabRagebot = 4;
                            if (ImGui::Button("假卡                 ", ImVec2{ 80, 20 })) activeSubTabRagebot = 5;
                            break;
                        case 3: //视觉选项
                            ImGui::SetCursorPosY(10);
                            if (ImGui::Button("视觉杂项                    ", ImVec2{ 80, 20 })) activeSubTabVisuals = 1;
                            if (ImGui::Button("透视选项                     ", ImVec2{ 80, 20 })) activeSubTabVisuals = 2;
                            if (ImGui::Button("上色功能                   ", ImVec2{ 80, 20 })) activeSubTabVisuals = 3;
                            if (ImGui::Button("发光功能                    ", ImVec2{ 80, 20 })) activeSubTabVisuals = 4;
                            if (ImGui::Button("皮肤修改器                   ", ImVec2{ 80, 20 })) activeSubTabVisuals = 5;
                            break;
                        case 4: //杂项
                            ImGui::SetCursorPosY(10);
                            if (ImGui::Button("功能                    ", ImVec2{ 80, 20 })) activeSubTabMisc = 1;
                            if (ImGui::Button("声音                   ", ImVec2{ 80, 20 })) activeSubTabMisc = 2;
                            break;
                        default:
                            break;
                        }

                        ImGui::EndChild();

                        ImGui::SetCursorPos(ImVec2{ 100, 5 });
                        Style->Colors[ImGuiCol_ChildBg] = ImColor(29, 34, 38);
                        Style->ChildRounding = 5;
                        ImGui::BeginChild("##SubMain", ImVec2{ 590, 350 }, false);
                        {
                            ImGui::SetCursorPos(ImVec2{ 10, 10 });
                            switch (activeTab)
                            {
                            case 1: //演技选项
                                switch (activeSubTabLegitbot)
                                {
                                case 1:
                                    //自瞄
                                    renderLegitbotWindow();
                                    break;
                                case 2:
                                    //回溯
                                    renderBacktrackWindow();
                                    break;
                                case 3:
                                    //Triggerbot
                                    renderTriggerbotWindow();
                                    break;
                                case 4:
                                    //反自瞄
                                    renderLegitAntiAimWindow();
                                    break;
                                default:
                                    break;
                                }
                                break;
                            case 2: //暴力选项
                                switch (activeSubTabRagebot)
                                {
                                case 1:
                                    //自瞄
                                    renderRagebotWindow();
                                    break;
                                case 2:
                                    //回溯
                                    renderBacktrackWindow();
                                    break;
                                case 3:
                                    //反自瞄
                                    renderRageAntiAimWindow();
                                    break;
                                case 4:
                                    //假角度
                                    renderFakeAngleWindow();
                                    break;
                                case 5:
                                    //假卡
                                    renderFakelagWindow();
                                    break;
                                default:
                                    break;
                                }
                                break;
                            case 3: // 视觉选项
                                switch (activeSubTabVisuals)
                                {
                                case 1:
                                    //视觉杂项
                                    renderVisualsWindow();
                                    break;
                                case 2:
                                    //透视功能
                                    renderStreamProofESPWindow();
                                    break;
                                case 3:
                                    //上色功能
                                    renderChamsWindow();
                                    break;
                                case 4:
                                    //发光功能
                                    renderGlowWindow();
                                    break;
                                case 5:
                                    //皮肤修改器
                                    renderSkinChangerWindow();
                                    break;
                                default:
                                    break;
                                }
                                break;
                            case 4:
                                switch (activeSubTabMisc)
                                {
                                case 1:
                                    //功能
                                    renderMiscWindow();
                                    break;
                                case 2:
                                    //声音
                                    Sound::drawGUI();
                                    break;
                                default:
                                    break;
                                }
                                break;
                            case 5:
                                //参数配置
                                renderConfigWindow();
                                break;
                            default:
                                break;
                            }
                        }
                        ImGui::EndChild();
                    }
                    ImGui::EndChild();

                    ImGui::SetCursorPos(ImVec2{ 0, 410 });
                    Style->Colors[ImGuiCol_ChildBg] = ImColor(45, 50, 54);
                    Style->ChildRounding = 0;
                    ImGui::BeginChild("##Text", ImVec2{ 700, 20 }, false);
                    {
                        ImGui::SetCursorPos(ImVec2{ 2, 2 });
                        ImGui::Text("欢迎使用必胜客BSK完整版 祝您游戏愉快");
                    }
                    ImGui::EndChild();
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
    Style->Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 0.75f);
}
