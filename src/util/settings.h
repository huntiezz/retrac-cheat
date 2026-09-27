#pragma once

namespace Settings {

	//screen shit
	int Width = GetSystemMetrics(SM_CXSCREEN);
	int Height = GetSystemMetrics(SM_CYSCREEN);
	int CenterWidth = Width / 2;
	int CenterHeight = Height / 2;

	//aimbot
	bool Aimbot = true;
    bool VisCheck = true;
	bool ShowFOV = false;
	int AimKey = 0x02;
    bool Triggerbot = false;
    int TriggerbotKey = 0x14; // CAPS LOCK default? Or just let user set. 0x14 is Caps.
    int TriggerbotDelay = 100;
	float AimbotFOV = 150.f;
	float Smoothnes = 5.f;
    ImColor FOVColor = ImColor(255, 255, 255, 255);
    ImColor MenuColor = ImColor(140, 0, 213, 255);

	//esp
	bool Box = true;
    int BoxType = 0;
	bool WeaponESP = true;
	bool AmmoESP = true;
	bool Snapline = true;
    int SnaplinePos = 0;
	bool Distance = true;
	bool Skeleton = true;
	bool Username = true;
	float FontSize = 16.0f;

    // World ESP
    bool WorldESP = true;
    bool PickupESP = true;
    bool PickupDistance = true;
    float WorldESPFontSize = 14.0f;
    float WorldESPMaxDistance = 150.0f;
    bool WorldESPTextOutline = false;
    int MinRarity = 0;

	//Exploits
	bool InstantReload = false;
	bool InstantReloadReset = true;
	bool NoSpread = false;
	bool NoRecoil = false;
	bool NoRecoilReset = true;

	bool NoSpreadReset = true;

	bool FastPickaxe = false;
	bool FastPickaxeReset = true;
	bool TeleportEnemies = false;
    int TeleportKey = 0;
	bool BulletTP = false;

    bool FOVChanger = false;
    bool FOVChangerReset = true;
    float FOVChangerValue = 120.0f;
    bool AimWhileJumping = false;
    bool AimWhileJumpingReset = true;

    bool InstantCharge = false;
    bool InstantChargeReset = true;
    bool MagicBullet = false;
    bool MagicBulletReset = true;


	//utils
	bool Menu = false;
	bool ShowFPS = true;
    bool DebugLogs = false;
	enum class DebugVerbosity : int { Off = 0, Error = 1, Info = 2, Trace = 3 };
	inline DebugVerbosity DebugLevel = DebugVerbosity::Info;
	inline int EffectiveDebugLevel() {
		if (DebugLogs)
			return static_cast<int>(DebugVerbosity::Trace);
		return static_cast<int>(DebugLevel);
	}
	inline bool DebugAtLeast(DebugVerbosity level) {
		return EffectiveDebugLevel() >= static_cast<int>(level);
	}
    bool W2SDebugDraw = false;
    bool StreamProof = false;
	bool VSync = false;

	//math shit
	float ESPThickness = 1.5f;
    bool PlayerESPOutline = false;
    bool TextOutline = false;

	//colors
	ImColor VisibleColor = ImColor(0, 255, 0, 255);
	ImColor NVisibleColor = ImColor(255, 0, 0, 255);
	ImColor TextColor = ImColor(255, 0, 255, 255);
    
    // Chams
    bool Chams = false;
    bool ChamsReset = true;
    int ChamsColor = 8;
    
    inline bool KillESP = false;
    bool Platform = false;
	
	void* AnimeTexture = nullptr;
    int AnimeWidth = 0;
    int AnimeHeight = 0;

    // Radar
    bool Radar = false;
    bool RadarLoot = false;
    int RadarType = 0; // 0=Dot, 1=Square
    float RadarX = 200.0f;
    float RadarY = 200.0f;
    float RadarSize = 200.0f;
    float RadarRange = 25000.0f;
    bool RadarBackground = true;
}