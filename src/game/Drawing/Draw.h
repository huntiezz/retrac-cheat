#pragma once
#include "../../menu/ImGui/imgui.h"

void DrawCornerBox(int X, int Y, int W, int H, const ImColor color, int thickness) {
	float lineW = (W / 3);
	float lineH = (H / 3);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X, Y), ImVec2(X, Y + lineH), color, thickness);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X, Y), ImVec2(X + lineW, Y), color, thickness);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X + W - lineW, Y), ImVec2(X + W, Y), color, thickness);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X + W, Y), ImVec2(X + W, Y + lineH), color, thickness);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X, Y + H - lineH), ImVec2(X, Y + H), color, thickness);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X, Y + H), ImVec2(X + lineW, Y + H), color, thickness);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X + W - lineW, Y + H), ImVec2(X + W, Y + H), color, thickness);
	ImGui::GetBackgroundDrawList()->AddLine(ImVec2(X + W, Y + H - lineH), ImVec2(X + W, Y + H), color, thickness);
}

void DrawBox(int X, int Y, int W, int H, const ImColor color, int thickness) {
	ImGui::GetBackgroundDrawList()->AddRect(ImVec2(X, Y), ImVec2(X + W, Y + H), color, 0.0f, 0, thickness);
}

void DrawFilledBox(int X, int Y, int W, int H, const ImColor color, int thickness) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(X, Y), ImVec2(X + W, Y + H), ImColor(0, 0, 0, 70)); 
    ImGui::GetBackgroundDrawList()->AddRect(ImVec2(X, Y), ImVec2(X + W, Y + H), color, 0.0f, 0, thickness); 
}

void Draw3DBox(uintptr_t Mesh, ImColor Color, float thickness = Settings::ESPThickness) {
    if (!Mesh) return;
    Vector3 Head = GetBoneWithRotation(Mesh, EBoneIndex::Head);
    Vector3 Top3D, Bottom3D;
    if (!TryGetEspBoxWorldExtents(Mesh, Top3D, Bottom3D))
        return;
    Head = Top3D;
    Vector3 Root = Bottom3D;
    if (BoneWorldMissing(Head) || BoneWorldMissing(Root)) return;
    float height = Head.z - Root.z;
    if (height < 10.f) return;
    float width = height * 0.3f; 
    Vector3 bottom1 = Vector3(Root.x + width, Root.y + width, Root.z);
    Vector3 bottom2 = Vector3(Root.x - width, Root.y + width, Root.z);
    Vector3 bottom3 = Vector3(Root.x - width, Root.y - width, Root.z);
    Vector3 bottom4 = Vector3(Root.x + width, Root.y - width, Root.z);

    Vector3 top1 = Vector3(Head.x + width, Head.y + width, Head.z + 15);
    Vector3 top2 = Vector3(Head.x - width, Head.y + width, Head.z + 15);
    Vector3 top3 = Vector3(Head.x - width, Head.y - width, Head.z + 15);
    Vector3 top4 = Vector3(Head.x + width, Head.y - width, Head.z + 15);

    Vector3 b1, b2, b3, b4, t1, t2, t3, t4;
    if (ProjectWorldToScreen(bottom1, &b1) && ProjectWorldToScreen(bottom2, &b2) &&
        ProjectWorldToScreen(bottom3, &b3) && ProjectWorldToScreen(bottom4, &b4) &&
        ProjectWorldToScreen(top1, &t1) && ProjectWorldToScreen(top2, &t2) &&
        ProjectWorldToScreen(top3, &t3) && ProjectWorldToScreen(top4, &t4)) {

        auto dl = ImGui::GetBackgroundDrawList();
        
        dl->AddLine(ImVec2(b1.x, b1.y), ImVec2(b2.x, b2.y), Color, thickness);
        dl->AddLine(ImVec2(b2.x, b2.y), ImVec2(b3.x, b3.y), Color, thickness);
        dl->AddLine(ImVec2(b3.x, b3.y), ImVec2(b4.x, b4.y), Color, thickness);
        dl->AddLine(ImVec2(b4.x, b4.y), ImVec2(b1.x, b1.y), Color, thickness);

        dl->AddLine(ImVec2(t1.x, t1.y), ImVec2(t2.x, t2.y), Color, thickness);
        dl->AddLine(ImVec2(t2.x, t2.y), ImVec2(t3.x, t3.y), Color, thickness);
        dl->AddLine(ImVec2(t3.x, t3.y), ImVec2(t4.x, t4.y), Color, thickness);
        dl->AddLine(ImVec2(t4.x, t4.y), ImVec2(t1.x, t1.y), Color, thickness);

        dl->AddLine(ImVec2(b1.x, b1.y), ImVec2(t1.x, t1.y), Color, thickness);
        dl->AddLine(ImVec2(b2.x, b2.y), ImVec2(t2.x, t2.y), Color, thickness);
        dl->AddLine(ImVec2(b3.x, b3.y), ImVec2(t3.x, t3.y), Color, thickness);
        dl->AddLine(ImVec2(b4.x, b4.y), ImVec2(t4.x, t4.y), Color, thickness);
    }
}

