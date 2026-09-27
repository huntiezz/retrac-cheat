#pragma once
#include "../../util/common.h"
#include <unordered_map>
#include "../../sdk-offsets/sdk.h"
#include "../../sdk-offsets/offsets.hpp"
#include "../../util/Settings.h"
#include "../../menu/ImGui/imgui.h"
#include "../Drawing/Draw.h"


std::unordered_map<uintptr_t, std::string> ItemNameCache;

std::string ReadFText(uintptr_t FTextPtr) {
    if (!FTextPtr) return "";
    uintptr_t History = Read<uintptr_t>(FTextPtr);
    if (!History) return "";
    uintptr_t StrData = Read<uintptr_t>(History + 0x28);
    int32_t StrLen = Read<int32_t>(History + 0x30);
    if (StrLen > 0 && StrLen < 64) {
        std::vector<wchar_t> buf(StrLen + 1);
        Memory::ReadVirtual((void*)StrData, buf.data(), StrLen * sizeof(wchar_t));
        std::wstring ws(buf.data());
        return std::string(ws.begin(), ws.end());
    }
    return "";
}



inline bool ActorLooksLikePickup(uintptr_t actor) {
    if (!actor || !Memory::IsValid(actor))
        return false;
    const uintptr_t itemEntry = actor + Offsets::PrimaryPickupItemEntry;
    const uintptr_t itemDef = Read<uintptr_t>(itemEntry + Offsets::ItemDefinition);
    if (!itemDef || !Memory::IsValid(itemDef))
        return false;
    const int count = Read<int>(itemEntry + 0x0C);
    if (count <= 0 || count >= 1000)
        return false;
    return !Read<bool>(actor + Offsets::bPickedUp);
}

inline Vector3 ActorWorldLocation(uintptr_t actor) {
    const uintptr_t root = Read<uintptr_t>(actor + Offsets::RootComponent);
    if (!root || !Memory::IsValid(root))
        return Vector3{};
    return ReadFTransformTranslation(root + Offsets::ComponentToWorld);
}

void ProcessActor(uintptr_t Actor, Vector3 Location, float Dist, std::vector<LocalPtrs::CachedEntity>& TempCache) {
    if (Settings::PickupESP || Settings::RadarLoot) {
        uintptr_t ItemEntryAddr = Actor + Offsets::PrimaryPickupItemEntry;
        uintptr_t ItemDef = Read<uintptr_t>(ItemEntryAddr + Offsets::ItemDefinition);
        if (ItemDef) {
            int Count = Read<int>(ItemEntryAddr + 0x0C);
            if (Count > 0 && Count < 1000) {
                bool isPickedUp = Read<bool>(Actor + Offsets::bPickedUp);
                if (!isPickedUp) {
                    int Rarity = (int)Read<uint8_t>(ItemDef + Offsets::ItemRarity);
                    if (Rarity < Settings::MinRarity) return;
                    std::string Name;
                    if (ItemNameCache.find(ItemDef) != ItemNameCache.end()) {
                        Name = ItemNameCache[ItemDef];
                    } else {
                        Name = ReadFText(ItemDef + Offsets::DisplayName);
                        if (!Name.empty()) {
                            ItemNameCache[ItemDef] = Name;
                        }
                    }
                    if (!Name.empty() && Name != "Item") {
                        LocalPtrs::CachedEntity Entity;
                        Entity.Position = Location;
                        Entity.Name = Name;
                        Entity.Color = GetColorFromRarity(Rarity);
                        Entity.Distance = Dist;
                        TempCache.push_back(Entity);
                        return;
                    }
                }
            }
        }
    }
}


