#pragma once
#include "common.h"
#include "../util/Settings.h"
#include <ShlObj.h>
#include "obfuscate.h"

namespace Config {
    std::string ConfigFolder = xorstr_("C:\\Configs\\");

    void Setup() {
        if (!std::filesystem::exists(ConfigFolder)) {
            std::filesystem::create_directory(ConfigFolder);
        }
    }

    std::vector<std::string> GetConfigs() {
        std::vector<std::string> configs;
        for (const auto& entry : std::filesystem::directory_iterator(ConfigFolder)) {
            if (entry.path().extension() == xorstr_(".cfg")) {
                configs.push_back(entry.path().stem().string());
            }
        }
        return configs;
    }

    void Save(std::string name) {
        if (name.empty()) return;
        std::ofstream file(ConfigFolder + name + xorstr_(".cfg"));
        if (file.is_open()) {
            file << xorstr_("Aimbot ") << Settings::Aimbot << xorstr_("\n");
            file << xorstr_("VisCheck ") << Settings::VisCheck << xorstr_("\n");
            file << xorstr_("ShowFOV ") << Settings::ShowFOV << xorstr_("\n");
            file << xorstr_("AimKey ") << Settings::AimKey << xorstr_("\n");
            file << xorstr_("Triggerbot ") << Settings::Triggerbot << xorstr_("\n");
            file << xorstr_("TriggerbotKey ") << Settings::TriggerbotKey << xorstr_("\n");
            file << xorstr_("TriggerbotDelay ") << Settings::TriggerbotDelay << xorstr_("\n");
            file << xorstr_("AimbotFOV ") << Settings::AimbotFOV << xorstr_("\n");
            file << xorstr_("Smoothnes ") << Settings::Smoothnes << xorstr_("\n");
            file << xorstr_("Box ") << Settings::Box << xorstr_("\n");
            file << xorstr_("BoxType ") << Settings::BoxType << xorstr_("\n");
            file << xorstr_("WeaponESP ") << Settings::WeaponESP << xorstr_("\n");
            file << xorstr_("AmmoESP ") << Settings::AmmoESP << xorstr_("\n");
            file << xorstr_("Snapline ") << Settings::Snapline << xorstr_("\n");
            file << xorstr_("SnaplinePos ") << Settings::SnaplinePos << xorstr_("\n");
            file << xorstr_("Distance ") << Settings::Distance << xorstr_("\n");
            file << xorstr_("Skeleton ") << Settings::Skeleton << xorstr_("\n");
            file << xorstr_("Username ") << Settings::Username << xorstr_("\n");
            file << xorstr_("FontSize ") << Settings::FontSize << xorstr_("\n");
            file << xorstr_("ESPThickness ") << Settings::ESPThickness << xorstr_("\n");
            file << xorstr_("WorldESP ") << Settings::WorldESP << xorstr_("\n");
            file << xorstr_("PickupESP ") << Settings::PickupESP << xorstr_("\n");
            file << xorstr_("PickupDistance ") << Settings::PickupDistance << xorstr_("\n");
            file << xorstr_("WorldESPFontSize ") << Settings::WorldESPFontSize << xorstr_("\n");
            file << xorstr_("WorldESPMaxDistance ") << Settings::WorldESPMaxDistance << xorstr_("\n");
            file << xorstr_("MinRarity ") << Settings::MinRarity << xorstr_("\n");
            file << xorstr_("WorldESPTextOutline ") << Settings::WorldESPTextOutline << xorstr_("\n");
            file << xorstr_("InstantReload ") << Settings::InstantReload << xorstr_("\n");
            file << xorstr_("NoSpread ") << Settings::NoSpread << xorstr_("\n");
            file << xorstr_("NoRecoil ") << Settings::NoRecoil << xorstr_("\n");
            file << xorstr_("FastPickaxe ") << Settings::FastPickaxe << xorstr_("\n");
            file << xorstr_("TeleportKey ") << Settings::TeleportKey << xorstr_("\n");
            file << xorstr_("TeleportEnemies ") << Settings::TeleportEnemies << xorstr_("\n");
            file << xorstr_("BulletTP ") << Settings::BulletTP << xorstr_("\n");
            file << xorstr_("AimWhileJumping ") << Settings::AimWhileJumping << xorstr_("\n");
            file << xorstr_("MagicBullet ") << Settings::MagicBullet << xorstr_("\n");
            file << xorstr_("InstantCharge ") << Settings::InstantCharge << xorstr_("\n");
            file << xorstr_("FOVChanger ") << Settings::FOVChanger << xorstr_("\n");
            file << xorstr_("FOVChangerValue ") << Settings::FOVChangerValue << xorstr_("\n");
            file << xorstr_("Chams ") << Settings::Chams << xorstr_("\n");
            file << xorstr_("ChamsColor ") << Settings::ChamsColor << xorstr_("\n");
            file << xorstr_("PlayerESPOutline ") << Settings::PlayerESPOutline << xorstr_("\n");
            file << xorstr_("TextOutline ") << Settings::TextOutline << xorstr_("\n");
            file << xorstr_("Platform ") << Settings::Platform << xorstr_("\n");
            file << xorstr_("KillESP ") << Settings::KillESP << xorstr_("\n");
            file << xorstr_("StreamProof ") << Settings::StreamProof << xorstr_("\n");
            
            auto writeColor = [&](const char* name, ImColor col) {
                file << name << xorstr_(" ") << col.Value.x << xorstr_(" ") << col.Value.y << xorstr_(" ") << col.Value.z << xorstr_(" ") << col.Value.w << xorstr_("\n");
            };
            writeColor(xorstr_("VisibleColor"), Settings::VisibleColor);
            writeColor(xorstr_("NVisibleColor"), Settings::NVisibleColor);
            writeColor(xorstr_("TextColor"), Settings::TextColor);
            writeColor(xorstr_("FOVColor"), Settings::FOVColor);
            writeColor(xorstr_("MenuColor"), Settings::MenuColor);

            file << xorstr_("Radar ") << Settings::Radar << xorstr_("\n");
            file << xorstr_("RadarLoot ") << Settings::RadarLoot << xorstr_("\n");
            file << xorstr_("RadarType ") << Settings::RadarType << xorstr_("\n");
            file << xorstr_("RadarX ") << Settings::RadarX << xorstr_("\n");
            file << xorstr_("RadarY ") << Settings::RadarY << xorstr_("\n");
            file << xorstr_("RadarSize ") << Settings::RadarSize << xorstr_("\n");
            file << xorstr_("RadarRange ") << Settings::RadarRange << xorstr_("\n");
            file << xorstr_("RadarBackground ") << Settings::RadarBackground << xorstr_("\n");

            file << xorstr_("ShowFPS ") << Settings::ShowFPS << xorstr_("\n");
            file << xorstr_("VSync ") << Settings::VSync << xorstr_("\n");
            file.close();
        }
    }

