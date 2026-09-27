#pragma once 
#include "../util/math.h"
#include "../sdk-offsets/sdk.h"
#include "../sdk-offsets/offsets.hpp"
#include "../util/Settings.h"
#include "../menu/ImGui/imgui.h"
#include "Aimbot/Aimbot.h"
#include "Drawing/Draw.h"
#include "Exploits/Exploits.h"
#include "world-esp/WorldESP.h"
#include "../util/obfuscate.h"
#include <unordered_set>


inline float GetCrossDistance(double x1, double y1, double x2, double y2) {
    return (float)sqrt(pow(y2 - y1, 2) + pow(x2 - x1, 2));
}

inline void PlayerCacheThread() {
    while (true) {
      try {
        if (!LocalPtrs::Gworld) {
             std::this_thread::sleep_for(std::chrono::milliseconds(100));
             continue;
        }
        std::vector<LocalPtrs::CachedPlayer> TempCache;
        TempCache.reserve(100);
        std::unordered_set<uintptr_t> seenPawns;
        uintptr_t GameState = Read<uintptr_t>(LocalPtrs::Gworld + Offsets::GameState);
        if (!GameState || !Memory::IsValid(GameState)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        uintptr_t PlayerArray = Read<uintptr_t>(GameState + Offsets::PlayerArray);
        int Num = Read<int>(GameState + (Offsets::PlayerArray + sizeof(uintptr_t)));
        if (Num > 200) Num = 200;
        if (PlayerArray && Num > 0) {
            for (int i = 0; i < Num; i++) {
                uintptr_t PlayerState = Read<uintptr_t>(PlayerArray + (i * sizeof(uintptr_t)));
                if (!PlayerState) continue;
                const uintptr_t Pawn = Read<uintptr_t>(PlayerState + Offsets::PawnPrivate);
                if (Pawn && seenPawns.count(Pawn)) continue;
                if (Pawn) seenPawns.insert(Pawn);
                PushCachedPlayerFromPlayerState(PlayerState, TempCache);
            }
        }
        // Level actor scan is lobby-only; in-match PlayerArray is enough and the scan is costly.
        if (Num <= 1)
            AppendPlayersFromWorldLevels(LocalPtrs::Gworld, TempCache, seenPawns);
        PushCachedLobbyLocalPreview(TempCache);
        {
            std::lock_guard<std::mutex> lock(LocalPtrs::CachedPlayersMutex);
            LocalPtrs::CachedPlayers = std::move(TempCache);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
      } catch (...) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
}

void UpdateLocalPlayerThread() {
    while (true) {
        const uintptr_t world = CurrentWorld();
        if (world && Memory::IsValid(world))
            SyncLocalPtrsFromWorld(world);
        else if (!LocalPtrs::Gworld || !Memory::IsValid(LocalPtrs::Gworld))
            LogRuntimeChainFailure("WORLD", "no UWorld from image global or cache");
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}


// TRACE-only ESP summary (~120 frames). Runtime stage changes log separately.
inline void LogEspDebug(int cached, int drawn, uintptr_t firstMesh) {
    (void)cached;
    (void)drawn;
    (void)firstMesh;
    if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace))
        return;
    const uint64_t frame = EspFrameCounter.load(std::memory_order_relaxed);
    if ((frame % 120ull) != 0ull)
        return;
    std::cout << xorstr_("[esp] drawn ") << drawn << xorstr_(" cached ") << cached << '\n';
}

inline void ActorLoop() { 

	if (Settings::ShowFPS) {
		ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 40), ImColor(255, 255, 255), (xorstr_("FPS: ") + std::to_string((int)ImGui::GetIO().Framerate)).c_str());
	}
    
	if (!Settings::Menu) {
		ImGui::GetBackgroundDrawList()->AddText(ImVec2(10, 10), ImColor(214, 214, 214, 255),
		                                       xorstr_("Retrac External — Insert menu"));
	}

	PollSdkReadyFromOverlay();
	if (!SdkReady.load(std::memory_order_relaxed))
		return;

	RefreshLiveChainForRender();

	uintptr_t CurrentBestMesh = NULL;
	float CurrentBestDist = FLT_MAX;

	EspFrameCounter.fetch_add(1, std::memory_order_relaxed);
	RenderPipeline::ResetFrameStats();
	GetCamera();
	CommitCameraFrameForW2S();
	SyncRuntimeInitStageWithCamera(Camera::ViewProjectionReady, Camera::Valid);

	CameraSnapshot renderCam{};
	{
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		renderCam = g_W2SCameraSnapshot;
	}
	if (renderCam.projectionValid &&
	    Settings::DebugAtLeast(Settings::DebugVerbosity::Trace))
		DrawRenderPipelineSanityTest(renderCam);
	DrawW2SDebugVisualization();

    // World ESP needs a world and a camera, not the local pawn. Gating it on AcknowledgedPawn
    // (which reads 0 on Retrac) is why it never drew anything.
    DrawWorldESP();

    DrawRadar(LocalPtrs::CachedPlayers);

	if (Settings::Aimbot && Settings::ShowFOV) {
		ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(CenterWidth, CenterHeight), Settings::AimbotFOV, Settings::FOVColor, 64);
	}
    int CachedCount = 0;
    int DrawnCount = 0;
    uintptr_t FirstMesh = 0;
    if (!Camera::ViewProjectionReady) {
        LogEspDebug(CachedCount, DrawnCount, FirstMesh);
        return;
    }
    std::vector<LocalPtrs::CachedPlayer> players;
    {
        std::lock_guard<std::mutex> lock(LocalPtrs::CachedPlayersMutex);
        players = LocalPtrs::CachedPlayers;
    }
    CachedCount = (int)players.size();
    for (const auto& CachedP : players) {
            if (!CachedP.Pawn || !Memory::IsValid(CachedP.Pawn))
                continue;
            const uintptr_t mesh =
                CachedP.Mesh && Memory::IsValid(CachedP.Mesh)
                    ? CachedP.Mesh
                    : Read<uintptr_t>(CachedP.Pawn + Offsets::Mesh);
            if (!mesh || !Memory::IsValid(mesh))
                continue;
            if (!CachedP.LobbyPreview &&
                IsLocalPlayerTarget(CachedP.Pawn, CachedP.PlayerState, mesh))
                continue;
            if (!FirstMesh)
                FirstMesh = mesh;
            const uintptr_t rootComponent = Read<uintptr_t>(CachedP.Pawn + Offsets::RootComponent);
            if (!rootComponent || !Memory::IsValid(rootComponent))
                continue;

            const Vector3 enemyPos = GetMeshWorldLocation(mesh);
            float Distance = 0.f;
            if (LocalPtrs::Player && Memory::IsValid(LocalPtrs::Player)) {
                const uintptr_t localMesh = Read<uintptr_t>(LocalPtrs::Player + Offsets::Mesh);
                if (localMesh && Memory::IsValid(localMesh))
                    Distance = Dist3(GetMeshWorldLocation(localMesh), enemyPos) / 100.f;
            }
            if (Distance <= 0.f && Camera::Valid)
                Distance = Dist3(Camera::Location, enemyPos) / 100.f;
            if (Distance > 250000.0f) continue;

            const bool visible = IsVisible(mesh);
            ImColor Color = visible ? Settings::VisibleColor : Settings::NVisibleColor;

            const BoneFrameSnapshot& boneSnap = GetBoneFrameSnapshot(mesh);
            if (!BoneSnapshotOkForRender(boneSnap))
                continue;

            EspScreenBounds espBounds{};
            if (!BuildEspScreenBoundsFromSnapshot(boneSnap, espBounds) || espBounds.height < 4.f)
                continue;

            if (Settings::Skeleton)
                DrawSkeletonWithOutlineFromSnapshot(boneSnap, Color, ImColor(0, 0, 0, 255));

            ++DrawnCount;
            RenderPipeline::g_FrameStats.queuedPrimitives.fetch_add(1, std::memory_order_relaxed);
            RenderPipeline::g_FrameStats.submittedPrimitives.fetch_add(1, std::memory_order_relaxed);

            const float boxTopY = espBounds.topY;
            const float boxBottomY = espBounds.bottomY;
            const float BoxCenterX = espBounds.centerX;
            const float CornerHeight = espBounds.height;
            const float CornerWidth = espBounds.width;
            float TextOffset = 0.0f;

            if (Settings::Box) {
                if (Settings::BoxType == 0) {
                    if (Settings::PlayerESPOutline) DrawBox(BoxCenterX - (CornerWidth / 2), boxTopY, CornerWidth, CornerHeight, ImColor(0, 0, 0, 255), Settings::ESPThickness + 2.0f);
                    DrawBox(BoxCenterX - (CornerWidth / 2), boxTopY, CornerWidth, CornerHeight, Color, Settings::ESPThickness);
                }
                else if (Settings::BoxType == 1) {
                    if (Settings::PlayerESPOutline) DrawCornerBox(BoxCenterX - (CornerWidth / 2), boxTopY, CornerWidth, CornerHeight, ImColor(0, 0, 0, 255), Settings::ESPThickness + 2.0f);
                    DrawCornerBox(BoxCenterX - (CornerWidth / 2), boxTopY, CornerWidth, CornerHeight, Color, Settings::ESPThickness);
                }
                else if (Settings::BoxType == 2) {
                    if (Settings::PlayerESPOutline) Draw3DBox(mesh, ImColor(0, 0, 0, 255), Settings::ESPThickness + 2.0f);
                    Draw3DBox(mesh, Color);
                }
                else if (Settings::BoxType == 3) {
                     DrawFilledBox(BoxCenterX - (CornerWidth / 2), boxTopY, CornerWidth, CornerHeight, Color, Settings::ESPThickness);
                     if (Settings::PlayerESPOutline) DrawBox(BoxCenterX - (CornerWidth / 2), boxTopY, CornerWidth, CornerHeight, ImColor(0, 0, 0, 255), 1.5f);
                }
            }

            if (Settings::Username || Settings::Distance) {
                std::string text = "";
                if (Settings::Username) {
                    std::string displayName = CachedP.Name;
                    if (!PlayerDisplayNamePlausible(displayName))
                        displayName.clear();
                    if (CachedP.PlayerState) {
                        const std::string live = TryReadPlayerDisplayName(CachedP.PlayerState);
                        if (PlayerDisplayNamePlausible(live))
                            displayName = live;
                    }
                    if (!PlayerDisplayNamePlausible(displayName) && CachedP.LobbyPreview)
                        displayName = xorstr_("You");
                    else if (!PlayerDisplayNamePlausible(displayName))
                        displayName = xorstr_("Unknown");
                    text += displayName;
                }
                if (Settings::Distance) {
                    if (!text.empty()) text += xorstr_(" ");
                    text += xorstr_("[") + std::to_string(static_cast<int>(Distance)) + xorstr_("m]");
                }

                if (!text.empty()) {
                    ImVec2 textSize = LocalPtrs::GameFont->CalcTextSizeA(Settings::FontSize, FLT_MAX, 0.0f, text.c_str());
                    ImVec2 pos = ImVec2(BoxCenterX - (textSize.x / 2.0f), boxTopY - 25);
                    if (Settings::TextOutline) {
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x-1, pos.y}, ImColor(0,0,0,255), text.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x+1, pos.y}, ImColor(0,0,0,255), text.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x, pos.y-1}, ImColor(0,0,0,255), text.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x, pos.y+1}, ImColor(0,0,0,255), text.c_str());
                    }
                    ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, pos, Settings::TextColor, text.c_str());
                }

            }

            if (Settings::KillESP || Settings::Platform) {
                std::string killText = "";
                ImVec2 killSize = ImVec2(0,0);
                if (Settings::KillESP) {
                    killText = xorstr_("Kills: ") + std::to_string(CachedP.Kills);
                    killSize = LocalPtrs::GameFont->CalcTextSizeA(Settings::FontSize, FLT_MAX, 0.0f, killText.c_str());
                }

                std::string platText = "";
                ImColor platColor = ImColor(255,255,255,255);
                ImVec2 platSize = ImVec2(0,0);
                if (Settings::Platform) {
                    platText = get_player_platform(CachedP.PlayerState, platColor);
                    if (!platText.empty()) {
                        platSize = LocalPtrs::GameFont->CalcTextSizeA(Settings::FontSize, FLT_MAX, 0.0f, platText.c_str());
                    }
                }

                float AbsWidth = CornerWidth;

                if (Settings::KillESP && !killText.empty()) {
                     float killY = boxTopY + (CornerHeight / 2.0f) - (killSize.y / 2.0f);
                     float killX = BoxCenterX - (AbsWidth / 2.0f) - killSize.x - 5.0f;
                     
                      if (Settings::TextOutline) {
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {killX-1, killY}, ImColor(0,0,0,255), killText.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {killX+1, killY}, ImColor(0,0,0,255), killText.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {killX, killY-1}, ImColor(0,0,0,255), killText.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {killX, killY+1}, ImColor(0,0,0,255), killText.c_str());
                    }
                    ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, ImVec2(killX, killY), Settings::TextColor, killText.c_str());

                     if (Settings::Platform && !platText.empty()) {
                         float platY = killY - platSize.y;
                         float platX = BoxCenterX - (AbsWidth / 2.0f) - platSize.x - 5.0f;
                         
                         if (Settings::TextOutline) {
                            ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX-1, platY}, ImColor(0,0,0,255), platText.c_str());
                            ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX+1, platY}, ImColor(0,0,0,255), platText.c_str());
                            ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX, platY-1}, ImColor(0,0,0,255), platText.c_str());
                            ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX, platY+1}, ImColor(0,0,0,255), platText.c_str());
                        }
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, ImVec2(platX, platY), platColor, platText.c_str());
                     }

                } else if (Settings::Platform && !platText.empty()) {
                    float platY = boxTopY + (CornerHeight / 2.0f) - (platSize.y / 2.0f);
                    float platX = BoxCenterX - (AbsWidth / 2.0f) - platSize.x - 5.0f;
                    if (Settings::TextOutline) {
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX-1, platY}, ImColor(0,0,0,255), platText.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX+1, platY}, ImColor(0,0,0,255), platText.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX, platY-1}, ImColor(0,0,0,255), platText.c_str());
                        ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {platX, platY+1}, ImColor(0,0,0,255), platText.c_str());
                    }
                    ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, ImVec2(platX, platY), platColor, platText.c_str());
                }
            }

            if (Settings::WeaponESP || Settings::AmmoESP) {
                 std::string finalString = "";
                 bool isBuilding = false;
                 uint8_t state = Read<uint8_t>(CachedP.Pawn + Offsets::BuildingState);
                 if (state == (uint8_t)EFortBuildingState::Placement || state == (uint8_t)EFortBuildingState::EditMode) {
                     finalString = xorstr_("Building Plan");
                     isBuilding = true;
                 }
                 
                 if (!isBuilding) {
                     uintptr_t CurrentWeapon = Read<uintptr_t>(CachedP.Pawn + Offsets::CurrentWeapon);
                     if (CurrentWeapon) {
                         uintptr_t WeaponData = Read<uintptr_t>(CurrentWeapon + Offsets::WeaponData);
                         if (WeaponData) {
                             std::string WeaponName = "";
                             if (LocalPtrs::WeaponNameCache.find(WeaponData) != LocalPtrs::WeaponNameCache.end()) {
                                 WeaponName = LocalPtrs::WeaponNameCache[WeaponData];
                             } else {
                                 uintptr_t ItemNamePtr = Read<uintptr_t>(WeaponData + Offsets::ItemName);
                                 if (ItemNamePtr) {
                                     uintptr_t FDataPtr = Read<uintptr_t>(ItemNamePtr + Offsets::FData);
                                     if (FDataPtr) {
                                         std::string str = read_wstr(FDataPtr);
                                         if (!str.empty()) {
                                             LocalPtrs::WeaponNameCache[WeaponData] = str;
                                             WeaponName = str;
                                         }
                                     }
                                 }
                             }

                             bool isPickaxe = (WeaponName.find(xorstr_("Harvesting Tool")) != std::string::npos);

                             if (Settings::WeaponESP) {
                                  finalString += WeaponName;
                             }
                             if (Settings::AmmoESP && !isPickaxe) {
                                  int AmmoCount = Read<int>(CurrentWeapon + Offsets::AmmoCount);
                                  if (!finalString.empty()) finalString += xorstr_(" ");
                                  finalString += xorstr_("[") + std::to_string(AmmoCount) + xorstr_("]");
                             }
                         }
                     } else {
                         if (Settings::WeaponESP) finalString = xorstr_("No Weapon Equipped");
                         if (Settings::AmmoESP) {
                             if (!finalString.empty()) finalString += xorstr_(" ");
                             finalString += xorstr_("[0]");
                         }
                     }
                 }

                 if (!finalString.empty()) {
                      ImVec2 textSize = LocalPtrs::GameFont->CalcTextSizeA(Settings::FontSize, FLT_MAX, 0.0f, finalString.c_str());
                      ImVec2 pos = ImVec2(BoxCenterX - (textSize.x / 2.0f), boxBottomY + TextOffset);
                      if (Settings::TextOutline) {
                           ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x-1, pos.y}, ImColor(0,0,0,255), finalString.c_str());
                           ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x+1, pos.y}, ImColor(0,0,0,255), finalString.c_str());
                           ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x, pos.y-1}, ImColor(0,0,0,255), finalString.c_str());
                           ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, {pos.x, pos.y+1}, ImColor(0,0,0,255), finalString.c_str());
                      }
                      ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::FontSize, pos, Settings::TextColor, finalString.c_str());
                      TextOffset += Settings::FontSize; 
                 }
            }
 
            if (Settings::Snapline && !CachedP.LobbyPreview) {
                if (Settings::SnaplinePos == 0) { 
                    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(Settings::Width / 2.0f, Settings::Height), ImVec2(BoxCenterX, boxBottomY + TextOffset), Color, Settings::ESPThickness);
                }
                else if (Settings::SnaplinePos == 1) {
                     ImGui::GetBackgroundDrawList()->AddLine(ImVec2(Settings::Width / 2.0f, Settings::Height / 2.0f), ImVec2(BoxCenterX, boxTopY + CornerHeight * 0.5f), Color, Settings::ESPThickness);
                }
                else if (Settings::SnaplinePos == 2) {
                    float StopY = boxTopY;
                    if (Settings::Username || Settings::Distance) StopY -= 25.0f;
                    ImGui::GetBackgroundDrawList()->AddLine(ImVec2(Settings::Width / 2.0f, 0.0f), ImVec2(BoxCenterX, StopY), Color, Settings::ESPThickness);
                }
            }
 
        if (Settings::Aimbot && !CachedP.LobbyPreview) {
            Vector3 head2d{};
            const Vector3 headW = BoneWorldFromSnapshotData(boneSnap, EBoneIndex::Head);
            if (!ProjectWorldToScreen(headW, &head2d))
                continue;
		auto dist = sqrt(pow(head2d.x - Settings::CenterWidth, 2) + pow(head2d.y - Settings::CenterHeight, 2));
		if (dist < CurrentBestDist && dist < Settings::AimbotFOV) {
            if (Settings::VisCheck && !IsVisible(mesh)) continue;
			CurrentBestDist = dist;
			CurrentBestMesh = mesh;
		}
        }
        }

    LogEspDebug(CachedCount, DrawnCount, FirstMesh);

	static uintptr_t LockedMesh = 0;
    LocalPtrs::closest_distance = CurrentBestDist;
    LocalPtrs::closest_mesh = CurrentBestMesh;

	if (Settings::Aimbot) {
        if (GetAsyncKeyState(Settings::AimKey) & 0x8000) {
            if (LockedMesh) {
                bool Valid = false;
                {
                    std::lock_guard<std::mutex> lock(LocalPtrs::CachedPlayersMutex);
                    for (const auto& p : LocalPtrs::CachedPlayers) {
                        if (p.Mesh != LockedMesh) continue;
                        if (Settings::VisCheck && !IsVisible(p.Mesh)) break;
                        Vector3 Head = GetBoneWithRotation(LockedMesh, 66);
                        Vector3 Head2D;
                        if (ProjectWorldToScreen(Head, &Head2D)) {
                            float dist = GetCrossDistance(Head2D.x, Head2D.y, Settings::Width / 2, Settings::Height / 2);
                            if (dist <= Settings::AimbotFOV) Valid = true;
                        }
                        break;
                    }
                }           
                if (!Valid) LockedMesh = 0; 
            }    
            if (!LockedMesh) {
                 LockedMesh = LocalPtrs::closest_mesh;
            }
            if (LockedMesh) {
                Camera_Aimbot(LockedMesh);
            } else {
                ResetCamera();
            }

        } else {
             LockedMesh = 0;
             ResetCamera();
        }
	} else {
        LockedMesh = 0;
    }

	const char* camRejectSummary = renderCam.rejectionReason ? renderCam.rejectionReason
	                                                         : RenderPipeline::g_CameraAcquireRejectReason;
	RenderPipeline::LogRenderPipelineSummary(Camera::ViewProjectionReady, Camera::Valid,
	                                         camRejectSummary, renderCam);
}