void DrawBone(uintptr_t Mesh, int bone1, int bone2, ImColor Color) {
	Vector3 b1 = GetBoneWithRotation(Mesh, bone1);
	Vector3 b2 = GetBoneWithRotation(Mesh, bone2);
	if (BoneWorldMissing(b1) || BoneWorldMissing(b2))
		return;
	Vector3 b1_2d, b2_2d;
    
    if (ProjectWorldToScreen(b1, &b1_2d) && ProjectWorldToScreen(b2, &b2_2d)) {
	    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(b1_2d.x, b1_2d.y), ImVec2(b2_2d.x, b2_2d.y), Color, Settings::ESPThickness);
    }
}

void DrawCachedSkeletonScreen(const std::vector<std::pair<ImVec2, ImVec2>>& Lines, ImColor Color,
                              float thickness = Settings::ESPThickness) {
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	for (const auto& line : Lines)
		dl->AddLine(line.first, line.second, Color, thickness);
}

void DrawCachedSkeleton(const std::vector<std::pair<Vector3, Vector3>>& Lines, ImColor Color,
                        float thickness = Settings::ESPThickness) {
	for (const auto& line : Lines) {
		if (BoneWorldMissing(line.first) || BoneWorldMissing(line.second))
			continue;
		Vector3 s2d, e2d;
		if (ProjectWorldToScreen(line.first, &s2d) && ProjectWorldToScreen(line.second, &e2d)) {
			ImGui::GetBackgroundDrawList()->AddLine(ImVec2(s2d.x, s2d.y), ImVec2(e2d.x, e2d.y),
			                                       Color, thickness);
		}
	}
}

void DrawSkeleton(uintptr_t Mesh, ImColor Color) {
	if (!BoneSnapshotOkForRender(Mesh))
		return;
	static thread_local std::vector<std::pair<Vector3, Vector3>> tlsLines;
	static thread_local unsigned tlsFrame = 0;
	static thread_local uintptr_t tlsMesh = 0;
	const unsigned frame = EspFrameCounter.load(std::memory_order_relaxed);
	if (frame != tlsFrame || Mesh != tlsMesh) {
		tlsFrame = frame;
		tlsMesh = Mesh;
		CollectSkeletonWorldLines(Mesh, tlsLines);
	}
	DrawCachedSkeleton(tlsLines, Color);
}

void DrawSkeletonFromSnapshot(const BoneFrameSnapshot& snap, ImColor Color) {
	if (!BoneSnapshotOkForRender(snap))
		return;
	static thread_local std::vector<std::pair<Vector3, Vector3>> tlsLines;
	static thread_local unsigned tlsFrame = 0;
	static thread_local uintptr_t tlsMesh = 0;
	const unsigned frame = EspFrameCounter.load(std::memory_order_relaxed);
	if (frame != tlsFrame || snap.mesh != tlsMesh) {
		tlsFrame = frame;
		tlsMesh = snap.mesh;
		CollectSkeletonWorldLinesFromSnapshot(snap, tlsLines);
	}
	DrawCachedSkeleton(tlsLines, Color);
}

inline void DrawSkeletonWithOutline(uintptr_t mesh, ImColor color, ImColor outline) {
	if (Settings::PlayerESPOutline)
		DrawSkeleton(mesh, outline);
	DrawSkeleton(mesh, color);
}