inline void ScanLevelForLoot(uintptr_t level, std::vector<LocalPtrs::CachedEntity>& tempCache,
                             float maxDistMeters) {
    if (!level || !Memory::IsValid(level))
        return;
    const uintptr_t actorsArray = Read<uintptr_t>(level + Offsets::ActorsArray);
    const int actorCount = Read<int>(level + Offsets::ActorsCount);
    if (!actorsArray || !Memory::IsValid(actorsArray) || actorCount <= 0 || actorCount > 10000)
        return;
    const int cap = actorCount > 400 ? 400 : actorCount;
    const int chunkSize = 128;
    std::vector<uintptr_t> chunk(chunkSize);
    for (int i = 0; i < cap; i += chunkSize) {
        const int readCount = (std::min)(chunkSize, cap - i);
        const uintptr_t readAddr = actorsArray + static_cast<uintptr_t>(i) * sizeof(uintptr_t);
        Memory::ReadVirtual((void*)readAddr, chunk.data(), readCount * sizeof(uintptr_t));
        for (int j = 0; j < readCount; ++j) {
            const uintptr_t actor = chunk[j];
            if (!actor || actor == LocalPtrs::Player)
                continue;
            const Vector3 location = ActorWorldLocation(actor);
            if (location.x == 0.f && location.y == 0.f && location.z == 0.f)
                continue;
            const Vector3 pawn = LocalPtrs::Player ? GetActorRootWorldLocation(LocalPtrs::Player)
                                                    : LocalPtrs::relative_location;
            const float dist = (pawn.x != 0.f || pawn.y != 0.f || pawn.z != 0.f)
                                   ? Dist3(pawn, location) / 100.f
                                   : Dist3(Camera::Location, location) / 100.f;
            if (dist > maxDistMeters)
                continue;
            if (!ActorLooksLikePickup(actor))
                continue;
            ProcessActor(actor, location, dist, tempCache);
        }
        if (i % 512 == 0)
            std::this_thread::yield();
    }
}

void WorldLoop() {
    if (!Settings::WorldESP && !Settings::RadarLoot) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return;
    }
    if (!LocalPtrs::Gworld) return;
    static std::vector<LocalPtrs::CachedEntity> TempCache;
    TempCache.clear(); 
    if (TempCache.capacity() < 1000) TempCache.reserve(1000);

    float maxDist = Settings::WorldESPMaxDistance;
    if (Settings::Radar && Settings::RadarLoot) {
        const float radarMaxMeters = Settings::RadarRange / 100.0f;
        if (radarMaxMeters > maxDist) maxDist = radarMaxMeters;
    }

    const uintptr_t world = LocalPtrs::Gworld;
    const uintptr_t persistent = Read<uintptr_t>(world + Offsets::PersistentLevel);
    ScanLevelForLoot(persistent, TempCache, maxDist);
    const uintptr_t levels = Read<uintptr_t>(world + Offsets::Levels);
    const int levelCount = Read<int>(world + Offsets::Levels + sizeof(uintptr_t));
    if (levels && Memory::IsValid(levels) && levelCount > 0 && levelCount <= 64) {
        for (int li = 0; li < levelCount; ++li) {
            const uintptr_t level = Read<uintptr_t>(levels + static_cast<uintptr_t>(li) * sizeof(uintptr_t));
            if (!level || level == persistent)
                continue;
            ScanLevelForLoot(level, TempCache, maxDist);
        }
    }
    {
        std::lock_guard<std::mutex> lock(LocalPtrs::LevelActorsMutex);
        LocalPtrs::LevelActors = TempCache; 
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}

void DrawWorldESP() {
    if (!Settings::WorldESP) return;
    std::lock_guard<std::mutex> lock(LocalPtrs::LevelActorsMutex);
    for (const auto& Entity : LocalPtrs::LevelActors) {
        if (!Settings::PickupESP) continue; 
        if (Entity.Distance > Settings::WorldESPMaxDistance) continue;

        Vector3 ScreenLoc;
        if (ProjectWorldToScreen(Entity.Position, &ScreenLoc)) {
            std::string text = Entity.Name;
            if (Settings::PickupDistance) {
                text += " [" + std::to_string((int)Entity.Distance) + "m]";
            }
            ImVec2 textSize = LocalPtrs::GameFont->CalcTextSizeA(Settings::WorldESPFontSize, FLT_MAX, 0.0f, text.c_str());
            ImVec2 pos = ImVec2(ScreenLoc.x - textSize.x / 2, ScreenLoc.y);
            
            if (Settings::WorldESPTextOutline) {
                ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::WorldESPFontSize, {pos.x - 1, pos.y}, ImColor(0, 0, 0, 255), text.c_str());
                ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::WorldESPFontSize, {pos.x + 1, pos.y}, ImColor(0, 0, 0, 255), text.c_str());
                ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::WorldESPFontSize, {pos.x, pos.y - 1}, ImColor(0, 0, 0, 255), text.c_str());
                ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::WorldESPFontSize, {pos.x, pos.y + 1}, ImColor(0, 0, 0, 255), text.c_str());
            }
            ImGui::GetBackgroundDrawList()->AddText(LocalPtrs::GameFont, Settings::WorldESPFontSize, pos, Entity.Color, text.c_str());
        }
    }
}


inline void World_esp_thread()
{
    while (true)
    {
        // Scanning the level needs a world, not the local pawn.
        if (LocalPtrs::Gworld && (Settings::WorldESP || Settings::RadarLoot))
        {
            WorldLoop();
        }
        else 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}
