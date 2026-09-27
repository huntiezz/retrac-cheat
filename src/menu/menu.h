#pragma once
#include "../util/common.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "icons.h"
#include "../util/Settings.h"
#include "../util/Config.h"
#include "../util/obfuscate.h"
#include "overlay.h"

const char* GetKeyName(int key) {
	static char name[128];
	switch (key) {
	case VK_LBUTTON: return xorstr_("Left Mouse");
	case VK_RBUTTON: return xorstr_("Right Mouse");
	case VK_MBUTTON: return xorstr_("Middle Mouse");
	case VK_XBUTTON1: return xorstr_("XButton 1");
	case VK_XBUTTON2: return xorstr_("XButton 2");
	default:
		if (GetKeyNameTextA(MapVirtualKeyA(key, MAPVK_VK_TO_VSC) << 16, name, sizeof(name))) {
			return name;
		}
		return xorstr_("Unknown");
	}
}

void SetStyle() {
	ImGuiStyle& style = ImGui::GetStyle();
	const ImVec4 accent = ImVec4(
	    Settings::MenuColor.Value.x, Settings::MenuColor.Value.y, Settings::MenuColor.Value.z, 1.f);
	const ImVec4 accentHover = ImVec4(
	    (std::min)(1.f, accent.x + 0.08f), (std::min)(1.f, accent.y + 0.09f),
	    (std::min)(1.f, accent.z + 0.04f), 1.f);
	const ImVec4 accentPress = ImVec4(accent.x * 0.78f, accent.y * 0.78f, accent.z * 0.78f, 1.f);
	const ImVec4 border = ImVec4(0.235f, 0.235f, 0.235f, 1.f);
	const ImVec4 frame = ImVec4(0.18f, 0.18f, 0.18f, 1.f);

	style.WindowRounding = 0.f;
	style.ChildRounding = 0.f;
	style.FrameRounding = 2.f;
	style.GrabRounding = 2.f;
	style.PopupRounding = 2.f;
	style.ScrollbarRounding = 0.f;
	style.WindowBorderSize = 1.f;
	style.ChildBorderSize = 1.f;
	style.FrameBorderSize = 1.f;
	style.WindowPadding = ImVec2(0.f, 0.f);
	style.FramePadding = ImVec2(6.f, 2.f);
	style.ItemSpacing = ImVec2(6.f, 4.f);

	style.Colors[ImGuiCol_Text] = ImVec4(0.839f, 0.839f, 0.839f, 1.f);
	style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.541f, 0.541f, 0.541f, 1.f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.106f, 0.106f, 0.106f, 1.f);
	style.Colors[ImGuiCol_ChildBg] = ImVec4(0.078f, 0.078f, 0.078f, 1.f);
	style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.98f);
	style.Colors[ImGuiCol_Border] = border;
	style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);
	style.Colors[ImGuiCol_FrameBg] = frame;
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.f);
	style.Colors[ImGuiCol_FrameBgActive] = frame;
	style.Colors[ImGuiCol_TitleBg] = ImVec4(0.165f, 0.165f, 0.165f, 1.f);
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.165f, 0.165f, 0.165f, 1.f);
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.165f, 0.165f, 0.165f, 1.f);
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.106f, 0.106f, 0.106f, 1.f);
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.f);
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.23f, 0.23f, 0.23f, 1.f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.29f, 0.29f, 0.29f, 1.f);
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.f);
	style.Colors[ImGuiCol_CheckMark] = accent;
	style.Colors[ImGuiCol_SliderGrab] = accent;
	style.Colors[ImGuiCol_SliderGrabActive] = accentPress;
	style.Colors[ImGuiCol_Button] = frame;
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.22f, 0.22f, 1.f);
	style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.f);
	style.Colors[ImGuiCol_Header] = ImVec4(0.122f, 0.122f, 0.122f, 1.f);
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.16f, 0.16f, 0.16f, 1.f);
	style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.2f, 0.2f, 0.2f, 1.f);
	style.Colors[ImGuiCol_Separator] = border;
	style.Colors[ImGuiCol_Tab] = ImVec4(0.059f, 0.059f, 0.059f, 1.f);
	style.Colors[ImGuiCol_TabHovered] = ImVec4(0.122f, 0.122f, 0.122f, 1.f);
	style.Colors[ImGuiCol_TabActive] = ImVec4(0.078f, 0.078f, 0.078f, 1.f);
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
}