inline void DrawSkeletonWithOutlineFromSnapshot(const BoneFrameSnapshot& snap, ImColor color,
                                                ImColor outline) {
	if (!BoneSnapshotOkForRender(snap))
		return;
	static thread_local std::vector<std::pair<Vector3, Vector3>> tlsLines;
	static thread_local std::vector<std::pair<ImVec2, ImVec2>> tlsScreen;
	static thread_local unsigned tlsFrame = UINT_MAX;
	static thread_local uintptr_t tlsMesh = 0;
	const unsigned frame = EspFrameCounter.load(std::memory_order_relaxed);
	if (frame != tlsFrame || snap.mesh != tlsMesh) {
		tlsFrame = frame;
		tlsMesh = snap.mesh;
		CollectSkeletonWorldLinesFromSnapshot(snap, tlsLines);
		tlsScreen.clear();
		tlsScreen.reserve(tlsLines.size());
		for (const auto& line : tlsLines) {
			if (BoneWorldMissing(line.first) || BoneWorldMissing(line.second))
				continue;
			Vector3 s2d, e2d;
			if (ProjectWorldToScreen(line.first, &s2d) && ProjectWorldToScreen(line.second, &e2d))
				tlsScreen.emplace_back(ImVec2(s2d.x, s2d.y), ImVec2(e2d.x, e2d.y));
		}
	}
	if (Settings::PlayerESPOutline)
		DrawCachedSkeletonScreen(tlsScreen, outline, Settings::ESPThickness + 2.f);
	DrawCachedSkeletonScreen(tlsScreen, color, Settings::ESPThickness);
}

