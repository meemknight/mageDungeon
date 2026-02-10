#include <gameplay/gameLogic.h>
#include <gameplay/map.h>
#include <imgui.h>
#include <platformInput.h>
#include <gameLayer.h>
#include <glui/glui.h>
#include <imguiTools.h>
#include <iostream>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <logs.h>
#include <gameplay/entities/entity.h>
#include <gameplay/entities/enemyTypes.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <randomStuff.h>
#include <particles/particleCreator.h>
#include <gameplay/statusEffects.h>
#include <gameplay/inputPrompts.h>
#include <gameplay/aStar.h>

#include <gameplay/elements.h>
#include <gameplay/worldTextSystem.h>
#include <gameplay/spellRecepieWindow.h>

#include <worldGen/floorGen.h>

// Temporary: disable legacy random enemy spawning system.
static const bool disableRandomEnemySpawns = true;
static bool removeLightSystem = false;
static bool disableSecondLightLayer = true;
// Debug toggle: disable particle bloom while investigating visual issues.
static bool disableParticleBloom = false;
// Debug toggle for fullscreen HDR tone mapping post process.
static bool disableGameHdrTonemap = false;
static const bool skipTutorialFloor = true;
static const float trapRoomChance = 0.80f;
static const float trapRoomTriggerMargin = 1.5f;
static const float trapRoomSpawnDelaySeconds = 1.3f;
static const float trapRoomSpawnStaggerSeconds = 0.3f;
static const int tutorialFloorSizeX = 100;
static const int tutorialFloorSizeY = 100;
static const int dungeonFloorSizeX = 100;
static const int dungeonFloorSizeY = 100;
static const int maxCombatFloors = 4;
static bool renderCollidersFlag = false;

bool renderColliders()
{
	return renderCollidersFlag;
}

// Global color grading settings are kept outside gameplay state resets.
// We reuse the same struct so default values stay in gameHdrPostProcess.h.
static GameHdrPostProcess globalHdrToneMapSettings;

namespace
{
	void copyHdrToneMapSettings(GameHdrPostProcess &to, const GameHdrPostProcess &from)
	{
		to.enabled = from.enabled;
		to.toneMapper = from.toneMapper;
		to.exposure = from.exposure;
		to.saturation = from.saturation;
		to.vibrance = from.vibrance;
		to.gamma = from.gamma;
		to.shadowBoost = from.shadowBoost;
		to.highlightBoost = from.highlightBoost;
		to.vignette = from.vignette;
		to.lift = from.lift;
		to.gain = from.gain;
	}

	void applyGlobalHdrToneMapSettings(GameHdrPostProcess &hdrPost)
	{
		copyHdrToneMapSettings(hdrPost, globalHdrToneMapSettings);
	}

	void storeGlobalHdrToneMapSettings(const GameHdrPostProcess &hdrPost)
	{
		copyHdrToneMapSettings(globalHdrToneMapSettings, hdrPost);
	}

	int getFloorDifficulty(int floorIndex)
	{
		int tier = floorIndex - 1;
		if (tier < 0) { tier = 0; }
		if (tier > 3) { tier = 3; }
		return tier;
	}

	int getFloorWandTier(int floorIndex)
	{
		if (floorIndex <= 0) { return 0; }
		int tier = floorIndex + 1;
		if (tier > 5) { tier = 5; }
		return tier;
	}

	bool isLastFloor(int floorIndex)
	{
		return floorIndex >= maxCombatFloors;
	}

	// Spawns a random enemy from the basic roster.
	void spawnRandomEnemy(EntityHolder &entityHolder, std::ranlux24_base &rng, glm::vec2 pos, int difficulty)
	{
		difficulty = std::max(0, std::min(difficulty, 3));
		switch (difficulty)
		{
			case 0:
			{
				int pick = getRandomInt(rng, 0, 9);
				if (pick < 4) { entityHolder.addEntity(EnemyTypes::getGoblinThiefEnemy(), pos); }
				else if (pick < 7) { entityHolder.addEntity(EnemyTypes::getGoblinSpearmanEnemy(), pos); }
				else if (pick < 9) { entityHolder.addEntity(EnemyTypes::getGoblinArcherEnemy(), pos); }
				else { entityHolder.addEntity(EnemyTypes::getGoblinHeavyEnemy(), pos); }
			} break;
			case 1:
			{
				int pick = getRandomInt(rng, 0, 11);
				if (pick < 3) { entityHolder.addEntity(EnemyTypes::getGoblinThiefEnemy(), pos); }
				else if (pick < 5) { entityHolder.addEntity(EnemyTypes::getGoblinSpearmanEnemy(), pos); }
				else if (pick < 7) { entityHolder.addEntity(EnemyTypes::getGoblinArcherEnemy(), pos); }
				else if (pick < 9) { entityHolder.addEntity(EnemyTypes::getOrcArcherEnemy(), pos); }
				else if (pick < 11) { entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), pos); }
				else { entityHolder.addEntity(EnemyTypes::getGoblinHeavyEnemy(), pos); }
			} break;
			case 2:
			{
				int pick = getRandomInt(rng, 0, 11);
				if (pick < 3) { entityHolder.addEntity(EnemyTypes::getTemplarOriginalEnemy(), pos); }
				else if (pick < 5) { entityHolder.addEntity(EnemyTypes::getEarthTemplarEnemy(), pos); }
				else if (pick < 7) { entityHolder.addEntity(EnemyTypes::getFireTemplarEnemy(), pos); }
				else if (pick < 9) { entityHolder.addEntity(EnemyTypes::getIceTemplarEnemy(), pos); }
				else if (pick < 10) { entityHolder.addEntity(EnemyTypes::getWaterTemplarEnemy(), pos); }
				else { entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), pos); }
			} break;
			default:
			{
				int pick = getRandomInt(rng, 0, 9);
				if (pick < 6)
				{
					int templarPick = getRandomInt(rng, 0, 4);
					switch (templarPick)
					{
						case 0: entityHolder.addEntity(EnemyTypes::getTemplarOriginalEnemy(), pos); break;
						case 1: entityHolder.addEntity(EnemyTypes::getEarthTemplarEnemy(), pos); break;
						case 2: entityHolder.addEntity(EnemyTypes::getFireTemplarEnemy(), pos); break;
						case 3: entityHolder.addEntity(EnemyTypes::getIceTemplarEnemy(), pos); break;
						default: entityHolder.addEntity(EnemyTypes::getWaterTemplarEnemy(), pos); break;
					}
				}
				else
				{
					entityHolder.addEntity(EnemyTypes::getDarkAngelEnemy(), pos);
				}
			} break;
		}
	}
}

bool GameLogic::init()
{

	FloorGenerator floorGenerator;
	floorGenerator.init();

	std::vector<FloorConnection> connections;
	if (worldSeed <= 0)
	{
		std::ranlux24_base localRng{std::random_device{}()};
		worldSeed = getRandomInt(localRng, 1, 2000000000);
	}

	int sizeX = (currentFloorIndex == 0) ? tutorialFloorSizeX : dungeonFloorSizeX;
	int sizeY = (currentFloorIndex == 0) ? tutorialFloorSizeY : dungeonFloorSizeY;
	if (skipTutorialFloor && currentFloorIndex == 0)
	{
		currentFloorIndex = 1;
	}
	sizeX = (currentFloorIndex == 0) ? tutorialFloorSizeX : dungeonFloorSizeX;
	sizeY = (currentFloorIndex == 0) ? tutorialFloorSizeY : dungeonFloorSizeY;
	if (currentFloorIndex == 0)
	{
		floorGenerator.generateTutorialFloor(sizeX, sizeY, map, floorInfo, doorHolder);
	}
	else
	{
		int floorSeed = worldSeed + currentFloorIndex * 7919;
		floorGenerator.generateDungeonFloor(sizeX, sizeY, map, floorSeed, connections, true, floorInfo, doorHolder);
	}


	floorGenerator.clear();

	// Place door collision blocks after world generation.
	auto placeDoorCollisionBlocks = [&]()
	{
		for (auto &doorPair : doorHolder.doors)
		{
			auto &door = doorPair.second;
			if (door.open) { continue; }
			glm::ivec2 pos = doorPair.first;
			if (door.orientation == Door::Orientation::Horizontal)
			{
				for (int dx = 0; dx <= 1; dx++)
				{
					int x = pos.x + dx;
					int y = pos.y;
					if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y) { continue; }
					auto &tile = map.secondLayer.getBlockUnsafe(x, y);
					if (tile.type == Blocks::none || tile.type == Blocks::doorCollision)
					{
						tile.type = Blocks::doorCollision;
					}
				}
			}
			else if (door.orientation == Door::Orientation::Vertical)
			{
				for (int dy = 0; dy <= 1; dy++)
				{
					int x = pos.x;
					int y = pos.y - dy;
					if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y) { continue; }
					auto &tile = map.secondLayer.getBlockUnsafe(x, y);
					if (tile.type == Blocks::none || tile.type == Blocks::doorCollision)
					{
						tile.type = Blocks::doorCollision;
					}
				}
			}
		}
	};

	// Extract map-placed breakable decorations for custom handling/rendering.
	breakableDecorations.clear();
	auto extractBreakableDecorations = [&](MapLayer &layer)
	{
		for (int y = 0; y < layer.size.y; y++)
		{
			for (int x = 0; x < layer.size.x; x++)
			{
				auto &tile = layer.getBlockUnsafe(x, y);
				if (!isBreakableDecoration(tile.type)) { continue; }
				glm::ivec2 pos = {x, y};
				auto found = std::find_if(breakableDecorations.positions.begin(),
					breakableDecorations.positions.end(), [&](const glm::ivec2 &other)
				{
					return other.x == pos.x && other.y == pos.y;
				});
				if (found == breakableDecorations.positions.end())
				{
					breakableDecorations.positions.push_back(pos);
				}
				tile.type = Blocks::none;
			}
		}
	};
	extractBreakableDecorations(map.firstLayer);
	extractBreakableDecorations(map.secondLayer);

	trapSpikes.clear();
	for (int y = 0; y < map.secondLayer.size.y; y++)
	{
		for (int x = 0; x < map.secondLayer.size.x; x++)
		{
			auto &tile = map.secondLayer.getBlockUnsafe(x, y);
			if (tile.type == Blocks::spikeTrap)
			{
				trapSpikes.spikes.push_back({{x, y}});
			}
		}
	}

	placeDoorCollisionBlocks();
	roomLightingSystem.resetForFloor(map, floorInfo, doorHolder);

	// Pick trap rooms (spawn/exit rooms are excluded).
	trapRooms.clear();
	trapRooms.resize(floorInfo.rooms.size());
	std::vector<int> eligibleTrapRooms;
	int spawnRoomIndex = floorInfo.spawnRoomIndex.value_or(-1);
	int exitRoomIndex = floorInfo.exitRoomIndex.value_or(-1);
	int trapCount = 0;
	for (size_t i = 0; i < floorInfo.rooms.size(); i++)
	{
		const auto &room = floorInfo.rooms[i];
		if (room.isSpawnRoom || room.isExitRoom || room.isEmptyRoom)
		{
			continue;
		}
		if ((int)i == spawnRoomIndex || (int)i == exitRoomIndex)
		{
			continue;
		}
		if (room.enemySpawnPositions.empty() || room.doorPositions.empty())
		{
			continue;
		}
		eligibleTrapRooms.push_back((int)i);
		float roll = getRandomFloat(rng, 0.0f, 1.0f);
		if (roll < trapRoomChance)
		{
			trapRooms[i].isTrap = true;
			trapCount++;
		}
	}
	if (trapCount == 0 && !eligibleTrapRooms.empty())
	{
		int pick = getRandomInt(rng, 0, (int)eligibleTrapRooms.size() - 1);
		trapRooms[eligibleTrapRooms[pick]].isTrap = true;
	}

	auto addTrapDoorAnchor = [&](TrapRoomState &state, glm::ivec2 anchor)
	{
		for (const auto &existing : state.doorAnchors)
		{
			if (existing.x == anchor.x && existing.y == anchor.y)
			{
				return;
			}
		}
		state.doorAnchors.push_back(anchor);
	};

	for (size_t i = 0; i < floorInfo.rooms.size(); i++)
	{
		auto &state = trapRooms[i];
		state.doorAnchors.clear();
		const auto &room = floorInfo.rooms[i];
		int left = room.pos.x;
		int top = room.pos.y;
		int right = room.pos.x + room.size.x - 1;
		int bottom = room.pos.y + room.size.y - 1;
		for (const auto &doorPair : doorHolder.doors)
		{
			glm::ivec2 anchor = doorPair.first;
			const auto &door = doorPair.second;
			if (door.orientation == Door::Orientation::Horizontal)
			{
				if (anchor.y != top && anchor.y != bottom) { continue; }
				if (anchor.x < left || anchor.x > right - 1) { continue; }
				addTrapDoorAnchor(state, anchor);
			}
			else
			{
				if (anchor.x != left && anchor.x != right) { continue; }
				if (anchor.y < top + 1 || anchor.y > bottom) { continue; }
				addTrapDoorAnchor(state, anchor);
			}
		}
	}

	int validTrapCount = 0;
	for (size_t i = 0; i < trapRooms.size(); i++)
	{
		auto &state = trapRooms[i];
		if (state.isTrap && state.doorAnchors.empty())
		{
			state.isTrap = false;
			continue;
		}
		if (state.isTrap)
		{
			validTrapCount++;
		}
	}
	if (validTrapCount == 0 && !eligibleTrapRooms.empty())
	{
		std::vector<int> anchoredRooms;
		anchoredRooms.reserve(eligibleTrapRooms.size());
		for (int index : eligibleTrapRooms)
		{
			if (!trapRooms[index].doorAnchors.empty())
			{
				anchoredRooms.push_back(index);
			}
		}
		if (!anchoredRooms.empty())
		{
			int pick = getRandomInt(rng, 0, (int)anchoredRooms.size() - 1);
			trapRooms[anchoredRooms[pick]].isTrap = true;
		}
	}
	if (currentFloorIndex == 0)
	{
		for (auto &state : trapRooms)
		{
			state.isTrap = false;
		}
	}

	//wands[0] = getRandomWand(0, rng); //starter wand
	//wands[0] = getRandomWand(1, rng); //temporary give tier 1 wand to the player
	{
		Wand elderWand;
		elderWand.up = {WandSlotType::Element, Elements::Fire, 4};
		elderWand.down = {WandSlotType::Element, Elements::Water, 4};
		elderWand.left = {WandSlotType::Element, Elements::Earth, 4};
		elderWand.right = {WandSlotType::Element, Elements::Ice, 4};
		elderWand.alwaysCast = {WandSlotType::Disabled};
		elderWand.maxMana = 15;
		elderWand.manaChargeSpeed = 1.3f;
		elderWand.maxElementsPerCast = 7;
		elderWand.wandSprite = Wand::elderWand;
		elderWand.sanitize();
		wands[0] = elderWand;
	}

	hasWand[0] = true;
	hasWand[1] = false;
	activeWandIndex = 0;
	spellRecepies[0].clear();
	spellRecepies[1].clear();
	spellSelectionLogic[0] = {};
	spellSelectionLogic[1] = {};
	spellbookPage.init();
	inventoryPage = 0;
	stoneInventory.clear();
	for (int wandIndex = 0; wandIndex < 2; wandIndex++)
	{
		for (int slotIndex = 0; slotIndex < 4; slotIndex++)
		{
			wandStoneSlots[wandIndex][slotIndex] = {};
		}
	}
	draggingStoneIndex = -1;
	draggingStoneOffset = {};
	draggingStone = false;
	controllerInventoryFocus = 0;
	controllerInventoryWandSlot = 0;
	controllerInventoryStoneIndex = 0;
	controllerInventorySelectedStoneIndex = -1;
	controllerInventoryHasSelectedStone = false;
	controllerInventoryStickLockX = false;
	controllerInventoryStickLockY = false;
	droppedItems.clear();
	summons.clear();
	cameraShakeSystem.clear();

	particlePostProcessRenderer.init();
	particlePostProcessRenderer.bloomEnabled = !disableParticleBloom;
	cosmeticDynamicLightSystem.init();
	cosmeticDynamicLightSystem.resetForFloor(map);
	gameHdrPostProcess.init();
	applyGlobalHdrToneMapSettings(gameHdrPostProcess);
	if (disableGameHdrTonemap)
	{
		gameHdrPostProcess.enabled = false;
	}
	minimapSystem.init();
	gameFbo.create(1, 1, true);
	paletteEffect.loadPalette();

	if (floorInfo.playerSpawnPos)
	{
		player.physics.teleport(*floorInfo.playerSpawnPos);
	}
	else
	{
		player.physics.getPos() = {35, 35};
	}
	player.resetHealth();
	playerDamageCooldown = 0.0f;

	// spawn a few wands and chests in rooms
	{
		std::vector<glm::vec2> spawnPositions;
		spawnPositions.reserve(48);
		std::vector<glm::vec2> preferredCenters;
		preferredCenters.reserve(16);

		// Avoid placing drops on tiles with walls beneath.
		auto isSpawnSpotValid = [&](glm::vec2 pos)
		{
			glm::ivec2 tile = WorldToTile(pos);
			if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
			{
				return false;
			}
			if (floorInfo.exitPos && glm::distance(pos, *floorInfo.exitPos) < 0.4f)
			{
				return false;
			}
			auto &over = map.secondLayer.getBlockUnsafe(tile.x, tile.y);
			if (over.type == Blocks::spikeTrap)
			{
				return false;
			}
			if (map.isCollidableAtPosSafe(tile.x, tile.y)) { return false; }
			int belowY = tile.y + 1;
			if (belowY < 0 || belowY >= map.size.y) { return false; }
			if (map.isCollidableAtPosSafe(tile.x, belowY)) { return false; }
			return true;
		};

		auto addUniquePos = [&](std::vector<glm::vec2> &list, glm::vec2 pos)
		{
			for (const auto &existing : list)
			{
				if (existing.x == pos.x && existing.y == pos.y)
				{
					return;
				}
			}
			list.push_back(pos);
		};
		for (const auto &room : floorInfo.rooms)
		{
			if (room.isSpawnRoom)
			{
				continue;
			}

			const auto &roomSpawns = room.wandSpawnPositions.empty()
				? room.enemySpawnPositions
				: room.wandSpawnPositions;

			for (const auto &pos : roomSpawns)
			{
				if (glm::distance(pos, player.physics.getPos()) < 3.0f)
				{
					continue;
				}
				if (!isSpawnSpotValid(pos)) { continue; }
				addUniquePos(spawnPositions, pos);
			}

			glm::vec2 centerPos = {
				(float)room.center().x + 0.5f,
				(float)room.center().y + 0.5f
			};
			if (glm::distance(centerPos, player.physics.getPos()) >= 3.0f && isSpawnSpotValid(centerPos))
			{
				addUniquePos(spawnPositions, centerPos);
				addUniquePos(preferredCenters, centerPos);
			}
		}

		auto popSpawn = [&](std::ranlux24_base &rng)
		{
			if (!preferredCenters.empty() && getRandomChance(rng, 0.35f))
			{
				int pick = getRandomInt(rng, 0, (int)preferredCenters.size() - 1);
				glm::vec2 pos = preferredCenters[pick];
				preferredCenters[pick] = preferredCenters.back();
				preferredCenters.pop_back();
				for (int i = 0; i < (int)spawnPositions.size(); i++)
				{
					if (spawnPositions[i].x == pos.x && spawnPositions[i].y == pos.y)
					{
						spawnPositions[i] = spawnPositions.back();
						spawnPositions.pop_back();
						break;
					}
				}
				return pos;
			}
			int index = getRandomInt(rng, 0, (int)spawnPositions.size() - 1);
			glm::vec2 pos = spawnPositions[index];
			spawnPositions[index] = spawnPositions.back();
			spawnPositions.pop_back();
			return pos;
		};

		auto isDropSpotFree = [&](glm::vec2 pos)
		{
			for (const auto &item : droppedItems.items)
			{
				if (glm::distance(item.pos, pos) < 0.5f)
				{
					return false;
				}
			}
			return true;
		};
		auto popFreeSpawn = [&](glm::vec2 &outPos)
		{
			while (!spawnPositions.empty())
			{
				glm::vec2 pos = popSpawn(rng);
				if (isDropSpotFree(pos))
				{
					outPos = pos;
					return true;
				}
			}
			return false;
		};

		int wandSpawnCount = 0;
		if (currentFloorIndex > 0)
		{
			int maxWandSpawns = std::min(5, (int)spawnPositions.size());
			int minWandSpawns = std::min(0, maxWandSpawns);
			wandSpawnCount = maxWandSpawns > 0 ? getRandomInt(rng, minWandSpawns, maxWandSpawns) : 0;
		}
		for (int i = 0; i < wandSpawnCount; i++)
		{
			glm::vec2 pos = {};
			if (!popFreeSpawn(pos)) { break; }
			int tier = getFloorWandTier(currentFloorIndex);
			droppedItems.spawnWand(pos, getRandomWand(tier, rng), rng);
		}

		int chestSpawnCount = 0;
		const float chestWandChance = 0.22f;
		if (currentFloorIndex > 0)
		{
			int maxChestSpawns = std::min(8, (int)spawnPositions.size());
			int minChestSpawns = std::min(2, maxChestSpawns);
			chestSpawnCount = maxChestSpawns > 0 ? getRandomInt(rng, minChestSpawns, maxChestSpawns) : 0;
		}
		for (int i = 0; i < chestSpawnCount; i++)
		{
			glm::vec2 pos = {};
			if (!popFreeSpawn(pos)) { break; }
			if (getRandomChance(rng, chestWandChance))
			{
				int tier = getFloorWandTier(currentFloorIndex);
				Wand chestWand = getRandomWand(tier, rng);
				droppedItems.spawnChest(pos, rng, &chestWand);
			}
			else
			{
				droppedItems.spawnChest(pos, rng);
			}
		}

		if (currentFloorIndex > 0 && floorInfo.exitRoomIndex)
		{
			const auto &exitRoom = floorInfo.rooms[*floorInfo.exitRoomIndex];
			std::vector<glm::vec2> exitSpawns = exitRoom.wandSpawnPositions.empty()
				? exitRoom.enemySpawnPositions
				: exitRoom.wandSpawnPositions;
			bool hasExitPos = floorInfo.exitPos.has_value();
			glm::vec2 exitPos = hasExitPos ? *floorInfo.exitPos : glm::vec2{};
			auto isExitBlocked = [&](glm::vec2 pos)
			{
				return hasExitPos && glm::distance(pos, exitPos) < 0.4f;
			};
			auto pickExitSpot = [&](glm::vec2 &outPos)
			{
				while (!exitSpawns.empty())
				{
					int index = getRandomInt(rng, 0, (int)exitSpawns.size() - 1);
					glm::vec2 pick = exitSpawns[index];
					exitSpawns[index] = exitSpawns.back();
					exitSpawns.pop_back();
					if (isSpawnSpotValid(pick) && isDropSpotFree(pick) && !isExitBlocked(pick))
					{
						outPos = pick;
						return true;
					}
				}
				return false;
			};
			auto pickExitOffsetSpot = [&](glm::vec2 &outPos)
			{
				if (!hasExitPos) { return false; }
				glm::vec2 offsets[] = {
					{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f},
					{1.0f, 1.0f}, {-1.0f, 1.0f}, {1.0f, -1.0f}, {-1.0f, -1.0f}
				};
				for (const auto &offset : offsets)
				{
					glm::vec2 pick = exitPos + offset;
					if (isSpawnSpotValid(pick) && isDropSpotFree(pick) && !isExitBlocked(pick))
					{
						outPos = pick;
						return true;
					}
				}
				return false;
			};
			glm::vec2 pos = {};
			bool foundPos = pickExitSpot(pos);
			if (!foundPos)
			{
				foundPos = pickExitOffsetSpot(pos);
			}
			if (foundPos)
			{
				int tier = getFloorWandTier(currentFloorIndex);
				Wand chestWand = getRandomWand(tier, rng);
				droppedItems.spawnChest(pos, rng, &chestWand);
			}

			glm::vec2 hearthPos = {};
			bool foundHearthPos = pickExitSpot(hearthPos);
			if (!foundHearthPos)
			{
				foundHearthPos = pickExitOffsetSpot(hearthPos);
			}
			if (foundHearthPos)
			{
				droppedItems.spawnChest(hearthPos, rng, nullptr, true);
			}
		}
	}

	// spawn enemies, avoid wand/chest pickup spots
	if (!disableRandomEnemySpawns)
	{
		const float itemBlockRadius = PIXEL_SIZE * 8.0f;
		const float itemBlockDist2 = itemBlockRadius * itemBlockRadius;
		auto isItemBlocking = [&](glm::vec2 pos)
		{
			for (const auto &item : droppedItems.items)
			{
				glm::vec2 diff = item.pos - pos;
				if (glm::dot(diff, diff) <= itemBlockDist2)
				{
					return true;
				}
			}
			return false;
		};

		int floorDifficulty = getFloorDifficulty(currentFloorIndex);
		for (int i = 0; i < (int)floorInfo.rooms.size(); i++)
		{
			const auto &room = floorInfo.rooms[i];
			if (room.isSpawnRoom)
			{
				continue;
			}

			if (room.enemySpawnPositions.empty())
			{
				continue;
			}

			int maxSpawns = std::min(2, (int)room.enemySpawnPositions.size());
			int spawnCount = getRandomInt(rng, 0, maxSpawns);

			auto spawnPositions = room.enemySpawnPositions;
			for (int j = 0; j < spawnCount; j++)
			{
				bool spawned = false;
				while (!spawnPositions.empty() && !spawned)
				{
					int index = getRandomInt(rng, 0, (int)spawnPositions.size() - 1);
					glm::vec2 pos = spawnPositions[index];
					spawnPositions[index] = spawnPositions.back();
					spawnPositions.pop_back();

					if (floorInfo.exitPos && glm::distance(pos, *floorInfo.exitPos) < 0.4f)
					{
						continue;
					}
					glm::ivec2 tile = WorldToTile(pos);
					if (tile.x >= 0 && tile.y >= 0 && tile.x < map.size.x && tile.y < map.size.y)
					{
						auto &over = map.secondLayer.getBlockUnsafe(tile.x, tile.y);
						if (over.type == Blocks::spikeTrap)
						{
							continue;
						}
					}
					if (isItemBlocking(pos))
					{
						continue;
					}
					spawnRandomEnemy(entityHolder, rng, pos, floorDifficulty);
					spawned = true;
				}
			}
		}
	}



	inGame = true;
	return true;
}

