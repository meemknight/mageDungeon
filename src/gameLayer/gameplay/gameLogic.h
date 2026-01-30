#pragma once
#include <gameplay/map.h>
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
#include "spellSelectionInputLogic.h"

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
	Wand wands[2];
	bool hasWand[2] = {};
	int activeWandIndex = 0;
	WandStoneSlot wandStoneSlots[2][4] = {};
	std::vector<MagicStone> stoneInventory;
	int draggingStoneIndex = -1;
	glm::vec2 draggingStoneOffset = {};
	bool draggingStone = false;
	int quickActionEditIndex = -1;
	float wandHoverTimer = 0.0f;
	float wandFailTimer = 0.0f;
	DroppedItemSystem droppedItems; // dropped items like wands

	ParticleSystem particleSystem;
	ParticlePostProcessRenderer particlePostProcessRenderer;
	gl2d::FrameBuffer gameFbo;
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

	void close();

	float zoom = 100;
	bool inGame = 0;

	std::ranlux24_base rng{std::random_device()()};

};