    void Delete(std::string name) {
        if (name.empty()) return;
        std::string path = ConfigFolder + name + xorstr_(".cfg");
        if (std::filesystem::exists(path)) {
            std::filesystem::remove(path);
        }
    }

    void Load(std::string name) {
        if (name.empty()) return;
        std::ifstream file(ConfigFolder + name + xorstr_(".cfg"));
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                std::stringstream ss(line);
                std::string key;
                ss >> key;
                if (key == xorstr_("Aimbot")) ss >> Settings::Aimbot;
                else if (key == xorstr_("VisCheck")) ss >> Settings::VisCheck;
                else if (key == xorstr_("ShowFOV")) ss >> Settings::ShowFOV;
                else if (key == xorstr_("AimKey")) ss >> Settings::AimKey;
                else if (key == xorstr_("Triggerbot")) ss >> Settings::Triggerbot;
                else if (key == xorstr_("TriggerbotKey")) ss >> Settings::TriggerbotKey;
                else if (key == xorstr_("TriggerbotDelay")) ss >> Settings::TriggerbotDelay;
                else if (key == xorstr_("AimbotFOV")) ss >> Settings::AimbotFOV;
                else if (key == xorstr_("Smoothnes")) ss >> Settings::Smoothnes;
                else if (key == xorstr_("Box")) ss >> Settings::Box;
                else if (key == xorstr_("BoxType")) ss >> Settings::BoxType;
                else if (key == xorstr_("WeaponESP")) ss >> Settings::WeaponESP;
                else if (key == xorstr_("AmmoESP")) ss >> Settings::AmmoESP;
                else if (key == xorstr_("Snapline")) ss >> Settings::Snapline;
                else if (key == xorstr_("SnaplinePos")) ss >> Settings::SnaplinePos;
                else if (key == xorstr_("Distance")) ss >> Settings::Distance;
                else if (key == xorstr_("Skeleton")) ss >> Settings::Skeleton;
                else if (key == xorstr_("Username")) ss >> Settings::Username;
                else if (key == xorstr_("FontSize")) ss >> Settings::FontSize;
                else if (key == xorstr_("ESPThickness")) ss >> Settings::ESPThickness;
                else if (key == xorstr_("WorldESP")) ss >> Settings::WorldESP;
                else if (key == xorstr_("PickupESP")) ss >> Settings::PickupESP;
                else if (key == xorstr_("PickupDistance")) ss >> Settings::PickupDistance;
                else if (key == xorstr_("WorldESPFontSize")) ss >> Settings::WorldESPFontSize;
                else if (key == xorstr_("WorldESPMaxDistance")) ss >> Settings::WorldESPMaxDistance;
                else if (key == xorstr_("MinRarity")) ss >> Settings::MinRarity;
                else if (key == xorstr_("WorldESPTextOutline")) ss >> Settings::WorldESPTextOutline;
                else if (key == xorstr_("InstantReload")) ss >> Settings::InstantReload;
                else if (key == xorstr_("NoSpread")) ss >> Settings::NoSpread;
                else if (key == xorstr_("NoRecoil")) ss >> Settings::NoRecoil;
                else if (key == xorstr_("FastPickaxe")) ss >> Settings::FastPickaxe;
                else if (key == xorstr_("TeleportKey")) ss >> Settings::TeleportKey;
                else if (key == xorstr_("TeleportEnemies")) ss >> Settings::TeleportEnemies;
                else if (key == xorstr_("BulletTP")) ss >> Settings::BulletTP;
                else if (key == xorstr_("AimWhileJumping")) ss >> Settings::AimWhileJumping;
                else if (key == xorstr_("MagicBullet")) ss >> Settings::MagicBullet;
                else if (key == xorstr_("InstantCharge")) ss >> Settings::InstantCharge;
                else if (key == xorstr_("FOVChanger")) ss >> Settings::FOVChanger; 
                else if (key == xorstr_("FOVChangerValue")) ss >> Settings::FOVChangerValue;
                else if (key == xorstr_("Chams")) ss >> Settings::Chams;
                else if (key == xorstr_("ChamsColor")) ss >> Settings::ChamsColor;
                else if (key == xorstr_("PlayerESPOutline")) ss >> Settings::PlayerESPOutline;
                else if (key == xorstr_("TextOutline")) ss >> Settings::TextOutline;
                else if (key == xorstr_("Platform")) ss >> Settings::Platform;
                else if (key == xorstr_("KillESP")) ss >> Settings::KillESP;
                else if (key == xorstr_("StreamProof")) ss >> Settings::StreamProof;
                else if (key == xorstr_("ShowFPS")) ss >> Settings::ShowFPS;
                else if (key == xorstr_("VSync")) ss >> Settings::VSync;
                else if (key == xorstr_("VisibleColor")) { float r,g,b,a; ss >> r >> g >> b >> a; Settings::VisibleColor = ImColor(r,g,b,a); }
                else if (key == xorstr_("NVisibleColor")) { float r,g,b,a; ss >> r >> g >> b >> a; Settings::NVisibleColor = ImColor(r,g,b,a); }
                else if (key == xorstr_("TextColor")) { float r,g,b,a; ss >> r >> g >> b >> a; Settings::TextColor = ImColor(r,g,b,a); }
                else if (key == xorstr_("FOVColor")) { float r,g,b,a; ss >> r >> g >> b >> a; Settings::FOVColor = ImColor(r,g,b,a); }
                else if (key == xorstr_("MenuColor")) { float r,g,b,a; ss >> r >> g >> b >> a; Settings::MenuColor = ImColor(r,g,b,a); }
            

                else if (key == xorstr_("Radar")) ss >> Settings::Radar;
                else if (key == xorstr_("RadarLoot")) ss >> Settings::RadarLoot;
                else if (key == xorstr_("RadarType")) ss >> Settings::RadarType;
                else if (key == xorstr_("RadarX")) ss >> Settings::RadarX;
                else if (key == xorstr_("RadarY")) ss >> Settings::RadarY;
                else if (key == xorstr_("RadarSize")) ss >> Settings::RadarSize;
                else if (key == xorstr_("RadarRange")) ss >> Settings::RadarRange;
                else if (key == xorstr_("RadarBackground")) ss >> Settings::RadarBackground;
            }
            file.close();
            file.close();

            Settings::InstantReloadReset = false; 
            Settings::NoRecoilReset = false;
            Settings::NoSpreadReset = false;
            Settings::FastPickaxeReset = false;
            Settings::AimWhileJumpingReset = false;
            Settings::MagicBulletReset = false;
            Settings::InstantChargeReset = false;
            Settings::FOVChangerReset = false;
            Settings::ChamsReset = false;
            

         }
    }
}
