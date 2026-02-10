#pragma once
#include <gameplay/map.h>
#include <gameplay/doors.h>
#include <gameplay/elements.h>
#include <vector>
#include <gameplay/Physics.h>
#include <gameplay/player.h>
#include <gameplay/projectiles/projectiles.h>
#include <gameplay/summons.h>
#include <gameplay/paletteEffect.h>
#include "particleSystem.h"
#include <gameplay/entities/entity.h>
#include <gameplay/spells/spells.h>
#include <gameplay/spells/spellTypes.h>
#include <gameplay/damageViewerSystem.h>
#include <worldGen/floorGen.h>
#include <gameplay/wand.h>
#include <gameplay/droppedItems.h>
#include <gameplay/spellbookPage.h>
#include <gameplay/gameHdrPostProcess.h>
#include "spellSelectionInputLogic.h"
#include <gameplay/trapWaves.h>
#include <gameplay/minimapSystem.h>
#include <gameplay/roomLightingSystem.h>
#include <gameplay/cameraShakeSystem.h>

//this is an instance of the game.
//This shouldn't load things like textures, those should be load outside
// Magic stones are temporary element upgrades for wand slots.
struct MagicStone
{
	int element = Elements::Fire;
	int uses = 1;
};

struct WandStoneSlot
{
	bool hasStone = false;
	MagicStone stone = {};
};

struct TrapRoomSpawn
{
	TrapWaveSpawn spawn = {};
	float timer = 0.0f;
	bool effectStarted = false;
};

struct TrapRoomState
{
	// Trap rooms lock doors until cleared once.
	bool isTrap = false;
	bool triggered = false;
	bool cleared = false;
	bool rewardGranted = false;
	std::vector<glm::ivec2> doorAnchors;
	std::vector<std::vector<TrapWaveSpawn>> wavePlan;
	int currentWaveIndex = -1;
	std::vector<TrapRoomSpawn> pendingSpawns;
};

struct BreakableDecorationSystem
{
	// Holds map-placed breakable decorations for custom logic/rendering.
	std::vector<glm::ivec2> positions;

	void clear()
	{
		positions.clear();
	}
};

struct TrapSpikeElement
{
	glm::ivec2 pos = {};
	// Closed -> OpeningDelay -> Opening -> Open -> Closing
	enum class State : unsigned char
	{
		Closed,
		OpeningDelay,
		Opening,
		Open,
		Closing
	};
	State state = State::Closed;
	float stateTimer = 0.0f;
	float damageCooldown = 0.0f;
	bool queuedOpen = false;
};

struct SpikeTrapSettings
{
	static constexpr float OpenDelaySeconds = 0.8f;
	static constexpr float OpenAnimSeconds = 0.2f;
	static constexpr float OpenHoldSeconds = 4.0f;
	static constexpr float CloseAnimSeconds = 0.2f;
	static constexpr float DamageCooldownSeconds = 1.0f;
};

struct TrapSpikeSystem
{
	// Trap spike placements copied from the map for custom rendering.
	std::vector<TrapSpikeElement> spikes;

	void clear()
	{
		spikes.clear();
	}
};

struct GameLogic
{
	Map map;
	Player player;
	ProjectileHolder projectiles;
	StandbyProjectileSystem standbyProjectiles; // orbiting projectiles that wait and fire
	SummonHolder summons; // friendly summons that assist the player
	EntityHolder entityHolder;
	SpellsHolder spellsHolder;
	SpellRecepie spellRecepies[2]; // current spell recepies for each wand
	SleppSelectionInputLogic spellSelectionLogic[2]; // spell selection input and UI per wand
	DamageViewerSystem damageViewerSystem;
	FloorInfo floorInfo;
	DoorHolder doorHolder;
	std::vector<TrapRoomState> trapRooms;
	int trapDifficulty = 0;
	bool forceTrapDifficulty = false;
	BreakableDecorationSystem breakableDecorations;
	TrapSpikeSystem trapSpikes;
	RoomLightingSystem roomLightingSystem;
	Wand wands[2];
	bool hasWand[2] = {};
	int activeWandIndex = 0;
	WandStoneSlot wandStoneSlots[2][4] = {};
	std::vector<MagicStone> stoneInventory;
	int draggingStoneIndex = -1;
	glm::vec2 draggingStoneOffset = {};
	bool draggingStone = false;
	// Controller inventory navigation state for stone placement.
	int controllerInventoryFocus = 0; // 0 = wand slots, 1 = stone list
	int controllerInventoryWandSlot = 0;
	int controllerInventoryStoneIndex = 0;
	int controllerInventorySelectedStoneIndex = -1;
	bool controllerInventoryHasSelectedStone = false;
	bool controllerInventoryStickLockX = false;
	bool controllerInventoryStickLockY = false;
	int quickActionEditIndex = -1;
	float wandHoverTimer = 0.0f;
	DroppedItemSystem droppedItems; // dropped items like wands and chests
	SpellbookPage spellbookPage;
	int inventoryPage = 0;

	ParticleSystem particleSystem;
	ParticlePostProcessRenderer particlePostProcessRenderer;
	MinimapSystem minimapSystem; // offscreen minimap renderer
	CameraShakeSystem cameraShakeSystem;
	gl2d::FrameBuffer gameFbo;
	GameHdrPostProcess gameHdrPostProcess;
	PaletteEffect paletteEffect;
	bool inventoryOpen = false;

	glm::vec2 fireDirection = {1,0};
	glm::vec2 fireTargetPos = {1,0};
	bool fireInputActive = false;
	float playerDamageCooldown = 0.0f; // time until next contact hit

	//returns false on fail
	bool init();

	//returns false on fail
	bool update(float deltaTime, gl2d::Renderer2D &renderer,
		AssetsManager &assetsManager, platform::Input &input);
	void addCameraShake(CameraShakeType type, float intensity, float duration);

	void close();

	float zoom = 100;
	// World seed used for procedural floor generation.
	int worldSeed = 123469;
	int currentFloorIndex = 0;
	bool keepFloorOnClose = false;
	// Free camera debug mode decouples camera from player.
	bool freeCameraMode = false;
	glm::vec2 freeCameraPosition = {};
	bool mapViewerOpen = false; // fullscreen map overlay toggle
	glm::vec2 mapViewerCenter = {};
	float mapViewerViewSize = 56.0f;
	bool inGame = 0;

	std::ranlux24_base rng{std::random_device()()};

};