static void PushVenzaPrimaryButton() {
	const ImVec4 accent = ImVec4(Settings::MenuColor.Value.x, Settings::MenuColor.Value.y,
	                             Settings::MenuColor.Value.z, 1.f);
	ImGui::PushStyleColor(ImGuiCol_Button, accent);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
	                      ImVec4((std::min)(1.f, accent.x + 0.08f), (std::min)(1.f, accent.y + 0.09f),
	                             (std::min)(1.f, accent.z + 0.04f), 1.f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,
	                      ImVec4(accent.x * 0.78f, accent.y * 0.78f, accent.z * 0.78f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
}

static void PopVenzaPrimaryButton() {
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(4);
}

void Hotkey(const char* label, int* k, const ImVec2& size_arg = ImVec2(0, 0))
{
    ImGui::PushID(label); 
    ImGui::Text("%s", label);   
    static const auto* waiting_key_ptr = (int*)nullptr;
    char buf[128];
    if (waiting_key_ptr == k) {
        strcpy(buf, xorstr_("..."));
    } else {
        strcpy(buf, GetKeyName(*k));
    }   
    ImVec2 size = size_arg;
    if (size.x == 0) size.x = 200;
    if (size.y == 0) size.y = 22;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.235f, 0.235f, 0.235f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
    if (ImGui::Button(buf, size)) {
        if (waiting_key_ptr == k) {
            waiting_key_ptr = nullptr;
        } else {
            waiting_key_ptr = k;
        }
    }   
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    
    if (waiting_key_ptr == k) {
        for (int i = 1; i < 256; i++) {
             if (GetAsyncKeyState(i) & 0x8000) {
                 if (i == VK_LBUTTON && ImGui::IsMouseClicked(0)) continue; 
                 if (i == VK_ESCAPE) {
                     waiting_key_ptr = nullptr;
                     break; 
                 }
                 *k = i;
                 waiting_key_ptr = nullptr;
                 break;
             }
         }
    }
    
    ImGui::PopID();
}

static bool VenzaNavTab(const char* label, int index, int& active, const ImVec2& size) {
	const bool selected = active == index;
	if (selected) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.078f, 0.078f, 0.078f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.078f, 0.078f, 0.078f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.078f, 0.078f, 0.078f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.84f, 0.84f, 0.84f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.235f, 0.235f, 0.235f, 1.f));
	} else {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.059f, 0.059f, 0.059f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.122f, 0.122f, 0.122f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.149f, 0.149f, 0.149f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.54f, 0.54f, 0.54f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.235f, 0.235f, 0.235f, 1.f));
	}
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 2.f));
	const bool clicked = ImGui::Button(label, size);
	ImGui::PopStyleVar(2);
	ImGui::PopStyleColor(5);
	if (clicked)
		active = index;
	return clicked;
}

