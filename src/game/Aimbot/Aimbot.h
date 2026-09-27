#pragma once
#include "../../util/math.h"
#include "../../sdk-offsets/sdk.h"

void ResetCamera() {}

void Camera_Aimbot(uintptr_t mesh)
{
    if (!mesh || !LocalPtrs::PlayerController || !Memory::IsValid(LocalPtrs::PlayerController))
        return;
    if (Settings::VisCheck && !IsVisible(mesh)) return;
    const Vector3 head3d = GetBoneWithRotation(mesh, 66);
    if (BoneWorldMissing(head3d) || !Camera::Valid) return;
    const uintptr_t rotAddr = LocalPtrs::PlayerController + Offsets::ControlRotation;
    Vector3 CurrentRot = Read<Vector3>(rotAddr);
    Vector3 TargetRot = CalcAngle(Camera::Location, head3d);
    Vector3 Delta = TargetRot - CurrentRot;
    if (Delta.y > 180.0f) Delta.y -= 360.0f;
    if (Delta.y < -180.0f) Delta.y += 360.0f;
    const float smooth = Settings::Smoothnes < 1.f ? 1.f : Settings::Smoothnes;
    Vector3 SmoothedRot = CurrentRot + (Delta / smooth);
    if (SmoothedRot.x > 89.f) SmoothedRot.x = 89.f;
    if (SmoothedRot.x < -89.f) SmoothedRot.x = -89.f;
    if (SmoothedRot.y > 180.0f) SmoothedRot.y -= 360.0f;
    if (SmoothedRot.y < -180.0f) SmoothedRot.y += 360.0f;
    SmoothedRot.z = 0.f;
    Write<Vector3>(rotAddr, SmoothedRot);
}

void TriggerbotThread() {
    while (true) {
        if (!Settings::Triggerbot || !LocalPtrs::Player) {
             std::this_thread::sleep_for(std::chrono::milliseconds(100));
             continue;
        }
        if (Settings::TriggerbotKey != 0 && !(GetAsyncKeyState(Settings::TriggerbotKey) & 0x8000)) {
             std::this_thread::sleep_for(std::chrono::milliseconds(1));
             continue;
        }
        Vector3 ReticleLoc = Read<Vector3>(LocalPtrs::PlayerController + Offsets::LocationUnderReticle);
        if (ReticleLoc.x == 0 && ReticleLoc.y == 0 && ReticleLoc.z == 0) {
             std::this_thread::sleep_for(std::chrono::milliseconds(1));
             continue;
        }
        std::vector<LocalPtrs::CachedPlayer> LocalCache;
        {
            std::lock_guard<std::mutex> lock(LocalPtrs::CachedPlayersMutex);
            LocalCache = LocalPtrs::CachedPlayers;
        }
        bool shot = false;
        for (const auto& p : LocalCache) {
            if (!p.Mesh) continue;
            Vector3 RootPos = Read<Vector3>(p.RootComponent + Offsets::Realitivelocation);
            float dist = ReticleLoc.Distance(RootPos);
            if (dist < 150.0f) {
                 mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                 std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
                 mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                 if (Settings::TriggerbotDelay > 0)
                 std::this_thread::sleep_for(std::chrono::milliseconds(Settings::TriggerbotDelay));                 
                 shot = true;
                 break;
            }
        }
        if (!shot) std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }
}

