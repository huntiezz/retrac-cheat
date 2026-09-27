#pragma once
#include "../util/math.h"
#include "offsets.hpp"
#include "../util/settings.h"
#include <math.h>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <iostream>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include "../driver/communication.h"
#include <string>
#include <sstream>
#include <chrono>
#include <fstream>
#include <array>

#define M_PI 3.14159265358979323846264338327950288

inline std::mutex g_ConsoleDiagMutex;

inline void ConsoleDiagLine(const std::string& line) {
	if (line.empty())
		return;
	std::lock_guard<std::mutex> lock(g_ConsoleDiagMutex);
	std::cout << line;
	if (line.back() != '\n')
		std::cout << '\n';
}

struct RpmHeaderDataProbeStats {
	int headerStrictOk = 0;
	int headerStrictFail = 0;
	int headerLooseOk = 0;
	int dataStrictOk = 0;
	int dataStrictFail = 0;
	int dataLooseOk = 0;
};
inline RpmHeaderDataProbeStats gRpmHeaderDataProbeStats{};
inline std::atomic<bool> gRpmHeaderDataProbeDone{false};
inline std::atomic<bool> gRpmHeaderDataProbeSummaryLogged{false};

inline void ResetRpmHeaderDataProbeState() {
	gRpmHeaderDataProbeStats = {};
	gRpmHeaderDataProbeDone.store(false, std::memory_order_relaxed);
	gRpmHeaderDataProbeSummaryLogged.store(false, std::memory_order_relaxed);
}

inline bool RpmHeaderOkButDataBlocked() {
	const auto& s = gRpmHeaderDataProbeStats;
	const bool headerReads = s.headerStrictOk > 0 || s.headerLooseOk > 0;
	const bool dataReads = s.dataStrictOk > 0 || s.dataLooseOk > 0;
	return headerReads && !dataReads;
}

inline void RunRpmHeaderVsDataProbeOnce(uintptr_t moduleBase) {
	if (!moduleBase || gRpmHeaderDataProbeDone.exchange(true, std::memory_order_relaxed))
		return;
	auto sample = [](uintptr_t addr, bool dataBucket) {
		if (!addr)
			return;
		uint64_t strictVal{};
		uint64_t looseVal{};
		const bool strictOk = Memory::Process.ReadRequestOk(addr, strictVal);
		const bool looseOk =
			Memory::Process.ReadUnchecked(addr, &looseVal, static_cast<DWORD>(sizeof(looseVal)));
		if (dataBucket) {
			if (strictOk)
				++gRpmHeaderDataProbeStats.dataStrictOk;
			else
				++gRpmHeaderDataProbeStats.dataStrictFail;
			if (looseOk)
				++gRpmHeaderDataProbeStats.dataLooseOk;
		} else {
			if (strictOk)
				++gRpmHeaderDataProbeStats.headerStrictOk;
			else
				++gRpmHeaderDataProbeStats.headerStrictFail;
			if (looseOk)
				++gRpmHeaderDataProbeStats.headerLooseOk;
		}
	};
	for (uintptr_t off = 0; off < 0x1000; off += 0x100)
		sample(moduleBase + off, false);
	const uintptr_t dataAnchor = moduleBase + Offsets::GObjects;
	for (int i = -8; i <= 8; ++i)
		sample(dataAnchor + static_cast<intptr_t>(i) * 0x100, true);
}

inline void LogRpmHeaderVsDataProbeSummaryOnce() {
	if (gRpmHeaderDataProbeSummaryLogged.exchange(true, std::memory_order_relaxed))
		return;
	const auto& s = gRpmHeaderDataProbeStats;
	std::cout << xorstr_("[+] RPM probe header strict ok/fail ")
	          << std::dec << s.headerStrictOk << xorstr_("/") << s.headerStrictFail
	          << xorstr_(" loose ok ") << s.headerLooseOk << xorstr_(" | .data@GObjects strict ok/fail ")
	          << s.dataStrictOk << xorstr_("/") << s.dataStrictFail << xorstr_(" loose ok ")
	          << s.dataLooseOk;
	if (Memory::Process.ReadOnlyAttach)
		std::cout << xorstr_(" | attach=read-only");
	std::cout << std::endl;
	std::cout.flush();
}

int Width = GetSystemMetrics(SM_CXSCREEN);
int Height = GetSystemMetrics(SM_CYSCREEN);
int CenterWidth = Width / 2;
int CenterHeight = Height / 2;

inline void UpdateViewportSize(int w, int h) {
	if (w <= 0 || h <= 0)
		return;
	Settings::Width = w;
	Settings::Height = h;
	Settings::CenterWidth = w / 2;
	Settings::CenterHeight = h / 2;
	Width = w;
	Height = h;
	CenterWidth = w / 2;
	CenterHeight = h / 2;
}

struct FMinimalViewInfo final {
public:
	Vector3                                Location;
	Vector3                                Rotation;
	float                                  FOV;
	float                                  DesiredFOV;
	float                                  AspectRatio;
};

struct FCameraCacheEntry final {
public:
	float                                  Timestamp;
	uint8_t                                Pad_4[0xC];
	FMinimalViewInfo                       POV;
};

// Layout from project SDK structs (not guessed at runtime).
namespace PcmSdkLayout {
inline constexpr uint32_t kCacheEntryTimestamp = 0x0;
inline constexpr uint32_t kCacheEntryPov = 0x10; // offsetof(FCameraCacheEntry, POV)
inline constexpr uint32_t kPovLocation = 0x0;
inline constexpr uint32_t kPovRotation = 0xC;
inline constexpr uint32_t kPovFov = 0x18;
inline constexpr uint32_t kPovDesiredFov = 0x1C;
inline constexpr uint32_t kPovAspectRatio = 0x20;
inline constexpr uint32_t kCacheEntrySize = 0x10 + 0x24; // entry header + minimal POV used here
} // namespace PcmSdkLayout

enum class EFortWeaponTriggerType : uint8_t {
    OnPress = 0,
    Automatic = 1,
    OnRelease = 2,
    OnPressAndRelease = 3,
    EFortWeaponTriggerType_MAX = 4,
};

enum class EMovementMode : uint8_t {
    MOVE_None = 0,
    MOVE_Walking = 1,
    MOVE_NavWalking = 2,
    MOVE_Falling = 3,
    MOVE_Swimming = 4,
    MOVE_Flying = 5,
    MOVE_Custom = 6,
    MOVE_MAX = 7,
};

enum class EFortBuildingState : uint8_t
{
	Placement                                = 0,
	EditMode                                 = 1,
	None                                     = 2,
	EFortBuildingState_MAX                   = 3,
};

enum class EFortRarity : uint8_t
{
	Common                                   = 0,
	Uncommon                                 = 1,
	Rare                                     = 2,
	Epic                                     = 3,
	Legendary                                = 4,
	Mythic                                   = 5,
	Transcendent                             = 6,
	Unattainable                             = 7,
	NumRarityValues                          = 8,
	EFortRarity_MAX                          = 9,
};

enum class EFortPickupSpawnSource : uint8_t
{
	Unset                                    = 0,
	PlayerElimination                        = 1,
	Chest                                    = 2,
	SupplyDrop                               = 3,
	AmmoBox                                  = 4,
	Drone                                    = 5,
	ItemSpawner                              = 6,
	EFortPickupSpawnSource_MAX               = 7,
};

enum EBoneIndex : int {
	Root = 0,
	Attach = 1,
	Pelvis = 2,
	Spine_01 = 3,
	Spine_02 = 4,
	Spine_03 = 5,
	Spine_04 = 6,
	Spine_05 = 7,
	Clavicle_L = 8,
	UpperArm_L = 9,
	LowerArm_L = 10,
	Hand_L = 11,
	Index_Metacarpal_L = 12,
	Index_01_L = 13,
	Index_02_L = 14,
	Index_03_L = 15,
	Middle_Metacarpal_L = 16,
	Middle_01_L = 17,
	Middle_02_L = 18,
	Middle_03_L = 19,
	Pinky_Metacarpal_L = 20,
	Pinky_01_L = 21,
	Pinky_02_L = 22,
	Pinky_03_L = 23,
	Ring_Metacarpal_L = 24,
	Ring_01_L = 25,
	Ring_02_L = 26,
	Ring_03_L = 27,
	Thumb_01_L = 28,
	Thumb_02_L = 29,
	Thumb_03_L = 30,
	Weapon_L = 31,
	LowerArm_Twist_01_L = 32,
	LowerArm_Twist_02_L = 33,
	UpperArm_Twist_01_L = 34,
	UpperArm_Twist_02_L = 35,
	Clavicle_R = 36,
	UpperArm_R = 37,
	LowerArm_R = 38,
	Hand_R = 39,
	Index_Metacarpal_R = 40,
	Index_01_R = 41,
	Index_02_R = 42,
	Index_03_R = 43,
	Middle_Metacarpal_R = 44,
	Middle_01_R = 45,
	Middle_02_R = 46,
	Middle_03_R = 47,
	Pinky_Metacarpal_R = 48,
	Pinky_01_R = 49,
	Pinky_02_R = 50,
	Pinky_03_R = 51,
	Ring_Metacarpal_R = 52,
	Ring_01_R = 53,
	Ring_02_R = 54,
	Ring_03_R = 55,
	Thumb_01_R = 56,
	Thumb_02_R = 57,
	Thumb_03_R = 58,
	Weapon_R = 59,
	LowerArm_Twist_01_R = 60,
	LowerArm_Twist_02_R = 61,
	UpperArm_Twist_01_R = 62,
	UpperArm_Twist_02_R = 63,
	Neck_01 = 64,
	Neck_02 = 65,
	Head = 66,
	Thigh_L = 67,
	Calf_L = 68,
	Calf_Twist_01_L = 69,
	Calf_Twist_02_L = 70,
	Foot_L = 71,
	Ball_L = 72,
	Thigh_Twist_01_L = 73,
	Thigh_R = 74,
	Calf_R = 75,
	Calf_Twist_01_R = 76,
	Calf_Twist_02_R = 77,
	Foot_R = 78,
	Ball_R = 79,
	Thigh_Twist_01_R = 80,
	IK_Foot_Root = 81,
	IK_Foot_L = 82,
	IK_Foot_R = 83,
	IK_Hand_Root = 84,
	IK_Hand_Gun = 85,
	IK_Hand_L = 86,
	IK_Hand_R = 87,
	VB_Spine_05_Weapon_R = 88,
	VB_VB_Spine_05_Weapon_R_IK_Hand_Gun = 89,
	VB_VB_Spine_05_Weapon_R_IK_Hand_L = 90,
	VB_Spine_05_UpperArm_R = 91,
	VB_VB_Spine_05_UpperArm_R_LowerArm_R = 92,
	VB_VB_VB_Spine_05_UpperArm_R_LowerArm_R_Hand_R = 93,
	VB_IK_Foot_Root_Weapon_L = 94,
	VB_Root_Prop = 95,
	VB_Head_FX = 96,
	VB_Root_Hand_L = 97,
	b = 98
};


namespace Camera {
	Vector3 Rotation;
	Vector3 Location;
	float FOV;
	float AspectRatio = 0.f; // 0 => viewport width/height
	Vector3 vAxisX, vAxisY, vAxisZ;
	float FovCoef;
	D3DMATRIX ViewMatrix{};
	D3DMATRIX ProjectionMatrix{};
	D3DMATRIX ViewProjectionMatrix{};
	float ViewRectMinX = 0.f;
	float ViewRectMinY = 0.f;
	float ViewRectWidth = 0.f;
	float ViewRectHeight = 0.f;
	bool ViewProjectionReady = false;
	// False until a POV has been read this frame. Everything that projects must respect it:
	// a retained POV from the previous map draws the whole skeleton at the wrong screen spot.
	bool Valid = false;
	uintptr_t World = 0;
	const char* Source = "none";
	// Byte offset of FMinimalViewInfo inside CameraPovObject (UWorld, PC, or PCM).
	uint32_t SourceOffset = 0;
};


namespace LocalPtrs {
	inline float LastFireTime;
	inline float LastFireTimeVerified;
	inline bool PlayerReloading;
	inline uintptr_t PlayerWeapon;
	inline uintptr_t PlayerMesh;
	inline uintptr_t Player;
	inline uintptr_t Mesh;
	inline uintptr_t PlayerState;
	inline uintptr_t RootComponent;
	inline uintptr_t PlayerCam;
	inline uintptr_t LocalPlayers;
	inline uintptr_t GameInstance;
	inline uintptr_t PlayerController;
	inline uintptr_t Gworld;
	inline Vector3 relative_location;
	inline uintptr_t closest_mesh;
	inline float closest_distance;
    inline uintptr_t GameState;
    inline uintptr_t PlayerArray;
    inline int PlayerArrayCount;
	inline ImFont* GameFont;
    inline std::vector<uintptr_t> chams_meshes;
    inline std::mutex chams_mutex;

    struct CachedPlayer {
        uintptr_t Pawn;
        uintptr_t Mesh;
        uintptr_t PlayerState;
        uintptr_t RootComponent;
        std::string Name;
        int Kills = 0;
        bool LobbyPreview = false;
    };

    struct CachedEntity {
        Vector3 Position;
        std::string Name;
        ImColor Color;
        float Distance;
    };

    inline std::vector<CachedPlayer> CachedPlayers;
    inline std::mutex CachedPlayersMutex;
    inline std::vector<CachedEntity> LevelActors;
    inline std::mutex LevelActorsMutex;
    inline std::unordered_map<uintptr_t, std::string> WeaponNameCache;
}


inline Vector3 GetMeshWorldLocation(uintptr_t mesh) {
	if (!mesh || !Memory::IsValid(mesh))
		return Vector3{};
	FTransform componentToWorld = Read<FTransform>(mesh + Offsets::ComponentToWorld);
	return componentToWorld.translation;
}

// FTransform: FQuat at +0x00 (identity is 0,0,0,1), translation at +0x10, scale at +0x20.
inline Vector3 ReadFTransformTranslation(uintptr_t transformAddr) {
	return Read<Vector3>(transformAddr + 0x10);
}

inline bool BoneWorldMissing(const Vector3& v) {
	return v.x == 0.f && v.y == 0.f && v.z == 0.f;
}

inline float Vec3Distance(const Vector3& a, const Vector3& b) {
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	const float dz = a.z - b.z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

inline Vector3 GetActorRootWorldLocation(uintptr_t pawn) {
	if (!pawn || !Memory::IsValid(pawn))
		return Vector3{};
	const uintptr_t root = Read<uintptr_t>(pawn + Offsets::RootComponent);
	if (!root || !Memory::IsValid(root))
		return Vector3{};
	return ReadFTransformTranslation(root + Offsets::ComponentToWorld);
}

inline bool IsPlausibleUObject(uintptr_t p);
inline bool QuatNormalized(const FQuat& q);
inline bool FiniteVec3(const Vector3& v);
inline float CameraLocationMaxAbs(const Vector3& location);

struct MeshTransformSnapshot {
	bool valid = false;
	const char* source = "none";
	const char* rejectionReason = nullptr;
	FTransform c2w{};
	Vector3 translation{};
};

inline MeshTransformSnapshot DiagnoseComponentToWorld(uintptr_t component) {
	MeshTransformSnapshot snap{};
	if (!component || !IsPlausibleUObject(component)) {
		snap.rejectionReason = "c2w_read_failed";
		return snap;
	}
	if (!Memory::Process.ReadRequestOk(component + Offsets::ComponentToWorld, snap.c2w)) {
		snap.rejectionReason = "c2w_read_failed";
		return snap;
	}
	if (!QuatNormalized(snap.c2w.rot)) {
		snap.rejectionReason = "c2w_quat_invalid";
		return snap;
	}
	snap.translation = snap.c2w.translation;
	if (!FiniteVec3(snap.translation)) {
		snap.rejectionReason = "c2w_translation_zero";
		return snap;
	}
	if (BoneWorldMissing(snap.translation)) {
		snap.rejectionReason = "c2w_translation_zero";
		return snap;
	}
	const float span = CameraLocationMaxAbs(snap.translation);
	if (span <= 100.f || span > 5e7f) {
		snap.rejectionReason = "c2w_out_of_range";
		return snap;
	}
	snap.valid = true;
	snap.source = "mesh_c2w";
	return snap;
}

inline MeshTransformSnapshot ResolveMeshTransformSnapshot(uintptr_t mesh, uintptr_t pawnForRootFallback = 0) {
	MeshTransformSnapshot snap = DiagnoseComponentToWorld(mesh);
	if (snap.valid)
		return snap;
	if (!pawnForRootFallback)
		pawnForRootFallback = LocalPtrs::Player;
	const uintptr_t root = Read<uintptr_t>(pawnForRootFallback + Offsets::RootComponent);
	if (root && Memory::IsValid(root)) {
		MeshTransformSnapshot rootSnap = DiagnoseComponentToWorld(root);
		if (rootSnap.valid) {
			rootSnap.source = "root_c2w_fallback";
			return rootSnap;
		}
	}
	return snap;
}

// Humanoid bones stay within a few meters of the mesh component origin (UE cm).
inline bool BoneWorldPlausibleVsMesh(const Vector3& boneWorld, const Vector3& meshOrigin,
                                     const char** outReason = nullptr) {
	if (BoneWorldMissing(boneWorld)) {
		if (outReason)
			*outReason = "world_zero";
		return false;
	}
	if (BoneWorldMissing(meshOrigin)) {
		if (outReason)
			*outReason = "c2w_translation_zero";
		return false;
	}
	if (Vec3Distance(boneWorld, meshOrigin) > 2500.f) {
		if (outReason)
			*outReason = "world_implausible_vs_anchor";
		return false;
	}
	return true;
}

namespace RenderPipeline {

enum class BonePipelineVerdict : uint8_t {
	INVALID_UNINITIALIZED = 0,
	INVALID_TORN_SNAPSHOT,
	INVALID_TRANSFORM,
	INVALID_BONE_MAPPING,
	VALID_STABLE,
	VALID_ANIMATING,
};

inline const char* BonePipelineVerdictString(BonePipelineVerdict v) {
	switch (v) {
	case BonePipelineVerdict::VALID_STABLE:
		return "VALID_STABLE";
	case BonePipelineVerdict::VALID_ANIMATING:
		return "VALID_ANIMATING";
	case BonePipelineVerdict::INVALID_TORN_SNAPSHOT:
		return "INVALID_TORN_SNAPSHOT";
	case BonePipelineVerdict::INVALID_TRANSFORM:
		return "INVALID_TRANSFORM";
	case BonePipelineVerdict::INVALID_BONE_MAPPING:
		return "INVALID_BONE_MAPPING";
	default:
		return "INVALID_UNINITIALIZED";
	}
}

inline bool BoneVerdictConsumable(BonePipelineVerdict v) {
	return v == BonePipelineVerdict::VALID_STABLE || v == BonePipelineVerdict::VALID_ANIMATING;
}

inline bool BoneVerdictSameForLog(BonePipelineVerdict a, BonePipelineVerdict b) {
	if (a == b)
		return true;
	return BoneVerdictConsumable(a) && BoneVerdictConsumable(b);
}

inline std::atomic<bool> g_LocalBonesValid{ false };
inline std::atomic<const char*> g_LocalBonesReject{ "unknown" };
inline std::atomic<uint8_t> g_LocalBoneVerdict{
    static_cast<uint8_t>(BonePipelineVerdict::INVALID_UNINITIALIZED) };

inline void UpdateLocalBonePipeline(BonePipelineVerdict verdict, const char* detailReason) {
	g_LocalBoneVerdict.store(static_cast<uint8_t>(verdict), std::memory_order_relaxed);
	const bool ok = BoneVerdictConsumable(verdict);
	g_LocalBonesValid.store(ok, std::memory_order_relaxed);
	g_LocalBonesReject.store(ok ? BonePipelineVerdictString(verdict)
	                            : (detailReason ? detailReason : BonePipelineVerdictString(verdict)),
	                         std::memory_order_relaxed);
}

inline void UpdateLocalBoneState(bool valid, const char* reason) {
	UpdateLocalBonePipeline(
	    valid ? BonePipelineVerdict::VALID_ANIMATING : BonePipelineVerdict::INVALID_TRANSFORM,
	    reason);
}
} // namespace RenderPipeline

inline bool IsPlausibleUObject(uintptr_t p) {
	if (!p || (p & 0x7) != 0)
		return false;
	if (!Memory::IsValid(p))
		return false;
	const uintptr_t image = GlobalImageBase();
	// ponytail: UObject instances are heap allocations, not addresses inside the game PE
	if (image && p >= image && p < image + 0x08000000u)
		return false;
	return true;
}

inline float TranslationLength(const Vector3& t) {
	return sqrtf(t.x * t.x + t.y * t.y + t.z * t.z);
}

inline float Absf(float v) {
	return v < 0.f ? -v : v;
}

struct FUeTArrayHeader {
	uintptr_t Data;
	int32_t Num;
	int32_t Max;
};

inline bool ReadUeTArrayHeader(uintptr_t arrayAddr, FUeTArrayHeader& out) {
	out = {};
	if (!arrayAddr || !Memory::IsValid(arrayAddr))
		return false;
	return Memory::Process.ReadRequestOk(arrayAddr, out);
}

inline bool UeTArrayHeaderPlausible(const FUeTArrayHeader& hdr) {
	if (hdr.Num < 20 || hdr.Num > 1024 || hdr.Max < hdr.Num || hdr.Max > 4096)
		return false;
	if (!hdr.Data || !IsPlausibleUObject(hdr.Data) || (hdr.Data & 0xF))
		return false;
	return true;
}

struct ResolvedBoneArray {
	uintptr_t Data = 0;
	int Num = 0;
	uint32_t MeshOffset = 0;
	bool UsesBoneSpace = false;
};

inline bool QuatNormalized(const FQuat& q) {
	const float len2 = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
	return len2 > 0.9f && len2 < 1.1f;
}

// A pose buffer is a TArray<FTransform> of unit quaternions with finite translations.
// The component-space copy spreads its bones over the whole character (head, hands and
// spine all sit far from the component origin); the bone-space copy holds parent-relative
// offsets that stay short, so a majority vote on translation length tells them apart.
inline bool PoseSpreadIsComponentSpace(int sampled, int spread) {
	return sampled >= 8 && spread * 2 > sampled;
}

inline bool PoseSpreadSelfCheck() {
	return PoseSpreadIsComponentSpace(16, 13)   // component space: most bones far from origin
	    && !PoseSpreadIsComponentSpace(16, 2)   // bone space: only the pelvis reaches out
	    && !PoseSpreadIsComponentSpace(4, 4);   // too few bones to call it a skeleton
}

inline bool PoseBufferIsComponentSpace(uintptr_t data, int num) {
	if (!data || !Memory::IsValid(data))
		return false;
	const int step = num > 16 ? num / 16 : 1;
	int sampled = 0;
	int spread = 0;
	for (int i = 0; i < num; i += step) {
		FTransform bone{};
		if (!Memory::Process.ReadRequestOk(data + static_cast<uintptr_t>(i) * 0x30, bone))
			return false;
		if (!QuatNormalized(bone.rot))
			return false;
		const float len = TranslationLength(bone.translation);
		if (!(len < 10000.f)) // NaN fails this too
			return false;
		++sampled;
		if (len > 40.f)
			++spread;
	}
	return PoseSpreadIsComponentSpace(sampled, spread);
}

struct CachedBonePoseSite {
	uint32_t offset = 0;
	bool usesBoneSpace = false;
};

// Per-mesh pose site: offset + space (component vs bone) so RPM hiccups do not flip modes.
inline std::unordered_map<uintptr_t, CachedBonePoseSite> BonePoseSiteByMesh;
inline std::mutex BonePoseOffsetMutex;
inline std::atomic<unsigned long long> BonePoseNextScanMs{ 0 };
inline std::atomic<unsigned> EspFrameCounter{ 0 };

inline uint32_t ScanForPoseOffset(uintptr_t component, int* outNum = nullptr) {
	if (!component || !Memory::IsValid(component))
		return 0;
	for (uint32_t offset = 0x100; offset <= 0x1800; offset += 8) {
		FUeTArrayHeader hdr{};
		if (!ReadUeTArrayHeader(component + offset, hdr))
			continue;
		if (hdr.Num < 20 || hdr.Num > 1024 || hdr.Max < hdr.Num || hdr.Max > 4096)
			continue;
		if (!hdr.Data || !Memory::IsValid(hdr.Data) || (hdr.Data & 0xF))
			continue;
		if (!PoseBufferIsComponentSpace(hdr.Data, hdr.Num))
			continue;
		if (outNum)
			*outNum = hdr.Num;
		return offset;
	}
	return 0;
}

inline uint32_t FindBonePoseOffset(uintptr_t mesh) {
	if (!mesh || !IsPlausibleUObject(mesh))
		return 0;
	{
		std::lock_guard<std::mutex> lock(BonePoseOffsetMutex);
		const auto it = BonePoseSiteByMesh.find(mesh);
		if (it != BonePoseSiteByMesh.end() && it->second.offset) {
			FUeTArrayHeader hdr{};
			if (ReadUeTArrayHeader(mesh + it->second.offset, hdr) && UeTArrayHeaderPlausible(hdr))
				return it->second.offset;
			BonePoseSiteByMesh.erase(it);
		}
	}
	const unsigned long long now = GetTickCount64();
	if (now < BonePoseNextScanMs.load(std::memory_order_relaxed))
		return 0;
	BonePoseNextScanMs.store(now + 500, std::memory_order_relaxed);
	const uint32_t offset = ScanForPoseOffset(mesh);
	if (offset) {
		std::lock_guard<std::mutex> lock(BonePoseOffsetMutex);
		BonePoseSiteByMesh[mesh] = { offset, false };
	}
	return offset;
}

// Debug aid: if no offset on the pawn's Mesh holds a pose, the Mesh offset itself is wrong.
// Report which pawn field does point at a component that has one.
inline uint32_t FindPawnMeshOffset(uintptr_t pawn, uint32_t& outPoseOffset, int& outNum) {
	outPoseOffset = 0;
	outNum = 0;
	if (!pawn || !Memory::IsValid(pawn))
		return 0;
	for (uint32_t offset = 0x100; offset <= 0x800; offset += 8) {
		const uintptr_t component = Read<uintptr_t>(pawn + offset);
		if (!component || !Memory::IsValid(component) || (component & 0xF))
			continue;
		// USceneComponent check first: ComponentToWorld must hold a unit quat.
		FTransform c2w{};
		if (!Memory::Process.ReadRequestOk(component + Offsets::ComponentToWorld, c2w))
			continue;
		if (!QuatNormalized(c2w.rot))
			continue;
		const uint32_t poseOffset = ScanForPoseOffset(component, &outNum);
		if (!poseOffset)
			continue;
		outPoseOffset = poseOffset;
		return offset;
	}
	return 0;
}

inline bool PoseBufferLooksLikeBoneSpace(uintptr_t data, int num) {
	if (!data || num < 20)
		return false;
	const int step = num > 16 ? num / 16 : 1;
	int sampled = 0;
	int spread = 0;
	for (int i = 0; i < num; i += step) {
		FTransform bone{};
		if (!Memory::Process.ReadRequestOk(data + static_cast<uintptr_t>(i) * 0x30, bone))
			return false;
		if (!QuatNormalized(bone.rot))
			return false;
		const float len = TranslationLength(bone.translation);
		if (!(len < 500.f))
			return false;
		++sampled;
		if (len > 40.f)
			++spread;
	}
	return sampled >= 8 && !PoseSpreadIsComponentSpace(sampled, spread);
}

inline const char* BoneRawReject(const FTransform& t);
inline const char* ClassifyPoseTranslationSpace(const FTransform& pelvis, const FTransform& spine,
                                                const FTransform& head);
inline bool ClassifyPoseArrayComponentSpace(const ResolvedBoneArray& pose, bool& outComponentSpace);

// Classify Retrac TArray slots: 0x488 component-space pose only (0x6B8 is parent-local, not for W2S).
inline bool TryReadRetracClassifiedPoseSlot(uintptr_t mesh, uint32_t offset, ResolvedBoneArray& out) {
	if (offset == static_cast<uint32_t>(Offsets::BonePosePad))
		return false;
	if (offset != 0x488u && offset != static_cast<uint32_t>(Offsets::BoneCache))
		return false;
	out = {};
	if (!mesh || !IsPlausibleUObject(mesh))
		return false;
	FUeTArrayHeader hdr{};
	if (!ReadUeTArrayHeader(mesh + offset, hdr) || !UeTArrayHeaderPlausible(hdr))
		return false;
	if (hdr.Num <= EBoneIndex::Head)
		return false;
	FTransform probe{};
	if (!Memory::Process.ReadRequestOk(hdr.Data, probe) || !QuatNormalized(probe.rot))
		return false;
	FTransform pelvis{}, spine{}, head{};
	if (!Memory::Process.ReadRequestOk(
	        hdr.Data + static_cast<uintptr_t>(EBoneIndex::Pelvis) * 0x30u, pelvis) ||
	    !Memory::Process.ReadRequestOk(
	        hdr.Data + static_cast<uintptr_t>(EBoneIndex::Spine_05) * 0x30u, spine) ||
	    !Memory::Process.ReadRequestOk(
	        hdr.Data + static_cast<uintptr_t>(EBoneIndex::Head) * 0x30u, head))
		return false;
	if (BoneRawReject(pelvis) || BoneRawReject(spine) || BoneRawReject(head))
		return false;
	const char* space = ClassifyPoseTranslationSpace(pelvis, spine, head);
	if (std::strcmp(space, "mixed_or_unknown") == 0)
		return false;
	out.Data = hdr.Data;
	out.Num = hdr.Num;
	out.MeshOffset = offset;
	out.UsesBoneSpace = (std::strcmp(space, "local_space") == 0);
	return true;
}

inline bool RetracComponentPoseSlotSelfCheck() {
	return static_cast<uint32_t>(Offsets::BonePosePad) == 0x6B8u &&
	       static_cast<uint32_t>(Offsets::BoneCache) == 0x730u &&
	       static_cast<uint32_t>(Offsets::BoneArray) == 0x720u;
}

inline bool TryReadPoseAt(uintptr_t mesh, uint32_t offset, ResolvedBoneArray& out, bool relaxed) {
	out = {};
	if (!mesh || !IsPlausibleUObject(mesh) || !offset)
		return false;
	FUeTArrayHeader hdr{};
	if (!ReadUeTArrayHeader(mesh + offset, hdr) || !UeTArrayHeaderPlausible(hdr))
		return false;
	FTransform probe{};
	if (!Memory::Process.ReadRequestOk(hdr.Data, probe) || !QuatNormalized(probe.rot))
		return false;
	const bool component = PoseBufferIsComponentSpace(hdr.Data, hdr.Num);
	if (!component) {
		if (offset == 0x488u && hdr.Num >= 30) {
			// Retrac live pose at 0x488 when spread classifier rejects it
		} else if (relaxed && offset == static_cast<uint32_t>(Offsets::BonePosePad)) {
			return false;
		} else if (relaxed && PoseBufferLooksLikeBoneSpace(hdr.Data, hdr.Num)) {
			out.UsesBoneSpace = true;
		} else if (relaxed && offset == static_cast<uint32_t>(Offsets::BoneArray)) {
			out.UsesBoneSpace = true;
		} else {
			return false;
		}
	}
	out.Data = hdr.Data;
	out.Num = hdr.Num;
	out.MeshOffset = offset;
	return true;
}

inline bool TryReadComponentSpacePoseAt(uintptr_t mesh, uint32_t offset, ResolvedBoneArray& out) {
	ResolvedBoneArray tmp{};
	if (!TryReadPoseAt(mesh, offset, tmp, false))
		return false;
	if (tmp.UsesBoneSpace)
		return false;
	out = tmp;
	return true;
}

inline void RememberBonePoseSite(uintptr_t mesh, const ResolvedBoneArray& pose) {
	if (!mesh || !pose.MeshOffset || pose.UsesBoneSpace)
		return;
	if (pose.MeshOffset != 0x488u)
		return;
	std::lock_guard<std::mutex> lock(BonePoseOffsetMutex);
	BonePoseSiteByMesh[mesh] = { pose.MeshOffset, false };
}

inline bool ResolveBonePoseArrayAt(uintptr_t mesh, uint32_t offset, ResolvedBoneArray& out) {
	if (TryReadRetracClassifiedPoseSlot(mesh, offset, out))
		return true;
	if (TryReadComponentSpacePoseAt(mesh, offset, out))
		return true;
	ResolvedBoneArray relaxed{};
	if (TryReadPoseAt(mesh, offset, relaxed, true)) {
		out = relaxed;
		return true;
	}
	return false;
}

// Retrac: component-space animated pose at mesh+0x488 (do not use BonePosePad for world bones).
inline bool ResolveBonePoseArray(uintptr_t mesh, ResolvedBoneArray& out) {
	out = {};
	if (!IsPlausibleUObject(mesh))
		return false;
	{
		std::lock_guard<std::mutex> lock(BonePoseOffsetMutex);
		const auto it = BonePoseSiteByMesh.find(mesh);
		if (it != BonePoseSiteByMesh.end() && it->second.offset) {
			if (ResolveBonePoseArrayAt(mesh, it->second.offset, out) &&
			    out.MeshOffset == it->second.offset &&
			    out.UsesBoneSpace == it->second.usesBoneSpace)
				return true;
			BonePoseSiteByMesh.erase(it);
		}
	}
	static const uint32_t kPrefer[] = {
	    0x488u,
	    static_cast<uint32_t>(Offsets::BoneCache),
	    static_cast<uint32_t>(Offsets::BoneArray),
	};
	for (uint32_t off : kPrefer) {
		if (!ResolveBonePoseArrayAt(mesh, off, out))
			continue;
		bool componentSpace = false;
		if (off == 0x488u && ClassifyPoseArrayComponentSpace(out, componentSpace) && componentSpace) {
			RememberBonePoseSite(mesh, out);
			return true;
		}
		if (off != 0x488u && !out.UsesBoneSpace) {
			return true;
		}
	}
	return false;
}

inline bool TryLoadRefSkeletonParents(uintptr_t skelMeshComponent, int boneCount,
                                      std::vector<int32_t>& outParents) {
	outParents.clear();
	if (!skelMeshComponent || !IsPlausibleUObject(skelMeshComponent) || boneCount < 2)
		return false;
	static const uint32_t kSkelMeshOff[] = { 0x480u, 0x4A0u, 0x498u, 0x4A8u, 0x510u };
	for (uint32_t smOff : kSkelMeshOff) {
		const uintptr_t skelAsset = Read<uintptr_t>(skelMeshComponent + smOff);
		if (!IsPlausibleUObject(skelAsset))
			continue;
		for (uint32_t refOff = 0x10u; refOff <= 0x90u; refOff += 8u) {
			FUeTArrayHeader hdr{};
			if (!ReadUeTArrayHeader(skelAsset + refOff, hdr) || hdr.Num != boneCount ||
			    !UeTArrayHeaderPlausible(hdr))
				continue;
			std::vector<int32_t> trial(static_cast<size_t>(boneCount), -2);
			int good = 0;
			for (int i = 0; i < boneCount; ++i) {
				const int32_t parent =
				    Read<int32_t>(hdr.Data + static_cast<uintptr_t>(i) * 0x10u + 0x8u);
				if (parent >= -1 && parent < boneCount) {
					trial[static_cast<size_t>(i)] = parent;
					++good;
				}
			}
			if (good < boneCount - 2)
				continue;
			if (trial[0] != -1 && trial[0] != 0)
				continue;
			outParents = std::move(trial);
			return true;
		}
	}
	return false;
}

inline const std::vector<int32_t>* GetCachedRefSkeletonParents(uintptr_t skelMeshComponent,
                                                               int boneCount) {
	static thread_local struct {
		uintptr_t mesh = 0;
		int num = 0;
		std::vector<int32_t> parents;
	} cache;
	if (cache.mesh != skelMeshComponent || cache.num != boneCount) {
		cache.mesh = skelMeshComponent;
		cache.num = boneCount;
		cache.parents.clear();
		(void)TryLoadRefSkeletonParents(skelMeshComponent, boneCount, cache.parents);
	}
	return cache.parents.empty() ? nullptr : &cache.parents;
}

inline bool RefSkeletonParentsTrusted(uintptr_t skelMeshComponent, int boneCount) {
	std::vector<int32_t> trial;
	return TryLoadRefSkeletonParents(skelMeshComponent, boneCount, trial);
}

inline bool TryLoadBoneParentIndex(uintptr_t skelMeshComponent, int boneCount, int boneIndex,
                                   int& outParent) {
	outParent = -1;
	if (boneIndex < 0 || boneIndex >= boneCount)
		return false;
	const std::vector<int32_t>* parents =
	    GetCachedRefSkeletonParents(skelMeshComponent, boneCount);
	if (!parents || boneIndex >= static_cast<int>(parents->size()))
		return false;
	outParent = (*parents)[static_cast<size_t>(boneIndex)];
	return true;
}

inline bool BuildBoneComponentSpaceMatrix(const ResolvedBoneArray& pose, uintptr_t mesh,
                                          int bone_id, D3DMATRIX& outCs) {
	outCs = {};
	outCs._11 = outCs._22 = outCs._33 = outCs._44 = 1.f;
	if (bone_id < 0 || bone_id >= pose.Num)
		return false;
	int chain[128];
	int chainLen = 0;
	for (int b = bone_id; b >= 0 && chainLen < 128; ) {
		chain[chainLen++] = b;
		if (b == 0)
			break;
		int parent = 0;
		if (!TryLoadBoneParentIndex(mesh, pose.Num, b, parent))
			parent = b - 1;
		if (parent < 0 || parent >= pose.Num)
			break;
		b = parent;
	}
	for (int i = chainLen - 1; i >= 0; --i) {
		FTransform local{};
		if (!Memory::Process.ReadRequestOk(
		        pose.Data + static_cast<uintptr_t>(chain[i]) * 0x30u, local))
			continue;
		const D3DMATRIX lm = local.ToMatrixWithScale();
		outCs = MatrixMultiplication(lm, outCs);
	}
	return true;
}

inline Vector3 MatrixOrigin(const D3DMATRIX& m) {
	return Vector3(m._41, m._42, m._43);
}

inline bool ResolveBoneComponentSpaceTransform(uintptr_t mesh, int bone_id, FTransform& outCs) {
	if (!mesh || !IsPlausibleUObject(mesh) || bone_id < 0)
		return false;
	ResolvedBoneArray pose{};
	if (!ResolveBonePoseArray(mesh, pose) || bone_id >= pose.Num)
		return false;
	if (pose.UsesBoneSpace) {
		D3DMATRIX csMat{};
		if (!BuildBoneComponentSpaceMatrix(pose, mesh, bone_id, csMat))
			return false;
		outCs.translation = MatrixOrigin(csMat);
		outCs.rot.w = 1.f;
		outCs.scale = Vector3(1.f, 1.f, 1.f);
		return true;
	}
	if (!Memory::Process.ReadRequestOk(pose.Data + static_cast<uintptr_t>(bone_id) * 0x30, outCs))
		return false;
	return QuatNormalized(outCs.rot);
}

inline Vector3 RotateVectorByQuat(const FQuat& q, const Vector3& v) {
	const float x2 = q.x + q.x;
	const float y2 = q.y + q.y;
	const float z2 = q.z + q.z;
	const float xx = q.x * x2;
	const float yy = q.y * y2;
	const float zz = q.z * z2;
	const float xy = q.x * y2;
	const float xz = q.x * z2;
	const float yz = q.y * z2;
	const float wx = q.w * x2;
	const float wy = q.w * y2;
	const float wz = q.w * z2;
	return Vector3((1.f - (yy + zz)) * v.x + (xy - wz) * v.y + (xz + wy) * v.z,
	               (xy + wz) * v.x + (1.f - (xx + zz)) * v.y + (yz - wx) * v.z,
	               (xz - wy) * v.x + (yz + wx) * v.y + (1.f - (xx + yy)) * v.z);
}

// ponytail: resolved once via BoneMatrixMultiplyOrderSelfCheck (component-space offset vs C2W).
inline bool g_BoneMatrixMultiplyBoneFirst = true;

inline Vector3 TransformBoneToWorld(const FTransform& bone, const FTransform& componentToWorld,
                                      bool boneMatrixFirst = g_BoneMatrixMultiplyBoneFirst) {
	if (!boneMatrixFirst) {
		const D3DMATRIX boneMat = bone.ToMatrixWithScale();
		const D3DMATRIX c2wMat = componentToWorld.ToMatrixWithScale();
		return MatrixOrigin(MatrixMultiplication(c2wMat, boneMat));
	}
	const auto axis = [](float s) { return s == 0.f ? 1.f : s; };
	const Vector3 local(bone.translation.x * axis(componentToWorld.scale.x),
	                    bone.translation.y * axis(componentToWorld.scale.y),
	                    bone.translation.z * axis(componentToWorld.scale.z));
	const Vector3 rotated = RotateVectorByQuat(componentToWorld.rot, local);
	return Vector3(componentToWorld.translation.x + rotated.x,
	               componentToWorld.translation.y + rotated.y,
	               componentToWorld.translation.z + rotated.z);
}

inline bool BoneMatrixMultiplyOrderSelfCheck() {
	FTransform c2w{};
	c2w.rot.y = 0.707106781f;
	c2w.rot.w = 0.707106781f;
	c2w.translation = Vector3(1000.f, 2000.f, 300.f);
	c2w.scale = Vector3(1.f, 1.f, 1.f);
	FTransform bone{};
	bone.translation = Vector3(50.f, 0.f, 0.f);
	bone.rot.w = 1.f;
	bone.scale = Vector3(1.f, 1.f, 1.f);
	const Vector3 expected =
	    Vector3(c2w.translation.x + RotateVectorByQuat(c2w.rot, bone.translation).x,
	            c2w.translation.y + RotateVectorByQuat(c2w.rot, bone.translation).y,
	            c2w.translation.z + RotateVectorByQuat(c2w.rot, bone.translation).z);
	const Vector3 boneFirst = TransformBoneToWorld(bone, c2w, true);
	const Vector3 c2wFirst = TransformBoneToWorld(bone, c2w, false);
	const float errBoneFirst = Vec3Distance(boneFirst, expected);
	const float errC2wFirst = Vec3Distance(c2wFirst, expected);
	g_BoneMatrixMultiplyBoneFirst = errBoneFirst <= errC2wFirst;
	return errBoneFirst <= errC2wFirst || errC2wFirst <= errBoneFirst;
}

inline bool BoneIdentityUsesC2wTranslationSelfCheck() {
	FTransform c2w{};
	c2w.rot.y = -1.570796f;
	c2w.rot.w = 0.707106f;
	c2w.translation = Vector3(67.f, 125458.f, 2.94f);
	c2w.scale = Vector3(1.f, 1.f, 1.f);
	FTransform bone{};
	bone.rot.w = 1.f;
	bone.scale = Vector3(1.f, 1.f, 1.f);
	const Vector3 world = TransformBoneToWorld(bone, c2w);
	return Vec3Distance(world, c2w.translation) < 0.05f;
}

inline bool BoneMultiplyKeepsOffset() {
	FTransform bone{};
	bone.rot.w = 1.f;
	bone.translation = Vector3(0.f, 0.f, 106.f);
	FTransform c2w{};
	c2w.rot.w = 1.f;
	c2w.translation = Vector3(67.f, 125458.f, 3.f);
	const Vector3 hand = TransformBoneToWorld(bone, c2w);
	bone.translation = Vector3(0.f, 0.f, 0.f);
	const Vector3 root = TransformBoneToWorld(bone, c2w);
	return hand.z > 100.f && hand.y > 100000.f && !BoneWorldMissing(root);
}

inline bool MeshHasRetracPoseBuffer(uintptr_t mesh) {
	if (!mesh || !Memory::IsValid(mesh))
		return false;
	FUeTArrayHeader hdr{};
	if (!ReadUeTArrayHeader(mesh + 0x488, hdr))
		return false;
	return hdr.Data && Memory::IsValid(hdr.Data) && hdr.Num >= 30 && hdr.Num <= 1024;
}

inline uintptr_t ResolveSkeletonMesh(uintptr_t mesh) {
	if (!mesh || !Memory::IsValid(mesh))
		return mesh;
	if (MeshHasRetracPoseBuffer(mesh))
		return mesh;
	const uintptr_t master = Read<uintptr_t>(mesh + Offsets::MasterPoseComponent);
	if (master && Memory::IsValid(master))
		return master;
	return mesh;
}

inline void LogBoneTransformStateOnChange(uintptr_t mesh, const MeshTransformSnapshot& c2wSnap,
                                          const Vector3& headWorld, const Vector3& pelvisWorld,
                                          const char* worldRejectReason) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static std::atomic<unsigned long long> nextLogMs{ 0 };
	static bool prevC2wValid = false;
	static const char* prevC2wReject = nullptr;
	static const char* prevWorldReject = nullptr;
	static bool prevHeadZero = true;
	static bool prevPelvisZero = true;
	const unsigned long long now = GetTickCount64();
	const bool headZero = BoneWorldMissing(headWorld);
	const bool pelvisZero = BoneWorldMissing(pelvisWorld);
	const bool c2wChanged = c2wSnap.valid != prevC2wValid ||
	                        (c2wSnap.rejectionReason != prevC2wReject &&
	                         (c2wSnap.rejectionReason || prevC2wReject));
	const bool worldChanged =
	    (worldRejectReason != prevWorldReject && (worldRejectReason || prevWorldReject)) ||
	    headZero != prevHeadZero || pelvisZero != prevPelvisZero;
	if (!c2wChanged && !worldChanged && now < nextLogMs.load(std::memory_order_relaxed))
		return;
	nextLogMs.store(now + 3000, std::memory_order_relaxed);
	prevC2wValid = c2wSnap.valid;
	prevC2wReject = c2wSnap.rejectionReason;
	prevWorldReject = worldRejectReason;
	prevHeadZero = headZero;
	prevPelvisZero = pelvisZero;
	std::ostringstream line;
	line << xorstr_("[bone] c2w=") << (c2wSnap.source ? c2wSnap.source : "?")
	     << xorstr_(" valid=") << (c2wSnap.valid ? 1 : 0) << xorstr_(" loc=")
	     << c2wSnap.translation.x << ',' << c2wSnap.translation.y << ','
	     << c2wSnap.translation.z << xorstr_(" head=") << headWorld.x << ',' << headWorld.y << ','
	     << headWorld.z << xorstr_(" pelvis=") << pelvisWorld.x << ',' << pelvisWorld.y << ','
	     << pelvisWorld.z;
	if (c2wSnap.rejectionReason)
		line << xorstr_(" c2w_reject=") << c2wSnap.rejectionReason;
	if (worldRejectReason)
		line << xorstr_(" world_reject=") << worldRejectReason;
	ConsoleDiagLine(line.str());
	(void)mesh;
}

// Same mannequin indices the drawer requests. Logged so a short translation is not
// mistaken for a wrong bone id: in bone-local space these lengths are normal.
inline const int kBoneDiagIndices[] = {
    EBoneIndex::Root,     EBoneIndex::Pelvis, EBoneIndex::Spine_05, EBoneIndex::Hand_L,
    EBoneIndex::Neck_01,  EBoneIndex::Head,   EBoneIndex::Foot_L,
};

inline const char* BoneRawReject(const FTransform& t) {
	if (!std::isfinite(t.translation.x) || !std::isfinite(t.translation.y) ||
	    !std::isfinite(t.translation.z) || !std::isfinite(t.rot.x) || !std::isfinite(t.rot.y) ||
	    !std::isfinite(t.rot.z) || !std::isfinite(t.rot.w) || !std::isfinite(t.scale.x) ||
	    !std::isfinite(t.scale.y) || !std::isfinite(t.scale.z))
		return "non_finite";
	if (!QuatNormalized(t.rot))
		return "invalid_quaternion";
	if (t.scale.x == 0.f || t.scale.y == 0.f || t.scale.z == 0.f || Absf(t.scale.x) > 20.f ||
	    Absf(t.scale.y) > 20.f || Absf(t.scale.z) > 20.f)
		return "invalid_scale";
	return nullptr;
}

// Component-space mannequin: spine/head sit well above the mesh origin.
// Bone-local: those same indices are short parent offsets; only pelvis reaches ~90cm.
inline const char* ClassifyPoseTranslationSpace(const FTransform& pelvis, const FTransform& spine,
                                                const FTransform& head) {
	const float headLen = TranslationLength(head.translation);
	const float pelvisLen = TranslationLength(pelvis.translation);
	const float spineZ = Absf(spine.translation.z);
	const bool headLocal = headLen < 40.f;
	const bool spineLocal = spineZ < 20.f;
	const bool pelvisLarge = pelvisLen > 50.f;
	if (headLocal && spineLocal && pelvisLarge)
		return "local_space";
	if (!headLocal && spineZ > 40.f)
		return "component_space";
	return "mixed_or_unknown";
}

inline bool PoseSpaceClassifySelfCheck() {
	FTransform pelvis{};
	pelvis.translation = Vector3(2.7f, 0.9f, 93.8f);
	pelvis.rot.w = 1.f;
	pelvis.scale = Vector3(1.f, 1.f, 1.f);
	FTransform spine{};
	spine.translation = Vector3(6.7f, 0.f, 0.f);
	spine.rot.w = 1.f;
	spine.scale = Vector3(1.f, 1.f, 1.f);
	FTransform head{};
	head.translation = Vector3(5.4f, 0.f, 0.f);
	head.rot.w = 1.f;
	head.scale = Vector3(1.f, 1.f, 1.f);
	FTransform spineCs = spine;
	spineCs.translation = Vector3(-8.f, 7.f, 130.f);
	FTransform headCs = head;
	headCs.translation = Vector3(-6.f, 10.f, 150.f);
	return std::strcmp(ClassifyPoseTranslationSpace(pelvis, spine, head), "local_space") == 0 &&
	       std::strcmp(ClassifyPoseTranslationSpace(pelvis, spineCs, headCs), "component_space") ==
	           0;
}

// Double-read the pose, classify space, and track head/pelvis deltas. Does not rewrite transforms.
inline const char* DiagnoseBonePoseSnapshot(uintptr_t mesh, const ResolvedBoneArray& pose,
                                            const std::vector<FTransform>& first,
                                            const FTransform& c2w, bool c2wValid) {
	if (!pose.Data || first.empty())
		return "pose_unread";
	std::vector<FTransform> second(first.size());
	const bool reread = Memory::Process.Read(pose.Data, second.data(),
	                                          static_cast<DWORD>(first.size() * sizeof(FTransform)));
	bool torn = false;
	if (reread) {
		// ponytail: animation updates the TArray between back-to-back RPM reads; limbs move faster
		// than pelvis/head. Only gate on core bones with a generous delta (not 1uu).
		constexpr float kSnapshotTearMax = 64.f;
		const int tearBones[] = { EBoneIndex::Pelvis, EBoneIndex::Head };
		for (int bone : tearBones) {
			if (bone < 0 || bone >= static_cast<int>(first.size()))
				continue;
			if (Vec3Distance(first[static_cast<size_t>(bone)].translation,
			                 second[static_cast<size_t>(bone)].translation) > kSnapshotTearMax) {
				torn = true;
				break;
			}
		}
	}
	const char* rawReject = nullptr;
	int rawBad = 0;
	for (int bone : kBoneDiagIndices) {
		if (bone < 0 || bone >= static_cast<int>(first.size())) {
			rawReject = "bone_index_oob";
			++rawBad;
			continue;
		}
		if (const char* why = BoneRawReject(first[static_cast<size_t>(bone)])) {
			rawReject = why;
			++rawBad;
		}
	}
	const bool haveAnatomical = static_cast<int>(first.size()) > EBoneIndex::Head;
	const char* space = "unclassified";
	if (haveAnatomical && !rawReject) {
		space = ClassifyPoseTranslationSpace(first[static_cast<size_t>(EBoneIndex::Pelvis)],
		                                     first[static_cast<size_t>(EBoneIndex::Spine_05)],
		                                     first[static_cast<size_t>(EBoneIndex::Head)]);
	}
	const char* reason = nullptr;
	if (!reread)
		reason = "pose_reread_failed";
	else if (torn)
		reason = "inconsistent_snapshot";
	else if (rawReject)
		reason = rawReject;
	else if (!c2wValid)
		reason = "c2w_invalid";
	else if (std::strcmp(space, "mixed_or_unknown") == 0)
		reason = "pose_mixed_space";
	else if (std::strcmp(space, "local_space") == 0 && !pose.UsesBoneSpace)
		reason = "transform_interpretation_mismatch";
	else if (std::strcmp(space, "component_space") == 0 && pose.UsesBoneSpace)
		reason = "transform_interpretation_mismatch";

	static uintptr_t slotTrackMesh = 0;
	static uint32_t slotTrackOff = 0;
	if (slotTrackMesh == mesh && slotTrackOff && slotTrackOff != pose.MeshOffset && !reason)
		reason = "source_pose_slot_flip";
	slotTrackMesh = mesh;
	slotTrackOff = pose.MeshOffset;

	static int sampleN = 0;
	static Vector3 prevHeadCs{};
	static Vector3 prevPelvisCs{};
	static Vector3 prevHeadW{};
	static Vector3 prevPelvisW{};
	static bool havePrev = false;
	++sampleN;
	const uintptr_t poseMesh = ResolveSkeletonMesh(mesh);
	auto worldOf = [&](int bone) -> Vector3 {
		if (!c2wValid || bone < 0 || bone >= static_cast<int>(first.size()))
			return Vector3{};
		if (pose.UsesBoneSpace) {
			D3DMATRIX csMat{};
			if (!BuildBoneComponentSpaceMatrix(pose, poseMesh, bone, csMat))
				return Vector3{};
			const D3DMATRIX c2wMat = c2w.ToMatrixWithScale();
			return MatrixOrigin(MatrixMultiplication(csMat, c2wMat));
		}
		return TransformBoneToWorld(first[static_cast<size_t>(bone)], c2w);
	};
	const Vector3 headCs =
	    haveAnatomical ? first[static_cast<size_t>(EBoneIndex::Head)].translation : Vector3{};
	const Vector3 pelvisCs =
	    haveAnatomical ? first[static_cast<size_t>(EBoneIndex::Pelvis)].translation : Vector3{};
	const Vector3 headW = worldOf(EBoneIndex::Head);
	const Vector3 pelvisW = worldOf(EBoneIndex::Pelvis);
	Vector3 headDelta{};
	Vector3 pelvisDelta{};
	if (havePrev) {
		headDelta = Vector3(headCs.x - prevHeadCs.x, headCs.y - prevHeadCs.y, headCs.z - prevHeadCs.z);
		pelvisDelta = Vector3(pelvisCs.x - prevPelvisCs.x, pelvisCs.y - prevPelvisCs.y,
		                      pelvisCs.z - prevPelvisCs.z);
		if (!reason && Vec3Distance(headCs, prevHeadCs) > 220.f)
			reason = "source_pose_discontinuity";
	}
	if (Settings::DebugAtLeast(Settings::DebugVerbosity::Info)) {
		static const char* lastReason = nullptr;
		static int loggedSamples = 0;
		const bool dumpRaw = loggedSamples < 4 || reason != lastReason;
		const bool dumpDelta =
		    sampleN <= 6 || (sampleN % 12) == 0 ||
		    (havePrev && Vec3Distance(headCs, prevHeadCs) > 20.f) ||
		    reason != lastReason;
		if (dumpRaw) {
			std::cout << xorstr_("[bone-diag] mesh=0x") << std::hex << mesh << std::dec
			          << xorstr_(" slot=0x") << std::hex << pose.MeshOffset << std::dec
			          << xorstr_(" num=") << pose.Num << xorstr_(" uses_bone_space=")
			          << (pose.UsesBoneSpace ? 1 : 0) << xorstr_(" convention=")
			          << (pose.UsesBoneSpace ? "bone_local_chain" : "component_times_c2w")
			          << xorstr_(" space=") << space << xorstr_(" torn=") << (torn ? 1 : 0)
			          << xorstr_(" raw_bad=") << rawBad << xorstr_(" quat=FQuat")
			          << xorstr_(" order=bone_matrix*c2w_matrix reason=")
			          << (reason ? reason : "ok") << '\n';
			for (int bone : kBoneDiagIndices) {
				if (bone < 0 || bone >= static_cast<int>(first.size()))
					continue;
				const FTransform& t = first[static_cast<size_t>(bone)];
				const Vector3 world = worldOf(bone);
				const char* raw = BoneRawReject(t);
				std::cout << xorstr_("[bone-diag] bone=") << bone << xorstr_(" raw=")
				          << (raw ? raw : "ok") << xorstr_(" t=") << t.translation.x << ','
				          << t.translation.y << ',' << t.translation.z << xorstr_(" q=") << t.rot.x
				          << ',' << t.rot.y << ',' << t.rot.z << ',' << t.rot.w << xorstr_(" s=")
				          << t.scale.x << ',' << t.scale.y << ',' << t.scale.z
				          << xorstr_(" component=") << t.translation.x << ',' << t.translation.y
				          << ',' << t.translation.z << xorstr_(" world=") << world.x << ','
				          << world.y << ',' << world.z << '\n';
			}
			++loggedSamples;
		}
		if (dumpDelta) {
			std::cout << xorstr_("[bone-diag] bone=") << EBoneIndex::Head << xorstr_(" sample=")
			          << sampleN << xorstr_(" component=") << headCs.x << ',' << headCs.y << ','
			          << headCs.z << xorstr_(" world=") << headW.x << ',' << headW.y << ',' << headW.z
			          << xorstr_(" delta=") << headDelta.x << ',' << headDelta.y << ',' << headDelta.z
			          << xorstr_(" space=") << space << '\n';
			std::cout << xorstr_("[bone-diag] bone=") << EBoneIndex::Pelvis << xorstr_(" sample=")
			          << sampleN << xorstr_(" component=") << pelvisCs.x << ',' << pelvisCs.y << ','
			          << pelvisCs.z << xorstr_(" world=") << pelvisW.x << ',' << pelvisW.y << ','
			          << pelvisW.z << xorstr_(" delta=") << pelvisDelta.x << ',' << pelvisDelta.y
			          << ',' << pelvisDelta.z << xorstr_(" space=") << space << '\n';
		}
		lastReason = reason;
	}
	prevHeadCs = headCs;
	prevPelvisCs = pelvisCs;
	prevHeadW = headW;
	prevPelvisW = pelvisW;
	havePrev = true;
	(void)mesh;
	return reason;
}

struct BoneFrameSnapshot {
	bool acquired = false;
	RenderPipeline::BonePipelineVerdict verdict = RenderPipeline::BonePipelineVerdict::INVALID_UNINITIALIZED;
	const char* rejectReason = nullptr;
	unsigned frame = 0;
	uintptr_t mesh = 0;
	uintptr_t poseMesh = 0;
	MeshTransformSnapshot c2w{};
	ResolvedBoneArray pose{};
	std::vector<FTransform> bones;
	const char* dataSpace = "unclassified";
	bool usesBoneSpaceFromData = false;
	Vector3 headWorld{};
	Vector3 pelvisWorld{};
};

inline const int kBoneTraceIndices[] = { EBoneIndex::Pelvis, EBoneIndex::Head, EBoneIndex::Hand_L };

inline bool ClassifyPoseArrayComponentSpace(const ResolvedBoneArray& pose, bool& outComponentSpace) {
	outComponentSpace = false;
	if (!pose.Data || pose.Num <= EBoneIndex::Head)
		return false;
	FTransform pelvis{}, spine{}, head{};
	if (!Memory::Process.ReadRequestOk(
	        pose.Data + static_cast<uintptr_t>(EBoneIndex::Pelvis) * 0x30u, pelvis) ||
	    !Memory::Process.ReadRequestOk(
	        pose.Data + static_cast<uintptr_t>(EBoneIndex::Spine_05) * 0x30u, spine) ||
	    !Memory::Process.ReadRequestOk(
	        pose.Data + static_cast<uintptr_t>(EBoneIndex::Head) * 0x30u, head))
		return false;
	if (BoneRawReject(pelvis) || BoneRawReject(spine) || BoneRawReject(head))
		return false;
	const char* space = ClassifyPoseTranslationSpace(pelvis, spine, head);
	if (std::strcmp(space, "mixed_or_unknown") == 0)
		return false;
	outComponentSpace = (std::strcmp(space, "component_space") == 0);
	return true;
}

// Retrac live pose at mesh+0x488: header-only pick (AcquireBoneFrameSnapshot validates after bulk read).
inline bool MeshHasKnownRetracPose488Site(uintptr_t mesh) {
	if (!mesh)
		return false;
	std::lock_guard<std::mutex> lock(BonePoseOffsetMutex);
	const auto it = BonePoseSiteByMesh.find(mesh);
	return it != BonePoseSiteByMesh.end() && it->second.offset == 0x488u && !it->second.usesBoneSpace;
}

inline bool TryPickRetracLivePose488(uintptr_t mesh, ResolvedBoneArray& outPose) {
	outPose = {};
	if (!mesh || !IsPlausibleUObject(mesh))
		return false;
	FUeTArrayHeader hdr{};
	if (!ReadUeTArrayHeader(mesh + 0x488u, hdr) || !UeTArrayHeaderPlausible(hdr))
		return false;
	if (hdr.Num <= EBoneIndex::Head)
		return false;
	const bool knownSite = MeshHasKnownRetracPose488Site(mesh);
	FTransform probe{};
	const bool probeOk =
	    Memory::Process.ReadRequestOk(hdr.Data, probe) && QuatNormalized(probe.rot);
	if (!probeOk && !knownSite)
		return false;
	if (knownSite && (!hdr.Data || !Memory::IsValid(hdr.Data)))
		return false;
	outPose.Data = hdr.Data;
	outPose.Num = hdr.Num;
	outPose.MeshOffset = 0x488u;
	outPose.UsesBoneSpace = false;
	return true;
}

inline constexpr unsigned kBoneStickyMaxFrameAge = 24;
inline constexpr unsigned long long kBoneStickyMaxMs = 400;

inline std::unordered_map<uintptr_t, BoneFrameSnapshot>& BoneStickySnapshotMap() {
	static thread_local std::unordered_map<uintptr_t, BoneFrameSnapshot> map;
	return map;
}

inline std::unordered_map<uintptr_t, unsigned>& BoneStickyFrameMap() {
	static thread_local std::unordered_map<uintptr_t, unsigned> map;
	return map;
}

inline std::unordered_map<uintptr_t, unsigned long long>& BoneStickyTickMap() {
	static thread_local std::unordered_map<uintptr_t, unsigned long long> map;
	return map;
}

inline void RememberStickyBoneSnapshot(uintptr_t mesh, const BoneFrameSnapshot& snap) {
	if (!mesh || !snap.acquired || !RenderPipeline::BoneVerdictConsumable(snap.verdict))
		return;
	BoneStickySnapshotMap()[mesh] = snap;
	BoneStickyFrameMap()[mesh] = snap.frame;
	BoneStickyTickMap()[mesh] = GetTickCount64();
}

inline bool TryConsumeStickyBoneSnapshot(uintptr_t mesh, unsigned currentFrame,
                                         BoneFrameSnapshot& out) {
	const auto it = BoneStickySnapshotMap().find(mesh);
	if (it == BoneStickySnapshotMap().end())
		return false;
	const BoneFrameSnapshot& sticky = it->second;
	if (!sticky.acquired || !RenderPipeline::BoneVerdictConsumable(sticky.verdict))
		return false;
	const unsigned stickyFrame = BoneStickyFrameMap()[mesh];
	const unsigned long long stickyTick = BoneStickyTickMap()[mesh];
	const unsigned long long now = GetTickCount64();
	if (currentFrame - stickyFrame > kBoneStickyMaxFrameAge && now - stickyTick > kBoneStickyMaxMs)
		return false;
	out = sticky;
	out.frame = currentFrame;
	out.mesh = mesh;
	return true;
}

inline bool EspPoseOffsetOk(uint32_t offset) {
	return offset == 0x488u || offset == static_cast<uint32_t>(Offsets::BoneCache);
}

// SDK CachedComponentSpaceTransforms is mesh+0x730. 0x488 is unnamed pad, used only when 0x730 is empty.
inline bool PickPoseSlotForSnapshot(uintptr_t mesh, ResolvedBoneArray& outPose) {
	outPose = {};
	if (!mesh || !IsPlausibleUObject(mesh))
		return false;
	if (TryReadComponentSpacePoseAt(mesh, static_cast<uint32_t>(Offsets::BoneCache), outPose) &&
	    outPose.Num > EBoneIndex::Head)
		return true;
	if (TryPickRetracLivePose488(mesh, outPose))
		return true;
	{
		std::lock_guard<std::mutex> lock(BonePoseOffsetMutex);
		const auto it = BonePoseSiteByMesh.find(mesh);
		if (it != BonePoseSiteByMesh.end() && it->second.offset == 0x488u &&
		    !it->second.usesBoneSpace) {
			if (TryPickRetracLivePose488(mesh, outPose))
				return true;
		}
	}
	if (ResolveBonePoseArrayAt(mesh, 0x488u, outPose) ||
	    TryReadPoseAt(mesh, 0x488u, outPose, true)) {
		outPose.UsesBoneSpace = false;
		outPose.MeshOffset = 0x488u;
		return true;
	}
	return false;
}

inline Vector3 BoneWorldFromSnapshotTransforms(const BoneFrameSnapshot& snap, int bone_id) {
	if (!snap.c2w.valid || bone_id < 0 || bone_id >= static_cast<int>(snap.bones.size()))
		return Vector3{};
	const FTransform& componentToWorld = snap.c2w.c2w;
	if (snap.usesBoneSpaceFromData) {
		D3DMATRIX csMat{};
		if (!BuildBoneComponentSpaceMatrix(snap.pose, snap.poseMesh, bone_id, csMat))
			return Vector3{};
		const D3DMATRIX c2wMat = componentToWorld.ToMatrixWithScale();
		const D3DMATRIX worldMat = g_BoneMatrixMultiplyBoneFirst
		                               ? MatrixMultiplication(csMat, c2wMat)
		                               : MatrixMultiplication(c2wMat, csMat);
		return MatrixOrigin(worldMat);
	}
	return TransformBoneToWorld(snap.bones[static_cast<size_t>(bone_id)], componentToWorld);
}

inline Vector3 BoneWorldFromSnapshotData(const BoneFrameSnapshot& snap, int bone_id) {
	if (!snap.acquired || !RenderPipeline::BoneVerdictConsumable(snap.verdict))
		return Vector3{};
	return BoneWorldFromSnapshotTransforms(snap, bone_id);
}

inline void CommitLocalBonePipeline(uintptr_t mesh, const BoneFrameSnapshot& snap) {
	const uintptr_t localMesh =
	    LocalPtrs::Player ? Read<uintptr_t>(LocalPtrs::Player + Offsets::Mesh) : 0;
	if (localMesh && mesh == localMesh)
		RenderPipeline::UpdateLocalBonePipeline(
		    snap.verdict, snap.rejectReason ? snap.rejectReason : "unknown");
}

inline float BoneOffsetInvariantError(const FTransform& bone, const FTransform& c2w,
                                      const Vector3& world) {
	const Vector3 delta(world.x - c2w.translation.x, world.y - c2w.translation.y,
	                    world.z - c2w.translation.z);
	const float rotMag = Absf(bone.rot.x) + Absf(bone.rot.y) + Absf(bone.rot.z);
	const float scaleErr = Absf(bone.scale.x - 1.f) + Absf(bone.scale.y - 1.f) +
	                       Absf(bone.scale.z - 1.f);
	if (rotMag < 0.08f && scaleErr < 0.08f) {
		const auto axis = [](float s) { return s == 0.f ? 1.f : s; };
		const Vector3 local(bone.translation.x * axis(c2w.scale.x),
		                    bone.translation.y * axis(c2w.scale.y),
		                    bone.translation.z * axis(c2w.scale.z));
		const Vector3 expected = RotateVectorByQuat(c2w.rot, local);
		return Vec3Distance(delta, expected);
	}
	const Vector3 rebuilt = TransformBoneToWorld(bone, c2w);
	return Vec3Distance(rebuilt, world);
}

inline void LogBoneSnapshotCompact(const BoneFrameSnapshot& snap, int sampleN,
                                   const Vector3* prevWorld, bool havePrev) {
	(void)prevWorld;
	(void)havePrev;
	(void)sampleN;
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	const uintptr_t localMesh =
	    LocalPtrs::Player && Memory::IsValid(LocalPtrs::Player)
	        ? Read<uintptr_t>(LocalPtrs::Player + Offsets::Mesh)
	        : 0;
	const bool localPawnMesh = localMesh && snap.mesh == localMesh;
	if (!localPawnMesh && !Settings::DebugAtLeast(Settings::DebugVerbosity::Trace))
		return;

	static thread_local RenderPipeline::BonePipelineVerdict lastVerdict =
	    RenderPipeline::BonePipelineVerdict::INVALID_UNINITIALIZED;
	static thread_local const char* lastReject = nullptr;
	static thread_local unsigned long long lastLogMs = 0;
	const unsigned long long now = GetTickCount64();
	const bool consumable = RenderPipeline::BoneVerdictConsumable(snap.verdict);
	const bool verdictChanged =
	    !RenderPipeline::BoneVerdictSameForLog(snap.verdict, lastVerdict);
	const bool rejectChanged =
	    (snap.rejectReason != lastReject) &&
	    (snap.rejectReason || lastReject) &&
	    (!snap.rejectReason || !lastReject ||
	     std::strcmp(snap.rejectReason, lastReject ? lastReject : "") != 0);
	const unsigned long long throttleMs =
	    consumable ? 8000ull : 1500ull;
	if (consumable && !verdictChanged && !rejectChanged && !snap.rejectReason && lastLogMs &&
	    now - lastLogMs < throttleMs)
		return;
	lastVerdict = snap.verdict;
	lastReject = snap.rejectReason;
	lastLogMs = now;

	const bool traceDetail = Settings::DebugAtLeast(Settings::DebugVerbosity::Trace);
	const bool showPerBone =
	    traceDetail && (!consumable || verdictChanged || rejectChanged || snap.rejectReason);

	std::ostringstream line;
	line << xorstr_("[bone] ") << RenderPipeline::BonePipelineVerdictString(snap.verdict)
	     << xorstr_(" slot=0x") << std::hex << snap.pose.MeshOffset << std::dec << xorstr_(" ")
	     << snap.dataSpace << xorstr_(" c2w=") << snap.c2w.translation.x << ','
	     << snap.c2w.translation.y << ',' << snap.c2w.translation.z;
	if (snap.rejectReason)
		line << xorstr_(" reject=") << snap.rejectReason;
	if (!showPerBone) {
		ConsoleDiagLine(line.str());
		return;
	}
	for (int bone : kBoneTraceIndices) {
		if (bone < 0 || bone >= static_cast<int>(snap.bones.size()))
			continue;
		const FTransform& raw = snap.bones[static_cast<size_t>(bone)];
		const Vector3 world = BoneWorldFromSnapshotTransforms(snap, bone);
		line << '\n' << xorstr_("  b") << bone << xorstr_(" cs=") << raw.translation.x << ','
		     << raw.translation.y << ',' << raw.translation.z << xorstr_(" w=") << world.x << ','
		     << world.y << ',' << world.z;
	}
	ConsoleDiagLine(line.str());
}

inline void AcquireBoneFrameSnapshot(uintptr_t mesh, BoneFrameSnapshot& out) {
	out = {};
	out.frame = EspFrameCounter.load(std::memory_order_relaxed);
	out.mesh = mesh;
	if (!mesh || !IsPlausibleUObject(mesh)) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_BONE_MAPPING;
		out.rejectReason = "mesh_invalid";
		return;
	}
	(void)BoneMatrixMultiplyOrderSelfCheck();
	out.poseMesh = ResolveSkeletonMesh(mesh);
	if (!PickPoseSlotForSnapshot(out.poseMesh, out.pose)) {
		if (mesh != out.poseMesh && PickPoseSlotForSnapshot(mesh, out.pose))
			out.poseMesh = mesh;
		else {
			out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_BONE_MAPPING;
			out.rejectReason = "pose_slot_missing";
			return;
		}
	}
	if (out.pose.Num <= EBoneIndex::Head) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_BONE_MAPPING;
		out.rejectReason = "bone_count_short";
		return;
	}
	const MeshTransformSnapshot c2wBefore = DiagnoseComponentToWorld(mesh);
	FUeTArrayHeader hdrBefore{};
	if (!ReadUeTArrayHeader(out.poseMesh + out.pose.MeshOffset, hdrBefore) ||
	    !UeTArrayHeaderPlausible(hdrBefore) || hdrBefore.Num != out.pose.Num) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_BONE_MAPPING;
		out.rejectReason = "pose_header_invalid";
		return;
	}

	out.bones.resize(static_cast<size_t>(hdrBefore.Num));
	if (!Memory::Process.Read(hdrBefore.Data, out.bones.data(),
	                          static_cast<DWORD>(hdrBefore.Num * sizeof(FTransform)))) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TORN_SNAPSHOT;
		out.rejectReason = "pose_bulk_read_failed";
		return;
	}

	FTransform probePelvisBefore = out.bones[static_cast<size_t>(EBoneIndex::Pelvis)];
	FTransform probeHeadBefore = out.bones[static_cast<size_t>(EBoneIndex::Head)];

	const MeshTransformSnapshot c2wAfter = DiagnoseComponentToWorld(mesh);
	FUeTArrayHeader hdrAfter{};
	(void)ReadUeTArrayHeader(out.poseMesh + out.pose.MeshOffset, hdrAfter);
	FTransform probePelvisAfter{};
	FTransform probeHeadAfter{};
	(void)Memory::Process.ReadRequestOk(
	    hdrBefore.Data + static_cast<uintptr_t>(EBoneIndex::Pelvis) * 0x30u, probePelvisAfter);
	(void)Memory::Process.ReadRequestOk(
	    hdrBefore.Data + static_cast<uintptr_t>(EBoneIndex::Head) * 0x30u, probeHeadAfter);

	bool torn = false;
	if (hdrBefore.Data != hdrAfter.Data || hdrBefore.Num != hdrAfter.Num)
		torn = true;
	if (c2wBefore.valid && c2wAfter.valid &&
	    Vec3Distance(c2wBefore.translation, c2wAfter.translation) > 250.f)
		torn = true;
	if (Vec3Distance(probePelvisBefore.translation, probePelvisAfter.translation) > 120.f ||
	    Vec3Distance(probeHeadBefore.translation, probeHeadAfter.translation) > 120.f)
		torn = true;

	out.c2w = c2wAfter.valid ? c2wAfter : c2wBefore;
	if (!out.c2w.valid) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TRANSFORM;
		out.rejectReason = out.c2w.rejectionReason ? out.c2w.rejectionReason : "c2w_invalid";
		return;
	}
	if (torn) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TORN_SNAPSHOT;
		out.rejectReason = "acquire_state_changed";
		return;
	}

	out.dataSpace = ClassifyPoseTranslationSpace(
	    out.bones[static_cast<size_t>(EBoneIndex::Pelvis)],
	    out.bones[static_cast<size_t>(EBoneIndex::Spine_05)],
	    out.bones[static_cast<size_t>(EBoneIndex::Head)]);
	if (std::strcmp(out.dataSpace, "mixed_or_unknown") == 0) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TRANSFORM;
		out.rejectReason = "pose_mixed_space";
		return;
	}
	out.usesBoneSpaceFromData = (std::strcmp(out.dataSpace, "local_space") == 0);
	out.pose.UsesBoneSpace = out.usesBoneSpaceFromData;
	if (out.usesBoneSpaceFromData || out.pose.MeshOffset == static_cast<uint32_t>(Offsets::BonePosePad)) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_BONE_MAPPING;
		out.rejectReason = "wrong_pose_buffer";
		return;
	}

	const uintptr_t localPawn = LocalPtrs::Player;
	const uintptr_t localMesh =
	    localPawn ? Read<uintptr_t>(localPawn + Offsets::Mesh) : 0;
	if (localMesh && mesh == localMesh) {
		const Vector3 rootLoc = GetActorRootWorldLocation(localPawn);
		if (!BoneWorldMissing(rootLoc) &&
		    Vec3Distance(out.c2w.translation, rootLoc) > 450.f) {
			out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TORN_SNAPSHOT;
			out.rejectReason = "c2w_incoherent_with_root";
			return;
		}
	}

	for (int bone : kBoneTraceIndices) {
		if (const char* why = BoneRawReject(out.bones[static_cast<size_t>(bone)])) {
			out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TRANSFORM;
			out.rejectReason = why;
			return;
		}
	}

	const Vector3 rootW = BoneWorldFromSnapshotTransforms(out, EBoneIndex::Root);
	if (Vec3Distance(rootW, out.c2w.translation) > 2.5f) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TRANSFORM;
		out.rejectReason = "root_not_c2w";
		return;
	}

	const float pelvisErr = BoneOffsetInvariantError(
	    out.bones[static_cast<size_t>(EBoneIndex::Pelvis)], out.c2w.c2w,
	    BoneWorldFromSnapshotTransforms(out, EBoneIndex::Pelvis));
	if (pelvisErr > 8.f) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TRANSFORM;
		out.rejectReason = "invariant_pelvis";
		return;
	}

	out.headWorld = BoneWorldFromSnapshotTransforms(out, EBoneIndex::Head);
	out.pelvisWorld = BoneWorldFromSnapshotTransforms(out, EBoneIndex::Pelvis);
	if (BoneWorldMissing(out.headWorld) || BoneWorldMissing(out.pelvisWorld)) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TRANSFORM;
		out.rejectReason = "world_zero";
		return;
	}

	static thread_local struct {
		uintptr_t mesh = 0;
		uint32_t poseSlot = 0;
		Vector3 c2wTranslation{};
		Vector3 headWorld{};
		Vector3 pelvisWorld{};
		Vector3 headCs{};
		Vector3 pelvisCs{};
		Vector3 traceWorld[128]{};
		bool have = false;
	} prevSnap;
	static int sampleN = 0;
	++sampleN;

	Vector3 prevTrace[128]{};
	bool havePrevTrace = prevSnap.have && prevSnap.mesh == mesh;
	if (havePrevTrace && prevSnap.poseSlot && prevSnap.poseSlot != out.pose.MeshOffset &&
	    !EspPoseOffsetOk(out.pose.MeshOffset)) {
		out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TORN_SNAPSHOT;
		out.rejectReason = "pose_slot_flip";
		LogBoneSnapshotCompact(out, sampleN, nullptr, false);
		return;
	}
	const Vector3 headCs = out.bones[static_cast<size_t>(EBoneIndex::Head)].translation;
	const Vector3 pelvisCs = out.bones[static_cast<size_t>(EBoneIndex::Pelvis)].translation;
	if (havePrevTrace) {
		for (int bone : kBoneTraceIndices)
			prevTrace[bone] = prevSnap.traceWorld[bone];
		// Island load moves the same mesh tens of thousands of units. Drop the lobby
		// baseline and accept the new pose; a torn bone-array read still rejects above.
		if (Vec3Distance(out.c2w.translation, prevSnap.c2wTranslation) > 25000.f)
			havePrevTrace = false;
	}
	if (havePrevTrace) {
		const float headCsJump = Vec3Distance(headCs, prevSnap.headCs);
		const float pelvisCsJump = Vec3Distance(pelvisCs, prevSnap.pelvisCs);
		if (headCsJump > 380.f || pelvisCsJump > 380.f) {
			out.verdict = RenderPipeline::BonePipelineVerdict::INVALID_TORN_SNAPSHOT;
			out.rejectReason = "pose_cs_discontinuity";
			LogBoneSnapshotCompact(out, sampleN, havePrevTrace ? prevTrace : nullptr, havePrevTrace);
			return;
		}
		const float motion = headCsJump > pelvisCsJump ? headCsJump : pelvisCsJump;
		out.verdict = motion < 3.f ? RenderPipeline::BonePipelineVerdict::VALID_STABLE
		                           : RenderPipeline::BonePipelineVerdict::VALID_ANIMATING;
	} else {
		out.verdict = RenderPipeline::BonePipelineVerdict::VALID_STABLE;
	}

	for (int bone : kBoneTraceIndices)
		prevSnap.traceWorld[bone] = BoneWorldFromSnapshotTransforms(out, bone);
	prevSnap.headWorld = out.headWorld;
	prevSnap.pelvisWorld = out.pelvisWorld;
	prevSnap.headCs = headCs;
	prevSnap.pelvisCs = pelvisCs;
	prevSnap.c2wTranslation = out.c2w.translation;
	prevSnap.poseSlot = out.pose.MeshOffset;
	prevSnap.mesh = mesh;
	prevSnap.have = true;

	out.acquired = true;
	out.rejectReason = nullptr;
	RememberBonePoseSite(out.poseMesh, out.pose);
	RememberStickyBoneSnapshot(mesh, out);
	LogBoneSnapshotCompact(out, sampleN, havePrevTrace ? prevTrace : nullptr, havePrevTrace);
}

inline bool RefreshStickyBonePoseLive(uintptr_t renderMesh, BoneFrameSnapshot& sticky) {
	if (!renderMesh || !sticky.acquired || sticky.bones.empty() || !sticky.pose.MeshOffset)
		return false;
	const MeshTransformSnapshot c2w = DiagnoseComponentToWorld(renderMesh);
	if (!c2w.valid)
		return false;
	const uintptr_t poseMesh = sticky.poseMesh ? sticky.poseMesh : renderMesh;
	FUeTArrayHeader hdr{};
	if (!ReadUeTArrayHeader(poseMesh + sticky.pose.MeshOffset, hdr) ||
	    !UeTArrayHeaderPlausible(hdr) || hdr.Num != sticky.pose.Num ||
	    hdr.Num != static_cast<int>(sticky.bones.size()))
		return false;
	if (!Memory::Process.Read(hdr.Data, sticky.bones.data(),
	                          static_cast<DWORD>(hdr.Num * sizeof(FTransform))))
		return false;
	sticky.c2w = c2w;
	sticky.pose.Data = hdr.Data;
	sticky.headWorld = BoneWorldFromSnapshotTransforms(sticky, EBoneIndex::Head);
	sticky.pelvisWorld = BoneWorldFromSnapshotTransforms(sticky, EBoneIndex::Pelvis);
	return !BoneWorldMissing(sticky.headWorld) && !BoneWorldMissing(sticky.pelvisWorld);
}

inline void CommitBonePipelineForRenderMesh(uintptr_t renderMesh, const BoneFrameSnapshot& snap) {
	const uintptr_t localMesh =
	    LocalPtrs::Player ? Read<uintptr_t>(LocalPtrs::Player + Offsets::Mesh) : 0;
	if (!localMesh || renderMesh != localMesh)
		return;
	if (snap.acquired && RenderPipeline::BoneVerdictConsumable(snap.verdict))
		RenderPipeline::UpdateLocalBonePipeline(snap.verdict, nullptr);
	else
		RenderPipeline::UpdateLocalBonePipeline(snap.verdict,
		                                        snap.rejectReason ? snap.rejectReason : "unknown");
}

inline BoneFrameSnapshot& GetBoneFrameSnapshot(uintptr_t mesh) {
	static thread_local std::unordered_map<uintptr_t, BoneFrameSnapshot> byMesh;
	const unsigned frame = EspFrameCounter.load(std::memory_order_relaxed);
	const auto it = byMesh.find(mesh);
	if (it != byMesh.end() && it->second.frame == frame)
		return it->second;

	BoneFrameSnapshot sticky{};
	const bool haveSticky = TryConsumeStickyBoneSnapshot(mesh, frame, sticky);

	BoneFrameSnapshot fresh{};
	AcquireBoneFrameSnapshot(mesh, fresh);
	const bool consumable =
	    fresh.acquired && RenderPipeline::BoneVerdictConsumable(fresh.verdict);
	if (consumable) {
		fresh.frame = frame;
		byMesh[mesh] = std::move(fresh);
		CommitBonePipelineForRenderMesh(mesh, byMesh[mesh]);
		return byMesh[mesh];
	}
	if (haveSticky && EspPoseOffsetOk(sticky.pose.MeshOffset)) {
		if (RefreshStickyBonePoseLive(mesh, sticky))
			RememberStickyBoneSnapshot(mesh, sticky);
		sticky.frame = frame;
		byMesh[mesh] = std::move(sticky);
		CommitBonePipelineForRenderMesh(mesh, byMesh[mesh]);
		return byMesh[mesh];
	}
	fresh.frame = frame;
	byMesh[mesh] = std::move(fresh);
	CommitBonePipelineForRenderMesh(mesh, byMesh[mesh]);
	return byMesh[mesh];
}

inline bool BoneSnapshotOkForRender(uintptr_t mesh) {
	const BoneFrameSnapshot& snap = GetBoneFrameSnapshot(mesh);
	return snap.acquired && RenderPipeline::BoneVerdictConsumable(snap.verdict) &&
	       !snap.usesBoneSpaceFromData && EspPoseOffsetOk(snap.pose.MeshOffset);
}

Vector3 GetBoneWithRotation(uintptr_t mesh, int bone_id) {
	if (!mesh || !IsPlausibleUObject(mesh) || bone_id < 0)
		return Vector3{};
	const BoneFrameSnapshot& snap = GetBoneFrameSnapshot(mesh);
	if (!snap.acquired || !RenderPipeline::BoneVerdictConsumable(snap.verdict))
		return Vector3{};
	if (bone_id >= snap.pose.Num || bone_id >= static_cast<int>(snap.bones.size()))
		return Vector3{};
	const Vector3 world = BoneWorldFromSnapshotTransforms(snap, bone_id);
	if (BoneWorldMissing(world))
		return Vector3{};
	const char* plausibleReason = nullptr;
	if (!BoneWorldPlausibleVsMesh(world, snap.c2w.translation, &plausibleReason))
		return Vector3{};
	const uintptr_t localMesh =
	    LocalPtrs::Player ? Read<uintptr_t>(LocalPtrs::Player + Offsets::Mesh) : 0;
	if (localMesh && mesh == localMesh && bone_id == EBoneIndex::Head)
		LogBoneTransformStateOnChange(mesh, snap.c2w, world, snap.pelvisWorld, nullptr);
	return world;
}

// Full-height ESP anchors: head top + lowest foot (pelvis-only boxes miss ~half the mesh on Retrac).
inline bool TryGetEspBoxWorldExtents(uintptr_t mesh, Vector3& outTop, Vector3& outBottom) {
	Vector3 head = GetBoneWithRotation(mesh, EBoneIndex::Head);
	if (BoneWorldMissing(head))
		return false;
	outTop = head;
	outTop.z += 12.f;

	const Vector3 footL = GetBoneWithRotation(mesh, EBoneIndex::Foot_L);
	const Vector3 footR = GetBoneWithRotation(mesh, EBoneIndex::Foot_R);
	const bool haveL = !BoneWorldMissing(footL);
	const bool haveR = !BoneWorldMissing(footR);
	if (haveL && haveR) {
		outBottom.x = (footL.x + footR.x) * 0.5f;
		outBottom.y = (footL.y + footR.y) * 0.5f;
		outBottom.z = (footL.z < footR.z) ? footL.z : footR.z;
	} else if (haveL)
		outBottom = footL;
	else if (haveR)
		outBottom = footR;
	else {
		outBottom = GetBoneWithRotation(mesh, EBoneIndex::Pelvis);
		if (BoneWorldMissing(outBottom))
			outBottom = GetBoneWithRotation(mesh, EBoneIndex::Root);
	}
	if (BoneWorldMissing(outBottom))
		return false;
	outBottom.z -= 6.f;
	return outTop.z > outBottom.z;
}

struct EspScreenBounds {
	float centerX = 0.f;
	float topY = 0.f;
	float width = 0.f;
	float height = 0.f;
	float bottomY = 0.f;
	bool ok = false;
};

// Retrac 0x488 pose reads a compact rig; reach vs spine on the same snapshot estimates mesh bulk.
inline float ComputeEspVisualScaleFromSnapshot(const BoneFrameSnapshot& snap) {
	if (!snap.c2w.valid || snap.pose.Num <= EBoneIndex::Foot_L)
		return 1.f;
	if (static_cast<size_t>(EBoneIndex::Head) >= snap.bones.size() ||
	    static_cast<size_t>(EBoneIndex::Pelvis) >= snap.bones.size())
		return 1.f;
	const Vector3 pelvisCs = snap.bones[static_cast<size_t>(EBoneIndex::Pelvis)].translation;
	const Vector3 headCs = snap.bones[static_cast<size_t>(EBoneIndex::Head)].translation;
	const float csSpine = Vec3Distance(pelvisCs, headCs);
	if (csSpine < 8.f)
		return 1.f;
	const auto csReachOk = [&](int boneId) -> bool {
		if (boneId < 0 || boneId >= snap.pose.Num ||
		    static_cast<size_t>(boneId) >= snap.bones.size())
			return false;
		return Vec3Distance(pelvisCs, snap.bones[static_cast<size_t>(boneId)].translation) <=
		       csSpine * 1.08f;
	};
	const Vector3 pelvis = BoneWorldFromSnapshotTransforms(snap, EBoneIndex::Pelvis);
	const Vector3 head = BoneWorldFromSnapshotTransforms(snap, EBoneIndex::Head);
	const Vector3 handL = BoneWorldFromSnapshotTransforms(snap, EBoneIndex::Hand_L);
	const Vector3 handR = BoneWorldFromSnapshotTransforms(snap, EBoneIndex::Hand_R);
	const Vector3 footL = BoneWorldFromSnapshotTransforms(snap, EBoneIndex::Foot_L);
	if (BoneWorldMissing(pelvis) || BoneWorldMissing(head))
		return 1.f;
	const float spine = Vec3Distance(pelvis, head);
	if (spine < 8.f)
		return 1.f;
	float scale = 1.f;
	if (!BoneWorldMissing(handL) && csReachOk(EBoneIndex::Hand_L)) {
		const float arm = Vec3Distance(pelvis, handL);
		if (arm > 1.f) {
			const float target = spine * 0.88f;
			const float s = target / arm;
			if (s > scale)
				scale = s;
		}
	}
	if (!BoneWorldMissing(handR) && csReachOk(EBoneIndex::Hand_R)) {
		const float arm = Vec3Distance(pelvis, handR);
		if (arm > 1.f) {
			const float target = spine * 0.88f;
			const float s = target / arm;
			if (s > scale)
				scale = s;
		}
	}
	if (!BoneWorldMissing(footL) && csReachOk(EBoneIndex::Foot_L)) {
		const float leg = Vec3Distance(pelvis, footL);
		if (leg > 1.f) {
			const float target = spine * 1.02f;
			const float s = target / leg;
			if (s > scale)
				scale = s;
		}
	}
	Vector3 headTop = head;
	headTop.z += 12.f;
	const float headReach = Vec3Distance(pelvis, headTop);
	if (headReach > 1.f) {
		const float target = spine * 1.12f;
		const float s = target / headReach;
		if (s > scale)
			scale = s;
	}
	float boneSpan = spine * 2.2f;
	if (!BoneWorldMissing(footL)) {
		const float zSpan = head.z - footL.z;
		if (zSpan > 40.f)
			boneSpan = zSpan;
	}
	if (snap.verdict == RenderPipeline::BonePipelineVerdict::VALID_STABLE) {
		struct FBoxSphereBoundsProbe {
			Vector3 origin;
			Vector3 extent;
			float sphereRadius = 0.f;
		};
		static const uint32_t kBoundsOffsets[] = { 0x0188u, 0x0160u, 0x0100u, 0x0140u };
		for (uint32_t off : kBoundsOffsets) {
			FBoxSphereBoundsProbe b{};
			if (!Memory::Process.ReadRequestOk(snap.mesh + off, b))
				continue;
			if (b.extent.x < 12.f || b.extent.x > 220.f || b.extent.z < 25.f || b.extent.z > 280.f)
				continue;
			const float boundsHeight = b.extent.z * 2.f;
			if (boneSpan < 10.f || boundsHeight < boneSpan)
				continue;
			const float s = boundsHeight / boneSpan;
			if (s > scale)
				scale = s;
			break;
		}
	}
	if (scale < 1.f)
		scale = 1.f;
	if (scale > 1.45f)
		scale = 1.45f;
	return scale;
}

inline float ResolveStableEspVisualScale(const BoneFrameSnapshot& snap) {
	static thread_local uintptr_t tlsMesh = 0;
	static thread_local float tlsScale = 1.f;
	static thread_local bool have = false;
	if (snap.mesh != tlsMesh) {
		tlsMesh = snap.mesh;
		have = false;
		tlsScale = 1.f;
	}
	if (snap.verdict != RenderPipeline::BonePipelineVerdict::VALID_STABLE)
		return have ? tlsScale : 1.f;
	const float raw = ComputeEspVisualScaleFromSnapshot(snap);
	if (!have) {
		tlsScale = raw;
		have = true;
		return tlsScale;
	}
	float delta = raw - tlsScale;
	if (delta > 0.04f)
		delta = 0.04f;
	if (delta < -0.04f)
		delta = -0.04f;
	tlsScale += delta;
	return tlsScale;
}

inline void SmoothEspScreenBounds(uintptr_t mesh, EspScreenBounds& bounds, bool fastFollow = false) {
	static thread_local uintptr_t tlsMesh = 0;
	static thread_local EspScreenBounds prev{};
	static thread_local bool havePrev = false;
	if (mesh != tlsMesh) {
		tlsMesh = mesh;
		havePrev = false;
	}
	if (!havePrev) {
		prev = bounds;
		havePrev = true;
		return;
	}
	const float posStep = fastFollow ? 28.f : 12.f;
	const float sizeStep = fastFollow ? 20.f : 9.f;
	const auto stepToward = [](float cur, float target, float maxDelta) -> float {
		float d = target - cur;
		if (d > maxDelta)
			d = maxDelta;
		if (d < -maxDelta)
			d = -maxDelta;
		return cur + d;
	};
	prev.centerX = stepToward(prev.centerX, bounds.centerX, posStep);
	prev.topY = stepToward(prev.topY, bounds.topY, posStep);
	prev.width = stepToward(prev.width, bounds.width, sizeStep);
	prev.height = stepToward(prev.height, bounds.height, sizeStep);
	prev.bottomY = prev.topY + prev.height;
	bounds = prev;
}

inline Vector3 EspVisualScaleWorldPoint(const BoneFrameSnapshot& snap, const Vector3& world,
                                        float scale) {
	if (scale <= 1.0001f || BoneWorldMissing(world))
		return world;
	const Vector3 pelvis = BoneWorldFromSnapshotTransforms(snap, EBoneIndex::Pelvis);
	if (BoneWorldMissing(pelvis))
		return world;
	return Vector3(pelvis.x + (world.x - pelvis.x) * scale, pelvis.y + (world.y - pelvis.y) * scale,
	               pelvis.z + (world.z - pelvis.z) * scale);
}

inline Vector3 BoneWorldVisualFromSnapshot(const BoneFrameSnapshot& snap, int bone_id, float scale) {
	return EspVisualScaleWorldPoint(snap, BoneWorldFromSnapshotTransforms(snap, bone_id), scale);
}

inline bool BoneSnapshotOkForRender(const BoneFrameSnapshot& snap) {
	return snap.acquired && RenderPipeline::BoneVerdictConsumable(snap.verdict) &&
	       !snap.usesBoneSpaceFromData && EspPoseOffsetOk(snap.pose.MeshOffset);
}

// Mannequin fallback when ref-skeleton parent RPM fails (torso uses spine chain + head).
inline void CollectMannequinSkeletonWorldLines(uintptr_t mesh,
                                               std::vector<std::pair<Vector3, Vector3>>& lines) {
	lines.clear();
	if (!mesh || !IsPlausibleUObject(mesh))
		return;
	static const std::pair<int, int> kEdges[] = {
	    { EBoneIndex::Pelvis, EBoneIndex::Spine_03 },
	    { EBoneIndex::Spine_03, EBoneIndex::Spine_05 },
	    { EBoneIndex::Spine_05, EBoneIndex::Neck_01 },
	    { EBoneIndex::Neck_01, EBoneIndex::Head },
	    { EBoneIndex::Neck_01, EBoneIndex::UpperArm_L },
	    { EBoneIndex::UpperArm_L, EBoneIndex::LowerArm_L },
	    { EBoneIndex::LowerArm_L, EBoneIndex::Hand_L },
	    { EBoneIndex::Neck_01, EBoneIndex::UpperArm_R },
	    { EBoneIndex::UpperArm_R, EBoneIndex::LowerArm_R },
	    { EBoneIndex::LowerArm_R, EBoneIndex::Hand_R },
	    { EBoneIndex::Pelvis, EBoneIndex::Thigh_L },
	    { EBoneIndex::Thigh_L, EBoneIndex::Calf_L },
	    { EBoneIndex::Calf_L, EBoneIndex::Foot_L },
	    { EBoneIndex::Pelvis, EBoneIndex::Thigh_R },
	    { EBoneIndex::Thigh_R, EBoneIndex::Calf_R },
	    { EBoneIndex::Calf_R, EBoneIndex::Foot_R },
	};
	lines.reserve(sizeof(kEdges) / sizeof(kEdges[0]));
	for (const auto& edge : kEdges) {
		Vector3 a = GetBoneWithRotation(mesh, edge.first);
		Vector3 b = GetBoneWithRotation(mesh, edge.second);
		if (BoneWorldMissing(a) || BoneWorldMissing(b))
			continue;
		if (Vec3Distance(a, b) > 280.f)
			continue;
		lines.emplace_back(a, b);
	}
}

inline void CollectSkeletonHierarchyWorldLines(
    uintptr_t mesh, std::vector<std::pair<Vector3, Vector3>>& lines) {
	lines.clear();
	if (!mesh || !IsPlausibleUObject(mesh))
		return;
	const uintptr_t poseMesh = ResolveSkeletonMesh(mesh);
	ResolvedBoneArray pose{};
	if (!ResolveBonePoseArray(poseMesh, pose) || pose.Num < 2)
		return;
	const std::vector<int32_t>* parents = GetCachedRefSkeletonParents(poseMesh, pose.Num);
	if (!parents || static_cast<int>(parents->size()) != pose.Num)
		return;

	const int boneCount = pose.Num;
	std::vector<Vector3> world(static_cast<size_t>(boneCount));
	for (int i = 0; i < boneCount; ++i)
		world[static_cast<size_t>(i)] = GetBoneWithRotation(mesh, i);

	lines.reserve(static_cast<size_t>(boneCount));
	for (int i = 0; i < boneCount; ++i) {
		const int parent = (*parents)[static_cast<size_t>(i)];
		if (parent < 0 || parent >= boneCount || parent == i)
			continue;
		const Vector3& childW = world[static_cast<size_t>(i)];
		const Vector3& parentW = world[static_cast<size_t>(parent)];
		if (BoneWorldMissing(childW) || BoneWorldMissing(parentW))
			continue;
		if (Vec3Distance(childW, parentW) > 280.f)
			continue;
		lines.emplace_back(parentW, childW);
	}
}

inline void CollectSkeletonWorldLines(uintptr_t mesh,
                                      std::vector<std::pair<Vector3, Vector3>>& lines) {
	lines.clear();
	if (!mesh || !IsPlausibleUObject(mesh))
		return;
	const uintptr_t poseMesh = ResolveSkeletonMesh(mesh);
	ResolvedBoneArray pose{};
	const bool havePose = ResolveBonePoseArray(poseMesh, pose) && pose.Num >= 2;
	if (havePose && RefSkeletonParentsTrusted(poseMesh, pose.Num)) {
		CollectSkeletonHierarchyWorldLines(mesh, lines);
		if (lines.size() >= 20)
			return;
		lines.clear();
	}
	CollectMannequinSkeletonWorldLines(mesh, lines);
}

inline void CollectMannequinSkeletonLinesFromSnapshot(
    const BoneFrameSnapshot& snap, std::vector<std::pair<Vector3, Vector3>>& lines) {
	lines.clear();
	if (!BoneSnapshotOkForRender(snap))
		return;
	static const std::pair<int, int> kEdges[] = {
	    { EBoneIndex::Pelvis, EBoneIndex::Spine_03 },
	    { EBoneIndex::Spine_03, EBoneIndex::Spine_05 },
	    { EBoneIndex::Spine_05, EBoneIndex::Neck_01 },
	    { EBoneIndex::Neck_01, EBoneIndex::Head },
	    { EBoneIndex::Neck_01, EBoneIndex::UpperArm_L },
	    { EBoneIndex::UpperArm_L, EBoneIndex::LowerArm_L },
	    { EBoneIndex::LowerArm_L, EBoneIndex::Hand_L },
	    { EBoneIndex::Neck_01, EBoneIndex::UpperArm_R },
	    { EBoneIndex::UpperArm_R, EBoneIndex::LowerArm_R },
	    { EBoneIndex::LowerArm_R, EBoneIndex::Hand_R },
	    { EBoneIndex::Pelvis, EBoneIndex::Thigh_L },
	    { EBoneIndex::Thigh_L, EBoneIndex::Calf_L },
	    { EBoneIndex::Calf_L, EBoneIndex::Foot_L },
	    { EBoneIndex::Pelvis, EBoneIndex::Thigh_R },
	    { EBoneIndex::Thigh_R, EBoneIndex::Calf_R },
	    { EBoneIndex::Calf_R, EBoneIndex::Foot_R },
	};
	lines.reserve(sizeof(kEdges) / sizeof(kEdges[0]));
	constexpr float kSkeletonScale = 1.75f;
	for (const auto& edge : kEdges) {
		Vector3 a = BoneWorldVisualFromSnapshot(snap, edge.first, kSkeletonScale);
		Vector3 b = BoneWorldVisualFromSnapshot(snap, edge.second, kSkeletonScale);
		if (BoneWorldMissing(a) || BoneWorldMissing(b))
			continue;
		if (Vec3Distance(a, b) > 280.f)
			continue;
		lines.emplace_back(a, b);
	}
}

inline void CollectSkeletonWorldLinesFromSnapshot(
    const BoneFrameSnapshot& snap, std::vector<std::pair<Vector3, Vector3>>& lines) {
	lines.clear();
	if (!BoneSnapshotOkForRender(snap))
		return;
	const uintptr_t poseMesh = snap.poseMesh;
	if (RefSkeletonParentsTrusted(poseMesh, snap.pose.Num)) {
		const std::vector<int32_t>* parents = GetCachedRefSkeletonParents(poseMesh, snap.pose.Num);
		if (parents && static_cast<int>(parents->size()) == snap.pose.Num) {
			lines.reserve(static_cast<size_t>(snap.pose.Num));
			constexpr float kSkeletonScale = 1.75f;
			for (int i = 0; i < snap.pose.Num; ++i) {
				const int parent = (*parents)[static_cast<size_t>(i)];
				if (parent < 0 || parent >= snap.pose.Num || parent == i)
					continue;
				const Vector3 childW = BoneWorldVisualFromSnapshot(snap, i, kSkeletonScale);
				const Vector3 parentW = BoneWorldVisualFromSnapshot(snap, parent, kSkeletonScale);
				if (BoneWorldMissing(childW) || BoneWorldMissing(parentW))
					continue;
				if (Vec3Distance(childW, parentW) > 280.f)
					continue;
				lines.emplace_back(parentW, childW);
			}
			if (lines.size() >= 20)
				return;
			lines.clear();
		}
	}
	CollectMannequinSkeletonLinesFromSnapshot(snap, lines);
}

// 14.60 UWorld has no CameraLocation/CameraRotation. Chain is UWorld ->
// OwningGameInstance 0x180 -> LocalPlayers 0x38 -> PlayerController 0x30 ->
// PlayerCameraManager 0x2B8. FMinimalViewInfo sits at field+0x10
// (Location +0, Rotation +0xC, FOV +0x18).
// CameraCache 0x0290 is the default POV (0,0,0 FOV 90). It is not projected.
inline float CameraLocationMaxAbs(const Vector3& location) {
	float ax = location.x < 0.f ? -location.x : location.x;
	float ay = location.y < 0.f ? -location.y : location.y;
	float az = location.z < 0.f ? -location.z : location.z;
	float m = ax;
	if (ay > m) m = ay;
	if (az > m) m = az;
	return m;
}

inline bool CameraPovUsable(const Vector3& location, float fov) {
	if (!(fov > 1.f && fov < 179.f))
		return false;
	return CameraLocationMaxAbs(location) > 100.f;
}

inline bool MeshHasPlausibleComponentToWorld(uintptr_t mesh) {
	if (!IsPlausibleUObject(mesh))
		return false;
	FTransform c2w{};
	if (!Memory::Process.ReadRequestOk(mesh + Offsets::ComponentToWorld, c2w))
		return false;
	if (!QuatNormalized(c2w.rot))
		return false;
	const float span = CameraLocationMaxAbs(c2w.translation);
	return span > 100.f && span < 5e7f;
}

inline bool MeshHasPlausibleWorldLocation(uintptr_t mesh) {
	if (!IsPlausibleUObject(mesh))
		return false;
	const Vector3 loc = GetMeshWorldLocation(mesh);
	const float span = CameraLocationMaxAbs(loc);
	return span > 100.f && span < 5e7f;
}

struct SdkPovField {
	const char* name;
	uint32_t offset;
	bool live;
};

inline const SdkPovField* SdkPovFields(int& count) {
	// Documented APlayerCameraManager cache members only (public caches first for stock UE paths).
	static const SdkPovField fields[] = {
		{ "CameraCache", static_cast<uint32_t>(Offsets::CameraCache), true },
		{ "LastFrameCameraCache", static_cast<uint32_t>(Offsets::LastFrameCameraCache), true },
		{ "CameraCachePrivate", static_cast<uint32_t>(Offsets::camera_cache_private), true },
		{ "LastFrameCameraCachePrivate", static_cast<uint32_t>(Offsets::LastFrameCameraCachePrivate),
		  true },
		{ "ViewTarget", static_cast<uint32_t>(Offsets::ViewTarget), true },
		{ "PendingViewTarget", static_cast<uint32_t>(Offsets::PendingViewTarget), true },
	};
	count = static_cast<int>(sizeof(fields) / sizeof(fields[0]));
	return fields;
}

inline const char* SdkNameForFovOffset(uint32_t fovOffset) {
	int count = 0;
	const SdkPovField* fields = SdkPovFields(count);
	for (int i = 0; i < count; ++i) {
		if (fields[i].offset + 0x10 + 0x18 == fovOffset)
			return fields[i].name;
	}
	if (fovOffset == static_cast<uint32_t>(Offsets::DefaultFOV))
		return "DefaultFOV";
	return nullptr;
}

inline void ReadManagerPovAt(uintptr_t pcm, uint32_t cacheEntryOffset, uint32_t povDeltaFromEntry,
                             Vector3& location, Vector3& rotation, float& fov, float* outAspect = nullptr) {
	const uintptr_t povBase = pcm + cacheEntryOffset + povDeltaFromEntry;
	location = Read<Vector3>(povBase + PcmSdkLayout::kPovLocation);
	rotation = Read<Vector3>(povBase + PcmSdkLayout::kPovRotation);
	fov = Read<float>(povBase + PcmSdkLayout::kPovFov);
	if (outAspect) {
		const float aspect = Read<float>(povBase + PcmSdkLayout::kPovAspectRatio);
		*outAspect = (aspect >= 0.5f && aspect <= 3.f) ? aspect : 0.f;
	}
}

inline void ReadManagerPovFromSdkCacheEntry(uintptr_t pcm, uint32_t cacheEntryOffset,
                                            Vector3& location, Vector3& rotation, float& fov,
                                            float* outAspect = nullptr) {
	ReadManagerPovAt(pcm, cacheEntryOffset, PcmSdkLayout::kCacheEntryPov, location, rotation, fov,
	                 outAspect);
}

inline void ReadManagerPov(uintptr_t pcm, uint32_t fieldOffset, Vector3& location, Vector3& rotation,
                           float& fov) {
	ReadManagerPovAt(pcm, fieldOffset, 0x10, location, rotation, fov);
}

inline bool PovBlockPlausible(const Vector3& location, const Vector3& rotation, float fov);
inline bool TryReadControlRotation(uintptr_t playerController, Vector3& rotation,
                                   uint32_t& outOffset);

struct UWorldCameraSample {
	const char* name;
	uint32_t fieldOffset;
	Vector3 location;
	Vector3 rotation;
	float fov;
	uintptr_t pcm;
	bool resolved;
	bool usable;
};

inline UWorldCameraSample ReadUWorldCamera(uintptr_t world) {
	UWorldCameraSample sample{};
	sample.name = "CameraCachePrivate";
	sample.fieldOffset = static_cast<uint32_t>(Offsets::camera_cache_private);
	if (!world || !Memory::IsValid(world))
		return sample;
	const uintptr_t gameInstance = Read<uintptr_t>(world + Offsets::OwningGameInstance);
	if (!gameInstance || !Memory::IsValid(gameInstance))
		return sample;
	const uintptr_t localPlayers = Read<uintptr_t>(gameInstance + Offsets::LocalPlayers);
	const uintptr_t localPlayer = Read<uintptr_t>(localPlayers);
	if (!localPlayer || !Memory::IsValid(localPlayer))
		return sample;
	const uintptr_t playerController = Read<uintptr_t>(localPlayer + Offsets::PlayerController);
	if (!playerController || !Memory::IsValid(playerController))
		return sample;
	const uintptr_t pcm = Read<uintptr_t>(playerController + Offsets::playercameramanager);
	if (!pcm || !Memory::IsValid(pcm))
		return sample;
	LocalPtrs::PlayerCam = pcm;
	sample.pcm = pcm;
	sample.resolved = true;

	int count = 0;
	const SdkPovField* fields = SdkPovFields(count);
	bool sawLive = false;
	for (int i = 0; i < count; ++i) {
		if (!fields[i].live)
			continue;
		Vector3 location{};
		Vector3 rotation{};
		float fov = 0.f;
		ReadManagerPov(pcm, fields[i].offset, location, rotation, fov);
		if (!sawLive) {
			sample.name = fields[i].name;
			sample.fieldOffset = fields[i].offset;
			sample.location = location;
			sample.rotation = rotation;
			sample.fov = fov;
			sample.usable = CameraPovUsable(location, fov);
			sawLive = true;
		}
		if (!CameraPovUsable(location, fov))
			continue;
		if (!PovBlockPlausible(location, rotation, fov))
			continue;
		sample.name = fields[i].name;
		sample.fieldOffset = fields[i].offset;
		sample.location = location;
		sample.rotation = rotation;
		sample.fov = fov;
		sample.usable = true;
		return sample;
	}
	sample.usable = false;
	return sample;
}

inline uintptr_t LocalPlayerFromWorld(uintptr_t world) {
	if (!world || !Memory::IsValid(world))
		return 0;
	const uintptr_t gameInstance = Read<uintptr_t>(world + Offsets::OwningGameInstance);
	if (!gameInstance || !Memory::IsValid(gameInstance))
		return 0;
	const uintptr_t localPlayers = Read<uintptr_t>(gameInstance + Offsets::LocalPlayers);
	const uintptr_t localPlayer = Read<uintptr_t>(localPlayers);
	if (!localPlayer || !Memory::IsValid(localPlayer))
		return 0;
	return localPlayer;
}

inline uintptr_t PlayerControllerFromWorld(uintptr_t world) {
	const uintptr_t localPlayer = LocalPlayerFromWorld(world);
	if (!localPlayer)
		return 0;
	const uintptr_t playerController = Read<uintptr_t>(localPlayer + Offsets::PlayerController);
	if (!playerController || !Memory::IsValid(playerController))
		return 0;
	return playerController;
}

struct CameraCandidate {
	uint32_t fovOffset;
	const char* sdkName;
	Vector3 location;
	float fov;
};

inline int CollectCameraCandidates(uintptr_t /*pcm*/, CameraCandidate* /*out*/, int /*cap*/) {
	// Heuristic FOV-offset scanning disabled: use SdkPovFields + PcmSdkLayout only.
	return 0;
}

// ---------------------------------------------------------------------------
// POV scan. Every documented POV field on the 14.60 PlayerCameraManager reads back the
// 0,0,0 FOV 90 template in a live Retrac match, so the live FMinimalViewInfo is found by
// shape and scored against the local pawn, then its offset is cached. The POV itself is
// re-read every frame, so it tracks the player instead of pinning to a lobby location.
// ---------------------------------------------------------------------------
inline constexpr float kCameraPawnMaxDistance = 50000.f; // 500m; anything further is another view
inline constexpr float kCameraNearPawnMaxDistance = 4000.f; // third-person / eye POV should be here
// Pre-match carrier + in-match TPS: eye is ~250–400uu behind the pawn, not a sky rig 450uu away.
inline constexpr float kGameplayEyeMaxDistFromPawn = 420.f;
inline constexpr int kPovScanBytes = 0x2A00; // through FreeCamDistance @ PCM+0x26A0 (Retrac SDK)

inline bool FiniteF(float v) {
	return v == v && v > -3.0e38f && v < 3.0e38f;
}

inline bool FiniteVec3(const Vector3& v) {
	return FiniteF(v.x) && FiniteF(v.y) && FiniteF(v.z);
}

inline float Dist3(const Vector3& a, const Vector3& b) {
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	const float dz = a.z - b.z;
	return sqrtf(dx * dx + dy * dy + dz * dz);
}

// FMinimalViewInfo shape: finite location inside the playable volume, an FRotator in range,
// a plausible FOV. The all-zero rotation rejects the manager's template POV, which is the
// block that made 0,0,0 FOV 90 look like a camera.
inline bool RotatorHasAim(const Vector3& rotation) {
	return Absf(rotation.x) > 0.05f || Absf(rotation.y) > 0.05f;
}

inline bool PovBlockPlausible(const Vector3& location, const Vector3& rotation, float fov) {
	if (!(fov >= 50.f && fov <= 120.f))
		return false;
	if (!FiniteVec3(location) || !FiniteVec3(rotation))
		return false;
	const float span = CameraLocationMaxAbs(location);
	if (span <= 100.f || span > 2.0e6f)
		return false;
	if (Absf(rotation.x) > 90.5f || Absf(rotation.y) > 360.5f || Absf(rotation.z) > 180.5f)
		return false;
	if (!RotatorHasAim(rotation))
		return false;
	return true;
}

struct CameraRef {
	Vector3 pawn;
	Vector3 head;
	bool valid = false;
};

inline bool IsCarrierSkyRigPov(const CameraRef& ref, const Vector3& location) {
	return ref.valid && ref.pawn.y > 80000.f && location.y > 80000.f &&
	       Dist3(location, ref.pawn) <= kCameraNearPawnMaxDistance;
}

inline Vector3 CameraSpatialTarget(const CameraRef& ref) {
	if (!ref.valid)
		return Vector3{};
	if (Dist3(ref.head, ref.pawn) > 400.f)
		return ref.pawn;
	return ref.head;
}

inline void SanitizeControlRotation(Vector3& rotation) {
	if (!FiniteVec3(rotation))
		return;
	rotation.z = 0.f;
	if (rotation.x > 89.f)
		rotation.x = 89.f;
	if (rotation.x < -89.f)
		rotation.x = -89.f;
}

// World-space anchor for camera spatial checks (no bone reads — independent of bone validity).
inline CameraRef ReadCameraSpatialAnchor() {
	CameraRef ref{};
	const uintptr_t pawn = LocalPtrs::Player;
	if (!pawn || !Memory::IsValid(pawn))
		return ref;
	const uintptr_t mesh = Read<uintptr_t>(pawn + Offsets::Mesh);
	if (mesh && Memory::IsValid(mesh)) {
		const MeshTransformSnapshot meshSnap = ResolveMeshTransformSnapshot(mesh, pawn);
		if (meshSnap.valid) {
			ref.pawn = meshSnap.translation;
			ref.head = meshSnap.translation;
			ref.valid = true;
			return ref;
		}
		const Vector3 meshLoc = GetMeshWorldLocation(mesh);
		if (FiniteVec3(meshLoc) && !BoneWorldMissing(meshLoc) &&
		    CameraLocationMaxAbs(meshLoc) > 100.f) {
			ref.pawn = meshLoc;
			ref.head = meshLoc;
			ref.valid = true;
			return ref;
		}
	}
	const uintptr_t root = Read<uintptr_t>(pawn + Offsets::RootComponent);
	if (root && Memory::IsValid(root)) {
		const MeshTransformSnapshot rootSnap = DiagnoseComponentToWorld(root);
		if (rootSnap.valid) {
			ref.pawn = rootSnap.translation;
			ref.head = rootSnap.translation;
			ref.valid = true;
		}
	}
	return ref;
}

// Same world anchor bone ESP uses (mesh C2W); does not require CameraRef.valid.
inline bool ResolveCameraPivot(Vector3& outPivot, Vector3& outHead) {
	const uintptr_t mesh = LocalPtrs::PlayerMesh;
	if (mesh && Memory::IsValid(mesh)) {
		const Vector3 c2w = GetMeshWorldLocation(mesh);
		if (FiniteVec3(c2w) && !BoneWorldMissing(c2w) && CameraLocationMaxAbs(c2w) > 100.f) {
			outPivot = c2w;
			outHead = c2w;
			const Vector3 head = GetBoneWithRotation(mesh, EBoneIndex::Head);
			if (FiniteVec3(head) && !BoneWorldMissing(head))
				outHead = head;
			return true;
		}
	}
	const CameraRef anchor = ReadCameraSpatialAnchor();
	if (!anchor.valid)
		return false;
	outPivot = anchor.pawn;
	outHead = anchor.head;
	return true;
}

inline CameraRef ReadLocalCameraRef() {
	return ReadCameraSpatialAnchor();
}

// Lower is better, negative means unusable. The player's own camera is the POV closest to the
// pawn; a view that cannot see the pawn's head (the projection's own depth test) is tripled so
// a spectator or capture POV at the same range never outranks it.
inline float ScorePov(const Vector3& location, const Vector3& rotation, const CameraRef& ref) {
	if (!ref.valid)
		return -1.f;
	const float distance = Dist3(location, ref.pawn);
	// A POV tens of thousands of units away still "sees" the pawn on a lucky yaw and
	// projects ESP off the screen. Third person stays inside kCameraNearPawnMaxDistance.
	if (!(distance > 1.f && distance < kCameraNearPawnMaxDistance))
		return -1.f;
	const Vector3 target = CameraSpatialTarget(ref);
	const D3DMATRIX m = Matrix(rotation);
	const Vector3 forward(m.m[0][0], m.m[0][1], m.m[0][2]);
	const float depth = (target.x - location.x) * forward.x + (target.y - location.y) * forward.y
	                  + (target.z - location.z) * forward.z;
	return depth >= 1.f ? distance : distance * 3.f;
}

struct ScannedPov {
	uint32_t offset;
	Vector3 location;
	Vector3 rotation;
	float fov;
};

inline void ReadPovAt(uintptr_t object, uint32_t offset, Vector3& location, Vector3& rotation, float& fov) {
	location = Read<Vector3>(object + offset);
	rotation = Read<Vector3>(object + offset + 0xC);
	fov = Read<float>(object + offset + 0x18);
}

// One page-sized read per 0x1000 instead of a driver round trip per offset.
inline int ReadObjectBlock(uintptr_t object, uint8_t* buffer, int size) {
	int read = 0;
	for (int offset = 0; offset < size; offset += 0x1000) {
		const int chunk = (size - offset) < 0x1000 ? (size - offset) : 0x1000;
		if (!Memory::Process.Read(object + offset, buffer + offset, chunk))
			break;
		read = offset + chunk;
	}
	return read;
}

inline bool ScanObjectForPovNearPawn(uintptr_t object, const CameraRef& ref, ScannedPov& out) {
	(void)object;
	(void)ref;
	(void)out;
	return false;
#if 0 // heuristic PCM POV scan disabled
	if (!object || !Memory::IsValid(object) || !ref.valid)
		return false;
	static std::vector<uint8_t> buffer(kPovScanBytes);
	const int read = ReadObjectBlock(object, buffer.data(), kPovScanBytes);
	float bestDist = 1e9f;
	bool found = false;
	for (int offset = 0; offset + 0x1C <= read; offset += 4) {
		float f[7];
		memcpy(f, buffer.data() + offset, sizeof(f));
		const Vector3 location(f[0], f[1], f[2]);
		const float nearDist = Dist3(location, ref.head);
		const float pawnDist = Dist3(location, ref.pawn);
		if (nearDist > kCameraNearPawnMaxDistance && pawnDist > kCameraNearPawnMaxDistance)
			continue;
		const Vector3 rotation(f[3], f[4], f[5]);
		if (!PovBlockPlausible(location, rotation, f[6]))
			continue;
		const float pick = nearDist < pawnDist ? nearDist : pawnDist;
		if (found && pick >= bestDist)
			continue;
		bestDist = pick;
		found = true;
		out = { static_cast<uint32_t>(offset), location, rotation, f[6] };
	}
	return found;
#endif
}

inline bool ScanObjectForPov(uintptr_t object, const CameraRef& ref, ScannedPov& out) {
	(void)object;
	(void)ref;
	(void)out;
	return false;
}

inline std::atomic<uintptr_t> CameraPovObject{ 0 };
inline std::atomic<uint32_t> CameraPovOffset{ 0 };
inline std::atomic<unsigned long long> CameraPovNextScanMs{ 0 };

// A near camera that sees the pawn beats a far one, a POV that cannot see the pawn loses to one
// that can, and the lobby POV is out of range once the match has loaded.
inline bool CameraPovScoringSelfCheck() {
	CameraRef ref{};
	ref.pawn = Vector3(-62518.f, 99733.f, 9900.f);
	ref.head = Vector3(-62518.f, 99733.f, 10043.f);
	ref.valid = true;
	const Vector3 behind(-62818.f, 99733.f, 10143.f); // 300u behind, looking +X at the pawn
	const float scoreNear = ScorePov(behind, Vector3(0.f, 0.1f, 0.f), ref);
	const float scoreAway = ScorePov(behind, Vector3(0.f, 180.f, 0.f), ref);
	const float scoreLobby = ScorePov(Vector3(52.f, 125913.f, 89.f), Vector3(0.f, 0.1f, 0.f), ref);
	return scoreNear > 0.f && scoreAway > scoreNear && scoreLobby < 0.f
	    && !PovBlockPlausible(Vector3(0.f, 0.f, 0.f), Vector3(0.f, 0.f, 0.f), 90.f)
	    && PovBlockPlausible(behind, Vector3(0.f, 0.1f, 0.f), 80.f);
}

inline constexpr int32_t kSdkMinGObjectsElements = 10000;

inline bool LooksLikeAsciiPointer(uint64_t p) {
	if (!p)
		return false;
	int asciiBytes = 0;
	for (int i = 0; i < 8; ++i) {
		const uint8_t b = static_cast<uint8_t>((p >> (8 * i)) & 0xFF);
		if (b >= 0x20 && b <= 0x7E)
			++asciiBytes;
	}
	if (asciiBytes >= 5)
		return true;
	int utf16Chars = 0;
	for (int i = 0; i < 4; ++i) {
		const uint16_t w = static_cast<uint16_t>((p >> (16 * i)) & 0xFFFF);
		if ((w & 0xFF00) == 0) {
			const uint8_t c = static_cast<uint8_t>(w & 0xFF);
			if (c >= 0x20 && c <= 0x7E)
				++utf16Chars;
		}
	}
	return utf16Chars >= 3;
}

inline bool LooksLikeUserObjectPointer(uint64_t p) {
	if (!p || !Memory::IsValid(p))
		return false;
	if (LooksLikeAsciiPointer(p))
		return false;
	const uint64_t high = p >> 48;
	if (high != 0 && high != 1 && high != 2 && high != 7)
		return false;
	if ((p & 7) != 0)
		return false;
	return true;
}

inline bool UObjectPointerPlausible(uintptr_t imageBase, uint32_t sizeOfImage, uint64_t p) {
	if (!LooksLikeUserObjectPointer(p))
		return false;
	if (imageBase && sizeOfImage && p >= imageBase && p < imageBase + sizeOfImage)
		return false;
	if (imageBase && !sizeOfImage && p >= imageBase && p < imageBase + 0x10000000)
		return false;
	return true;
}

inline bool UFieldPointerPlausibleRelaxed(uint64_t p) {
	if (!p || !Memory::IsValid(p))
		return false;
	if ((p & 7) != 0)
		return false;
	const uint64_t high = p >> 48;
	if (high != 0 && high != 1 && high != 2 && high != 7)
		return false;
	return true;
}

struct UWorldFieldSnapshot {
	uint64_t persistentLevel = 0;
	uint64_t owningGameInstance = 0;
	uint64_t gameState = 0;
	uint64_t localPlayersArray = 0;
	uint64_t localPlayer = 0;
	uint64_t playerController = 0;
};

enum class UWorldRejectReason : uint8_t {
	Ok,
	NullWorld,
	WorldNotValid,
	WorldAsciiStrict,
	PersistentInvalid,
	GameInstanceInvalid,
	GameStateInvalid,
	LocalPlayersInvalid,
	LocalPlayerInvalid,
	PlayerControllerInvalid,
};

inline const char* UWorldRejectReasonName(UWorldRejectReason r) {
	switch (r) {
	case UWorldRejectReason::Ok:
		return "ok";
	case UWorldRejectReason::NullWorld:
		return "null world ptr";
	case UWorldRejectReason::WorldNotValid:
		return "world RPM invalid";
	case UWorldRejectReason::WorldAsciiStrict:
		return "ASCII-like world ptr (strict)";
	case UWorldRejectReason::PersistentInvalid:
		return "PersistentLevel invalid";
	case UWorldRejectReason::GameInstanceInvalid:
		return "OwningGameInstance invalid";
	case UWorldRejectReason::GameStateInvalid:
		return "GameState invalid";
	case UWorldRejectReason::LocalPlayersInvalid:
		return "LocalPlayers invalid";
	case UWorldRejectReason::LocalPlayerInvalid:
		return "LocalPlayer invalid";
	case UWorldRejectReason::PlayerControllerInvalid:
		return "PlayerController invalid";
	default:
		return "unknown";
	}
}

inline bool ReadUWorldFieldSnapshot(uint64_t world, UWorldFieldSnapshot& out) {
	out = {};
	if (!world || !Memory::IsValid(world))
		return false;
	out.persistentLevel = Read<uint64_t>(world + Offsets::PersistentLevel);
	out.owningGameInstance = Read<uint64_t>(world + Offsets::OwningGameInstance);
	out.gameState = Read<uint64_t>(world + Offsets::GameState);
	if (out.owningGameInstance && Memory::IsValid(out.owningGameInstance)) {
		out.localPlayersArray = Read<uint64_t>(out.owningGameInstance + Offsets::LocalPlayers);
		if (out.localPlayersArray && Memory::IsValid(out.localPlayersArray)) {
			out.localPlayer = Read<uint64_t>(out.localPlayersArray);
			if (out.localPlayer && Memory::IsValid(out.localPlayer))
				out.playerController = Read<uint64_t>(out.localPlayer + Offsets::PlayerController);
		}
	}
	return true;
}

inline int ScoreUWorldSnapshot(const UWorldFieldSnapshot& s, bool relaxAscii) {
	const auto fieldOk = [&](uint64_t p) -> bool {
		return relaxAscii ? UFieldPointerPlausibleRelaxed(p)
		                  : UObjectPointerPlausible(GlobalImageBase(), 0, p);
	};
	int score = 0;
	if (fieldOk(s.persistentLevel))
		++score;
	if (fieldOk(s.owningGameInstance))
		score += 2;
	if (fieldOk(s.gameState))
		++score;
	if (fieldOk(s.localPlayersArray))
		score += 2;
	if (fieldOk(s.localPlayer))
		score += 2;
	if (fieldOk(s.playerController))
		score += 4;
	return score;
}

inline UWorldRejectReason ClassifyUWorldCandidate(uint64_t world, bool relaxAscii,
                                                  UWorldFieldSnapshot* snapOut = nullptr,
                                                  int* scoreOut = nullptr) {
	if (!world)
		return UWorldRejectReason::NullWorld;
	if (!Memory::IsValid(world))
		return UWorldRejectReason::WorldNotValid;
	if (!relaxAscii) {
		if (LooksLikeAsciiPointer(world))
			return UWorldRejectReason::WorldAsciiStrict;
		if (!UObjectPointerPlausible(GlobalImageBase(), 0, world))
			return UWorldRejectReason::WorldNotValid;
	} else if (!UFieldPointerPlausibleRelaxed(world))
		return UWorldRejectReason::WorldNotValid;

	UWorldFieldSnapshot snap{};
	if (!ReadUWorldFieldSnapshot(world, snap))
		return UWorldRejectReason::WorldNotValid;
	if (snapOut)
		*snapOut = snap;

	const auto fieldOk = [&](uint64_t p) -> bool {
		return relaxAscii ? UFieldPointerPlausibleRelaxed(p)
		                  : UObjectPointerPlausible(GlobalImageBase(), 0, p);
	};

	if (!fieldOk(snap.persistentLevel))
		return UWorldRejectReason::PersistentInvalid;
	if (!fieldOk(snap.owningGameInstance))
		return UWorldRejectReason::GameInstanceInvalid;

	const int score = ScoreUWorldSnapshot(snap, relaxAscii);
	if (scoreOut)
		*scoreOut = score;

	if (score >= 3)
		return UWorldRejectReason::Ok;

	if (snap.gameState && !fieldOk(snap.gameState))
		return UWorldRejectReason::GameStateInvalid;
	if (snap.localPlayersArray && !fieldOk(snap.localPlayersArray))
		return UWorldRejectReason::LocalPlayersInvalid;
	if (snap.localPlayer && !fieldOk(snap.localPlayer))
		return UWorldRejectReason::LocalPlayerInvalid;
	if (snap.playerController && !fieldOk(snap.playerController))
		return UWorldRejectReason::PlayerControllerInvalid;
	return UWorldRejectReason::GameStateInvalid;
}

inline bool LooksLikeUWorld(uint64_t world) {
	return ClassifyUWorldCandidate(world, false) == UWorldRejectReason::Ok;
}

inline bool LooksLikeUWorldRelaxed(uint64_t world, int minScore = 3) {
	int score = 0;
	const UWorldRejectReason r = ClassifyUWorldCandidate(world, true, nullptr, &score);
	if (r != UWorldRejectReason::Ok)
		return false;
	return score >= minScore;
}

// PE-resolved RVAs override offsets.hpp when a scan finds live globals (defined after TUObjectArray).
namespace SdkPeDiscovery {
inline std::atomic<uintptr_t> GObjectsSlotAbsolute{0};
inline std::atomic<uint64_t> GObjectRvaDiscovered{0};
inline std::atomic<uint64_t> UWorldRvaDiscovered{0};
inline std::atomic<bool> discoveryFinished{false};
inline std::atomic<bool> staticRvasValidated{false};
inline std::atomic<bool> scanFoundLive{false};
inline std::atomic<int> peRejectLogCount{0};
inline std::atomic<int> staticSlotFailPolls{0};
inline std::atomic<bool> peDataScanEnabled{false};
inline std::atomic<bool> peScanGObjectsLogged{false};
inline std::atomic<bool> peScanUWorldLogged{false};
inline constexpr int kMaxPeRejectLogsPerSession = 32;
inline constexpr int kMaxUWorldRejectLogsPerSession = 8;
inline constexpr int kEnablePeScanAfterStaticFailPolls = 45;
inline std::atomic<int> uworldRejectLogCount{0};
inline bool SdkVerbosePeScan = false;

inline bool ShouldLogPeScanReject() {
	if (SdkVerbosePeScan)
		return true;
	return peRejectLogCount.fetch_add(1, std::memory_order_relaxed) < kMaxPeRejectLogsPerSession;
}

inline bool ShouldLogUWorldReject() {
	if (SdkVerbosePeScan)
		return true;
	return uworldRejectLogCount.fetch_add(1, std::memory_order_relaxed)
	       < kMaxUWorldRejectLogsPerSession;
}

inline void LogUWorldCandidateProbe(uint64_t rva, uint64_t world, UWorldRejectReason reason,
                                    const UWorldFieldSnapshot& snap, int score, bool accepted) {
	if (accepted ? !ShouldLogPeScanReject() : !ShouldLogUWorldReject())
		return;
	std::cout << (accepted ? xorstr_("[+] UWorld candidate accept RVA 0x")
	                       : xorstr_("[-] UWorld candidate reject RVA 0x"))
	          << std::hex << std::uppercase << rva << xorstr_(" world 0x") << world << std::dec;
	if (!accepted)
		std::cout << xorstr_(" (") << UWorldRejectReasonName(reason) << xorstr_(")");
	std::cout << xorstr_(" score ") << score << xorstr_(" PersistentLevel 0x") << std::hex
	          << snap.persistentLevel << xorstr_(" GameInstance 0x") << snap.owningGameInstance
	          << xorstr_(" GameState 0x") << snap.gameState << xorstr_(" LocalPlayer 0x")
	          << snap.localPlayer << xorstr_(" PC 0x") << snap.playerController << std::dec
	          << std::endl;
	std::cout.flush();
}

inline void NoteStaticSlotPollMiss() {
	if (peDataScanEnabled.load(std::memory_order_relaxed))
		return;
	const int n = staticSlotFailPolls.fetch_add(1, std::memory_order_relaxed) + 1;
	if (n >= kEnablePeScanAfterStaticFailPolls)
		peDataScanEnabled.store(true, std::memory_order_relaxed);
}

uint64_t GObjectsRva();
uint64_t UWorldRva();
inline void EnsureStaticDiscoveryState(uintptr_t imageBase);
inline void EnsureDiscoveredOffsets(uintptr_t imageBase);
} // namespace SdkPeDiscovery

inline bool StaticGObjectsRpmAtBase(uintptr_t imageBase);

inline uint64_t ReadUWorldFromImage(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return 0;
	const uintptr_t readImage = imageBase;
	SdkPeDiscovery::EnsureStaticDiscoveryState(readImage);
	if (SdkPeDiscovery::peDataScanEnabled.load(std::memory_order_relaxed)
	    && !SdkPeDiscovery::UWorldRvaDiscovered.load(std::memory_order_relaxed)
	    && !SdkPeDiscovery::UWorldRva())
		SdkPeDiscovery::EnsureDiscoveredOffsets(readImage);
	const uint64_t rva = SdkPeDiscovery::UWorldRva();
	if (!rva)
		return 0;
	const uint64_t world = Read<uint64_t>(readImage + rva);
	if (LooksLikeUWorld(world))
		return world;
	if (LooksLikeUWorldRelaxed(world, 3))
		return world;
	return 0;
}

// Lobby often has PersistentLevel before OwningGameInstance; wait path must not require full UWorld.
inline uint64_t ReadUWorldCandidateFromImage(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return 0;
	const uint64_t strict = ReadUWorldFromImage(imageBase);
	if (strict)
		return strict;
	const uintptr_t readImage = imageBase;
	SdkPeDiscovery::EnsureStaticDiscoveryState(readImage);
	const uint64_t rva = SdkPeDiscovery::UWorldRva();
	if (!rva)
		return 0;
	const uint64_t world = Read<uint64_t>(readImage + rva);
	if (!world || !Memory::IsValid(world))
		return 0;
	const uint64_t persistent = Read<uint64_t>(world + Offsets::PersistentLevel);
	if (persistent && Memory::IsValid(persistent))
		return world;
	return 0;
}

// GWorld is swapped when the match loads; the background refresh is up to a second behind, so
// re-read the global here (one read) and only fall back to the cached pointer if it is empty.
inline uintptr_t CurrentWorld() {
	const uintptr_t image = GlobalImageBase();
	if (image) {
		const uint64_t live = ReadUWorldFromImage(image);
		if (live)
			return static_cast<uintptr_t>(live);
		const uint64_t candidate = ReadUWorldCandidateFromImage(image);
		if (candidate)
			return static_cast<uintptr_t>(candidate);
	}
	return LocalPtrs::Gworld;
}

inline void GetOverlayViewportSize(float& sw, float& sh) {
	sw = (float)Settings::Width;
	sh = (float)Settings::Height;
	if (sw < 1.f || sh < 1.f) {
		sw = (float)Width;
		sh = (float)Height;
	}
}

inline float EffectiveProjectionAspect(float aspectRatio, float viewportWidth, float viewportHeight) {
	if (aspectRatio >= 0.5f && aspectRatio <= 3.f)
		return aspectRatio;
	return viewportHeight > 1.f ? viewportWidth / viewportHeight : 16.f / 9.f;
}

inline bool BuildCameraViewProjection(const Vector3& camLoc, const Vector3& camRot, float fov,
                                      float aspectRatio, float viewportWidth, float viewportHeight,
                                      D3DMATRIX* outView, D3DMATRIX* outProj,
                                      D3DMATRIX* outViewProjection) {
	if (fov < 1.f || viewportWidth < 1.f || viewportHeight < 1.f)
		return false;
	if (!FiniteVec3(camLoc) || !FiniteVec3(camRot))
		return false;
	const float aspect = EffectiveProjectionAspect(aspectRatio, viewportWidth, viewportHeight);
	const D3DMATRIX view = BuildUnrealViewMatrix(camLoc, camRot);
	// Retrac FMinimalViewInfo FOV matches vertical FReversedZPerspectiveMatrix (not horizontal).
	const D3DMATRIX proj = BuildUnrealReversedZPerspectiveMatrix(fov, aspect);
	const D3DMATRIX vp = MatrixMultiplication(view, proj);
	if (!Matrix4Finite(view) || !Matrix4Finite(proj) || !Matrix4Finite(vp))
		return false;
	if (outView)
		*outView = view;
	if (outProj)
		*outProj = proj;
	if (outViewProjection)
		*outViewProjection = vp;
	return true;
}

inline bool FloatUsableScalar(float v) {
	if (!FiniteF(v))
		return false;
	const float a = Absf(v);
	return !(a > 0.f && a < 1e-4f);
}

// First failing invariant (nullptr == acceptable for W2S snapshot).
inline constexpr float kCameraFovDegreesMin = 1.f;
inline constexpr float kCameraFovDegreesMax = 179.f;

inline const char* ValidateCameraFovDegrees(float fov) {
	if (!std::isfinite(fov) || !FloatUsableScalar(fov))
		return "fov_invalid_value";
	if (!(fov > kCameraFovDegreesMin && fov < kCameraFovDegreesMax))
		return "fov_out_of_range";
	return "ok";
}

// FRotator axes are float degrees: x=Pitch, y=Yaw, z=Roll. Unreal may store 359 for -1.
inline float NormalizeRotatorAxisDegrees(float angle) {
	float a = std::fmod(angle, 360.f);
	if (a > 180.f)
		a -= 360.f;
	else if (a <= -180.f)
		a += 360.f;
	return a;
}

inline bool RotatorAxesInRange(const Vector3& rotation) {
	return Absf(NormalizeRotatorAxisDegrees(rotation.x)) <= 90.5f && Absf(rotation.y) <= 360.5f &&
	       Absf(NormalizeRotatorAxisDegrees(rotation.z)) <= 180.5f;
}

inline bool CameraFovValidationSelfCheck() {
	return std::strcmp(ValidateCameraFovDegrees(52.f), "ok") == 0 &&
	       std::strcmp(ValidateCameraFovDegrees(1e-30f), "fov_invalid_value") == 0 &&
	       std::strcmp(ValidateCameraFovDegrees(0.f), "fov_out_of_range") == 0 &&
	       std::strcmp(ValidateCameraFovDegrees(180.f), "fov_out_of_range") == 0;
}

inline const char* DiagnoseCameraPovRejectReason(const Vector3& location, const Vector3& rotation,
                                                 float fov, const CameraRef* spatialRef,
                                                 float viewportW, float viewportH) {
	if (location.x == 0.f && location.y == 0.f && location.z == 0.f && rotation.x == 0.f &&
	    rotation.y == 0.f && rotation.z == 0.f && fov == 0.f)
		return "pov_all_zero";
	if (!FloatUsableScalar(location.x) || !FloatUsableScalar(location.y) ||
	    !FloatUsableScalar(location.z))
		return "location_denormal";
	if (!FiniteVec3(location))
		return "location_non_finite";
	if (!FiniteVec3(rotation))
		return "rotation_non_finite";
	const char* fovVerdict = ValidateCameraFovDegrees(fov);
	if (std::strcmp(fovVerdict, "ok") != 0)
		return fovVerdict;
	const float span = CameraLocationMaxAbs(location);
	if (span <= 100.f)
		return "location_near_origin";
	if (span > 2.0e6f)
		return "location_out_of_volume";
	if (!RotatorHasAim(rotation))
		return "rotation_no_aim";
	if (!RotatorAxesInRange(rotation))
		return "rotation_out_of_range";
	if (spatialRef && spatialRef->valid) {
		const float anchorSpan = CameraLocationMaxAbs(spatialRef->pawn);
		if (anchorSpan > 1000.f && span < 1000.f)
			return "location_not_coherent_with_pawn";
		// Leaving the lobby: the anchor is still the mannequin (y ~125913) while
		// CameraCachePrivate already has the island POV (fov ~65, not the 90 template).
		const bool lobbyAnchor = spatialRef->pawn.y > 80000.f;
		const bool liveIslandPov = location.y < 80000.f && fov >= 40.f && fov <= 120.f &&
		                           !(fov >= 89.5f && fov <= 90.5f) && RotatorHasAim(rotation) &&
		                           span > 1000.f;
		float maxDistFromPawn = kCameraNearPawnMaxDistance;
		if (spatialRef->pawn.y > 80000.f && location.y > 80000.f &&
		    !IsCarrierSkyRigPov(*spatialRef, location))
			maxDistFromPawn = kGameplayEyeMaxDistFromPawn;
		if (!(lobbyAnchor && liveIslandPov) &&
		    Dist3(location, spatialRef->pawn) > maxDistFromPawn)
			return "location_too_far_from_pawn";
	}
	const float vw = viewportW >= 1.f ? viewportW : 1920.f;
	const float vh = viewportH >= 1.f ? viewportH : 1080.f;
	D3DMATRIX view{}, proj{}, vp{};
	if (!BuildCameraViewProjection(location, rotation, fov, 0.f, vw, vh, &view, &proj, &vp))
		return "view_projection_build_failed";
	if (!Matrix4Finite(view))
		return "view_matrix_non_finite";
	if (!Matrix4Finite(proj))
		return "projection_matrix_non_finite";
	if (!Matrix4Finite(vp))
		return "view_projection_matrix_non_finite";
	return nullptr;
}

inline bool CameraPovStrictAccept(const Vector3& location, const Vector3& rotation, float fov,
                                  const CameraRef* spatialRef = nullptr) {
	if (std::strcmp(ValidateCameraFovDegrees(fov), "ok") != 0)
		return false;
	if (!FloatUsableScalar(location.x) || !FloatUsableScalar(location.y) ||
	    !FloatUsableScalar(location.z))
		return false;
	if (!FiniteVec3(location) || !FiniteVec3(rotation))
		return false;
	const float span = CameraLocationMaxAbs(location);
	if (span <= 100.f || span > 2.0e6f)
		return false;
	if (!RotatorHasAim(rotation))
		return false;
	if (!RotatorAxesInRange(rotation))
		return false;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	return DiagnoseCameraPovRejectReason(location, rotation, fov, spatialRef, sw, sh) == nullptr;
}

inline void LogCameraRejectedOnce(const char* reason) {
	if (!reason || !reason[0])
		return;
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static std::atomic<const char*> lastLogged{ nullptr };
	static std::atomic<unsigned long long> lastLogMs{ 0 };
	const unsigned long long now = GetTickCount64();
	const char* prev = lastLogged.load(std::memory_order_relaxed);
	if (prev && std::strcmp(prev, reason) == 0 &&
	    now < lastLogMs.load(std::memory_order_relaxed) + 5000)
		return;
	lastLogged.store(reason, std::memory_order_relaxed);
	lastLogMs.store(now, std::memory_order_relaxed);
	std::cout << xorstr_("camera rejected: ") << reason << '\n';
}

inline void LogCameraRejectDetailOnce(const char* source, const Vector3& location,
                                      const Vector3& rotation, float fov, float pawnDistance,
                                      float viewW, float viewH, const char* reason) {
	if (!reason || !reason[0])
		return;
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static std::atomic<const char*> lastReason{ nullptr };
	static std::atomic<unsigned long long> lastLogMs{ 0 };
	const unsigned long long now = GetTickCount64();
	const char* prev = lastReason.load(std::memory_order_relaxed);
	if (prev && std::strcmp(prev, reason) == 0 &&
	    now < lastLogMs.load(std::memory_order_relaxed) + 5000)
		return;
	lastReason.store(reason, std::memory_order_relaxed);
	lastLogMs.store(now, std::memory_order_relaxed);
	std::cout << xorstr_("camera rejected: reason=") << reason << xorstr_(" source=")
	          << (source ? source : "?") << xorstr_(" loc ") << location.x << ',' << location.y
	          << ',' << location.z << xorstr_(" rot ") << rotation.x << ',' << rotation.y << ','
	          << rotation.z << xorstr_(" fov ") << fov << xorstr_(" pawn_dist ") << pawnDistance
	          << xorstr_(" viewport ") << viewW << 'x' << viewH << '\n';
}

struct CameraFrameSnapshot {
	uint64_t frameSerial = 0;
	uint64_t capturedTickMs = 0;
	bool valid = false;
	bool projectionValid = false;
	Vector3 location{};
	Vector3 rotation{};
	float fov = 0.f;
	float aspectRatio = 0.f;
	const char* source = "none";
	const char* rejectionReason = nullptr;
	uint32_t stabilityFrames = 0;
	D3DMATRIX viewMatrix{};
	D3DMATRIX projectionMatrix{};
	D3DMATRIX viewProjectionMatrix{};
	float viewRectMinX = 0.f;
	float viewRectMinY = 0.f;
	float viewRectWidth = 0.f;
	float viewRectHeight = 0.f;
};

using CameraSnapshot = CameraFrameSnapshot;

inline const char* CanonicalCameraRejectReason(const char* reason) {
	if (!reason)
		return "structure_mismatch";
	if (std::strcmp(reason, "location_non_finite") == 0 ||
	    std::strcmp(reason, "location_denormal") == 0 ||
	    std::strcmp(reason, "location_near_origin") == 0 ||
	    std::strcmp(reason, "location_out_of_volume") == 0 ||
	    std::strcmp(reason, "location_not_coherent_with_pawn") == 0 ||
	    std::strcmp(reason, "location_too_far_from_pawn") == 0)
		return "invalid_location";
	if (std::strcmp(reason, "fov_invalid_value") == 0 ||
	    std::strcmp(reason, "fov_out_of_range") == 0 ||
	    std::strcmp(reason, "fov_unit_mismatch") == 0)
		return reason;
	if (std::strcmp(reason, "viewport_size") == 0 || std::strcmp(reason, "display_size") == 0)
		return "viewport_invalid";
	if (std::strcmp(reason, "no_camera_manager") == 0 || std::strcmp(reason, "invalid_pointer") == 0)
		return "invalid_pointer";
	if (std::strcmp(reason, "stale_snapshot") == 0)
		return "stale_snapshot";
	if (std::strcmp(reason, "no_pcm_pov_candidate") == 0 ||
	    std::strcmp(reason, "structure_mismatch") == 0 ||
	    std::strcmp(reason, "pov_scoring_rejected") == 0)
		return "structure_mismatch";
	return reason;
}

inline bool IsDefaultTemplatePov(const Vector3& location, const Vector3& rotation, float fov) {
	if (!(fov >= 89.5f && fov <= 90.5f))
		return false;
	if (RotatorHasAim(rotation))
		return false;
	return CameraLocationMaxAbs(location) < 1.f;
}

inline const char* DiagnoseCameraSnapshotRejectReason(const CameraSnapshot& snap) {
	if (!snap.valid)
		return snap.rejectionReason ? snap.rejectionReason : "stale_snapshot";
	if (!FiniteVec3(snap.location))
		return "invalid_location";
	if (!FiniteVec3(snap.rotation))
		return "invalid_rotation";
	const char* fovVerdict = ValidateCameraFovDegrees(snap.fov);
	if (std::strcmp(fovVerdict, "ok") != 0)
		return fovVerdict;
	if (snap.aspectRatio > 0.f && (snap.aspectRatio < 0.5f || snap.aspectRatio > 3.f))
		return "viewport_invalid";
	if (snap.viewRectWidth < 1.f || snap.viewRectHeight < 1.f)
		return "viewport_invalid";
	const CameraRef spatialRef = ReadLocalCameraRef();
	const CameraRef* spatialPtr = spatialRef.valid ? &spatialRef : nullptr;
	const char* povReason = DiagnoseCameraPovRejectReason(
	    snap.location, snap.rotation, snap.fov, spatialPtr, snap.viewRectWidth, snap.viewRectHeight);
	if (povReason)
		return povReason;
	if (!snap.projectionValid)
		return "view_projection_not_built";
	if (!Matrix4Finite(snap.viewMatrix))
		return "view_matrix_non_finite";
	if (!Matrix4Finite(snap.projectionMatrix))
		return "projection_matrix_non_finite";
	if (!Matrix4Finite(snap.viewProjectionMatrix))
		return "view_projection_matrix_non_finite";
	return nullptr;
}

namespace RenderPipeline {
struct FrameStats {
	std::atomic<uint32_t> worldPoints{ 0 };
	std::atomic<uint32_t> projectedPoints{ 0 };
	std::atomic<uint32_t> visiblePoints{ 0 };
	std::atomic<uint32_t> queuedPrimitives{ 0 };
	std::atomic<uint32_t> submittedPrimitives{ 0 };
};
inline FrameStats g_FrameStats{};

inline char g_CameraAcquireRejectReason[48] = "structure_mismatch";

inline void SetCameraAcquireRejectReason(const char* reason) {
	const char* r = reason && reason[0] ? reason : "structure_mismatch";
	std::strncpy(g_CameraAcquireRejectReason, r, sizeof(g_CameraAcquireRejectReason) - 1);
	g_CameraAcquireRejectReason[sizeof(g_CameraAcquireRejectReason) - 1] = '\0';
}

// camera_read -> candidate_selected -> candidate_validated -> snapshot_committed -> CAMERA_READY.
// `stage` is the first stage that failed this frame, or CAMERA_READY.
struct CameraLifecycle {
	const char* stage = "camera_read";
	char source[48] = "none";
	char reason[64] = "not_started";
};
inline CameraLifecycle g_CameraLifecycle{};

inline void SetCameraLifecycle(const char* stage, const char* source, const char* reason) {
	g_CameraLifecycle.stage = stage ? stage : "camera_read";
	std::snprintf(g_CameraLifecycle.source, sizeof(g_CameraLifecycle.source), "%s",
	              source && source[0] ? source : "none");
	std::snprintf(g_CameraLifecycle.reason, sizeof(g_CameraLifecycle.reason), "%s",
	              reason && reason[0] ? reason : "unknown");
}

inline void LogCameraLifecycleIfChanged() {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static char lastKey[176]{};
	static unsigned long long lastMs = 0;
	const CameraLifecycle& lc = g_CameraLifecycle;
	char key[176];
	std::snprintf(key, sizeof(key), "%s|%s|%s", lc.stage, lc.source, lc.reason);
	if (std::strcmp(key, lastKey) == 0)
		return;
	const unsigned long long now = GetTickCount64();
	if (lastMs && now - lastMs < 2000)
		return;
	std::strncpy(lastKey, key, sizeof(lastKey) - 1);
	lastMs = now;
	static const char* const kStages[] = { "camera_read", "candidate_selected", "candidate_validated",
	                                       "snapshot_committed", "CAMERA_READY" };
	const bool ready = std::strcmp(lc.stage, "CAMERA_READY") == 0;
	bool reached = true;
	std::cout << xorstr_("[camera] lifecycle");
	for (const char* s : kStages) {
		std::cout << ' ' << s << '=';
		if (!ready && std::strcmp(s, lc.stage) == 0) {
			std::cout << xorstr_("FAIL(") << lc.reason << ')';
			reached = false;
		} else {
			std::cout << (reached ? xorstr_("ok") : xorstr_("-"));
		}
	}
	std::cout << xorstr_(" source=") << lc.source << '\n';
}

inline void ResetFrameStats() {
	g_FrameStats.worldPoints.store(0, std::memory_order_relaxed);
	g_FrameStats.projectedPoints.store(0, std::memory_order_relaxed);
	g_FrameStats.visiblePoints.store(0, std::memory_order_relaxed);
	g_FrameStats.queuedPrimitives.store(0, std::memory_order_relaxed);
	g_FrameStats.submittedPrimitives.store(0, std::memory_order_relaxed);
}

inline void LogFrameStatsIfDue(uint64_t frameSerial) {
	(void)frameSerial;
}

inline void LogRenderPipelineSummary(bool cameraProjectionReady, bool cameraAcquired,
                                     const char* cameraReject, const CameraFrameSnapshot& snap) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static bool prevCamProj = false;
	static bool prevCamAcq = false;
	static bool prevBones = false;
	static const char* prevCamReject = nullptr;
	static const char* prevBoneReject = nullptr;
	static const char* prevProjBlock = nullptr;
	static const char* prevCamSource = nullptr;
	static BonePipelineVerdict prevBoneVerdict = BonePipelineVerdict::INVALID_UNINITIALIZED;
	static uint32_t prevWorld = 0;
	static uint32_t prevProjected = 0;
	static unsigned long long nextForceMs = 0;
	const unsigned long long now = GetTickCount64();
	const auto boneVerdict = static_cast<BonePipelineVerdict>(
	    g_LocalBoneVerdict.load(std::memory_order_relaxed));
	const bool bonesValid = g_LocalBonesValid.load(std::memory_order_relaxed);
	const char* boneReject = g_LocalBonesReject.load(std::memory_order_relaxed);
	const char* projBlock =
	    cameraProjectionReady ? "ok" : (snap.rejectionReason ? snap.rejectionReason : "camera_not_ready");
	const uint32_t world = g_FrameStats.worldPoints.load(std::memory_order_relaxed);
	const uint32_t projected = g_FrameStats.projectedPoints.load(std::memory_order_relaxed);
	const bool statsJump = Absf(static_cast<int>(world) - static_cast<int>(prevWorld)) > 8 ||
	                       Absf(static_cast<int>(projected) - static_cast<int>(prevProjected)) > 8;
	static char prevLifecycle[176]{};
	char lifecycle[176];
	std::snprintf(lifecycle, sizeof(lifecycle), "%s|%s|%s", g_CameraLifecycle.stage,
	              g_CameraLifecycle.source, g_CameraLifecycle.reason);
	const bool boneVerdictChanged = !BoneVerdictSameForLog(boneVerdict, prevBoneVerdict);
	const bool camSourceChanged =
	    (snap.source != prevCamSource) &&
	    (snap.source || prevCamSource) &&
	    (!snap.source || !prevCamSource ||
	     std::strcmp(snap.source ? snap.source : "", prevCamSource ? prevCamSource : "") != 0);
	const bool changed = cameraProjectionReady != prevCamProj || cameraAcquired != prevCamAcq ||
	                     bonesValid != prevBones || cameraReject != prevCamReject ||
	                     boneReject != prevBoneReject || projBlock != prevProjBlock ||
	                     camSourceChanged || boneVerdictChanged || statsJump ||
	                     std::strcmp(lifecycle, prevLifecycle) != 0;
	if (!changed && now < nextForceMs)
		return;
	nextForceMs = now + (cameraProjectionReady && bonesValid ? 8000ull : 2500ull);
	std::strncpy(prevLifecycle, lifecycle, sizeof(prevLifecycle) - 1);
	prevCamProj = cameraProjectionReady;
	prevCamAcq = cameraAcquired;
	prevBones = bonesValid;
	prevCamReject = cameraReject;
	prevBoneReject = boneReject;
	prevProjBlock = projBlock;
	prevCamSource = snap.source;
	prevBoneVerdict = boneVerdict;
	prevWorld = world;
	prevProjected = projected;
	std::ostringstream line;
	line << xorstr_("[pipe] cam=") << (cameraProjectionReady ? xorstr_("ok") : xorstr_("bad"));
	if (cameraProjectionReady)
		line << xorstr_(" src=") << (snap.source ? snap.source : "?") << xorstr_(" fov=") << snap.fov;
	else
		line << xorstr_(" stage=") << g_CameraLifecycle.stage << xorstr_(" reason=")
		     << g_CameraLifecycle.reason;
	line << xorstr_(" bones=") << BonePipelineVerdictString(boneVerdict);
	if (boneReject && boneReject[0])
		line << xorstr_("(") << boneReject << ')';
	line << xorstr_(" w2s=") << projected << '/' << world << xorstr_(" esp=")
	     << g_FrameStats.submittedPrimitives.load();
	ConsoleDiagLine(line.str());
}
} // namespace RenderPipeline

inline CameraFrameSnapshot g_W2SCameraSnapshot{};
inline std::mutex g_W2SCameraSnapshotMutex;

inline bool GetRenderViewportRect(float& minX, float& minY, float& width, float& height) {
	// ImDrawList coords are viewport-local; Pos is screen offset and must not be added to W2S.
	minX = 0.f;
	minY = 0.f;
	width = 0.f;
	height = 0.f;
	if (ImGui::GetCurrentContext()) {
		const ImGuiIO& io = ImGui::GetIO();
		if (ImGuiViewport* vp = ImGui::GetMainViewport()) {
			width = vp->Size.x;
			height = vp->Size.y;
		} else {
			width = io.DisplaySize.x;
			height = io.DisplaySize.y;
		}
		if (width >= 1.f && height >= 1.f)
			return true;
	}
	GetOverlayViewportSize(width, height);
	return width >= 1.f && height >= 1.f;
}

inline bool MatricesRoughlyEqual(const D3DMATRIX& a, const D3DMATRIX& b, float eps = 1e-3f) {
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			if (Absf(a.m[r][c] - b.m[r][c]) > eps)
				return false;
		}
	}
	return true;
}

inline void LogW2SCameraFrameDelta(const CameraFrameSnapshot& prev, const CameraFrameSnapshot& cur) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace) || !cur.projectionValid)
		return;
	if (prev.frameSerial == 0)
		return;
	const bool sourceSwitch = prev.projectionValid && prev.source && cur.source &&
	                          std::strcmp(prev.source, cur.source) != 0;
	const bool vpChanged =
	    prev.projectionValid &&
	    !MatricesRoughlyEqual(prev.viewProjectionMatrix, cur.viewProjectionMatrix, 0.05f);
	const bool viewportChanged =
	    prev.projectionValid &&
	    (Absf(prev.viewRectWidth - cur.viewRectWidth) > 0.5f ||
	     Absf(prev.viewRectHeight - cur.viewRectHeight) > 0.5f);
	if (!sourceSwitch && !vpChanged && !viewportChanged)
		return;
	std::ostringstream line;
	line << "W2S trace f" << cur.frameSerial << " source " << (cur.source ? cur.source : "?");
	if (sourceSwitch)
		line << " SOURCE_SWITCH " << prev.source << " -> " << cur.source;
	if (vpChanged)
		line << " VP_CHANGED";
	if (viewportChanged)
		line << " VIEWPORT_CHANGED";
	std::cout << line.str() << '\n';
}

inline void LogCameraRuntimeEvents(bool nowValid, const char* source, const Vector3& location,
                                   const Vector3& rotation, float fov,
                                   const char* rejectReason = nullptr) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace))
		return;
	static bool prevValid = false;
	static char prevSource[40]{};
	static const char* prevReject = nullptr;
	static unsigned long long nextLogMs = 0;
	const unsigned long long now = GetTickCount64();
	const bool rejectChanged = rejectReason != prevReject && (rejectReason || prevReject);
	if (!nowValid && nowValid == prevValid && !rejectChanged && now < nextLogMs)
		return;
	if (nowValid != prevValid || rejectChanged || now >= nextLogMs) {
		std::cout << xorstr_("[camera] ") << (nowValid ? xorstr_("VALID") : xorstr_("INVALID"));
		if (!nowValid && rejectReason)
			std::cout << xorstr_(" reason=") << rejectReason;
		if (nowValid && source)
			std::cout << xorstr_(" source ") << source << xorstr_(" loc ") << location.x << ','
			          << location.y << ',' << location.z << xorstr_(" rot ") << rotation.x << ','
			          << rotation.y << ',' << rotation.z << xorstr_(" fov ") << fov;
		std::cout << '\n';
		prevValid = nowValid;
		prevReject = rejectReason;
		nextLogMs = now + 5000;
	}
	if (nowValid && source && source[0]) {
		if (std::strcmp(prevSource, source) != 0) {
			if (prevSource[0])
				std::cout << xorstr_("[camera] source ") << prevSource << xorstr_(" -> ")
				          << source << '\n';
			std::strncpy(prevSource, source, sizeof(prevSource) - 1);
			prevSource[sizeof(prevSource) - 1] = '\0';
		}
	} else if (!nowValid) {
		prevSource[0] = '\0';
	}
}

inline float AbsYawDelta(float a, float b);
inline bool RetracHelicarrierPcmPovLive(uintptr_t pcm);

inline void LogW2SProbePoint(const char* label, const Vector3& world, const CameraFrameSnapshot& snap);
inline void LogW2SDiagnosticProbes(const CameraFrameSnapshot& snap);

// One immutable camera + viewport snapshot per overlay frame. All W2S calls use this only.
inline void CommitCameraFrameForW2S() {
	const bool cameraAcquiredThisFrame = Camera::Valid;
	CameraFrameSnapshot next{};
	next.capturedTickMs = GetTickCount64();
	next.valid = Camera::Valid;
	next.location = Camera::Location;
	next.rotation = Camera::Rotation;
	next.fov = Camera::FOV;
	next.aspectRatio = Camera::AspectRatio;
	next.source = Camera::Source;

	float minX = 0.f;
	float minY = 0.f;
	float sw = 0.f;
	float sh = 0.f;
	if (!GetRenderViewportRect(minX, minY, sw, sh)) {
		{
			std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
			g_W2SCameraSnapshot = next;
		}
		Camera::ViewProjectionReady = false;
		if (cameraAcquiredThisFrame)
			RenderPipeline::SetCameraLifecycle("snapshot_committed", next.source, "viewport_invalid");
		RenderPipeline::LogCameraLifecycleIfChanged();
		return;
	}
	next.viewRectMinX = minX;
	next.viewRectMinY = minY;
	next.viewRectWidth = sw;
	next.viewRectHeight = sh;

	const CameraRef spatialRef = ReadLocalCameraRef();
	const CameraRef* spatialPtr = spatialRef.valid ? &spatialRef : nullptr;
	const char* rejectReason = nullptr;
	static CameraFrameSnapshot s_stabilityPrev{};
	static int s_stabilityCount = 0;
	if (next.valid) {
		rejectReason =
		    DiagnoseCameraPovRejectReason(next.location, next.rotation, next.fov, spatialPtr, sw, sh);
		bool matricesOk = false;
		if (!rejectReason && next.fov >= 1.f &&
		    BuildCameraViewProjection(next.location, next.rotation, next.fov, next.aspectRatio, sw,
		                              sh, &next.viewMatrix, &next.projectionMatrix,
		                              &next.viewProjectionMatrix) &&
		    Matrix4Finite(next.viewMatrix) && Matrix4Finite(next.projectionMatrix) &&
		    Matrix4Finite(next.viewProjectionMatrix))
			matricesOk = true;
		else if (!rejectReason)
			rejectReason = "view_projection_build_failed";
		if (matricesOk) {
			s_stabilityCount = s_stabilityCount < 1000000 ? s_stabilityCount + 1 : s_stabilityCount;
			next.stabilityFrames = static_cast<uint32_t>(s_stabilityCount);
			s_stabilityPrev = next;
			s_stabilityPrev.valid = true;
			next.projectionValid = true;
		} else {
			s_stabilityCount = 0;
			s_stabilityPrev = {};
		}
	} else {
		s_stabilityCount = 0;
		s_stabilityPrev = {};
		if (!rejectReason)
			rejectReason = RenderPipeline::g_CameraAcquireRejectReason;
	}
	if (!next.projectionValid) {
		next.valid = false;
		next.rejectionReason =
		    CanonicalCameraRejectReason(rejectReason ? rejectReason
		                                             : RenderPipeline::g_CameraAcquireRejectReason);
		// Acquire failure is summarized once; only log commit-stage rejects (stale_snapshot, etc.).
		if (cameraAcquiredThisFrame)
			LogCameraRejectDetailOnce(next.source, next.location, next.rotation, next.fov,
			                          spatialRef.valid ? Dist3(next.location, spatialRef.pawn) : 0.f,
			                          sw, sh, next.rejectionReason);
	} else {
		next.rejectionReason = nullptr;
	}

	CameraFrameSnapshot prev{};
	{
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		prev = g_W2SCameraSnapshot;
		next.frameSerial = prev.frameSerial + 1;
		g_W2SCameraSnapshot = next;
	}
	LogW2SCameraFrameDelta(prev, next);
	LogCameraRuntimeEvents(next.projectionValid, next.source, next.location, next.rotation, next.fov,
	                       next.projectionValid ? nullptr : next.rejectionReason);
	if (cameraAcquiredThisFrame)
		RenderPipeline::SetCameraLifecycle(next.projectionValid ? "CAMERA_READY" : "snapshot_committed",
		                                   next.source,
		                                   next.projectionValid ? "ok"
		                                                        : (rejectReason ? rejectReason : "rejected"));
	RenderPipeline::LogCameraLifecycleIfChanged();

	Camera::ViewMatrix = next.viewMatrix;
	Camera::ProjectionMatrix = next.projectionMatrix;
	Camera::ViewProjectionMatrix = next.viewProjectionMatrix;
	Camera::ViewRectMinX = next.viewRectMinX;
	Camera::ViewRectMinY = next.viewRectMinY;
	Camera::ViewRectWidth = next.viewRectWidth;
	Camera::ViewRectHeight = next.viewRectHeight;
	Camera::ViewProjectionReady = next.projectionValid;
	if (next.projectionValid) {
		const D3DMATRIX tempMatrix = Matrix(next.rotation);
		Camera::vAxisX = Vector3(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
		Camera::vAxisY = Vector3(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
		Camera::vAxisZ = Vector3(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);
		const float centerX = sw * 0.5f;
		const float tanHalf = tanf(next.fov * (float)M_PI / 360.f);
		Camera::FovCoef = tanHalf >= 1e-6f ? centerX / tanHalf : 0.f;
		LogW2SDiagnosticProbes(next);
	}
}

inline void LogW2SProbePoint(const char* label, const Vector3& world, const CameraFrameSnapshot& snap) {
	if (!label || !snap.projectionValid)
		return;
	float clip[4]{};
	UnrealTransformFVector4(snap.viewProjectionMatrix, world.x, world.y, world.z, 1.f, clip);
	const float w = clip[3];
	const float ndcX = w > 1e-4f ? clip[0] / w : 0.f;
	const float ndcY = w > 1e-4f ? clip[1] / w : 0.f;
	const float ndcZ = w > 1e-4f ? clip[2] / w : 0.f;
	Vector3 screen{};
	const bool projected = ProjectWorldWithViewProjection(
	    snap.viewProjectionMatrix, snap.viewRectMinX, snap.viewRectMinY, snap.viewRectWidth,
	    snap.viewRectHeight, world, &screen);
	const float dist = Vec3Distance(snap.location, world);
	std::ostringstream line;
	line << "W2S probe [" << label << "] f" << snap.frameSerial << " world " << world.x << ','
	     << world.y << ',' << world.z << " cam " << snap.location.x << ',' << snap.location.y
	     << ',' << snap.location.z << " dist " << dist << " clip " << clip[0] << ',' << clip[1]
	     << ',' << clip[2] << " w " << w << " ndc " << ndcX << ',' << ndcY << ',' << ndcZ
	     << " screen " << (projected ? screen.x : 0.f) << ',' << (projected ? screen.y : 0.f)
	     << " ok " << (projected ? 1 : 0);
	std::cout << line.str() << '\n';
}

inline void LogW2SDiagnosticProbes(const CameraFrameSnapshot& snap) {
	if (!snap.projectionValid)
		return;
	const bool trace = Settings::DebugAtLeast(Settings::DebugVerbosity::Trace);
	if (!Settings::W2SDebugDraw && !trace)
		return;
	if (trace && (snap.frameSerial % 120ull) != 0ull)
		return;
	static std::atomic<unsigned long long> nextLogMs{ 0 };
	const unsigned long long now = GetTickCount64();
	if (!trace && now < nextLogMs.load(std::memory_order_relaxed))
		return;
	if (!trace)
		nextLogMs.store(now + 2000, std::memory_order_relaxed);

	static Vector3 fixedWorld{};
	static bool fixedSet = false;
	if (!fixedSet) {
		const D3DMATRIX axes = Matrix(snap.rotation);
		const Vector3 forward(axes.m[0][0], axes.m[0][1], axes.m[0][2]);
		fixedWorld = Vector3(snap.location.x + forward.x * 800.f, snap.location.y + forward.y * 800.f,
		                     snap.location.z + forward.z * 800.f);
		fixedSet = true;
	}
	LogW2SProbePoint("fixed_world", fixedWorld, snap);

	const uintptr_t pawn = LocalPtrs::Player;
	if (!pawn || !Memory::IsValid(pawn))
		return;
	const uintptr_t mesh = Read<uintptr_t>(pawn + Offsets::Mesh);
	if (mesh && Memory::IsValid(mesh)) {
		const Vector3 meshOrigin = GetMeshWorldLocation(mesh);
		LogW2SProbePoint("mesh_origin", meshOrigin, snap);
		const Vector3 head = GetBoneWithRotation(mesh, EBoneIndex::Head);
		LogW2SProbePoint("bone_head", head, snap);
		if (trace && FiniteVec3(head) && !BoneWorldMissing(head)) {
			ResolvedBoneArray pose{};
			const uintptr_t poseMesh = ResolveSkeletonMesh(mesh);
			if (ResolveBonePoseArray(poseMesh, pose) && EBoneIndex::Head < pose.Num) {
				FTransform boneCs{};
				Memory::Process.ReadRequestOk(
				    pose.Data + static_cast<uintptr_t>(EBoneIndex::Head) * 0x30, boneCs);
				std::cout << "W2S bone_space meshOff 0x" << std::hex << pose.MeshOffset << std::dec
				          << " mode " << (pose.UsesBoneSpace ? "bone_chain" : "component")
				          << " csT " << boneCs.translation.x << ',' << boneCs.translation.y << ','
				          << boneCs.translation.z << " c2w " << meshOrigin.x << ',' << meshOrigin.y
				          << ',' << meshOrigin.z << " dist_mesh_head "
				          << Vec3Distance(meshOrigin, head) << '\n';
			}
		}
	}
	const Vector3 rootLoc = GetActorRootWorldLocation(pawn);
	if (FiniteVec3(rootLoc) && !BoneWorldMissing(rootLoc))
		LogW2SProbePoint("root", rootLoc, snap);
}

inline void RebuildCameraViewProjection() {
	CommitCameraFrameForW2S();
}

inline void LogW2SProjectionDebug(const Vector3& world, const D3DMATRIX& view, const D3DMATRIX& proj,
                                    const D3DMATRIX& viewProjection, const Vector3& screen) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace))
		return;
	static std::atomic<unsigned long long> nextLogMs{ 0 };
	const unsigned long long now = GetTickCount64();
	if (now < nextLogMs.load(std::memory_order_relaxed))
		return;
	nextLogMs.store(now + 500, std::memory_order_relaxed);
	float clip[4];
	UnrealTransformFVector4(viewProjection, world.x, world.y, world.z, 1.f, clip);
	const float ndcX = clip[3] > 0.f ? clip[0] / clip[3] : 0.f;
	const float ndcY = clip[3] > 0.f ? clip[1] / clip[3] : 0.f;
	auto row = [](const D3DMATRIX& m, int r) {
		std::ostringstream o;
		o << m.m[r][0] << ',' << m.m[r][1] << ',' << m.m[r][2] << ',' << m.m[r][3];
		return o.str();
	};
	std::ostringstream line;
	line << "W2S view[" << row(view, 0) << " | " << row(view, 1) << " | " << row(view, 2) << " | "
	     << row(view, 3) << "] proj[" << row(proj, 0) << " | " << row(proj, 1) << " | "
	     << row(proj, 2) << " | " << row(proj, 3) << "] clip " << clip[0] << ',' << clip[1] << ','
	     << clip[2] << " w " << clip[3] << " ndc " << ndcX << ',' << ndcY << " screen " << screen.x
	     << ',' << screen.y << " viewRect " << Camera::ViewRectMinX << ',' << Camera::ViewRectMinY
	     << ' ' << Camera::ViewRectWidth << 'x' << Camera::ViewRectHeight;
	std::cout << line.str() << '\n';
}

// ViewProjection = View * Projection; perspective divide uses clip.W (view-space depth).
inline bool ProjectPointWithView(const Vector3& camLoc, const Vector3& camRot, float fov,
                                 float aspectRatio, const Vector3& world, float sw, float sh,
                                 Vector3* outScreen) {
	if (!outScreen || fov < 1.f || sw < 1.f || sh < 1.f)
		return false;
	D3DMATRIX view{};
	D3DMATRIX proj{};
	D3DMATRIX vp{};
	if (!BuildCameraViewProjection(camLoc, camRot, fov, aspectRatio, sw, sh, &view, &proj, &vp))
		return false;
	if (!ProjectWorldWithViewProjection(vp, 0.f, 0.f, sw, sh, world, outScreen))
		return false;
	LogW2SProjectionDebug(world, view, proj, vp, *outScreen);
	return true;
}

inline float WorldPointScreenCenterError(const Vector3& camLoc, const Vector3& camRot, float fov,
                                         float aspectRatio, const Vector3& world, float sw,
                                         float sh) {
	Vector3 screen{};
	if (!ProjectPointWithView(camLoc, camRot, fov, aspectRatio, world, sw, sh, &screen))
		return 1e9f;
	const float dx = screen.x - sw * 0.5f;
	const float dy = screen.y - sh * 0.5f;
	return sqrtf(dx * dx + dy * dy);
}

inline float ReticleScreenCenterErrorThreshold(float viewportWidth) {
	return viewportWidth * 0.075f;
}

// After bus / PCM swap, LocationUnderReticle can stay at spawn for many seconds.
inline constexpr float kMaxReticleDistanceFromHead = 12000.f;

inline bool ResolveReticleDistanceReference(uintptr_t playerController, Vector3& outHead,
                                            Vector3& outPawn) {
	outHead = Vector3{};
	outPawn = Vector3{};
	const CameraRef anchor = ReadCameraSpatialAnchor();
	if (anchor.valid) {
		outHead = anchor.head;
		outPawn = anchor.pawn;
		return true;
	}
	Vector3 pivot{}, head{};
	if (ResolveCameraPivot(pivot, head)) {
		outPawn = pivot;
		outHead = head;
		return true;
	}
	if (!playerController || !Memory::IsValid(playerController))
		return false;
	const uintptr_t pawn =
	    Read<uintptr_t>(playerController + Offsets::AcknowledgedPawn);
	if (!pawn || !Memory::IsValid(pawn))
		return false;
	const uintptr_t root = Read<uintptr_t>(pawn + Offsets::RootComponent);
	if (root && Memory::IsValid(root)) {
		const MeshTransformSnapshot rootSnap = DiagnoseComponentToWorld(root);
		if (rootSnap.valid && CameraLocationMaxAbs(rootSnap.translation) > 100.f) {
			outPawn = rootSnap.translation;
			outHead = rootSnap.translation;
			return true;
		}
	}
	return false;
}

inline bool ReadPlayerReticleWorld(uintptr_t playerController, Vector3& out) {
	out = Vector3{};
	if (!playerController || !Memory::IsValid(playerController))
		return false;
	const Vector3 reticle =
	    Read<Vector3>(playerController + Offsets::LocationUnderReticle);
	if (!FiniteVec3(reticle) || BoneWorldMissing(reticle))
		return false;
	if (CameraLocationMaxAbs(reticle) <= 1000.f)
		return false;
	Vector3 head{}, pawn{};
	if (!ResolveReticleDistanceReference(playerController, head, pawn))
		return false;
	if (Dist3(reticle, head) > kMaxReticleDistanceFromHead)
		return false;
	if (Dist3(reticle, pawn) > kMaxReticleDistanceFromHead * 1.25f)
		return false;
	out = reticle;
	return true;
}

// Third-person view rotators often differ from ControlRotation; reticle center error is the gate.
inline bool PovPassesReticleGate(const Vector3& location, const Vector3& rotation, float fov,
                                 float aspectRatio, uintptr_t playerController,
                                 const CameraRef* spatialRef) {
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	if (DiagnoseCameraPovRejectReason(location, rotation, fov, spatialRef, sw, sh))
		return false;
	if (!CameraPovStrictAccept(location, rotation, fov, spatialRef))
		return false;
	if (spatialRef && spatialRef->valid) {
		const float maxDist = IsCarrierSkyRigPov(*spatialRef, location)
		                          ? kCameraNearPawnMaxDistance
		                          : kGameplayEyeMaxDistFromPawn;
		if (Dist3(location, spatialRef->pawn) > maxDist)
			return false;
	}
	Vector3 reticle{};
	if (!playerController || !Memory::IsValid(playerController) ||
	    !ReadPlayerReticleWorld(playerController, reticle)) {
		if (spatialRef && spatialRef->valid && spatialRef->pawn.y < 80000.f) {
			if (PovMatchesEngineControlRotation(playerController, rotation))
				return true;
			return false;
		}
		return true;
	}
	const float aspect =
	    (aspectRatio >= 0.5f && aspectRatio <= 3.f)
	        ? aspectRatio
	        : ((sw > 1.f && sh > 1.f) ? sw / sh : 16.f / 9.f);
	const float err =
	    WorldPointScreenCenterError(location, rotation, fov, aspect, reticle, sw, sh);
	return err <= ReticleScreenCenterErrorThreshold(sw);
}

inline float ReadPovAspectRatio(uintptr_t object, uint32_t povOffset) {
	if (!object || !Memory::IsValid(object))
		return 0.f;
	const float aspect =
	    Read<float>(object + povOffset + PcmSdkLayout::kPovAspectRatio);
	if (aspect >= 0.5f && aspect <= 3.f)
		return aspect;
	return 0.f;
}

inline float OverlayAspectRatio(uintptr_t pcm) {
	if (pcm && Memory::IsValid(pcm)) {
		const uint32_t cachePov =
		    static_cast<uint32_t>(Offsets::camera_cache_private) + PcmSdkLayout::kCacheEntryPov;
		const float fromPov = ReadPovAspectRatio(pcm, cachePov);
		if (fromPov >= 0.5f && fromPov <= 3.f)
			return fromPov;
	}
	float aspect = pcm && Memory::IsValid(pcm) ? ReadPovAspectRatio(pcm, 0) : 0.f;
	if (aspect >= 0.5f && aspect <= 3.f)
		return aspect;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	return sh > 1.f ? sw / sh : 16.f / 9.f;
}

inline bool CameraProjectsRefHeadOnScreen(const Vector3& camLoc, const Vector3& camRot, float fov,
                                        float aspectRatio, const CameraRef& ref, float sw,
                                        float sh) {
	if (!ref.valid)
		return true;
	Vector3 screen{};
	if (!ProjectPointWithView(camLoc, camRot, fov, aspectRatio, ref.head, sw, sh, &screen))
		return false;
	if (screen.x < -sw * 0.25f || screen.x > sw * 1.25f || screen.y < -sh * 0.25f ||
	    screen.y > sh * 1.25f)
		return false;
	return true;
}

inline bool PcmHeavyCameraScanDue() {
	static std::atomic<unsigned long long> nextMs{ 0 };
	const unsigned long long now = GetTickCount64();
	const unsigned long long next = nextMs.load(std::memory_order_relaxed);
	if (now < next)
		return false;
	nextMs.store(now + 220, std::memory_order_relaxed);
	return true;
}

struct LastGoodCameraPov {
	bool valid = false;
	unsigned long long tickMs = 0;
	Vector3 location{};
	Vector3 rotation{};
	float fov = 0.f;
	float aspect = 0.f;
	char source[24]{};
	uint32_t sourceOffset = 0;
};
inline LastGoodCameraPov g_LastGoodCameraPov{};
inline float g_LastLivePlayFov = 0.f;

inline bool IsTemplateFovDegrees(float fov) {
	return fov >= 89.5f && fov <= 90.5f;
}

inline float SanitizePcmFov(float fov, uintptr_t pcm) {
	if (fov >= 50.f && fov <= 120.f && !IsTemplateFovDegrees(fov))
		return fov;
	if (pcm && Memory::IsValid(pcm)) {
		const float def = Read<float>(pcm + Offsets::DefaultFOV);
		if (def >= 50.f && def <= 120.f && !IsTemplateFovDegrees(def))
			return def;
	}
	if (g_LastLivePlayFov >= 40.f && g_LastLivePlayFov <= 120.f && !IsTemplateFovDegrees(g_LastLivePlayFov))
		return g_LastLivePlayFov;
	return 80.f;
}

inline bool IsSdkLiveCameraSource(const char* source) {
	if (!source || !source[0])
		return false;
	if (std::strcmp(source, "CameraCachePrivate") == 0 ||
	    std::strcmp(source, "LastFrameCameraCachePrivate") == 0 ||
	    std::strcmp(source, "LastFrameCameraCache") == 0 || std::strcmp(source, "CameraCache") == 0 ||
	    std::strcmp(source, "ViewTarget") == 0 ||
	    std::strcmp(source, "PendingViewTarget") == 0 ||
	    std::strcmp(source, "PcmRoot") == 0 || 	    std::strcmp(source, "FollowTPS+Reticle") == 0 || std::strcmp(source, "FollowTPS") == 0 ||
	    std::strcmp(source, "FreeCamTPS+Reticle") == 0)
		return true;
	if (std::strstr(source, "+pov+0x") != nullptr || std::strstr(source, ".scan+0x") != nullptr ||
	    std::strstr(source, "Pcm.live+0x") != nullptr)
		return true;
	return false;
}

// In-match PCM caches are often template; ControlRotation + pawn pivot still drives the real view.
inline bool IsSyntheticInMatchCameraSource(const char* source) {
	return source && (std::strcmp(source, "FortniteControlView") == 0 ||
	                  std::strcmp(source, "ReticleView") == 0 ||
	                  std::strcmp(source, "CtrlRot") == 0);
}

inline bool SyntheticInMatchCameraSourceSelfCheck() {
	return IsSyntheticInMatchCameraSource("FortniteControlView") &&
	       IsSyntheticInMatchCameraSource("ReticleView") &&
	       IsSyntheticInMatchCameraSource("CtrlRot") &&
	       !IsSdkLiveCameraSource("FortniteControlView") &&
	       IsSdkLiveCameraSource("PcmRoot");
}

inline bool CameraSourceWorthyOfLastGoodHold(const char* source) {
	if (!source || !source[0])
		return false;
	if (std::strcmp(source, "PcmRoot") == 0 || std::strcmp(source, "FortniteControlView") == 0 ||
	    std::strcmp(source, "CtrlRot") == 0 || std::strcmp(source, "FreeCamTPS+Reticle") == 0 ||
	    std::strcmp(source, "FollowTPS") == 0 || std::strcmp(source, "FollowTPS+Reticle") == 0)
		return false;
	return true;
}

inline void ApplyCameraPov(const Vector3& location, const Vector3& rotation, float fov,
                           const char* source, uint32_t offset, float aspectRatio = 0.f) {
	const bool sdkLive = IsSdkLiveCameraSource(source);
	const bool synthetic = IsSyntheticInMatchCameraSource(source);
	if (!sdkLive && !synthetic)
		return;
	// Synthetic fallbacks run only after PCM failed; never clobber a live SDK POV.
	if (synthetic && Camera::Valid && Camera::Source && IsSdkLiveCameraSource(Camera::Source))
		return;
	float commitFov = fov;
	if (IsTemplateFovDegrees(commitFov))
		commitFov = SanitizePcmFov(commitFov, LocalPtrs::PlayerCam);
	Camera::Location = location;
	Camera::Rotation = rotation;
	Camera::FOV = Settings::FOVChanger ? Settings::FOVChangerValue : commitFov;
	Camera::AspectRatio = aspectRatio;
	Camera::Source = source;
	Camera::SourceOffset = offset;
	Camera::Valid = true;
	Camera::ViewProjectionReady = false;
	if (CameraSourceWorthyOfLastGoodHold(source)) {
		g_LastGoodCameraPov.valid = true;
		g_LastGoodCameraPov.tickMs = GetTickCount64();
		g_LastGoodCameraPov.location = location;
		g_LastGoodCameraPov.rotation = rotation;
		g_LastGoodCameraPov.fov = commitFov;
		g_LastGoodCameraPov.aspect = aspectRatio;
		std::strncpy(g_LastGoodCameraPov.source, source, sizeof(g_LastGoodCameraPov.source) - 1);
		g_LastGoodCameraPov.sourceOffset = offset;
	}
	if (sdkLive && location.y < 80000.f && commitFov >= 40.f && commitFov <= 120.f &&
	    !IsTemplateFovDegrees(commitFov))
		g_LastLivePlayFov = commitFov;
}

inline bool RotatorPlausible(const Vector3& rotation) {
	if (!FiniteVec3(rotation))
		return false;
	if (Absf(rotation.x) > 90.5f || Absf(rotation.y) > 360.5f || Absf(rotation.z) > 180.5f)
		return false;
	return !(rotation.x == 0.f && rotation.y == 0.f && rotation.z == 0.f);
}

// Engine reference for PCM POV comparison: documented ControlRotation only, no heuristic slots.
inline bool ControlRotationTrustworthyForCompare(const Vector3& rotation) {
	if (!FiniteVec3(rotation))
		return false;
	if (!FloatUsableScalar(rotation.x) || !FloatUsableScalar(rotation.y) ||
	    !FloatUsableScalar(rotation.z))
		return false;
	if (!RotatorHasAim(rotation))
		return false;
	if (Absf(rotation.x) > 90.5f || Absf(rotation.y) > 360.5f || Absf(rotation.z) > 180.5f)
		return false;
	return true;
}

inline bool ControlRotationReferenceSelfCheck() {
	const Vector3 garbage(0.f, 9.2197e-41f, 4.11918e-20f);
	return !ControlRotationTrustworthyForCompare(garbage) &&
	       ControlRotationTrustworthyForCompare(Vector3(0.87f, -88.f, 0.0089f));
}

inline float CameraForwardDepth(const Vector3& location, const Vector3& rotation,
                                const Vector3& worldPoint) {
	const D3DMATRIX m = Matrix(rotation);
	const Vector3 forward(m.m[0][0], m.m[0][1], m.m[0][2]);
	const float dx = worldPoint.x - location.x;
	const float dy = worldPoint.y - location.y;
	const float dz = worldPoint.z - location.z;
	return dx * forward.x + dy * forward.y + dz * forward.z;
}

inline bool CameraSeesPoint(const Vector3& location, const Vector3& rotation,
                            const Vector3& worldPoint) {
	return CameraForwardDepth(location, rotation, worldPoint) >= 1.f;
}

inline bool TryPickCameraLocationOnPcm(uintptr_t pcm, const Vector3& ctrlRot, const CameraRef& ref,
                                       Vector3& outLocation, uint32_t& outOffset) {
	if (!pcm || !Memory::IsValid(pcm) || !ref.valid)
		return false;
	static std::vector<uint8_t> buffer(kPovScanBytes);
	const int read = ReadObjectBlock(pcm, buffer.data(), kPovScanBytes);
	float bestScore = -1.f;
	bool found = false;
	for (int offset = 0; offset + 0xC <= read; offset += 4) {
		float f[3];
		memcpy(f, buffer.data() + offset, sizeof(f));
		const Vector3 loc(f[0], f[1], f[2]);
		if (CameraLocationMaxAbs(loc) <= 1000.f)
			continue;
		const float dPawn = Dist3(loc, ref.pawn);
		if (dPawn < 150.f || dPawn > 4000.f)
			continue;
		if (!CameraSeesPoint(loc, ctrlRot, ref.head))
			continue;
		const float score = 3000.f - Absf(dPawn - 450.f);
		if (found && score <= bestScore)
			continue;
		bestScore = score;
		outLocation = loc;
		outOffset = static_cast<uint32_t>(offset);
		found = true;
	}
	return found;
}

inline float AbsYawDelta(float a, float b) {
	float d = a - b;
	while (d > 180.f)
		d -= 360.f;
	while (d < -180.f)
		d += 360.f;
	return d < 0.f ? -d : d;
}

inline float ScorePcmViewLocation(const Vector3& loc, float fov, const Vector3& ctrlRot,
                                  const CameraRef& ref) {
	if (!ref.valid || !CameraPovUsable(loc, fov))
		return -1.f;
	const float dHead = Dist3(loc, ref.head);
	const float dPawn = Dist3(loc, ref.pawn);
	if (dHead < 80.f)
		return -1.f;
	if (dPawn < 150.f || dPawn > kCameraNearPawnMaxDistance)
		return -1.f;
	if (!CameraSeesPoint(loc, ctrlRot, ref.head))
		return -1.f;
	// Third-person camera sits behind the pawn, not on the nearest float to the head.
	return 2000.f - Absf(dPawn - 450.f) - dHead * 0.05f;
}

inline bool TryFindLivePovOnPcm(uintptr_t pcm, const Vector3& ctrlRot, const CameraRef& ref,
                                ScannedPov& out) {
	if (!pcm || !Memory::IsValid(pcm) || !ref.valid)
		return false;
	static std::vector<uint8_t> buffer(kPovScanBytes);
	const int read = ReadObjectBlock(pcm, buffer.data(), kPovScanBytes);
	float bestScore = -1.f;
	bool found = false;
	for (int offset = 0; offset + 0x1C <= read; offset += 4) {
		float f[7];
		memcpy(f, buffer.data() + offset, sizeof(f));
		const Vector3 loc(f[0], f[1], f[2]);
		const Vector3 rot(f[3], f[4], f[5]);
		if (!PovBlockPlausible(loc, rot, f[6]))
			continue;
		if (AbsYawDelta(rot.y, ctrlRot.y) > 120.f)
			continue;
		if (Absf(rot.x - ctrlRot.x) > 60.f)
			continue;
		if (!CameraSeesPoint(loc, rot, ref.head))
			continue;
		const float dPawn = Dist3(loc, ref.pawn);
		if (dPawn < 50.f || dPawn > kCameraNearPawnMaxDistance)
			continue;
		const float score = 3000.f - Absf(dPawn - 450.f);
		if (found && score <= bestScore)
			continue;
		bestScore = score;
		out = { static_cast<uint32_t>(offset), loc, rot, f[6] };
		found = true;
	}
	return found;
}

inline bool TryPickPcmViewForControlRotation(uintptr_t pcm, const Vector3& ctrlRot,
                                             const CameraRef& ref, Vector3& location, float& fov,
                                             uint32_t& outOffset) {
	if (!pcm || !Memory::IsValid(pcm) || !ref.valid)
		return false;
	static std::vector<uint8_t> buffer(kPovScanBytes);
	const int read = ReadObjectBlock(pcm, buffer.data(), kPovScanBytes);
	float bestScore = -1.f;
	bool found = false;
	for (int offset = 0; offset + 0x1C <= read; offset += 4) {
		float f[7];
		memcpy(f, buffer.data() + offset, sizeof(f));
		const Vector3 loc(f[0], f[1], f[2]);
		const float fv = f[6];
		const float score = ScorePcmViewLocation(loc, fv, ctrlRot, ref);
		if (score < 0.f || (found && score <= bestScore))
			continue;
		bestScore = score;
		location = loc;
		fov = fv;
		outOffset = static_cast<uint32_t>(offset);
		found = true;
	}
	return found;
}

inline Vector3 CameraOrbitPivot(const CameraRef& ref) {
	if (!ref.valid)
		return Vector3{};
	Vector3 pivot{};
	pivot.x = ref.head.x * 0.35f + ref.pawn.x * 0.65f;
	pivot.y = ref.head.y * 0.35f + ref.pawn.y * 0.65f;
	pivot.z = ref.pawn.z + 88.f;
	if (ref.head.z > ref.pawn.z + 40.f)
		pivot.z = ref.pawn.z + (ref.head.z - ref.pawn.z) * 0.55f;
	return pivot;
}

// Eye below the capsule reads as "looking up" and draws ESP above the mesh.
inline void SanitizeThirdPersonEyeHeight(const CameraRef& ref, Vector3& loc) {
	if (!ref.valid || !FiniteVec3(loc))
		return;
	const float minZ = ref.pawn.z + 55.f;
	// Shoulder-height orbit; high eye (130+) foreshortens lobby TPS and shrinks ESP on screen.
	const float maxZ = ref.pawn.z + 102.f;
	if (loc.z < minZ)
		loc.z = minZ;
	if (loc.z > maxZ)
		loc.z = maxZ;
}

inline void NudgeThirdPersonEyeDistanceFromPawn(const CameraRef& ref, Vector3& loc,
                                                float minDistFromPawn) {
	if (!ref.valid || minDistFromPawn <= 0.f)
		return;
	const float d = Dist3(loc, ref.pawn);
	if (d >= minDistFromPawn || d < 1.f)
		return;
	const float inv = 1.f / d;
	const Vector3 dir((loc.x - ref.pawn.x) * inv, (loc.y - ref.pawn.y) * inv,
	                  (loc.z - ref.pawn.z) * inv);
	loc.x = ref.pawn.x + dir.x * minDistFromPawn;
	loc.y = ref.pawn.y + dir.y * minDistFromPawn;
	loc.z = ref.pawn.z + dir.z * minDistFromPawn;
}

inline bool TryGuessThirdPersonCameraLocation(const Vector3& pivot, const Vector3& rotation,
                                              Vector3& out) {
	const D3DMATRIX m = Matrix(rotation);
	const Vector3 forward(m.m[0][0], m.m[0][1], m.m[0][2]);
	const Vector3 up(m.m[2][0], m.m[2][1], m.m[2][2]);
	static const float kPullBack[] = { 320.f, 420.f, 520.f, 640.f, 780.f, 960.f, 1200.f };
	float bestDepth = -1.f;
	bool found = false;
	for (float dist : kPullBack) {
		const Vector3 guess(pivot.x - forward.x * dist + up.x * 72.f,
		                    pivot.y - forward.y * dist + up.y * 72.f,
		                    pivot.z - forward.z * dist + up.z * 72.f);
		if (!FiniteVec3(guess) || CameraLocationMaxAbs(guess) <= 100.f)
			continue;
		const float depth = CameraForwardDepth(guess, rotation, pivot);
		if (depth < 1.f)
			continue;
		if (found && depth <= bestDepth)
			continue;
		bestDepth = depth;
		out = guess;
		found = true;
	}
	return found;
}

inline float ResolveSyntheticGameplayFov(uintptr_t pcm, float cacheOrGuessFov);
inline void FinalizeSyntheticGameplayCamera(uintptr_t pcm, const CameraRef& ref, Vector3& loc,
                                            float& fov);

inline bool TryThirdPersonCameraFromPcm(uintptr_t pcm, const Vector3& rotation, const CameraRef& ref,
                                      Vector3& outLocation, float& outFov) {
	if (!pcm || !Memory::IsValid(pcm) || !ref.valid)
		return false;
	float dist = Read<float>(pcm + Offsets::FreeCamDistance);
	if (!(dist >= 220.f && dist <= 480.f))
		dist = 360.f;
	Vector3 freeOff = Read<Vector3>(pcm + Offsets::FreeCamOffset);
	if (!FiniteVec3(freeOff) || CameraLocationMaxAbs(freeOff) > 400.f)
		freeOff = Vector3{};
	const D3DMATRIX m = Matrix(rotation);
	const Vector3 forward(m.m[0][0], m.m[0][1], m.m[0][2]);
	const Vector3 pivot = CameraOrbitPivot(ref);
	outLocation = Vector3(pivot.x - forward.x * dist + freeOff.x,
	                      pivot.y - forward.y * dist + freeOff.y,
	                      pivot.z - forward.z * dist + freeOff.z);
	Vector3 cacheLoc{}, cacheRot{};
	float cacheFov = 0.f;
	ReadManagerPov(pcm, static_cast<uint32_t>(Offsets::camera_cache_private), cacheLoc, cacheRot,
	               cacheFov);
	outFov = ResolveSyntheticGameplayFov(pcm, cacheFov);
	FinalizeSyntheticGameplayCamera(pcm, ref, outLocation, outFov);
	if (!FiniteVec3(outLocation) || CameraLocationMaxAbs(outLocation) <= 100.f)
		return false;
	return CameraSeesPoint(outLocation, rotation, ref.head);
}

struct PcmSdkCacheCalibration {
	bool valid = false;
	uint32_t cacheEntryOffset = 0;
	uint32_t povDeltaFromEntry = PcmSdkLayout::kCacheEntryPov;
	const char* sourceName = nullptr;
};
inline PcmSdkCacheCalibration g_PcmSdkCacheCalibration{};

struct EngineReferenceView {
	Vector3 location{};
	Vector3 rotation{};
	float fov = 80.f;
	float aspect = 0.f;
	bool controlRotationOk = false;
	bool cachePovOk = false;
	const char* cacheSource = nullptr;
};

inline constexpr float kCameraControlRotationToleranceDeg = 15.f;
inline uint32_t g_ControlRotationRefOffset = 0;

inline Vector3 NormalizeRotatorDegrees(const Vector3& rotation) {
	return Vector3(NormalizeRotatorAxisDegrees(rotation.x), NormalizeRotatorAxisDegrees(rotation.y),
	               NormalizeRotatorAxisDegrees(rotation.z));
}

inline const char* DiagnoseControlRotationMismatch(const Vector3& povRot, const Vector3& ctrlRot,
                                                   float* outPitchDelta = nullptr,
                                                   float* outYawDelta = nullptr) {
	if (!FiniteVec3(povRot) || !FiniteVec3(ctrlRot))
		return "rotation_non_finite";
	const Vector3 pov = NormalizeRotatorDegrees(povRot);
	const Vector3 ctrl = NormalizeRotatorDegrees(ctrlRot);
	const float pitchDelta = Absf(NormalizeRotatorAxisDegrees(pov.x - ctrl.x));
	const float yawDelta = Absf(NormalizeRotatorAxisDegrees(pov.y - ctrl.y));
	if (outPitchDelta)
		*outPitchDelta = pitchDelta;
	if (outYawDelta)
		*outYawDelta = yawDelta;
	if (pitchDelta >= kCameraControlRotationToleranceDeg)
		return "rotation_pitch_mismatch";
	if (yawDelta >= kCameraControlRotationToleranceDeg)
		return "rotation_yaw_mismatch";
	return nullptr;
}

inline bool ControlRotationSelfCheck() {
	return DiagnoseControlRotationMismatch(Vector3(359.f, 10.f, 0.f), Vector3(-1.f, 10.f, 0.f)) ==
	           nullptr &&
	       DiagnoseControlRotationMismatch(Vector3(0.f, 179.f, 0.f), Vector3(0.f, -179.f, 0.f)) ==
	           nullptr &&
	       std::strcmp(DiagnoseControlRotationMismatch(Vector3(30.f, 0.f, 0.f), Vector3(0.f, 0.f, 0.f)),
	                   "rotation_pitch_mismatch") == 0;
}

inline void LogRotationCheck(const char* source, const Vector3& raw, const Vector3& ctrlRaw,
                             uint32_t ctrlOffset, const char* verdict) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static char lastKey[96]{};
	static unsigned long long nextMs = 0;
	char key[96];
	std::snprintf(key, sizeof(key), "%s|%X", verdict ? verdict : "ok", ctrlOffset);
	const unsigned long long now = GetTickCount64();
	if (std::strcmp(key, lastKey) == 0 && now < nextMs)
		return;
	std::strncpy(lastKey, key, sizeof(lastKey) - 1);
	nextMs = now + 5000;
	const Vector3 norm = NormalizeRotatorDegrees(raw);
	const Vector3 ctrl = NormalizeRotatorDegrees(ctrlRaw);
	float pitchDelta = 0.f, yawDelta = 0.f;
	(void)DiagnoseControlRotationMismatch(raw, ctrlRaw, &pitchDelta, &yawDelta);
	const bool documentedRef = ctrlOffset == static_cast<uint32_t>(Offsets::ControlRotation);
	std::cout << xorstr_("[camera] rotation_check source=") << (source ? source : "?")
	          << xorstr_(" raw=") << raw.x << ',' << raw.y << ',' << raw.z
	          << xorstr_(" normalized=") << norm.x << ',' << norm.y << ',' << norm.z
	          << xorstr_(" validator=") << norm.x << ',' << norm.y << ',' << norm.z
	          << xorstr_(" reference_raw=") << ctrlRaw.x << ',' << ctrlRaw.y << ',' << ctrlRaw.z
	          << xorstr_(" reference=") << ctrl.x << ',' << ctrl.y << ',' << ctrl.z
	          << xorstr_(" reference_src=PC+0x") << std::hex << std::uppercase << ctrlOffset
	          << std::dec << (documentedRef ? xorstr_("(ControlRotation)") : xorstr_("(heuristic)"))
	          << xorstr_(" pitch_delta=") << pitchDelta << xorstr_(" yaw_delta=") << yawDelta
	          << xorstr_(" roll=ignored tol=") << kCameraControlRotationToleranceDeg
	          << xorstr_(" unit=degrees order=PYR type=float32 verdict=")
	          << (verdict ? verdict : "ok") << '\n';
}

inline bool ControlRotationNear(const Vector3& povRot, const Vector3& ctrlRot) {
	return DiagnoseControlRotationMismatch(povRot, ctrlRot) == nullptr;
}

inline bool PovMatchesEngineControlRotation(uintptr_t playerController, const Vector3& povRot) {
	Vector3 ctrl{};
	uint32_t rotOff = 0;
	if (!TryReadControlRotation(playerController, ctrl, rotOff))
		return true;
	return ControlRotationNear(povRot, ctrl);
}

inline bool DocumentedPovRotationAcceptable(uintptr_t playerController, const Vector3& location,
                                            const Vector3& rotation, float fov, float aspect,
                                            const CameraRef* spatialRef) {
	if (!playerController || !Memory::IsValid(playerController))
		return true;
	if (PovMatchesEngineControlRotation(playerController, rotation))
		return true;
	return PovPassesReticleGate(location, rotation, fov, aspect, playerController, spatialRef);
}

inline bool TryApplyReticleGatedCameraPov(uintptr_t playerController, const CameraRef* spatialRef,
                                          const Vector3& location, const Vector3& rotation, float fov,
                                          float aspect, const char* source, uint32_t offset) {
	if (!DocumentedPovRotationAcceptable(playerController, location, rotation, fov, aspect,
	                                     spatialRef))
		return false;
	ApplyCameraPov(location, rotation, fov, source, offset, aspect);
	return Camera::Valid;
}

// Loc+rot+fov read together from PCM memory (not a synthetic eye). Reticle gate is for TPS guesses.
inline bool TryApplyEngineBundledPcmPov(uintptr_t playerController, const CameraRef* spatialRef,
                                        const Vector3& location, const Vector3& rotation, float fov,
                                        float aspect, const char* source, uint32_t offset) {
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	if (DiagnoseCameraPovRejectReason(location, rotation, fov, spatialRef, sw, sh))
		return false;
	if (!CameraPovStrictAccept(location, rotation, fov, spatialRef))
		return false;
	if (IsTemplateFovDegrees(fov))
		return false;
	if (spatialRef && spatialRef->valid) {
		const float dPawn = Dist3(location, spatialRef->pawn);
		if (dPawn < 50.f || dPawn > kCameraNearPawnMaxDistance)
			return false;
		if (!CameraSeesPoint(location, rotation, spatialRef->head))
			return false;
	}
	// Bundled engine POV: rotation came with location from the same struct — do not re-gate on
	// ControlRotation or reticle (camera lag vs ctrl is normal in third person / skydive).
	ApplyCameraPov(location, rotation, fov, source, offset, aspect);
	return Camera::Valid;
}

inline constexpr unsigned long long kLastGoodCameraPovHoldMs = 2500;

inline bool TryRestoreLastGoodCameraPov(uintptr_t playerController, const CameraRef& ref) {
	if (!g_LastGoodCameraPov.valid || !g_LastGoodCameraPov.source[0])
		return false;
	const unsigned long long now = GetTickCount64();
	if (now - g_LastGoodCameraPov.tickMs > kLastGoodCameraPovHoldMs)
		return false;
	const CameraRef* spatialPtr = ref.valid ? &ref : nullptr;
	Vector3 rotation = g_LastGoodCameraPov.rotation;
	uint32_t rotOff = g_LastGoodCameraPov.sourceOffset;
	Vector3 ctrl{};
	if (TryReadControlRotation(playerController, ctrl, rotOff))
		rotation = ctrl;
	if (!PovPassesReticleGate(g_LastGoodCameraPov.location, rotation, g_LastGoodCameraPov.fov,
	                            g_LastGoodCameraPov.aspect, playerController, spatialPtr))
		return false;
	ApplyCameraPov(g_LastGoodCameraPov.location, rotation, g_LastGoodCameraPov.fov,
	               g_LastGoodCameraPov.source, g_LastGoodCameraPov.sourceOffset,
	               g_LastGoodCameraPov.aspect);
	return Camera::Valid;
}

inline EngineReferenceView ReadEngineReferenceView(uintptr_t playerController, uintptr_t pcm) {
	EngineReferenceView view{};
	uint32_t rotOff = 0;
	if (playerController && TryReadControlRotation(playerController, view.rotation, rotOff))
		view.controlRotationOk = true;
	if (pcm && Memory::IsValid(pcm)) {
		const float def = Read<float>(pcm + Offsets::DefaultFOV);
		if (def >= 40.f && def <= 120.f)
			view.fov = def;
	}
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const CameraRef ref = ReadLocalCameraRef();
	const CameraRef* spatialPtr = ref.valid ? &ref : nullptr;
	int fieldCount = 0;
	const SdkPovField* fields = SdkPovFields(fieldCount);
	for (int i = 0; i < fieldCount; ++i) {
		if (!fields[i].live)
			continue;
		Vector3 loc{}, rot{};
		float fov = 0.f;
		ReadManagerPovFromSdkCacheEntry(pcm, fields[i].offset, loc, rot, fov, &view.aspect);
		if (DiagnoseCameraPovRejectReason(loc, rot, fov, spatialPtr, sw, sh))
			continue;
		if (view.controlRotationOk && !ControlRotationNear(rot, view.rotation))
			continue;
		view.location = loc;
		view.rotation = rot;
		view.fov = fov;
		view.cachePovOk = true;
		view.cacheSource = fields[i].name;
		break;
	}
	return view;
}

inline void LogPcmStructureProbeOnce(uintptr_t pcm, uintptr_t playerController) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace) || !pcm || !Memory::IsValid(pcm))
		return;
	static std::atomic<bool> logged{ false };
	if (logged.exchange(true))
		return;
	auto logCache = [&](const char* label, uint32_t entryOff) {
		Vector3 loc{}, rot{};
		float fov = 0.f;
		float aspect = 0.f;
		ReadManagerPovFromSdkCacheEntry(pcm, entryOff, loc, rot, fov, &aspect);
		const bool templ = IsDefaultTemplatePov(loc, rot, fov);
		std::cout << xorstr_("[camera] pcm probe ") << label << xorstr_(" entry+0x")
		          << std::hex << entryOff << std::dec << xorstr_(" pov loc ") << loc.x << ','
		          << loc.y << ',' << loc.z << xorstr_(" rot ") << rot.x << ',' << rot.y << ','
		          << rot.z << xorstr_(" fov ") << fov << xorstr_(" aspect ") << aspect
		          << xorstr_(" template=") << (templ ? 1 : 0) << '\n';
	};
	logCache("CameraCache", static_cast<uint32_t>(Offsets::CameraCache));
	logCache("CameraCachePrivate", static_cast<uint32_t>(Offsets::camera_cache_private));
	const float defFov = Read<float>(pcm + Offsets::DefaultFOV);
	const float modFov = Read<float>(pcm + Offsets::PcmModifiableFov);
	const float freeDist = Read<float>(pcm + Offsets::FreeCamDistance);
	Vector3 freeOff = Read<Vector3>(pcm + Offsets::FreeCamOffset);
	Vector3 ctrl{};
	uint32_t rotOff = 0;
	if (playerController && TryReadControlRotation(playerController, ctrl, rotOff))
		std::cout << xorstr_("[camera] pcm probe ControlRotation ") << ctrl.x << ',' << ctrl.y
		          << ',' << ctrl.z << xorstr_(" DefaultFOV@0x238 ") << defFov
		          << xorstr_(" ModifiableFov@0x23C ") << modFov << xorstr_(" FreeCamDistance ")
		          << freeDist << xorstr_(" FreeCamOffset ") << freeOff.x << ',' << freeOff.y << ','
		          << freeOff.z << '\n';
	Vector3 pubLoc{}, pubRot{}, privLoc{}, privRot{};
	float pubFov = 0.f, privFov = 0.f;
	ReadManagerPovFromSdkCacheEntry(pcm, static_cast<uint32_t>(Offsets::CameraCache), pubLoc, pubRot,
	                              pubFov, nullptr);
	ReadManagerPovFromSdkCacheEntry(pcm, static_cast<uint32_t>(Offsets::camera_cache_private),
	                                privLoc, privRot, privFov, nullptr);
	const bool pubPopulated = !IsDefaultTemplatePov(pubLoc, pubRot, pubFov);
	const CameraRef anchor = ReadCameraSpatialAnchor();
	const bool privLayoutOk =
	    CameraPovStrictAccept(privLoc, privRot, privFov, anchor.valid ? &anchor : nullptr) &&
	    !IsDefaultTemplatePov(privLoc, privRot, privFov);
	std::cout << xorstr_("[camera] pcm verdict cache_populated=") << (pubPopulated ? "yes" : "no")
	          << xorstr_(" private_matches_minimal_view_info=") << (privLayoutOk ? "yes" : "no")
	          << xorstr_(" template_pov=") << (IsDefaultTemplatePov(pubLoc, pubRot, pubFov) ? "yes" : "no")
	          << '\n';
}

inline void LogEngineVsSdkCameraCompareOnce(const EngineReferenceView& engine, uintptr_t pcm) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace) || !pcm)
		return;
	static std::atomic<bool> logged{ false };
	if (logged.exchange(true))
		return;
	std::cout << xorstr_("[camera] engine ControlRotation ") << engine.rotation.x << ','
	          << engine.rotation.y << ',' << engine.rotation.z << xorstr_(" DefaultFOV ")
	          << engine.fov;
	if (engine.cachePovOk) {
		std::cout << xorstr_(" | sdk cache [") << (engine.cacheSource ? engine.cacheSource : "?")
		          << xorstr_("] loc ") << engine.location.x << ',' << engine.location.y << ','
		          << engine.location.z << xorstr_(" rot ") << engine.rotation.x << ','
		          << engine.rotation.y << ',' << engine.rotation.z << xorstr_(" fov ")
		          << engine.fov;
	} else {
		std::cout << xorstr_(" | sdk cache POV: none matched engine rotation + validation");
	}
	std::cout << '\n';
}

// Temporary camera pipeline diagnostics (does not publish synthetic POV).
namespace CameraDiagnostics {
inline constexpr uint32_t kMaxCandidates = 96;
inline constexpr uint32_t kMaxScanAddsPerObject = 24;
inline constexpr uint32_t kPcScanBytes = 0x3200;
inline constexpr uint32_t kConfirmValidStreak = 3;
inline constexpr uint32_t kConfirmMotionEvents = 1;

struct CandidateKey {
	uintptr_t object = 0;
	uint32_t fieldOffset = 0;
	uint32_t povDelta = 0;
};

struct CandidateSample {
	CandidateKey key{};
	char source[56]{};
	Vector3 location{};
	Vector3 rotation{};
	float fov = 0.f;
	float aspect = 0.f;
	float cacheTimestamp = 0.f;
	bool valid = false;
	const char* rejectReason = nullptr;
	uint32_t frameAge = 0;
	bool movedSinceLast = false;
	bool ctrlRotConsistent = false;
};

struct CandidateTrack {
	CandidateKey key{};
	char source[56]{};
	Vector3 lastLoc{};
	Vector3 lastRot{};
	float lastFov = 0.f;
	float lastTimestamp = 0.f;
	Vector3 lastCtrl{};
	uint32_t validStreak = 0;
	uint32_t motionEvents = 0;
	uint32_t tickSerial = 0;
	bool confirmed = false;
};

inline CandidateTrack g_tracks[kMaxCandidates]{};
inline int g_trackCount = 0;
inline CandidateKey g_confirmedKey{};
inline char g_confirmedSource[56]{};
inline bool g_hasConfirmed = false;
inline uint32_t g_tickSerial = 0;
inline uintptr_t g_trackedPcm = 0;

// Tracks and confirmed authority are keyed by object address; a new PCM (world transition)
// must re-earn authority instead of reading the previous, possibly freed, object.
inline void ResetAuthorityIfPcmChanged(uintptr_t pcm) {
	if (g_trackedPcm == pcm)
		return;
	if ((g_hasConfirmed || g_trackCount > 0) &&
	    Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		std::cout << xorstr_("[camera] authority_reset reason=pcm_changed old=0x") << std::hex
		          << std::uppercase << g_trackedPcm << xorstr_(" new=0x") << pcm << std::dec
		          << xorstr_(" confirmed=") << (g_hasConfirmed ? 1 : 0) << '\n';
	g_trackedPcm = pcm;
	for (CandidateTrack& t : g_tracks)
		t = CandidateTrack{};
	g_trackCount = 0;
	g_hasConfirmed = false;
	g_confirmedKey = {};
	g_confirmedSource[0] = '\0';
}

inline bool KeysEqual(const CandidateKey& a, const CandidateKey& b) {
	return a.object == b.object && a.fieldOffset == b.fieldOffset && a.povDelta == b.povDelta;
}

inline CandidateTrack* FindTrack(const CandidateKey& key) {
	for (int i = 0; i < g_trackCount; ++i) {
		if (KeysEqual(g_tracks[i].key, key))
			return &g_tracks[i];
	}
	return nullptr;
}

inline CandidateTrack* AllocTrack(const CandidateKey& key, const char* source) {
	CandidateTrack* t = FindTrack(key);
	if (t)
		return t;
	if (g_trackCount >= kMaxCandidates)
		return nullptr;
	t = &g_tracks[g_trackCount++];
	t->key = key;
	if (source && source[0])
		std::strncpy(t->source, source, sizeof(t->source) - 1);
	return t;
}

inline void LogCameraFovValueFlowOnce(const char* source, float rawFov) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static std::atomic<bool> logged{ false };
	if (logged.exchange(true))
		return;
	const float normalizedFov = rawFov; // FMinimalViewInfo::FOV is already float degrees.
	const float validatorFov = normalizedFov;
	uint32_t bits = 0;
	std::memcpy(&bits, &validatorFov, sizeof(bits));
	std::cout << xorstr_("[camera] fov_flow source=") << (source ? source : "?")
	          << xorstr_(" raw_fov=") << rawFov << xorstr_(" normalized_fov=") << normalizedFov
	          << xorstr_(" validator_fov=") << validatorFov
	          << xorstr_(" type=float32 bits=0x") << std::hex << std::uppercase << bits << std::dec
	          << xorstr_(" unit=degrees expected=(") << kCameraFovDegreesMin << ','
	          << kCameraFovDegreesMax << xorstr_(") verdict=")
	          << ValidateCameraFovDegrees(validatorFov) << '\n';
}

inline void SampleCandidate(uintptr_t pcm, const char* name, uint32_t fieldOff, uint32_t povDelta,
                            CandidateSample& out, const CameraRef* spatial, float sw, float sh,
                            const Vector3& ctrlRot, bool ctrlOk) {
	out = {};
	out.key = { pcm, fieldOff, povDelta };
	if (name && std::strstr(name, ".scan+0x") != nullptr)
		std::strncpy(out.source, name, sizeof(out.source) - 1);
	else if (name)
		std::snprintf(out.source, sizeof(out.source), "%s+pov+0x%X", name,
		              static_cast<unsigned>(povDelta));
	ReadManagerPovAt(pcm, fieldOff, povDelta, out.location, out.rotation, out.fov, &out.aspect);
	out.cacheTimestamp = Read<float>(pcm + fieldOff + PcmSdkLayout::kCacheEntryTimestamp);
	if (IsDefaultTemplatePov(out.location, out.rotation, out.fov)) {
		out.rejectReason = "template_pov";
		return;
	}
	if (FiniteVec3(out.location) && FiniteVec3(out.rotation) && RotatorHasAim(out.rotation) &&
	    CameraLocationMaxAbs(out.location) > 100.f &&
	    std::strcmp(ValidateCameraFovDegrees(out.fov), "ok") == 0)
		LogCameraFovValueFlowOnce(out.source, out.fov);
	out.rejectReason =
	    DiagnoseCameraPovRejectReason(out.location, out.rotation, out.fov, spatial, sw, sh);
	if (out.rejectReason)
		return;
	if (ctrlOk) {
		out.rejectReason = DiagnoseControlRotationMismatch(out.rotation, ctrlRot);
		if (povDelta == PcmSdkLayout::kCacheEntryPov)
			LogRotationCheck(out.source, out.rotation, ctrlRot, g_ControlRotationRefOffset,
			                 out.rejectReason);
		if (out.rejectReason)
			return;
	}
	out.ctrlRotConsistent = ctrlOk;
	out.valid = true;
}

inline void UpdateTrack(CandidateTrack& track, const CandidateSample& sample, const Vector3& ctrlRot,
                        bool ctrlOk) {
	track.tickSerial = g_tickSerial;
	if (!sample.valid) {
		track.validStreak = 0;
		return;
	}
	const bool locMoved = track.validStreak > 0 && Dist3(sample.location, track.lastLoc) > 2.f;
	const bool rotMoved =
	    track.validStreak > 0 &&
	    (AbsYawDelta(sample.rotation.y, track.lastRot.y) > 0.15f ||
	     Absf(sample.rotation.x - track.lastRot.x) > 0.15f);
	const bool ctrlMoved =
	    ctrlOk && track.validStreak > 0 &&
	    (AbsYawDelta(ctrlRot.y, track.lastCtrl.y) > 0.15f ||
	     Absf(ctrlRot.x - track.lastCtrl.x) > 0.15f);
	const bool tsMoved =
	    track.validStreak > 0 && Absf(sample.cacheTimestamp - track.lastTimestamp) > 1e-4f &&
	    sample.cacheTimestamp > 0.f;
	if ((rotMoved && ctrlMoved) || (locMoved && ctrlMoved) || (locMoved && tsMoved))
		track.motionEvents++;
	track.validStreak++;
	track.lastLoc = sample.location;
	track.lastRot = sample.rotation;
	track.lastFov = sample.fov;
	track.lastTimestamp = sample.cacheTimestamp;
	if (ctrlOk)
		track.lastCtrl = ctrlRot;
	if (!g_hasConfirmed && track.validStreak >= kConfirmValidStreak &&
	    track.motionEvents >= kConfirmMotionEvents) {
		track.confirmed = true;
		g_hasConfirmed = true;
		g_confirmedKey = track.key;
		std::strncpy(g_confirmedSource, track.source, sizeof(g_confirmedSource) - 1);
	}
}

inline void LogCandidateLine(const CandidateSample& s, uint32_t trackValidStreak,
                             uint32_t motionEvents) {
	std::cout << xorstr_("[camera-diag] src=") << s.source << xorstr_(" obj=0x") << std::hex
	          << s.key.object << xorstr_(" field+0x") << s.key.fieldOffset << xorstr_(" pov+0x")
	          << s.key.povDelta << std::dec << xorstr_(" loc ") << s.location.x << ','
	          << s.location.y << ',' << s.location.z << xorstr_(" rot ") << s.rotation.x << ','
	          << s.rotation.y << ',' << s.rotation.z << xorstr_(" fov ") << s.fov
	          << xorstr_(" ts ") << s.cacheTimestamp << xorstr_(" valid=") << (s.valid ? 1 : 0)
	          << xorstr_(" reason=") << (s.rejectReason ? s.rejectReason : "ok")
	          << xorstr_(" streak=") << trackValidStreak << xorstr_(" motion=") << motionEvents
	          << xorstr_(" moved=") << (s.movedSinceLast ? 1 : 0)
	          << xorstr_(" ctrl_ok=") << (s.ctrlRotConsistent ? 1 : 0) << '\n';
}

inline bool OffsetIsKnownSdkPov(uint32_t absOffset) {
	int fieldCount = 0;
	const SdkPovField* fields = SdkPovFields(fieldCount);
	for (int i = 0; i < fieldCount; ++i) {
		if (absOffset == fields[i].offset + PcmSdkLayout::kCacheEntryPov ||
		    absOffset == fields[i].offset || absOffset == fields[i].offset + 0x8u)
			return true;
	}
	return false;
}

inline int AppendMinimalViewInfoScan(uintptr_t object, const char* objectLabel,
                                     CandidateSample* samples, int sampleCount,
                                     const CameraRef* spatial, float sw, float sh,
                                     const Vector3& ctrlRot, bool ctrlOk) {
	if (!object || !Memory::IsValid(object) || sampleCount >= static_cast<int>(kMaxCandidates))
		return sampleCount;
	static std::vector<uint8_t> buffer(kPcScanBytes);
	const int read = ReadObjectBlock(object, buffer.data(), kPcScanBytes);
	int added = 0;
	for (int offset = 0; offset + 0x20 <= read && sampleCount < static_cast<int>(kMaxCandidates) &&
	                    added < static_cast<int>(kMaxScanAddsPerObject);
	     offset += 4) {
		const uint32_t off = static_cast<uint32_t>(offset);
		if (objectLabel && std::strcmp(objectLabel, "PCM") == 0 && OffsetIsKnownSdkPov(off))
			continue;
		float f[7];
		memcpy(f, buffer.data() + offset, sizeof(f));
		const Vector3 rot(f[3], f[4], f[5]);
		if (!ctrlOk || !ControlRotationNear(rot, ctrlRot))
			continue;
		char name[48];
		std::snprintf(name, sizeof(name), "%s.scan+0x%X", objectLabel ? objectLabel : "OBJ", off);
		SampleCandidate(object, name, off, 0u, samples[sampleCount], spatial, sw, sh, ctrlRot,
		                ctrlOk);
		samples[sampleCount].key.object = object;
		sampleCount++;
		added++;
	}
	return sampleCount;
}

inline void LogBootSdkSummary(uintptr_t pcm, const CameraRef* spatial, float sw, float sh,
                              const Vector3& ctrlRot, bool ctrlOk) {
	static std::atomic<bool> logged{ false };
	if (logged.exchange(true))
		return;
	int fieldCount = 0;
	const SdkPovField* fields = SdkPovFields(fieldCount);
	static const uint32_t kPovDeltas[] = { PcmSdkLayout::kCacheEntryPov, 0x0u };
	int sdkSlots = 0;
	int sdkValid = 0;
	const char* lastReject = "structure_mismatch";
	CandidateSample agreeingCandidate{};
	bool haveAgreeingCandidate = false;
	int agreeingSources = 0;
	for (int fi = 0; fi < fieldCount; ++fi) {
		for (uint32_t povDelta : kPovDeltas) {
			CandidateSample s{};
			SampleCandidate(pcm, fields[fi].name, fields[fi].offset, povDelta, s, spatial, sw, sh,
			                ctrlRot, ctrlOk);
			sdkSlots++;
			if (s.valid)
				sdkValid++;
			else if (s.rejectReason)
				lastReject = s.rejectReason;
			const bool structurallyLive =
			    povDelta == PcmSdkLayout::kCacheEntryPov && FiniteVec3(s.location) &&
			    FiniteVec3(s.rotation) && RotatorHasAim(s.rotation) &&
			    CameraLocationMaxAbs(s.location) > 100.f &&
			    std::strcmp(ValidateCameraFovDegrees(s.fov), "ok") == 0;
			if (!structurallyLive)
				continue;
			if (!haveAgreeingCandidate) {
				agreeingCandidate = s;
				haveAgreeingCandidate = true;
				agreeingSources = 1;
			} else if (Dist3(s.location, agreeingCandidate.location) < 0.01f &&
			           Dist3(s.rotation, agreeingCandidate.rotation) < 0.01f &&
			           Absf(s.fov - agreeingCandidate.fov) < 0.001f) {
				agreeingSources++;
			}
		}
	}
	const char* verdict =
	    haveAgreeingCandidate
	        ? (agreeingCandidate.rejectReason ? agreeingCandidate.rejectReason : "ok")
	        : lastReject;
	std::cout << xorstr_("[camera] v23 phase=boot sdk_valid=") << sdkValid << '/' << sdkSlots
	          << xorstr_(" verdict=") << verdict << xorstr_(" agreement=") << agreeingSources
	          << xorstr_(" (detail=Trace)\n");
}

// Exactly the six documented PCM candidates the validator reads (entry + POV at +0x10), one
// block read each, so read failures are visible instead of zero-filled by Read<T>.
struct RawCameraCacheEntry {
	float timestamp;
	float header[3];
	float location[3];
	float rotation[3];
	float fov;
};
static_assert(sizeof(RawCameraCacheEntry) ==
                  PcmSdkLayout::kCacheEntryPov + PcmSdkLayout::kPovFov + sizeof(float),
              "entry header + FMinimalViewInfo location/rotation/fov");

inline constexpr int kMaxDocumentedCandidates = 8;

struct CandidateRead {
	const char* name = "";
	uint32_t offset = 0;
	bool readOk = false;
	bool hasTimestamp = false;
	float timestamp = 0.f;
	int timestampAdvanced = -1; // -1 unknown, 0 frozen over >=500ms, 1 advancing
	Vector3 location{};
	Vector3 rotation{};
	float fov = 0.f;
	const char* cls = "read_failed";
	const char* reason = "read_failed";
	bool selectable = false; // non-empty block that reached the validator predicates
};

struct CandidateScan {
	CandidateRead reads[kMaxDocumentedCandidates]{};
	int count = 0;
	int readOk = 0;
	int selectable = 0;
	int best = -1;
	char noLiveReason[64] = "no_live_candidate";
};

// Same predicates, same order as TryReadSdkDocumentedCamera; only groups the outcome.
inline void ClassifyCandidate(CandidateRead& c, const CameraRef* spatial, float sw, float sh,
                              const Vector3& ctrlRot, bool ctrlOk) {
	if (!c.readOk)
		return;
	const Vector3& loc = c.location;
	const Vector3& rot = c.rotation;
	if (IsDefaultTemplatePov(loc, rot, c.fov)) {
		c.cls = "template_pov";
		c.reason = "template_fov90_zero_pose";
		return;
	}
	if (!FiniteVec3(loc) || !FiniteVec3(rot) || !FiniteF(c.fov)) {
		c.cls = "non_finite";
		c.reason = !FiniteVec3(loc) ? "location_non_finite"
		         : !FiniteVec3(rot) ? "rotation_non_finite"
		                            : "fov_non_finite";
		return;
	}
	c.reason = DiagnoseCameraPovRejectReason(loc, rot, c.fov, spatial, sw, sh);
	if (c.reason && std::strcmp(c.reason, "pov_all_zero") == 0) {
		c.cls = "zero";
		return;
	}
	if (c.reason && (std::strcmp(c.reason, "location_denormal") == 0 ||
	                 std::strcmp(c.reason, "fov_invalid_value") == 0)) {
		c.cls = "subnormal";
		return;
	}
	c.selectable = true;
	if (c.reason) {
		c.cls = "invalid";
		return;
	}
	c.reason = ctrlOk ? DiagnoseControlRotationMismatch(rot, ctrlRot) : nullptr;
	if (c.reason) {
		c.cls = "rejected";
		return;
	}
	c.reason = "ok";
	c.cls = c.timestampAdvanced == 1 ? "valid_live" : c.timestampAdvanced == 0 ? "valid_stale" : "valid";
}

inline bool CandidateClassificationSelfCheck() {
	auto cls = [](bool readOk, const Vector3& loc, const Vector3& rot, float fov) {
		CandidateRead c{};
		c.readOk = readOk;
		c.location = loc;
		c.rotation = rot;
		c.fov = fov;
		ClassifyCandidate(c, nullptr, 1920.f, 1080.f, Vector3(), false);
		return c.cls;
	};
	const Vector3 zero(0.f, 0.f, 0.f);
	return std::strcmp(cls(false, zero, zero, 0.f), "read_failed") == 0 &&
	       std::strcmp(cls(true, zero, zero, 0.f), "zero") == 0 &&
	       std::strcmp(cls(true, zero, zero, 90.f), "template_pov") == 0 &&
	       std::strcmp(cls(true, Vector3(36.5f, -4.126f, 1.05795e-37f), Vector3(0.f, 0.f, -4.125f),
	                       1.17549e-38f),
	                   "subnormal") == 0 &&
	       std::strcmp(cls(true, Vector3(52.27f, 125913.f, 89.25f), Vector3(0.87f, -88.07f, 0.0089f),
	                       52.f),
	                   "valid") == 0;
}

inline void LogCandidateScanIfChanged(const CandidateScan& scan, uintptr_t pcm,
                                      uintptr_t playerController, const CameraRef& ref) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		return;
	static char lastSig[256]{};
	static unsigned long long lastMs = 0;
	char sig[256];
	int n = std::snprintf(sig, sizeof(sig), "%llX", static_cast<unsigned long long>(pcm));
	for (int i = 0; i < scan.count && n > 0 && n < static_cast<int>(sizeof(sig)); ++i)
		n += std::snprintf(sig + n, sizeof(sig) - n, "|%s", scan.reads[i].cls);
	if (std::strcmp(sig, lastSig) == 0)
		return;
	const unsigned long long now = GetTickCount64();
	if (lastMs && now - lastMs < 2000)
		return;
	std::strncpy(lastSig, sig, sizeof(lastSig) - 1);
	lastMs = now;
	const uintptr_t pcFreshPcm =
	    playerController && Memory::IsValid(playerController)
	        ? Read<uintptr_t>(playerController + Offsets::playercameramanager)
	        : 0;
	std::cout << xorstr_("[camera] candidates pcm=0x") << std::hex << std::uppercase << pcm;
	if (pcFreshPcm == pcm)
		std::cout << xorstr_(" pc_pcm=match");
	else
		std::cout << xorstr_(" pc_pcm=0x") << pcFreshPcm << xorstr_("(MISMATCH)");
	std::cout << std::dec;
	if (ref.valid)
		std::cout << xorstr_(" pawn=") << ref.pawn.x << ',' << ref.pawn.y << ',' << ref.pawn.z;
	else
		std::cout << xorstr_(" pawn=none");
	std::cout << xorstr_(" read_ok=") << scan.readOk << '/' << scan.count << xorstr_(" selectable=")
	          << scan.selectable << '/' << scan.count << '\n';
	for (int i = 0; i < scan.count; ++i) {
		const CandidateRead& c = scan.reads[i];
		std::cout << xorstr_("[camera]   ") << c.name << xorstr_("@0x") << std::hex << std::uppercase
		          << c.offset << std::dec;
		if (!c.readOk) {
			std::cout << xorstr_(" read=FAIL class=read_failed\n");
			continue;
		}
		std::cout << xorstr_(" read=ok ts=");
		if (c.hasTimestamp)
			std::cout << c.timestamp << xorstr_(" ts_adv=")
			          << (c.timestampAdvanced < 0 ? "?" : (c.timestampAdvanced ? "1" : "0"));
		else
			std::cout << xorstr_("n/a");
		std::cout << xorstr_(" loc=") << c.location.x << ',' << c.location.y << ',' << c.location.z
		          << xorstr_(" rot=") << c.rotation.x << ',' << c.rotation.y << ',' << c.rotation.z
		          << xorstr_(" fov=") << c.fov << xorstr_(" class=") << c.cls << xorstr_(" reason=")
		          << c.reason << '\n';
	}
}

inline CandidateScan ScanDocumentedCandidates(uintptr_t pcm, uintptr_t playerController,
                                              const CameraRef& ref) {
	CandidateScan scan{};
	static uintptr_t s_pcm = 0;
	static float s_refTs[kMaxDocumentedCandidates]{};
	static unsigned long long s_refMs[kMaxDocumentedCandidates]{};
	static int s_adv[kMaxDocumentedCandidates]{ -1, -1, -1, -1, -1, -1, -1, -1 };
	if (s_pcm != pcm) {
		s_pcm = pcm;
		for (int i = 0; i < kMaxDocumentedCandidates; ++i) {
			s_refMs[i] = 0;
			s_adv[i] = -1;
		}
	}
	Vector3 ctrlRot{};
	uint32_t rotOff = 0;
	const bool ctrlOk = playerController && Memory::IsValid(playerController) &&
	                    TryReadControlRotation(playerController, ctrlRot, rotOff);
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const CameraRef* spatial = ref.valid ? &ref : nullptr;
	const unsigned long long now = GetTickCount64();
	int fieldCount = 0;
	const SdkPovField* fields = SdkPovFields(fieldCount);
	int counts[5]{}; // read_failed, zero, template_pov, non_finite, subnormal
	for (int i = 0; i < fieldCount && scan.count < kMaxDocumentedCandidates; ++i) {
		CandidateRead& c = scan.reads[scan.count++];
		c.name = fields[i].name;
		c.offset = fields[i].offset;
		RawCameraCacheEntry raw{};
		c.readOk = Memory::Process.ReadRequestOk(pcm + fields[i].offset, raw);
		if (c.readOk) {
			scan.readOk++;
			c.location = Vector3(raw.location[0], raw.location[1], raw.location[2]);
			c.rotation = Vector3(raw.rotation[0], raw.rotation[1], raw.rotation[2]);
			c.fov = raw.fov;
			// FTViewTarget starts with an AActor*, not FCameraCacheEntry::Timestamp.
			c.hasTimestamp = std::strstr(c.name, "Cache") != nullptr;
			c.timestamp = raw.timestamp;
			if (c.hasTimestamp) {
				if (!s_refMs[i]) {
					s_refTs[i] = raw.timestamp;
					s_refMs[i] = now;
				} else if (now - s_refMs[i] >= 500) {
					s_adv[i] = raw.timestamp != s_refTs[i] ? 1 : 0;
					s_refTs[i] = raw.timestamp;
					s_refMs[i] = now;
				}
				c.timestampAdvanced = s_adv[i];
			}
		}
		ClassifyCandidate(c, spatial, sw, sh, ctrlRot, ctrlOk);
		if (c.selectable)
			scan.selectable++;
		static const char* const kEmpty[5] = { "read_failed", "zero", "template_pov", "non_finite",
		                                       "subnormal" };
		for (int k = 0; k < 5; ++k)
			if (std::strcmp(c.cls, kEmpty[k]) == 0)
				counts[k]++;
	}
	for (int pass = 0; pass < 3 && scan.best < 0; ++pass) {
		for (int i = 0; i < scan.count && scan.best < 0; ++i) {
			const char* cls = scan.reads[i].cls;
			const bool hit = pass == 0   ? std::strncmp(cls, "valid", 5) == 0
			                 : pass == 1 ? std::strcmp(cls, "rejected") == 0
			                             : std::strcmp(cls, "invalid") == 0;
			if (hit)
				scan.best = i;
		}
	}
	if (scan.selectable == 0) {
		static const char* const kLabels[5] = { "read_failed", "zero", "template", "non_finite",
		                                        "subnormal" };
		int n = std::snprintf(scan.noLiveReason, sizeof(scan.noLiveReason), "no_live_candidate");
		char sep = ':';
		for (int k = 0; k < 5 && n > 0 && n < static_cast<int>(sizeof(scan.noLiveReason)); ++k) {
			if (!counts[k])
				continue;
			n += std::snprintf(scan.noLiveReason + n, sizeof(scan.noLiveReason) - n, "%c%s=%d", sep,
			                   kLabels[k], counts[k]);
			sep = ',';
		}
	}
	LogCandidateScanIfChanged(scan, pcm, playerController, ref);
	return scan;
}

inline void Tick(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	if (!pcm || !Memory::IsValid(pcm))
		return;
	ResetAuthorityIfPcmChanged(pcm);
	g_tickSerial++;
	Vector3 ctrlRot{};
	uint32_t rotOff = 0;
	const bool ctrlOk =
	    playerController && Memory::IsValid(playerController) &&
	    TryReadControlRotation(playerController, ctrlRot, rotOff);
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const CameraRef* spatial = ref.valid ? &ref : nullptr;
	LogBootSdkSummary(pcm, spatial, sw, sh, ctrlRot, ctrlOk);

	static const uint32_t kPovDeltas[] = { PcmSdkLayout::kCacheEntryPov, 0x0u };
	int fieldCount = 0;
	const SdkPovField* fields = SdkPovFields(fieldCount);
	CandidateSample samples[kMaxCandidates]{};
	int sampleCount = 0;
	for (int fi = 0; fi < fieldCount && sampleCount < kMaxCandidates; ++fi) {
		for (uint32_t povDelta : kPovDeltas) {
			if (sampleCount >= kMaxCandidates)
				break;
			SampleCandidate(pcm, fields[fi].name, fields[fi].offset, povDelta,
			                samples[sampleCount], spatial, sw, sh, ctrlRot, ctrlOk);
			samples[sampleCount].key.object = pcm;
			sampleCount++;
		}
		if (sampleCount >= kMaxCandidates)
			continue;
		if (std::strcmp(fields[fi].name, "ViewTarget") == 0 ||
		    std::strcmp(fields[fi].name, "PendingViewTarget") == 0) {
			SampleCandidate(pcm, fields[fi].name, fields[fi].offset, 0x8u, samples[sampleCount],
			                spatial, sw, sh, ctrlRot, ctrlOk);
			samples[sampleCount].key.object = pcm;
			sampleCount++;
		}
	}
	static unsigned long long lastHeavyScanMs = 0;
	const unsigned long long scanNow = GetTickCount64();
	const bool heavyScan = (scanNow - lastHeavyScanMs) >= 400;
	if (heavyScan)
		lastHeavyScanMs = scanNow;
	if (heavyScan) {
		sampleCount = AppendMinimalViewInfoScan(pcm, "PCM", samples, sampleCount, spatial, sw, sh,
		                                        ctrlRot, ctrlOk);
		if (playerController && Memory::IsValid(playerController))
			sampleCount = AppendMinimalViewInfoScan(playerController, "PC", samples, sampleCount,
			                                        spatial, sw, sh, ctrlRot, ctrlOk);
		const uintptr_t localPlayer = LocalPtrs::LocalPlayers;
		if (localPlayer && Memory::IsValid(localPlayer))
			sampleCount = AppendMinimalViewInfoScan(localPlayer, "LP", samples, sampleCount, spatial,
			                                        sw, sh, ctrlRot, ctrlOk);
	}

	for (int i = 0; i < sampleCount; ++i) {
		CandidateTrack* track = AllocTrack(samples[i].key, samples[i].source);
		if (!track)
			continue;
		if (track->validStreak > 0)
			samples[i].movedSinceLast =
			    Dist3(samples[i].location, track->lastLoc) > 2.f ||
			    AbsYawDelta(samples[i].rotation.y, track->lastRot.y) > 0.15f;
		UpdateTrack(*track, samples[i], ctrlRot, ctrlOk);
	}

	if (g_hasConfirmed && Settings::DebugAtLeast(Settings::DebugVerbosity::Info)) {
		static std::atomic<bool> authorityLogged{ false };
		if (!authorityLogged.exchange(true)) {
			std::cout << xorstr_("[camera-diag] AUTHORITY src=") << g_confirmedSource
			          << xorstr_(" field+0x") << std::hex << g_confirmedKey.fieldOffset
			          << xorstr_(" pov+0x") << g_confirmedKey.povDelta << std::dec << '\n';
		}
	}
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace))
		return;
	static unsigned long long nextLogMs = 0;
	static char lastBestSource[56]{};
	const unsigned long long now = GetTickCount64();
	int bestIdx = -1;
	uint32_t bestScore = 0;
	for (int i = 0; i < sampleCount; ++i) {
		CandidateTrack* track = FindTrack(samples[i].key);
		const uint32_t score =
		    track ? (track->validStreak * 10u + track->motionEvents * 50u +
		             (samples[i].valid ? 5u : 0u))
		          : 0u;
		if (score > bestScore) {
			bestScore = score;
			bestIdx = i;
		}
	}
	const bool bestChanged =
	    bestIdx >= 0 && std::strcmp(lastBestSource, samples[bestIdx].source) != 0;
	if (!bestChanged && now < nextLogMs)
		return;
	nextLogMs = now + 8000;
	if (bestIdx >= 0) {
		std::strncpy(lastBestSource, samples[bestIdx].source, sizeof(lastBestSource) - 1);
		CandidateTrack* track = FindTrack(samples[bestIdx].key);
		LogCandidateLine(samples[bestIdx], track ? track->validStreak : 0,
		                 track ? track->motionEvents : 0);
	}
}

inline bool TryPublishConfirmed(uintptr_t /*pcm*/, uintptr_t playerController,
                                const CameraRef& ref) {
	if (!g_hasConfirmed)
		return false;
	const uintptr_t obj = g_confirmedKey.object;
	if (!obj || !Memory::IsValid(obj))
		return false;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const CameraRef* spatial = ref.valid ? &ref : nullptr;
	Vector3 location{}, rotation{};
	float fov = 0.f;
	float aspect = 0.f;
	ReadManagerPovAt(obj, g_confirmedKey.fieldOffset, g_confirmedKey.povDelta, location, rotation,
	                 fov, &aspect);
	if (IsDefaultTemplatePov(location, rotation, fov))
		return false;
	if (!CameraPovStrictAccept(location, rotation, fov, spatial))
		return false;
	const bool lobbyAnchor = spatial && spatial->pawn.y > 80000.f;
	const bool liveIslandPov = location.y < 80000.f && !(fov >= 89.5f && fov <= 90.5f);
	const bool carrierSky =
	    spatial && spatial->valid && IsCarrierSkyRigPov(*spatial, location);
	if (!(lobbyAnchor && liveIslandPov) && !carrierSky && playerController &&
	    Memory::IsValid(playerController) &&
	    !DocumentedPovRotationAcceptable(playerController, location, rotation, fov, aspect, spatial))
		return false;
	if (aspect < 0.5f || aspect > 3.f)
		aspect = (sw > 1.f && sh > 1.f) ? sw / sh : 0.f;
	const uint32_t povOff = g_confirmedKey.fieldOffset + g_confirmedKey.povDelta;
	ApplyCameraPov(location, rotation, fov, g_confirmedSource, povOff, aspect);
	return Camera::Valid;
}
} // namespace CameraDiagnostics

inline bool TryCalibratePcmSdkCache(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	if (g_PcmSdkCacheCalibration.valid)
		return true;
	if (!pcm || !Memory::IsValid(pcm) || !playerController || !Memory::IsValid(playerController))
		return false;
	Vector3 ctrl{};
	uint32_t rotOff = 0;
	if (!TryReadControlRotation(playerController, ctrl, rotOff))
		return false;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const CameraRef* spatialPtr = ref.valid ? &ref : nullptr;
	int fieldCount = 0;
	const SdkPovField* fields = SdkPovFields(fieldCount);
	for (int i = 0; i < fieldCount; ++i) {
		if (!fields[i].live)
			continue;
		Vector3 loc{}, rot{};
		float fov = 0.f;
		ReadManagerPovFromSdkCacheEntry(pcm, fields[i].offset, loc, rot, fov, nullptr);
		if (DiagnoseCameraPovRejectReason(loc, rot, fov, spatialPtr, sw, sh))
			continue;
		if (!ControlRotationNear(rot, ctrl))
			continue;
		g_PcmSdkCacheCalibration.valid = true;
		g_PcmSdkCacheCalibration.cacheEntryOffset = fields[i].offset;
		g_PcmSdkCacheCalibration.povDeltaFromEntry = PcmSdkLayout::kCacheEntryPov;
		g_PcmSdkCacheCalibration.sourceName = fields[i].name;
		return true;
	}
	return false;
}

// Documented PCM cache entries only (FCameraCacheEntry + FMinimalViewInfo layout).
inline bool TryReadSdkDocumentedCamera(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	(void)TryCalibratePcmSdkCache(pcm, playerController, ref);
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const CameraRef* spatialPtr = ref.valid ? &ref : nullptr;
	int fieldCount = 0;
	const SdkPovField* fields = SdkPovFields(fieldCount);
	auto tryPublishAt = [&](const SdkPovField& field, uint32_t povDelta) -> bool {
		Vector3 location{}, rotation{};
		float fov = 0.f;
		float aspect = 0.f;
		ReadManagerPovAt(pcm, field.offset, povDelta, location, rotation, fov, &aspect);
		if (IsDefaultTemplatePov(location, rotation, fov))
			return false;
		if (!CameraPovStrictAccept(location, rotation, fov, spatialPtr))
			return false;
		const bool lobbyAnchor = spatialPtr && spatialPtr->pawn.y > 80000.f;
		const bool liveIslandPov = location.y < 80000.f && !(fov >= 89.5f && fov <= 90.5f);
		const bool carrierSky =
		    spatialPtr && spatialPtr->valid && IsCarrierSkyRigPov(*spatialPtr, location);
		if (!(lobbyAnchor && liveIslandPov) && !carrierSky && playerController &&
		    Memory::IsValid(playerController) &&
		    !DocumentedPovRotationAcceptable(playerController, location, rotation, fov, aspect,
		                                   spatialPtr))
			return false;
		if (aspect < 0.5f || aspect > 3.f)
			aspect = (sw > 1.f && sh > 1.f) ? sw / sh : 0.f;
		const uint32_t povOff = field.offset + povDelta;
		ApplyCameraPov(location, rotation, fov, field.name, povOff, aspect);
		if (Camera::Valid) {
			CameraPovObject.store(pcm, std::memory_order_relaxed);
			CameraPovOffset.store(povOff, std::memory_order_relaxed);
			g_PcmSdkCacheCalibration.valid = true;
			g_PcmSdkCacheCalibration.cacheEntryOffset = field.offset;
			g_PcmSdkCacheCalibration.povDeltaFromEntry = povDelta;
			g_PcmSdkCacheCalibration.sourceName = field.name;
		}
		return Camera::Valid;
	};
	auto tryPublish = [&](const SdkPovField& field) -> bool {
		static const uint32_t kDeltas[] = { PcmSdkLayout::kCacheEntryPov, 0x8u, 0x0u };
		for (uint32_t povDelta : kDeltas) {
			if (tryPublishAt(field, povDelta))
				return true;
		}
		return false;
	};
	if (g_PcmSdkCacheCalibration.valid) {
		for (int i = 0; i < fieldCount; ++i) {
			if (fields[i].live && fields[i].offset == g_PcmSdkCacheCalibration.cacheEntryOffset)
				return tryPublish(fields[i]);
		}
	}
	const auto fieldIsLastFrame = [](const SdkPovField& field) {
		return std::strstr(field.name, "LastFrame") != nullptr;
	};
	if (spatialPtr && spatialPtr->valid && spatialPtr->pawn.y < 80000.f) {
		for (int i = 0; i < fieldCount; ++i) {
			if (!fields[i].live || !fieldIsLastFrame(fields[i]))
				continue;
			if (tryPublish(fields[i]))
				return true;
		}
	}
	for (int i = 0; i < fieldCount; ++i) {
		if (!fields[i].live)
			continue;
		if (tryPublish(fields[i]))
			return true;
	}
	return false;
}

inline bool TryLiveCameraCachePrivate(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	return TryReadSdkDocumentedCamera(pcm, playerController, ref);
}

inline bool TryFortniteControlRotationView(uintptr_t pcm, uintptr_t playerController,
                                           const CameraRef& ref);

inline bool RunOfflineRendererPipelineSelfTest(const char** outFail = nullptr) {
	auto fail = [&](const char* reason) {
		if (outFail)
			*outFail = reason;
		return false;
	};
	const uintptr_t pc = LocalPtrs::PlayerController;
	const uintptr_t pcm = LocalPtrs::PlayerCam;
	if (!pc || !Memory::IsValid(pc) || !pcm || !Memory::IsValid(pcm))
		return fail("invalid_pc_or_pcm");
	const EngineReferenceView engine = ReadEngineReferenceView(pc, pcm);
	if (!engine.controlRotationOk)
		return fail("engine_control_rotation_unreadable");
	CameraFrameSnapshot snap{};
	if (engine.cachePovOk) {
		snap.valid = true;
		snap.location = engine.location;
		snap.rotation = engine.rotation;
		snap.fov = engine.fov;
		snap.aspectRatio = engine.aspect;
		snap.source = engine.cacheSource;
	} else if (Camera::Valid) {
		snap.valid = true;
		snap.location = Camera::Location;
		snap.rotation = Camera::Rotation;
		snap.fov = Camera::FOV;
		snap.aspectRatio = Camera::AspectRatio;
		snap.source = Camera::Source;
	} else {
		return fail("camera_not_ready");
	}
	if (!GetRenderViewportRect(snap.viewRectMinX, snap.viewRectMinY, snap.viewRectWidth,
	                           snap.viewRectHeight))
		return fail("invalid_viewport");
	if (!BuildCameraViewProjection(snap.location, snap.rotation, snap.fov, snap.aspectRatio,
	                               snap.viewRectWidth, snap.viewRectHeight, &snap.viewMatrix,
	                               &snap.projectionMatrix, &snap.viewProjectionMatrix))
		return fail("view_projection_build_failed");
	snap.projectionValid = true;
	const D3DMATRIX axes = Matrix(snap.rotation);
	const Vector3 forward(axes.m[0][0], axes.m[0][1], axes.m[0][2]);
	const Vector3 world(snap.location.x + forward.x * 600.f, snap.location.y + forward.y * 600.f,
	                    snap.location.z + forward.z * 600.f);
	Vector3 screen{};
	if (!ProjectWorldWithViewProjection(snap.viewProjectionMatrix, snap.viewRectMinX,
	                                    snap.viewRectMinY, snap.viewRectWidth, snap.viewRectHeight,
	                                    world, &screen))
		return fail("projection_failed");
	static std::atomic<bool> loggedOk{ false };
	if (!loggedOk.exchange(true) && Settings::DebugAtLeast(Settings::DebugVerbosity::Info)) {
		std::cout << xorstr_("[renderer-test] engine cache POV projected world->screen ok at ")
		          << screen.x << ',' << screen.y << xorstr_(" source ")
		          << (engine.cacheSource ? engine.cacheSource : "?") << '\n';
	}
	return true;
}

inline bool TryScanPcmLivePov(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	if (!pcm || !Memory::IsValid(pcm) || !ref.valid || !playerController ||
	    !Memory::IsValid(playerController))
		return false;
	Vector3 ctrlRot{};
	uint32_t rotOff = 0;
	if (!TryReadControlRotation(playerController, ctrlRot, rotOff))
		return false;
	ScannedPov found{};
	if (!TryFindLivePovOnPcm(pcm, ctrlRot, ref, found) &&
	    !ScanObjectForPovNearPawn(pcm, ref, found) && !ScanObjectForPov(pcm, ref, found))
		return false;
	if (!CameraSeesPoint(found.location, found.rotation, ref.head))
		return false;
	CameraPovObject.store(pcm, std::memory_order_relaxed);
	CameraPovOffset.store(found.offset, std::memory_order_relaxed);
	ApplyCameraPov(found.location, found.rotation, found.fov, "PCMScanLive", found.offset);
	return true;
}

inline bool TryReadControlRotation(uintptr_t playerController, Vector3& rotation, uint32_t& outOffset) {
	if (!playerController || !Memory::IsValid(playerController))
		return false;
	const uint32_t off = static_cast<uint32_t>(Offsets::ControlRotation);
	const Vector3 r = Read<Vector3>(playerController + off);
	if (!ControlRotationTrustworthyForCompare(r))
		return false;
	rotation = r;
	outOffset = off;
	g_ControlRotationRefOffset = off;
	return true;
}

inline bool TrySynthesizeCameraFromController(uintptr_t playerController, uintptr_t pcm,
                                              const CameraRef& ref) {
	if (!ref.valid || !playerController || !Memory::IsValid(playerController))
		return false;
	Vector3 rotation{};
	uint32_t rotOffset = 0;
	if (!TryReadControlRotation(playerController, rotation, rotOffset))
		return false;

	Vector3 location{};
	float fov = 80.f;
	if (pcm && Memory::IsValid(pcm) &&
	    TryThirdPersonCameraFromPcm(pcm, rotation, ref, location, fov)) {
		ApplyCameraPov(location, rotation, fov, "FreeCamTPS", rotOffset);
		return true;
	}
	if (!TryGuessThirdPersonCameraLocation(ref.pawn, rotation, location))
		return false;
	fov = SanitizePcmFov(0.f, pcm);
	if (!CameraSeesPoint(location, rotation, ref.head))
		return false;
	ApplyCameraPov(location, rotation, fov, "ControlRotationTPS", rotOffset);
	return true;
}

inline void ClearCameraPovCache() {
	CameraPovObject.store(0, std::memory_order_relaxed);
	CameraPovOffset.store(0, std::memory_order_relaxed);
}

inline bool TryApplyPcmLocationWithControlRotation(uintptr_t pcm, uint32_t povOffset,
                                                   const Vector3& location, float fov,
                                                   const CameraRef& ref,
                                                   uintptr_t playerController) {
	if (!ref.valid || !pcm || !povOffset || !playerController ||
	    !Memory::IsValid(playerController))
		return false;
	Vector3 rotation{};
	uint32_t rotOffset = 0;
	if (!TryReadControlRotation(playerController, rotation, rotOffset))
		return false;
	if (!CameraPovUsable(location, fov))
		return false;
	if (!CameraSeesPoint(location, rotation, ref.head))
		return false;
	ApplyCameraPov(location, rotation, fov, "PCM+ControlRotation", povOffset);
	return true;
}

// Retrac often leaves PCM POV slots at the 0,0,0 / FOV 90 template; scan UWorld and the local
// PlayerController before falling back to the manager.
inline bool TryCachedPovFromObject(uintptr_t object, const char* objectLabel,
                                   uintptr_t pcmForNames, const CameraRef& ref,
                                   uintptr_t playerController) {
	if (!object || !Memory::IsValid(object))
		return false;
	const uintptr_t cachedObject = CameraPovObject.load(std::memory_order_relaxed);
	const uint32_t cachedOffset = CameraPovOffset.load(std::memory_order_relaxed);
	if (cachedObject != object || !cachedOffset)
		return false;
	Vector3 location{}, rotation{};
	float fov = 0.f;
	ReadPovAt(object, cachedOffset, location, rotation, fov);
	if (!PovBlockPlausible(location, rotation, fov) ||
	    (ref.valid && ScorePov(location, rotation, ref) < 0.f)) {
		if (pcmForNames && object == pcmForNames && ref.valid &&
		    TryApplyPcmLocationWithControlRotation(object, cachedOffset, location, fov, ref,
		                                           playerController))
			return true;
		ClearCameraPovCache();
		return false;
	}
	const char* named = (pcmForNames && object == pcmForNames)
	                        ? SdkNameForFovOffset(cachedOffset + 0x18)
	                        : nullptr;
	ApplyCameraPov(location, rotation, fov, named ? named : objectLabel, cachedOffset);
	return true;
}

inline bool ScanAndApplyPovFromObject(uintptr_t object, const char* objectLabel,
                                      uintptr_t pcmForNames, const CameraRef& ref,
                                      uintptr_t playerController) {
	if (!object || !Memory::IsValid(object) || !ref.valid)
		return false;
	ScannedPov found{};
	if (!ScanObjectForPov(object, ref, found)) {
		if (pcmForNames && object == pcmForNames && playerController) {
			Vector3 location{};
			float fov = 0.f;
			uint32_t locOffset = 0;
			Vector3 ctrlRot{};
			uint32_t rotOff = 0;
			if (playerController && TryReadControlRotation(playerController, ctrlRot, rotOff) &&
			    TryPickPcmViewForControlRotation(object, ctrlRot, ref, location, fov, locOffset)) {
				CameraPovObject.store(object, std::memory_order_relaxed);
				CameraPovOffset.store(locOffset, std::memory_order_relaxed);
				return TryApplyPcmLocationWithControlRotation(object, locOffset, location, fov, ref,
				                                              playerController);
			}
		}
		return false;
	}
	CameraPovObject.store(object, std::memory_order_relaxed);
	CameraPovOffset.store(found.offset, std::memory_order_relaxed);
	const char* named = (pcmForNames && object == pcmForNames)
	                        ? SdkNameForFovOffset(found.offset + 0x18)
	                        : nullptr;
	ApplyCameraPov(found.location, found.rotation, found.fov, named ? named : objectLabel,
	               found.offset);
	return true;
}

// Retrac 14.60: use a full FMinimalViewInfo (loc+rot+fov from the same POV). Mixing PCM location
// with ControlRotation projects ESP to the wrong place on screen.
inline bool TryReadRetracSdkCamera(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	(void)playerController;
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	int count = 0;
	const SdkPovField* fields = SdkPovFields(count);
	float bestScore = 1e9f;
	bool found = false;
	Vector3 bestLoc{}, bestRot{};
	float bestFov = 80.f;
	const char* bestName = nullptr;
	uint32_t bestFieldOff = 0;
	for (int i = 0; i < count; ++i) {
		Vector3 location{}, rotation{};
		float fov = 0.f;
		ReadManagerPov(pcm, fields[i].offset, location, rotation, fov);
		if (!PovBlockPlausible(location, rotation, fov))
			continue;
		const float score = ref.valid ? ScorePov(location, rotation, ref) : 0.f;
		if (ref.valid && score < 0.f)
			continue;
		const float pick = ref.valid ? score : Dist3(location, ref.pawn);
		if (found && pick >= bestScore)
			continue;
		found = true;
		bestScore = pick;
		bestLoc = location;
		bestRot = rotation;
		bestFov = fov;
		bestName = fields[i].name;
		bestFieldOff = fields[i].offset;
	}
	if (!found || !bestName)
		return false;
	ApplyCameraPov(bestLoc, bestRot, bestFov, bestName, bestFieldOff + 0x10);
	return true;
}

// Third-person: aim at LocationUnderReticle (crosshair), not the local head. Pair a PCM eye
// location with ControlRotation or a look-at-reticle rotation; validate reticle near screen center.
inline bool TryApplyTpsView(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref,
                            const Vector3& location, const Vector3& rotation, float fov,
                            uint32_t povOffset, uintptr_t cacheObject, const char* source) {
	if (!ref.valid || !CameraPovUsable(location, fov) || !RotatorHasAim(rotation))
		return false;
	if (!CameraSeesPoint(location, rotation, ref.head))
		return false;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	Vector3 reticle{};
	if (ReadPlayerReticleWorld(playerController, reticle)) {
		const float aspect = ReadPovAspectRatio(cacheObject ? cacheObject : pcm, povOffset);
		const float err =
		    WorldPointScreenCenterError(location, rotation, fov, aspect, reticle, sw, sh);
		if (err > sw * 0.55f)
			return false;
	}
	if (cacheObject && povOffset && cacheObject == pcm && povOffset >= 0x290u) {
		Vector3 verifyLoc{}, verifyRot{};
		float verifyFov = 0.f;
		ReadPovAt(cacheObject, povOffset, verifyLoc, verifyRot, verifyFov);
		if (PovBlockPlausible(verifyLoc, verifyRot, SanitizePcmFov(verifyFov, pcm))) {
			CameraPovObject.store(cacheObject, std::memory_order_relaxed);
			CameraPovOffset.store(povOffset, std::memory_order_relaxed);
		}
	}
	const float aspect = ReadPovAspectRatio(cacheObject ? cacheObject : pcm, povOffset);
	ApplyCameraPov(location, rotation, fov, source, povOffset, aspect);
	return true;
}

inline bool TryReadLiveSdkPovFields(uintptr_t pcm, const CameraRef& ref) {
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	int count = 0;
	const SdkPovField* fields = SdkPovFields(count);
	float bestPick = 1e9f;
	bool found = false;
	Vector3 bestLoc{}, bestRot{};
	float bestFov = 80.f;
	const char* bestName = nullptr;
	uint32_t bestPovOff = 0;
	for (int i = 0; i < count; ++i) {
		Vector3 location{}, rotation{};
		float fov = 0.f;
		ReadManagerPovAt(pcm, fields[i].offset, 0x10, location, rotation, fov);
		if (!PovBlockPlausible(location, rotation, fov))
			continue;
		if (ref.valid && Dist3(location, ref.pawn) > kCameraNearPawnMaxDistance)
			continue;
		if (ref.valid && !CameraSeesPoint(location, rotation, ref.head))
			continue;
		fov = SanitizePcmFov(fov, pcm);
		const float pick = ref.valid ? Dist3(location, ref.head) : 0.f;
		if (found && pick >= bestPick)
			continue;
		found = true;
		bestPick = pick;
		bestLoc = location;
		bestRot = rotation;
		bestFov = fov;
		bestName = fields[i].name;
		bestPovOff = fields[i].offset + 0x10;
	}
	if (!found || !bestName)
		return false;
	const float aspect = ReadPovAspectRatio(pcm, bestPovOff);
	const uintptr_t pc = LocalPtrs::PlayerController;
	if (pc && Memory::IsValid(pc) &&
	    !PovPassesReticleGate(bestLoc, bestRot, bestFov, aspect, pc, ref.valid ? &ref : nullptr))
		return false;
	CameraPovObject.store(pcm, std::memory_order_relaxed);
	CameraPovOffset.store(bestPovOff, std::memory_order_relaxed);
	ApplyCameraPov(bestLoc, bestRot, bestFov, bestName, bestPovOff, aspect);
	return true;
}

// Recomputed every frame from the current pawn + ControlRotation (never cached POV bytes).
inline bool TryBuildPerFrameFollowCamera(uintptr_t pcm, uintptr_t playerController,
                                         const CameraRef& ref) {
	if (!ref.valid || !playerController || !Memory::IsValid(playerController))
		return false;
	Vector3 ctrlRot{};
	uint32_t rotOff = 0;
	if (!TryReadControlRotation(playerController, ctrlRot, rotOff))
		return false;
	float fov = SanitizePcmFov(0.f, pcm);
	Vector3 loc{};
	const Vector3 orbitPivot = CameraOrbitPivot(ref);
	if (pcm && Memory::IsValid(pcm) &&
	    TryThirdPersonCameraFromPcm(pcm, ctrlRot, ref, loc, fov)) {
		// SDK FreeCamDistance / FreeCamOffset on PCM
	} else if (!TryGuessThirdPersonCameraLocation(orbitPivot, ctrlRot, loc)) {
		return false;
	}
	FinalizeSyntheticGameplayCamera(pcm, ref, loc, fov);
	if (!CameraSeesPoint(loc, ctrlRot, ref.head))
		return false;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const float aspect = OverlayAspectRatio(pcm);
	ClearCameraPovCache();
	if (!CameraPovStrictAccept(loc, ctrlRot, fov, &ref))
		return false;
	return TryApplyReticleGatedCameraPov(playerController, &ref, loc, ctrlRot, fov, aspect,
	                                     "FollowTPS", rotOff);
}

// Pre-match carrier: PCM cache *location* is the sky rig; rotation + FOV still match deck view.
inline bool TryReadCarrierPcmViewAngles(uintptr_t pcm, Vector3& outRot, float& outFov) {
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	Vector3 loc{}, rot{};
	float fov = 0.f;
	ReadManagerPovAt(pcm, static_cast<uint32_t>(Offsets::camera_cache_private), 0x10, loc, rot, fov,
	               nullptr);
	if (IsDefaultTemplatePov(loc, rot, fov))
		return false;
	if (!RotatorPlausible(rot) || !(fov >= 40.f && fov <= 120.f) || !(loc.y > 80000.f))
		return false;
	outRot = rot;
	outFov = fov;
	return true;
}

inline bool TryCarrierDeckGameplayCamera(uintptr_t pcm, uintptr_t playerController,
                                         const CameraRef& ref) {
	if (!ref.valid || ref.pawn.y <= 80000.f || !playerController ||
	    !Memory::IsValid(playerController))
		return false;
	Vector3 viewRot{};
	float fov = 0.f;
	uint32_t rotOff = static_cast<uint32_t>(Offsets::ControlRotation);
	Vector3 ctrlRot{};
	if (TryReadControlRotation(playerController, ctrlRot, rotOff)) {
		viewRot = ctrlRot;
		fov = SanitizePcmFov(0.f, pcm);
	} else if (!TryReadCarrierPcmViewAngles(pcm, viewRot, fov)) {
		return false;
	}
	Vector3 loc{};
	const Vector3 orbitPivot = CameraOrbitPivot(ref);
	uint32_t pcmEyeOff = 0;
	if (!TryThirdPersonCameraFromPcm(pcm, viewRot, ref, loc, fov) &&
	    !TryPickPcmViewForControlRotation(pcm, viewRot, ref, loc, fov, pcmEyeOff) &&
	    !TryGuessThirdPersonCameraLocation(orbitPivot, viewRot, loc))
		return false;
	FinalizeSyntheticGameplayCamera(pcm, ref, loc, fov);
	if (Dist3(loc, ref.pawn) > kGameplayEyeMaxDistFromPawn)
		return false;
	if (!CameraSeesPoint(loc, viewRot, ref.head))
		return false;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const float aspect = OverlayAspectRatio(pcm);
	if (!CameraPovStrictAccept(loc, viewRot, fov, &ref))
		return false;
	return TryApplyReticleGatedCameraPov(playerController, &ref, loc, viewRot, fov, aspect,
	                                     "FollowTPS", rotOff);
}

inline bool TryResolveTpsCamera(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	return false;
}

inline bool TryCachedCameraPov(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	const uintptr_t cachedObject = CameraPovObject.load(std::memory_order_relaxed);
	const uint32_t cachedOffset = CameraPovOffset.load(std::memory_order_relaxed);
	if (!cachedObject || !cachedOffset || !ref.valid)
		return false;
	Vector3 location{}, rotation{};
	float fov = 0.f;
	ReadPovAt(cachedObject, cachedOffset, location, rotation, fov);
	fov = SanitizePcmFov(fov, pcm);
	if (CameraLocationMaxAbs(location) <= 1000.f)
		return false;
	Vector3 ctrlRot{};
	uint32_t rotOff = 0;
	if (!TryReadControlRotation(playerController, ctrlRot, rotOff))
		return false;
	Vector3 reticle{};
	if (ReadPlayerReticleWorld(playerController, reticle)) {
		const Vector3 rotAim = CalcAngle(location, reticle);
		if (TryApplyTpsView(pcm, playerController, ref, location, rotAim, fov, cachedOffset,
		                    cachedObject, "Cached+Reticle"))
			return true;
	}
	if (PovBlockPlausible(location, rotation, fov) &&
	    TryApplyTpsView(pcm, playerController, ref, location, rotation, fov, cachedOffset,
	                    cachedObject, "CachedPOV"))
		return true;
	if (TryApplyTpsView(pcm, playerController, ref, location, ctrlRot, fov, cachedOffset,
	                    cachedObject, "Cached+CtrlRot"))
		return true;
	ClearCameraPovCache();
	return false;
}

// Cheap every-frame TPS: FreeCam eye + ControlRotation (Retrac match PCM POV is often stale).
inline bool TryFastTpsCamera(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	return false;
}

// Pick the TPS eye + rotation that puts LocationUnderReticle nearest screen center.
inline bool TryPickReticleTunedTpsCamera(uintptr_t pcm, uintptr_t playerController,
                                         const CameraRef& ref, bool allowPcmScan) {
	return false;
}

inline float ReadPlayFov(uintptr_t pcm) {
	if (g_LastLivePlayFov >= 40.f && g_LastLivePlayFov <= 120.f)
		return g_LastLivePlayFov;
	if (pcm && Memory::IsValid(pcm)) {
		const float fov = Read<float>(pcm + Offsets::DefaultFOV);
		if (fov >= 40.f && fov <= 120.f && !(fov >= 89.5f && fov <= 90.5f))
			return fov;
	}
	return 80.f;
}

inline bool PcmLiveFovScalarStrict(float fov) {
	if (!FloatUsableScalar(fov) || !(fov >= 40.f && fov <= 120.f))
		return false;
	// FCameraCacheEntry template POV uses FOV 90 at loc 0 — not a live player setting.
	if (fov >= 89.5f && fov <= 90.5f)
		return false;
	return true;
}

inline bool ReadPcmDefaultFovStrict(uintptr_t pcm, float& outFov) {
	outFov = 0.f;
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	static const uint32_t kFovFields[] = {
	    static_cast<uint32_t>(Offsets::DefaultFOV),
	};
	for (uint32_t off : kFovFields) {
		const float fov = Read<float>(pcm + off);
		if (PcmLiveFovScalarStrict(fov)) {
			outFov = fov;
			return true;
		}
	}
	return false;
}

// Retrac POV FOV matches vertical BuildUnrealReversedZPerspectiveMatrix in BuildCameraViewProjection.
inline float ResolveSyntheticGameplayFov(uintptr_t pcm, float cacheOrGuessFov) {
	float out = 0.f;
	if (cacheOrGuessFov >= 50.f && cacheOrGuessFov <= 120.f && !IsTemplateFovDegrees(cacheOrGuessFov))
		out = cacheOrGuessFov;
	else
		out = SanitizePcmFov(cacheOrGuessFov, pcm);
	if (IsTemplateFovDegrees(out))
		out = ReadPlayFov(pcm);
	return out;
}

inline void FinalizeSyntheticGameplayCamera(uintptr_t pcm, const CameraRef& ref, Vector3& loc,
                                            float& fov) {
	SanitizeThirdPersonEyeHeight(ref, loc);
	float targetDist = 360.f;
	if (pcm && Memory::IsValid(pcm)) {
		const float pcmDist = Read<float>(pcm + Offsets::FreeCamDistance);
		if (pcmDist >= 220.f && pcmDist <= 480.f)
			targetDist = pcmDist;
	}
	NudgeThirdPersonEyeDistanceFromPawn(ref, loc, targetDist);
	const float d = Dist3(loc, ref.pawn);
	if (d > kGameplayEyeMaxDistFromPawn && d > 1.f) {
		const float scale = kGameplayEyeMaxDistFromPawn / d;
		loc.x = ref.pawn.x + (loc.x - ref.pawn.x) * scale;
		loc.y = ref.pawn.y + (loc.y - ref.pawn.y) * scale;
		loc.z = ref.pawn.z + (loc.z - ref.pawn.z) * scale;
	}
	fov = ResolveSyntheticGameplayFov(pcm, fov);
}

inline bool TryPublishLivePcmScan(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	if (!ref.valid || !pcm || !Memory::IsValid(pcm) || !playerController ||
	    !Memory::IsValid(playerController))
		return false;
	Vector3 ctrlRot{};
	uint32_t rotOff = 0;
	if (TryReadControlRotation(playerController, ctrlRot, rotOff)) {
		ScannedPov live{};
		if (TryFindLivePovOnPcm(pcm, ctrlRot, ref, live)) {
			const float aspect = OverlayAspectRatio(pcm);
			char liveName[24];
			std::snprintf(liveName, sizeof(liveName), "Pcm.live+0x%X", live.offset);
			if (TryApplyEngineBundledPcmPov(playerController, &ref, live.location, live.rotation,
			                                live.fov, aspect, liveName, live.offset))
				return true;
		}
	}
	Vector3 scanLoc{}, scanRot{};
	float scanFov = 0.f;
	uint32_t scanOff = 0;
	if (TryScanLiveMinimalView(pcm, ref.pawn, scanLoc, scanRot, scanFov, scanOff)) {
		const float aspect = OverlayAspectRatio(pcm);
		char scanName[24];
		std::snprintf(scanName, sizeof(scanName), "Pcm.scan+0x%X", scanOff);
		if (TryApplyEngineBundledPcmPov(playerController, &ref, scanLoc, scanRot, scanFov, aspect,
		                                scanName, scanOff))
			return true;
	}
	return false;
}

inline bool TryPublishFreeCamTpsCamera(uintptr_t pcm, uintptr_t playerController,
                                     const CameraRef& ref) {
	if (!ref.valid || !pcm || !Memory::IsValid(pcm) || !playerController ||
	    !Memory::IsValid(playerController))
		return false;
	if (TryPublishLivePcmScan(pcm, playerController, ref))
		return true;
	const uint32_t rotOff = static_cast<uint32_t>(Offsets::ControlRotation);
	Vector3 rotation = Read<Vector3>(playerController + rotOff);
	if (!FiniteVec3(rotation) || !RotatorHasAim(rotation))
		return false;
	if (rotation.x > 89.f)
		rotation.x = 89.f;
	if (rotation.x < -89.f)
		rotation.x = -89.f;
	float fov = 0.f;
	Vector3 location{};
	if (!TryThirdPersonCameraFromPcm(pcm, rotation, ref, location, fov) &&
	    !ReadPcmDefaultFovStrict(pcm, fov))
		fov = ReadPlayFov(pcm);
	if (!FiniteVec3(location) || CameraLocationMaxAbs(location) <= 100.f) {
		const D3DMATRIX m = Matrix(rotation);
		const Vector3 forward(m.m[0][0], m.m[0][1], m.m[0][2]);
		float dist = Read<float>(pcm + Offsets::FreeCamDistance);
		if (!(dist >= 220.f && dist <= 480.f))
			dist = 360.f;
		Vector3 freeOff = Read<Vector3>(pcm + Offsets::FreeCamOffset);
		if (!FiniteVec3(freeOff) || CameraLocationMaxAbs(freeOff) > 400.f)
			freeOff = Vector3{};
		location = Vector3(ref.head.x - forward.x * dist + freeOff.x,
		                   ref.head.y - forward.y * dist + freeOff.y,
		                   ref.head.z - forward.z * dist + freeOff.z);
	}
	FinalizeSyntheticGameplayCamera(pcm, ref, location, fov);
	const CameraRef* spatialPtr = &ref;
	if (!CameraPovStrictAccept(location, rotation, fov, spatialPtr))
		return false;
	if (Dist3(location, ref.pawn) > kGameplayEyeMaxDistFromPawn)
		return false;
	const float aspect = OverlayAspectRatio(pcm);
	return TryApplyReticleGatedCameraPov(playerController, spatialPtr, location, rotation, fov,
	                                     aspect, "FreeCamTPS+Reticle", rotOff);
}

// Documented cache slots stay at the fov-90 template in a match. The live POV is still
// somewhere on the camera manager: location near the pawn, then rotator, then fov.
inline bool TryScanLiveMinimalView(uintptr_t pcm, const Vector3& pawn, Vector3& outLoc, Vector3& outRot,
                                   float& outFov, uint32_t& outOffset) {
	if (!pcm || !Memory::IsValid(pcm) || CameraLocationMaxAbs(pawn) < 100.f)
		return false;
	static std::vector<uint8_t> buffer(kPovScanBytes);
	const int read = ReadObjectBlock(pcm, buffer.data(), kPovScanBytes);
	float best = 1.e9f;
	bool found = false;
	for (int offset = 0; offset + 0x1C <= read; offset += 4) {
		float f[7];
		memcpy(f, buffer.data() + offset, sizeof(f));
		const Vector3 loc(f[0], f[1], f[2]);
		const Vector3 rot(f[3], f[4], f[5]);
		const float fov = f[6];
		if (!(fov >= 40.f && fov <= 120.f) || !FiniteVec3(loc) || !FiniteVec3(rot))
			continue;
		if (CameraLocationMaxAbs(loc) < 1000.f)
			continue;
		if (Absf(rot.x) > 89.5f)
			continue;
		const float dist = Dist3(loc, pawn);
		if (dist < 50.f || dist > 3500.f)
			continue;
		const float score = Absf(dist - 400.f);
		if (found && score >= best)
			continue;
		best = score;
		outLoc = loc;
		outRot = rot;
		outFov = fov;
		outOffset = static_cast<uint32_t>(offset);
		found = true;
	}
	return found;
}

// FQuat::Rotator. Pitch/yaw/roll match Matrix() (x/y/z).
inline Vector3 QuatToRotator(const FQuat& q) {
	constexpr float kRadToDeg = 57.2957795f;
	constexpr float kSingularity = 0.4999995f;
	const float singularity = q.z * q.x - q.w * q.y;
	const float yawY = 2.f * (q.w * q.z + q.x * q.y);
	const float yawX = 1.f - 2.f * (q.y * q.y + q.z * q.z);
	Vector3 rot{};
	if (singularity < -kSingularity) {
		rot.x = -90.f;
		rot.y = atan2f(yawY, yawX) * kRadToDeg;
	} else if (singularity > kSingularity) {
		rot.x = 90.f;
		rot.y = atan2f(yawY, yawX) * kRadToDeg;
	} else {
		rot.x = asinf(2.f * singularity) * kRadToDeg;
		rot.y = atan2f(yawY, yawX) * kRadToDeg;
		rot.z = atan2f(-2.f * (q.w * q.x + q.y * q.z), 1.f - 2.f * (q.x * q.x + q.y * q.y)) *
		        kRadToDeg;
	}
	return rot;
}

inline bool QuatToRotatorSelfCheck() {
	const FQuat identity{ 0.f, 0.f, 0.f, 1.f };
	const Vector3 zero = QuatToRotator(identity);
	const float s = 0.70710678f;
	const FQuat yaw90{ 0.f, 0.f, s, s };
	const Vector3 yaw = QuatToRotator(yaw90);
	return Absf(zero.x) < 0.05f && Absf(zero.y) < 0.05f && Absf(zero.z) < 0.05f &&
	       Absf(yaw.x) < 0.05f && Absf(yaw.y - 90.f) < 0.05f && Absf(yaw.z) < 0.05f;
}

inline float YawDeltaDegrees(float a, float b) {
	float d = a - b;
	while (d > 180.f)
		d -= 360.f;
	while (d < -180.f)
		d += 360.f;
	return d;
}

inline Vector3 TurnLookAround(Vector3 rotation) {
	rotation.x = -rotation.x;
	rotation.y += 180.f;
	if (rotation.y > 180.f)
		rotation.y -= 360.f;
	if (rotation.y < -180.f)
		rotation.y += 360.f;
	return rotation;
}

inline bool ReadSceneCameraPose(uintptr_t component, const CameraRef& ref, Vector3& outLoc,
                                Vector3& outRot) {
	if (!component || !Memory::IsValid(component))
		return false;
	FTransform c2w{};
	if (!Memory::Process.ReadRequestOk(component + Offsets::ComponentToWorld, c2w))
		return false;
	if (!QuatNormalized(c2w.rot) || !FiniteVec3(c2w.translation))
		return false;
	if (CameraLocationMaxAbs(c2w.translation) < 1000.f || c2w.translation.y > 80000.f)
		return false;
	if (ref.valid && Dist3(c2w.translation, ref.pawn) < 40.f)
		return false;
	const Vector3 rotation = QuatToRotator(c2w.rot);
	if (!RotatorPlausible(rotation) || !RotatorHasAim(rotation))
		return false;
	outLoc = c2w.translation;
	outRot = rotation;
	return true;
}

inline void PushSceneComponent(uintptr_t component, uintptr_t* out, int& count, int cap) {
	if (!component || !Memory::IsValid(component) || count >= cap)
		return;
	for (int i = 0; i < count; ++i) {
		if (out[i] == component)
			return;
	}
	out[count++] = component;
}

inline void PushAttachChildren(uintptr_t component, uintptr_t* out, int& count, int cap) {
	FUeTArrayHeader kids{};
	if (!ReadUeTArrayHeader(component + Offsets::children, kids) || kids.Num <= 0 || kids.Num > 8 ||
	    !kids.Data || !Memory::IsValid(kids.Data))
		return;
	for (int i = 0; i < kids.Num && count < cap; ++i) {
		const uintptr_t child =
		    Read<uintptr_t>(kids.Data + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
		PushSceneComponent(child, out, count, cap);
	}
}

// Named POV caches stay at the fov-90 template on the bus. The live camera is a scene
// transform. Keep the facing (raw or turned around) that puts the crosshair on the world.
inline bool TryPcmRootCamera(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	Vector3 reticle{};
	if (!ReadPlayerReticleWorld(playerController, reticle))
		return false;
	uintptr_t comps[24]{};
	int count = 0;
	const uintptr_t pcmRoot = Read<uintptr_t>(pcm + Offsets::RootComponent);
	PushSceneComponent(pcmRoot, comps, count, 24);
	PushAttachChildren(pcmRoot, comps, count, 24);
	const uintptr_t viewTarget = Read<uintptr_t>(pcm + Offsets::ViewTarget);
	if (viewTarget && Memory::IsValid(viewTarget)) {
		const uintptr_t viewRoot = Read<uintptr_t>(viewTarget + Offsets::RootComponent);
		PushSceneComponent(viewRoot, comps, count, 24);
		PushAttachChildren(viewRoot, comps, count, 24);
	}
	if (LocalPtrs::Player && Memory::IsValid(LocalPtrs::Player)) {
		const uintptr_t pawnRoot = Read<uintptr_t>(LocalPtrs::Player + Offsets::RootComponent);
		PushAttachChildren(pawnRoot, comps, count, 24);
	}
	const float aspect = OverlayAspectRatio(pcm);
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	float fov = 0.f;
	if (!ReadPcmDefaultFovStrict(pcm, fov))
		fov = ReadPlayFov(pcm);
	if (!(fov >= 40.f && fov <= 120.f))
		fov = 80.f;
	float bestErr = 1e9f;
	Vector3 bestLoc{};
	Vector3 bestRot{};
	int bestComp = -1;
	int posed = 0;
	for (int i = 0; i < count; ++i) {
		Vector3 loc{};
		Vector3 rot{};
		if (!ReadSceneCameraPose(comps[i], ref, loc, rot))
			continue;
		++posed;
		const Vector3 turns[2] = { rot, TurnLookAround(rot) };
		for (const Vector3& turn : turns) {
			const float err =
			    WorldPointScreenCenterError(loc, turn, fov, aspect, reticle, sw, sh);
			if (err < bestErr) {
				bestErr = err;
				bestLoc = loc;
				bestRot = turn;
				bestComp = i;
			}
		}
	}
	static int logs = 0;
	static unsigned long long nextLogMs = 0;
	const unsigned long long nowMs = GetTickCount64();
	if (logs < 12 && nowMs >= nextLogMs) {
		++logs;
		nextLogMs = nowMs + 2000;
		std::cout << "[camera-scene] comps=" << count << " posed=" << posed << " best=" << bestComp
		          << " err=" << bestErr << " loc " << bestLoc.x << ',' << bestLoc.y << ','
		          << bestLoc.z << " rot " << bestRot.x << ',' << bestRot.y << ',' << bestRot.z
		          << " reticle " << reticle.x << ',' << reticle.y << ',' << reticle.z << '\n';
	}
	if (bestComp < 0 || !(sw > 1.f) || bestErr > ReticleScreenCenterErrorThreshold(sw))
		return false;
	const CameraRef* spatialPtr = ref.valid ? &ref : nullptr;
	return TryApplyReticleGatedCameraPov(playerController, spatialPtr, bestLoc, bestRot, fov, aspect,
	                                     "PcmRoot", static_cast<uint32_t>(Offsets::ComponentToWorld));
}

// Named POV slots stay at the fov-90 template in a match. The live eye is still a world
// location on the manager, sitting behind the pawn along the crosshair.
inline bool TryCameraBehindLook(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	if (!pcm || !Memory::IsValid(pcm) || !ref.valid)
		return false;
	Vector3 reticle{};
	if (!ReadPlayerReticleWorld(playerController, reticle))
		return false;
	const Vector3 toAim = Vector3(reticle.x - ref.pawn.x, reticle.y - ref.pawn.y, reticle.z - ref.pawn.z);
	const float aimLen = sqrtf(toAim.x * toAim.x + toAim.y * toAim.y + toAim.z * toAim.z);
	if (aimLen < 200.f)
		return false;
	const Vector3 look(toAim.x / aimLen, toAim.y / aimLen, toAim.z / aimLen);
	static std::vector<uint8_t> buffer(kPovScanBytes);
	const int read = ReadObjectBlock(pcm, buffer.data(), static_cast<int>(buffer.size()));
	float best = -1.f;
	Vector3 bestLoc{};
	uint32_t bestOff = 0;
	bool found = false;
	for (int offset = 0; offset + 0xC <= read; offset += 4) {
		float f[3];
		memcpy(f, buffer.data() + offset, sizeof(f));
		const Vector3 loc(f[0], f[1], f[2]);
		if (!FiniteVec3(loc) || CameraLocationMaxAbs(loc) < 1000.f)
			continue;
		const float dist = Dist3(loc, ref.pawn);
		if (dist < 80.f || dist > 1200.f)
			continue;
		const Vector3 back(ref.pawn.x - loc.x, ref.pawn.y - loc.y, ref.pawn.z - loc.z);
		const float align = (back.x * look.x + back.y * look.y + back.z * look.z) / dist;
		if (align < 0.75f)
			continue;
		const float score = align * 1000.f - Absf(dist - 380.f);
		if (found && score <= best)
			continue;
		best = score;
		bestLoc = loc;
		bestOff = static_cast<uint32_t>(offset);
		found = true;
	}
	if (!found)
		return false;
	Vector3 rotation = CalcAngle(bestLoc, reticle);
	SanitizeControlRotation(rotation);
	if (!RotatorHasAim(rotation))
		return false;
	float fov = 0.f;
	if (!ReadPcmDefaultFovStrict(pcm, fov))
		fov = ReadPlayFov(pcm);
	if (!(fov >= 40.f && fov <= 120.f))
		fov = 80.f;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const float aspect = (sw > 1.f && sh > 1.f) ? sw / sh : 0.f;
	static char scanName[24];
	std::snprintf(scanName, sizeof(scanName), "Pcm.scan+0x%X", bestOff);
	const CameraRef* spatialPtr = ref.valid ? &ref : nullptr;
	return TryApplyReticleGatedCameraPov(playerController, spatialPtr, bestLoc, rotation, fov, aspect,
	                                     scanName, bestOff);
}

// When PCM cache POV is the 0/0/0 FOV-90 template, Fortnite still drives view from
// AController::ControlRotation + a third-person eye behind the pawn mesh (same frame).
inline bool TryFortniteControlRotationView(uintptr_t pcm, uintptr_t playerController,
                                           const CameraRef& ref) {
	if (!ref.valid || !playerController || !Memory::IsValid(playerController))
		return false;
	const uint32_t rotOff = static_cast<uint32_t>(Offsets::ControlRotation);
	Vector3 rotation = Read<Vector3>(playerController + rotOff);
	if (!FiniteVec3(rotation) || !RotatorHasAim(rotation))
		return false;
	SanitizeControlRotation(rotation);
	if (rotation.x > 89.f)
		rotation.x = 89.f;
	if (rotation.x < -89.f)
		rotation.x = -89.f;
	float fov = 0.f;
	Vector3 location{};
	const bool fromPcm = pcm && Memory::IsValid(pcm) &&
	                     TryThirdPersonCameraFromPcm(pcm, rotation, ref, location, fov);
	if (!fromPcm) {
		const D3DMATRIX m = Matrix(rotation);
		const Vector3 forward(m.m[0][0], m.m[0][1], m.m[0][2]);
		float dist = pcm && Memory::IsValid(pcm) ? Read<float>(pcm + Offsets::FreeCamDistance) : 0.f;
		if (!(dist >= 220.f && dist <= 480.f))
			dist = 360.f;
		Vector3 freeOff{};
		if (pcm && Memory::IsValid(pcm))
			freeOff = Read<Vector3>(pcm + Offsets::FreeCamOffset);
		if (!FiniteVec3(freeOff) || CameraLocationMaxAbs(freeOff) > 400.f)
			freeOff = Vector3{};
		const Vector3 pivot = CameraOrbitPivot(ref);
		location = Vector3(pivot.x - forward.x * dist + freeOff.x,
		                   pivot.y - forward.y * dist + freeOff.y,
		                   pivot.z - forward.z * dist + freeOff.z);
		if (!ReadPcmDefaultFovStrict(pcm, fov))
			fov = ReadPlayFov(pcm);
		FinalizeSyntheticGameplayCamera(pcm, ref, location, fov);
	}
	if (!FiniteVec3(location) || CameraLocationMaxAbs(location) <= 100.f)
		return false;
	if (!(fov >= 40.f && fov <= 120.f))
		return false;
	if (Dist3(location, ref.pawn) > kGameplayEyeMaxDistFromPawn)
		return false;
	const float aspect = OverlayAspectRatio(pcm);
	return TryApplyReticleGatedCameraPov(playerController, &ref, location, rotation, fov, aspect,
	                                     "FortniteControlView", rotOff);
}

inline bool IsRetracLobbySession();

inline bool RetracHelicarrierPcmCacheReadable(uintptr_t pcm) {
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	Vector3 location{};
	Vector3 rotation{};
	float fov = 0.f;
	ReadManagerPovFromSdkCacheEntry(pcm, static_cast<uint32_t>(Offsets::camera_cache_private),
	                                location, rotation, fov, nullptr);
	if (IsDefaultTemplatePov(location, rotation, fov))
		return false;
	if (!FiniteVec3(location) || !FiniteVec3(rotation) || !PovBlockPlausible(location, rotation, fov))
		return false;
	return location.y > 80000.f;
}

// Gameplay W2S must not treat the sky-rig cache *location* as the player eye.
inline bool RetracHelicarrierPcmPovLive(uintptr_t pcm) {
	if (!RetracHelicarrierPcmCacheReadable(pcm))
		return false;
	Vector3 location{};
	Vector3 rotation{};
	float fov = 0.f;
	ReadManagerPovFromSdkCacheEntry(pcm, static_cast<uint32_t>(Offsets::camera_cache_private),
	                                location, rotation, fov, nullptr);
	const CameraRef anchor = ReadCameraSpatialAnchor();
	if (!CameraPovStrictAccept(location, rotation, fov, anchor.valid ? &anchor : nullptr))
		return false;
	if (anchor.valid && Dist3(location, anchor.pawn) > kGameplayEyeMaxDistFromPawn)
		return false;
	return true;
}

// FCameraCacheEntry::POV is FMinimalViewInfo at +0x10 (Location, Rotation, FOV).
inline bool TryCameraCachePrivatePov(uintptr_t pcm, const CameraRef& ref) {
	if (!pcm || !Memory::IsValid(pcm))
		return false;
	Vector3 location{};
	Vector3 rotation{};
	float fov = 0.f;
	float aspect = 0.f;
	ReadManagerPovAt(pcm, static_cast<uint32_t>(Offsets::camera_cache_private), 0x10, location,
	                 rotation, fov, &aspect);
	if (!(fov >= 40.f && fov <= 120.f) || !FiniteVec3(location) || !FiniteVec3(rotation))
		return false;
	if (!RotatorPlausible(rotation) || !RotatorHasAim(rotation))
		return false;
	if (CameraLocationMaxAbs(location) <= 100.f)
		return false;
	const bool nearGameplayEye =
	    ref.valid && Dist3(location, ref.pawn) <= kGameplayEyeMaxDistFromPawn;
	const bool liveIsland =
	    ref.valid && ref.pawn.y > 80000.f && location.y < 80000.f &&
	    !(fov >= 89.5f && fov <= 90.5f) && RotatorHasAim(rotation);
	const bool carrierSkyRig = ref.valid && IsCarrierSkyRigPov(ref, location);
	if (!nearGameplayEye && !liveIsland && !carrierSkyRig)
		return false;
	const uint32_t pov = static_cast<uint32_t>(Offsets::camera_cache_private) + 0x10u;
	ApplyCameraPov(location, rotation, fov, "CameraCachePrivate", pov, aspect);
	return true;
}

// Cache POV is stale on this build (FOV ~1e-38). Eye stays 300 behind the head
// on ControlRotation forward. A reticle in front only replaces projection angles.
inline bool TryControlRotationCamera(uintptr_t pcm, uintptr_t playerController, const CameraRef& ref) {
	(void)ref;
	if (!playerController || !Memory::IsValid(playerController))
		return false;
	Vector3 pivot{}, head{};
	if (!ResolveCameraPivot(pivot, head))
		return false;
	const uint32_t rotOff = static_cast<uint32_t>(Offsets::ControlRotation);
	Vector3 rotation = Read<Vector3>(playerController + rotOff);
	if (!FiniteVec3(rotation) || !RotatorHasAim(rotation))
		return false;
	SanitizeControlRotation(rotation);
	const D3DMATRIX axes = Matrix(rotation);
	const Vector3 forward(axes.m[0][0], axes.m[0][1], axes.m[0][2]);
	constexpr float kArm = 300.f;
	const Vector3 eye(pivot.x - forward.x * kArm, pivot.y - forward.y * kArm,
	                  pivot.z - forward.z * kArm);
	const float fov = ReadPlayFov(pcm);
	if (!(fov >= 40.f && fov <= 120.f) || !FiniteVec3(eye))
		return false;
	float sw = 0.f;
	float sh = 0.f;
	GetOverlayViewportSize(sw, sh);
	const float aspect = (sw > 1.f && sh > 1.f) ? sw / sh : 0.f;
	const CameraRef anchor = ReadCameraSpatialAnchor();
	return TryApplyReticleGatedCameraPov(playerController, anchor.valid ? &anchor : nullptr, eye,
	                                     rotation, fov, aspect, "CtrlRot", rotOff);
}

// Lobby preview: eye behind head from ControlRotation only (no reticle aim snap).
inline bool TryControlRotationLobbyMinimal(uintptr_t pcm, uintptr_t playerController,
                                           CameraRef ref) {
	if (!IsRetracLobbySession() || !playerController || !Memory::IsValid(playerController))
		return false;
	if (!ref.valid) {
		const uintptr_t mesh = LocalPtrs::PlayerMesh;
		if (mesh && Memory::IsValid(mesh)) {
			const Vector3 c2w = GetMeshWorldLocation(mesh);
			if (FiniteVec3(c2w) && !BoneWorldMissing(c2w)) {
				ref.pawn = c2w;
				ref.head = c2w;
				ref.valid = true;
				const Vector3 head = GetBoneWithRotation(mesh, EBoneIndex::Head);
				if (FiniteVec3(head) && !BoneWorldMissing(head))
					ref.head = head;
			}
		}
	}
	if (!ref.valid)
		return false;
	const uint32_t rotOff = static_cast<uint32_t>(Offsets::ControlRotation);
	Vector3 rotation = Read<Vector3>(playerController + rotOff);
	if (!RotatorPlausible(rotation))
		return false;
	if (rotation.x > 89.f)
		rotation.x = 89.f;
	if (rotation.x < -89.f)
		rotation.x = -89.f;
	const D3DMATRIX axes = Matrix(rotation);
	const Vector3 forward(axes.m[0][0], axes.m[0][1], axes.m[0][2]);
	constexpr float kArm = 300.f;
	const Vector3 pivot = ref.pawn;
	const Vector3 eye(pivot.x - forward.x * kArm, pivot.y - forward.y * kArm,
	                  pivot.z - forward.z * kArm);
	const float fov = ReadPlayFov(pcm);
	if (!(fov >= 40.f && fov <= 120.f) || !FiniteVec3(eye))
		return false;
	ApplyCameraPov(eye, rotation, fov, "CtrlRot", rotOff, 0.f);
	return Camera::Valid;
}

inline char g_StickyW2SCameraSource[24] = {};

inline bool TryNamedCameraSource(const char* name, uintptr_t pcm, uintptr_t playerController,
                                 const CameraRef& ref, bool lobbyMinimal) {
	if (!name || !name[0])
		return false;
	if (std::strcmp(name, "CameraCachePrivate") == 0)
		return TryCameraCachePrivatePov(pcm, ref);
	if (std::strcmp(name, "CtrlRot") == 0)
		return TryControlRotationCamera(pcm, playerController, ref);
	if (lobbyMinimal && std::strcmp(name, "CtrlRotLobby") == 0)
		return TryControlRotationLobbyMinimal(pcm, playerController, ref);
	return false;
}

inline void RememberStickyCameraSource(const char* source) {
	if (!source || !source[0])
		return;
	std::strncpy(g_StickyW2SCameraSource, source, sizeof(g_StickyW2SCameraSource) - 1);
	g_StickyW2SCameraSource[sizeof(g_StickyW2SCameraSource) - 1] = '\0';
}

void GetCamera() {
	Camera::Valid = false;
	Camera::ViewProjectionReady = false;
	Camera::SourceOffset = 0;
	Camera::AspectRatio = 0.f;
	Camera::Source = "unusable";
	Camera::World = LocalPtrs::Gworld;

	uintptr_t playerController = LocalPtrs::PlayerController;
	uintptr_t pcm = LocalPtrs::PlayerCam;
	if ((!pcm || !Memory::IsValid(pcm)) && playerController && Memory::IsValid(playerController))
		pcm = Read<uintptr_t>(playerController + Offsets::playercameramanager);
	if (pcm && Memory::IsValid(pcm))
		LocalPtrs::PlayerCam = pcm;
	if (!pcm || !Memory::IsValid(pcm)) {
		Camera::Source = "no_camera_manager";
		RenderPipeline::SetCameraAcquireRejectReason("invalid_pointer");
		RenderPipeline::SetCameraLifecycle("camera_read", "none", "pcm_invalid");
		return;
	}
	if ((!LocalPtrs::PlayerMesh || !Memory::IsValid(LocalPtrs::PlayerMesh)) &&
	    LocalPtrs::Player && Memory::IsValid(LocalPtrs::Player))
		LocalPtrs::PlayerMesh = Read<uintptr_t>(LocalPtrs::Player + Offsets::Mesh);

	LogPcmStructureProbeOnce(pcm, playerController);
	const CameraRef ref = ReadCameraSpatialAnchor();
	const EngineReferenceView engineRef = ReadEngineReferenceView(playerController, pcm);
	LogEngineVsSdkCameraCompareOnce(engineRef, pcm);
	if (Settings::DebugAtLeast(Settings::DebugVerbosity::Trace) ||
	    !CameraDiagnostics::g_hasConfirmed)
		CameraDiagnostics::Tick(pcm, playerController, ref);
	const CameraDiagnostics::CandidateScan scan =
	    CameraDiagnostics::ScanDocumentedCandidates(pcm, playerController, ref);
	// Front-end / island: PCM cache location matches the visible view when not on the carrier.
	if (ref.valid && ref.pawn.y < 80000.f &&
	    (TryReadSdkDocumentedCamera(pcm, playerController, ref) ||
	     CameraDiagnostics::TryPublishConfirmed(pcm, playerController, ref))) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	// Pre-match carrier: sky-rig CameraCachePrivate is the real view (synthetic FollowTPS oversizes ESP).
	if (ref.valid && ref.pawn.y > 80000.f &&
	    (TryReadSdkDocumentedCamera(pcm, playerController, ref) ||
	     TryCameraCachePrivatePov(pcm, ref))) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	// Real in-match POV on PCM (bundled loc+rot+fov) before any synthetic eye behind the pawn.
	if (ref.valid && playerController && Memory::IsValid(playerController) &&
	    TryPublishLivePcmScan(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	// Carrier / island: documented cache is often a sky rig hundreds of uu from the deck pawn.
	if (ref.valid && playerController && Memory::IsValid(playerController) &&
	    TryBuildPerFrameFollowCamera(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	if (ref.valid && playerController && Memory::IsValid(playerController) &&
	    TryCarrierDeckGameplayCamera(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	if (TryReadSdkDocumentedCamera(pcm, playerController, ref) ||
	    CameraDiagnostics::TryPublishConfirmed(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	// Hold only SDK POV fields; rotation is refreshed from ControlRotation each frame.
	if (ref.valid && playerController && Memory::IsValid(playerController) &&
	    TryRestoreLastGoodCameraPov(playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	if (ref.valid && ref.pawn.y < 80000.f && playerController && Memory::IsValid(playerController) &&
	    TryBuildPerFrameFollowCamera(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	if (ref.valid && TryPublishFreeCamTpsCamera(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	if (ref.valid && playerController && Memory::IsValid(playerController) &&
	    TryFortniteControlRotationView(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	if (TryReadLiveSdkPovFields(pcm, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	if (TryCameraBehindLook(pcm, playerController, ref) ||
	    TryPcmRootCamera(pcm, playerController, ref) ||
	    TryControlRotationCamera(pcm, playerController, ref) ||
	    TryControlRotationLobbyMinimal(pcm, playerController, ref)) {
		RenderPipeline::SetCameraAcquireRejectReason("acquired");
		RenderPipeline::SetCameraLifecycle("snapshot_committed", Camera::Source, "pending");
		return;
	}
	Camera::Source = "pov_unusable";
	const char* stage = "candidate_validated";
	const char* source = "none";
	const char* reason = "validator_rejected";
	if (scan.readOk == 0) {
		stage = "camera_read";
		reason = "read_failed";
	} else if (scan.best < 0) {
		stage = "candidate_selected";
		reason = scan.noLiveReason;
	} else {
		source = scan.reads[scan.best].name;
		if (std::strcmp(scan.reads[scan.best].reason, "ok") != 0)
			reason = scan.reads[scan.best].reason;
	}
	RenderPipeline::SetCameraAcquireRejectReason(reason);
	RenderPipeline::SetCameraLifecycle(stage, source, reason);
}

inline void LogW2STemporalTrace(const Vector3& world, const CameraFrameSnapshot& snap,
                                  const Vector3& screen, bool projected) {
	if (!Settings::DebugAtLeast(Settings::DebugVerbosity::Trace))
		return;
	static std::atomic<unsigned long long> nextLogMs{ 0 };
	const unsigned long long now = GetTickCount64();
	if (now < nextLogMs.load(std::memory_order_relaxed))
		return;
	nextLogMs.store(now + 250, std::memory_order_relaxed);
	float clip[4]{};
	if (snap.projectionValid)
		UnrealTransformFVector4(snap.viewProjectionMatrix, world.x, world.y, world.z, 1.f, clip);
	const float ndcX = clip[3] > 1e-4f ? clip[0] / clip[3] : 0.f;
	const float ndcY = clip[3] > 1e-4f ? clip[1] / clip[3] : 0.f;
	std::ostringstream line;
	line << "W2S trace f" << snap.frameSerial << " world " << world.x << ',' << world.y << ','
	     << world.z << " cam " << snap.location.x << ',' << snap.location.y << ','
	     << snap.location.z << " rot " << snap.rotation.x << ',' << snap.rotation.y << ','
	     << snap.rotation.z << " vp[" << snap.viewProjectionMatrix.m[0][0] << ','
	     << snap.viewProjectionMatrix.m[1][1] << ',' << snap.viewProjectionMatrix.m[3][2]
	     << "] clip " << clip[0] << ',' << clip[1] << ',' << clip[2] << " w " << clip[3]
	     << " ndc " << ndcX << ',' << ndcY << " screen " << (projected ? screen.x : 0.f) << ','
	     << (projected ? screen.y : 0.f) << " ok " << (projected ? 1 : 0);
	std::cout << line.str() << '\n';
}

inline void DrawW2SDebugVisualization() {
	if (!Settings::W2SDebugDraw)
		return;
	CameraFrameSnapshot snap{};
	{
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		snap = g_W2SCameraSnapshot;
	}
	if (!snap.projectionValid)
		return;

	static Vector3 anchorWorld{};
	static bool anchorSet = false;
	static Vector3 lastPelvis{};
	const uintptr_t pawn = LocalPtrs::Player;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	const auto drawTarget = [&](const Vector3& world, ImU32 color, const char* label) {
		Vector3 screen{};
		if (!ProjectWorldWithViewProjection(snap.viewProjectionMatrix, snap.viewRectMinX,
		                                    snap.viewRectMinY, snap.viewRectWidth,
		                                    snap.viewRectHeight, world, &screen))
			return;
		dl->AddCircleFilled(ImVec2(screen.x, screen.y), 6.f, color);
		dl->AddCircle(ImVec2(screen.x, screen.y), 10.f, IM_COL32(255, 255, 255, 220));
		if (label)
			dl->AddText(ImVec2(screen.x + 12.f, screen.y - 8.f), color, label);
	};

	if (pawn && Memory::IsValid(pawn)) {
		const uintptr_t dbgMesh = Read<uintptr_t>(pawn + Offsets::Mesh);
		if (dbgMesh && Memory::IsValid(dbgMesh)) {
			drawTarget(GetMeshWorldLocation(dbgMesh), IM_COL32(255, 180, 80, 255), "mesh");
			Vector3 pelvis = GetBoneWithRotation(dbgMesh, EBoneIndex::Pelvis);
			if (FiniteVec3(pelvis) && !BoneWorldMissing(pelvis)) {
				lastPelvis = pelvis;
				if (!anchorSet) {
					anchorWorld = pelvis;
					anchorSet = true;
				}
				drawTarget(pelvis, IM_COL32(80, 200, 255, 255), "pelvis");
			}
			drawTarget(GetBoneWithRotation(dbgMesh, EBoneIndex::Spine_03),
			           IM_COL32(180, 255, 120, 255), "spine");
			drawTarget(GetBoneWithRotation(dbgMesh, EBoneIndex::Head), IM_COL32(255, 120, 120, 255),
			           "head");
			drawTarget(GetBoneWithRotation(dbgMesh, EBoneIndex::Hand_L),
			           IM_COL32(120, 180, 255, 255), "L_hand");
			drawTarget(GetBoneWithRotation(dbgMesh, EBoneIndex::Hand_R),
			           IM_COL32(120, 180, 255, 255), "R_hand");
			drawTarget(GetBoneWithRotation(dbgMesh, EBoneIndex::Foot_L),
			           IM_COL32(200, 140, 255, 255), "L_foot");
			drawTarget(GetBoneWithRotation(dbgMesh, EBoneIndex::Foot_R),
			           IM_COL32(200, 140, 255, 255), "R_foot");
		}
	}

	if (anchorSet)
		drawTarget(anchorWorld, IM_COL32(255, 220, 80, 255), "anchor");

	if (Settings::DebugAtLeast(Settings::DebugVerbosity::Trace)) {
		if (anchorSet) {
			Vector3 sa{};
			const bool okA = ProjectWorldWithViewProjection(
			    snap.viewProjectionMatrix, snap.viewRectMinX, snap.viewRectMinY, snap.viewRectWidth,
			    snap.viewRectHeight, anchorWorld, &sa);
			LogW2STemporalTrace(anchorWorld, snap, sa, okA);
		}
		if (FiniteVec3(lastPelvis) && !BoneWorldMissing(lastPelvis)) {
			Vector3 sp{};
			const bool okP = ProjectWorldWithViewProjection(
			    snap.viewProjectionMatrix, snap.viewRectMinX, snap.viewRectMinY, snap.viewRectWidth,
			    snap.viewRectHeight, lastPelvis, &sp);
			LogW2STemporalTrace(lastPelvis, snap, sp, okP);
		}
	}
}

inline bool ProjectWorldToScreenWithReason(Vector3 WorldLocation, Vector3* ScreenLocation, const char** outReason)
{
	auto fail = [&](const char* reason) -> bool {
		if (outReason)
			*outReason = reason;
		if (ScreenLocation) {
			ScreenLocation->x = 0.f;
			ScreenLocation->y = 0.f;
			ScreenLocation->z = 0.f;
		}
		return false;
	};
	if (!ScreenLocation)
		return fail("no_out");
	ScreenLocation->x = 0.f;
	ScreenLocation->y = 0.f;
	ScreenLocation->z = 0.f;
	if (BoneWorldMissing(WorldLocation))
		return fail("world_zero");
	RenderPipeline::g_FrameStats.worldPoints.fetch_add(1, std::memory_order_relaxed);

	static thread_local unsigned tlsW2SFrame = UINT_MAX;
	static thread_local CameraFrameSnapshot tlsW2SSnap{};
	const unsigned w2sFrame = EspFrameCounter.load(std::memory_order_relaxed);
	if (w2sFrame != tlsW2SFrame) {
		tlsW2SFrame = w2sFrame;
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		tlsW2SSnap = g_W2SCameraSnapshot;
	}
	const CameraFrameSnapshot& snap = tlsW2SSnap;
	if (!snap.valid)
		return fail("camera_not_ready");
	if (!snap.projectionValid) {
		const char* diag = DiagnoseCameraSnapshotRejectReason(snap);
		return fail(diag ? diag : "view_projection");
	}
	if (snap.viewRectWidth < 1.f || snap.viewRectHeight < 1.f)
		return fail("viewport_size");
	if (!WorldPointVisibleInCameraFrustum(snap.viewProjectionMatrix, WorldLocation))
		return fail("off_screen");
	float clip[4]{};
	UnrealTransformFVector4(snap.viewProjectionMatrix, WorldLocation.x, WorldLocation.y, WorldLocation.z,
	                        1.f, clip);
	if (!std::isfinite(clip[3]) || clip[3] <= 1e-4f)
		return fail("behind_camera");
	const float ndcX = clip[0] / clip[3];
	const float ndcY = clip[1] / clip[3];
	const float sw = snap.viewRectWidth;
	const float sh = snap.viewRectHeight;
	ScreenLocation->x = snap.viewRectMinX + (0.5f + ndcX * 0.5f) * sw;
	ScreenLocation->y = snap.viewRectMinY + (0.5f - ndcY * 0.5f) * sh;
	ScreenLocation->z = 0.f;
	RenderPipeline::g_FrameStats.projectedPoints.fetch_add(1, std::memory_order_relaxed);
	RenderPipeline::g_FrameStats.visiblePoints.fetch_add(1, std::memory_order_relaxed);
	if (outReason)
		*outReason = "ok";
	return true;
}

bool ProjectWorldToScreen(Vector3 WorldLocation, Vector3* ScreenLocation)
{
	return ProjectWorldToScreenWithReason(WorldLocation, ScreenLocation, nullptr);
}

inline bool BuildEspScreenBoundsFromSnapshot(const BoneFrameSnapshot& snap, EspScreenBounds& out) {
	out = {};
	if (!snap.acquired || !RenderPipeline::BoneVerdictConsumable(snap.verdict))
		return false;

	float minX = 1e9f;
	float maxX = -1e9f;
	float minY = 1e9f;
	float maxY = -1e9f;
	int projected = 0;
	const float visualScale = ResolveStableEspVisualScale(snap);
	static const int kBoxBones[] = {
	    EBoneIndex::Head,       EBoneIndex::Pelvis,     EBoneIndex::Hand_L,
	    EBoneIndex::Hand_R,     EBoneIndex::UpperArm_L, EBoneIndex::UpperArm_R,
	    EBoneIndex::LowerArm_L, EBoneIndex::LowerArm_R, EBoneIndex::Foot_L,
	    EBoneIndex::Foot_R,
	};
	for (int boneId : kBoxBones) {
		if (boneId < 0 || boneId >= snap.pose.Num ||
		    static_cast<size_t>(boneId) >= snap.bones.size())
			continue;
		const Vector3 world = BoneWorldVisualFromSnapshot(snap, boneId, visualScale);
		if (BoneWorldMissing(world))
			continue;
		Vector3 screen{};
		if (!ProjectWorldToScreen(world, &screen))
			continue;
		++projected;
		if (screen.x < minX)
			minX = screen.x;
		if (screen.x > maxX)
			maxX = screen.x;
		if (screen.y < minY)
			minY = screen.y;
		if (screen.y > maxY)
			maxY = screen.y;
	}
	if (projected < 2 || maxY <= minY)
		return false;
	const float rawHeight = maxY - minY;
	const float rawWidth = maxX - minX;
	const float padTop = rawHeight * 0.22f + 12.f;
	const float padBottom = rawHeight * 0.12f + 8.f;
	const float padSide = rawWidth * 0.18f + 10.f;
	out.height = rawHeight + padTop + padBottom;
	out.width = rawWidth + padSide * 2.f;
	if (out.height > 8.f && out.width < out.height * 0.42f)
		out.width = out.height * 0.42f;
	out.centerX = (minX + maxX) * 0.5f;
	out.topY = minY - padTop;
	out.bottomY = out.topY + out.height;
	out.ok = true;
	return true;
}

// An invalid camera must project nothing, and a point that lands far outside the viewport must
// be rejected instead of drawn as a line running off to nowhere. Anything in front still passes.
inline bool ProjectionFailsClosedSelfCheck(const char** outFailedInvariant = nullptr) {
	auto fail = [&](const char* reason) -> bool {
		if (outFailedInvariant)
			*outFailedInvariant = reason;
		return false;
	};

	CameraFrameSnapshot savedSnap{};
	{
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		savedSnap = g_W2SCameraSnapshot;
	}
	CameraFrameSnapshot empty{};
	{
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		g_W2SCameraSnapshot = empty;
	}
	Vector3 screen{};
	if (ProjectWorldToScreen(Vector3(100.f, 200.f, 300.f), &screen)) {
		{
			std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
			g_W2SCameraSnapshot = savedSnap;
		}
		return fail("invalid_snapshot_did_not_block_w2s");
	}

	CameraFrameSnapshot test{};
	test.valid = true;
	test.location = Vector3(52.f, 125913.f, 89.f);
	test.rotation = Vector3(-8.f, 42.f, 0.f);
	test.fov = 80.f;
	test.aspectRatio = 16.f / 9.f;
	test.viewRectMinX = 0.f;
	test.viewRectMinY = 0.f;
	test.viewRectWidth = 1920.f;
	test.viewRectHeight = 1080.f;
	if (!BuildCameraViewProjection(test.location, test.rotation, test.fov, test.aspectRatio,
	                               test.viewRectWidth, test.viewRectHeight, &test.viewMatrix,
	                               &test.projectionMatrix, &test.viewProjectionMatrix))
		return fail("self_check_test_matrix_build");
	test.projectionValid = true;
	{
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		g_W2SCameraSnapshot = test;
	}
	const D3DMATRIX selfAxes = Matrix(test.rotation);
	const Vector3 selfForward(selfAxes.m[0][0], selfAxes.m[0][1], selfAxes.m[0][2]);
	const Vector3 inFrontPoint(
	    test.location.x + selfForward.x * 500.f, test.location.y + selfForward.y * 500.f,
	    test.location.z + selfForward.z * 500.f);
	// Self-check validates view*proj math; production W2S adds a frustum gate on top.
	const bool passesInFront = ProjectWorldWithViewProjection(
	    test.viewProjectionMatrix, test.viewRectMinX, test.viewRectMinY, test.viewRectWidth,
	    test.viewRectHeight, inFrontPoint, &screen);
	const D3DMATRIX ax = Matrix(test.rotation);
	const Vector3 right(ax.m[1][0], ax.m[1][1], ax.m[1][2]);
	const Vector3 farLateral(test.location.x + right.x * 500000.f,
	                         test.location.y + right.y * 500000.f,
	                         test.location.z + right.z * 500000.f);
	const bool blockedWhenOffScreen =
	    !WorldPointVisibleInCameraFrustum(test.viewProjectionMatrix, farLateral) &&
	    !ProjectWorldToScreen(farLateral, &screen);

	{
		std::lock_guard<std::mutex> lock(g_W2SCameraSnapshotMutex);
		g_W2SCameraSnapshot = savedSnap;
	}

	if (!passesInFront)
		return fail("in_front_point_not_projected");
	if (!blockedWhenOffScreen)
		return fail("off_screen_point_not_rejected");
	return true;
}

inline bool RunCameraProjectionFrustumSelfTest(const char** outFail = nullptr) {
	auto fail = [&](const char* reason) {
		if (outFail)
			*outFail = reason;
		return false;
	};
	const Vector3 camLoc(0.f, 0.f, 100.f);
	const Vector3 camRot(0.f, 0.f, 0.f);
	const float fov = 90.f;
	const float aspect = 16.f / 9.f;
	const float w = 2560.f;
	const float h = 1440.f;
	D3DMATRIX vp{};
	if (!BuildCameraViewProjection(camLoc, camRot, fov, aspect, w, h, nullptr, nullptr, &vp))
		return fail("matrix_build");
	auto project = [&](const Vector3& world, Vector3& screen, float& ndcX, float& ndcY) -> bool {
		float clip[4]{};
		UnrealTransformFVector4(vp, world.x, world.y, world.z, 1.f, clip);
		if (clip[3] <= 1e-4f)
			return false;
		ndcX = clip[0] / clip[3];
		ndcY = clip[1] / clip[3];
		screen.x = (0.5f + ndcX * 0.5f) * w;
		screen.y = (0.5f - ndcY * 0.5f) * h;
		return true;
	};
	Vector3 screen{};
	float ndcX = 0.f;
	float ndcY = 0.f;
	if (!project(Vector3(1000.f, 0.f, 100.f), screen, ndcX, ndcY))
		return fail("center_in_front");
	if (Absf(screen.x - w * 0.5f) > w * 0.08f || Absf(screen.y - h * 0.5f) > h * 0.08f)
		return fail("center_not_at_viewport_center");
	Vector3 leftScreen{}, rightScreen{}, upScreen{}, downScreen{};
	float lx = 0.f, rx = 0.f, uy = 0.f, dy = 0.f;
	if (!project(Vector3(1000.f, -500.f, 100.f), leftScreen, lx, ndcY) ||
	    !project(Vector3(1000.f, 500.f, 100.f), rightScreen, rx, ndcY))
		return fail("lateral_points");
	if (!(leftScreen.x < w * 0.5f && rightScreen.x > w * 0.5f))
		return fail("lateral_direction");
	if (!project(Vector3(1000.f, 0.f, 600.f), upScreen, ndcX, uy) ||
	    !project(Vector3(1000.f, 0.f, -400.f), downScreen, ndcX, dy))
		return fail("vertical_points");
	if (!(upScreen.y < h * 0.5f && downScreen.y > h * 0.5f))
		return fail("vertical_direction");
	float clipBehind[4]{};
	UnrealTransformFVector4(vp, -1000.f, 0.f, 100.f, 1.f, clipBehind);
	if (clipBehind[3] > 1e-4f)
		return fail("behind_point_not_rejected");
	const D3DMATRIX ax = Matrix(camRot);
	const Vector3 right(ax.m[1][0], ax.m[1][1], ax.m[1][2]);
	const Vector3 farLateral(camLoc.x + right.x * 500000.f, camLoc.y + right.y * 500000.f,
	                         camLoc.z + right.z * 500000.f);
	if (WorldPointVisibleInCameraFrustum(vp, farLateral))
		return fail("off_frustum_not_rejected");
	return true;
}

inline void DrawRenderPipelineSanityTest(const CameraSnapshot& snap) {
	if (!snap.projectionValid)
		return;
	const D3DMATRIX axes = Matrix(snap.rotation);
	const Vector3 forward(axes.m[0][0], axes.m[0][1], axes.m[0][2]);
	const Vector3 world(snap.location.x + forward.x * 600.f, snap.location.y + forward.y * 600.f,
	                    snap.location.z + forward.z * 600.f);
	RenderPipeline::g_FrameStats.worldPoints.fetch_add(1, std::memory_order_relaxed);
	Vector3 screen{};
	if (!ProjectWorldWithViewProjection(snap.viewProjectionMatrix, snap.viewRectMinX,
	                                    snap.viewRectMinY, snap.viewRectWidth, snap.viewRectHeight,
	                                    world, &screen))
		return;
	RenderPipeline::g_FrameStats.projectedPoints.fetch_add(1, std::memory_order_relaxed);
	RenderPipeline::g_FrameStats.visiblePoints.fetch_add(1, std::memory_order_relaxed);
	if (!ImGui::GetCurrentContext())
		return;
	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl)
		return;
	RenderPipeline::g_FrameStats.queuedPrimitives.fetch_add(1, std::memory_order_relaxed);
	dl->AddCircleFilled(ImVec2(screen.x, screen.y), 8.f, IM_COL32(120, 255, 120, 255));
	RenderPipeline::g_FrameStats.submittedPrimitives.fetch_add(1, std::memory_order_relaxed);
}

inline int CountSubmittedSkeleton(uintptr_t mesh) {
	if (!Settings::Skeleton || !mesh || !Memory::IsValid(mesh))
		return 0;
	if (Vec3Distance(Camera::Location, GetMeshWorldLocation(mesh)) / 100.f > 1260.f)
		return 0;
	std::vector<std::pair<Vector3, Vector3>> lines;
	CollectSkeletonWorldLines(mesh, lines);
	int submitted = 0;
	for (const auto& line : lines) {
		Vector3 sa, sb;
		if (ProjectWorldToScreen(line.first, &sa) && ProjectWorldToScreen(line.second, &sb))
			++submitted;
	}
	return submitted;
}

inline bool IsInScreen(Vector3 WorldLocation)
{
    Vector3 ScreenLocation;
    return ProjectWorldToScreen(WorldLocation, &ScreenLocation);
}

inline bool PlayerDisplayNamePlausible(const std::string& s) {
	if (s.size() < 3 || s.size() > 24)
		return false;
	if (s == "Unknown" || s == "unknown")
		return false;
	int meaningful = 0;
	for (unsigned char c : s) {
		if (c < 32 || c == 127)
			return false;
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' ||
		    c == '_' || c == '-' || c == '.')
			++meaningful;
	}
	return meaningful >= 3 && meaningful * 2 >= static_cast<int>(s.size());
}

inline std::string TryReadPlayerNameXorDecrypt(uintptr_t fStringObj) {
	if (!fStringObj || !Memory::IsValid(fStringObj))
		return {};
	const int iLength = Read<int>(fStringObj + 16);
	if (iLength <= 0 || iLength > 64)
		return {};
	const auto v6 = static_cast<__int64>(iLength);
	const uintptr_t fText = Read<uintptr_t>(fStringObj + 8);
	if (!fText || !Memory::IsValid(fText))
		return {};
	std::vector<wchar_t> wcBuffer(static_cast<size_t>(iLength) + 1, L'\0');
	Memory::ReadVirtual(PVOID(fText), wcBuffer.data(), iLength * sizeof(wchar_t));
	char v21 = static_cast<char>(v6 - 1);
	if (!(DWORD)v6)
		v21 = 0;
	int v22 = 0;
	WORD* v23 = reinterpret_cast<WORD*>(wcBuffer.data());
	int i = (v21) & 3;
	for (;;) {
		int v25 = v6 - 1;
		if (!(DWORD)v6)
			v25 = 0;
		if (v22 >= v25)
			break;
		*v23++ += i & 7;
		i += 3;
		++v22;
	}
	std::wstring wsUsername(wcBuffer.data(), static_cast<size_t>(iLength));
	return std::string(wsUsername.begin(), wsUsername.end());
}

inline std::string TryReadPlayerNamePlainFStringObj(uintptr_t fStringObj) {
	if (!fStringObj || !Memory::IsValid(fStringObj))
		return {};
	const int iLength = Read<int>(fStringObj + 16);
	if (iLength <= 0 || iLength > 64)
		return {};
	const uintptr_t fText = Read<uintptr_t>(fStringObj + 8);
	if (!fText || !Memory::IsValid(fText))
		return {};
	std::vector<wchar_t> wcBuffer(static_cast<size_t>(iLength) + 1, L'\0');
	Memory::ReadVirtual(PVOID(fText), wcBuffer.data(), iLength * sizeof(wchar_t));
	std::wstring ws(wcBuffer.data(), static_cast<size_t>(iLength));
	return std::string(ws.begin(), ws.end());
}

inline std::string TryReadPlayerNameInlineFString(uintptr_t playerState, uint32_t offset) {
	if (!playerState || !Memory::IsValid(playerState))
		return {};
	const int iLength = Read<int>(playerState + offset + 0x10);
	if (iLength <= 0 || iLength > 64)
		return {};
	const uintptr_t fText = Read<uintptr_t>(playerState + offset + 0x8);
	if (!fText || !Memory::IsValid(fText))
		return {};
	std::vector<wchar_t> wcBuffer(static_cast<size_t>(iLength) + 1, L'\0');
	Memory::ReadVirtual(PVOID(fText), wcBuffer.data(), iLength * sizeof(wchar_t));
	std::wstring ws(wcBuffer.data(), static_cast<size_t>(iLength));
	return std::string(ws.begin(), ws.end());
}

inline std::string TryReadPlayerNameFTextAt(uintptr_t playerState, uint32_t offset) {
	const uintptr_t itemNamePtr = Read<uintptr_t>(playerState + offset);
	if (!itemNamePtr || !Memory::IsValid(itemNamePtr))
		return {};
	const uintptr_t fDataPtr = Read<uintptr_t>(itemNamePtr + Offsets::FData);
	if (!fDataPtr || !Memory::IsValid(fDataPtr))
		return {};
	const std::string str = read_wstr(fDataPtr);
	return str;
}

inline std::string TryReadPlayerDisplayName(uintptr_t playerState) {
	if (!playerState || !Memory::IsValid(playerState))
		return {};
	std::string name =
	    TryReadPlayerNameXorDecrypt(Read<uintptr_t>(playerState + Offsets::Playername));
	if (PlayerDisplayNamePlausible(name))
		return name;
	name = TryReadPlayerNamePlainFStringObj(Read<uintptr_t>(playerState + Offsets::Playername));
	if (PlayerDisplayNamePlausible(name))
		return name;
	name = TryReadPlayerNameFTextAt(playerState, Offsets::Playername);
	if (PlayerDisplayNamePlausible(name))
		return name;
	return {};
}

std::string GetPlayerName(uintptr_t PlayerState) {
	const std::string name = TryReadPlayerDisplayName(PlayerState);
	return name.empty() ? std::string(("Unknown")) : name;
}

bool IsVisible(uintptr_t mesh)
{
	float LastSubmitTime = Read<float>(mesh + 0x278);
	float LastRenderTimeOnScreen = Read<float>(mesh + 0x280);
	return LastRenderTimeOnScreen + 0.06f >= LastSubmitTime;
}

bool is_dead(uintptr_t Player)
{
	return (Read<char>(Player + Offsets::bIsDying) >> 3) & 1;
}

inline bool IsRetracLobbySession();

inline uintptr_t GetPawnMeshComponent(uintptr_t pawn) {
	if (!IsPlausibleUObject(pawn))
		return 0;
	const uintptr_t mesh = Read<uintptr_t>(pawn + Offsets::Mesh);
	if (MeshHasPlausibleComponentToWorld(mesh) || MeshHasPlausibleWorldLocation(mesh))
		return mesh;
	uint32_t poseOff = 0;
	int poseNum = 0;
	const uint32_t fieldOff = FindPawnMeshOffset(pawn, poseOff, poseNum);
	if (!fieldOff)
		return 0;
	const uintptr_t alt = Read<uintptr_t>(pawn + fieldOff);
	return MeshHasPlausibleComponentToWorld(alt) ? alt : 0;
}

inline bool PlayerArrayTeamsAreMixed() {
	if (!LocalPtrs::GameState || !Memory::IsValid(LocalPtrs::GameState))
		return false;
	if (!LocalPtrs::PlayerState || !Memory::IsValid(LocalPtrs::PlayerState))
		return false;
	if (!LocalPtrs::PlayerArray || LocalPtrs::PlayerArrayCount <= 1)
		return false;
	const uint8_t localTeam = Read<uint8_t>(LocalPtrs::PlayerState + Offsets::TeamIndex);
	int num = LocalPtrs::PlayerArrayCount;
	if (num > 200)
		num = 200;
	for (int i = 0; i < num; ++i) {
		const uintptr_t ps =
		    Read<uintptr_t>(LocalPtrs::PlayerArray + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
		if (!ps || !Memory::IsValid(ps))
			continue;
		const uint8_t team = Read<uint8_t>(ps + Offsets::TeamIndex);
		if (team != localTeam)
			return true;
	}
	return false;
}

inline bool IsLikelyPlayerPawn(uintptr_t pawn) {
	if (!IsPlausibleUObject(pawn))
		return false;
	const uintptr_t ps = Read<uintptr_t>(pawn + Offsets::PlayerState);
	const uintptr_t root = Read<uintptr_t>(pawn + Offsets::RootComponent);
	if (!IsPlausibleUObject(ps) || !IsPlausibleUObject(root))
		return false;
	const uintptr_t mesh = Read<uintptr_t>(pawn + Offsets::Mesh);
	if (!GetPawnMeshComponent(pawn) && !MeshHasPlausibleWorldLocation(mesh) &&
	    !MeshHasPlausibleComponentToWorld(mesh))
		return false;
	const uintptr_t psPawn = Read<uintptr_t>(ps + Offsets::PawnPrivate);
	if (psPawn && psPawn != pawn)
		return false;
	return true;
}

inline uintptr_t ResolveEspMeshForPawn(uintptr_t pawn) {
	const uintptr_t mesh = GetPawnMeshComponent(pawn);
	return mesh ? mesh : Read<uintptr_t>(pawn + Offsets::Mesh);
}

inline bool IsLocalPlayerTarget(uintptr_t pawn, uintptr_t playerState, uintptr_t mesh) {
	if (LocalPtrs::Player && pawn && pawn == LocalPtrs::Player)
		return true;
	if (LocalPtrs::PlayerMesh && mesh && mesh == LocalPtrs::PlayerMesh)
		return true;
	if (LocalPtrs::PlayerState && playerState && playerState == LocalPtrs::PlayerState)
		return true;
	if (LocalPtrs::PlayerState && Memory::IsValid(LocalPtrs::PlayerState) && pawn) {
		const uintptr_t owned =
		    Read<uintptr_t>(LocalPtrs::PlayerState + Offsets::PawnPrivate);
		if (owned && owned == pawn)
			return true;
	}
	if (LocalPtrs::PlayerController && Memory::IsValid(LocalPtrs::PlayerController) && pawn) {
		const uintptr_t ack =
		    Read<uintptr_t>(LocalPtrs::PlayerController + Offsets::AcknowledgedPawn);
		if (ack && ack == pawn)
			return true;
	}
	// ponytail: lobby-only duplicate preview on the same spot — never use in-match (100 u FFAs)
	if (IsRetracLobbySession() && LocalPtrs::PlayerMesh && mesh &&
	    mesh != LocalPtrs::PlayerMesh && pawn != LocalPtrs::Player) {
		const Vector3 localLoc = GetMeshWorldLocation(LocalPtrs::PlayerMesh);
		const Vector3 targetLoc = GetMeshWorldLocation(mesh);
		if (Dist3(localLoc, targetLoc) < 25.f)
			return true;
	}
	return false;
}

inline void PushCachedPlayerFromPlayerState(uintptr_t ps,
                                            std::vector<LocalPtrs::CachedPlayer>& out) {
	if (!IsPlausibleUObject(ps))
		return;
	const uintptr_t pawn = Read<uintptr_t>(ps + Offsets::PawnPrivate);
	if (!IsPlausibleUObject(pawn) || is_dead(pawn))
		return;
	const uintptr_t root = Read<uintptr_t>(pawn + Offsets::RootComponent);
	if (!IsPlausibleUObject(root))
		return;
	uintptr_t mesh = GetPawnMeshComponent(pawn);
	if (!mesh)
		mesh = Read<uintptr_t>(pawn + Offsets::Mesh);
	if (!MeshHasPlausibleWorldLocation(mesh) && !MeshHasPlausibleComponentToWorld(mesh))
		return;
	if (IsLocalPlayerTarget(pawn, ps, mesh))
		return;
	if (PlayerArrayTeamsAreMixed() && LocalPtrs::PlayerState &&
	    Memory::IsValid(LocalPtrs::PlayerState)) {
		const uint8_t localTeam = Read<uint8_t>(LocalPtrs::PlayerState + Offsets::TeamIndex);
		const uint8_t targetTeam = Read<uint8_t>(ps + Offsets::TeamIndex);
		if (localTeam != 255 && localTeam == targetTeam)
			return;
	}
	LocalPtrs::CachedPlayer p{};
	p.Pawn = pawn;
	p.Mesh = mesh;
	p.PlayerState = ps;
	p.RootComponent = root;
	if (Settings::Username)
		p.Name = GetPlayerName(ps);
	if (Settings::KillESP && LocalPtrs::Player)
		p.Kills = Read<_int32>(ps + Offsets::KillScore);
	out.push_back(std::move(p));
}

inline void PushCachedPlayerFromPawn(uintptr_t pawn, std::vector<LocalPtrs::CachedPlayer>& out) {
	if (!IsLikelyPlayerPawn(pawn) || is_dead(pawn))
		return;
	const uintptr_t ps = Read<uintptr_t>(pawn + Offsets::PlayerState);
	if (!ps)
		return;
	PushCachedPlayerFromPlayerState(ps, out);
}

inline bool IsRetracLobbySession() {
	if (LocalPtrs::PlayerArrayCount > 1 && PlayerArrayTeamsAreMixed())
		return false;
	uintptr_t pcm = LocalPtrs::PlayerCam;
	if ((!pcm || !Memory::IsValid(pcm)) && LocalPtrs::PlayerController &&
	    Memory::IsValid(LocalPtrs::PlayerController))
		pcm = Read<uintptr_t>(LocalPtrs::PlayerController + Offsets::playercameramanager);
	if (RetracHelicarrierPcmCacheReadable(pcm))
		return true;
	if (LocalPtrs::PlayerArrayCount > 1)
		return false;
	if (!LocalPtrs::Player || !Memory::IsValid(LocalPtrs::Player))
		return false;
	if (!LocalPtrs::PlayerState || !Memory::IsValid(LocalPtrs::PlayerState))
		return false;
	const uint8_t team = Read<uint8_t>(LocalPtrs::PlayerState + Offsets::TeamIndex);
	return team == 255;
}

// Solo lobby: no remote pawns pass the cache, but bones/W2S should still draw on your character.
inline void PushCachedLobbyLocalPreview(std::vector<LocalPtrs::CachedPlayer>& out) {
	if (!IsRetracLobbySession())
		return;
	for (const auto& existing : out) {
		if (existing.LobbyPreview)
			return;
	}
	const uintptr_t pawn = LocalPtrs::Player;
	const uintptr_t ps = LocalPtrs::PlayerState;
	const uintptr_t mesh = ResolveEspMeshForPawn(pawn);
	if (!MeshHasPlausibleComponentToWorld(mesh))
		return;
	if (!MeshHasRetracPoseBuffer(mesh))
		return;
	LocalPtrs::CachedPlayer p{};
	p.Pawn = pawn;
	p.Mesh = mesh;
	p.PlayerState = ps;
	p.RootComponent = Read<uintptr_t>(pawn + Offsets::RootComponent);
	p.LobbyPreview = true;
	if (Settings::Username)
		p.Name = GetPlayerName(ps);
	out.push_back(std::move(p));
}

inline void AppendPlayersFromLevel(uintptr_t level, std::vector<LocalPtrs::CachedPlayer>& out,
                                 std::unordered_set<uintptr_t>& seen) {
	if (!level || !Memory::IsValid(level))
		return;
	const uintptr_t actors = Read<uintptr_t>(level + Offsets::ActorsArray);
	const int num = Read<int>(level + Offsets::ActorsCount);
	if (!actors || !Memory::IsValid(actors) || num <= 0 || num > 8000)
		return;
	const int cap = num > 3500 ? 3500 : num;
	for (int i = 0; i < cap; ++i) {
		const uintptr_t actor = Read<uintptr_t>(actors + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
		if (!actor || !Memory::IsValid(actor) || seen.count(actor))
			continue;
		if (!IsLikelyPlayerPawn(actor))
			continue;
		seen.insert(actor);
		PushCachedPlayerFromPawn(actor, out);
	}
}

inline void AppendPlayersFromWorldLevels(uintptr_t world, std::vector<LocalPtrs::CachedPlayer>& out,
                                         std::unordered_set<uintptr_t>& seen) {
	if (!world || !Memory::IsValid(world))
		return;
	AppendPlayersFromLevel(Read<uintptr_t>(world + Offsets::PersistentLevel), out, seen);
	const uintptr_t levels = Read<uintptr_t>(world + Offsets::Levels);
	const int levelCount = Read<int>(world + Offsets::Levels + sizeof(uintptr_t));
	if (!levels || !Memory::IsValid(levels) || levelCount <= 0 || levelCount > 64)
		return;
	for (int i = 0; i < levelCount; ++i) {
		const uintptr_t level = Read<uintptr_t>(levels + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
		AppendPlayersFromLevel(level, out, seen);
	}
}

ImColor GetColorFromRarity(int rarity) {
	switch (rarity) {
	case 0: return ImColor(200, 200, 200, 255); 
	case 1: return ImColor(0, 255, 0, 255);     
	case 2: return ImColor(0, 150, 255, 255);   
	case 3: return ImColor(200, 0, 255, 255);   
	case 4: return ImColor(255, 128, 0, 255);   
	case 5: return ImColor(255, 255, 0, 255);  
	case 6: return ImColor(0, 255, 255, 255);   
	case 7: return ImColor(255, 0, 0, 255);    
	default: return ImColor(255, 255, 255, 255);
	}
}

inline std::string get_player_platform(uintptr_t player_state, ImColor& resultColor) {
	if (!player_state) return "Lobby";
	static wchar_t platform_buffer[64] = { 0 };
	static std::string cachedPlatform;
	static ImColor cachedColor;
	static uintptr_t lastPlayerState = 0;

	if (lastPlayerState == player_state && !cachedPlatform.empty()) {
		resultColor = cachedColor;
		return cachedPlatform;
	}

	const uintptr_t fstring_ptr = Read<uintptr_t>(player_state + Offsets::Platform);

	if (fstring_ptr) {
		for (int i = 0; i < 64; i++) {
			platform_buffer[i] = Read<wchar_t>(fstring_ptr + (i * sizeof(wchar_t)));
			if (platform_buffer[i] == 0) break;
		}
	}
	else {
		platform_buffer[0] = 0;
	}

	std::wstring platform_wstr(platform_buffer);
	std::string platform_str(platform_wstr.begin(), platform_wstr.end());
	static const std::unordered_map<std::string, std::pair<std::string, ImColor>> platform_map = {
		{ xorstr_("XBL"),      { xorstr_("XBOX"),         ImColor(0, 255, 0)     } },
		{ xorstr_("PSN"),      { xorstr_("PS4"),    ImColor(0, 0, 255)     } },
		{ xorstr_("PS5"),      { xorstr_("PS5"),    ImColor(0, 0, 200)     } },
		{ xorstr_("XSX"),      { xorstr_("XBOX S/X"),  ImColor(0, 128, 0)     } },
		{ xorstr_("SWT"),      { xorstr_("NINTENDO"),         ImColor(255, 0, 0)     } },
		{ xorstr_("WIN"),      { xorstr_("WIN"),          ImColor(255, 255, 255) } },
		{ xorstr_("MOBIL-A"),  { xorstr_("Phone"),          ImColor(0, 255, 0)     } },
		{ xorstr_("MOBIL-I"),  { xorstr_("IOS"),              ImColor(0, 122, 255)   } }
	};
	auto it = platform_map.find(platform_str);
	if (it != platform_map.end()) {
		cachedPlatform = it->second.first;
		cachedColor = it->second.second;
		resultColor = cachedColor;
		lastPlayerState = player_state;
		return cachedPlatform;
	}
	cachedPlatform = "Lobby";
	cachedColor = ImColor(128, 128, 128);
	resultColor = cachedColor;
	lastPlayerState = player_state;
	return cachedPlatform;
}

inline uint64_t UWorldFromEngine(uint64_t engine) {
	if (!engine || !Memory::IsValid(engine))
		return 0;
	const uint64_t viewport = Read<uint64_t>(engine + Offsets::GameViewport);
	if (!viewport || !Memory::IsValid(viewport))
		return 0;
	const uint64_t world = Read<uint64_t>(viewport + Offsets::ViewportWorld);
	if (!world || !Memory::IsValid(world))
		return 0;
	return world;
}

// Retrac-SDK-14.60 SDK/Basic.hpp: TUObjectArray at ImageBase + Offsets::GObjects.
namespace TUObjectArrayLayout {
	inline constexpr uint64_t Objects = 0x0;       // FUObjectItem**
	inline constexpr uint64_t MaxElements = 0x10;
	inline constexpr uint64_t NumElements = 0x14;
	inline constexpr uint64_t MaxChunks = 0x18;
	inline constexpr uint64_t NumChunks = 0x1C;
	inline constexpr int32_t ElementsPerChunk = 0x10000;
	inline constexpr int32_t FUObjectItemSize = 0x18; // FUObjectItem::Object @ 0x0
	inline constexpr uint32_t RF_ClassDefaultObject = 0x10; // UObject::Flags @ 0x8
}

struct ResolvedGObjects {
	uintptr_t arrayBase = 0;
	uint64_t chunkTable = 0;
	int32_t numElements = 0;
	int32_t numChunks = 0;
	bool viaDereference = false;
	bool numElementsReadOk = false;
	bool chunkTableReadOk = false;
};

enum class GObjectsResolveLayout : uint8_t {
	None = 0,
	InlineAtSlot,
	SlotPointerDeref,
	FUObjectArrayObjObjects,
	UnresolvedFallback,
};

inline std::atomic<uint8_t> LastGObjectsResolveLayout{0};

inline const char* GObjectsResolveLayoutName(GObjectsResolveLayout layout) {
	switch (layout) {
	case GObjectsResolveLayout::InlineAtSlot:
		return "inline@GObjects";
	case GObjectsResolveLayout::SlotPointerDeref:
		return "pointer-deref";
	case GObjectsResolveLayout::FUObjectArrayObjObjects:
		return "FUObjectArray+0x10";
	case GObjectsResolveLayout::UnresolvedFallback:
		return "fallback(unresolved)";
	default:
		return "none";
	}
}

template <typename T>
inline bool ReadFieldBestEffort(uintptr_t address, T& out) {
	if (!address) {
		out = T{};
		return false;
	}
	if (ReadOk(address, out))
		return true;
	if (Memory::Process.ReadUnchecked(address, &out, static_cast<DWORD>(sizeof(T))))
		return true;
	out = Read<T>(address);
	return Memory::Process.ReadUnchecked(address, &out, static_cast<DWORD>(sizeof(T)));
}

inline bool ReadTUObjectArrayFields(uintptr_t arrayBase, uint64_t& chunkTable, int32_t& numElements,
                                    int32_t& numChunks, bool& chunkOk, bool& numOk) {
	chunkOk = ReadFieldBestEffort(arrayBase + TUObjectArrayLayout::Objects, chunkTable);
	numOk = ReadFieldBestEffort(arrayBase + TUObjectArrayLayout::NumElements, numElements);
	if (!numOk) {
		numElements = Read<int32_t>(arrayBase + TUObjectArrayLayout::NumElements);
		numOk = numElements > 0 && numElements < 5000000;
	}
	if (!chunkOk) {
		chunkTable = Read<uint64_t>(arrayBase + TUObjectArrayLayout::Objects);
		chunkOk = chunkTable != 0 && Memory::IsValid(chunkTable);
	}
	const bool numChunksOk =
		ReadFieldBestEffort(arrayBase + TUObjectArrayLayout::NumChunks, numChunks);
	return chunkOk && numOk && numChunksOk;
}

enum class TUObjectArrayReject : uint8_t {
	Ok,
	InvalidBase,
	FieldReadFailed,
	NumOutOfRange,
	BelowMinElements,
	BadNumChunks,
	InvalidChunkTable,
	NonCanonicalChunkLayout,
};

inline const char* TUObjectArrayRejectName(TUObjectArrayReject r) {
	switch (r) {
	case TUObjectArrayReject::Ok:
		return "ok";
	case TUObjectArrayReject::InvalidBase:
		return "invalid base";
	case TUObjectArrayReject::FieldReadFailed:
		return "field RPM failed";
	case TUObjectArrayReject::NumOutOfRange:
		return "NumElements out of range";
	case TUObjectArrayReject::BelowMinElements:
		return "NumElements below min";
	case TUObjectArrayReject::BadNumChunks:
		return "NumChunks out of range";
	case TUObjectArrayReject::InvalidChunkTable:
		return "invalid chunk table ptr";
	case TUObjectArrayReject::NonCanonicalChunkLayout:
		return "ChunkTableLooksCanonical failed";
	default:
		return "unknown";
	}
}

inline bool ChunkTableLooksCanonical(uint64_t chunkTable, int32_t numElements, int32_t numChunks) {
	if (!chunkTable || !Memory::IsValid(chunkTable))
		return false;
	if (numElements <= 0 || numChunks < 1)
		return false;
	const int32_t needChunks =
		(numElements + TUObjectArrayLayout::ElementsPerChunk - 1) / TUObjectArrayLayout::ElementsPerChunk;
	// UE 4.27 chunked arrays may keep one spare chunk (e.g. 32758 elems, 2 chunks).
	if (numChunks < needChunks || numChunks > needChunks + 2)
		return false;
	const int32_t probeChunks = numChunks < needChunks + 1 ? numChunks : needChunks + 1;
	for (int32_t ci = 0; ci < probeChunks; ++ci) {
		uint64_t chunkPtr = 0;
		if (!ReadOk(chunkTable + static_cast<uintptr_t>(ci) * sizeof(uint64_t), chunkPtr))
			return false;
		if (!chunkPtr || !Memory::IsValid(chunkPtr))
			return false;
	}
	uint64_t firstChunk = 0;
	if (!ReadOk(chunkTable, firstChunk) || !firstChunk || !Memory::IsValid(firstChunk))
		return false;
	for (int32_t si = 0; si < 16; ++si) {
		uint64_t obj = 0;
		const uintptr_t item = static_cast<uintptr_t>(firstChunk)
		                       + static_cast<uintptr_t>(si) * TUObjectArrayLayout::FUObjectItemSize;
		if (!ReadOk(item, obj) || !obj)
			continue;
		if (LooksLikeUserObjectPointer(obj))
			return true;
	}
	return true;
}

inline TUObjectArrayReject ClassifyTUObjectArray(uintptr_t arrayBase, int32_t minElements,
                                                 bool requireCanonical, int32_t* outNum = nullptr) {
	if (!arrayBase || !Memory::IsValid(arrayBase))
		return TUObjectArrayReject::InvalidBase;
	uint64_t chunkTable = 0;
	int32_t numElements = 0;
	int32_t numChunks = 0;
	bool chunkOk = false;
	bool numOk = false;
	if (!ReadTUObjectArrayFields(arrayBase, chunkTable, numElements, numChunks, chunkOk, numOk))
		return TUObjectArrayReject::FieldReadFailed;
	if (!chunkOk || !numOk)
		return TUObjectArrayReject::FieldReadFailed;
	if (outNum)
		*outNum = numElements;
	if (numElements > 5000000 || numElements < 0)
		return TUObjectArrayReject::NumOutOfRange;
	if (minElements > 0 && numElements < minElements)
		return TUObjectArrayReject::BelowMinElements;
	if (numChunks < 1 || numChunks > 64)
		return TUObjectArrayReject::BadNumChunks;
	if (!chunkTable || !Memory::IsValid(chunkTable))
		return TUObjectArrayReject::InvalidChunkTable;
	if (requireCanonical && minElements >= kSdkMinGObjectsElements
	    && !ChunkTableLooksCanonical(chunkTable, numElements, numChunks))
		return TUObjectArrayReject::NonCanonicalChunkLayout;
	return TUObjectArrayReject::Ok;
}

inline bool LooksLikeTUObjectArray(uintptr_t arrayBase, int32_t minElements = 100,
                                   bool requireCanonical = true) {
	return ClassifyTUObjectArray(arrayBase, minElements, requireCanonical, nullptr)
	       == TUObjectArrayReject::Ok;
}

inline bool FillResolvedGObjectsFromArrayBase(uintptr_t arrayBase, ResolvedGObjects& r, bool viaDeref) {
	if (!arrayBase || !Memory::IsValid(arrayBase))
		return false;
	r.arrayBase = arrayBase;
	r.viaDereference = viaDeref;
	r.chunkTableReadOk =
		ReadFieldBestEffort(arrayBase + TUObjectArrayLayout::Objects, r.chunkTable);
	r.numElementsReadOk =
		ReadFieldBestEffort(arrayBase + TUObjectArrayLayout::NumElements, r.numElements);
	(void)ReadFieldBestEffort(arrayBase + TUObjectArrayLayout::NumChunks, r.numChunks);
	return r.numElementsReadOk && r.chunkTableReadOk;
}

inline bool TryResolveGObjectsAtSlot(uintptr_t slot, ResolvedGObjects& r,
                                     GObjectsResolveLayout* layoutOut = nullptr) {
	r = {};
	if (!slot || !Memory::IsValid(slot))
		return false;

	auto finish = [&](GObjectsResolveLayout layout, bool ok) {
		LastGObjectsResolveLayout.store(static_cast<uint8_t>(layout), std::memory_order_relaxed);
		if (layoutOut)
			*layoutOut = layout;
		return ok;
	};

	if (LooksLikeTUObjectArray(slot, 1)) {
		FillResolvedGObjectsFromArrayBase(slot, r, false);
		return finish(GObjectsResolveLayout::InlineAtSlot, true);
	}

	uint64_t indirect = 0;
	if (ReadFieldBestEffort(slot, indirect) && indirect && Memory::IsValid(static_cast<uintptr_t>(indirect))
	    && LooksLikeTUObjectArray(static_cast<uintptr_t>(indirect), 1)) {
		FillResolvedGObjectsFromArrayBase(static_cast<uintptr_t>(indirect), r, true);
		return finish(GObjectsResolveLayout::SlotPointerDeref, true);
	}

	const uintptr_t nested = slot + 0x10; // FUObjectArray::ObjObjects
	if (LooksLikeTUObjectArray(nested, 1)) {
		FillResolvedGObjectsFromArrayBase(nested, r, false);
		return finish(GObjectsResolveLayout::FUObjectArrayObjObjects, true);
	}

	if (FillResolvedGObjectsFromArrayBase(slot, r, false))
		return finish(GObjectsResolveLayout::UnresolvedFallback, false);
	return finish(GObjectsResolveLayout::None, false);
}

inline bool KernelGObjectsSlotDataReadable(uintptr_t imageBase, int32_t* outNum = nullptr) {
	if (!imageBase || !Memory::Process.KernelAttached)
		return false;
	const uintptr_t slot = imageBase + Offsets::GObjects;
	int32_t num = 0;
	if (ReadFieldBestEffort(slot + TUObjectArrayLayout::NumElements, num) && num > 100
	    && num < 5000000) {
		if (outNum)
			*outNum = num;
		return true;
	}
	if (ReadFieldBestEffort(slot + 0x10 + TUObjectArrayLayout::NumElements, num) && num > 100
	    && num < 5000000) {
		if (outNum)
			*outNum = num;
		return true;
	}
	ResolvedGObjects r{};
	if (TryResolveGObjectsAtSlot(slot, r) && r.numElementsReadOk && r.numElements > 100
	    && r.numElements < 5000000) {
		if (outNum)
			*outNum = r.numElements;
		return true;
	}
	return false;
}

namespace SdkPeSections {
struct View {
	uint32_t virtualAddress = 0;
	uint32_t virtualSize = 0;
	char name[9]{};
};

inline bool Parse(uintptr_t imageBase, std::vector<View>& sections, uint32_t& sizeOfImage) {
	sections.clear();
	sizeOfImage = 0;
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	if (Read<uint16_t>(imageBase) != 0x5A4D)
		return false;
	const uint32_t e_lfanew = Read<uint32_t>(imageBase + 0x3C);
	if (!e_lfanew || e_lfanew > 0x800)
		return false;
	const uintptr_t nt = imageBase + e_lfanew;
	if (Read<uint32_t>(nt) != 0x00004550)
		return false;
	const uint16_t optSize = Read<uint16_t>(nt + 0x14);
	const uint16_t nSections = Read<uint16_t>(nt + 0x6);
	if (Read<uint16_t>(nt + 0x18) != 0x20B)
		return false;
	sizeOfImage = Read<uint32_t>(nt + 0x18 + 0x38);
	const uintptr_t firstSection = nt + 0x18 + optSize;
	for (uint16_t i = 0; i < nSections && i < 96; ++i) {
		const uintptr_t sh = firstSection + static_cast<uintptr_t>(i) * 40;
		View v{};
		for (int j = 0; j < 8; ++j)
			v.name[j] = static_cast<char>(Read<uint8_t>(sh + j));
		v.virtualSize = Read<uint32_t>(sh + 8);
		v.virtualAddress = Read<uint32_t>(sh + 12);
		sections.push_back(v);
	}
	return sizeOfImage > 0x1000;
}

inline bool SectionIsDataLike(const char* name) {
	return std::strcmp(name, ".data") == 0 || std::strcmp(name, ".rdata") == 0
	       || std::strcmp(name, ".idata") == 0 || std::strcmp(name, ".bss") == 0;
}

inline bool SectionIsExecutable(const char* name) {
	return std::strcmp(name, ".text") == 0 || std::strcmp(name, "CODE") == 0;
}
} // namespace SdkPeSections

// Packed launcher module (~SizeOfImage 0x112A000); full unpacked UE image is much larger.
inline constexpr uint32_t kPackedStubMaxSizeOfImage = 0x02000000u;
inline constexpr uint32_t kFullUnpackedMinSizeOfImage = 0x05000000u;

inline bool ReadPeSizeOfImage(uintptr_t imageBase, uint32_t& sizeOfImage) {
	sizeOfImage = 0;
	std::vector<SdkPeSections::View> sections;
	return SdkPeSections::Parse(imageBase, sections, sizeOfImage);
}

inline bool IsPackedStubPeImage(uintptr_t imageBase) {
	uint32_t sizeOfImage = 0;
	if (!ReadPeSizeOfImage(imageBase, sizeOfImage) || !sizeOfImage)
		return false;
	return sizeOfImage < kPackedStubMaxSizeOfImage;
}

inline bool IsFullUnpackedPeImage(uintptr_t imageBase) {
	uint32_t sizeOfImage = 0;
	if (!ReadPeSizeOfImage(imageBase, sizeOfImage))
		return false;
	return sizeOfImage >= kFullUnpackedMinSizeOfImage;
}

inline bool VirtualQuerySlotCommitted(uintptr_t slot) {
	if (!slot || !Memory::Process.Handle)
		return false;
	MEMORY_BASIC_INFORMATION mbi{};
	if (!VirtualQueryEx(Memory::Process.Handle, reinterpret_cast<LPCVOID>(slot), &mbi,
	                    sizeof(mbi)))
		return false;
	return mbi.State == MEM_COMMIT;
}

inline bool RvaInSection(const std::vector<SdkPeSections::View>& sections, uint64_t rva,
                         const char* sectionName) {
	for (const auto& sec : sections) {
		if (std::strcmp(sec.name, sectionName) != 0)
			continue;
		const uint64_t lo = sec.virtualAddress;
		const uint64_t hi = lo + (sec.virtualSize ? sec.virtualSize : 0);
		if (rva >= lo && rva + 8 <= hi)
			return true;
	}
	return false;
}

inline bool GObjectsPeScanCandidateRejected(
    uintptr_t imageBase, uintptr_t addr, const std::vector<SdkPeSections::View>& sections) {
	const uint64_t rva = addr - imageBase;
	if (RvaInSection(sections, rva, ".rdata"))
		return true;
	uint64_t chunkTable = 0;
	if (ReadFieldBestEffort(addr + TUObjectArrayLayout::Objects, chunkTable)
	    && LooksLikeAsciiPointer(chunkTable))
		return true;
	for (int32_t si = 0; si < 4; ++si) {
		uint64_t obj = 0;
		const uintptr_t item = addr + static_cast<uintptr_t>(si) * TUObjectArrayLayout::FUObjectItemSize;
		if (ReadFieldBestEffort(item, obj) && obj && LooksLikeAsciiPointer(obj))
			return true;
	}
	return false;
}

namespace SdkPeDiscovery {
inline std::atomic<bool> staticGlobalsReadableLogged{false};
void LogStaticGlobalsReadable(uintptr_t imageBase);
} // namespace SdkPeDiscovery

inline bool StaticGObjectsRvaInImage(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	uint32_t sizeOfImage = 0;
	std::vector<SdkPeSections::View> sections;
	if (!SdkPeSections::Parse(imageBase, sections, sizeOfImage))
		return false;
	return Offsets::GObjects + 0x20 <= sizeOfImage;
}

inline bool StaticUWorldRvaInImage(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	uint32_t sizeOfImage = 0;
	std::vector<SdkPeSections::View> sections;
	if (!SdkPeSections::Parse(imageBase, sections, sizeOfImage))
		return false;
	return Offsets::UWorld + 8 <= sizeOfImage;
}

inline bool StaticGObjectsSlotRegionUnusable(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase) || !StaticGObjectsRvaInImage(imageBase))
		return false;
	const uintptr_t slot = imageBase + Offsets::GObjects;
	if (Memory::Process.Handle) {
		MEMORY_BASIC_INFORMATION mbi{};
		if (VirtualQueryEx(Memory::Process.Handle, reinterpret_cast<LPCVOID>(slot), &mbi,
		                   sizeof(mbi))
		    && mbi.State == MEM_FREE)
			return true;
	}
	uint64_t q = 0;
	if (!ReadFieldBestEffort(slot, q))
		return true;
	if (q != 0)
		return false;
	int32_t numInline = 0;
	(void)ReadFieldBestEffort(slot + TUObjectArrayLayout::NumElements, numInline);
	int32_t numNested = 0;
	(void)ReadFieldBestEffort(slot + 0x10 + TUObjectArrayLayout::NumElements, numNested);
	return numInline == 0 && numNested == 0;
}

inline bool StaticGObjectsSlotTrusted(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	if (!StaticGObjectsRvaInImage(imageBase))
		return false;
	if (StaticGObjectsSlotRegionUnusable(imageBase))
		return false;
	const uintptr_t slot = imageBase + Offsets::GObjects;
	ResolvedGObjects r{};
	if (!TryResolveGObjectsAtSlot(slot, r))
		return false;
	if (!r.numElementsReadOk)
		return false;
	return LooksLikeTUObjectArray(r.arrayBase, 100, false);
}

inline bool StaticGObjectsSlotLive(uintptr_t imageBase, int32_t minElements = 100) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	if (!StaticGObjectsRvaInImage(imageBase))
		return false;
	const uintptr_t slot = imageBase + Offsets::GObjects;
	ResolvedGObjects r{};
	GObjectsResolveLayout layout = GObjectsResolveLayout::None;
	if (!TryResolveGObjectsAtSlot(slot, r, &layout))
		return false;
	if (!r.numElementsReadOk)
		return false;
	const bool requireCanonical = minElements >= kSdkMinGObjectsElements;
	return LooksLikeTUObjectArray(r.arrayBase, minElements, requireCanonical);
}

inline constexpr int32_t kLargePeProbeGObjectsMin = 50000;
inline constexpr int32_t kLargePeProbeGObjectsMax = 2000000;
inline std::atomic<bool> largePeSdkSuccessLogged{false};

inline bool LargePeStaticGObjectsLive(uintptr_t imageBase, int32_t* outNumElements) {
	if (!imageBase || !StaticGObjectsRvaInImage(imageBase))
		return false;
	const uintptr_t slot = imageBase + Offsets::GObjects;
	ResolvedGObjects r{};
	if (!TryResolveGObjectsAtSlot(slot, r) || !r.numElementsReadOk)
		return false;
	if (r.numElements < kLargePeProbeGObjectsMin || r.numElements > kLargePeProbeGObjectsMax)
		return false;
	if (r.numChunks < 1 || r.numChunks > 64)
		return false;
	if (!r.chunkTable || !Memory::IsValid(r.chunkTable))
		return false;
	if (!LooksLikeTUObjectArray(r.arrayBase, kLargePeProbeGObjectsMin, true))
		return false;
	if (!ChunkTableLooksCanonical(r.chunkTable, r.numElements, r.numChunks))
		return false;
	if (outNumElements)
		*outNumElements = r.numElements;
	return true;
}

inline void SdkPeDiscovery::LogStaticGlobalsReadable(uintptr_t imageBase) {
	if (staticGlobalsReadableLogged.exchange(true, std::memory_order_relaxed))
		return;
	if (!imageBase || !StaticGObjectsSlotLive(imageBase, kSdkMinGObjectsElements))
		return;
	std::cout << xorstr_("[+] static GObjects readable image base 0x") << std::hex << std::uppercase
	          << imageBase << xorstr_(" RVA 0x") << Offsets::GObjects << std::dec << std::endl;
	if (StaticUWorldRvaInImage(imageBase)) {
		const uint64_t world = Read<uint64_t>(imageBase + Offsets::UWorld);
		std::cout << xorstr_("[+] static UWorld slot image base 0x") << std::hex << std::uppercase
		          << imageBase << xorstr_(" RVA 0x") << Offsets::UWorld << xorstr_(" -> 0x") << world
		          << std::dec << std::endl;
	}
	std::cout.flush();
}

inline bool ScanDataSectionsForLiveGObjects(uintptr_t imageBase,
                                            const std::vector<SdkPeSections::View>& sections,
                                            uint64_t& outGObjectsRva) {
	constexpr size_t kMaxProbes = 512;
	size_t probes = 0;
	for (const auto& sec : sections) {
		if (std::strcmp(sec.name, ".data") != 0)
			continue;
		const uintptr_t secStart = imageBase + sec.virtualAddress;
		const size_t secBytes =
			std::min<size_t>(sec.virtualSize ? sec.virtualSize : 0, 0x200000);
		for (size_t off = 0; off + 0x20 <= secBytes && probes < kMaxProbes; off += 8, ++probes) {
			const uintptr_t addr = secStart + off;
			const uint64_t rva = addr - imageBase;
			if (GObjectsPeScanCandidateRejected(imageBase, addr, sections))
				continue;
			const TUObjectArrayReject loose =
				ClassifyTUObjectArray(addr, 100, true, nullptr);
			if (loose != TUObjectArrayReject::Ok)
				continue;
			int32_t n = 0;
			const TUObjectArrayReject strict =
				ClassifyTUObjectArray(addr, kSdkMinGObjectsElements, false, &n);
			if (strict == TUObjectArrayReject::Ok) {
				outGObjectsRva = rva;
				return true;
			}
			if (SdkPeDiscovery::ShouldLogPeScanReject()) {
				std::cout << xorstr_("[-] PE scan reject GObjects candidate RVA 0x") << std::hex
				          << std::uppercase << rva << xorstr_(" NumElements ") << std::dec << n
				          << xorstr_(" (") << TUObjectArrayRejectName(strict) << xorstr_(")") << std::endl;
				std::cout.flush();
			}
		}
	}
	return false;
}

inline bool TryUWorldGlobalAtDataSlot(uintptr_t imageBase, uintptr_t addr, uint64_t& outUWorldRva,
                                      int& inOutBestScore) {
	uint64_t world = 0;
	if (!ReadOk(addr, world) || !world)
		return false;
	const uint64_t rva = addr - imageBase;
	UWorldFieldSnapshot snap{};
	int score = 0;
	const UWorldRejectReason strict = ClassifyUWorldCandidate(world, false, &snap, &score);
	if (strict == UWorldRejectReason::Ok) {
		SdkPeDiscovery::LogUWorldCandidateProbe(rva, world, strict, snap, score, true);
		outUWorldRva = rva;
		inOutBestScore = score;
		return true;
	}
	const UWorldRejectReason relaxed = ClassifyUWorldCandidate(world, true, &snap, &score);
	if (relaxed == UWorldRejectReason::Ok && score >= 3) {
		if (score > inOutBestScore) {
			inOutBestScore = score;
			outUWorldRva = rva;
			SdkPeDiscovery::LogUWorldCandidateProbe(rva, world, relaxed, snap, score, true);
			return true;
		}
		SdkPeDiscovery::LogUWorldCandidateProbe(rva, world, relaxed, snap, score, false);
		return false;
	}
	SdkPeDiscovery::LogUWorldCandidateProbe(rva, world, relaxed, snap, score, false);
	return false;
}

inline bool ScanDataSectionsForUWorldRva(uintptr_t imageBase,
                                       const std::vector<SdkPeSections::View>& sections,
                                       uint64_t& outUWorldRva,
                                       uint64_t nearGObjectsRva = 0) {
	constexpr size_t kMaxProbes = 384;
	size_t probes = 0;
	int bestScore = 0;
	uint64_t bestRva = 0;

	const auto considerSlot = [&](uintptr_t addr) {
		if (probes >= kMaxProbes)
			return;
		++probes;
		uint64_t candidateRva = 0;
		int slotScore = bestScore;
		if (TryUWorldGlobalAtDataSlot(imageBase, addr, candidateRva, slotScore) && slotScore >= 3) {
			if (slotScore > bestScore) {
				bestScore = slotScore;
				bestRva = candidateRva;
			}
		}
	};

	const auto scanRange = [&](uintptr_t rangeStart, uintptr_t rangeEnd) {
		for (const auto& sec : sections) {
			if (std::strcmp(sec.name, ".data") != 0)
				continue;
			const uintptr_t secStart = imageBase + sec.virtualAddress;
			const uintptr_t secEnd =
				secStart + std::min<size_t>(sec.virtualSize ? sec.virtualSize : 0, 0x100000);
			const uintptr_t lo = (std::max)(rangeStart, secStart);
			const uintptr_t hi = (std::min)(rangeEnd, secEnd);
			for (uintptr_t addr = lo & ~uintptr_t{7}; addr + 8 <= hi && probes < kMaxProbes;
			     addr += 8)
				considerSlot(addr);
		}
	};

	if (nearGObjectsRva >= 0x1000 && nearGObjectsRva == Offsets::GObjects) {
		const uintptr_t bandLo = imageBase + nearGObjectsRva - 0x1000;
		const uintptr_t bandHi = imageBase + nearGObjectsRva + 0x2000;
		scanRange(bandLo, bandHi);
		if (bestScore >= 9) {
			outUWorldRva = bestRva;
			return true;
		}
	}

	for (const auto& sec : sections) {
		if (std::strcmp(sec.name, ".data") != 0)
			continue;
		const uintptr_t secStart = imageBase + sec.virtualAddress;
		const size_t secBytes =
			std::min<size_t>(sec.virtualSize ? sec.virtualSize : 0, 0x100000);
		for (size_t off = 0; off + 8 <= secBytes && probes < kMaxProbes; off += 8) {
			const uintptr_t addr = secStart + off;
			if (nearGObjectsRva >= 0x1000 && nearGObjectsRva == Offsets::GObjects) {
				const uint64_t rva = addr - imageBase;
				if (rva + 0x1000 >= nearGObjectsRva && rva <= nearGObjectsRva + 0x2000)
					continue;
			}
			considerSlot(addr);
			if (bestScore >= 12)
				break;
		}
		if (bestScore >= 12)
			break;
	}

	if (bestRva && bestScore >= 3) {
		outUWorldRva = bestRva;
		return true;
	}
	return false;
}

inline void SdkPeDiscoveryPreferStaticRvas(uintptr_t imageBase) {
	if (!imageBase || !StaticGObjectsSlotTrusted(imageBase))
		return;
	SdkPeDiscovery::GObjectRvaDiscovered.store(0, std::memory_order_relaxed);
	SdkPeDiscovery::UWorldRvaDiscovered.store(0, std::memory_order_relaxed);
	SdkPeDiscovery::scanFoundLive.store(false, std::memory_order_relaxed);
	SdkPeDiscovery::staticRvasValidated.store(true, std::memory_order_relaxed);
	SdkPeDiscovery::discoveryFinished.store(true, std::memory_order_relaxed);
}

inline bool SdkPeScanResultStillPlausible(uintptr_t imageBase) {
	if (!SdkPeDiscovery::scanFoundLive.load(std::memory_order_relaxed))
		return false;
	const uint64_t rva = SdkPeDiscovery::GObjectRvaDiscovered.load(std::memory_order_relaxed);
	if (!rva)
		return false;
	if (StaticGObjectsRvaInImage(imageBase)
	    && StaticGObjectsSlotLive(imageBase, kSdkMinGObjectsElements)) {
		if (rva != Offsets::GObjects)
			return false;
	}
	const uintptr_t slot = imageBase + rva;
	ResolvedGObjects r{};
	if (!TryResolveGObjectsAtSlot(slot, r) || !r.numElementsReadOk)
		return false;
	return r.numElements >= kSdkMinGObjectsElements
	       && LooksLikeTUObjectArray(r.arrayBase, kSdkMinGObjectsElements);
}

namespace SdkPeDiscovery {
inline uint64_t GObjectsRva() {
	const uintptr_t img = GlobalImageBase();
	uint64_t discovered = GObjectRvaDiscovered.load(std::memory_order_relaxed);
	if (discovered && discovered != Offsets::GObjects)
		discovered = 0;
	if (img && StaticGObjectsRvaInImage(img) && StaticGObjectsSlotTrusted(img))
		return Offsets::GObjects;
	if (discovered)
		return discovered;
	return Offsets::GObjects;
}

inline uint64_t UWorldRva() {
	const uint64_t discovered = UWorldRvaDiscovered.load(std::memory_order_relaxed);
	if (discovered)
		return discovered;
	const auto staticIfLive = [](uintptr_t img) -> uint64_t {
		if (!img || !StaticUWorldRvaInImage(img))
			return 0;
		const uint64_t world = Read<uint64_t>(img + Offsets::UWorld);
		if (LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3))
			return Offsets::UWorld;
		return 0;
	};
	if (const uint64_t rva = staticIfLive(GlobalImageBase()))
		return rva;
	const uintptr_t packed = Baseadress;
	if (packed && packed != GlobalImageBase())
		if (const uint64_t rva = staticIfLive(packed))
			return rva;
	return 0;
}
} // namespace SdkPeDiscovery

inline void SdkPeDiscovery::EnsureStaticDiscoveryState(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return;

	if (StaticGObjectsSlotRegionUnusable(imageBase))
		peDataScanEnabled.store(true, std::memory_order_relaxed);

	SdkPeDiscoveryPreferStaticRvas(imageBase);

	if (discoveryFinished.load(std::memory_order_relaxed)
	    && (!scanFoundLive.load(std::memory_order_relaxed)
	        || SdkPeScanResultStillPlausible(imageBase)))
		return;

	std::vector<SdkPeSections::View> sections;
	uint32_t sizeOfImage = 0;
	if (SdkPeSections::Parse(imageBase, sections, sizeOfImage)) {
		const auto rvaPlausible = [&](uint64_t rva) -> bool {
			return rva >= 0x1000 && rva + 8 <= sizeOfImage;
		};
		if (rvaPlausible(Offsets::GObjects) && rvaPlausible(Offsets::UWorld))
			staticRvasValidated.store(true, std::memory_order_relaxed);
	}

	if (StaticGObjectsSlotTrusted(imageBase)) {
		SdkPeDiscoveryPreferStaticRvas(imageBase);
		return;
	}

	const uintptr_t slot = imageBase + Offsets::GObjects;
	ResolvedGObjects r{};
	if (TryResolveGObjectsAtSlot(slot, r) && r.numElementsReadOk) {
		discoveryFinished.store(true, std::memory_order_relaxed);
		return;
	}

	if (!StaticGObjectsRvaInImage(imageBase)) {
		NoteStaticSlotPollMiss();
		if (!peDataScanEnabled.load(std::memory_order_relaxed))
			discoveryFinished.store(true, std::memory_order_relaxed);
		return;
	}

	NoteStaticSlotPollMiss();
	if (!peDataScanEnabled.load(std::memory_order_relaxed))
		discoveryFinished.store(true, std::memory_order_relaxed);
}

inline void SdkPeDiscovery::EnsureDiscoveredOffsets(uintptr_t imageBase) {
	EnsureStaticDiscoveryState(imageBase);
	if (discoveryFinished.load(std::memory_order_relaxed)
	    && (!scanFoundLive.load(std::memory_order_relaxed)
	        || SdkPeScanResultStillPlausible(imageBase)))
		return;

	if (!peDataScanEnabled.load(std::memory_order_relaxed))
		return;

	const uintptr_t scanImage = GlobalImageBase();
	if (!scanImage || !Memory::IsValid(scanImage))
		return;
	if (IsPackedStubPeImage(scanImage))
		return;
	imageBase = scanImage;

	static std::mutex discoveryMutex;
	std::lock_guard<std::mutex> lock(discoveryMutex);
	EnsureStaticDiscoveryState(imageBase);
	if (discoveryFinished.load(std::memory_order_relaxed)
	    && (!scanFoundLive.load(std::memory_order_relaxed)
	        || SdkPeScanResultStillPlausible(imageBase)))
		return;

	if (StaticGObjectsSlotTrusted(imageBase)) {
		SdkPeDiscoveryPreferStaticRvas(imageBase);
		return;
	}

	if (scanFoundLive.load(std::memory_order_relaxed) && !SdkPeScanResultStillPlausible(imageBase)) {
		const uint64_t goRva = GObjectRvaDiscovered.load(std::memory_order_relaxed);
		bool scanGoStillLive = false;
		if (goRva) {
			ResolvedGObjects r{};
			const uintptr_t slot = imageBase + goRva;
			scanGoStillLive = TryResolveGObjectsAtSlot(slot, r) && r.numElementsReadOk
			                  && r.numElements >= 100;
		}
		if (scanGoStillLive) {
			discoveryFinished.store(true, std::memory_order_relaxed);
		} else if (StaticGObjectsSlotLive(imageBase, kSdkMinGObjectsElements)) {
			if (ShouldLogPeScanReject()) {
				std::cout << xorstr_("[-] PE scan discredited — static GObjects slot or weak scan match")
				          << std::endl;
				std::cout.flush();
			}
			GObjectRvaDiscovered.store(0, std::memory_order_relaxed);
			UWorldRvaDiscovered.store(0, std::memory_order_relaxed);
			scanFoundLive.store(false, std::memory_order_relaxed);
			discoveryFinished.store(false, std::memory_order_relaxed);
		}
	}

	std::vector<SdkPeSections::View> sections;
	uint32_t sizeOfImage = 0;
	if (!SdkPeSections::Parse(imageBase, sections, sizeOfImage)) {
		discoveryFinished.store(true, std::memory_order_relaxed);
		return;
	}

	uint64_t gobjectsRva = GObjectRvaDiscovered.load(std::memory_order_relaxed);
	if (!gobjectsRva && !StaticGObjectsSlotLive(imageBase, kSdkMinGObjectsElements)
	    && ScanDataSectionsForLiveGObjects(imageBase, sections, gobjectsRva)
	    && gobjectsRva == Offsets::GObjects) {
		GObjectRvaDiscovered.store(gobjectsRva, std::memory_order_relaxed);
		scanFoundLive.store(true, std::memory_order_relaxed);
		if (!peScanGObjectsLogged.exchange(true, std::memory_order_relaxed)) {
			std::cout << xorstr_("[+] PE scan: live GObjects at RVA 0x") << std::hex << std::uppercase
			          << gobjectsRva << std::dec << std::endl;
			std::cout.flush();
		}
	}

	uint64_t uworldRva = UWorldRvaDiscovered.load(std::memory_order_relaxed);
	if (!uworldRva) {
		if (StaticUWorldRvaInImage(imageBase)) {
			const uint64_t world = Read<uint64_t>(imageBase + Offsets::UWorld);
			if (LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3)) {
				UWorldRvaDiscovered.store(Offsets::UWorld, std::memory_order_relaxed);
				uworldRva = Offsets::UWorld;
			}
		}
	}
	if (!uworldRva) {
		uint64_t nearGo = 0;
		if (gobjectsRva == Offsets::GObjects)
			nearGo = Offsets::GObjects;
		else if (StaticGObjectsSlotLive(imageBase, kSdkMinGObjectsElements))
			nearGo = Offsets::GObjects;
		if (ScanDataSectionsForUWorldRva(imageBase, sections, uworldRva, nearGo)) {
			UWorldRvaDiscovered.store(uworldRva, std::memory_order_relaxed);
			scanFoundLive.store(true, std::memory_order_relaxed);
			if (!peScanUWorldLogged.exchange(true, std::memory_order_relaxed)) {
				std::cout << xorstr_("[+] PE scan: live UWorld global at RVA 0x") << std::hex
				          << std::uppercase << uworldRva << std::dec << std::endl;
				std::cout.flush();
			}
		}
	}

	discoveryFinished.store(true, std::memory_order_relaxed);
}

inline void ResetSdkPeDiscoveryIfStillEmpty(uintptr_t imageBase) {
	if (StaticGObjectsSlotTrusted(imageBase)) {
		SdkPeDiscoveryPreferStaticRvas(imageBase);
		return;
	}
	if (SdkPeDiscovery::scanFoundLive.load(std::memory_order_relaxed)
	    && SdkPeScanResultStillPlausible(imageBase))
		return;
	if (StaticGObjectsSlotLive(imageBase, 100))
		return;
	if (ReadUWorldFromImage(imageBase))
		return;
	if (SdkPeDiscovery::scanFoundLive.load(std::memory_order_relaxed)) {
		SdkPeDiscovery::GObjectRvaDiscovered.store(0, std::memory_order_relaxed);
		SdkPeDiscovery::UWorldRvaDiscovered.store(0, std::memory_order_relaxed);
		SdkPeDiscovery::scanFoundLive.store(false, std::memory_order_relaxed);
	}
	SdkPeDiscovery::discoveryFinished.store(false, std::memory_order_relaxed);
}

inline bool HasLiveGObjectsChunks(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	SdkPeDiscovery::EnsureStaticDiscoveryState(imageBase);
	const uintptr_t slot = imageBase + SdkPeDiscovery::GObjectsRva();
	ResolvedGObjects r{};
	if (!TryResolveGObjectsAtSlot(slot, r))
		return false;
	if (r.numElements < kSdkMinGObjectsElements || r.numElements > 5000000)
		return false;
	if (r.numChunks < 1 || r.numChunks > 64)
		return false;
	if (!r.chunkTable || !Memory::IsValid(r.chunkTable))
		return false;
	return ChunkTableLooksCanonical(r.chunkTable, r.numElements, r.numChunks);
}

inline bool LooksLikeGObjectsAtImageBase(uintptr_t imageBase, int32_t minElements = 100) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	SdkPeDiscovery::EnsureStaticDiscoveryState(imageBase);
	const uintptr_t slot = imageBase + SdkPeDiscovery::GObjectsRva();
	ResolvedGObjects r{};
	if (TryResolveGObjectsAtSlot(slot, r) && LooksLikeTUObjectArray(r.arrayBase, minElements))
		return true;
	return LooksLikeTUObjectArray(slot, minElements);
}

inline bool SdkUnpackedScanSkipped = false;
inline uint64_t ResolveEngineViaCommandNotRecognizedXref(bool allowDeferredScan);

inline int SdkUnpackedRegionsChecked = 0;
inline int SdkKernelRegionScanProbes = 0;
inline int32_t SdkKernelRegionScanBestNum = 0;
inline std::atomic<bool> fullMappingSearchLogged{false};
inline int SdkFullMappingCandidates = 0;
inline int32_t SdkFullMappingBestNumElements = 0;
inline uintptr_t SdkFullMappingBestBase = 0;

namespace SdkImageCache {
inline std::mutex resolveMutex;
inline uintptr_t cachedUnpacked = 0;
inline bool resolveLocked = false;
}

inline void ResetSdkImageBaseCache() {
	SdkImageCache::cachedUnpacked = 0;
	SdkImageCache::resolveLocked = false;
	UnpackedBase = 0;
	fullMappingSearchLogged.store(false, std::memory_order_relaxed);
}

inline void DisableGEngineStringScanForLiveSdk();

inline bool TryResolveSdkFromLargePeCandidate(uintptr_t base, int32_t* outNumElements = nullptr) {
	if (!base || !Memory::IsValid(base) || Read<uint16_t>(base) != 0x5A4D)
		return false;

	const bool gobjectsLive = LargePeStaticGObjectsLive(base, outNumElements);
	bool uworldLive = false;
	if (StaticUWorldRvaInImage(base)) {
		const uint64_t world = Read<uint64_t>(base + Offsets::UWorld);
		uworldLive = LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3);
	}
	if (!gobjectsLive && !uworldLive)
		return false;

	UnpackedBase = base;
	SdkImageCache::cachedUnpacked = base;
	if (gobjectsLive && StaticGObjectsRpmAtBase(base))
		SdkImageCache::resolveLocked = true;
	if (gobjectsLive) {
		SdkPeDiscoveryPreferStaticRvas(base);
		SdkPeDiscovery::LogStaticGlobalsReadable(base);
		if (!largePeSdkSuccessLogged.exchange(true, std::memory_order_relaxed)) {
			int32_t num = outNumElements ? *outNumElements : 0;
			if (!num)
				(void)LargePeStaticGObjectsLive(base, &num);
			std::cout << xorstr_("[+] large PE SDK base 0x") << std::hex << std::uppercase << base
			          << xorstr_(" GObjects NumElements ") << std::dec << num << std::endl;
			std::cout.flush();
		}
		if (uworldLive) {
			const uint64_t world = Read<uint64_t>(base + Offsets::UWorld);
			if (world && Memory::IsValid(world))
				DisableGEngineStringScanForLiveSdk();
		}
	} else {
		SdkPeDiscovery::staticRvasValidated.store(true, std::memory_order_relaxed);
		SdkPeDiscovery::discoveryFinished.store(true, std::memory_order_relaxed);
	}
	return true;
}

inline bool LooksLikeGObjectsAtImageBaseStrict(uintptr_t imageBase) {
	return LooksLikeGObjectsAtImageBase(imageBase, 10000);
}

inline SIZE_T SdkPeGObjectsSlotEndBytes(uintptr_t imageBaseForPe) {
	constexpr SIZE_T kSlotBytes = 0x20;
	uint64_t rva = SdkPeDiscovery::GObjectRvaDiscovered.load(std::memory_order_relaxed);
	if (!rva)
		rva = Offsets::GObjects;
	if (imageBaseForPe && StaticGObjectsRvaInImage(imageBaseForPe) && !rva)
		rva = Offsets::GObjects;
	uint32_t sizeOfImage = 0;
	std::vector<SdkPeSections::View> sections;
	if (imageBaseForPe && SdkPeSections::Parse(imageBaseForPe, sections, sizeOfImage) && sizeOfImage > 0x1000) {
		const SIZE_T need = static_cast<SIZE_T>(rva) + kSlotBytes;
		return need <= sizeOfImage ? need : static_cast<SIZE_T>(sizeOfImage);
	}
	return static_cast<SIZE_T>(rva) + kSlotBytes;
}

inline bool LiveGObjectsAtImageRva(uintptr_t imageBase, uint64_t rva, int32_t minElements) {
	if (!imageBase || !rva || !Memory::IsValid(imageBase))
		return false;
	const uintptr_t slot = imageBase + rva;
	if (!Memory::IsValid(slot))
		return false;
	int32_t num = 0;
	if (ReadFieldBestEffort(slot + TUObjectArrayLayout::NumElements, num) && num >= minElements
	    && num < 5000000)
		return true;
	ResolvedGObjects r{};
	if (!TryResolveGObjectsAtSlot(slot, r) || !r.numElementsReadOk)
		return false;
	const bool requireCanonical = minElements >= kSdkMinGObjectsElements;
	return r.numElements >= minElements
	       && LooksLikeTUObjectArray(r.arrayBase, minElements, requireCanonical);
}

inline bool ImageBaseGObjectsSlotStrictRpmOk(uintptr_t imageBase, int32_t minElements) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	const uint64_t discovered = SdkPeDiscovery::GObjectRvaDiscovered.load(std::memory_order_relaxed);
	uint64_t rva = discovered;
	if (!rva) {
		if (!StaticGObjectsRvaInImage(imageBase))
			return false;
		rva = Offsets::GObjects;
	}
	if (Read<uint16_t>(imageBase) == 0x5A4D) {
		uint32_t sizeOfImage = 0;
		std::vector<SdkPeSections::View> sections;
		if (SdkPeSections::Parse(imageBase, sections, sizeOfImage) && rva + 0x20 > sizeOfImage)
			return false;
	}
	return LiveGObjectsAtImageRva(imageBase, rva, minElements);
}

inline bool StaticGObjectsRpmAtBase(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	const uint64_t discovered = SdkPeDiscovery::GObjectRvaDiscovered.load(std::memory_order_relaxed);
	uint64_t rva = Offsets::GObjects;
	if (discovered && (!StaticGObjectsRvaInImage(imageBase) || !StaticGObjectsSlotTrusted(imageBase)))
		rva = discovered;
	else if (!StaticGObjectsRvaInImage(imageBase)) {
		if (!discovered)
			return false;
		rva = discovered;
	}
	const uintptr_t slot = imageBase + rva;
	int32_t num = 0;
	if (ReadFieldBestEffort(slot + TUObjectArrayLayout::NumElements, num) && num > 100
	    && num < 5000000)
		return true;
	const uintptr_t nested = slot + 0x10;
	if (ReadFieldBestEffort(nested + TUObjectArrayLayout::NumElements, num) && num > 100
	    && num < 5000000)
		return true;
	uint64_t indirect = 0;
	if (ReadFieldBestEffort(slot, indirect) && indirect && Memory::IsValid(indirect)) {
		if (ReadFieldBestEffort(static_cast<uintptr_t>(indirect) + TUObjectArrayLayout::NumElements,
		                        num)
		    && num > 100 && num < 5000000)
			return true;
	}
	return LiveGObjectsAtImageRva(imageBase, rva, 100);
}

inline bool PreferEngineDiscoveryPath() {
	if (!Baseadress)
		return false;
	if (SdkUnpackedScanSkipped)
		return true;
	if (IsPackedStubPeImage(Baseadress))
		return true;
	return StaticGObjectsSlotRegionUnusable(Baseadress) || !StaticGObjectsRpmAtBase(Baseadress);
}

inline std::atomic<bool> earlyUnpackedResolveLogged{false};
inline std::atomic<bool> regionGObjectsScanFailLogged{false};
inline std::atomic<bool> blindTuScanFailLogged{false};
inline std::atomic<int> blindTuScanRegionsWalked{0};
inline std::atomic<int> blindTuScanProbes{0};

inline void ResetSdkScanStateAfterKernelAttach() {
	ResetSdkImageBaseCache();
	ResetRpmHeaderDataProbeState();
	blindTuScanFailLogged.store(false, std::memory_order_relaxed);
	regionGObjectsScanFailLogged.store(false, std::memory_order_relaxed);
	earlyUnpackedResolveLogged.store(false, std::memory_order_relaxed);
	SdkKernelRegionScanProbes = 0;
	SdkKernelRegionScanBestNum = 0;
}

inline void LogKernelGObjectsSlotProbeOnce(uintptr_t imageBase) {
	static std::atomic<bool> logged{false};
	if (logged.exchange(true, std::memory_order_relaxed) || !Memory::Process.KernelAttached)
		return;
	const uintptr_t tryBases[] = {
		UnpackedBase,
		SdkImageCache::cachedUnpacked,
		imageBase,
		Baseadress,
	};
	int32_t num = 0;
	uintptr_t probedBase = 0;
	bool ok = false;
	for (uintptr_t base : tryBases) {
		if (!base)
			continue;
		num = 0;
		if (KernelGObjectsSlotDataReadable(base, &num)) {
			ok = true;
			probedBase = base;
			break;
		}
	}
	if (!ok && !probedBase)
		probedBase = imageBase ? imageBase : Baseadress;
	std::cout << xorstr_("[+] kernel read probe GObjects slot ")
	          << (ok ? xorstr_("ok") : xorstr_("fail"));
	if (ok)
		std::cout << xorstr_(" @ 0x") << std::hex << std::uppercase << probedBase << std::dec
		          << xorstr_(" NumElements ") << num;
	else if (probedBase) {
		std::cout << xorstr_(" @ 0x") << std::hex << std::uppercase << probedBase << std::dec;
		static std::atomic<bool> ioctlErrLogged{false};
		if (!ioctlErrLogged.exchange(true, std::memory_order_relaxed)) {
			if (Memory::Process.DriverReadLastFailureTargetInaccessible) {
				std::cout << xorstr_(" reason=target VA unmapped (driver STATUS_UNSUCCESSFUL, phys translate failed)");
				if (Memory::Process.DriverReadLastTargetVa)
					std::cout << xorstr_(" lastTarget=0x") << std::hex << std::uppercase
					          << Memory::Process.DriverReadLastTargetVa << std::dec;
			} else if (Memory::Process.DriverReadLastError) {
				std::cout << xorstr_(" reason=IOCTL/driver failure IOCTL GetLastError=") << std::dec
				          << Memory::Process.DriverReadLastError;
			}
		}
	}
	std::cout << std::endl;
	std::cout.flush();
}

inline bool OfficialGObjectsSlotLiveAtImageBase(uintptr_t imageBase, int32_t* outNum);
inline void LogUnpackedSdkImageFoundOnce(uintptr_t base, int32_t numElements, const char* viaTag);

inline SIZE_T GObjectsSlotEndBytesFromBase() {
	return static_cast<SIZE_T>(Offsets::GObjects) + 0x20;
}

inline bool CommittedRegionFitsGObjectsSlot(SIZE_T regionSize) {
	constexpr SIZE_T kMinFullImageBytes = 0x09400000ULL;
	return regionSize >= kMinFullImageBytes || regionSize >= GObjectsSlotEndBytesFromBase();
}

inline bool ImageBaseGObjectsSlotWithinRegion(uintptr_t imageBase, uintptr_t regionEnd) {
	const uintptr_t slotEnd = imageBase + GObjectsSlotEndBytesFromBase();
	return slotEnd <= regionEnd;
}

inline void LogKernelRegionScanStatsOnce(uintptr_t bestBase, int32_t bestNum) {
	std::cout << xorstr_("[+] kernel region scan stats: regions ")
	          << std::dec << SdkUnpackedRegionsChecked << xorstr_(" probes ")
	          << SdkKernelRegionScanProbes << xorstr_(" best NumElements ")
	          << SdkKernelRegionScanBestNum;
	if (bestBase)
		std::cout << xorstr_(" base 0x") << std::hex << std::uppercase << bestBase << std::dec;
	else
		std::cout << xorstr_(" (no match)");
	std::cout << std::endl;
	std::cout.flush();
}

inline uintptr_t FindImageBaseViaKernelRegionScan(int32_t* outNum = nullptr) {
	if (!Memory::Process.KernelAttached || !Memory::Process.Handle)
		return 0;

	SdkUnpackedRegionsChecked = 0;
	SdkKernelRegionScanProbes = 0;
	SdkKernelRegionScanBestNum = 0;
	uintptr_t bestBase = 0;
	int32_t bestNum = 0;

	uintptr_t cursor = 0;
	for (;;) {
		MEMORY_BASIC_INFORMATION mbi{};
		const SIZE_T infoSize =
			VirtualQueryEx(Memory::Process.Handle, reinterpret_cast<LPCVOID>(cursor), &mbi,
			               sizeof(mbi));
		if (!infoSize)
			break;

		++SdkUnpackedRegionsChecked;

		const uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
		const uintptr_t regionEnd = regionBase + mbi.RegionSize;
		if (regionEnd <= cursor)
			break;
		cursor = regionEnd;

		if (mbi.State != MEM_COMMIT)
			continue;
		if (!CommittedRegionFitsGObjectsSlot(mbi.RegionSize))
			continue;

		for (uintptr_t imageBase = regionBase; ImageBaseGObjectsSlotWithinRegion(imageBase, regionEnd);
		     imageBase += 0x10000) {
			++SdkKernelRegionScanProbes;
			int32_t num = 0;
			if (!OfficialGObjectsSlotLiveAtImageBase(imageBase, &num))
				continue;
			if (num > bestNum) {
				bestNum = num;
				bestBase = imageBase;
				SdkKernelRegionScanBestNum = bestNum;
			}
		}
	}

	LogKernelRegionScanStatsOnce(bestBase, bestNum);

	if (!bestBase)
		return 0;

	SdkFullMappingBestBase = bestBase;
	SdkFullMappingBestNumElements = bestNum;
	SdkFullMappingCandidates = 1;
	if (outNum)
		*outNum = bestNum;
	return bestBase;
}

inline uint64_t ResolveEngineViaGObjects();

namespace GEngineDiscovery {
inline void ResetAfterKernelAttach();
inline bool BootstrapStaticRvaPeProbe(uint64_t& outEngine);
inline void LogStringScanExhaustedIfNeeded();
inline bool TryPromoteEngineAfterStaticSdk(uint64_t& outEngine);
} // namespace GEngineDiscovery

inline bool TryConnectKernelForBlockedData(uintptr_t& inOutImageBase) {
	if (Memory::Process.KernelAttached)
		return true;
	uint16_t mz{};
	if (!Memory::Process.ReadRequestOk(inOutImageBase, mz) || mz != 0x5A4D)
		return false;
	int32_t numProbe{};
	const uintptr_t numAddr =
		inOutImageBase + Offsets::GObjects + TUObjectArrayLayout::NumElements;
	if (Memory::Process.ReadRequestOk(numAddr, numProbe))
		return false;
	if (!Memory::Process.ConnectKernelDriver()) {
		std::cout << xorstr_("[!] load driver.sys / run as admin") << std::endl;
		std::cout.flush();
		return false;
	}
	std::cout << xorstr_("[+] Kernel driver attached") << std::endl;
	std::cout.flush();
	ResetSdkScanStateAfterKernelAttach();
	const uintptr_t refreshed = Memory::ModuleBase();
	if (refreshed)
		inOutImageBase = refreshed;

	const uintptr_t kernelReportedBase =
		Memory::Process.GetBase(Memory::process_id);
	if (kernelReportedBase && kernelReportedBase != inOutImageBase) {
		int32_t kernelBaseNum = 0;
		if (OfficialGObjectsSlotLiveAtImageBase(kernelReportedBase, &kernelBaseNum)) {
			UnpackedBase = kernelReportedBase;
			SdkImageCache::cachedUnpacked = kernelReportedBase;
			LogUnpackedSdkImageFoundOnce(kernelReportedBase, kernelBaseNum,
			                             xorstr_("kernel GetBase"));
		}
	}

	const bool stubPacked =
		IsPackedStubPeImage(inOutImageBase) || StaticGObjectsSlotRegionUnusable(inOutImageBase);
	if (stubPacked) {
		SdkUnpackedScanSkipped = true;
		std::cout << xorstr_("[!] packed/stub module — deferring GObjects region scan; GEngine string path")
		          << std::endl;
		std::cout.flush();
	} else if (!UnpackedBase && !KernelGObjectsSlotDataReadable(inOutImageBase, nullptr)) {
		std::cout << xorstr_("[!] kernel attached but GObjects slot unmapped at packed+RVA; scanning...")
		          << std::endl;
		std::cout.flush();
		int32_t scanNum = 0;
		const uintptr_t scanned = FindImageBaseViaKernelRegionScan(&scanNum);
		if (scanned) {
			UnpackedBase = scanned;
			SdkImageCache::cachedUnpacked = scanned;
			LogUnpackedSdkImageFoundOnce(scanned, scanNum, xorstr_("kernel region scan"));
		}
	} else if (!UnpackedBase) {
		int32_t scanNum = 0;
		const uintptr_t scanned = FindImageBaseViaKernelRegionScan(&scanNum);
		if (scanned) {
			UnpackedBase = scanned;
			SdkImageCache::cachedUnpacked = scanned;
			LogUnpackedSdkImageFoundOnce(scanned, scanNum, xorstr_("kernel region scan"));
		}
	}

	LogKernelGObjectsSlotProbeOnce(inOutImageBase);

	if (Memory::Process.KernelAttached) {
		GEngineDiscovery::ResetAfterKernelAttach();
		uint64_t probeEngine = 0;
		(void)GEngineDiscovery::BootstrapStaticRvaPeProbe(probeEngine);
	}

	return true;
}

inline bool BlindTuObjectArrayShapeOk(uintptr_t arrayBase, int32_t* outNum = nullptr) {
	if (!arrayBase || !Memory::IsValid(arrayBase))
		return false;
	int32_t num = 0;
	const TUObjectArrayReject r =
		ClassifyTUObjectArray(arrayBase, 50000, true, &num);
	if (r != TUObjectArrayReject::Ok)
		return false;
	if (num > 500000)
		return false;
	if (outNum)
		*outNum = num;
	return true;
}

inline uintptr_t DeriveImageBaseFromTuObjectArray(uintptr_t arrayBase, int32_t* outNum = nullptr) {
	if (!BlindTuObjectArrayShapeOk(arrayBase, outNum))
		return 0;
	const uintptr_t tryBases[] = {
		arrayBase > Offsets::GObjects ? arrayBase - Offsets::GObjects : 0,
		arrayBase > Offsets::GObjects + 0x10 ? arrayBase - Offsets::GObjects - 0x10 : 0,
	};
	for (uintptr_t imageBase : tryBases) {
		if (!imageBase || !Memory::IsValid(imageBase))
			continue;
		int32_t num = 0;
		if (OfficialGObjectsSlotLiveAtImageBase(imageBase, &num)) {
			if (outNum)
				*outNum = num;
			return imageBase;
		}
	}
	return 0;
}

inline bool RegionProtectLikelyGameData(DWORD protect) {
	protect &= 0xFF;
	return protect == PAGE_READONLY || protect == PAGE_READWRITE
	       || protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE
	       || protect == PAGE_WRITECOPY || protect == PAGE_EXECUTE_WRITECOPY;
}

inline uintptr_t BlindScanTuObjectArray(int32_t* outNum = nullptr,
                                        DWORD timeBudgetMs = 2000) {
	blindTuScanRegionsWalked.store(0, std::memory_order_relaxed);
	blindTuScanProbes.store(0, std::memory_order_relaxed);
	HANDLE process = Memory::Process.Handle;
	if (!process)
		return 0;

	const bool unlimitedWalk = Memory::Process.KernelAttached;
	const std::chrono::steady_clock::time_point deadline =
		unlimitedWalk
			? (std::chrono::steady_clock::time_point::max)()
			: std::chrono::steady_clock::now() + std::chrono::milliseconds(timeBudgetMs);
	uintptr_t bestImage = 0;
	int32_t bestNum = 0;

	uintptr_t cursor = 0;
	for (;;) {
		if (std::chrono::steady_clock::now() >= deadline)
			break;
		MEMORY_BASIC_INFORMATION mbi{};
		const SIZE_T infoSize =
			VirtualQueryEx(process, reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi));
		if (!infoSize)
			break;

		blindTuScanRegionsWalked.fetch_add(1, std::memory_order_relaxed);

		const uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
		const uintptr_t regionEnd = regionBase + mbi.RegionSize;
		if (regionEnd <= cursor)
			break;
		cursor = regionEnd;

		if (mbi.State != MEM_COMMIT || !RegionProtectLikelyGameData(mbi.Protect))
			continue;
		if (mbi.RegionSize < 0x10000)
			continue;

		uintptr_t addr = (regionBase + 0xF) & ~static_cast<uintptr_t>(0xF);
		const uintptr_t end = regionEnd > 0x20 ? regionEnd - 0x20 : regionBase;
		for (; addr < end; addr += 0x1000) {
			if (std::chrono::steady_clock::now() >= deadline)
				break;
			blindTuScanProbes.fetch_add(1, std::memory_order_relaxed);
			if (!BlindTuObjectArrayShapeOk(addr, nullptr))
				continue;
			int32_t num = 0;
			uintptr_t imageBase = DeriveImageBaseFromTuObjectArray(addr, &num);
			if (!imageBase) {
				ResolvedGObjects r{};
				if (TryResolveGObjectsAtSlot(addr, r) && r.numElementsReadOk && r.numElements >= 50000
				    && r.numElements <= 500000
				    && LooksLikeTUObjectArray(r.arrayBase, 50000, true)) {
					SdkPeDiscovery::GObjectsSlotAbsolute.store(addr, std::memory_order_relaxed);
					num = r.numElements;
					imageBase = addr > Offsets::GObjects ? addr - Offsets::GObjects : 0;
				}
			}
			if (!imageBase)
				continue;
			if (num > bestNum) {
				bestNum = num;
				bestImage = imageBase;
			}
		}
	}

	if (!bestImage)
		return 0;
	SdkPeDiscovery::GObjectsSlotAbsolute.store(bestImage + Offsets::GObjects,
	                                           std::memory_order_relaxed);
	if (outNum)
		*outNum = bestNum;
	return bestImage;
}

inline void LogBlindTuScanRpmFailureOnce() {
	if (blindTuScanFailLogged.exchange(true, std::memory_order_relaxed))
		return;
	std::cout << xorstr_("[!] blind TUObjectArray scan: no match (regions ")
	          << std::dec << blindTuScanRegionsWalked.load(std::memory_order_relaxed)
	          << xorstr_(" probes ") << blindTuScanProbes.load(std::memory_order_relaxed)
	          << xorstr_(")") << std::endl;
	LogRpmHeaderVsDataProbeSummaryOnce();
	const auto& s = gRpmHeaderDataProbeStats;
	const bool headerReads = s.headerStrictOk > 0 || s.headerLooseOk > 0;
	const bool dataReads = s.dataStrictOk > 0 || s.dataLooseOk > 0;
	const bool kernelDataOk =
		Memory::Process.KernelAttached
		&& (KernelGObjectsSlotDataReadable(UnpackedBase)
		    || KernelGObjectsSlotDataReadable(Baseadress));
	const bool kernelScanResolved = UnpackedBase || SdkImageCache::cachedUnpacked;
	if (Memory::Process.KernelAttached && !kernelDataOk && !kernelScanResolved) {
		std::cout << xorstr_("[!] diagnosis: kernel attached; packed GObjects slot unmapped; "
		                      "full region scan found no TUObjectArray (cause B/C)")
		          << std::endl;
	} else if (Memory::Process.KernelAttached && (kernelDataOk || kernelScanResolved)) {
		std::cout << xorstr_("[!] diagnosis: kernel reads .data but no TUObjectArray match in blind scan "
		                      "(cause B image base or C static RVA)")
		          << std::endl;
	} else if (headerReads && !dataReads && !kernelDataOk) {
		std::cout << xorstr_("[!] diagnosis: OpenProcess RPM reads PE/header but not .data (cause A)")
		          << std::endl;
	} else if (!headerReads && !dataReads && !kernelDataOk) {
		std::cout << xorstr_("[!] diagnosis: RPM failed header and .data probes (cause A attach)")
		          << std::endl;
	} else if (dataReads || kernelDataOk) {
		std::cout << xorstr_("[!] diagnosis: RPM reads some .data but no TUObjectArray (cause B image base or C static RVA)")
		          << std::endl;
	}
	std::cout.flush();
}

inline bool OfficialGObjectsSlotLiveAtImageBase(uintptr_t imageBase, int32_t* outNum = nullptr) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return false;
	const uintptr_t slot = imageBase + Offsets::GObjects;
	if (!Memory::IsValid(slot))
		return false;
	uint64_t slotHead = 0;
	if (!ReadFieldBestEffort(slot, slotHead))
		return false;

	ResolvedGObjects r{};
	if (!TryResolveGObjectsAtSlot(slot, r) || !r.numElementsReadOk)
		return false;
	if (r.numElements < kSdkMinGObjectsElements || r.numElements > 5000000)
		return false;
	if (!LooksLikeTUObjectArray(r.arrayBase, kSdkMinGObjectsElements, true))
		return false;
	if (outNum)
		*outNum = r.numElements;
	return true;
}

inline uintptr_t FindUnpackedBaseViaCommittedRegionScan(int32_t* outNum = nullptr) {
	if (SdkUnpackedScanSkipped || (Baseadress && PreferEngineDiscoveryPath()))
		return 0;
	if (Memory::Process.KernelAttached) {
		const uintptr_t kernelFound = FindImageBaseViaKernelRegionScan(outNum);
		if (kernelFound)
			return kernelFound;
	}

	SdkUnpackedRegionsChecked = 0;
	HANDLE process = Memory::Process.Handle;
	if (!process)
		return 0;

	uintptr_t bestBase = 0;
	int32_t bestNum = 0;

	uintptr_t cursor = 0;
	for (;;) {
		MEMORY_BASIC_INFORMATION mbi{};
		const SIZE_T infoSize =
			VirtualQueryEx(process, reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi));
		if (!infoSize)
			break;

		++SdkUnpackedRegionsChecked;

		const uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
		const uintptr_t regionEnd = regionBase + mbi.RegionSize;
		if (regionEnd <= cursor)
			break;
		cursor = regionEnd;

		if (mbi.State != MEM_COMMIT)
			continue;
		if (!CommittedRegionFitsGObjectsSlot(mbi.RegionSize))
			continue;

		for (uintptr_t imageBase = regionBase; ImageBaseGObjectsSlotWithinRegion(imageBase, regionEnd);
		     imageBase += 0x10000) {
			int32_t num = 0;
			if (!OfficialGObjectsSlotLiveAtImageBase(imageBase, &num))
				continue;
			if (num > bestNum) {
				bestNum = num;
				bestBase = imageBase;
			}
		}
	}

	if (!bestBase) {
		SdkFullMappingCandidates = 0;
		SdkFullMappingBestNumElements = 0;
		SdkFullMappingBestBase = 0;
		return 0;
	}

	SdkFullMappingBestBase = bestBase;
	SdkFullMappingBestNumElements = bestNum;
	SdkFullMappingCandidates = 1;
	if (outNum)
		*outNum = bestNum;
	return bestBase;
}

inline uintptr_t FindUnpackedBaseViaLargeModules(int32_t* outNum = nullptr) {
	if (!Memory::process_id)
		return 0;
	const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
	                                               Memory::process_id);
	if (snap == INVALID_HANDLE_VALUE)
		return 0;

	uintptr_t found = 0;
	MODULEENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	if (Module32FirstW(snap, &entry)) {
		do {
			if (entry.modBaseSize < kFullUnpackedMinSizeOfImage)
				continue;
			const uintptr_t base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
			int32_t num = 0;
			if (OfficialGObjectsSlotLiveAtImageBase(base, &num)) {
				found = base;
				if (outNum)
					*outNum = num;
				break;
			}
		} while (Module32NextW(snap, &entry));
	}
	CloseHandle(snap);
	return found;
}

inline uintptr_t TryUnpackedBaseFromPackedGObjectsPointer(int32_t* outNum = nullptr) {
	if (!Baseadress)
		return 0;
	const uintptr_t slot = Baseadress + Offsets::GObjects;
	uint64_t indirect = 0;
	if (!Memory::Process.ReadUnchecked(slot, &indirect, sizeof(indirect)) || !indirect
	    || !Memory::IsValid(static_cast<uintptr_t>(indirect)))
		return 0;

	if (LooksLikeTUObjectArray(static_cast<uintptr_t>(indirect), kSdkMinGObjectsElements, true)) {
		if (indirect > Offsets::GObjects) {
			const uintptr_t candidate = static_cast<uintptr_t>(indirect) - Offsets::GObjects;
			if (OfficialGObjectsSlotLiveAtImageBase(candidate, outNum))
				return candidate;
		}
	}

	ResolvedGObjects r{};
	if (TryResolveGObjectsAtSlot(slot, r) && r.viaDereference && r.numElementsReadOk
	    && r.numElements >= kSdkMinGObjectsElements
	    && LooksLikeTUObjectArray(r.arrayBase, kSdkMinGObjectsElements, true)) {
		if (r.arrayBase > Offsets::GObjects) {
			const uintptr_t candidate = r.arrayBase - Offsets::GObjects;
			if (OfficialGObjectsSlotLiveAtImageBase(candidate, outNum))
				return candidate;
		}
	}
	return 0;
}

inline void LogUnpackedSdkImageFoundOnce(uintptr_t base, int32_t numElements, const char* viaTag) {
	if (!base || earlyUnpackedResolveLogged.exchange(true, std::memory_order_relaxed))
		return;
	std::cout << xorstr_("[+] SDK image base 0x") << std::hex << std::uppercase << base
	          << xorstr_(" (") << viaTag << xorstr_(") NumElements ") << std::dec << numElements
	          << std::endl;
	std::cout.flush();
}

inline void LogRegionGObjectsScanFailedOnce() {
	if (regionGObjectsScanFailLogged.exchange(true, std::memory_order_relaxed))
		return;
	std::cout << xorstr_("[!] no committed region contains readable GObjects at RVA 0x")
	          << std::hex << std::uppercase << Offsets::GObjects << xorstr_(".") << std::dec;
	if (Memory::Process.KernelAttached)
		std::cout << xorstr_(" (kernel scan regions ") << std::dec << SdkUnpackedRegionsChecked
		          << xorstr_(" probes ") << SdkKernelRegionScanProbes << xorstr_(" best NumElements ")
		          << SdkKernelRegionScanBestNum << xorstr_(")");
	std::cout << std::endl;
	std::cout.flush();
}

inline bool TryResolveUnpackedSdkImageEarly() {
	if (!Baseadress)
		return false;

	if (UnpackedBase) {
		int32_t existingNum = 0;
		if (OfficialGObjectsSlotLiveAtImageBase(UnpackedBase, &existingNum))
			return true;
	}

	int32_t numElements = 0;
	uintptr_t base = 0;
	const char* via = xorstr_("GEngine string path");
	if (PreferEngineDiscoveryPath()) {
		SdkUnpackedScanSkipped = true;
		if (UnpackedBase)
			base = UnpackedBase;
	} else {
		base = FindUnpackedBaseViaCommittedRegionScan(&numElements);
		via = Memory::Process.KernelAttached ? xorstr_("kernel region scan")
		                                     : xorstr_("region scan");
	}
	if (!base && !PreferEngineDiscoveryPath()) {
		LogRegionGObjectsScanFailedOnce();
		const DWORD blindBudget = Memory::Process.KernelAttached ? 0u : 2000u;
		base = BlindScanTuObjectArray(&numElements, blindBudget);
		via = xorstr_("blind TUObjectArray");
	}
	if (!base && !SdkUnpackedScanSkipped && !PreferEngineDiscoveryPath()) {
		LogBlindTuScanRpmFailureOnce();
	}
	if (!base) {
		base = FindUnpackedBaseViaLargeModules(&numElements);
		via = xorstr_("module scan");
	}
	if (!base) {
		base = TryUnpackedBaseFromPackedGObjectsPointer(&numElements);
		via = xorstr_("pointer chase");
	}
	if (!base)
		return false;

	UnpackedBase = base;
	SdkImageCache::cachedUnpacked = base;
	LogUnpackedSdkImageFoundOnce(base, numElements, via);
	return true;
}

inline uintptr_t FindFullUnpackedImageBase(bool /*strictGObjects*/) {
	int32_t num = 0;
	return FindUnpackedBaseViaCommittedRegionScan(&num);
}

inline uintptr_t FindUnpackedBaseViaVirtualQuery(bool strictGObjects) {
	(void)strictGObjects;
	return FindFullUnpackedImageBase(true);
}

inline uintptr_t ResolveUnpackedBaseFromDiscoveredGObjects() {
	if (LiveGObjectsAtImageRva(Baseadress, Offsets::GObjects, kSdkMinGObjectsElements))
		return Baseadress;
	uintptr_t fromRegions = FindUnpackedBaseViaVirtualQuery(true);
	if (!fromRegions)
		fromRegions = FindUnpackedBaseViaVirtualQuery(false);
	return fromRegions;
}

inline int32_t ReadGObjectsNumElements(uintptr_t imageBase) {
	if (!imageBase || !Memory::IsValid(imageBase))
		return 0;
	SdkPeDiscovery::EnsureStaticDiscoveryState(imageBase);
	const uintptr_t slot = imageBase + SdkPeDiscovery::GObjectsRva();
	ResolvedGObjects r{};
	(void)TryResolveGObjectsAtSlot(slot, r);
	if (!r.numElementsReadOk)
		return -1;
	return r.numElements;
}

// 14.60 RVAs are relative to the unpacked image; driver GetBase returns the packed MZ module.
inline bool SdkImageReadyAt(uintptr_t image);

inline uintptr_t ResolveSdkImageBase(bool resetCache = false) {
	std::lock_guard<std::mutex> resolveLock(SdkImageCache::resolveMutex);
	if (resetCache)
		ResetSdkImageBaseCache();
	if (!Baseadress)
		return 0;

	if (SdkImageCache::resolveLocked) {
		const uintptr_t base =
			SdkImageCache::cachedUnpacked ? SdkImageCache::cachedUnpacked : Baseadress;
		if (base && StaticGObjectsRpmAtBase(base)) {
			UnpackedBase = base;
			return base;
		}
		SdkImageCache::resolveLocked = false;
	}

	if (!resetCache && !SdkUnpackedScanSkipped)
		SdkUnpackedRegionsChecked = 0;

	auto commit = [&](uintptr_t base, bool lockResolve) {
		SdkImageCache::cachedUnpacked = base;
		SdkImageCache::resolveLocked = lockResolve && StaticGObjectsRpmAtBase(base);
		UnpackedBase = base;
		if (base && StaticGObjectsSlotLive(base, kSdkMinGObjectsElements))
			SdkPeDiscovery::LogStaticGlobalsReadable(base);
		return base;
	};

	SdkPeDiscovery::EnsureStaticDiscoveryState(Baseadress);

	const bool packedStub = IsPackedStubPeImage(Baseadress);
	const bool packedStaticUnusable =
		StaticGObjectsSlotRegionUnusable(Baseadress) || packedStub;
	const bool packedStaticRpm =
		!packedStaticUnusable && StaticGObjectsRpmAtBase(Baseadress);

	if (packedStaticUnusable) {
		SdkUnpackedScanSkipped = true;
		uintptr_t fullImage = UnpackedBase ? UnpackedBase : SdkImageCache::cachedUnpacked;
		if (!fullImage && PreferEngineDiscoveryPath()) {
			SdkPeDiscovery::peDataScanEnabled.store(true, std::memory_order_relaxed);
			return commit(fullImage ? fullImage : Baseadress, false);
		}
		if (!fullImage && !PreferEngineDiscoveryPath()) {
			fullImage = FindUnpackedBaseViaVirtualQuery(true);
			if (!fullImage)
				fullImage = FindUnpackedBaseViaVirtualQuery(false);
		}
		if (!fullImage && !PreferEngineDiscoveryPath() && !Memory::Process.KernelAttached) {
			int32_t blindNum = 0;
			fullImage = BlindScanTuObjectArray(&blindNum, 2000u);
			if (fullImage)
				LogUnpackedSdkImageFoundOnce(fullImage, blindNum, xorstr_("blind TUObjectArray"));
		}
		if (!fullImage && !PreferEngineDiscoveryPath())
			LogBlindTuScanRpmFailureOnce();
		if (fullImage
		    && (LiveGObjectsAtImageRva(fullImage, Offsets::GObjects, kSdkMinGObjectsElements)
		        || SdkPeDiscovery::GObjectsSlotAbsolute.load(std::memory_order_relaxed)))
			return commit(fullImage, true);
		SdkPeDiscovery::peDataScanEnabled.store(true, std::memory_order_relaxed);
	} else if (!packedStaticRpm)
		SdkPeDiscovery::peDataScanEnabled.store(true, std::memory_order_relaxed);

	if (SdkPeDiscovery::peDataScanEnabled.load(std::memory_order_relaxed))
		SdkPeDiscovery::EnsureDiscoveredOffsets(Baseadress);

	const uint64_t discoveredRva =
		SdkPeDiscovery::GObjectRvaDiscovered.load(std::memory_order_relaxed);

	if (packedStaticRpm && LooksLikeGObjectsAtImageBase(Baseadress, 100))
		return commit(Baseadress, true);

	// ponytail: cap unpacked pointer hunt in PE header + early image; no byte-step VA scan
	constexpr int kMaxUnpackedProbes = 64;
	int probes = 0;

	if (Read<uint16_t>(Baseadress) == 0x5A4D) {
		const uint32_t e_lfanew = Read<uint32_t>(Baseadress + 0x3C);
		const uintptr_t nt = Baseadress + e_lfanew;
		if (Read<uint32_t>(nt) == 0x00004550) {
			const uintptr_t scanEnd = Baseadress + 0x20000;
			for (uintptr_t addr = Baseadress + 0x1000;
			     addr + sizeof(uint64_t) <= scanEnd && probes < kMaxUnpackedProbes;
			     addr += 0x200) {
				++probes;
				const uint64_t cand = Read<uint64_t>(addr);
				if (cand < 0x100000000ULL || cand > 0x7FFFFFFFFFFFULL)
					continue;
				if ((cand & 0xFFF) != 0)
					continue;
				const uintptr_t base = static_cast<uintptr_t>(cand);
				if (base == Baseadress)
					continue;
				if (LooksLikeGObjectsAtImageBase(base, 100))
					return commit(base, true);
			}
		}
	}

	{
		const uintptr_t unpacked = ResolveUnpackedBaseFromDiscoveredGObjects();
		if (unpacked
		    && LiveGObjectsAtImageRva(unpacked, Offsets::GObjects, kSdkMinGObjectsElements))
			return commit(unpacked, true);
	}

	uintptr_t fromRegions = 0;
	if (!packedStaticRpm || !LooksLikeGObjectsAtImageBase(Baseadress, 100)) {
		fromRegions = FindUnpackedBaseViaVirtualQuery(true);
		if (!fromRegions)
			fromRegions = FindUnpackedBaseViaVirtualQuery(false);
	}
	if (fromRegions
	    && LiveGObjectsAtImageRva(fromRegions, Offsets::GObjects, kSdkMinGObjectsElements))
		return commit(fromRegions, true);

	const bool liveOnPacked = packedStaticRpm
	                          && (LooksLikeGObjectsAtImageBase(Baseadress, 100)
	                              || static_cast<bool>(ReadUWorldCandidateFromImage(Baseadress)));
	return commit(Baseadress, liveOnPacked);
}

inline ResolvedGObjects ResolveGObjectsArray() {
	(void)ResolveSdkImageBase();
	ResolvedGObjects r{};

	const uintptr_t slotAbsolute =
		SdkPeDiscovery::GObjectsSlotAbsolute.load(std::memory_order_relaxed);
	if (slotAbsolute && Memory::IsValid(slotAbsolute)) {
		(void)TryResolveGObjectsAtSlot(slotAbsolute, r);
		return r;
	}

	const uintptr_t imageBase = GlobalImageBase();
	if (!imageBase)
		return r;
	SdkPeDiscovery::EnsureStaticDiscoveryState(imageBase);
	const uintptr_t slot = imageBase + SdkPeDiscovery::GObjectsRva();

	if (!slot || !Memory::IsValid(slot))
		return r;

	(void)TryResolveGObjectsAtSlot(slot, r);
	return r;
}

inline uint64_t GObjectsObjectByIndex(const ResolvedGObjects& arr, int32_t index) {
	if (index < 0 || index >= arr.numElements || arr.numChunks <= 0)
		return 0;

	const int32_t chunkIndex = index / TUObjectArrayLayout::ElementsPerChunk;
	const int32_t inChunkIndex = index % TUObjectArrayLayout::ElementsPerChunk;
	if (chunkIndex >= arr.numChunks || !arr.chunkTable)
		return 0;

	const uint64_t chunk = Read<uint64_t>(
		arr.chunkTable + static_cast<uint64_t>(chunkIndex) * sizeof(uint64_t));
	if (!chunk)
		return 0;

	const uint64_t object = Read<uint64_t>(
		chunk + static_cast<uint64_t>(inChunkIndex) * TUObjectArrayLayout::FUObjectItemSize);
	if (!object || !Memory::IsValid(object))
		return 0;
	return object;
}

namespace GEngineDiscovery {
inline std::atomic<uint64_t> CachedEngine{0};
inline std::atomic<bool> DisableDeferredStringScan{false};
inline std::atomic<uintptr_t> CachedGlobalSlot{0};
inline std::atomic<uintptr_t> StringScanCursor{0};
inline std::atomic<bool> XrefSuccessLogged{false};
inline std::atomic<bool> ChainWorldValidLogged{false};

struct CommittedRegion {
	uintptr_t base = 0;
	size_t size = 0;
	DWORD protect = 0;
	bool exec = false;
};

inline std::vector<CommittedRegion> committedRegions;
inline std::vector<CommittedRegion> deferredRestRegions;
inline std::array<size_t, 4> tierRegionEnds{};
inline size_t eligibleRegionTotal = 0;
inline std::mutex regionCacheMutex;
inline std::atomic<bool> regionCacheBuilt{false};
inline std::atomic<int> stringScanRegionIdx{0};
inline std::atomic<int> stringScanRegionsChecked{0};
inline std::atomic<bool> stringScanPassComplete{false};
inline std::atomic<bool> stringHitLogged{false};
inline std::atomic<uintptr_t> pendingStringVa{0};
inline std::atomic<int> xrefExecRegionIdx{0};
inline std::atomic<bool> staticGEngineFallbackTried{false};
inline std::atomic<bool> restTierExpanded{false};
inline std::atomic<bool> sessionCacheLoadTried{false};
inline std::atomic<bool> largePeTierBootstrapDone{false};
inline std::atomic<bool> staticRvaKernelProbeDone{false};
inline std::atomic<bool> staticLargePeProbeOk{false};
inline std::atomic<bool> stringScanCompleteLogged{false};

constexpr int kStringScanTierCount = 4;
constexpr size_t kStringScanMinRegion = 0x10000;
constexpr size_t kRestTierBacklogCap = 2000;
constexpr int kOverlayStringRegionsPerTick = 120;

inline bool ProtectIsNoAccess(DWORD protect) {
	return (protect & 0xFF) == PAGE_NOACCESS;
}

inline bool IsCanonicalUserVa(uintptr_t va) {
	if (va < 0x10000)
		return false;
	if (va > 0x00007FFFFFFFFFFFULL)
		return false;
	if ((va >> 47) != 0)
		return false;
	return true;
}

inline bool CommandStringTerminatorOk(char c) {
	return c == '\0' || c == ' ' || c == '.' || c == '\r' || c == '\n' || c == ':';
}

inline bool RereadCommandNotRecognizedAt(uintptr_t va, bool* outWide = nullptr) {
	const char kUtf8[] = "Command not recognized";
	const size_t kUtf8Len = sizeof(kUtf8) - 1;
	char utf8[sizeof(kUtf8) + 2]{};
	if (Memory::Process.ReadUnchecked(va, utf8, static_cast<DWORD>(kUtf8Len + 2))) {
		if (std::memcmp(utf8, kUtf8, kUtf8Len) == 0 && CommandStringTerminatorOk(utf8[kUtf8Len])) {
			if (outWide)
				*outWide = false;
			return true;
		}
	}
	const wchar_t kWide[] = L"Command not recognized";
	const size_t wchars = std::wcslen(kWide);
	const size_t wbytes = wchars * sizeof(wchar_t);
	wchar_t wide[32]{};
	if (Memory::Process.ReadUnchecked(va, wide,
	                                   static_cast<DWORD>(wbytes + sizeof(wchar_t)))) {
		if (std::memcmp(wide, kWide, wbytes) == 0) {
			const wchar_t wterm = wide[wchars];
			if (wterm == L'\0' || wterm == L' ' || wterm == L'.') {
				if (outWide)
					*outWide = true;
				return true;
			}
		}
	}
	return false;
}

inline bool RegionContainsAddress(const CommittedRegion& r, uintptr_t addr) {
	return addr && r.base <= addr && addr < r.base + r.size;
}

inline std::atomic<bool> candidateRejectLoggingEnabled{true};
inline std::mutex candidateRejectLogMutex;
inline std::unordered_set<uintptr_t> candidateRejectLoggedOnce;
inline std::chrono::steady_clock::time_point candidateRejectRateWindowStart{};
inline int candidateRejectRateWindowCount = 0;

inline void SetCandidateRejectLogging(bool enabled) {
	candidateRejectLoggingEnabled.store(enabled, std::memory_order_relaxed);
}

inline void LogStringCandidateRejected(uintptr_t va, const char* reason) {
	if (!candidateRejectLoggingEnabled.load(std::memory_order_relaxed))
		return;
	{
		std::lock_guard<std::mutex> lock(candidateRejectLogMutex);
		if (!candidateRejectLoggedOnce.insert(va).second)
			return;
		const auto now = std::chrono::steady_clock::now();
		if (candidateRejectRateWindowCount == 0
		    || now - candidateRejectRateWindowStart > std::chrono::minutes(1)) {
			candidateRejectRateWindowStart = now;
			candidateRejectRateWindowCount = 0;
		}
		if (candidateRejectRateWindowCount >= 5)
			return;
		++candidateRejectRateWindowCount;
	}
	std::cout << xorstr_("[*] GEngine string candidate rejected (") << reason << xorstr_(" @ 0x")
	          << std::hex << std::uppercase << va << std::dec << xorstr_(")") << std::endl;
	std::cout.flush();
}

inline bool StringVaInPeDataLikeSection(uintptr_t imageBase, uintptr_t va) {
	if (!imageBase || va < imageBase)
		return true;
	std::vector<SdkPeSections::View> sections;
	uint32_t sizeOfImage = 0;
	if (!SdkPeSections::Parse(imageBase, sections, sizeOfImage))
		return true;
	const uint64_t rva = va - imageBase;
	for (const SdkPeSections::View& sec : sections) {
		if (!SdkPeSections::SectionIsDataLike(sec.name))
			continue;
		const uint64_t lo = sec.virtualAddress;
		const uint64_t hi = lo + (sec.virtualSize ? sec.virtualSize : 0x1000);
		if (rva >= lo && rva + 8 <= hi)
			return true;
	}
	return false;
}

inline void TryNoteUnpackedBaseFromPeRegion(const CommittedRegion& region) {
	if (UnpackedBase)
		return;
	if (Read<uint16_t>(region.base) != 0x5A4D)
		return;
	uint32_t sizeOfImage = 0;
	if (!ReadPeSizeOfImage(region.base, sizeOfImage))
		return;
	if (sizeOfImage >= kFullUnpackedMinSizeOfImage
	    || region.size >= 0x04000000ULL) {
		UnpackedBase = region.base;
		SdkImageCache::cachedUnpacked = region.base;
	}
}

inline bool AcceptStringHit(uintptr_t va, const CommittedRegion& region, int regionTier = -1) {
	if (!IsCanonicalUserVa(va)) {
		LogStringCandidateRejected(va, xorstr_("non-canonical user VA"));
		return false;
	}
	if (ProtectIsNoAccess(region.protect)) {
		LogStringCandidateRejected(va, xorstr_("PAGE_NOACCESS region"));
		return false;
	}
	if (!RegionContainsAddress(region, va)) {
		LogStringCandidateRejected(va, xorstr_("VA outside region"));
		return false;
	}
	if (!RereadCommandNotRecognizedAt(va)) {
		LogStringCandidateRejected(va, xorstr_("phrase re-read failed"));
		return false;
	}
	if ((regionTier == 0 || regionTier == 1) && Read<uint16_t>(region.base) == 0x5A4D) {
		if (!StringVaInPeDataLikeSection(region.base, va)) {
			LogStringCandidateRejected(va, xorstr_("outside PE data section"));
			return false;
		}
	}
	return true;
}

inline bool GlobalSlotLooksPlausible(uintptr_t slot) {
	if (!IsCanonicalUserVa(slot))
		return false;
	if (slot & 7)
		return false;
	return Memory::IsValid(slot);
}

inline size_t TierRegionCount(int tier0) {
	if (tier0 < 0 || tier0 > 3)
		return 0;
	if (tier0 == 0)
		return tierRegionEnds[0];
	return tierRegionEnds[static_cast<size_t>(tier0)]
	       - tierRegionEnds[static_cast<size_t>(tier0 - 1)];
}

inline int ClassifyStringScanTier(const CommittedRegion& r, uintptr_t moduleBase) {
	if (RegionContainsAddress(r, moduleBase))
		return 0;
	if (r.size >= 0x04000000ULL && Read<uint16_t>(r.base) == 0x5A4D)
		return 1;
	const DWORD p = r.protect & 0xFF;
	if ((p == PAGE_READONLY || p == PAGE_EXECUTE_READ) && r.size >= kStringScanMinRegion
	    && r.size <= 0x1000000ULL)
		return 2;
	return 3;
}

inline bool RegionQuickReadable(const CommittedRegion& r) {
	uint32_t probe{};
	return Memory::Process.ReadUnchecked(r.base, &probe, static_cast<DWORD>(sizeof(probe)));
}

inline int ActiveTierCountForLog() {
	return kStringScanTierCount;
}

inline int TierIndexForRegion(int regionIdx) {
	if (regionIdx < 0)
		return 0;
	for (int t = 0; t < 4; ++t) {
		if (regionIdx < static_cast<int>(tierRegionEnds[static_cast<size_t>(t)]))
			return t;
	}
	return 3;
}

inline bool ProtectIsExecutable(DWORD protect) {
	protect &= 0xFF;
	return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ
	       || protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

inline void InjectModuleBaseTier0(std::array<std::vector<CommittedRegion>, 4>& tiers,
                                  uintptr_t moduleBase) {
	if (!moduleBase)
		return;
	const auto tierAlreadyCovers = [&](int tier) {
		for (const CommittedRegion& r : tiers[static_cast<size_t>(tier)]) {
			if (RegionContainsAddress(r, moduleBase))
				return true;
		}
		return false;
	};
	if (tierAlreadyCovers(0))
		return;
	for (int t = 1; t < 4; ++t) {
		if (tierAlreadyCovers(t))
			return;
	}
	HANDLE process = Memory::Process.Handle;
	if (process) {
		MEMORY_BASIC_INFORMATION mbi{};
		if (VirtualQueryEx(process, reinterpret_cast<LPCVOID>(moduleBase), &mbi, sizeof(mbi))
		    && mbi.State == MEM_COMMIT && !ProtectIsNoAccess(mbi.Protect)) {
			CommittedRegion r{};
			r.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
			r.size = static_cast<size_t>(mbi.RegionSize);
			r.protect = mbi.Protect;
			r.exec = ProtectIsExecutable(mbi.Protect);
			tiers[0].push_back(r);
			return;
		}
	}
	uint32_t sizeOfImage = 0;
	if (ReadPeSizeOfImage(moduleBase, sizeOfImage) && sizeOfImage > 0x1000) {
		CommittedRegion r{};
		r.base = moduleBase;
		r.size = sizeOfImage;
		r.protect = PAGE_EXECUTE_READ;
		r.exec = true;
		tiers[0].push_back(r);
	}
}

inline void ExpandDeferredRestRegionsLocked() {
	if (DisableDeferredStringScan.load(std::memory_order_relaxed))
		return;
	if (restTierExpanded.load(std::memory_order_relaxed) || deferredRestRegions.empty())
		return;
	const size_t before = committedRegions.size();
	committedRegions.insert(committedRegions.end(), deferredRestRegions.begin(),
	                        deferredRestRegions.end());
	tierRegionEnds[3] = committedRegions.size();
	eligibleRegionTotal = committedRegions.size();
	deferredRestRegions.clear();
	restTierExpanded.store(true, std::memory_order_release);
	std::cout << xorstr_("[*] GEngine string scan: expanding low-priority backlog (+")
	          << (committedRegions.size() - before) << xorstr_(" regions)") << std::endl;
	std::cout.flush();
}

inline void AppendModuleRegionsFallback() {
	if (!Memory::process_id)
		return;
	const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
	                                               Memory::process_id);
	if (snap == INVALID_HANDLE_VALUE)
		return;
	MODULEENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	if (Module32FirstW(snap, &entry)) {
		do {
			CommittedRegion r{};
			r.base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
			r.size = entry.modBaseSize;
			r.protect = PAGE_EXECUTE_READWRITE;
			r.exec = true;
			committedRegions.push_back(r);
		} while (Module32NextW(snap, &entry));
	}
	CloseHandle(snap);
}

inline void EnsureCommittedRegionCache() {
	if (regionCacheBuilt.load(std::memory_order_acquire))
		return;
	std::lock_guard<std::mutex> lock(regionCacheMutex);
	if (regionCacheBuilt.load(std::memory_order_relaxed))
		return;
	committedRegions.clear();
	deferredRestRegions.clear();
	tierRegionEnds = {};
	eligibleRegionTotal = 0;
	restTierExpanded.store(false, std::memory_order_relaxed);

	std::array<std::vector<CommittedRegion>, 4> tiers{};
	size_t vqWalked = 0;
	size_t skippedIneligible = 0;
	const uintptr_t moduleBase = Baseadress;

	HANDLE process = Memory::Process.Handle;
	if (process) {
		uintptr_t cursor = 0;
		for (;;) {
			MEMORY_BASIC_INFORMATION mbi{};
			const SIZE_T infoSize =
				VirtualQueryEx(process, reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi));
			if (!infoSize)
				break;
			const uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
			const uintptr_t regionEnd = regionBase + mbi.RegionSize;
			if (regionEnd <= cursor)
				break;
			cursor = regionEnd;
			++vqWalked;
			if (mbi.State != MEM_COMMIT)
				continue;
			if (ProtectIsNoAccess(mbi.Protect))
				continue;
			if (mbi.RegionSize < 0x1000)
				continue;
			const bool coversModule =
				moduleBase && regionBase <= moduleBase && moduleBase < regionEnd;
			if (mbi.RegionSize < kStringScanMinRegion && !coversModule)
				continue;
			if (!RegionProtectLikelyGameData(mbi.Protect))
				continue;

			CommittedRegion r{};
			r.base = regionBase;
			r.size = static_cast<size_t>(mbi.RegionSize);
			r.protect = mbi.Protect;
			r.exec = ProtectIsExecutable(mbi.Protect);
			const int tier = ClassifyStringScanTier(r, moduleBase);
			if (tier == 1 && Read<uint16_t>(regionBase) == 0x5A4D) {
				uint32_t sizeOfImage = 0;
				if (ReadPeSizeOfImage(regionBase, sizeOfImage)) {
					std::cout << xorstr_("[*] large PE candidate base 0x") << std::hex
					          << std::uppercase << regionBase << xorstr_(" SizeOfImage 0x")
					          << sizeOfImage << std::dec << std::endl;
					std::cout.flush();
				}
			}
			tiers[static_cast<size_t>(tier)].push_back(r);
		}
	}

	InjectModuleBaseTier0(tiers, moduleBase);

	for (size_t t = 0; t < 4; ++t) {
		if (t == 3) {
			size_t capUsed = 0;
			for (const CommittedRegion& r : tiers[3]) {
				if (capUsed < kRestTierBacklogCap) {
					committedRegions.push_back(r);
					++capUsed;
				} else {
					deferredRestRegions.push_back(r);
				}
			}
		} else {
			for (const CommittedRegion& r : tiers[t])
				committedRegions.push_back(r);
		}
		tierRegionEnds[t] = committedRegions.size();
	}

	skippedIneligible = vqWalked > eligibleRegionTotal ? vqWalked - eligibleRegionTotal : 0;
	eligibleRegionTotal =
		committedRegions.size() + deferredRestRegions.size();

	if (committedRegions.empty())
		AppendModuleRegionsFallback();

	std::cout << xorstr_("[*] GEngine string scan: eligible ") << std::dec << eligibleRegionTotal
	          << xorstr_(" regions (tier1=") << tierRegionEnds[0] << xorstr_(" tier2=")
	          << (tierRegionEnds[1] - tierRegionEnds[0]) << xorstr_(" tier3=")
	          << (tierRegionEnds[2] - tierRegionEnds[1]) << xorstr_(" rest=")
	          << (tierRegionEnds[3] - tierRegionEnds[2]);
	if (!deferredRestRegions.empty())
		std::cout << xorstr_(" deferred=") << deferredRestRegions.size();
	std::cout << xorstr_(")") << std::endl;
	std::cout.flush();
	(void)skippedIneligible;
	regionCacheBuilt.store(true, std::memory_order_release);
}

inline bool EngineChainWorldValid(uint64_t engine, uint64_t* outWorld = nullptr) {
	if (!engine || !Memory::IsValid(engine))
		return false;
	const uint64_t world = UWorldFromEngine(engine);
	if (!world || !Memory::IsValid(world))
		return false;
	if (!LooksLikeUWorld(world) && !LooksLikeUWorldRelaxed(world, 3))
		return false;
	if (outWorld)
		*outWorld = world;
	return true;
}

inline bool EngineDiscoveryChainReady() {
	const uint64_t eng = CachedEngine.load(std::memory_order_relaxed);
	if (eng && EngineChainWorldValid(eng))
		return true;
	if (!XrefSuccessLogged.load(std::memory_order_relaxed))
		return false;
	if (!eng || !EngineChainWorldValid(eng))
		return false;
	return ChainWorldValidLogged.load(std::memory_order_relaxed);
}

inline void ResetAfterKernelAttach() {
	regionCacheBuilt.store(false, std::memory_order_relaxed);
	largePeTierBootstrapDone.store(false, std::memory_order_relaxed);
	staticRvaKernelProbeDone.store(false, std::memory_order_relaxed);
	stringScanPassComplete.store(false, std::memory_order_relaxed);
	stringScanCompleteLogged.store(false, std::memory_order_relaxed);
	staticLargePeProbeOk.store(false, std::memory_order_relaxed);
	stringScanRegionIdx.store(0, std::memory_order_relaxed);
	stringScanRegionsChecked.store(0, std::memory_order_relaxed);
	staticGEngineFallbackTried.store(false, std::memory_order_relaxed);
	restTierExpanded.store(false, std::memory_order_relaxed);
	pendingStringVa.store(0, std::memory_order_relaxed);
	xrefExecRegionIdx.store(0, std::memory_order_relaxed);
	std::lock_guard<std::mutex> lock(regionCacheMutex);
	committedRegions.clear();
	deferredRestRegions.clear();
	tierRegionEnds = {};
	eligibleRegionTotal = 0;
}

inline bool TryPromoteEngineAfterStaticSdk(uint64_t& outEngine) {
	const uintptr_t sdkImage = GlobalImageBase();
	if (sdkImage) {
		const uint64_t directWorld = ReadUWorldFromImage(sdkImage);
		if (directWorld && Memory::IsValid(directWorld)) {
			DisableGEngineStringScanForLiveSdk();
			return true;
		}
	}
	const uint64_t cached = CachedEngine.load(std::memory_order_relaxed);
	if (cached && EngineChainWorldValid(cached)) {
		outEngine = cached;
		return true;
	}
	const uint64_t eng = ResolveEngineViaGObjects();
	if (!eng || !EngineChainWorldValid(eng))
		return false;
	CachedEngine.store(eng, std::memory_order_relaxed);
	if (!ChainWorldValidLogged.exchange(true, std::memory_order_relaxed)) {
		std::cout << xorstr_("[+] GEngine chain World valid") << std::endl;
		std::cout.flush();
	}
	outEngine = eng;
	return true;
}

inline bool BootstrapStaticRvaPeProbe(uint64_t& outEngine) {
	if (staticRvaKernelProbeDone.exchange(true, std::memory_order_relaxed))
		return UnpackedBase != 0;
	EnsureCommittedRegionCache();
	const int tier1End = static_cast<int>(tierRegionEnds[1]);
	for (int i = 0; i < tier1End; ++i) {
		const CommittedRegion& r = committedRegions[static_cast<size_t>(i)];
		if (Read<uint16_t>(r.base) != 0x5A4D)
			continue;
		if (!RegionQuickReadable(r))
			continue;
		int32_t num = 0;
		if (TryResolveSdkFromLargePeCandidate(r.base, &num)) {
			staticLargePeProbeOk.store(true, std::memory_order_relaxed);
			(void)TryPromoteEngineAfterStaticSdk(outEngine);
			return true;
		}
	}
	return UnpackedBase != 0;
}

inline void LogStringScanExhaustedIfNeeded() {
	if (!stringScanPassComplete.load(std::memory_order_relaxed))
		return;
	if (stringScanCompleteLogged.exchange(true, std::memory_order_relaxed))
		return;
	const uintptr_t sdkImage = UnpackedBase ? UnpackedBase : SdkImageCache::cachedUnpacked;
	const bool staticOk = staticLargePeProbeOk.load(std::memory_order_relaxed)
	                      || (sdkImage && HasLiveGObjectsChunks(sdkImage));
	const uint64_t eng = CachedEngine.load(std::memory_order_relaxed);
	const bool engineOk = eng && EngineChainWorldValid(eng);
	std::cout << xorstr_("[!] GEngine string scan complete — no hit; static RVA on large PE: ")
	          << (staticOk || engineOk ? xorstr_("ok") : xorstr_("fail")) << std::endl;
	std::cout.flush();
}

inline uintptr_t PeImageBaseContaining(uintptr_t addr) {
	if (!addr)
		return 0;
	uintptr_t page = addr & ~static_cast<uintptr_t>(0xFFF);
	for (int i = 0; i < 0x4000; ++i) {
		if (!Memory::IsValid(page))
			break;
		if (Read<uint16_t>(page) == 0x5A4D)
			return page;
		if (page < 0x10000)
			break;
		page -= 0x1000;
	}
	return 0;
}

struct GEngineSessionCacheBlob {
	uint32_t magic = 0x474E5853u;
	uint32_t version = 1;
	uint64_t cacheKey = 0;
	uint64_t stringVa = 0;
	uint64_t gengineVa = 0;
	uint64_t globalSlot = 0;
	uint64_t imageBase = 0;
};

inline uint64_t GEngineSessionCacheKey() {
	uint64_t key = Baseadress;
	if (!Baseadress)
		return key;
	const uint32_t e_lfanew = Read<uint32_t>(Baseadress + 0x3C);
	if (Memory::IsValid(Baseadress + e_lfanew)) {
		const uintptr_t nt = Baseadress + e_lfanew;
		if (Read<uint32_t>(nt) == 0x00004550) {
			key ^= static_cast<uint64_t>(Read<uint32_t>(nt + 0x50)) << 32;
			key ^= static_cast<uint64_t>(Read<uint32_t>(nt + 0x58));
		}
	}
	return key;
}

inline std::wstring GEngineSessionCachePath() {
	wchar_t tmp[MAX_PATH]{};
	const DWORD n = GetTempPathW(MAX_PATH, tmp);
	if (!n || n >= MAX_PATH)
		return L"";
	return std::wstring(tmp) + L"senex_retrac_gengine_v1.bin";
}

inline void SaveGEngineSessionCache(uint64_t engine, uintptr_t globalSlot, uintptr_t stringAddr,
                                    uintptr_t imageBase) {
	const std::wstring path = GEngineSessionCachePath();
	if (path.empty())
		return;
	GEngineSessionCacheBlob blob{};
	blob.cacheKey = GEngineSessionCacheKey();
	blob.stringVa = stringAddr;
	blob.gengineVa = engine;
	blob.globalSlot = globalSlot;
	blob.imageBase = imageBase;
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (out)
		out.write(reinterpret_cast<const char*>(&blob), sizeof(blob));
}

inline void CommitEngineSuccess(uint64_t engine, uintptr_t globalSlot, uintptr_t stringAddr) {
	CachedEngine.store(engine, std::memory_order_relaxed);
	CachedGlobalSlot.store(globalSlot, std::memory_order_relaxed);
	uintptr_t image = PeImageBaseContaining(stringAddr);
	if (!image && globalSlot)
		image = PeImageBaseContaining(globalSlot);
	if (image && IsFullUnpackedPeImage(image)) {
		UnpackedBase = image;
		SdkImageCache::cachedUnpacked = image;
	}
	SaveGEngineSessionCache(engine, globalSlot, stringAddr, image);
	if (stringAddr && !stringHitLogged.exchange(true, std::memory_order_relaxed)) {
		std::cout << xorstr_("[+] GEngine string @ 0x") << std::hex << std::uppercase << stringAddr
		          << std::dec << std::endl;
		std::cout.flush();
	}
	if (!XrefSuccessLogged.exchange(true, std::memory_order_relaxed)) {
		std::cout << xorstr_("[+] GEngine via Command-not-recognized xref") << std::endl;
		std::cout.flush();
	}
	if (EngineChainWorldValid(engine) && !ChainWorldValidLogged.exchange(true, std::memory_order_relaxed)) {
		std::cout << xorstr_("[+] GEngine chain World valid") << std::endl;
		std::cout.flush();
	}
}

inline bool TryLoadGEngineSessionCache(uint64_t& outEngine) {
	if (sessionCacheLoadTried.exchange(true, std::memory_order_relaxed))
		return false;
	const std::wstring path = GEngineSessionCachePath();
	if (path.empty())
		return false;
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	GEngineSessionCacheBlob blob{};
	in.read(reinterpret_cast<char*>(&blob), sizeof(blob));
	if (!in || blob.magic != 0x474E5853u || blob.version != 1)
		return false;
	if (blob.cacheKey != GEngineSessionCacheKey())
		return false;
	if (!IsCanonicalUserVa(static_cast<uintptr_t>(blob.stringVa))) {
		LogStringCandidateRejected(static_cast<uintptr_t>(blob.stringVa),
		                           xorstr_("session cache non-canonical string VA"));
		return false;
	}
	if (!blob.globalSlot || !GlobalSlotLooksPlausible(static_cast<uintptr_t>(blob.globalSlot)))
		return false;
	const char kUtf8[] = "Command not recognized";
	char probe[sizeof(kUtf8)]{};
	if (!Memory::Process.ReadUnchecked(static_cast<uintptr_t>(blob.stringVa), probe,
	                                   static_cast<DWORD>(sizeof(kUtf8) - 1)))
		return false;
	if (std::memcmp(probe, kUtf8, sizeof(kUtf8) - 1) != 0)
		return false;
	const uint64_t engine = Read<uint64_t>(blob.globalSlot);
	if (!EngineChainWorldValid(engine))
		return false;
	outEngine = engine;
	CommitEngineSuccess(engine, static_cast<uintptr_t>(blob.globalSlot),
	                  static_cast<uintptr_t>(blob.stringVa));
	if (blob.imageBase && Memory::IsValid(blob.imageBase)) {
		UnpackedBase = static_cast<uintptr_t>(blob.imageBase);
		SdkImageCache::cachedUnpacked = UnpackedBase;
	}
	std::cout << xorstr_("[+] GEngine session cache hit") << std::endl;
	std::cout.flush();
	return true;
}

inline bool BufferContainsUtf8(const uint8_t* data, size_t len, const char* needle, size_t needleLen) {
	if (!needleLen || len < needleLen)
		return false;
	for (size_t i = 0; i + needleLen <= len; ++i) {
		if (std::memcmp(data + i, needle, needleLen) == 0)
			return true;
	}
	return false;
}

inline bool BufferContainsWide(const uint8_t* data, size_t len, const wchar_t* needle) {
	const size_t needleBytes = std::wcslen(needle) * sizeof(wchar_t);
	if (!needleBytes || len < needleBytes)
		return false;
	for (size_t i = 0; i + needleBytes <= len; i += 2) {
		if (std::memcmp(data + i, needle, needleBytes) == 0)
			return true;
	}
	return false;
}

inline bool ScanSectionForCommandString(uintptr_t imageBase, const SdkPeSections::View& sec,
                                        uintptr_t& outStringAddr) {
	const char kUtf8[] = "Command not recognized";
	const size_t kUtf8Len = sizeof(kUtf8) - 1;
	const wchar_t kWide[] = L"Command not recognized";

	const uintptr_t secBase = imageBase + sec.virtualAddress;
	const size_t secSize = sec.virtualSize ? sec.virtualSize : 0x1000;
	if (!Memory::IsValid(secBase))
		return false;

	constexpr size_t kChunk = 0x8000;
	std::vector<uint8_t> buf(kChunk);
	for (size_t off = 0; off < secSize; off += kChunk - 64) {
		const size_t readSize = (off + kChunk > secSize) ? (secSize - off) : kChunk;
		if (readSize < kUtf8Len)
			break;
		if (!Memory::Process.ReadUnchecked(secBase + off, buf.data(),
		                                   static_cast<DWORD>(readSize)))
			continue;
		if (BufferContainsUtf8(buf.data(), readSize, kUtf8, kUtf8Len)) {
			for (size_t i = 0; i + kUtf8Len <= readSize; ++i) {
				if (std::memcmp(buf.data() + i, kUtf8, kUtf8Len) == 0) {
					const uintptr_t candidate = secBase + off + i;
					CommittedRegion secRegion{};
					secRegion.base = secBase;
					secRegion.size = secSize;
					secRegion.protect = PAGE_READONLY;
					if (AcceptStringHit(candidate, secRegion, 1)) {
						outStringAddr = candidate;
						return true;
					}
				}
			}
		}
		if (BufferContainsWide(buf.data(), readSize, kWide)) {
			const size_t wbytes = std::wcslen(kWide) * sizeof(wchar_t);
			for (size_t i = 0; i + wbytes <= readSize; i += 2) {
				if (std::memcmp(buf.data() + i, kWide, wbytes) == 0) {
					const uintptr_t candidate = secBase + off + i;
					CommittedRegion secRegion{};
					secRegion.base = secBase;
					secRegion.size = secSize;
					secRegion.protect = PAGE_READONLY;
					if (AcceptStringHit(candidate, secRegion, 1)) {
						outStringAddr = candidate;
						return true;
					}
				}
			}
		}
	}
	return false;
}

inline bool ScanRegionRawChunksForStringHit(const CommittedRegion& r, uintptr_t& outHit) {
	const char kUtf8[] = "Command not recognized";
	const size_t kUtf8Len = sizeof(kUtf8) - 1;
	const wchar_t kWide[] = L"Command not recognized";
	constexpr size_t kChunk = 0x10000;
	std::vector<uint8_t> buf(kChunk);
	for (size_t off = 0; off < r.size; off += kChunk - 64) {
		const size_t readSize = (off + kChunk > r.size) ? (r.size - off) : kChunk;
		if (readSize < kUtf8Len)
			break;
		if (!Memory::Process.ReadUnchecked(r.base + off, buf.data(),
		                                   static_cast<DWORD>(readSize)))
			continue;
		for (size_t i = 0; i + kUtf8Len <= readSize; ++i) {
			if (std::memcmp(buf.data() + i, kUtf8, kUtf8Len) != 0)
				continue;
			const uintptr_t candidate = r.base + off + i;
			if (RereadCommandNotRecognizedAt(candidate)) {
				outHit = candidate;
				return true;
			}
		}
		const size_t wbytes = std::wcslen(kWide) * sizeof(wchar_t);
		for (size_t i = 0; i + wbytes <= readSize; i += 2) {
			if (std::memcmp(buf.data() + i, kWide, wbytes) != 0)
				continue;
			const uintptr_t candidate = r.base + off + i;
			if (RereadCommandNotRecognizedAt(candidate)) {
				outHit = candidate;
				return true;
			}
		}
	}
	return false;
}

inline bool ScanRegionForStringHit(const CommittedRegion& r, uintptr_t& outHit) {
	outHit = 0;
	if (r.size < sizeof("Command not recognized") - 1)
		return false;
	if (Read<uint16_t>(r.base) == 0x5A4D && r.size >= 0x04000000ULL) {
		std::vector<SdkPeSections::View> sections;
		uint32_t sizeOfImage = 0;
		if (SdkPeSections::Parse(r.base, sections, sizeOfImage)) {
			for (const SdkPeSections::View& sec : sections) {
				if (!SdkPeSections::SectionIsDataLike(sec.name))
					continue;
				if (ScanSectionForCommandString(r.base, sec, outHit))
					return true;
			}
			return false;
		}
	}
	return ScanRegionRawChunksForStringHit(r, outHit);
}

inline bool RipTargetAt(const uint8_t* code, uintptr_t instrAddr, uintptr_t& outTarget) {
	if ((code[0] != 0x48 && code[0] != 0x4C) || code[1] != 0x8D)
		return false;
	const int32_t disp = *reinterpret_cast<const int32_t*>(code + 3);
	const uintptr_t rip = instrAddr + 7;
	outTarget = rip + static_cast<uintptr_t>(disp);
	return true;
}

inline bool MovRipGlobalAt(const uint8_t* code, uintptr_t instrAddr, uintptr_t& outGlobalSlot) {
	if ((code[0] != 0x48 && code[0] != 0x4C) || code[1] != 0x8B)
		return false;
	const uint8_t modrm = code[2];
	if ((modrm & 0xC7) != 0x05)
		return false;
	const int32_t disp = *reinterpret_cast<const int32_t*>(code + 3);
	const uintptr_t rip = instrAddr + 7;
	outGlobalSlot = rip + static_cast<uintptr_t>(disp);
	return true;
}

inline bool TryResolveGEngineNearXref(uintptr_t xrefInstr, uintptr_t stringAddr, uint64_t& outEngine,
                                      uintptr_t& outGlobalSlot) {
	uint8_t window[1024]{};
	const uintptr_t winBase = xrefInstr > 512 ? xrefInstr - 512 : xrefInstr;
	if (!Memory::Process.ReadUnchecked(winBase, window, sizeof(window)))
		return false;
	const size_t relXref = xrefInstr - winBase;

	for (size_t off = 0; off + 7 <= sizeof(window); ++off) {
		uintptr_t globalSlot = 0;
		if (!MovRipGlobalAt(window + off, winBase + off, globalSlot))
			continue;
		if (!GlobalSlotLooksPlausible(globalSlot))
			continue;
		const uint64_t engine = Read<uint64_t>(globalSlot);
		if (!IsCanonicalUserVa(static_cast<uintptr_t>(engine)))
			continue;
		if (!EngineChainWorldValid(engine))
			continue;
		outEngine = engine;
		outGlobalSlot = globalSlot;
		(void)stringAddr;
		(void)relXref;
		return true;
	}
	return false;
}

inline bool ScanExecBytesForStringXref(uintptr_t textBase, size_t textSize, uintptr_t stringAddr,
                                       uint64_t& outEngine, uintptr_t& outGlobalSlot) {
	if (!textSize || textSize < 7 || !Memory::IsValid(textBase))
		return false;
	constexpr size_t kChunk = 0x10000;
	std::vector<uint8_t> buf(kChunk);
	for (size_t off = 0; off + 7 <= textSize; off += kChunk - 16) {
		const size_t readSize = (off + kChunk > textSize) ? (textSize - off) : kChunk;
		if (readSize < 7)
			break;
		if (!Memory::Process.ReadUnchecked(textBase + off, buf.data(),
		                                   static_cast<DWORD>(readSize)))
			continue;
		for (size_t i = 0; i + 7 <= readSize; ++i) {
			uintptr_t target = 0;
			if (!RipTargetAt(buf.data() + i, textBase + off + i, target))
				continue;
			if (target < stringAddr || target > stringAddr + 64)
				continue;
			if (TryResolveGEngineNearXref(textBase + off + i, stringAddr, outEngine, outGlobalSlot))
				return true;
		}
	}
	return false;
}

inline bool XrefInImagePeExec(uintptr_t imageBase, uintptr_t stringAddr, uint64_t& outEngine,
                              uintptr_t& outGlobalSlot) {
	std::vector<SdkPeSections::View> sections;
	uint32_t sizeOfImage = 0;
	if (!SdkPeSections::Parse(imageBase, sections, sizeOfImage))
		return false;
	for (const SdkPeSections::View& sec : sections) {
		if (!SdkPeSections::SectionIsExecutable(sec.name))
			continue;
		if (ScanExecBytesForStringXref(imageBase + sec.virtualAddress, sec.virtualSize, stringAddr,
		                               outEngine, outGlobalSlot))
			return true;
	}
	return false;
}

inline bool FindStringXrefInText(uintptr_t imageBase, uintptr_t stringAddr, uint64_t& outEngine,
                                 uintptr_t& outGlobalSlot) {
	return XrefInImagePeExec(imageBase, stringAddr, outEngine, outGlobalSlot);
}

inline bool PollXrefPhase(uint64_t& outEngine, int maxExecRegionsPerTick);
inline bool PollStringPhase(uint64_t& outEngine, int maxRegionsPerTick);
inline bool BootstrapScanLargePeTier(uint64_t& outEngine);

inline bool TryEngineAtImageCandidates(uint64_t& outEngine) {
	if (DisableDeferredStringScan.load(std::memory_order_relaxed))
		return false;
	EnsureCommittedRegionCache();
	{
		std::lock_guard<std::mutex> lock(regionCacheMutex);
		ExpandDeferredRestRegionsLocked();
	}
	if (!largePeTierBootstrapDone.exchange(true, std::memory_order_relaxed)) {
		if (BootstrapScanLargePeTier(outEngine))
			return true;
	}
	while (!stringScanPassComplete.load(std::memory_order_relaxed)) {
		if (PollStringPhase(outEngine, 512))
			return true;
		if (pendingStringVa.load(std::memory_order_relaxed))
			(void)PollXrefPhase(outEngine, 512);
	}
	return false;
}

inline bool PollStringPhase(uint64_t& outEngine, int maxRegionsPerTick) {
	if (DisableDeferredStringScan.load(std::memory_order_relaxed))
		return false;
	EnsureCommittedRegionCache();
	const int total = static_cast<int>(committedRegions.size());
	if (total <= 0)
		return false;

	int idx = stringScanRegionIdx.load(std::memory_order_relaxed);
	int checkedThisTick = 0;
	while (idx < total && checkedThisTick < maxRegionsPerTick) {
		const CommittedRegion& r = committedRegions[static_cast<size_t>(idx)];
		const int regionTier = TierIndexForRegion(idx);
		++idx;
		++checkedThisTick;
		stringScanRegionsChecked.fetch_add(1, std::memory_order_relaxed);
		if (!RegionQuickReadable(r))
			continue;

		uintptr_t hit = 0;
		if (!ScanRegionForStringHit(r, hit))
			continue;
		if (!AcceptStringHit(hit, r, regionTier))
			continue;

		TryNoteUnpackedBaseFromPeRegion(r);
		pendingStringVa.store(hit, std::memory_order_relaxed);
		xrefExecRegionIdx.store(0, std::memory_order_relaxed);
		stringScanRegionIdx.store(idx, std::memory_order_relaxed);
		return PollXrefPhase(outEngine, 128);
	}

	stringScanRegionIdx.store(idx, std::memory_order_relaxed);
	if (idx >= total) {
		if (!deferredRestRegions.empty() && !restTierExpanded.load(std::memory_order_relaxed)) {
			std::lock_guard<std::mutex> lock(regionCacheMutex);
			ExpandDeferredRestRegionsLocked();
		} else {
			stringScanPassComplete.store(true, std::memory_order_relaxed);
			LogStringScanExhaustedIfNeeded();
		}
	}

	const int checked = stringScanRegionsChecked.load(std::memory_order_relaxed);
	if (stringScanPassComplete.load(std::memory_order_relaxed)) {
		LogStringScanExhaustedIfNeeded();
		return false;
	}
	if (checkedThisTick > 0
	    && (checked <= maxRegionsPerTick || checked % 120 == 0 || idx >= total)) {
		const int lastRegion = idx > 0 ? idx - 1 : 0;
		const int tier0 = TierIndexForRegion(lastRegion);
		const int tierStart =
			tier0 > 0 ? static_cast<int>(tierRegionEnds[static_cast<size_t>(tier0 - 1)]) : 0;
		const int tierEnd = static_cast<int>(tierRegionEnds[static_cast<size_t>(tier0)]);
		const int tierSize = tierEnd - tierStart;
		int tierChecked = idx - tierStart;
		if (tierChecked > tierSize)
			tierChecked = tierSize;
		if (tierChecked < 0)
			tierChecked = 0;
		std::cout << xorstr_("[*] GEngine string scan: tier ") << (tier0 + 1) << xorstr_("/")
		          << ActiveTierCountForLog() << xorstr_(" ") << std::dec << tierChecked << xorstr_("/")
		          << tierSize << xorstr_(" tier regions (") << eligibleRegionTotal
		          << xorstr_(" eligible)") << std::endl;
		std::cout.flush();
	}
	return false;
}

inline bool PollXrefPhase(uint64_t& outEngine, int maxExecRegionsPerTick) {
	const uintptr_t stringAddr = pendingStringVa.load(std::memory_order_relaxed);
	if (!stringAddr)
		return false;

	if (!IsCanonicalUserVa(stringAddr) || !RereadCommandNotRecognizedAt(stringAddr)) {
		LogStringCandidateRejected(stringAddr, xorstr_("pending string invalidated"));
		pendingStringVa.store(0, std::memory_order_relaxed);
		return false;
	}

	const uintptr_t imageBase = PeImageBaseContaining(stringAddr);
	if (imageBase) {
		uint64_t engine = 0;
		uintptr_t globalSlot = 0;
		if (XrefInImagePeExec(imageBase, stringAddr, engine, globalSlot)) {
			outEngine = engine;
			CommitEngineSuccess(engine, globalSlot, stringAddr);
			pendingStringVa.store(0, std::memory_order_relaxed);
			xrefExecRegionIdx.store(0, std::memory_order_relaxed);
			return true;
		}
	}

	EnsureCommittedRegionCache();
	const int total = static_cast<int>(committedRegions.size());
	int idx = xrefExecRegionIdx.load(std::memory_order_relaxed);
	int scanned = 0;
	uint32_t imageSize = 0;
	if (imageBase) {
		std::vector<SdkPeSections::View> peSections;
		(void)SdkPeSections::Parse(imageBase, peSections, imageSize);
	}
	const uintptr_t imageEnd =
		(imageBase && imageSize) ? imageBase + imageSize : 0;
	while (idx < total && scanned < maxExecRegionsPerTick) {
		const CommittedRegion& r = committedRegions[static_cast<size_t>(idx)];
		++idx;
		if (!r.exec)
			continue;
		if (imageEnd && (r.base < imageBase || r.base >= imageEnd))
			continue;
		++scanned;
		uint64_t engine = 0;
		uintptr_t globalSlot = 0;
		if (ScanExecBytesForStringXref(r.base, r.size, stringAddr, engine, globalSlot)) {
			outEngine = engine;
			CommitEngineSuccess(engine, globalSlot, stringAddr);
			pendingStringVa.store(0, std::memory_order_relaxed);
			xrefExecRegionIdx.store(idx, std::memory_order_relaxed);
			return true;
		}
	}
	xrefExecRegionIdx.store(idx, std::memory_order_relaxed);
	if (idx >= total)
		pendingStringVa.store(0, std::memory_order_relaxed);
	return false;
}

inline bool BootstrapScanLargePeTier(uint64_t& outEngine) {
	if (DisableDeferredStringScan.load(std::memory_order_relaxed))
		return false;
	if (!staticRvaKernelProbeDone.load(std::memory_order_relaxed)) {
		if (BootstrapStaticRvaPeProbe(outEngine))
			return TryPromoteEngineAfterStaticSdk(outEngine) || UnpackedBase != 0;
	}
	EnsureCommittedRegionCache();
	const int tier0End = static_cast<int>(tierRegionEnds[0]);
	const int tier1End = static_cast<int>(tierRegionEnds[1]);
	for (int i = 0; i < tier1End; ++i) {
		const CommittedRegion& r = committedRegions[static_cast<size_t>(i)];
		if (Read<uint16_t>(r.base) != 0x5A4D || !RegionQuickReadable(r))
			continue;
		int32_t num = 0;
		if (TryResolveSdkFromLargePeCandidate(r.base, &num)) {
			staticLargePeProbeOk.store(true, std::memory_order_relaxed);
			if (TryPromoteEngineAfterStaticSdk(outEngine))
				return true;
		}
	}
	if (tier1End <= tier0End)
		return UnpackedBase != 0;
	for (int i = tier0End; i < tier1End; ++i) {
		const CommittedRegion& r = committedRegions[static_cast<size_t>(i)];
		if (!RegionQuickReadable(r))
			continue;
		uintptr_t hit = 0;
		if (!ScanRegionForStringHit(r, hit))
			continue;
		const int curIdx = stringScanRegionIdx.load(std::memory_order_relaxed);
		if (i >= curIdx)
			stringScanRegionIdx.store(i + 1, std::memory_order_relaxed);
		if (!AcceptStringHit(hit, r, 1))
			continue;
		TryNoteUnpackedBaseFromPeRegion(r);
		pendingStringVa.store(hit, std::memory_order_relaxed);
		xrefExecRegionIdx.store(0, std::memory_order_relaxed);
		if (PollXrefPhase(outEngine, 128))
			return true;
	}
	const int curIdx = stringScanRegionIdx.load(std::memory_order_relaxed);
	if (curIdx >= tier0End && curIdx < tier1End)
		stringScanRegionIdx.store(tier1End, std::memory_order_relaxed);
	return false;
}

inline bool TryStaticGEngineInLargeMappings(uint64_t& outEngine) {
	constexpr uint64_t kGEngineRva = 0;
	if (!kGEngineRva)
		return false;
	EnsureCommittedRegionCache();
	for (const CommittedRegion& r : committedRegions) {
		if (r.size < 0x9000000ULL)
			continue;
		if (Read<uint16_t>(r.base) != 0x5A4D)
			continue;
		const uint32_t e_lfanew = Read<uint32_t>(r.base + 0x3C);
		const uintptr_t nt = r.base + e_lfanew;
		if (Read<uint32_t>(nt) != 0x00004550)
			continue;
		const uint32_t sizeOfImage = Read<uint32_t>(nt + 0x50);
		if (sizeOfImage < 0x9000000u)
			continue;
		const uintptr_t slot = r.base + kGEngineRva;
		const uint64_t engine = Read<uint64_t>(slot);
		if (!EngineChainWorldValid(engine))
			continue;
		outEngine = engine;
		CachedEngine.store(engine, std::memory_order_relaxed);
		CachedGlobalSlot.store(slot, std::memory_order_relaxed);
		UnpackedBase = r.base;
		SdkImageCache::cachedUnpacked = r.base;
		return true;
	}
	return false;
}

inline bool PollCommittedForCommandString(uintptr_t& /*ioCursor*/, int maxRegions,
                                          uint64_t& outEngine) {
	if (DisableDeferredStringScan.load(std::memory_order_relaxed)) {
		outEngine = 0;
		return false;
	}
	if (pendingStringVa.load(std::memory_order_relaxed)) {
		if (PollXrefPhase(outEngine, maxRegions * 8))
			return true;
	}
	if (!largePeTierBootstrapDone.exchange(true, std::memory_order_relaxed)) {
		if (BootstrapScanLargePeTier(outEngine))
			return true;
	}
	if (!stringScanPassComplete.load(std::memory_order_relaxed)) {
		if (PollStringPhase(outEngine, maxRegions))
			return true;
		return false;
	}
	LogStringScanExhaustedIfNeeded();
	if (!staticGEngineFallbackTried.exchange(true, std::memory_order_relaxed))
		return TryStaticGEngineInLargeMappings(outEngine);
	return false;
}
} // namespace GEngineDiscovery

inline uint64_t ResolveEngineViaCommandNotRecognizedXref(bool allowDeferredScan) {
	const uint64_t cached = GEngineDiscovery::CachedEngine.load(std::memory_order_relaxed);
	if (cached && Memory::IsValid(cached)) {
		if (GEngineDiscovery::EngineChainWorldValid(cached))
			return cached;
		GEngineDiscovery::CachedEngine.store(0, std::memory_order_relaxed);
	}

	const uintptr_t globalSlot = GEngineDiscovery::CachedGlobalSlot.load(std::memory_order_relaxed);
	if (globalSlot && Memory::IsValid(globalSlot)) {
		const uint64_t engine = Read<uint64_t>(globalSlot);
		if (GEngineDiscovery::EngineChainWorldValid(engine)) {
			GEngineDiscovery::CachedEngine.store(engine, std::memory_order_relaxed);
			return engine;
		}
	}

	uint64_t engine = 0;
	if (GEngineDiscovery::TryLoadGEngineSessionCache(engine))
		return engine;

	if (allowDeferredScan &&
	    !GEngineDiscovery::DisableDeferredStringScan.load(std::memory_order_relaxed)) {
		uintptr_t cursor = GEngineDiscovery::StringScanCursor.load(std::memory_order_relaxed);
		if (GEngineDiscovery::PollCommittedForCommandString(
		        cursor, GEngineDiscovery::kOverlayStringRegionsPerTick, engine)) {
			GEngineDiscovery::StringScanCursor.store(cursor, std::memory_order_relaxed);
			return engine;
		}
		GEngineDiscovery::StringScanCursor.store(cursor, std::memory_order_relaxed);
		return 0;
	}

	if (GEngineDiscovery::TryEngineAtImageCandidates(engine))
		return engine;
	return 0;
}

inline uint64_t ResolveEngineViaGObjects() {
	static uint64_t cachedEngine = 0;
	(void)ResolveSdkImageBase();
	const uintptr_t imageForDirect = GlobalImageBase();
	const auto viewportWorldMatchesDirect = [imageForDirect](uint64_t viewport) -> bool {
		if (!viewport || !Memory::IsValid(viewport))
			return false;
		const uint64_t world = Read<uint64_t>(viewport + Offsets::ViewportWorld);
		if (!world || !Memory::IsValid(world))
			return false;
		if (!imageForDirect)
			return true;
		const uint64_t direct = ReadUWorldFromImage(imageForDirect);
		return !direct || world == direct;
	};

	if (cachedEngine && Memory::IsValid(cachedEngine)) {
		const uint64_t viewport = Read<uint64_t>(cachedEngine + Offsets::GameViewport);
		if (viewportWorldMatchesDirect(viewport))
			return cachedEngine;
		cachedEngine = 0;
	}

	const ResolvedGObjects arr = ResolveGObjectsArray();
	if (arr.numElements <= 0 || arr.numChunks <= 0 || !arr.chunkTable)
		return 0;

	constexpr int32_t kMaxScan = 4096;
	const int32_t limit = arr.numElements < kMaxScan ? arr.numElements : kMaxScan;

	for (int32_t index = 0; index < limit; ++index) {
		const uint64_t object = GObjectsObjectByIndex(arr, index);
		if (!object)
			continue;

		if (Read<uint32_t>(object + 0x8) & TUObjectArrayLayout::RF_ClassDefaultObject)
			continue;

		const uint64_t viewport = Read<uint64_t>(object + Offsets::GameViewport);
		if (!viewportWorldMatchesDirect(viewport))
			continue;

		cachedEngine = object;
		return object;
	}
	return 0;
}

// SDK/Engine_functions.cpp: GWorld global, then UEngine::GetEngine() -> GameViewport -> World.
inline uint64_t ResolveUWorldViaEngine() {
	const bool allowStringScan =
	    !GEngineDiscovery::DisableDeferredStringScan.load(std::memory_order_relaxed);
	uint64_t engine = 0;
	if (PreferEngineDiscoveryPath())
		engine = ResolveEngineViaCommandNotRecognizedXref(allowStringScan);
	else
		engine = ResolveEngineViaGObjects();
	if (!engine && !PreferEngineDiscoveryPath())
		engine = ResolveEngineViaCommandNotRecognizedXref(allowStringScan);
	if (!engine || !Memory::IsValid(engine))
		return 0;
	const uint64_t world = UWorldFromEngine(engine);
	if (world && (LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3))) {
		if (!GEngineDiscovery::ChainWorldValidLogged.exchange(true, std::memory_order_relaxed)) {
			std::cout << xorstr_("[+] GEngine chain World valid") << std::endl;
			std::cout.flush();
		}
	}
	return world;
}

// APlayerController::AcknowledgedPawn (0x250) reads back 0 on Retrac, in the lobby and in a
// match. The controller's PlayerState is valid, and PawnPrivate on a PlayerState is the field
// the player cache already uses for every remote player, so take the local pawn from there.
inline uintptr_t ResolveLocalPawn(uintptr_t playerController, uintptr_t playerState) {
	if (playerController && Memory::IsValid(playerController)) {
		const uintptr_t acknowledged = Read<uintptr_t>(playerController + Offsets::AcknowledgedPawn);
		if (acknowledged && Memory::IsValid(acknowledged))
			return acknowledged;
	}
	if (playerState && Memory::IsValid(playerState)) {
		const uintptr_t pawn = Read<uintptr_t>(playerState + Offsets::PawnPrivate);
		const uintptr_t root = (pawn && Memory::IsValid(pawn))
			? Read<uintptr_t>(pawn + Offsets::RootComponent) : 0;
		if (root && Memory::IsValid(root))
			return pawn;
	}
	return 0;
}

inline uint64_t ResolveUWorldDirectFromImage() {
	const uintptr_t imageBase = GlobalImageBase() ? GlobalImageBase() : Baseadress;
	if (!imageBase || !Memory::IsValid(imageBase))
		return 0;
	uint64_t world = ReadUWorldFromImage(imageBase);
	if (!world)
		world = ReadUWorldCandidateFromImage(imageBase);
	if (world && Memory::IsValid(world))
		return world;
	return 0;
}

enum class RuntimeInitStage : uint8_t {
	Uninitialized = 0,
	SdkReady = 1,
	WorldReady = 2,
	PlayerReady = 3,
	CameraWaiting = 4,
	CameraReady = 5,
	RenderReady = 6,
};

inline std::atomic<RuntimeInitStage> g_RuntimeInitStage{RuntimeInitStage::Uninitialized};

inline const char* RuntimeInitStageName(RuntimeInitStage stage) {
	switch (stage) {
	case RuntimeInitStage::SdkReady:
		return "SDK_READY";
	case RuntimeInitStage::WorldReady:
		return "WORLD_READY";
	case RuntimeInitStage::PlayerReady:
		return "PLAYER_READY";
	case RuntimeInitStage::CameraWaiting:
		return "CAMERA_WAITING";
	case RuntimeInitStage::CameraReady:
		return "CAMERA_READY";
	case RuntimeInitStage::RenderReady:
		return "RENDER_READY";
	default:
		return "UNINITIALIZED";
	}
}

inline void AdvanceRuntimeInitStage(RuntimeInitStage stage) {
	RuntimeInitStage cur = g_RuntimeInitStage.load(std::memory_order_relaxed);
	while (static_cast<uint8_t>(stage) > static_cast<uint8_t>(cur)) {
		if (g_RuntimeInitStage.compare_exchange_weak(cur, stage, std::memory_order_relaxed)) {
			if (Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
				std::cout << xorstr_("[runtime] ") << RuntimeInitStageName(cur) << xorstr_(" -> ")
				          << RuntimeInitStageName(stage) << '\n';
			break;
		}
	}
}

inline void RegressRuntimeInitStage(RuntimeInitStage stage) {
	RuntimeInitStage cur = g_RuntimeInitStage.load(std::memory_order_relaxed);
	if (static_cast<uint8_t>(cur) <= static_cast<uint8_t>(stage))
		return;
	g_RuntimeInitStage.store(stage, std::memory_order_relaxed);
	if (Settings::DebugAtLeast(Settings::DebugVerbosity::Info))
		std::cout << xorstr_("[runtime] ") << RuntimeInitStageName(cur) << xorstr_(" -> ")
		          << RuntimeInitStageName(stage) << '\n';
}

inline void SyncRuntimeInitStageWithCamera(bool projectionReady, bool /*cameraAcquired*/) {
	if (projectionReady) {
		AdvanceRuntimeInitStage(RuntimeInitStage::CameraReady);
		AdvanceRuntimeInitStage(RuntimeInitStage::RenderReady);
		return;
	}
	RuntimeInitStage cur = g_RuntimeInitStage.load(std::memory_order_relaxed);
	if (static_cast<uint8_t>(cur) >= static_cast<uint8_t>(RuntimeInitStage::CameraReady))
		RegressRuntimeInitStage(RuntimeInitStage::CameraWaiting);
	if (static_cast<uint8_t>(cur) == static_cast<uint8_t>(RuntimeInitStage::PlayerReady))
		AdvanceRuntimeInitStage(RuntimeInitStage::CameraWaiting);
}

inline void LogRuntimeChainFailure(const char* stage, const char* detail) {
	static std::atomic<unsigned long long> nextLogMs{0};
	const unsigned long long now = GetTickCount64();
	if (now < nextLogMs.load(std::memory_order_relaxed))
		return;
	nextLogMs.store(now + 3000, std::memory_order_relaxed);
	std::cout << xorstr_("[-] Runtime ") << stage << xorstr_(" failed: ") << detail
	          << xorstr_(" (last stage ") << RuntimeInitStageName(g_RuntimeInitStage.load())
	          << xorstr_(")\n");
	std::cout.flush();
}

inline void ClearStaleViewChain() {
	LocalPtrs::PlayerController = 0;
	LocalPtrs::PlayerCam = 0;
	Camera::Valid = false;
	Camera::ViewProjectionReady = false;
	Camera::Source = "unusable";
}

// Walk UWorld → … → PCM. A dead controller must not keep the lobby POV.
inline bool SyncLocalPtrsFromWorld(uintptr_t world) {
	if (!world || !Memory::IsValid(world)) {
		LogRuntimeChainFailure("WORLD", "UWorld pointer invalid");
		return false;
	}
	LocalPtrs::Gworld = world;
	AdvanceRuntimeInitStage(RuntimeInitStage::WorldReady);

	const uint64_t gameInstance = Read<uint64_t>(world + Offsets::OwningGameInstance);
	if (!gameInstance || !Memory::IsValid(gameInstance)) {
		LogRuntimeChainFailure("GAME_INSTANCE", "OwningGameInstance invalid");
		return true;
	}
	LocalPtrs::GameInstance = gameInstance;

	const uint64_t localPlayersData = Read<uint64_t>(gameInstance + Offsets::LocalPlayers);
	const uint64_t localPlayer = Read<uint64_t>(localPlayersData);
	if (!localPlayer || !Memory::IsValid(localPlayer)) {
		LogRuntimeChainFailure("LOCAL_PLAYER", "LocalPlayers[0] invalid");
		return true;
	}
	LocalPtrs::LocalPlayers = localPlayer;

	const uint64_t playerController = Read<uint64_t>(localPlayer + Offsets::PlayerController);
	if (!playerController || !Memory::IsValid(playerController)) {
		LogRuntimeChainFailure("PLAYER_CONTROLLER", "PlayerController invalid");
		ClearStaleViewChain();
		return true;
	}
	LocalPtrs::PlayerController = playerController;

	LocalPtrs::PlayerState = Read<uint64_t>(playerController + Offsets::PlayerState);
	const uintptr_t pawn = ResolveLocalPawn(playerController, LocalPtrs::PlayerState);
	if (pawn && Memory::IsValid(pawn)) {
		LocalPtrs::Player = pawn;
		LocalPtrs::RootComponent = Read<uint64_t>(pawn + Offsets::RootComponent);
		LocalPtrs::PlayerMesh = Read<uint64_t>(pawn + Offsets::Mesh);
		if (LocalPtrs::RootComponent && Memory::IsValid(LocalPtrs::RootComponent))
			LocalPtrs::relative_location =
			    Read<Vector3>(LocalPtrs::RootComponent + Offsets::Realitivelocation);
		AdvanceRuntimeInitStage(RuntimeInitStage::PlayerReady);
	} else {
		LogRuntimeChainFailure("PAWN", "local pawn unresolved");
	}

	const uint64_t pcm = Read<uint64_t>(playerController + Offsets::playercameramanager);
	if (pcm && Memory::IsValid(pcm))
		LocalPtrs::PlayerCam = pcm;
	else {
		LogRuntimeChainFailure("PCM", "PlayerCameraManager invalid");
		if (pcm != LocalPtrs::PlayerCam)
			ClearStaleViewChain();
	}

	const uintptr_t gameState = Read<uintptr_t>(world + Offsets::GameState);
	if (gameState && Memory::IsValid(gameState)) {
		LocalPtrs::GameState = gameState;
		LocalPtrs::PlayerArray = Read<uintptr_t>(gameState + Offsets::PlayerArray);
		LocalPtrs::PlayerArrayCount =
		    Read<int>(gameState + Offsets::PlayerArray + sizeof(uintptr_t));
	} else {
		LogRuntimeChainFailure("GAME_STATE", "GameState invalid");
	}
	return true;
}

inline uint64_t ResolveUWorld() {
	(void)ResolveSdkImageBase();
	const uint64_t direct = ResolveUWorldDirectFromImage();
	if (direct)
		return direct;
	if (!GEngineDiscovery::DisableDeferredStringScan.load(std::memory_order_relaxed)) {
		const uint64_t viaEngine = ResolveUWorldViaEngine();
		if (viaEngine)
			return viaEngine;
	}
	if (LocalPtrs::Gworld && Memory::IsValid(LocalPtrs::Gworld))
		return LocalPtrs::Gworld;
	return 0;
}

inline bool SdkImageReadyAt(uintptr_t image) {
	if (!image || !Memory::IsValid(image))
		return false;
	if (PreferEngineDiscoveryPath()) {
		const uintptr_t sdkImage = GlobalImageBase() ? GlobalImageBase() : image;
		if (sdkImage && HasLiveGObjectsChunks(sdkImage))
			return true;
		if (sdkImage && StaticUWorldRvaInImage(sdkImage)) {
			const uint64_t world = Read<uint64_t>(sdkImage + Offsets::UWorld);
			if (LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3))
				return true;
		}
		uint64_t eng = GEngineDiscovery::CachedEngine.load(std::memory_order_relaxed);
		if (eng && GEngineDiscovery::EngineChainWorldValid(eng))
			return true;
		eng = ResolveEngineViaGObjects();
		if (eng && GEngineDiscovery::EngineChainWorldValid(eng)) {
			GEngineDiscovery::CachedEngine.store(eng, std::memory_order_relaxed);
			return true;
		}
		if (!GEngineDiscovery::DisableDeferredStringScan.load(std::memory_order_relaxed) &&
		    GEngineDiscovery::EngineDiscoveryChainReady()) {
			const uint64_t world = ResolveUWorldViaEngine();
			if (world && (LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3)))
				return true;
		}
		return false;
	}
	const uintptr_t sdkImage = GlobalImageBase() ? GlobalImageBase() : image;
	if (HasLiveGObjectsChunks(sdkImage)) {
		SdkPeDiscovery::EnsureStaticDiscoveryState(sdkImage);
		const uint64_t uworldRva = SdkPeDiscovery::UWorldRva();
		if (uworldRva) {
			const uint64_t world = Read<uint64_t>(sdkImage + uworldRva);
			if (LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3))
				return true;
		}
		return true;
	}
	if (GEngineDiscovery::DisableDeferredStringScan.load(std::memory_order_relaxed))
		return false;
	const uint64_t world = ResolveUWorldViaEngine();
	return world && (LooksLikeUWorld(world) || LooksLikeUWorldRelaxed(world, 3));
}

inline std::atomic<bool> SdkReady{false};

inline void RefreshLiveChainForRender() {
	if (!SdkReady.load(std::memory_order_relaxed))
		return;
	const uintptr_t world = CurrentWorld();
	if (!world || !Memory::IsValid(world))
		return;
	bool needSync = !LocalPtrs::Gworld || !Memory::IsValid(LocalPtrs::Gworld) ||
	                LocalPtrs::Gworld != world || !LocalPtrs::PlayerController ||
	                !Memory::IsValid(LocalPtrs::PlayerController) || !LocalPtrs::PlayerCam ||
	                !Memory::IsValid(LocalPtrs::PlayerCam) ||
	                (Camera::Valid && Camera::Location.y > 80000.f);
	if (!needSync && LocalPtrs::LocalPlayers && Memory::IsValid(LocalPtrs::LocalPlayers)) {
		const uint64_t livePc = Read<uint64_t>(LocalPtrs::LocalPlayers + Offsets::PlayerController);
		if (!livePc || !Memory::IsValid(livePc) || livePc != LocalPtrs::PlayerController) {
			needSync = true;
		} else {
			const uint64_t livePcm =
			    Read<uint64_t>(livePc + Offsets::playercameramanager);
			if (!livePcm || !Memory::IsValid(livePcm) || livePcm != LocalPtrs::PlayerCam)
				needSync = true;
		}
	}
	if (needSync)
		SyncLocalPtrsFromWorld(world);
	if (LocalPtrs::Gworld && Memory::IsValid(LocalPtrs::Gworld) && LocalPtrs::PlayerController
	    && Memory::IsValid(LocalPtrs::PlayerController) && LocalPtrs::PlayerCam
	    && Memory::IsValid(LocalPtrs::PlayerCam))
		GEngineDiscovery::DisableDeferredStringScan.store(true, std::memory_order_relaxed);
}

inline bool UpdateSdkReadyState(bool resetCache = false) {
	if (!Baseadress) {
		SdkReady.store(false, std::memory_order_relaxed);
		return false;
	}
	if (resetCache)
		ResetSdkPeDiscoveryIfStillEmpty(Baseadress);
	(void)ResolveSdkImageBase(resetCache);
	const uintptr_t image = GlobalImageBase();
	SdkPeDiscovery::EnsureStaticDiscoveryState(Baseadress);
	if (image && image != Baseadress)
		SdkPeDiscovery::EnsureStaticDiscoveryState(image);
	if (!SdkReady.load(std::memory_order_relaxed))
		SdkPeDiscovery::EnsureDiscoveredOffsets(Baseadress);
	const bool ready = image && SdkImageReadyAt(image);
	SdkReady.store(ready, std::memory_order_relaxed);
	return ready;
}

inline void BootstrapSdkReadyAttempt() {
	SdkReady.store(false, std::memory_order_relaxed);
	SdkPeDiscovery::peDataScanEnabled.store(false, std::memory_order_relaxed);
	SdkPeDiscovery::staticSlotFailPolls.store(0, std::memory_order_relaxed);
	if (!Baseadress)
		return;
	if (!StaticGObjectsRpmAtBase(Baseadress) || StaticGObjectsSlotRegionUnusable(Baseadress))
		SdkPeDiscovery::peDataScanEnabled.store(true, std::memory_order_relaxed);
	SdkPeDiscovery::EnsureStaticDiscoveryState(Baseadress);
	if (SdkPeDiscovery::peDataScanEnabled.load(std::memory_order_relaxed))
		SdkPeDiscovery::EnsureDiscoveredOffsets(Baseadress);
	(void)ResolveSdkImageBase(false);
	const uintptr_t image = GlobalImageBase();
	SdkReady.store(image && SdkImageReadyAt(image), std::memory_order_relaxed);
}

inline bool LiveResearchChainReady() {
	if (!LocalPtrs::Gworld || !Memory::IsValid(LocalPtrs::Gworld))
		return false;
	if (!LocalPtrs::PlayerController || !Memory::IsValid(LocalPtrs::PlayerController))
		return false;
	if (!LocalPtrs::PlayerCam || !Memory::IsValid(LocalPtrs::PlayerCam))
		return false;
	return true;
}

inline void DisableGEngineStringScanForLiveSdk() {
	GEngineDiscovery::DisableDeferredStringScan.store(true, std::memory_order_relaxed);
}

inline void PollSdkReadyFromOverlay() {
	if (SdkReady.load(std::memory_order_relaxed))
		return;
	if (LiveResearchChainReady()) {
		DisableGEngineStringScanForLiveSdk();
		SdkReady.store(true, std::memory_order_relaxed);
		AdvanceRuntimeInitStage(RuntimeInitStage::SdkReady);
		static std::atomic<bool> readyLogged{ false };
		if (!readyLogged.exchange(true)) {
			std::cout << xorstr_("[+] SDK ready — live chain (no GEngine scan)") << std::endl;
			std::cout.flush();
		}
		return;
	}
	static std::atomic<unsigned> overlayPoll{ 0 };
	const unsigned n = ++overlayPoll;
	if ((n % 30u) != 0u)
		return;
	const uintptr_t img = GlobalImageBase() ? GlobalImageBase() : Baseadress;
	if (!img)
		return;
	if (HasLiveGObjectsChunks(img) || StaticGObjectsRpmAtBase(img)) {
		DisableGEngineStringScanForLiveSdk();
		SdkReady.store(true, std::memory_order_relaxed);
		AdvanceRuntimeInitStage(RuntimeInitStage::SdkReady);
		const uint64_t world = ResolveUWorldDirectFromImage();
		if (world)
			SyncLocalPtrsFromWorld(static_cast<uintptr_t>(world));
		static std::atomic<bool> readyLoggedStatic{ false };
		if (!readyLoggedStatic.exchange(true)) {
			std::cout << xorstr_("[+] SDK ready — static SDK (no GEngine scan)") << std::endl;
			std::cout.flush();
		}
	}
}