bool AnimatedCombo(const char* label, int* current_item, const char* const items[], int items_count)
{
    ImGui::PushID(label);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    float w = ImGui::CalcItemWidth();
    float h = ImGui::GetFrameHeight();
    ImVec2 pos = window->DC.CursorPos;
    ImRect total_bb(pos, ImVec2(pos.x + w, pos.y + h));   
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id, &total_bb))
    {
        ImGui::PopID();
        return false;
    }
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);    
    static std::map<ImGuiID, bool> is_open_map;
    static std::map<ImGuiID, float> anim_map;
    if (pressed) {
        is_open_map[id] = !is_open_map[id];
    }
    float target = is_open_map[id] ? 1.0f : 0.0f;
    float dt = ImGui::GetIO().DeltaTime;
    float speed = 8.0f;
    if (anim_map[id] < target) {
        anim_map[id] += dt * speed;
        if (anim_map[id] > target) anim_map[id] = target;
    } else if (anim_map[id] > target) {
        anim_map[id] -= dt * speed;
        if (anim_map[id] < target) anim_map[id] = target;
    }
    ImVec4 c = Settings::MenuColor;
    window->DrawList->AddRectFilled(total_bb.Min, total_bb.Max, ImColor(0.1f, 0.1f, 0.1f, 1.0f), 4.0f);
    window->DrawList->AddRect(total_bb.Min, total_bb.Max, ImColor(c.x, c.y, c.z, 1.0f), 4.0f);
    const char* current_text = (current_item && *current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";
    ImVec2 text_size = ImGui::CalcTextSize(current_text);
    window->DrawList->AddText(ImVec2(pos.x + 10, pos.y + (h - text_size.y) / 2), ImColor(1.0f, 1.0f, 1.0f, 1.0f), current_text);
    ImVec2 label_size = ImGui::CalcTextSize(label);
    if (label_size.x > 0) {
       ImGui::SameLine(0, style.ItemInnerSpacing.x);
       ImGui::Text("%s", label);
    }
    ImVec2 arrow_center = ImVec2(pos.x + w - 20, pos.y + h / 2);
    ImVec2 p1 = ImVec2(arrow_center.x, arrow_center.y + 3);
    ImVec2 p2 = ImVec2(arrow_center.x - 4, arrow_center.y - 3);
    ImVec2 p3 = ImVec2(arrow_center.x + 4, arrow_center.y - 3);  
    if (anim_map[id] > 0.5f) {
         p1 = ImVec2(arrow_center.x, arrow_center.y - 3);
         p2 = ImVec2(arrow_center.x - 4, arrow_center.y + 3);
         p3 = ImVec2(arrow_center.x + 4, arrow_center.y + 3);
    }   
    window->DrawList->AddTriangleFilled(p1, p2, p3, ImColor(1.0f, 1.0f, 1.0f, 1.0f));
    bool value_changed = false;
    if (anim_map[id] > 0.01f)
    {
        ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + h + 2));
        float item_h = ImGui::GetFrameHeight();
        float full_h = item_h * items_count + style.WindowPadding.y * 2;
        float cur_h = full_h * anim_map[id];     
        ImGui::SetNextWindowSize(ImVec2(w, cur_h));     
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;     
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, anim_map[id]);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(c.x, c.y, c.z, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));    
        char popup_id[64];
        snprintf(popup_id, sizeof(popup_id), "##combo_popup_%08x", id);        
        if (ImGui::Begin(popup_id, nullptr, flags))
        {
             if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered() && !total_bb.Contains(ImGui::GetIO().MousePos)) {
                 is_open_map[id] = false;
             }            
             for (int i = 0; i < items_count; i++)
             {
                 bool is_selected = (*current_item == i);
                 if (ImGui::Selectable(items[i], is_selected)) {
                     *current_item = i;
                     value_changed = true;
                     is_open_map[id] = false;
                 }
             }
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);
    }
    ImGui::PopID();
    return value_changed;
}