bool GameLogic::update(float deltaTime,
	gl2d::Renderer2D &renderer,
	AssetsManager &assetsManager,
	platform::Input &input)
{

	bool exitDungeon = false;

	if (!hasWand[activeWandIndex])
	{
		activeWandIndex = hasWand[0] ? 0 : 1;
	}

	auto getWandSlot = [&](Wand &wand, int slotIndex) -> WandSlot *
	{
		switch (slotIndex)
		{
			case 0: return &wand.up;
			case 1: return &wand.down;
			case 2: return &wand.left;
			case 3: return &wand.right;
			default: return nullptr;
		}
	};

	auto clearWandSlot = [&](WandSlot &slot)
	{
		slot.type = WandSlotType::Empty;
		slot.element = Elements::NoneElement;
		slot.castCount = 1;
	};

	auto clearStoneSlots = [&](int wandIndex)
	{
		for (int slotIndex = 0; slotIndex < 4; slotIndex++)
		{
			wandStoneSlots[wandIndex][slotIndex] = {};
		}
	};

	auto returnStonesFromWand = [&](int wandIndex, Wand &targetWand)
	{
		for (int slotIndex = 0; slotIndex < 4; slotIndex++)
		{
			auto &stoneSlot = wandStoneSlots[wandIndex][slotIndex];
			if (!stoneSlot.hasStone) { continue; }
			stoneInventory.push_back(stoneSlot.stone);
			stoneSlot = {};
			if (auto *slot = getWandSlot(targetWand, slotIndex))
			{
				clearWandSlot(*slot);
			}
		}
	};

	auto applyStoneToSlot = [&](int wandIndex, int slotIndex, const MagicStone &stone)
	{
		auto *slot = getWandSlot(wands[wandIndex], slotIndex);
		if (!slot) { return false; }
		bool replacingStone = wandStoneSlots[wandIndex][slotIndex].hasStone;
		if (!replacingStone && slot->type != WandSlotType::Empty) { return false; }
		if (replacingStone)
		{
			stoneInventory.push_back(wandStoneSlots[wandIndex][slotIndex].stone);
		}
		wandStoneSlots[wandIndex][slotIndex].hasStone = true;
		wandStoneSlots[wandIndex][slotIndex].stone = stone;
		slot->type = WandSlotType::Element;
		slot->element = stone.element;
		slot->castCount = stone.uses;
		return true;
	};

	auto validateQuickAction = [&](Wand &wand, QuickAction &action)
	{
		if (action.count <= 0) { return; }
		int maxElements = std::min(wand.maxElementsPerCast, QuickAction::MAX_ELEMENTS);
		if (maxElements < 1) { maxElements = 1; }
		int alwaysCastCount = wand.alwaysCast.type == WandSlotType::Element ? 1 : 0;
		if (action.count + alwaysCastCount > maxElements)
		{
			action.clear();
			return;
		}

		int available[Elements::Ice + 1] = {};
		auto addSlot = [&](const WandSlot &slot)
		{
			if (slot.type == WandSlotType::Element)
			{
				available[slot.element] += slot.castCount;
			}
		};
		addSlot(wand.up);
		addSlot(wand.down);
		addSlot(wand.left);
		addSlot(wand.right);

		for (int i = 0; i < action.count; i++)
		{
			int element = action.elements[i];
			if (element < Elements::NoneElement || element > Elements::Ice)
			{
				action.clear();
				return;
			}
			available[element]--;
			if (available[element] < 0)
			{
				action.clear();
				return;
			}
		}
	};

	auto validateQuickActions = [&](int wandIndex)
	{
		for (int slotIndex = 0; slotIndex < 4; slotIndex++)
		{
			validateQuickAction(wands[wandIndex], wands[wandIndex].quickActions[slotIndex]);
		}
	};

	// Resets in-inventory stone drag/select interactions.
	auto resetInventoryStoneInteraction = [&]()
	{
		draggingStoneIndex = -1;
		draggingStoneOffset = {};
		draggingStone = false;
		controllerInventoryFocus = 0;
		controllerInventoryWandSlot = 0;
		controllerInventoryStoneIndex = 0;
		controllerInventorySelectedStoneIndex = -1;
		controllerInventoryHasSelectedStone = false;
		controllerInventoryStickLockX = false;
		controllerInventoryStickLockY = false;
	};

	auto switchActiveWand = [&](int newIndex, bool pauseManaCharge)
	{
		if (newIndex < 0 || newIndex > 1) { return; }
		if (!hasWand[newIndex]) { return; }
		if (newIndex == activeWandIndex) { return; }
		int oldIndex = activeWandIndex;
		resetInventoryStoneInteraction();
		activeWandIndex = newIndex;
		spellSelectionLogic[oldIndex].resetSelectionForWand(
			wands[oldIndex], spellRecepies[oldIndex], false);
		spellSelectionLogic[newIndex].resetSelectionForWand(
			wands[newIndex], spellRecepies[newIndex], false);
		if (pauseManaCharge)
		{
			spellSelectionLogic[newIndex].pauseManaCharge = true;
		}
	};

	bool switchToSlot0 = input.buttons[platform::Button::NR1].pressed;
	bool switchToSlot1 = input.buttons[platform::Button::NR2].pressed;
	if (switchToSlot0) { switchActiveWand(0, true); }
	if (switchToSlot1) { switchActiveWand(1, true); }
	if (input.controller.buttons[platform::Controller::LBumper].pressed ||
		input.controller.buttons[platform::Controller::RBumper].pressed)
	{
		int otherIndex = activeWandIndex == 0 ? 1 : 0;
		switchActiveWand(otherIndex, true);
	}

	if (input.buttons[platform::Button::M].pressed ||
		input.controller.buttons[platform::Controller::Back].pressed)
	{
		mapViewerOpen = !mapViewerOpen;
		if (mapViewerOpen)
		{
			inventoryOpen = false;
			resetInventoryStoneInteraction();
			mapViewerCenter = player.physics.getPos();
			mapViewerViewSize = minimapSystem.viewSize;
		}
	}

	if (!mapViewerOpen && (input.buttons[platform::Button::Tab].pressed ||
		input.controller.buttons[platform::Controller::Start].pressed))
	{
		inventoryOpen = !inventoryOpen;
		resetInventoryStoneInteraction();
	}
	if (!inventoryOpen && (draggingStone || controllerInventoryHasSelectedStone))
	{
		resetInventoryStoneInteraction();
	}
	if (!inventoryOpen && !mapViewerOpen)
	{
		quickActionEditIndex = -1;
	}
	if (quickActionEditIndex < -1 || quickActionEditIndex > 3)
	{
		quickActionEditIndex = -1;
	}

	int wandCountBefore = (hasWand[0] ? 1 : 0) + (hasWand[1] ? 1 : 0);
	bool pickupInput = !inventoryOpen && !mapViewerOpen && !freeCameraMode && (input.buttons[platform::Button::E].pressed ||
		input.controller.buttons[platform::Controller::A].pressed);
	if (pickupInput)
	{
		const float pickupRadius = PIXEL_SIZE * 16.0f;
		int interactIndex = droppedItems.findClosestInteractableIndex(player.physics.getPos(),
			pickupRadius * pickupRadius);
		if (interactIndex >= 0)
		{
			if (droppedItems.items[interactIndex].type == DroppedItemType::Chest)
			{
				droppedItems.openChest(interactIndex, rng, particleSystem);
			}
			else if (droppedItems.items[interactIndex].type == DroppedItemType::Hearth)
			{
				if (droppedItems.takeHearth(interactIndex, particleSystem, rng))
				{
					player.healLife(2.0f);
				}
			}
			else if (droppedItems.items[interactIndex].type == DroppedItemType::Coin)
			{
				droppedItems.takeCoin(interactIndex, particleSystem, rng);
			}
			else if (droppedItems.items[interactIndex].type == DroppedItemType::MagicStone)
			{
				int stoneElement = Elements::NoneElement;
				int stoneUses = 1;
				if (droppedItems.takeMagicStone(interactIndex, particleSystem, rng,
					stoneElement, stoneUses))
				{
					stoneInventory.push_back({stoneElement, stoneUses});
				}
			}
			else
			{
				int pickedSlot = -1;
				bool swappedWand = false;
				int pickedItemIndex = -1;
				glm::vec2 pickupPos = droppedItems.items[interactIndex].pos;
				if (droppedItems.trySwapWithPlayerIndex(interactIndex, wands, hasWand,
					activeWandIndex, pickedSlot, swappedWand, pickedItemIndex))
				{
					droppedItems.emitWandPickupParticles(pickupPos, particleSystem, rng);
					if (pickedSlot >= 0)
					{
						if (swappedWand && pickedItemIndex >= 0
							&& pickedItemIndex < (int)droppedItems.items.size())
						{
							returnStonesFromWand(pickedSlot, droppedItems.items[pickedItemIndex].wand);
						}
						else
						{
							clearStoneSlots(pickedSlot);
						}
						spellSelectionLogic[pickedSlot].resetSelectionForWand(
							wands[pickedSlot], spellRecepies[pickedSlot], true);
						if (!swappedWand && wandCountBefore == 1)
						{
							switchActiveWand(pickedSlot, true);
						}
					}
				}
			}
		}
	}

	validateQuickActions(0);
	validateQuickActions(1);

	Wand &currentWand = wands[activeWandIndex];
	bool resetWorld = false;
	int resetWorldSeed = 0;


	#pragma region imgui
	//ImGui::ShowDemoWindow();
	if (input.buttons[platform::Button::F7].pressed)
	{
		removeLightSystem = !removeLightSystem;
	}
	if (input.buttons[platform::Button::F8].pressed)
	{
		renderCollidersFlag = !renderCollidersFlag;
	}
	if (input.buttons[platform::Button::F9].pressed)
	{
		if (!mapViewerOpen)
		{
			freeCameraMode = !freeCameraMode;
			if (freeCameraMode)
			{
				freeCameraPosition = renderer.currentCamera.position;
			}
		}
	}
	if (input.buttons[platform::Button::F10].pressed)
	{
		ImGui::toggleImguiWindowOpen();
	}
	if (ImGui::isImguiWindowOpen())
	{
		ImGui::Begin("Game Debug");

	if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat2("Position", &player.physics.getPos()[0], 0.01);
		ImGui::DragFloat("zoom", &zoom);
	}
	if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("World Seed: %d", worldSeed);
		ImGui::Text("Floor: %d / %d", currentFloorIndex, maxCombatFloors);
		ImGui::Checkbox("Render Colliders", &renderCollidersFlag);
		ImGui::Checkbox("Disable Light System", &removeLightSystem);
		ImGui::Checkbox("Disable 0.8 Light Layer", &disableSecondLightLayer);
		ImGui::Checkbox("Force Trap Difficulty", &forceTrapDifficulty);
		ImGui::SliderInt("Trap Difficulty", &trapDifficulty, 0, 3);
		if (ImGui::Button("Reset World (New Seed)"))
		{
			resetWorldSeed = getRandomInt(rng, 1, 2000000000);
			resetWorld = true;
		}
	}
	if (ImGui::CollapsingHeader("Wands", ImGuiTreeNodeFlags_DefaultOpen))
	{
		static int randomWandTier = 1;
		ImGui::SliderInt("Random Wand Tier", &randomWandTier, 0, 5);
		if (ImGui::Button("Random Wand"))
		{
			returnStonesFromWand(activeWandIndex, wands[activeWandIndex]);
			wands[activeWandIndex] = getRandomWand(randomWandTier, rng);
			spellSelectionLogic[activeWandIndex].resetSelectionForWand(
				wands[activeWandIndex], spellRecepies[activeWandIndex], true);
		}
		if (ImGui::Button("Add Elder Wand"))
		{
			// Hardcoded max-tier elder wand for quick testing.
			returnStonesFromWand(activeWandIndex, wands[activeWandIndex]);
			Wand elderWand;
			elderWand.up = {WandSlotType::Element, Elements::Fire, 4};
			elderWand.down = {WandSlotType::Element, Elements::Water, 4};
			elderWand.left = {WandSlotType::Element, Elements::Earth, 4};
			elderWand.right = {WandSlotType::Element, Elements::Ice, 4};
			elderWand.alwaysCast = {WandSlotType::Disabled};
			elderWand.maxMana = 15;
			elderWand.manaChargeSpeed = 1.3f;
			elderWand.maxElementsPerCast = 7;
			elderWand.wandSprite = Wand::elderWand;
			elderWand.sanitize();
			wands[activeWandIndex] = elderWand;
			spellSelectionLogic[activeWandIndex].resetSelectionForWand(
				wands[activeWandIndex], spellRecepies[activeWandIndex], true);
		}

		ImGui::Separator();
		ImGui::Text("Magic Stones");
		if (ImGui::Button("Add Fire Stone"))
		{
			stoneInventory.push_back({Elements::Fire, 2});
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Water Stone"))
		{
			stoneInventory.push_back({Elements::Water, 2});
		}
		if (ImGui::Button("Add Earth Stone"))
		{
			stoneInventory.push_back({Elements::Earth, 2});
		}
		ImGui::SameLine();
		if (ImGui::Button("Add Ice Stone"))
		{
			stoneInventory.push_back({Elements::Ice, 2});
		}
		if (ImGui::Button("Remove Last Stone") && !stoneInventory.empty())
		{
			stoneInventory.pop_back();
		}

		ImGui::Separator();
		ImGui::Text("Wand Elements");
		ImGui::Text("Max Elements Per Cast: %d", currentWand.maxElementsPerCast);
		int elementUses[Elements::Ice + 1] = {};
		auto addElementUses = [&](const WandSlot &slot)
		{
			if (slot.type == WandSlotType::Element)
			{
				elementUses[slot.element] += slot.castCount;
			}
		};
		addElementUses(currentWand.up);
		addElementUses(currentWand.down);
		addElementUses(currentWand.left);
		addElementUses(currentWand.right);
		addElementUses(currentWand.alwaysCast);

		const char *elementNames[] = {"None", "Fire", "Water", "Earth", "Ice"};
		for (int element = Elements::Fire; element <= Elements::Ice; element++)
		{
			if (elementUses[element] > 0)
			{
				ImGui::Text("%s: %d", elementNames[element], elementUses[element]);
			}
		}
		if (currentWand.alwaysCast.type == WandSlotType::Element)
		{
			ImGui::Text("Always Cast: %s x%d", elementNames[currentWand.alwaysCast.element],
				currentWand.alwaysCast.castCount);
		}
		else if (currentWand.alwaysCast.type == WandSlotType::Empty)
		{
			ImGui::Text("Always Cast: Empty");
		}
		else
		{
			ImGui::Text("Always Cast: Disabled");
		}

		ImGui::Separator();
		ImGui::Text("Wand Slots");
		const char *slotNames[] = {"Up", "Down", "Left", "Right"};
		const WandSlot *slots[] = {&currentWand.up, &currentWand.down, &currentWand.left, &currentWand.right};
		for (int i = 0; i < 4; i++)
		{
			auto &slot = *slots[i];
			if (slot.type == WandSlotType::Element)
			{
				ImGui::Text("%s: %s x%d", slotNames[i], elementNames[slot.element], slot.castCount);
			}
			else if (slot.type == WandSlotType::Empty)
			{
				ImGui::Text("%s: Empty", slotNames[i]);
			}
			else
			{
				ImGui::Text("%s: Disabled", slotNames[i]);
			}
		}
	}

	ImGui::Separator();
	static bool particleUseVelocity = false;
	if (ImGui::CollapsingHeader("Particles"))
	{
		ImGui::Checkbox("Particle Velocity", &particleUseVelocity);

	auto spawnParticleTest = [&](ParticleSettings particle)
	{
		if (particleUseVelocity)
		{
			float speed = 40.0f * PIXEL_SIZE;
			glm::vec2 screenCenterDebug = {renderer.windowW / 2.f, renderer.windowH / 2.f};
			glm::vec2 dir = glm::vec2(platform::getRelMousePosition()) - screenCenterDebug;
			if (glm::length(dir) <= 0.0001f)
			{
				dir = fireDirection;
			}
			if (glm::length(dir) <= 0.0001f)
			{
				dir = {1.0f, 0.0f};
			}
			dir = glm::normalize(dir);
			particle.velocityX = {dir.x * speed, dir.x * speed};
			particle.velocityY = {dir.y * speed, dir.y * speed};
		}

		particleSystem.emitParticles(particle, player.physics.getPos(), rng, player.physics.getPos());
	};

	if (ImGui::Button("Particle Orbit"))
	{
		spawnParticleTest(getOrbitParticle({0.6f, 0.9f, 1.0f, 0.9f}, {0.2f, 0.5f, 1.0f, 0.7f}));
	}
	if (ImGui::Button("Particle Atom"))
	{
		spawnParticleTest(getAtomParticle({1.0f, 0.7f, 0.3f, 0.9f}, {1.0f, 0.4f, 0.2f, 0.7f}));
	}
	if (ImGui::Button("Particle ZigZag"))
	{
		spawnParticleTest(getZigZagParticle({0.7f, 1.0f, 0.5f, 0.9f}, {0.3f, 0.8f, 0.4f, 0.7f}));
	}
	if (ImGui::Button("Particle Spiral"))
	{
		spawnParticleTest(getSpiralParticle({0.8f, 0.6f, 1.0f, 0.9f}, {0.4f, 0.2f, 0.9f, 0.7f}));
	}
	if (ImGui::Button("Particle Figure8"))
	{
		spawnParticleTest(getFigure8Particle({0.8f, 0.9f, 0.6f, 0.9f}, {0.5f, 0.8f, 0.3f, 0.7f}));
	}
	if (ImGui::Button("Particle Bob"))
	{
		spawnParticleTest(getBobParticle({0.9f, 0.7f, 0.5f, 0.9f}, {0.6f, 0.4f, 0.3f, 0.7f}));
	}

	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Palette"))
	{
		ImGui::Checkbox("Palette Particles", &paletteEffect.enabledParticles);
		ImGui::Checkbox("Palette Game", &paletteEffect.enabledGame);
		if (!paletteEffect.hasPalette())
		{
			ImGui::Text("Palette: not loaded");
		}
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("HDR Tonemap"))
	{
		// Runtime controls for the fullscreen HDR tone mapping pass.
		ImGui::Checkbox("Enable HDR Tonemap", &gameHdrPostProcess.enabled);

		const char *tonemapperNames[GameHdrPostProcess::ToneMapper_Count] = {
			"ACES Fitted",
			"AGX",
			"ZCAM",
			"Uncharted2",
			"PBR Neutral"
		};

		int tonemapper = std::clamp(gameHdrPostProcess.toneMapper, 0,
			GameHdrPostProcess::ToneMapper_Count - 1);
		if (ImGui::Combo("Tonemapper", &tonemapper,
			tonemapperNames, GameHdrPostProcess::ToneMapper_Count))
		{
			gameHdrPostProcess.toneMapper = tonemapper;
		}

		ImGui::DragFloat("Exposure", &gameHdrPostProcess.exposure, 0.01f, 0.0f, 8.0f, "%.2f");
		ImGui::DragFloat("Saturation", &gameHdrPostProcess.saturation, 0.01f, 0.0f, 2.5f, "%.2f");
		ImGui::DragFloat("Vibrance", &gameHdrPostProcess.vibrance, 0.01f, 0.0f, 2.5f, "%.2f");
		ImGui::DragFloat("Grading Gamma", &gameHdrPostProcess.gamma, 0.01f, 0.1f, 4.0f, "%.2f");
		ImGui::DragFloat("Shadow Boost", &gameHdrPostProcess.shadowBoost, 0.01f, -1.0f, 2.0f, "%.2f");
		ImGui::DragFloat("Highlight Boost", &gameHdrPostProcess.highlightBoost, 0.01f, -1.0f, 2.0f, "%.2f");
		ImGui::DragFloat("Vignette", &gameHdrPostProcess.vignette, 0.01f, 0.0f, 1.0f, "%.2f");
		// Compact color controls (no large picker popup) for lift/gain grading.
		const ImGuiColorEditFlags liftGainColorFlags = ImGuiColorEditFlags_Float
			| ImGuiColorEditFlags_HDR
			| ImGuiColorEditFlags_NoPicker;
		ImGui::ColorEdit3("Lift", &gameHdrPostProcess.lift[0], liftGainColorFlags);
		ImGui::ColorEdit3("Gain", &gameHdrPostProcess.gain[0], liftGainColorFlags);
	}
	storeGlobalHdrToneMapSettings(gameHdrPostProcess);

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Cosmetic Dynamic Light"))
	{
		// Cosmetic-only LOS lighting layered on top of gameplay visibility.
		ImGui::Checkbox("Enable Cosmetic Light", &cosmeticDynamicLightSystem.enabled);
		ImGui::DragFloat("Global Extra Light", &cosmeticDynamicLightSystem.ambientLight,
			0.01f, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat("Player Light Radius", &cosmeticDynamicLightSystem.playerLightRadius, 0.05f, 0.5f, 32.0f, "%.2f");
		ImGui::DragFloat("Player Light Intensity", &cosmeticDynamicLightSystem.playerLightIntensity, 0.01f, 0.0f, 4.0f, "%.2f");
		ImGui::DragFloat("Player Light Falloff", &cosmeticDynamicLightSystem.playerLightFalloffPower,
			0.01f, 0.1f, 6.0f, "%.2f");
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Enemy Projectiles"))
	{
		if (ImGui::Button("Shoot Enemy Orb"))
		{
			EnemyOrbProjectile orb;
			orb.targetPlayer = &player;
			orb.targetSummons = &summons;
			orb.showCollider = true;
			// Shoot in aim direction with offset
			glm::vec2 screenCenterDebug = {renderer.windowW / 2.f, renderer.windowH / 2.f};
			glm::vec2 dir = glm::vec2(platform::getRelMousePosition()) - screenCenterDebug;
			if (glm::length(dir) > 0.0001f)
			{
				dir = glm::normalize(dir);
			}
			else
			{
				dir = {1.0f, 0.0f};
			}
			orb.setDirection(dir);
			glm::vec2 spawnPos = player.physics.getPos() + dir * 1.5f;
			projectiles.addProjectile(orb, spawnPos);
		}
	}

	ImGui::Separator();
	if (ImGui::CollapsingHeader("Spawn Enemies"))
	{
		auto spawnEnemyNearPlayer = [&](auto enemy)
		{
			glm::vec2 spawnPos = player.physics.getPos() + glm::vec2(1.6f, 0.0f);
			entityHolder.addEntity(enemy, spawnPos);
		};
		if (ImGui::Button("Random"))
		{
			spawnRandomEnemy(entityHolder, rng, player.physics.getPos() + glm::vec2(1.6f, 0.0f),
				getFloorDifficulty(currentFloorIndex));
		}
		ImGui::SameLine();
		if (ImGui::Button("Templar"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getTemplarOriginalEnemy());
		}
		ImGui::SameLine();
		if (ImGui::Button("Earth Templar"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getEarthTemplarEnemy());
		}
		if (ImGui::Button("Fire Templar"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getFireTemplarEnemy());
		}
		ImGui::SameLine();
		if (ImGui::Button("Ice Templar"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getIceTemplarEnemy());
		}
		ImGui::SameLine();
		if (ImGui::Button("Water Templar"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getWaterTemplarEnemy());
		}
		if (ImGui::Button("Goblin Archer"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getGoblinArcherEnemy());
		}
		ImGui::SameLine();
		if (ImGui::Button("Goblin Spearman"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getGoblinSpearmanEnemy());
		}
		ImGui::SameLine();
		if (ImGui::Button("Goblin Heavy"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getGoblinHeavyEnemy());
		}
		if (ImGui::Button("Goblin Thief"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getGoblinThiefEnemy());
		}
		ImGui::SameLine();
		if (ImGui::Button("Orc Archer"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getOrcArcherEnemy());
		}
		ImGui::SameLine();
		if (ImGui::Button("Dark Angel"))
		{
			spawnEnemyNearPlayer(EnemyTypes::getDarkAngelEnemy());
		}
	}

	if (ImGui::Button("Exit"))
	{
		exitDungeon = true;
	}

		ImGui::End();
		renderSpellRecepieWindow(spellsHolder, player, fireDirection);
	}
	roomLightingSystem.enableLowLightLayer = !disableSecondLightLayer;
	if (resetWorld)
	{
		close();
		worldSeed = resetWorldSeed;
		init();
		return true;
	}
#pragma endregion

	glm::vec2 cursorPos = platform::getRelMousePosition();
	glm::vec4 viewRect = renderer.getViewRect();
	glm::vec2 screenCenter = {renderer.windowW / 2.f, renderer.windowH / 2.f};
	static bool usesController = 0;
	{
		const auto &controllerButtons = input.controller.buttons;
		bool controllerUsed = glm::length(platform::getControllerButtons().LStick) > 0.1f ||
			glm::length(platform::getControllerButtons().RStick) > 0.1f ||
			controllerButtons[platform::Controller::LBumper].pressed ||
			controllerButtons[platform::Controller::RBumper].pressed ||
			controllerButtons[platform::Controller::A].pressed ||
			controllerButtons[platform::Controller::B].pressed ||
			controllerButtons[platform::Controller::X].pressed ||
			controllerButtons[platform::Controller::Y].pressed ||
			controllerButtons[platform::Controller::Up].pressed ||
			controllerButtons[platform::Controller::Down].pressed ||
			controllerButtons[platform::Controller::Left].pressed ||
			controllerButtons[platform::Controller::Right].pressed ||
			controllerButtons[platform::Controller::Start].pressed ||
			controllerButtons[platform::Controller::Back].pressed ||
			controllerButtons[platform::Controller::LThumb].pressed ||
			controllerButtons[platform::Controller::RThumb].pressed ||
			input.controller.LTButton.pressed ||
			input.controller.RTButton.pressed;

		bool keyboardUsed = input.buttons[platform::Button::A].pressed ||
			input.buttons[platform::Button::D].pressed ||
			input.buttons[platform::Button::W].pressed ||
			input.buttons[platform::Button::S].pressed ||
			input.buttons[platform::Button::Q].pressed ||
			input.buttons[platform::Button::E].pressed ||
			input.buttons[platform::Button::M].pressed ||
			input.buttons[platform::Button::NR1].pressed ||
			input.buttons[platform::Button::NR2].pressed ||
			input.buttons[platform::Button::Tab].pressed;

		bool mouseUsed = platform::isLMouseHeld() || platform::isRMouseHeld() || platform::mouseMoved();

		if (controllerUsed)
		{
			usesController = true;
		}
		if (mouseUsed || keyboardUsed)
		{
			usesController = false;
		}
	}

	// pause gameplay updates while inventory/map viewer is open
	float simDelta = (inventoryOpen || mapViewerOpen) ? 0.0f : deltaTime;
	wandHoverTimer += deltaTime;

	{
		auto statusTick = updateStatusEffects(player.statusEffects, player.statusImmunities, simDelta);
		player.statusSpeedMultiplier = statusTick.speedMultiplier;
		if (statusTick.damage > 0.0f)
		{
			player.applyDamage(statusTick.damage);
			glm::vec2 damagePos = player.physics.getPos();
			damagePos.y -= player.physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(statusTick.damage, damagePos);
		}
		updateStatusEffectParticles(player.statusEffects, particleSystem, rng, player.physics.getPos(), simDelta);
	}

	const float freeCameraZoomMin = 10.0f;
	const float freeCameraZoomMax = 100.0f;

#pragma region input
	{
		if (mapViewerOpen)
		{
			float zoomInput = 0.0f;
			if (platform::isButtonHeld(platform::Button::Q)) { zoomInput -= 1.0f; } // zoom in
			if (platform::isButtonHeld(platform::Button::E)) { zoomInput += 1.0f; } // zoom out
			if (zoomInput != 0.0f)
			{
				mapViewerViewSize += zoomInput * 16.0f * deltaTime;
			}
			float maxView = (float)std::max(map.size.x, map.size.y) + 8.0f;
			mapViewerViewSize = glm::clamp(mapViewerViewSize, 20.0f, maxView);

			glm::vec2 camMove = {};
			if (platform::isButtonHeld(platform::Button::A)) { camMove.x -= 1.0f; }
			if (platform::isButtonHeld(platform::Button::D)) { camMove.x += 1.0f; }
			if (platform::isButtonHeld(platform::Button::W)) { camMove.y -= 1.0f; }
			if (platform::isButtonHeld(platform::Button::S)) { camMove.y += 1.0f; }
			if (glm::length(camMove) > 0.0001f)
			{
				camMove = glm::normalize(camMove);
				float moveSpeed = std::max(12.0f, mapViewerViewSize * 0.9f);
				mapViewerCenter += camMove * moveSpeed * deltaTime;
			}

			float halfView = mapViewerViewSize * 0.5f;
			float minX = halfView;
			float minY = halfView;
			float maxX = std::max(minX, (float)map.size.x - halfView);
			float maxY = std::max(minY, (float)map.size.y - halfView);
			mapViewerCenter.x = glm::clamp(mapViewerCenter.x, minX, maxX);
			mapViewerCenter.y = glm::clamp(mapViewerCenter.y, minY, maxY);

			fireInputActive = false;
			usesController = false;
		}

		if (freeCameraMode && !mapViewerOpen)
		{

			const float freeCameraZoomTravelSeconds = 1.f;
			const float freeCameraMoveSpeedRatio = 0.6f;

			// Free camera input: WASD pan, Q/E zoom.
			float zoomInput = 0.0f;
			if (platform::isButtonHeld(platform::Button::Q))
			{
				zoomInput -= 1.0f;
			}
			if (platform::isButtonHeld(platform::Button::E))
			{
				zoomInput += 1.0f;
			}
			if (zoomInput != 0.0f)
			{
				float zoomSpeed = (freeCameraZoomMax - freeCameraZoomMin) / freeCameraZoomTravelSeconds;
				zoom += zoomInput * zoomSpeed * deltaTime;
			}
			zoom = glm::clamp(zoom, freeCameraZoomMin, freeCameraZoomMax);

			glm::vec2 camMove = {};
			if (platform::isButtonHeld(platform::Button::A))
			{
				camMove.x -= 1;
			}
			if (platform::isButtonHeld(platform::Button::D))
			{
				camMove.x += 1;
			}
			if (platform::isButtonHeld(platform::Button::W))
			{
				camMove.y -= 1;
			}
			if (platform::isButtonHeld(platform::Button::S))
			{
				camMove.y += 1;
			}

			if (glm::length(camMove) > 0.0001f)
			{
				camMove = glm::normalize(camMove);
				float viewW = renderer.windowW / zoom;
				float viewH = renderer.windowH / zoom;
				float cameraSpeed = std::min(viewW, viewH) * freeCameraMoveSpeedRatio;
				freeCameraPosition += camMove * cameraSpeed * deltaTime;
			}

			fireInputActive = false;
			usesController = false;
		}

		if (!inventoryOpen && !mapViewerOpen && !freeCameraMode)
		{
			glm::vec2 move = {};
			if (platform::isButtonHeld(platform::Button::A))
			{
				move.x -= 1;
				usesController = false;
			}
			if (platform::isButtonHeld(platform::Button::D))
			{
				move.x += 1;
				usesController = false;
			}
			if (platform::isButtonHeld(platform::Button::W))
			{
				move.y -= 1;
				usesController = false;
			}
			if (platform::isButtonHeld(platform::Button::S))
			{
				move.y += 1;
				usesController = false;
			}

			if (glm::length(platform::getControllerButtons().LStick) > 0.1 || glm::length(platform::getControllerButtons().RStick) > 0.1)
			{
				usesController = true;
			}

			if (platform::isLMouseHeld() || platform::isRMouseHeld() || platform::mouseMoved())
			{
				usesController = false;
			}

			fireInputActive = platform::mouseMoved() ||
				(glm::length(platform::getControllerButtons().RStick) > 0.4f);

			move += platform::getControllerButtons().LStick * glm::vec2(1, -1);

			if (glm::length(move) != 0)
			{
				move = glm::normalize(move);
				move *= simDelta * 6.f * player.statusSpeedMultiplier; //player speed
			}

			player.physics.getPos() += move;
			player.animator.setAnimationBasedOnMovement(move);

			//fire dirrection
			{
				if (!usesController)
				{
					fireDirection = cursorPos - screenCenter;
				}
				else
				{
					auto c = platform::getControllerButtons().RStick;
					float l = glm::length(c);
					if (l > 0.4)
					{
						fireDirection = c * glm::vec2(1,-1);
					}
				}

				float l = glm::length(fireDirection);
				float aimStrength = 0.0f;
				if (usesController)
				{
					float stickLen = glm::length(platform::getControllerButtons().RStick);
					if (stickLen > 0.4f)
					{
						aimStrength = glm::clamp((stickLen - 0.4f) / 0.6f, 0.0f, 1.0f);
					}
				}
				else
				{
					float threshold = std::min(renderer.windowW, renderer.windowH) / 3.0f;
					if (l > threshold && threshold > 0.0f)
					{
						aimStrength = glm::clamp((l - threshold) / threshold, 0.0f, 1.0f);
					}
				}
				if (l <= 0.000000001)
				{
					fireDirection = {1,0};
				}
				else
				{
					fireDirection /= l;
				}
				player.aimDirection = fireDirection;
				player.aimStrength = aimStrength;

				if (!usesController)
				{
					fireTargetPos = {
						viewRect.x + (cursorPos.x / renderer.windowW) * viewRect.z,
						viewRect.y + (cursorPos.y / renderer.windowH) * viewRect.w
					};
				}
				else
				{
					fireTargetPos = player.physics.getPos() + fireDirection * 1000.0f;
				}
			}
		}
		else if (!freeCameraMode)
		{
			fireInputActive = false;
		}

		platform::showMouse(!usesController);

	}
#pragma endregion

#pragma region updates

	player.physics.resolveConstrains(map);

	player.physics.updateMove();

	standbyProjectiles.update(simDelta, map, projectiles, rng, player, entityHolder,
		fireDirection, fireInputActive);
	projectiles.update(simDelta, map, particleSystem, rng, entityHolder);

	particleSystem.update(simDelta);
	damageViewerSystem.update(simDelta);
	droppedItems.update(simDelta, particleSystem, rng);

	summons.update(simDelta, map, particleSystem, projectiles, rng, player, entityHolder);

#pragma endregion


	zoom = glm::clamp(zoom, freeCameraZoomMin, freeCameraZoomMax);
	renderer.currentCamera.zoom = zoom;
	renderer.currentCamera.rotation = 0.0f;
	if (freeCameraMode)
	{
		renderer.currentCamera.position = freeCameraPosition;
	}
	else
	{
		renderer.currentCamera.follow(player.physics.transform.getCenter(),
			simDelta * 4.f, 0.00001, 0,
			renderer.windowW, renderer.windowH);
	}

	cameraShakeSystem.update(simDelta);
	auto shake = cameraShakeSystem.getCurrent();
	renderer.currentCamera.position += shake.offset;
	renderer.currentCamera.rotation += shake.rotation;

	particlePostProcessRenderer.updateWindowMetrics(renderer);

	const RoomLightingSystem *mapLighting = removeLightSystem ? nullptr : &roomLightingSystem;
	if (!mapViewerOpen)
	{
		// Build minimap texture before main world batching to avoid a mid-frame swapchain flush.
		minimapSystem.update(renderer, map, doorHolder, player.physics.getPos(), &floorInfo,
			mapLighting);
	}

	// Render particles into the post-process FBO early so FBO binds do not force a swapchain submit.
	particleSystem.render(renderer, particlePostProcessRenderer, {});

	// Build the cosmetic LOS light mask from CPU visibility and keep it separate from gameplay fog.
	if (gameHdrPostProcess.enabled)
	{
		cosmeticDynamicLightSystem.beginFrame(map);
		cosmeticDynamicLightSystem.addLight(player.physics.transform.getCenter(),
			cosmeticDynamicLightSystem.playerLightRadius,
			cosmeticDynamicLightSystem.playerLightIntensity,
			cosmeticDynamicLightSystem.playerLightFalloffPower,
			true);
		cosmeticDynamicLightSystem.buildLightMask(map);
		cosmeticDynamicLightSystem.updateWindowMetrics(renderer);
		cosmeticDynamicLightSystem.renderMask(renderer, map);
		gameHdrPostProcess.setCosmeticLightMaskTexture(cosmeticDynamicLightSystem.getMaskTexture());
	}

#pragma region rendering
	bool paletteGame = paletteEffect.enabledGame && paletteEffect.hasPalette();
	bool paletteParticles = paletteEffect.enabledParticles && paletteEffect.hasPalette() && !paletteGame;
	bool hdrGameTonemap = gameHdrPostProcess.beginScene(renderer);
	if (hdrGameTonemap)
	{
		// HDR path owns the main scene target, so we skip legacy palette readback.
		paletteGame = false;
		paletteParticles = paletteEffect.enabledParticles && paletteEffect.hasPalette();
	}
	if (paletteGame)
	{
		gameFbo.resize(renderer.windowW, renderer.windowH);
		gameFbo.clear();
		gameFbo.bind();
	}

	map.firstLayer.renderMap(renderer, assetsManager);

	// Render spike traps after the floor.
	if (!trapSpikes.spikes.empty())
	{
		auto viewRect = renderer.getViewRect();
		glm::ivec4 viewRectInt = {};
		viewRectInt.x = int(viewRect.x) - 1;
		viewRectInt.y = int(viewRect.y) - 2;
		viewRectInt.z = int(viewRect.z + 1.5f) + 2;
		viewRectInt.w = int(viewRect.w + 2.5f) + 2;
		viewRectInt.z += viewRect.x;
		viewRectInt.w += viewRect.y;
		viewRectInt = glm::clamp(viewRectInt, {0, 0, 0, 0},
			{map.size.x - 1, map.size.y - 1, map.size.x - 1, map.size.y - 1});
		auto &spikeSet = assetsManager.spikeTrap;
		for (const auto &spike : trapSpikes.spikes)
		{
			if (spike.pos.x < viewRectInt.x || spike.pos.x >= viewRectInt.z
				|| spike.pos.y < viewRectInt.y || spike.pos.y >= viewRectInt.w)
			{
				continue;
			}
			int frameX = 0;
			switch (spike.state)
			{
				case TrapSpikeElement::State::Closed:
				case TrapSpikeElement::State::OpeningDelay:
					frameX = 0;
					break;
				case TrapSpikeElement::State::Opening:
				{
					float progress = 1.0f - (spike.stateTimer / SpikeTrapSettings::OpenAnimSeconds);
					int step = std::min(2, (int)(progress * 3.0f));
					frameX = 1 + std::max(0, step);
				} break;
				case TrapSpikeElement::State::Open:
					frameX = 4;
					break;
				case TrapSpikeElement::State::Closing:
				{
					float progress = 1.0f - (spike.stateTimer / SpikeTrapSettings::CloseAnimSeconds);
					int step = std::min(2, (int)(progress * 3.0f));
					frameX = 5 + std::max(0, step);
				} break;
			}
			if (spikeSet.texture.isValid())
			{
				glm::vec4 rect = {
					(float)spike.pos.x,
					(float)spike.pos.y - 1.0f,
					1.0f,
					2.0f
				};
				renderer.renderRectangle(rect, spikeSet.texture, Colors_White, {}, 0,
					spikeSet.atlas.get(frameX, 0));
			}
			if (renderColliders())
			{
				glm::vec2 center = {
					spike.pos.x + 0.5f,
					spike.pos.y + 0.5f
				};
				renderer.renderCircleOutline(center, 0.45f, {1.0f, 0.35f, 0.2f, 0.55f},
					PIXEL_SIZE * 0.6f, 16);
			}
		}
	}

	map.secondLayer.renderMap(renderer, assetsManager);

	map.renderWallShadows(renderer, assetsManager);

	spellsHolder.renderBeforeEntities(renderer, particlePostProcessRenderer);
	droppedItems.render(renderer, assetsManager, player.physics.getPos(), usesController);

	entityHolder.update(simDelta, map, particleSystem, rng, player, summons, projectiles);
	resolveEntityPush(entityHolder, player);
	resolveSummonEntityPush(entityHolder, summons, player);

	// Break decorations on contact with player or enemies.
	if (!breakableDecorations.positions.empty())
	{
		breakDecorationsAtCollider(player.physics.transform);
		for (auto &entity : entityHolder.entities)
		{
			if (entity->dying) { continue; }
			if (entity->isFlying) { continue; }
			breakDecorationsAtCollider(entity->physics.transform);
		}
	}

	// Spike trap behavior and damage.
	if (!trapSpikes.spikes.empty())
	{
		auto emitSpikeTriggerParticles = [&](glm::vec2 pos)
		{
			glm::vec4 startColor = {1.0f, 0.35f, 0.15f, 0.9f};
			glm::vec4 endColor = {0.85f, 0.15f, 0.05f, 0.5f};
			ParticleSettings sparks = getSmallSquareParticle(startColor, endColor);
			sparks.onCreateCount = 8;
			sparks.particleLifeTime = {0.2f, 0.4f};
			sparks.velocityX = glm::vec2{-25.0f, 25.0f} * PIXEL_SIZE;
			sparks.velocityY = glm::vec2{-18.0f, -4.0f} * PIXEL_SIZE;
			sparks.dragX = glm::vec2{-80.0f, -140.0f} * PIXEL_SIZE;
			sparks.dragY = glm::vec2{-80.0f, -140.0f} * PIXEL_SIZE;
			sparks.positionX = glm::vec2{-3.0f, 3.0f} * PIXEL_SIZE;
			sparks.positionY = glm::vec2{-3.0f, 3.0f} * PIXEL_SIZE;
			particleSystem.emitParticles(sparks, pos, rng, pos);
		};

		auto isOnSpike = [&](const glm::vec4 &aabb, const glm::ivec2 &tile)
		{
			glm::vec4 spikeRect = {(float)tile.x, (float)tile.y, 1.0f, 1.0f};
			return checkCollisionRecs(aabb, spikeRect);
		};

		for (auto &spike : trapSpikes.spikes)
		{
			spike.damageCooldown = std::max(0.0f, spike.damageCooldown - simDelta);
			glm::vec4 playerAabb = player.physics.getAABB();
			bool playerOn = isOnSpike(playerAabb, spike.pos);
			if (playerOn)
			{
				if (spike.state == TrapSpikeElement::State::Closed)
				{
					spike.state = TrapSpikeElement::State::OpeningDelay;
					spike.stateTimer = SpikeTrapSettings::OpenDelaySeconds;
					spike.queuedOpen = false;
					emitSpikeTriggerParticles({spike.pos.x + 0.5f, spike.pos.y + 0.5f});
				}
				else if (spike.state == TrapSpikeElement::State::Open)
				{
					spike.stateTimer = SpikeTrapSettings::OpenHoldSeconds;
				}
				else if (spike.state == TrapSpikeElement::State::Closing)
				{
					spike.queuedOpen = true;
				}
			}

			switch (spike.state)
			{
				case TrapSpikeElement::State::Closed:
					break;
				case TrapSpikeElement::State::OpeningDelay:
					spike.stateTimer -= simDelta;
					if (spike.stateTimer <= 0.0f)
					{
						spike.state = TrapSpikeElement::State::Opening;
						spike.stateTimer = SpikeTrapSettings::OpenAnimSeconds;
					}
					break;
				case TrapSpikeElement::State::Opening:
					spike.stateTimer -= simDelta;
					if (spike.stateTimer <= 0.0f)
					{
						spike.state = TrapSpikeElement::State::Open;
						spike.stateTimer = SpikeTrapSettings::OpenHoldSeconds;
					}
					break;
				case TrapSpikeElement::State::Open:
					spike.stateTimer -= simDelta;
					if (spike.stateTimer <= 0.0f)
					{
						spike.state = TrapSpikeElement::State::Closing;
						spike.stateTimer = SpikeTrapSettings::CloseAnimSeconds;
					}
					break;
				case TrapSpikeElement::State::Closing:
					spike.stateTimer -= simDelta;
					if (spike.stateTimer <= 0.0f)
					{
						if (spike.queuedOpen)
						{
							spike.state = TrapSpikeElement::State::OpeningDelay;
							spike.stateTimer = SpikeTrapSettings::OpenDelaySeconds;
							spike.queuedOpen = false;
							emitSpikeTriggerParticles({spike.pos.x + 0.5f, spike.pos.y + 0.5f});
						}
						else
						{
							spike.state = TrapSpikeElement::State::Closed;
						}
					}
					break;
			}

			if (spike.state == TrapSpikeElement::State::Open && spike.damageCooldown <= 0.0f)
			{
				bool didDamage = false;
				if (playerOn)
				{
					player.applyDamage(1.0f);
					didDamage = true;
					glm::vec2 damagePos = player.physics.getPos();
					damagePos.y -= player.physics.transform.size.y * 0.6f;
					getDamageViewerSystem().addDamage(1.0f, damagePos);
				}
				HitStats hitStats = {};
				hitStats.damage = 1.0f;
				for (auto &entity : entityHolder.entities)
				{
					if (entity->dying) { continue; }
					if (entity->isFlying) { continue; }
					if (!isOnSpike(entity->physics.getAABB(), spike.pos)) { continue; }
					glm::vec2 push = {};
					entity->life.computeHit(hitStats, Elements::NoneElement, entity->element, {}, push);
					entity->onDamaged(hitStats.damage);
					didDamage = true;
					glm::vec2 damagePos = entity->physics.getPos();
					damagePos.y -= entity->physics.transform.size.y * 0.6f;
					getDamageViewerSystem().addDamage(1.0f, damagePos);
				}
				if (didDamage)
				{
					spike.damageCooldown = SpikeTrapSettings::DamageCooldownSeconds;
				}
			}
		}
	}

	// contact damage from enemies
	playerDamageCooldown = std::max(0.0f, playerDamageCooldown - simDelta);
	if (playerDamageCooldown <= 0.0f)
	{
		for (auto &entity : entityHolder.entities)
		{
			if (entity->dying) continue; // skip dying entities
			if (player.physics.transform.intersectTransform(entity->physics.transform))
			{
				float damage = entity->getContactDamage();
				if (damage <= 0.0f) { continue; }
				player.applyDamage(damage);
				playerDamageCooldown = 0.5f;
				glm::vec2 damagePos = player.physics.getPos();
				damagePos.y -= player.physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(damage, damagePos);
				break;
			}
		}
	}

	if (player.life <= 0.0f)
	{
		close();
		init();
		return true;
	}

	#pragma region temp enemy spawner
	// temporary: spawn enemies for testing
	if (!disableRandomEnemySpawns)
	{
		static float spawnTimer = 0.0f;
		spawnTimer -= simDelta;

		int maxEnemies = 30;
		if (spawnTimer <= 0.0f && entityHolder.entities.size() < maxEnemies && !floorInfo.rooms.empty())
		{
			spawnTimer = 1.8f;
			float avoidMargin = 3.0f;
			float minPlayerDistance = 10.0f;
			int maxRoomEnemies = 3;

			auto isInsideRoom = [&](const FloorRoom &room, glm::vec2 pos, float margin)
			{
				return pos.x >= room.pos.x - margin && pos.x <= room.pos.x + room.size.x + margin &&
					pos.y >= room.pos.y - margin && pos.y <= room.pos.y + room.size.y + margin;
			};

			for (int attempt = 0; attempt < 6; attempt++)
			{
				int roomIndex = getRandomInt(rng, 0, (int)floorInfo.rooms.size() - 1);
				const auto &room = floorInfo.rooms[roomIndex];
				if (room.enemySpawnPositions.empty()) { continue; }
				if (isInsideRoom(room, player.physics.getPos(), avoidMargin)) { continue; }
				if (glm::distance(glm::vec2(room.center()), player.physics.getPos()) < minPlayerDistance) { continue; }

				int roomEnemyCount = 0;
				for (auto &entity : entityHolder.entities)
				{
					if (isInsideRoom(room, entity->physics.getPos(), 0.0f))
					{
						roomEnemyCount++;
					}
				}
				if (roomEnemyCount >= maxRoomEnemies) { continue; }

				glm::vec2 spawnPos = {};
				bool placed = false;
				for (int spawnAttempt = 0; spawnAttempt < 6; spawnAttempt++)
				{
					int spawnIndex = getRandomInt(rng, 0, (int)room.enemySpawnPositions.size() - 1);
					spawnPos = room.enemySpawnPositions[spawnIndex];
					if (glm::distance(spawnPos, player.physics.getPos()) < minPlayerDistance)
					{
						continue;
					}
					bool occupied = false;
					for (auto &entity : entityHolder.entities)
					{
						if (glm::distance(entity->physics.getPos(), spawnPos) < 0.4f)
						{
							occupied = true;
							break;
						}
					}
					if (occupied)
					{
						continue;
					}
					placed = true;
					break;
				}

				if (!placed) { continue; }
				spawnRandomEnemy(entityHolder, rng, spawnPos, getFloorDifficulty(currentFloorIndex));
				break;
			}
		}
	}
	#pragma endregion
	//renderer.renderRectangle(player.physical.getAABB(), Colors_Red);
	player.update(simDelta);

	// Trap rooms lock doors until their enemies are cleared.
	auto isInsideRoom = [&](const FloorRoom &room, glm::vec2 pos, float margin)
	{
		float minX = room.pos.x + margin;
		float minY = room.pos.y + margin;
		float maxX = room.pos.x + room.size.x - margin;
		float maxY = room.pos.y + room.size.y - margin;
		if (maxX <= minX || maxY <= minY) { return false; }
		return pos.x >= minX && pos.x <= maxX &&
			pos.y >= minY && pos.y <= maxY;
	};

	auto isPlayerClearlyInsideRoom = [&](const FloorRoom &room)
	{
		return isInsideRoomTriggerBounds(room, player.physics.getAABB(), trapRoomTriggerMargin);
	};

	auto roomHasLivingEnemies = [&](const FloorRoom &room)
	{
		for (auto &entity : entityHolder.entities)
		{
			if (entity->dying) { continue; }
			if (isInsideRoom(room, entity->physics.getPos(), 0.0f))
			{
				return true;
			}
		}
		return false;
	};

	roomLightingSystem.update(simDelta, map, floorInfo, player.physics.getAABB(), trapRoomTriggerMargin);

	auto emitTrapSpawnParticles = [&](glm::vec2 pos, float duration)
	{
		glm::vec4 startColor = {1.0f, 0.7f, 0.25f, 0.9f};
		glm::vec4 endColor = {0.95f, 0.35f, 0.1f, 0.65f};
		ParticleSettings ring = getSmallSquareParticle(startColor, endColor);
		ring.onCreateCount = 12;
		ring.particleLifeTime = {duration * 0.85f, duration * 1.05f};
		ring.velocityX = {0.0f, 0.0f};
		ring.velocityY = {0.0f, 0.0f};
		ring.dragX = {0.0f, 0.0f};
		ring.dragY = {0.0f, 0.0f};
		ring.positionX = {0.0f, 0.0f};
		ring.positionY = {0.0f, 0.0f};
		ring.rotationSpeed = {0.0f, 0.0f};
		ring.animationType = ParticleSettings::ANIMATION_TYPES::animationCircle;
		ring.animationSpeed = {-7.5f, 7.5f};
		ring.animationAcceleration = {-2.5f, 2.5f};
		ring.animationScaleX = {PIXEL_SIZE * 4.5f, PIXEL_SIZE * 8.0f};
		ring.animationScaleY = {PIXEL_SIZE * 4.5f, PIXEL_SIZE * 8.0f};
		particleSystem.emitParticles(ring, pos, rng, pos);

		ParticleSettings flash = getSparkBurstParticle(startColor, endColor);
		flash.onCreateCount = 10;
		flash.particleLifeTime = {0.25f, 0.4f};
		flash.velocityX = glm::vec2{-30.0f, 30.0f} * PIXEL_SIZE;
		flash.velocityY = glm::vec2{-30.0f, 30.0f} * PIXEL_SIZE;
		flash.dragX = glm::vec2{-140.0f, -220.0f} * PIXEL_SIZE;
		flash.dragY = glm::vec2{-140.0f, -220.0f} * PIXEL_SIZE;
		flash.positionX = glm::vec2{-2.5f, 2.5f} * PIXEL_SIZE;
		flash.positionY = glm::vec2{-2.5f, 2.5f} * PIXEL_SIZE;
		particleSystem.emitParticles(flash, pos, rng, pos);
	};

	auto emitTrapRewardParticles = [&](glm::vec2 pos)
	{
		glm::vec4 startColor = {1.0f, 0.85f, 0.25f, 0.9f};
		glm::vec4 endColor = {0.85f, 0.55f, 0.15f, 0.65f};
		ParticleSettings burst = getSparkBurstParticle(startColor, endColor);
		burst.onCreateCount = 14;
		burst.particleLifeTime = {0.25f, 0.45f};
		burst.velocityX = glm::vec2{-32.0f, 32.0f} * PIXEL_SIZE;
		burst.velocityY = glm::vec2{-32.0f, 32.0f} * PIXEL_SIZE;
		burst.dragX = glm::vec2{-140.0f, -220.0f} * PIXEL_SIZE;
		burst.dragY = glm::vec2{-140.0f, -220.0f} * PIXEL_SIZE;
		particleSystem.emitParticles(burst, pos, rng, pos);
	};

	auto isItemBlocking = [&](glm::vec2 pos, float radius)
	{
		float radius2 = radius * radius;
		for (const auto &item : droppedItems.items)
		{
			glm::vec2 diff = item.pos - pos;
			if (glm::dot(diff, diff) <= radius2)
			{
				return true;
			}
		}
		return false;
	};

	auto buildTrapSpawnPositions = [&](const FloorRoom &room)
	{
		std::vector<glm::vec2> positions;
		positions.reserve(room.enemySpawnPositions.size());
		for (const auto &pos : room.enemySpawnPositions)
		{
			glm::ivec2 tile = WorldToTile(pos);
			if (tile.x < 0 || tile.y < 0 || tile.x >= map.size.x || tile.y >= map.size.y)
			{
				continue;
			}
			auto &over = map.secondLayer.getBlockUnsafe(tile.x, tile.y);
			if (over.type == Blocks::spikeTrap)
			{
				continue;
			}
			if (!isItemBlocking(pos, 0.5f))
			{
				positions.push_back(pos);
			}
		}
		return positions;
	};

	auto startTrapWave = [&](TrapRoomState &state)
	{
		state.pendingSpawns.clear();
		if (state.currentWaveIndex < 0
			|| state.currentWaveIndex >= (int)state.wavePlan.size())
		{
			return;
		}
		const auto &wave = state.wavePlan[state.currentWaveIndex];
		state.pendingSpawns.reserve(wave.size());
		int spawnIndex = 0;
		for (const auto &spawn : wave)
		{
			float delay = trapRoomSpawnDelaySeconds + spawnIndex * trapRoomSpawnStaggerSeconds;
			TrapRoomSpawn pending = {};
			pending.spawn = spawn;
			pending.timer = delay;
			state.pendingSpawns.push_back(pending);
			spawnIndex++;
		}
	};

	auto queueTrapWaves = [&](TrapRoomState &state, const FloorRoom &room)
	{
		auto spawnPositions = buildTrapSpawnPositions(room);
		int floorDifficulty = getFloorDifficulty(currentFloorIndex);
		int waveDifficulty = forceTrapDifficulty ? trapDifficulty : floorDifficulty;
		TrapWavePlan plan = buildTrapRoomWavePlan(room, waveDifficulty, rng, &spawnPositions);
		state.wavePlan = plan.waves;
		state.currentWaveIndex = state.wavePlan.empty() ? -1 : 0;
		startTrapWave(state);
	};

	auto isTrapSpawnOccupied = [&](glm::vec2 spawnPos)
	{
		if (isItemBlocking(spawnPos, 0.5f))
		{
			return true;
		}
		if (glm::distance(player.physics.getPos(), spawnPos) < 0.5f)
		{
			return true;
		}
		for (auto &summon : summons.summons)
		{
			if (summon->isDying()) { continue; }
			if (glm::distance(summon->physics.getPos(), spawnPos) < 0.4f)
			{
				return true;
			}
		}
		for (auto &entity : entityHolder.entities)
		{
			if (entity->dying) { continue; }
			if (glm::distance(entity->physics.getPos(), spawnPos) < 0.4f)
			{
				return true;
			}
		}
		return false;
	};

	auto pickTrapRewardSpot = [&](const FloorRoom &room, glm::vec2 &outPos)
	{
		std::vector<glm::vec2> positions = buildTrapSpawnPositions(room);
		for (int attempt = 0; attempt < 6 && !positions.empty(); attempt++)
		{
			int index = getRandomInt(rng, 0, (int)positions.size() - 1);
			glm::vec2 pos = positions[index];
			positions[index] = positions.back();
			positions.pop_back();
			bool blocked = false;
			for (const auto &item : droppedItems.items)
			{
				if (glm::distance(item.pos, pos) < 0.5f)
				{
					blocked = true;
					break;
				}
			}
			if (!blocked)
			{
				outPos = pos;
				return true;
			}
		}

		glm::vec2 centerPos = glm::vec2(room.center());
		for (const auto &item : droppedItems.items)
		{
			if (glm::distance(item.pos, centerPos) < 0.5f)
			{
				return false;
			}
		}
		outPos = centerPos;
		return true;
	};

	auto updateTrapSpawns = [&](TrapRoomState &state)
	{
		for (size_t spawnIndex = 0; spawnIndex < state.pendingSpawns.size(); )
		{
			auto &pending = state.pendingSpawns[spawnIndex];
			pending.timer -= simDelta;
			if (!pending.effectStarted && pending.timer <= trapRoomSpawnDelaySeconds)
			{
				pending.effectStarted = true;
				emitTrapSpawnParticles(pending.spawn.pos, trapRoomSpawnDelaySeconds);
			}
			if (pending.timer > 0.0f)
			{
				spawnIndex++;
				continue;
			}
			if (!isTrapSpawnOccupied(pending.spawn.pos))
			{
				spawnTrapWaveEnemy(entityHolder, pending.spawn.type,
					pending.spawn.pos, rng);
			}
			state.pendingSpawns[spawnIndex] = state.pendingSpawns.back();
			state.pendingSpawns.pop_back();
		}
	};

	auto closeTrapDoors = [&](TrapRoomState &state)
	{
		for (const auto &anchor : state.doorAnchors)
		{
			auto it = doorHolder.doors.find(anchor);
			if (it != doorHolder.doors.end())
			{
				it->second.open = false;
			}
		}
	};

	if (!trapRooms.empty() && trapRooms.size() == floorInfo.rooms.size())
	{
		for (size_t i = 0; i < trapRooms.size(); i++)
		{
			auto &state = trapRooms[i];
			if (!state.isTrap || state.cleared) { continue; }
			const auto &room = floorInfo.rooms[i];
			bool playerInside = isPlayerClearlyInsideRoom(room);
			if (!state.triggered && playerInside)
			{
				state.triggered = true;
				if (state.currentWaveIndex < 0)
				{
					queueTrapWaves(state, room);
				}
				for (auto &summon : summons.summons)
				{
					if (summon->isDying()) { continue; }
					if (!isInsideRoom(room, summon->physics.getPos(), 0.0f))
					{
						summon->physics.teleport(player.physics.getPos());
						summon->physics.velocity = {};
						summon->physics.acceleration = {};
					}
				}
				closeTrapDoors(state);
			}
			if (state.triggered && !state.pendingSpawns.empty())
			{
				updateTrapSpawns(state);
			}
			if (state.triggered && !state.cleared)
			{
				if (state.pendingSpawns.empty() && !roomHasLivingEnemies(room))
				{
					if (state.currentWaveIndex + 1 < (int)state.wavePlan.size())
					{
						state.currentWaveIndex++;
						startTrapWave(state);
					}
					else
					{
						state.cleared = true;
						if (!state.rewardGranted)
						{
							state.rewardGranted = true;
							if (getRandomChance(rng, 0.3f))
							{
								glm::vec2 rewardPos = {};
								if (pickTrapRewardSpot(room, rewardPos))
								{
									const float chestWandChance = 0.22f;
									if (getRandomChance(rng, chestWandChance))
									{
										int tier = getFloorWandTier(currentFloorIndex);
										Wand chestWand = getRandomWand(tier, rng);
										droppedItems.spawnChest(rewardPos, rng, &chestWand);
									}
									else
									{
										droppedItems.spawnChest(rewardPos, rng);
									}
									emitTrapRewardParticles(rewardPos);
								}
							}
						}
					}
				}
			}
		}
	}

	std::vector<glm::vec4> doorTriggerRects;
	doorTriggerRects.reserve(doorHolder.doors.size());

	auto isDoorLockedByTrap = [&](glm::ivec2 pos)
	{
		if (trapRooms.empty() || trapRooms.size() != floorInfo.rooms.size()) { return false; }
		for (size_t i = 0; i < trapRooms.size(); i++)
		{
			const auto &state = trapRooms[i];
			if (!state.isTrap || !state.triggered || state.cleared) { continue; }
			for (const auto &anchor : state.doorAnchors)
			{
				if (anchor.x == pos.x && anchor.y == pos.y)
				{
					return true;
				}
			}
		}
		return false;
	};

	// Door collisions and opening logic (horizontal doors only).
	auto updateDoors = [&]()
	{
		glm::vec4 playerRect = player.physics.getAABB();
		doorTriggerRects.clear();
		glm::vec2 moveIntent = {};
		if (!inventoryOpen && !mapViewerOpen)
		{
			if (platform::isButtonHeld(platform::Button::A)) { moveIntent.x -= 1.0f; }
			if (platform::isButtonHeld(platform::Button::D)) { moveIntent.x += 1.0f; }
			if (platform::isButtonHeld(platform::Button::W)) { moveIntent.y -= 1.0f; }
			if (platform::isButtonHeld(platform::Button::S)) { moveIntent.y += 1.0f; }
			moveIntent += platform::getControllerButtons().LStick * glm::vec2(1, -1);
			float moveLen = glm::length(moveIntent);
			if (moveLen > 0.0001f) { moveIntent /= moveLen; }
		}
		const float triggerThickness = 1.2f;
		const float triggerInset = 0.1f;
		const float moveThreshold = 0.2f;
		for (auto &doorPair : doorHolder.doors)
		{
			auto &door = doorPair.second;
			glm::ivec2 pos = doorPair.first;
			bool lockedByTrap = isDoorLockedByTrap(pos);
			if (lockedByTrap)
			{
				door.open = false;
			}
			if (door.orientation == Door::Orientation::Horizontal)
			{
				glm::vec4 triggerRect = {
					(float)pos.x + triggerInset,
					(float)pos.y + 0.5f - triggerThickness * 0.5f,
					2.0f - triggerInset * 2.0f,
					triggerThickness
				};
				doorTriggerRects.push_back(triggerRect);

				float doorCenterY = (float)pos.y + 0.5f;
				float playerCenterY = player.physics.getPos().y;
				bool movingToward = false;
				if (playerCenterY >= doorCenterY)
				{
					movingToward = moveIntent.y < -moveThreshold;
				}
				else
				{
					movingToward = moveIntent.y > moveThreshold;
				}

				if (!lockedByTrap && !door.open && movingToward && checkCollisionRecs(playerRect, triggerRect))
				{
					door.open = true;
				}

				bool solid = !door.open;
				for (int dx = 0; dx <= 1; dx++)
				{
					int x = pos.x + dx;
					int y = pos.y;
					if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y) { continue; }
					auto &tile = map.secondLayer.getBlockUnsafe(x, y);
					if (solid)
					{
						if (tile.type == Blocks::none || tile.type == Blocks::doorCollision)
						{
							tile.type = Blocks::doorCollision;
						}
					}
					else
					{
						if (tile.type == Blocks::doorCollision)
						{
							tile.type = Blocks::none;
						}
					}
				}
			}
			else if (door.orientation == Door::Orientation::Vertical)
			{
				glm::vec4 triggerRect = {
					(float)pos.x + 0.5f - triggerThickness * 0.5f,
					(float)pos.y - 1.0f + triggerInset,
					triggerThickness,
					2.0f - triggerInset * 2.0f
				};
				doorTriggerRects.push_back(triggerRect);

				float doorCenterX = (float)pos.x + 0.5f;
				float playerCenterX = player.physics.getPos().x;
				bool movingToward = false;
				if (playerCenterX >= doorCenterX)
				{
					movingToward = moveIntent.x < -moveThreshold;
				}
				else
				{
					movingToward = moveIntent.x > moveThreshold;
				}

				if (!lockedByTrap && !door.open && movingToward && checkCollisionRecs(playerRect, triggerRect))
				{
					door.open = true;
				}

				bool solid = !door.open;
				for (int dy = 0; dy <= 1; dy++)
				{
					int x = pos.x;
					int y = pos.y - dy;
					if (x < 0 || y < 0 || x >= map.size.x || y >= map.size.y) { continue; }
					auto &tile = map.secondLayer.getBlockUnsafe(x, y);
					if (solid)
					{
						if (tile.type == Blocks::none || tile.type == Blocks::doorCollision)
						{
							tile.type = Blocks::doorCollision;
						}
					}
					else
					{
						if (tile.type == Blocks::doorCollision)
						{
							tile.type = Blocks::none;
						}
					}
				}
			}
		}
	};

	updateDoors();

	struct FloorTransitionState
	{
		Player player = {};
		float playerDamageCooldown = 0.0f;
		Wand wands[2] = {};
		bool hasWand[2] = {};
		int activeWandIndex = 0;
		SpellRecepie spellRecepies[2] = {};
		WandStoneSlot wandStoneSlots[2][4] = {};
		std::vector<MagicStone> stoneInventory;
	};

	auto captureFloorState = [&]()
	{
		FloorTransitionState state = {};
		state.player = player;
		state.playerDamageCooldown = playerDamageCooldown;
		state.activeWandIndex = activeWandIndex;
		for (int i = 0; i < 2; i++)
		{
			state.wands[i] = wands[i];
			state.hasWand[i] = hasWand[i];
			state.spellRecepies[i] = spellRecepies[i];
			for (int slot = 0; slot < 4; slot++)
			{
				state.wandStoneSlots[i][slot] = wandStoneSlots[i][slot];
			}
		}
		state.stoneInventory = stoneInventory;
		return state;
	};

	auto restoreFloorState = [&](const FloorTransitionState &state)
	{
		player = state.player;
		playerDamageCooldown = state.playerDamageCooldown;
		activeWandIndex = state.activeWandIndex;
		for (int i = 0; i < 2; i++)
		{
			wands[i] = state.wands[i];
			hasWand[i] = state.hasWand[i];
			spellRecepies[i] = state.spellRecepies[i];
			for (int slot = 0; slot < 4; slot++)
			{
				wandStoneSlots[i][slot] = state.wandStoneSlots[i][slot];
			}
			spellSelectionLogic[i].resetSelectionForWand(wands[i], spellRecepies[i], false);
		}
		stoneInventory = state.stoneInventory;
		inventoryOpen = false;
		resetInventoryStoneInteraction();

		if (floorInfo.playerSpawnPos)
		{
			player.physics.teleport(*floorInfo.playerSpawnPos);
		}
		else
		{
			player.physics.teleport({35, 35});
		}
		player.physics.velocity = {};
		player.physics.acceleration = {};
	};

	auto tryAdvanceFloor = [&]()
	{
		if (!floorInfo.exitPos) { return false; }
		if (inventoryOpen || mapViewerOpen) { return false; }
		bool wantsExit = input.buttons[platform::Button::E].pressed ||
			input.controller.buttons[platform::Controller::A].pressed;
		if (!wantsExit) { return false; }
		float dist = glm::distance(player.physics.getPos(), *floorInfo.exitPos);
		if (dist > 1.1f) { return false; }
		if (isLastFloor(currentFloorIndex))
		{
			exitDungeon = true;
			return true;
		}
		FloorTransitionState state = captureFloorState();
		currentFloorIndex++;
		keepFloorOnClose = true;
		close();
		init();
		restoreFloorState(state);
		return true;
	};

	if (tryAdvanceFloor())
	{
		return !exitDungeon;
	}

	// Ground projectiles (like thorns) render under entities.
	projectiles.render(renderer, assetsManager, particlePostProcessRenderer);

	// Render entities/player/doors sorted by Y for proper overlap.
	struct RenderEntry
	{
		enum class Kind
		{
			Entity,
			Summon,
			Player,
			Door,
			BreakableDecoration
		};
		float sortY = 0.0f;
		Kind kind = Kind::Entity;
		Entity *entity = nullptr;
		SummonEntity *summon = nullptr;
		const Door *door = nullptr;
		glm::ivec2 doorPos = {};
		glm::ivec2 decorationPos = {};
	};

	std::vector<RenderEntry> renderEntries;
	renderEntries.reserve(entityHolder.entities.size() + summons.summons.size()
		+ doorHolder.doors.size() + breakableDecorations.positions.size() + 2);

	for (auto &entity : entityHolder.entities)
	{
		RenderEntry entry = {};
		entry.kind = RenderEntry::Kind::Entity;
		entry.sortY = entity->physics.transform.getBottom().y;
		entry.entity = entity.get();
		renderEntries.push_back(entry);
	}

	for (auto &summon : summons.summons)
	{
		RenderEntry entry = {};
		entry.kind = RenderEntry::Kind::Summon;
		entry.sortY = summon->physics.transform.getBottom().y;
		entry.summon = summon.get();
		renderEntries.push_back(entry);
	}

	RenderEntry playerEntry = {};
	playerEntry.kind = RenderEntry::Kind::Player;
	playerEntry.sortY = player.physics.transform.getBottom().y;
	renderEntries.push_back(playerEntry);

	auto doorViewRect = renderer.getViewRect();
	glm::ivec4 viewRectInt = {};
	viewRectInt.x = int(doorViewRect.x) - 2;
	viewRectInt.y = int(doorViewRect.y) - 2;
	viewRectInt.z = int(doorViewRect.z + 2.5f) + 2;
	viewRectInt.w = int(doorViewRect.w + 2.5f) + 2;
	viewRectInt.z += doorViewRect.x;
	viewRectInt.w += doorViewRect.y;
	viewRectInt = glm::clamp(viewRectInt, {0, 0, 0, 0},
		{map.size.x - 1, map.size.y - 1, map.size.x - 1, map.size.y - 1});

	auto isInViewRect = [&](glm::ivec2 pos)
	{
		return pos.x >= viewRectInt.x && pos.x < viewRectInt.z
			&& pos.y >= viewRectInt.y && pos.y < viewRectInt.w;
	};

	auto addBreakableDecorationEntries = [&]()
	{
		if (breakableDecorations.positions.empty()) { return; }
		for (const auto &pos : breakableDecorations.positions)
		{
			if (!isInViewRect(pos)) { continue; }
			RenderEntry entry = {};
			entry.kind = RenderEntry::Kind::BreakableDecoration;
			entry.sortY = (float)pos.y + 1.0f;
			entry.decorationPos = pos;
			renderEntries.push_back(entry);
		}
	};

	auto addDoorRenderEntries = [&]()
	{
		if (doorHolder.doors.empty()) { return; }
		for (const auto &doorPair : doorHolder.doors)
		{
			const glm::ivec2 pos = doorPair.first;
			const Door &door = doorPair.second;
			if (door.orientation != Door::Orientation::Horizontal) { continue; }
			if (!isInViewRect(pos)) { continue; }

			RenderEntry entry = {};
			entry.kind = RenderEntry::Kind::Door;
			entry.sortY = (float)pos.y + 1.0f;
			entry.door = &door;
			entry.doorPos = pos;
			renderEntries.push_back(entry);
		}
	};

	addBreakableDecorationEntries();
	addDoorRenderEntries();

	std::sort(renderEntries.begin(), renderEntries.end(),
		[](const RenderEntry &a, const RenderEntry &b)
		{
			return a.sortY < b.sortY;
		});

	auto renderHorizontalDoor = [&](glm::ivec2 pos, const Door &door)
	{
		glm::vec4 rect = {
			(float)pos.x,
			(float)pos.y - 1.0f,
			2.0f,
			2.0f
		};
		gl2d::Texture &sprite = door.open
			? assetsManager.doorOpenedHorizontal
			: assetsManager.doorClosedHorizontal;
		if (!sprite.isValid()) { return; }
		renderer.renderRectangle(rect, sprite, Colors_White);
		if (renderColliders())
		{
			renderer.renderRectangleOutline(rect, Colors_Green, 0.03f);
		}
	};

	auto hashPosition = [](int x, int y)
	{
		unsigned int h = 2166136261u;
		h = (h ^ (unsigned int)x) * 16777619u;
		h = (h ^ (unsigned int)y) * 16777619u;
		return h;
	};

	auto pickAtlasOffset = [](unsigned int h, int maxOffset, unsigned int salt)
	{
		if (maxOffset <= 0) { return 0; }
		h ^= salt + 0x9e3779b9u + (h << 6) + (h >> 2);
		return int(h % (unsigned int)(maxOffset + 1));
	};

	auto renderBreakableDecoration = [&](glm::ivec2 pos)
	{
		auto &decorations = assetsManager.tileSets[TileSets::woodenDecorations];
		if (!decorations.texture.isValid()) { return; }
		glm::vec4 rect = {
			(float)pos.x,
			(float)pos.y,
			1.0f,
			1.0f
		};
		glm::ivec2 randomOffsets = getRandomAtlasOffsets(Blocks::woodenDecorations);
		unsigned int h = hashPosition(pos.x, pos.y);
		int offsetX = pickAtlasOffset(h, randomOffsets.x, 0x68bc21ebu);
		renderer.renderRectangle(rect, decorations.texture, Colors_White, {}, 0,
			decorations.atlas.get(offsetX, 0));
	};

	for (auto &entry : renderEntries)
	{
		switch (entry.kind)
		{
			case RenderEntry::Kind::Entity:
				entry.entity->render(renderer, particlePostProcessRenderer);
				break;
			case RenderEntry::Kind::Summon:
				entry.summon->render(renderer, particlePostProcessRenderer);
				break;
			case RenderEntry::Kind::Player:
				player.render(renderer, assetsManager, currentWand, fireDirection);
				break;
			case RenderEntry::Kind::Door:
				if (entry.door)
				{
					renderHorizontalDoor(entry.doorPos, *entry.door);
				}
				break;
			case RenderEntry::Kind::BreakableDecoration:
				renderBreakableDecoration(entry.decorationPos);
				break;
		}
	}

	if (renderColliders())
	{
		for (auto &rect : doorTriggerRects)
		{
			renderer.renderRectangleOutline(rect, Colors_Blue, 0.02f);
		}
	}

	auto renderStatusIcons = [&](glm::vec4 aabb, const StatusEffects &effects)
	{
		struct StatusIcon
		{
			int element = 0;
			float alpha = 1.0f;
		};

		StatusIcon icons[3];
		int count = 0;

		auto pushIcon = [&](float amount, int element)
		{
			if (amount <= 0.0f) { return; }
			float alpha = 0.5f + 0.5f * std::min(amount / 2.0f, 1.0f);
			icons[count++] = {element, alpha};
		};

		pushIcon(effects.fire, Elements::Fire);
		pushIcon(effects.poison, Elements::Earth);
		pushIcon(effects.chill, Elements::Ice);

		if (count == 0) { return; }

		float iconSize = PIXEL_SIZE * 8.0f;
		float spacing = iconSize * 1.2f;
		glm::vec2 base = {aabb.x + aabb.z * 0.5f, aabb.y - PIXEL_SIZE * 2.0f};

		for (int i = 0; i < count; i++)
		{
			float offsetX = (i - (count - 1) * 0.5f) * spacing;
			glm::vec4 rect = {
				base.x + offsetX - iconSize * 0.5f,
				base.y - iconSize,
				iconSize,
				iconSize
			};
			gl2d::Color4f color = {1, 1, 1, icons[i].alpha};
			renderer.renderRectangle(rect, assetsManager.elements.texture, color, {}, 0,
				assetsManager.elements.atlas.get(icons[i].element, 0));
		}
	};

	for (auto &entity : entityHolder.entities)
	{
		renderStatusIcons(entity->physics.getAABB(), entity->statusEffects);
	}

	renderStatusIcons(player.physics.getAABB(), player.statusEffects);

	standbyProjectiles.render(renderer, particlePostProcessRenderer);
	projectiles.renderAfterEntities(renderer, assetsManager, particlePostProcessRenderer);

	if (paletteParticles)
	{
		if (paletteEffect.applyToTexture(renderer, particlePostProcessRenderer.fbo.texture,
			paletteEffect.particlesTexture, paletteEffect.particlesSize,
			{particlePostProcessRenderer.fbo.w, particlePostProcessRenderer.fbo.h}))
		{
			renderer.pushCamera();
			renderer.renderRectangle({0,0, renderer.windowW, renderer.windowH},
				paletteEffect.particlesTexture, {1,1,1,2}, {}, {}, {0,0,1,1});
			renderer.popCamera();
		}
		else
		{
			particlePostProcessRenderer.finalRender(renderer);
		}
	}
	else
	{
		particlePostProcessRenderer.finalRender(renderer);
	}

	static WorldTextSystem mapTextSystem;
	map.renderMapAfterEntities(renderer, assetsManager, &doorHolder, &mapTextSystem, usesController);

	if (floorInfo.exitPos)
	{
		const ButtonPrompt exitPrompt = {"E", "A"};
		const float promptRadius = 1.6f;
		float dist = glm::distance(player.physics.getPos(), *floorInfo.exitPos);
		if (dist <= promptRadius)
		{
			float hover = std::sin(wandHoverTimer * 1.1f) * (PIXEL_SIZE * 1.4f);
			float promptSize = PIXEL_SIZE * 12.0f;
			float promptOffset = PIXEL_SIZE * 18.0f;
			glm::vec2 promptPos = {floorInfo.exitPos->x, floorInfo.exitPos->y - promptOffset - hover};
			renderPrompt(renderer, assetsManager, usesController, exitPrompt, promptPos, promptSize, 0.85f);
		}
	}
	damageViewerSystem.render(renderer, assetsManager.font);
	if (!removeLightSystem)
	{
		roomLightingSystem.renderOverlay(renderer, map);
	}

#pragma endregion

	if (hdrGameTonemap)
	{
		gameHdrPostProcess.endScene(renderer);
	}

	// player life + spell healing + shield
	{
		const float uiBaseZoom = 100.0f;
		renderer.pushCamera();
		float cameraZoom = uiBaseZoom;
		float padding = PIXEL_SIZE * 3.0f * cameraZoom;
		float barWidth = PIXEL_SIZE * 48.0f * cameraZoom;
		float barHeight = PIXEL_SIZE * 6.0f * cameraZoom;
		float x = renderer.windowW - padding - barWidth;
		float y = padding;
		float outlineWidth = PIXEL_SIZE * cameraZoom;
		if (outlineWidth < 1.0f) { outlineWidth = 1.0f; }

		glm::vec4 barRect = {x, y, barWidth, barHeight};
		renderer.renderRectangle(barRect, {0.15f, 0.05f, 0.05f, 0.85f});
		float lifeDisplay = std::max(0.0f, player.life);
		float spellDisplay = std::max(0.0f, player.spellHealing);
		float shieldDisplay = std::max(0.0f, player.shield);
		float unitWidth = player.maxLife > 0.0f ? (barWidth / player.maxLife) : 0.0f;
		float lifeWidth = lifeDisplay * unitWidth;
		float spellWidth = spellDisplay * unitWidth;
		float shieldWidth = shieldDisplay * unitWidth;
		float totalWidth = lifeWidth + spellWidth + shieldWidth;
		float barRight = x + barWidth;
		float fillStart = barRight - totalWidth;
		float cursor = fillStart;
		// Shield -> spell healing -> life (rightmost) so damage peels from the left.
		if (shieldWidth > 0.0f)
		{
			glm::vec4 shieldRect = {cursor, y, shieldWidth, barHeight};
			renderer.renderRectangle(shieldRect, {0.65f, 0.65f, 0.7f, 0.9f});
			renderer.renderRectangleOutline(shieldRect, {0.35f, 0.35f, 0.38f, 0.95f}, outlineWidth);
			cursor += shieldWidth;
		}
		if (spellWidth > 0.0f)
		{
			renderer.renderRectangle({cursor, y, spellWidth, barHeight},
				{0.72f, 0.1f, 0.24f, 0.9f});
			cursor += spellWidth;
		}
		if (lifeWidth > 0.0f)
		{
			renderer.renderRectangle({cursor, y, lifeWidth, barHeight},
				{0.9f, 0.1f, 0.1f, 0.9f});
		}
		renderer.renderRectangleOutline(barRect, {0.4f, 0.1f, 0.1f, 0.9f}, outlineWidth);

		//char lifeText[32] = {};
		//snprintf(lifeText, sizeof(lifeText), "HP %d/%d", (int)std::ceil(lifeDisplay), (int)player.maxLife);
		//float textSize = PIXEL_SIZE * 6.0f * cameraZoom;
		//glm::vec2 textPos = {x, y - textSize * 0.2f};
		//renderer.renderText(textPos, lifeText, assetsManager.font,
		//	{1, 1, 1, 0.95f}, textSize, 4, 3, false);

		renderer.popCamera();
	}

	if (paletteGame)
	{
		gameFbo.unbind();
		renderer.pushCamera();
		if (paletteEffect.applyToTexture(renderer, gameFbo.texture, paletteEffect.gameTexture,
			paletteEffect.gameSize, {gameFbo.w, gameFbo.h}))
		{
			renderer.renderRectangle({0,0, renderer.windowW, renderer.windowH},
				paletteEffect.gameTexture, {1,1,1,1}, {}, {}, {0,0,1,1});
		}
		else
		{
			renderer.renderRectangle({0,0, renderer.windowW, renderer.windowH},
				gameFbo.texture, {1,1,1,1}, {}, {}, {0,0,1,1});
		}
		renderer.popCamera();
	}

	if (mapViewerOpen)
	{
		// Fullscreen map viewer: darken gameplay first, then draw the map on top.
		renderer.pushCamera();
		renderer.renderRectangle({0, 0, (float)renderer.windowW, (float)renderer.windowH},
			{0, 0, 0, 0.8f});
		renderer.popCamera();

		minimapSystem.renderFullscreenDirect(renderer, map, doorHolder, player.physics.getPos(), &floorInfo,
			mapLighting, &mapViewerCenter, &mapViewerViewSize, 0.78f);
	}
	else
	{
		minimapSystem.render(renderer);
	}

	if (!inventoryOpen && !mapViewerOpen)
	{
		// wand slots ui
		{
			const float uiBaseZoom = 100.0f;
			renderer.pushCamera();
			float cameraZoom = uiBaseZoom;
			float padding = PIXEL_SIZE * 3.0f * cameraZoom;
			float boxSize = PIXEL_SIZE * 16.0f * cameraZoom;
			float gap = PIXEL_SIZE * 3.0f * cameraZoom;
			float outlineWidth = std::max(PIXEL_SIZE * 0.8f * cameraZoom, 1.0f);
			float baseX = padding;
			float baseY = padding;

			for (int i = 0; i < 2; i++)
			{
				glm::vec4 boxRect = {baseX + i * (boxSize + gap), baseY, boxSize, boxSize};
				gl2d::Color4f boxColor = {0.08f, 0.08f, 0.1f, 0.7f};
				gl2d::Color4f outlineColor = {0.3f, 0.3f, 0.35f, 0.7f};
				if (i == activeWandIndex)
				{
					boxColor = {0.16f, 0.12f, 0.08f, 0.85f};
					outlineColor = {0.9f, 0.85f, 0.6f, 0.9f};
				}
				renderer.renderRectangle(boxRect, boxColor);
				renderer.renderRectangleOutline(boxRect, outlineColor, outlineWidth);

				if (hasWand[i])
				{
					float maxW = boxSize * 0.82f;
					float maxH = boxSize * 0.82f;
					glm::vec4 iconRect = {boxRect.x + (boxSize - maxW) * 0.5f,
						boxRect.y + (boxSize - maxH) * 0.5f, maxW, maxH};
					gl2d::Color4f tint = i == activeWandIndex
						? gl2d::Color4f{1, 1, 1, 1}
						: gl2d::Color4f{0.7f, 0.7f, 0.7f, 0.85f};
					renderer.renderRectangle(iconRect, assetsManager.wands.texture, tint, {}, 0,
						assetsManager.wands.atlas.get(wands[i].wandSprite, 0));
				}
			}

			renderer.popCamera();
		}

		if (!freeCameraMode)
		{
			// magic ui
			{
				spellSelectionLogic[activeWandIndex].update(simDelta, renderer, assetsManager,
					spellRecepies[activeWandIndex], spellsHolder, map, projectiles, entityHolder,
					player, fireDirection, usesController, currentWand, input);
				if (spellSelectionLogic[activeWandIndex].noManaFeedback)
				{
					player.wandFailTimer = 0.2f;
					spellSelectionLogic[activeWandIndex].noManaFeedback = false;
				}
			}
		}
	}

	if (inventoryOpen)
	{
		// inventory overlay
		const float uiBaseZoom = 100.0f;
		renderer.pushCamera();
		float cameraZoom = uiBaseZoom;
		float uiScale = std::min(renderer.windowW / 1280.0f, renderer.windowH / 720.0f);
		float uiZoom = cameraZoom * uiScale;
		renderer.renderRectangle({0, 0, (float)renderer.windowW, (float)renderer.windowH},
			{0.02f, 0.02f, 0.03f, 0.75f});
		// inventory book background
		float bookScale = 0.85f;
		float bookW = renderer.windowW * bookScale;
		float bookH = renderer.windowH * bookScale;
		glm::vec2 bookPos = {(renderer.windowW - bookW) * 0.5f, (renderer.windowH - bookH) * 0.5f};
		bookPos.y += renderer.windowH * 0.03f;
		glm::vec4 bookRect = {bookPos.x, bookPos.y, bookW, bookH};

		auto isInsideRect = [&](glm::vec4 rect, glm::vec2 pos)
		{
			return pos.x >= rect.x && pos.x <= rect.x + rect.z &&
				pos.y >= rect.y && pos.y <= rect.y + rect.w;
		};

		// bookmarks behind the book
		// tab buttons disabled for now
		/*
		{
			float tabW = bookW * 0.18f;
			float tabH = bookH * 0.09f;
			float tabGap = bookW * 0.02f;
			float tabX = bookPos.x + bookW * 0.1f;
			float tabY = bookPos.y - tabH * 0.9f;
			const char *tabNames[] = {"Wands", "Spells"};
			for (int i = 0; i < 2; i++)
			{
				glm::vec4 tabRect = {tabX + i * (tabW + tabGap), tabY, tabW, tabH};
				glm::vec4 tabColor = {0.18f, 0.16f, 0.12f, 0.85f};
				if (inventoryPage == i)
				{
					tabColor = {0.32f, 0.28f, 0.2f, 0.95f};
				}
				renderer.renderRectangle(tabRect, tabColor);
				float tabTextSize = tabH * 0.45f;
				glm::vec2 tabTextPos = {tabRect.x + tabRect.z * 0.5f, tabRect.y + tabRect.w * 0.2f};
				renderer.renderText(tabTextPos, tabNames[i], assetsManager.font,
					{0.9f, 0.9f, 0.9f, 0.9f}, tabTextSize, 4, 3, true);
				if (input.lMouse.pressed && isInsideRect(tabRect, cursorPos))
				{
					inventoryPage = i;
					quickActionEditIndex = -1;
					draggingStoneIndex = -1;
					draggingStone = false;
				}
			}
		}
		*/

		renderer.renderRectangle({bookPos.x, bookPos.y, bookW, bookH},
			assetsManager.book, {1, 1, 1, 1});

		spellbookPage.update(deltaTime, rng);
		if (inventoryPage == 1)
		{
			spellbookPage.render(renderer, assetsManager, rng, bookRect, cursorPos, input.lMouse.pressed);
		}
		else
		{
			glm::vec2 shadowOffset = {PIXEL_SIZE * 2.0f * uiZoom, PIXEL_SIZE * 2.0f * uiZoom};
			glm::vec2 wandShadowOffset = {-shadowOffset.x, shadowOffset.y * 0.9f};
			float shadowAlpha = 0.45f;

		float largeWandMaxW = renderer.windowW * 0.38f;
		float largeWandMaxH = renderer.windowH * 0.62f;
		float smallWandMaxW = largeWandMaxW * 0.8f;
		float smallWandMaxH = largeWandMaxH * 0.8f;
		float wandRowY = bookPos.y + bookH * 0.48f;
		glm::vec2 wandCenters[2] = {
			{bookPos.x + bookW * 0.22f, wandRowY},
			{bookPos.x + bookW * 0.38f, wandRowY}
		};
		int clickedSlot = -1;

		auto renderBookWand = [&](int slotIndex, glm::vec2 center, bool selected)
		{
			if (!hasWand[slotIndex]) { return glm::vec4{}; }
			gl2d::Texture &wandTexture = assetsManager.getWandIcon(wands[slotIndex].wandSprite);
			if (!wandTexture.isValid()) { return glm::vec4{}; }

			auto wandSize = wandTexture.GetSize();
			float maxW = selected ? largeWandMaxW : smallWandMaxW;
			float maxH = selected ? largeWandMaxH : smallWandMaxH;
			float scaleX = maxW / (float)wandSize.x;
			float scaleY = maxH / (float)wandSize.y;
			float scale = std::min(scaleX, scaleY);
			float drawW = wandSize.x * scale;
			float drawH = wandSize.y * scale;
			glm::vec2 drawCenter = center;
			float rotation = 0.0f;
			if (selected)
			{
				float hover = std::sin(wandHoverTimer * 0.8f) * (PIXEL_SIZE * 1.4f * uiZoom);
				float sway = std::sin(wandHoverTimer * 0.5f) * (PIXEL_SIZE * 0.8f * uiZoom);
				drawCenter.y -= hover;
				drawCenter.x += sway;
				rotation = std::sin(wandHoverTimer * 0.7f) * 2.0f;
			}
			glm::vec4 wandRect = {drawCenter.x - drawW * 0.5f, drawCenter.y - drawH * 0.5f, drawW, drawH};
			glm::vec4 wandShadowRect = {wandRect.x + wandShadowOffset.x, wandRect.y + wandShadowOffset.y,
				wandRect.z, wandRect.w};
			gl2d::Color4f tint = selected ? gl2d::Color4f{1, 1, 1, 1}
				: gl2d::Color4f{0.35f, 0.35f, 0.35f, 0.85f};
			glm::vec2 origin = {drawW * 0.5f, drawH * 0.5f};
			renderer.renderRectangle(wandShadowRect, wandTexture, {0, 0, 0, shadowAlpha}, origin, rotation);
			renderer.renderRectangle(wandRect, wandTexture, tint, origin, rotation);
			return wandRect;
		};

		for (int i = 0; i < 2; i++)
		{
			bool selected = i == activeWandIndex;
			glm::vec4 wandRect = renderBookWand(i, wandCenters[i], selected);
			if (hasWand[i] && input.lMouse.pressed && isInsideRect(wandRect, cursorPos))
			{
				clickedSlot = i;
			}
		}
		{
			glm::vec2 namePos = {(wandCenters[0].x + wandCenters[1].x) * 0.5f,
				wandRowY - largeWandMaxH * 0.55f};
			float nameSize = PIXEL_SIZE * 8.0f * uiZoom;
			const char *wandName = getWandSpriteName(wands[activeWandIndex].wandSprite);
			renderer.renderText(namePos, wandName, assetsManager.font,
				{0.95f, 0.95f, 0.95f, 0.95f}, nameSize, 4, 3, true);
		}
		if (clickedSlot >= 0)
		{
			switchActiveWand(clickedSlot, true);
		}

		glm::vec2 ringCenter = {renderer.windowW * 0.62f, renderer.windowH * 0.36f};
		float ringSize = PIXEL_SIZE * 44.0f * uiZoom * (2.0f / 2.6f);
		float ringOffset = ringSize * 0.4f;
		float iconSize = ringSize * 0.34f;
		float textSize = iconSize * 0.55f;
		float textOffset = iconSize * 0.6f;
		glm::vec2 slotDirs[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
		glm::vec4 ringSlotRects[4] = {};
		for (int i = 0; i < 4; i++)
		{
			glm::vec2 center = ringCenter + slotDirs[i] * ringOffset;
			ringSlotRects[i] = {center.x - iconSize * 0.5f, center.y - iconSize * 0.5f,
				iconSize, iconSize};
		}
		bool ringSlotRectsReady = true;
		glm::vec2 quickRingCenter = ringCenter + glm::vec2(0.0f, ringSize * 1.25f);
		glm::vec4 quickRingSlotRects[4] = {};
		for (int i = 0; i < 4; i++)
		{
			glm::vec2 center = quickRingCenter + slotDirs[i] * ringOffset;
			quickRingSlotRects[i] = {center.x - iconSize * 0.5f, center.y - iconSize * 0.5f,
				iconSize, iconSize};
		}

		auto getRingIndex = [&](glm::vec2 center)
		{
			glm::vec2 diff = cursorPos - center;
			float dist = glm::length(diff);
			float radius = ringSize * 0.5f;
			float innerRadius = ringSize * 0.2f;
			if (dist < innerRadius || dist > radius * 1.05f) { return -1; }
			if (std::abs(diff.x) > std::abs(diff.y))
			{
				return diff.x > 0 ? 3 : 2;
			}
			return diff.y > 0 ? 1 : 0;
		};

		int ringHoverSlot = getRingIndex(ringCenter);
		int quickHoverSlot = -1;
		for (int i = 0; i < 4; i++)
		{
			if (isInsideRect(quickRingSlotRects[i], cursorPos))
			{
				quickHoverSlot = i;
				break;
			}
		}

		float stoneSize = PIXEL_SIZE * 10.0f * uiZoom;
		float stoneSpacing = stoneSize * 1.25f;
		glm::vec2 stoneBase = {ringCenter.x + ringSize * 0.78f, ringCenter.y - ringSize * 0.55f};

		auto renderStone = [&](glm::vec4 rect, const MagicStone &stone, float alpha)
		{
			float inset = rect.z * 0.12f;
			glm::vec4 bgRect = {rect.x + inset, rect.y + inset, rect.z - inset * 2.0f, rect.w - inset * 2.0f};
			renderer.renderRectangle(bgRect, {0.2f, 0.2f, 0.2f, 0.85f * alpha});
			float iconSize = rect.z * 0.75f;
			glm::vec4 iconRect = {rect.x + (rect.z - iconSize) * 0.5f,
				rect.y + (rect.w - iconSize) * 0.5f, iconSize, iconSize};
			renderer.renderRectangle(iconRect, assetsManager.elements.texture,
				{1, 1, 1, alpha}, {}, 0,
				assetsManager.elements.atlas.get(stone.element, 0));

			char usesText[8] = {};
			snprintf(usesText, sizeof(usesText), "%d", stone.uses);
			glm::vec2 textPos = {rect.x + rect.z * 0.62f, rect.y + rect.w * 0.58f};
			float textSize = rect.z * 0.42f;
			renderer.renderText(textPos, usesText, assetsManager.font,
				{0.9f, 0.9f, 0.9f, alpha}, textSize, 4, 3, false);
		};

		auto scaledRect = [](glm::vec4 rect, float scale)
		{
			glm::vec2 center = {rect.x + rect.z * 0.5f, rect.y + rect.w * 0.5f};
			glm::vec2 size = {rect.z * scale, rect.w * scale};
			return glm::vec4{center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y};
		};

		Wand &inventoryWand = wands[activeWandIndex];

		if (draggingStone && (draggingStoneIndex < 0 || draggingStoneIndex >= (int)stoneInventory.size()))
		{
			draggingStoneIndex = -1;
			draggingStoneOffset = {};
			draggingStone = false;
		}

		if (!usesController)
		{
			controllerInventoryStickLockX = false;
			controllerInventoryStickLockY = false;
		}

		if (controllerInventoryWandSlot < -1 || controllerInventoryWandSlot > 3)
		{
			controllerInventoryWandSlot = 0;
		}

		auto isWandSlotRemovable = [&](int slotIndex)
		{
			if (slotIndex < 0 || slotIndex > 3) { return false; }
			return wandStoneSlots[activeWandIndex][slotIndex].hasStone;
		};

		auto isWandSlotPlaceable = [&](int slotIndex)
		{
			if (slotIndex < 0 || slotIndex > 3) { return false; }
			auto *slot = getWandSlot(inventoryWand, slotIndex);
			if (!slot) { return false; }
			if (wandStoneSlots[activeWandIndex][slotIndex].hasStone)
			{
				return true;
			}
			return slot->type == WandSlotType::Empty;
		};

		auto isWandSlotSelectable = [&](int slotIndex)
		{
			if (controllerInventoryHasSelectedStone)
			{
				return isWandSlotPlaceable(slotIndex);
			}
			return isWandSlotRemovable(slotIndex);
		};

		auto findFirstSelectableWandSlot = [&]()
		{
			for (int i = 0; i < 4; i++)
			{
				if (isWandSlotSelectable(i)) { return i; }
			}
			return -1;
		};

		if (controllerInventoryHasSelectedStone)
		{
			if (controllerInventorySelectedStoneIndex < 0
				|| controllerInventorySelectedStoneIndex >= (int)stoneInventory.size())
			{
				controllerInventoryHasSelectedStone = false;
				controllerInventorySelectedStoneIndex = -1;
			}
		}

		if (stoneInventory.empty())
		{
			controllerInventoryStoneIndex = 0;
			if (!controllerInventoryHasSelectedStone && controllerInventoryFocus == 1)
			{
				controllerInventoryFocus = 0;
			}
		}
		else
		{
			controllerInventoryStoneIndex = glm::clamp(controllerInventoryStoneIndex,
				0, (int)stoneInventory.size() - 1);
		}

		int firstSelectableWandSlot = findFirstSelectableWandSlot();
		if (firstSelectableWandSlot < 0)
		{
			controllerInventoryWandSlot = -1;
		}
		else if (!isWandSlotSelectable(controllerInventoryWandSlot))
		{
			controllerInventoryWandSlot = firstSelectableWandSlot;
		}

		// Controller-only wand/stone navigation and placement flow.
		bool controllerInventoryActive = usesController && quickActionEditIndex == -1 && !draggingStone;
		if (usesController && input.controller.buttons[platform::Controller::B].pressed)
		{
			controllerInventoryHasSelectedStone = false;
			controllerInventorySelectedStoneIndex = -1;
			if (quickActionEditIndex == -1)
			{
				controllerInventoryFocus = 0;
			}
		}
		if (controllerInventoryActive)
		{
			int navX = 0;
			int navY = 0;
			const float stickDeadzone = 0.62f;
			float stickX = input.controller.LStick.x;
			float stickY = input.controller.LStick.y;

			if (std::abs(stickX) <= stickDeadzone)
			{
				controllerInventoryStickLockX = false;
			}
			else if (!controllerInventoryStickLockX)
			{
				navX = stickX > 0.0f ? 1 : -1;
				controllerInventoryStickLockX = true;
			}

			if (std::abs(stickY) <= stickDeadzone)
			{
				controllerInventoryStickLockY = false;
			}
			else if (!controllerInventoryStickLockY)
			{
				navY = stickY > 0.0f ? -1 : 1;
				controllerInventoryStickLockY = true;
			}

			if (controllerInventoryFocus == 0)
			{
				if (controllerInventoryWandSlot < 0)
				{
					controllerInventoryWandSlot = firstSelectableWandSlot;
				}
				if (navY < 0)
				{
					if (isWandSlotSelectable(0)) { controllerInventoryWandSlot = 0; }
				}
				if (navY > 0)
				{
					if (isWandSlotSelectable(1)) { controllerInventoryWandSlot = 1; }
				}
				if (navX < 0)
				{
					if (isWandSlotSelectable(2)) { controllerInventoryWandSlot = 2; }
				}
				if (navX > 0)
				{
					if (!controllerInventoryHasSelectedStone && !stoneInventory.empty())
					{
						if (controllerInventoryWandSlot == 3 || !isWandSlotSelectable(3))
						{
							controllerInventoryFocus = 1;
						}
						else
						{
							controllerInventoryWandSlot = 3;
						}
					}
					else if (isWandSlotSelectable(3))
					{
						controllerInventoryWandSlot = 3;
					}
				}
			}
			else
			{
				if (stoneInventory.empty())
				{
					controllerInventoryFocus = 0;
				}
				else
				{
					if (navY != 0)
					{
						controllerInventoryStoneIndex = glm::clamp(
							controllerInventoryStoneIndex + navY,
							0, (int)stoneInventory.size() - 1);
					}
					if (navX < 0)
					{
						if (firstSelectableWandSlot >= 0)
						{
							controllerInventoryFocus = 0;
							if (!isWandSlotSelectable(controllerInventoryWandSlot))
							{
								controllerInventoryWandSlot = firstSelectableWandSlot;
							}
						}
					}
				}
			}

			if (input.controller.buttons[platform::Controller::A].pressed)
			{
				if (controllerInventoryFocus == 1 && !controllerInventoryHasSelectedStone)
				{
					if (!stoneInventory.empty())
					{
						controllerInventoryHasSelectedStone = true;
						controllerInventorySelectedStoneIndex = controllerInventoryStoneIndex;
						controllerInventoryFocus = 0;
						controllerInventoryWandSlot = firstSelectableWandSlot;
					}
				}
				else if (controllerInventoryFocus == 0)
				{
					if (controllerInventoryHasSelectedStone)
					{
						int selectedIndex = controllerInventorySelectedStoneIndex;
						if (selectedIndex >= 0 && selectedIndex < (int)stoneInventory.size())
						{
							MagicStone stone = stoneInventory[selectedIndex];
							if (isWandSlotPlaceable(controllerInventoryWandSlot)
								&& applyStoneToSlot(activeWandIndex, controllerInventoryWandSlot, stone))
							{
								stoneInventory.erase(stoneInventory.begin() + selectedIndex);
								spellSelectionLogic[activeWandIndex].resetSelectionForWand(
									wands[activeWandIndex], spellRecepies[activeWandIndex], false);
								controllerInventoryHasSelectedStone = false;
								controllerInventorySelectedStoneIndex = -1;
								if (stoneInventory.empty())
								{
									controllerInventoryStoneIndex = 0;
									controllerInventoryFocus = 0;
								}
								else
								{
									controllerInventoryStoneIndex = glm::clamp(controllerInventoryStoneIndex,
										0, (int)stoneInventory.size() - 1);
								}
							}
						}
						else
						{
							controllerInventoryHasSelectedStone = false;
							controllerInventorySelectedStoneIndex = -1;
						}
					}
					else if (isWandSlotRemovable(controllerInventoryWandSlot))
					{
						MagicStone stone = wandStoneSlots[activeWandIndex][controllerInventoryWandSlot].stone;
						wandStoneSlots[activeWandIndex][controllerInventoryWandSlot] = {};
						if (auto *slot = getWandSlot(inventoryWand, controllerInventoryWandSlot))
						{
							clearWandSlot(*slot);
						}
						stoneInventory.push_back(stone);
						controllerInventoryStoneIndex = (int)stoneInventory.size() - 1;
						spellSelectionLogic[activeWandIndex].resetSelectionForWand(
							wands[activeWandIndex], spellRecepies[activeWandIndex], false);
					}
				}
			}
		}

		for (int i = 0; i < (int)stoneInventory.size(); i++)
		{
			glm::vec4 stoneRect = {stoneBase.x, stoneBase.y + stoneSpacing * i, stoneSize, stoneSize};
			if (draggingStone && draggingStoneIndex == i)
			{
				continue;
			}
			bool focusedStone = controllerInventoryActive && controllerInventoryFocus == 1
				&& i == controllerInventoryStoneIndex && !controllerInventoryHasSelectedStone;
			bool selectedStone = controllerInventoryHasSelectedStone && i == controllerInventorySelectedStoneIndex;
			glm::vec4 drawStoneRect = selectedStone ? scaledRect(stoneRect, 1.16f) : stoneRect;
			renderStone(drawStoneRect, stoneInventory[i], 1.0f);
			if (focusedStone)
			{
				float outlineWidth = PIXEL_SIZE * 0.8f * uiZoom;
				glm::vec4 outlineColor = {0.9f, 0.12f, 0.12f, 0.95f};
				renderer.renderRectangleOutline(drawStoneRect, outlineColor, outlineWidth);
			}
			if (!draggingStone && input.lMouse.pressed && isInsideRect(stoneRect, cursorPos))
			{
				controllerInventoryHasSelectedStone = false;
				controllerInventorySelectedStoneIndex = -1;
				controllerInventoryFocus = 1;
				controllerInventoryStoneIndex = i;
				draggingStone = true;
				draggingStoneIndex = i;
				draggingStoneOffset = cursorPos - glm::vec2(stoneRect.x, stoneRect.y);
			}
		}

		int quickActionInput = -1;
		if (input.controller.buttons[platform::Controller::Up].pressed) { quickActionInput = 0; }
		if (input.controller.buttons[platform::Controller::Down].pressed) { quickActionInput = 1; }
		if (input.controller.buttons[platform::Controller::Left].pressed) { quickActionInput = 2; }
		if (input.controller.buttons[platform::Controller::Right].pressed) { quickActionInput = 3; }
		if (input.lMouse.pressed && quickHoverSlot >= 0)
		{
			quickActionInput = quickHoverSlot;
		}
		if (quickActionInput >= 0)
		{
			if (quickActionEditIndex == quickActionInput)
			{
				quickActionEditIndex = -1;
			}
			else
			{
				quickActionEditIndex = quickActionInput;
				wands[activeWandIndex].quickActions[quickActionEditIndex].clear();
			}
		}
		if (quickActionEditIndex >= 0 && input.lMouse.pressed && quickActionInput < 0)
		{
			if (ringHoverSlot < 0)
			{
				quickActionEditIndex = -1;
			}
		}
		controllerInventoryActive = usesController && quickActionEditIndex == -1 && !draggingStone;

		// wand stats ring (right side)
		{
			if (!draggingStone && quickActionEditIndex == -1 && input.lMouse.pressed)
			{
				for (int slotIndex = 0; slotIndex < 4; slotIndex++)
				{
					if (!wandStoneSlots[activeWandIndex][slotIndex].hasStone) { continue; }
					if (!isInsideRect(ringSlotRects[slotIndex], cursorPos)) { continue; }
					controllerInventoryHasSelectedStone = false;
					controllerInventorySelectedStoneIndex = -1;
					MagicStone stone = wandStoneSlots[activeWandIndex][slotIndex].stone;
					wandStoneSlots[activeWandIndex][slotIndex] = {};
					if (auto *slot = getWandSlot(inventoryWand, slotIndex))
					{
						clearWandSlot(*slot);
					}
					stoneInventory.push_back(stone);
					draggingStone = true;
					draggingStoneIndex = (int)stoneInventory.size() - 1;
					draggingStoneOffset = cursorPos - glm::vec2(ringSlotRects[slotIndex].x, ringSlotRects[slotIndex].y);
					spellSelectionLogic[activeWandIndex].resetSelectionForWand(
						wands[activeWandIndex], spellRecepies[activeWandIndex], false);
					break;
				}
			}

			int upRemaining = inventoryWand.up.type == WandSlotType::Element ? inventoryWand.up.castCount : 0;
			int downRemaining = inventoryWand.down.type == WandSlotType::Element ? inventoryWand.down.castCount : 0;
			int leftRemaining = inventoryWand.left.type == WandSlotType::Element ? inventoryWand.left.castCount : 0;
			int rightRemaining = inventoryWand.right.type == WandSlotType::Element ? inventoryWand.right.castCount : 0;
			int maxElements = std::min(inventoryWand.maxElementsPerCast, QuickAction::MAX_ELEMENTS);
			if (maxElements < 1) { maxElements = 1; }
			int alwaysCastCount = inventoryWand.alwaysCast.type == WandSlotType::Element ? 1 : 0;
			bool editingQuickAction = quickActionEditIndex >= 0;
			QuickAction *editAction = editingQuickAction ? &inventoryWand.quickActions[quickActionEditIndex] : nullptr;
			int usedElements = editAction ? (editAction->count + alwaysCastCount) : 0;
			bool canAddMore = editAction ? (usedElements < maxElements) : false;

			float ringTop = ringCenter.y - ringSize * 0.5f;
			int manaSlots = std::max(1, inventoryWand.maxMana);
			float manaBoxSize = iconSize * 0.30f;
			float manaGap = manaBoxSize * 0.35f;
			float manaRowWidth = manaBoxSize * manaSlots + manaGap * (manaSlots - 1);
			float manaRowX = ringCenter.x - manaRowWidth * 0.5f;
			float manaRowY = ringTop - manaBoxSize * 1.6f;
			for (int i = 0; i < manaSlots; i++)
			{
				glm::vec4 boxRect = {manaRowX + i * (manaBoxSize + manaGap), manaRowY, manaBoxSize, manaBoxSize};
				renderer.renderRectangle(boxRect, {0.22f, 0.45f, 0.95f, 0.75f});
			}

			int maxCastCount = std::max(1, inventoryWand.maxElementsPerCast);
			float castBoxSize = manaBoxSize * 0.9f;
			float castGap = castBoxSize * 0.35f;
			float castRowWidth = castBoxSize * maxCastCount + castGap * (maxCastCount - 1);
			float castRowX = ringCenter.x - castRowWidth * 0.5f;
			float castRowY = manaRowY - castBoxSize * 1.4f;
			SpellRecepie &currentSpell = spellRecepies[activeWandIndex];
			for (int i = 0; i < maxCastCount; i++)
			{
				glm::vec4 boxRect = {castRowX + i * (castBoxSize + castGap), castRowY, castBoxSize, castBoxSize};
				if (i < currentSpell.count)
				{
					renderer.renderRectangle(boxRect, elementToColor(currentSpell.elements[i]));
				}
				else
				{
					renderer.renderRectangle(boxRect, {0.5f, 0.5f, 0.52f, 0.75f});
				}
			}

			auto consumeRemainingForElement = [&](int element, int &ru, int &rd, int &rl, int &rr)
			{
				if (inventoryWand.up.type == WandSlotType::Element && inventoryWand.up.element == element && ru > 0)
				{
					ru--;
					return true;
				}
				if (inventoryWand.down.type == WandSlotType::Element && inventoryWand.down.element == element && rd > 0)
				{
					rd--;
					return true;
				}
				if (inventoryWand.left.type == WandSlotType::Element && inventoryWand.left.element == element && rl > 0)
				{
					rl--;
					return true;
				}
				if (inventoryWand.right.type == WandSlotType::Element && inventoryWand.right.element == element && rr > 0)
				{
					rr--;
					return true;
				}
				return false;
			};

			auto recomputeRemaining = [&]()
			{
				upRemaining = inventoryWand.up.type == WandSlotType::Element ? inventoryWand.up.castCount : 0;
				downRemaining = inventoryWand.down.type == WandSlotType::Element ? inventoryWand.down.castCount : 0;
				leftRemaining = inventoryWand.left.type == WandSlotType::Element ? inventoryWand.left.castCount : 0;
				rightRemaining = inventoryWand.right.type == WandSlotType::Element ? inventoryWand.right.castCount : 0;
				if (!editAction) { return; }
				usedElements = editAction->count + alwaysCastCount;
				canAddMore = usedElements < maxElements;
				for (int i = 0; i < editAction->count; i++)
				{
					consumeRemainingForElement(editAction->elements[i], upRemaining, downRemaining, leftRemaining, rightRemaining);
				}
			};
			recomputeRemaining();

			if (editAction)
			{
				int selectSlot = -1;
				if (input.controller.RStickButtonUp.pressed) { selectSlot = 0; }
				if (input.controller.RStickButtonDown.pressed) { selectSlot = 1; }
				if (input.controller.RStickButtonLeft.pressed) { selectSlot = 2; }
				if (input.controller.RStickButtonRight.pressed) { selectSlot = 3; }
				if (input.lMouse.pressed && ringHoverSlot >= 0) { selectSlot = ringHoverSlot; }
				if (selectSlot >= 0 && canAddMore)
				{
					if (spellSelectionLogic[activeWandIndex].currentMana < 1.0f)
					{
						// no mana feedback handled by selection logic
					}
					else
					{
						WandSlot *slot = getWandSlot(inventoryWand, selectSlot);
						int *remainingPtr = nullptr;
						switch (selectSlot)
						{
						case 0: remainingPtr = &upRemaining; break;
						case 1: remainingPtr = &downRemaining; break;
						case 2: remainingPtr = &leftRemaining; break;
						case 3: remainingPtr = &rightRemaining; break;
						default: break;
					}
						if (slot && remainingPtr && slot->type == WandSlotType::Element && *remainingPtr > 0)
						{
							if (editAction->add(slot->element, maxElements))
							{
								recomputeRemaining();
							}
						}
					}
				}
			}

			glm::vec4 ringRect = {ringCenter.x - ringSize * 0.5f, ringCenter.y - ringSize * 0.5f,
				ringSize, ringSize};
			glm::vec4 ringShadowRect = {ringRect.x + shadowOffset.x, ringRect.y + shadowOffset.y,
				ringRect.z, ringRect.w};

			auto renderRingPiece = [&](gl2d::Texture t, const WandSlot &slot, int remaining, bool selectable)
			{
				bool hasElement = slot.type == WandSlotType::Element;
				glm::vec3 baseColor = {0.65f, 0.65f, 0.65f};
				if (slot.type == WandSlotType::Disabled)
				{
					baseColor = {0.15f, 0.15f, 0.15f};
				}
				else if (hasElement)
				{
					baseColor = elementToColor(slot.element);
				}
				if (hasElement && (!selectable || remaining <= 0))
				{
					baseColor = glm::mix(baseColor, glm::vec3{0.2f, 0.2f, 0.2f}, 0.6f);
				}
				float opacity = 1.0f;
				renderer.renderRectangle(ringShadowRect, t, {0, 0, 0, shadowAlpha});
				renderer.renderRectangle(ringRect, t, {baseColor, opacity});
			};

			bool upSelectable = !editAction || (upRemaining > 0 && canAddMore);
			bool downSelectable = !editAction || (downRemaining > 0 && canAddMore);
			bool leftSelectable = !editAction || (leftRemaining > 0 && canAddMore);
			bool rightSelectable = !editAction || (rightRemaining > 0 && canAddMore);
			renderRingPiece(assetsManager.upCircle, inventoryWand.up, upRemaining, upSelectable);
			renderRingPiece(assetsManager.downCircle, inventoryWand.down, downRemaining, downSelectable);
			renderRingPiece(assetsManager.leftCircle, inventoryWand.left, leftRemaining, leftSelectable);
			renderRingPiece(assetsManager.rightCircle, inventoryWand.right, rightRemaining, rightSelectable);

			auto renderRingIcon = [&](const WandSlot &slot, int remaining, int slotIndex, bool selectable)
			{
				glm::vec4 iconRect = ringSlotRects[slotIndex];
				glm::vec2 center = {iconRect.x + iconRect.z * 0.5f, iconRect.y + iconRect.w * 0.5f};
				if (slot.type == WandSlotType::Element)
				{
					if (wandStoneSlots[activeWandIndex][slotIndex].hasStone)
					{
						float inset = iconRect.z * 0.12f;
						glm::vec4 stoneRect = {iconRect.x + inset, iconRect.y + inset,
							iconRect.z - inset * 2.0f, iconRect.w - inset * 2.0f};
						renderer.renderRectangle(stoneRect, {0.2f, 0.2f, 0.2f, 0.85f});
					}
					glm::vec4 iconShadowRect = {iconRect.x + shadowOffset.x, iconRect.y + shadowOffset.y,
						iconRect.z, iconRect.w};
					float iconAlpha = selectable ? 1.0f : 0.4f;
					renderer.renderRectangle(iconShadowRect, assetsManager.elements.texture,
						{0, 0, 0, shadowAlpha}, {}, 0,
						assetsManager.elements.atlas.get(slot.element, 0));
					renderer.renderRectangle(iconRect, assetsManager.elements.texture,
						{1, 1, 1, iconAlpha}, {}, 0,
						assetsManager.elements.atlas.get(slot.element, 0));

					char countText[8] = {};
					snprintf(countText, sizeof(countText), "%d", std::max(0, remaining));
					glm::vec2 textPos = {center.x + textOffset, center.y + textSize * 0.35f};
					float textAlpha = remaining > 0 && selectable ? 0.9f : 0.5f;
					renderer.renderText(textPos, countText, assetsManager.font,
						{1, 1, 1, textAlpha}, textSize, 4, 3, false);
				}
				else if (slot.type == WandSlotType::Disabled)
				{
					float xSize = iconSize * 0.45f;
					glm::vec2 textPos = {center.x - xSize * 0.25f, center.y - xSize * 0.20f};
					renderer.renderText(textPos, "X", assetsManager.font,
						{0.28f, 0.28f, 0.28f, 0.9f}, xSize, 4, 3, false);
				}
			};

			renderRingIcon(inventoryWand.up, upRemaining, 0, upSelectable);
			renderRingIcon(inventoryWand.down, downRemaining, 1, downSelectable);
			renderRingIcon(inventoryWand.left, leftRemaining, 2, leftSelectable);
			renderRingIcon(inventoryWand.right, rightRemaining, 3, rightSelectable);

			if (controllerInventoryActive
				&& (controllerInventoryFocus == 0 || controllerInventoryHasSelectedStone)
				&& controllerInventoryWandSlot >= 0
				&& isWandSlotSelectable(controllerInventoryWandSlot))
			{
				int selectedSlot = controllerInventoryWandSlot;
				float outlineWidth = PIXEL_SIZE * 0.9f * uiZoom;
				glm::vec4 outlineColor = controllerInventoryHasSelectedStone
					? glm::vec4{1.0f, 0.5f, 0.28f, 0.95f}
					: glm::vec4{0.9f, 0.12f, 0.12f, 0.95f};
				renderer.renderRectangleOutline(ringSlotRects[selectedSlot], outlineColor, outlineWidth);
			}

			if (inventoryWand.alwaysCast.type == WandSlotType::Element)
			{
				float centerSize = ringSize * 0.38f;
				glm::vec4 centerRect = {ringCenter.x - centerSize * 0.5f, ringCenter.y - centerSize * 0.5f,
					centerSize, centerSize};
				glm::vec4 centerShadowRect = {centerRect.x + shadowOffset.x, centerRect.y + shadowOffset.y,
					centerRect.z, centerRect.w};
				renderer.renderRectangle(centerShadowRect, assetsManager.elements.texture,
					{0, 0, 0, shadowAlpha}, {}, 0,
					assetsManager.elements.atlas.get(inventoryWand.alwaysCast.element, 0));
				renderer.renderRectangle(centerRect, assetsManager.elements.texture,
					{1, 1, 1, 1}, {}, 0,
					assetsManager.elements.atlas.get(inventoryWand.alwaysCast.element, 0));
			}
		}

		// quick action ring (gray)
		{
			glm::vec4 quickRingRect = {quickRingCenter.x - ringSize * 0.5f, quickRingCenter.y - ringSize * 0.5f,
				ringSize, ringSize};
			glm::vec4 quickRingShadowRect = {quickRingRect.x + shadowOffset.x, quickRingRect.y + shadowOffset.y,
				quickRingRect.z, quickRingRect.w};

			auto renderQuickPiece = [&](gl2d::Texture t, int slotIndex)
			{
				glm::vec3 baseColor = {0.35f, 0.35f, 0.35f};
				if (quickHoverSlot == slotIndex)
				{
					baseColor = {0.46f, 0.46f, 0.46f};
				}
				if (quickActionEditIndex == slotIndex)
				{
					baseColor = {0.68f, 0.68f, 0.68f};
				}
				renderer.renderRectangle(quickRingShadowRect, t, {0, 0, 0, shadowAlpha});
				renderer.renderRectangle(quickRingRect, t, {baseColor, 1.0f});
			};

			renderQuickPiece(assetsManager.upCircle, 0);
			renderQuickPiece(assetsManager.downCircle, 1);
			renderQuickPiece(assetsManager.leftCircle, 2);
			renderQuickPiece(assetsManager.rightCircle, 3);

			int maxElements = std::min(inventoryWand.maxElementsPerCast, QuickAction::MAX_ELEMENTS);
			if (maxElements < 1) { maxElements = 1; }
			bool hasAlwaysCast = inventoryWand.alwaysCast.type == WandSlotType::Element;
			for (int slotIndex = 0; slotIndex < 4; slotIndex++)
			{
				const QuickAction &action = inventoryWand.quickActions[slotIndex];
				int filledCount = action.count + (hasAlwaysCast ? 1 : 0);
				filledCount = std::min(filledCount, maxElements);
				float boxSize = iconSize * 0.12f;
				float boxGap = boxSize * 0.4f;
				float totalWidth = maxElements * boxSize + (maxElements - 1) * boxGap;
				glm::vec2 center = {quickRingSlotRects[slotIndex].x + quickRingSlotRects[slotIndex].z * 0.5f,
					quickRingSlotRects[slotIndex].y + quickRingSlotRects[slotIndex].w * 0.5f};
				float startX = center.x - totalWidth * 0.5f;
				float y = center.y + iconSize * 0.12f;

				for (int i = 0; i < maxElements; i++)
				{
					glm::vec4 boxRect = {startX + i * (boxSize + boxGap), y, boxSize, boxSize};
					if (i < filledCount)
					{
						int elementIndex = i;
						if (hasAlwaysCast)
						{
							if (i == 0)
							{
								renderer.renderRectangle(boxRect, elementToColor(inventoryWand.alwaysCast.element));
								continue;
							}
							elementIndex = i - 1;
						}
						if (elementIndex < action.count)
						{
							renderer.renderRectangle(boxRect, elementToColor(action.elements[elementIndex]));
						}
					}
					else
					{
						renderer.renderRectangle(boxRect, {0.18f, 0.18f, 0.18f, 0.7f});
					}
				}
			}
		}

		if (draggingStone && input.lMouse.released)
		{
			if (draggingStoneIndex >= 0 && draggingStoneIndex < (int)stoneInventory.size())
			{
				int dropSlot = -1;
				if (ringSlotRectsReady)
				{
					for (int i = 0; i < 4; i++)
					{
						if (isInsideRect(ringSlotRects[i], cursorPos))
						{
							dropSlot = i;
							break;
						}
					}
				}
				if (dropSlot >= 0)
				{
					MagicStone stone = stoneInventory[draggingStoneIndex];
					if (applyStoneToSlot(activeWandIndex, dropSlot, stone))
					{
						stoneInventory.erase(stoneInventory.begin() + draggingStoneIndex);
						spellSelectionLogic[activeWandIndex].resetSelectionForWand(
							wands[activeWandIndex], spellRecepies[activeWandIndex], false);
					}
				}
			}
			draggingStoneIndex = -1;
			draggingStoneOffset = {};
			draggingStone = false;
		}

		if (draggingStone && draggingStoneIndex >= 0 && draggingStoneIndex < (int)stoneInventory.size())
		{
			glm::vec2 dragPos = cursorPos - draggingStoneOffset;
			glm::vec4 dragRect = {dragPos.x, dragPos.y, stoneSize, stoneSize};
			renderStone(dragRect, stoneInventory[draggingStoneIndex], 0.95f);
		}
		} // inventoryPage == 0

		renderer.popCamera();
	}

	//we want the first frame of the spell to happen in the same frame it was cast
	spellsHolder.update(simDelta, map, particleSystem,
		projectiles, rng, player, entityHolder, fireDirection);


	#if GL2D_USE_SDL_GPU
	if (!renderer.gpuDevice)
	#endif
	{
		renderer.flush();
	}
	return !exitDungeon;
}

void GameLogic::addCameraShake(CameraShakeType type, float intensity, float duration)
{
	cameraShakeSystem.addShake(type, intensity, duration, rng);
}

void GameLogic::close()
{
	int keepSeed = worldSeed;
	int keepTrapDifficulty = trapDifficulty;
	bool keepForceTrap = forceTrapDifficulty;
	int keepFloorIndex = currentFloorIndex;
	bool keepFloor = keepFloorOnClose;
	storeGlobalHdrToneMapSettings(gameHdrPostProcess);

	// Release particle post-process resources before resetting gameplay state.
	particlePostProcessRenderer.cleanup();
	cosmeticDynamicLightSystem.cleanup();
	gameHdrPostProcess.cleanup();
	gameFbo.cleanup();

	*this = {};
	worldSeed = keepSeed; // keep current world seed across resets
	trapDifficulty = keepTrapDifficulty;
	forceTrapDifficulty = keepForceTrap;
	currentFloorIndex = keepFloor ? keepFloorIndex : 0;
	keepFloorOnClose = false;
	inGame = 0;
}