void DrawRadar(const std::vector<LocalPtrs::CachedPlayer>& Players) {
    if (!Settings::Radar) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 Center = ImVec2(Settings::RadarX + Settings::RadarSize / 2, Settings::RadarY + Settings::RadarSize / 2);
    
    if (Settings::RadarBackground) {
        dl->AddRectFilled(ImVec2(Settings::RadarX, Settings::RadarY), ImVec2(Settings::RadarX + Settings::RadarSize, Settings::RadarY + Settings::RadarSize), ImColor(15, 15, 15, 240));
        dl->AddRect(ImVec2(Settings::RadarX, Settings::RadarY), ImVec2(Settings::RadarX + Settings::RadarSize, Settings::RadarY + Settings::RadarSize), Settings::MenuColor, 1.5f);
        dl->AddLine(ImVec2(Center.x, Settings::RadarY), ImVec2(Center.x, Settings::RadarY + Settings::RadarSize), ImColor(180, 180, 180, 120), 1.0f);
        dl->AddLine(ImVec2(Settings::RadarX, Center.y), ImVec2(Settings::RadarX + Settings::RadarSize, Center.y), ImColor(180, 180, 180, 120), 1.0f);
    }
    
    dl->AddCircleFilled(Center, 3.0f, ImColor(0, 255, 0, 255));

        float Yaw = -Camera::Rotation.y * (float)(M_PI / 180.0f);
        float CosYaw = cosf(Yaw);
        float SinYaw = sinf(Yaw);

        for (const auto& p : Players) {
            if (!Camera::Valid) break;
            if (!p.RootComponent) continue;
            Vector3 RootPos = Read<Vector3>(p.RootComponent + Offsets::Realitivelocation);
            Vector3 LocalPos = Camera::Location;
            float dist = LocalPos.Distance(RootPos);   
            float dx = RootPos.x - LocalPos.x;
            float dy = RootPos.y - LocalPos.y;
            float x = dx * CosYaw - dy * SinYaw;
            float y = dx * SinYaw + dy * CosYaw;
            float range = Settings::RadarRange;
            float scale = (Settings::RadarSize / 2) / range;     
            float rx = Center.x + y * scale;      
            float screenX = Center.x + (y * scale); 
            float screenY = Center.y - (x * scale); 
            if (screenX < Settings::RadarX) screenX = Settings::RadarX;
            if (screenX > Settings::RadarX + Settings::RadarSize) screenX = Settings::RadarX + Settings::RadarSize;
            if (screenY < Settings::RadarY) screenY = Settings::RadarY;
            if (screenY > Settings::RadarY + Settings::RadarSize) screenY = Settings::RadarY + Settings::RadarSize;
            ImColor col = ImColor(255, 0, 0, 255);
            if (Settings::RadarType == 0) {
                dl->AddCircleFilled(ImVec2(screenX, screenY), 3.0f, col);
            } else {
                dl->AddRectFilled(ImVec2(screenX - 3, screenY - 3), ImVec2(screenX + 3, screenY + 3), col);
            }
        }

    if (Settings::RadarLoot && Camera::Valid) {
        std::lock_guard<std::mutex> lock(LocalPtrs::LevelActorsMutex);
        float Yaw = -Camera::Rotation.y * (float)(M_PI / 180.0f);
        float CosYaw = cosf(Yaw);
        float SinYaw = sinf(Yaw);

        for (const auto& Entity : LocalPtrs::LevelActors) {
             Vector3 RootPos = Entity.Position;
             Vector3 LocalPos = Camera::Location;
             float dist = LocalPos.Distance(RootPos); 
             if (dist > Settings::RadarRange) continue;

             float dx = RootPos.x - LocalPos.x;
             float dy = RootPos.y - LocalPos.y;
             float x = dx * CosYaw - dy * SinYaw;
             float y = dx * SinYaw + dy * CosYaw;
             float range = Settings::RadarRange;
             float scale = (Settings::RadarSize / 2) / range;     
             float screenX = Center.x + (y * scale); 
             float screenY = Center.y - (x * scale); 

             if (screenX < Settings::RadarX) screenX = Settings::RadarX;
             if (screenX > Settings::RadarX + Settings::RadarSize) screenX = Settings::RadarX + Settings::RadarSize;
             if (screenY < Settings::RadarY) screenY = Settings::RadarY;
             if (screenY > Settings::RadarY + Settings::RadarSize) screenY = Settings::RadarY + Settings::RadarSize;
             
             dl->AddCircleFilled(ImVec2(screenX, screenY), 2.0f, Entity.Color);
        }
    }


    if (Settings::Menu) {
        ImGui::SetNextWindowPos(ImVec2(Settings::RadarX, Settings::RadarY));
        ImGui::SetNextWindowSize(ImVec2(Settings::RadarSize, Settings::RadarSize + 30));
        ImGui::SetNextWindowBgAlpha(0.0f);
        
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | 
                                       ImGuiWindowFlags_NoResize | 
                                       ImGuiWindowFlags_NoMove | 
                                       ImGuiWindowFlags_NoScrollbar | 
                                       ImGuiWindowFlags_NoCollapse | 
                                       ImGuiWindowFlags_NoSavedSettings | 
                                       ImGuiWindowFlags_NoBackground;

        if (ImGui::Begin("RadarInteract", nullptr, windowFlags)) {
            float dragHeight = 22.0f;
            float gap = 2.0f;
            float startY = Settings::RadarY + Settings::RadarSize + gap;
            
            dl->AddRectFilled(
                ImVec2(Settings::RadarX, startY), 
                ImVec2(Settings::RadarX + Settings::RadarSize, startY + dragHeight), 
                ImColor(15, 15, 15, 240)
            );

            std::string dragText = "Drag to Move";
            ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(Settings::FontSize, FLT_MAX, 0.0f, dragText.c_str());
            float textX = Settings::RadarX + (Settings::RadarSize - textSize.x) / 2;    
            float textY = startY + (dragHeight - textSize.y) / 2;

            dl->AddText(ImGui::GetFont(), Settings::FontSize, ImVec2(textX, textY), ImColor(255, 255, 255), dragText.c_str());
            ImGui::SetCursorPos(ImVec2(0, 0));
            ImGui::InvisibleButton("##RadarInteraction", ImVec2(Settings::RadarSize, Settings::RadarSize + dragHeight));
            
            bool isHovering = ImGui::IsItemHovered();
            static bool isResizing = false;

            if (isHovering && ImGui::IsMouseClicked(0)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                ImVec2 winPos = ImGui::GetWindowPos();
                float rSize = Settings::RadarSize;
                if (mousePos.x >= winPos.x + rSize - 20 && mousePos.y >= winPos.y + rSize - 20 && mousePos.y <= winPos.y + rSize) {
                    isResizing = true;
                } else {
                    isResizing = false;
                }
            }

            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
                if (isResizing) {
                    float delta = ImGui::GetIO().MouseDelta.x;
                    Settings::RadarSize += delta;
                    if (Settings::RadarSize < 100.0f) Settings::RadarSize = 100.0f;
                } else {
                    Settings::RadarX += ImGui::GetIO().MouseDelta.x;
                    Settings::RadarY += ImGui::GetIO().MouseDelta.y;
                }
            }

            float handleSize = 15.0f;
            ImVec2 resizePos = ImVec2(Settings::RadarX + Settings::RadarSize - handleSize, Settings::RadarY + Settings::RadarSize - handleSize);
            dl->AddTriangleFilled(
                ImVec2(resizePos.x + handleSize, resizePos.y),                
                ImVec2(resizePos.x + handleSize, resizePos.y + handleSize),   
                ImVec2(resizePos.x, resizePos.y + handleSize),                
                ImColor(200, 200, 200, 200)
            );

            ImGui::End();
        }
    }
}