bool AnimatedButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0))
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
    return false;
    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
    return false;
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    static std::map<ImGuiID, float> anim_map;
    float target = (hovered || held) ? 1.0f : 0.0f;
    float speed = 8.0f;
    float dt = ImGui::GetIO().DeltaTime;   
    if (anim_map[id] < target) {
        anim_map[id] += dt * speed;
        if (anim_map[id] > target) anim_map[id] = target;
    } else if (anim_map[id] > target) {
        anim_map[id] -= dt * speed;
        if (anim_map[id] < target) anim_map[id] = target;
    }
    ImVec4 c = Settings::MenuColor;
    ImVec4 base_col = ImVec4(c.x * 0.6f, c.y * 0.6f, c.z * 0.6f, 0.4f);
    ImVec4 hover_col = ImVec4(c.x, c.y, c.z, 0.8f);
    float t = anim_map[id];
    ImVec4 final_col = ImVec4(
        base_col.x + (hover_col.x - base_col.x) * t,
        base_col.y + (hover_col.y - base_col.y) * t,
        base_col.z + (hover_col.z - base_col.z) * t,
        base_col.w + (hover_col.w - base_col.w) * t
    );
    window->DrawList->AddRectFilled(bb.Min, bb.Max, ImColor(final_col), 4.0f);
    window->DrawList->AddRect(bb.Min, bb.Max, ImColor(c.x, c.y, c.z, 0.8f), 4.0f);
    ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);
    return pressed;
}

void Menu() {
    SetStyle();
	static int activeTab = 0;
	static const char kMenuWindowId[] = "RetracMenu";
	const DWORD picker_flags = ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreview;
	ImGui::SetNextWindowSize({ 740.f, 520.f }, ImGuiCond_Always);
	ImGui::SetNextWindowPos(ImVec2(80.f, 80.f), ImGuiCond_FirstUseEver);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.106f, 0.106f, 0.106f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.235f, 0.235f, 0.235f, 1.f));
	ImGui::Begin(kMenuWindowId, 0,
	    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
	        ImGuiWindowFlags_NoTitleBar);
	ImGui::PopStyleColor(2);

	const ImVec2 winPos = ImGui::GetWindowPos();
	const float winW = ImGui::GetWindowWidth();
	ImDrawList* titleDl = ImGui::GetWindowDrawList();
	titleDl->AddRectFilled(winPos, ImVec2(winPos.x + winW, winPos.y + 22.f), IM_COL32(42, 42, 42, 255));
	titleDl->AddLine(ImVec2(winPos.x, winPos.y + 22.f), ImVec2(winPos.x + winW, winPos.y + 22.f),
	                 IM_COL32(60, 60, 60, 255));
	titleDl->AddText(ImVec2(winPos.x + 8.f, winPos.y + 3.f), IM_COL32(214, 214, 214, 255),
	                 xorstr_("Retrac External"));

	ImGui::SetCursorPos(ImVec2(0.f, 22.f));
	ImGui::BeginChild(xorstr_("##homeNav"), ImVec2(0.f, 32.f), false, ImGuiWindowFlags_NoScrollbar);
	const float navPadX = 16.f;
	const float tabGap = 4.f;
	const int tabCount = 8;
	ImGui::SetCursorPos(ImVec2(navPadX, 6.f));
	const float tabRowW = ImGui::GetContentRegionAvail().x;
	const float tabW =
	    tabRowW > tabGap * (tabCount - 1)
	        ? (tabRowW - tabGap * static_cast<float>(tabCount - 1)) / static_cast<float>(tabCount)
	        : tabRowW / static_cast<float>(tabCount);
	const ImVec2 tabSize(tabW, 22.f);
	static const char* kTabLabels[] = { "Aimbot", "Visuals", "World", "Exploits",
	                                    "Misc", "Style", "Config", "Info" };
	for (int i = 0; i < tabCount; ++i) {
		if (i > 0)
			ImGui::SameLine(0.f, tabGap);
		VenzaNavTab(kTabLabels[i], i, activeTab, tabSize);
	}
	ImGui::EndChild();

	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.078f, 0.078f, 0.078f, 1.f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 12.f));
	ImGui::BeginChild(xorstr_("##homePanel"), ImVec2(0.f, 0.f), false,
	    ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::PopStyleVar();

	switch (activeTab) {
	case 0:
			ImGui::Text(xorstr_("Aimbot Configuration"));
			ImGui::Spacing();
			ImGui::Checkbox(xorstr_("Enable Aimbot"), &Settings::Aimbot);
			ImGui::Checkbox(xorstr_("Enable Triggerbot"), &Settings::Triggerbot);
            ImGui::Checkbox(xorstr_("Visible Check"), &Settings::VisCheck);
			ImGui::Checkbox(xorstr_("Show FOV"), &Settings::ShowFOV);
			ImGui::SameLine();
			ImGui::ColorEdit4(xorstr_("##FOVColor"), (float*)&Settings::FOVColor, picker_flags);
			ImGui::SliderFloat(xorstr_("FOV Size"), &Settings::AimbotFOV, 20.0f, 800.0f, xorstr_("%.1f"));
			ImGui::SliderFloat(xorstr_("Smoothness"), &Settings::Smoothnes, 1.0f, 20.0f, xorstr_("%.2f"));
			
			ImGui::Spacing();
			Hotkey(xorstr_("Aim Key"), &Settings::AimKey);

            ImGui::Spacing();
			Hotkey(xorstr_("Triggerbot Key"), &Settings::TriggerbotKey);           
            ImGui::SliderInt(xorstr_("Trigger Bot Delay (ms)"), &Settings::TriggerbotDelay, 0, 1000);
		break;
	case 1:
			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnOffset(1, ImGui::GetWindowWidth() - 250.0f);
			ImGui::Text(xorstr_("Visuals Configuration"));
			ImGui::Spacing();
			ImGui::Checkbox(xorstr_("Box"), &Settings::Box);
            if (Settings::Box) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.0f);
                const char* boxtypes[] = { xorstr_("2D Box"), xorstr_("Corner Box"), xorstr_("3D Box"), xorstr_("Filled Box") };
                AnimatedCombo(xorstr_("Box Style"), &Settings::BoxType, boxtypes, IM_ARRAYSIZE(boxtypes));
            }
			ImGui::Checkbox(xorstr_("Snapline"), &Settings::Snapline);
			if (Settings::Snapline) {
				ImGui::SameLine();
				ImGui::SetNextItemWidth(150.0f);
				const char* snapos[] = { xorstr_("Bottom"), xorstr_("Center"), xorstr_("Top") };
				AnimatedCombo(xorstr_("Snapline Pos"), &Settings::SnaplinePos, snapos, IM_ARRAYSIZE(snapos));
			}
			ImGui::Checkbox(xorstr_("Weapon"), &Settings::WeaponESP);
			ImGui::Checkbox(xorstr_("Ammo"), &Settings::AmmoESP);
            ImGui::Checkbox(xorstr_("Platform"), &Settings::Platform);
            ImGui::Checkbox(xorstr_("Kills"), &Settings::KillESP);
            
            ImGui::Spacing();

            
			ImGui::Checkbox(xorstr_("Distance"), &Settings::Distance);
			ImGui::Checkbox(xorstr_("Playername"), &Settings::Username);
			ImGui::Checkbox(xorstr_("Skeleton"), &Settings::Skeleton);
			ImGui::NextColumn();
			ImGui::Text(xorstr_("Colors"));
			ImGui::ColorEdit4((xorstr_("Visible")), reinterpret_cast<float*>(&Settings::VisibleColor), picker_flags);
			ImGui::ColorEdit4((xorstr_("InVisible")), reinterpret_cast<float*>(&Settings::NVisibleColor), picker_flags);
			ImGui::ColorEdit4((xorstr_("Text Color")), reinterpret_cast<float*>(&Settings::TextColor), picker_flags);	
			ImGui::Columns(1);
		break;
	case 2: {
			ImGui::Text(xorstr_("World ESP Configuration"));
			ImGui::Spacing();
			ImGui::Checkbox(xorstr_("Enable World ESP"), &Settings::WorldESP);
			ImGui::Checkbox(xorstr_("Draw Pickups"), &Settings::PickupESP);
            ImGui::Checkbox(xorstr_("Draw Distance"), &Settings::PickupDistance);
			ImGui::Spacing();
			ImGui::SliderFloat(xorstr_("Max Distance"), &Settings::WorldESPMaxDistance, 10.0f, 500.0f, xorstr_("%.1fm"));
            const char* box_types[] = { xorstr_("Common"), xorstr_("Uncommon"), xorstr_("Rare"), xorstr_("Epic"), xorstr_("Legendary"), xorstr_("Mythic"),};
            AnimatedCombo(xorstr_("Min Rarity"), &Settings::MinRarity, box_types, IM_ARRAYSIZE(box_types));
		break;
	}
	case 3:
			ImGui::Text(xorstr_("Exploits Configuration"));
			ImGui::Spacing();
            ImGui::Text(xorstr_("Risky Use At Your Own Risk!!"));
            ImGui::Spacing();
			ImGui::Columns(2, nullptr, false);
            ImGui::SetColumnOffset(1, ImGui::GetWindowWidth() - 250.0f);
			ImGui::Checkbox(xorstr_("Instant Reload"), &Settings::InstantReload);
			ImGui::Checkbox(xorstr_("No Recoil"), &Settings::NoRecoil);
			ImGui::Checkbox(xorstr_("No Spread"), &Settings::NoSpread);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), xorstr_("(only works while targeting)"));
			ImGui::Checkbox(xorstr_("One hit Pickaxe"), &Settings::FastPickaxe);
            ImGui::Checkbox(xorstr_("Projectile tp"), &Settings::BulletTP);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), xorstr_("(only works with Snipers for hunting rifles)"));
			ImGui::Checkbox(xorstr_("Teleport To Nearest Enemy"), &Settings::TeleportEnemies);
            if (Settings::TeleportEnemies) {
                Hotkey(xorstr_("Teleport Key"), &Settings::TeleportKey);
            }
			ImGui::Checkbox(xorstr_("Aim While Jumping"), &Settings::AimWhileJumping);
			ImGui::Checkbox(xorstr_("Shot Trough Walls v2"), &Settings::MagicBullet);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), xorstr_("(Shoots through everything)"));
			ImGui::Checkbox(xorstr_("Instant Charge"), &Settings::InstantCharge);
			ImGui::Checkbox(xorstr_("FOV Changer"), &Settings::FOVChanger);
			if (Settings::FOVChanger) {
				ImGui::SliderFloat(xorstr_("FOV"), &Settings::FOVChangerValue, 60.0f, 170.0f, xorstr_("%.1f"));
			}
            ImGui::Checkbox(xorstr_("Chams"), &Settings::Chams);
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), xorstr_("(High Performance Cost)"));
            if (Settings::Chams) {
                const char* colors[] = { xorstr_("Blue"), xorstr_("Yellow"), xorstr_("Red") };
                int values[] = { 8, 12, 11 };
                int current_idx = 0;
                if (Settings::ChamsColor == 12) current_idx = 1;
                else if (Settings::ChamsColor == 11) current_idx = 2;
                if (AnimatedCombo(xorstr_("Chams Color"), &current_idx, colors, IM_ARRAYSIZE(colors))) {
                    Settings::ChamsColor = values[current_idx];
                }
            }
            ImGui::Columns(1);
		break;
	case 4:
            ImGui::Text(xorstr_("Misc Settings"));
			ImGui::Checkbox(xorstr_("VSync"), &Settings::VSync);
			ImGui::Checkbox(xorstr_("Show FPS"), &Settings::ShowFPS);
            ImGui::Checkbox(xorstr_("Debug Logs (console, every 2s)"), &Settings::DebugLogs);
            ImGui::Checkbox(xorstr_("Enable Radar"), &Settings::Radar);
            if (Settings::Radar) {
                ImGui::Checkbox(xorstr_("Show Loot"), &Settings::RadarLoot);
                ImGui::SliderFloat(xorstr_("Range"), &Settings::RadarRange, 1000.f, 50000.f, "%.0f");
                Settings::RadarBackground = true;
            }
            ImGui::ColorEdit4(xorstr_("Menu Color"), (float*)&Settings::MenuColor, picker_flags);
            ImGui::Spacing();
		break;
	case 5:
			ImGui::Text(xorstr_("Player Esp Customizations"));
			ImGui::SliderFloat(xorstr_("Esp Thickness"), &Settings::ESPThickness, 0.5f, 15.0f, xorstr_("%.1f"));
			ImGui::SliderFloat(xorstr_("Font Size"), &Settings::FontSize, 10.0f, 30.0f, xorstr_("%.0f"));          
            ImGui::Checkbox(xorstr_("Player Esp Outline"), &Settings::PlayerESPOutline);
            ImGui::Checkbox(xorstr_("Player Text Outline"), &Settings::TextOutline);
            ImGui::Spacing();
			ImGui::Spacing();
            ImGui::Text(xorstr_("World Esp Customizations"));
            ImGui::SliderFloat(xorstr_("World Font Size"), &Settings::WorldESPFontSize, 10.0f, 30.0f, xorstr_("%.0f"));         
            ImGui::Checkbox(xorstr_("World Text Outline"), &Settings::WorldESPTextOutline);
		break;
	case 6: {
            static std::vector<std::string> configs = Config::GetConfigs();
            static int selected = -1;
            ImGui::TextColored(ImVec4(1.f, 1.f, 0.f, 1.f), xorstr_("Info: Type a name to Create/Save. Select to Load."));
            ImGui::Spacing();
            ImGui::Text(xorstr_("Config Name:"));
            static char configName[64] = "";
            ImGui::SetNextItemWidth(250.0f);
            ImGui::InputText(xorstr_("##ConfigName"), configName, sizeof(configName));
            ImGui::SameLine();
            PushVenzaPrimaryButton();
            if (ImGui::Button(xorstr_("Save"), ImVec2(64.f, 24.f))) {
                Config::Save(configName);
            }
            PopVenzaPrimaryButton();
            ImGui::SameLine();
            if (ImGui::Button(xorstr_("Load"), ImVec2(64.f, 24.f))) {
                 Config::Load(configName);
            }
            ImGui::SameLine();
            if (ImGui::Button(xorstr_("Delete"), ImVec2(64.f, 24.f))) {
                 Config::Delete(configName);
                 configs = Config::GetConfigs();
                 memset(configName, 0, sizeof(configName));
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();           
            if (ImGui::Button(xorstr_("Refresh Config List"), ImVec2(-1.f, 24.f))) {
                configs = Config::GetConfigs();
            }
            ImGui::Spacing();
            ImGui::Text(xorstr_("Saved Configs:"));
            if (ImGui::BeginListBox(xorstr_("##SavedConfigs"), ImVec2(-1.f, 160.f))) {
                for (int i = 0; i < configs.size(); i++) {
                    const bool is_selected = (selected == i);
                    if (ImGui::Selectable(configs[i].c_str(), is_selected)) {
                        selected = i;
                        snprintf(configName, sizeof(configName), "%s", configs[i].c_str());
                    }
                    if (is_selected)
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::EndListBox();
            }
		break;
	}
	case 7:
            ImGui::Spacing();
            ImGui::Text(xorstr_("Credits:"));
            ImGui::Separator();
            ImGui::Text(xorstr_("Discord Inc"));
            ImGui::Text(xorstr_("Made in Germany"));
            ImGui::Spacing();
            ImGui::Spacing();         
            ImGui::Text(xorstr_("Debug Info:"));
            ImGui::Separator();
            ImGui::Text(xorstr_("Resolution: %dx%d"), Settings::Width, Settings::Height);
            ImGui::Text(xorstr_("Application: %.3f ms/frame (%.1f FPS)"), 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text(xorstr_("Menu Rendering: %s"), Settings::Menu ? xorstr_("Active") : xorstr_("Background"));
		break;
	default:
		break;
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::End();
}

static inline bool setup() {
    Config::Setup();
	return create_overlay();
}