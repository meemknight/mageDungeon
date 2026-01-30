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
#include <logs.h>
#include <gameplay/entities/entity.h>
#include <gameplay/entities/enemyTypes.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <randomStuff.h>
#include <particles/particleCreator.h>
#include <gameplay/statusEffects.h>

#include <gameplay/elements.h>

#include <worldGen/floorGen.h>

bool GameLogic::init()
{
	
	FloorGenerator floorGenerator;
	floorGenerator.init();

	std::vector<FloorConnection> connections;

	floorGenerator.generateDungeonFloor(140, 140, map, 1234, connections, true, floorInfo);

	floorGenerator.clear();

	wands[0] = getRandomWand(0, rng);
	hasWand[0] = true;
	hasWand[1] = false;
	activeWandIndex = 0;
	spellRecepies[0].clear();
	spellRecepies[1].clear();
	spellSelectionLogic[0] = {};
	spellSelectionLogic[1] = {};
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
	droppedItems.clear();
	summons.clear();

	particlePostProcessRenderer.init();
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
	player.life = player.maxLife;
	playerDamageCooldown = 0.0f;

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
			int index = getRandomInt(rng, 0, (int)spawnPositions.size() - 1);
			glm::vec2 pos = spawnPositions[index];
			spawnPositions[index] = spawnPositions.back();
			spawnPositions.pop_back();

		entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), pos);
		}
	}

	// spawn a few wands in rooms
	{
		std::vector<glm::vec2> spawnPositions;
		spawnPositions.reserve(32);

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
				spawnPositions.push_back(pos);
			}
		}

		int maxSpawns = std::min(24, (int)spawnPositions.size());
		int minSpawns = std::min(8, maxSpawns);
		int spawnCount = maxSpawns > 0 ? getRandomInt(rng, minSpawns, maxSpawns) : 0;
		for (int i = 0; i < spawnCount; i++)
		{
			int index = getRandomInt(rng, 0, (int)spawnPositions.size() - 1);
			glm::vec2 pos = spawnPositions[index];
			spawnPositions[index] = spawnPositions.back();
			spawnPositions.pop_back();

			int tier = getRandomInt(rng, 1, 3);
			droppedItems.spawnWand(pos, getRandomWand(tier, rng), rng);
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
		if (slot->type != WandSlotType::Empty) { return false; }
		wandStoneSlots[wandIndex][slotIndex].hasStone = true;
		wandStoneSlots[wandIndex][slotIndex].stone = stone;
		slot->type = WandSlotType::Element;
		slot->element = stone.element;
		slot->castCount = stone.uses;
		return true;
	};

	auto switchActiveWand = [&](int newIndex, bool pauseManaCharge)
	{
		if (newIndex < 0 || newIndex > 1) { return; }
		if (!hasWand[newIndex]) { return; }
		if (newIndex == activeWandIndex) { return; }
		int oldIndex = activeWandIndex;
		returnStonesFromWand(oldIndex, wands[oldIndex]);
		draggingStoneIndex = -1;
		draggingStoneOffset = {};
		draggingStone = false;
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

	if (input.buttons[platform::Button::Tab].pressed ||
		input.controller.buttons[platform::Controller::Start].pressed)
	{
		inventoryOpen = !inventoryOpen;
	}
	if (!inventoryOpen && draggingStone)
	{
		draggingStoneIndex = -1;
		draggingStoneOffset = {};
		draggingStone = false;
	}

	int wandCountBefore = (hasWand[0] ? 1 : 0) + (hasWand[1] ? 1 : 0);
	bool swapWandInput = !inventoryOpen && (input.buttons[platform::Button::E].pressed ||
		input.controller.buttons[platform::Controller::A].pressed);
	if (swapWandInput)
	{
		int pickedSlot = -1;
		bool swappedWand = false;
		int pickedItemIndex = -1;
		if (droppedItems.trySwapWithPlayer(player.physics.getPos(), wands, hasWand,
			activeWandIndex, pickedSlot, swappedWand, pickedItemIndex, true))
		{
			if (pickedSlot >= 0)
			{
				if (swappedWand && pickedItemIndex >= 0 && pickedItemIndex < (int)droppedItems.items.size())
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

	Wand &currentWand = wands[activeWandIndex];


#pragma region imgui
	//ImGui::ShowDemoWindow();
	if (input.buttons[platform::Button::F10].pressed)
	{
		ImGui::toggleImguiWindowOpen();
	}
	if (ImGui::isImguiWindowOpen())
	{
		ImGui::Begin("Game Debug");

	ImGui::DragFloat2("Position", &player.physics.getPos()[0], 0.01);
	ImGui::DragFloat("zoom", &zoom);
	static int randomWandTier = 1;
	ImGui::SliderInt("Random Wand Tier", &randomWandTier, 0, 5);
	if (ImGui::Button("Random Wand"))
	{
		returnStonesFromWand(activeWandIndex, wands[activeWandIndex]);
		wands[activeWandIndex] = getRandomWand(randomWandTier, rng);
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
	ImGui::Separator();

	static bool particleUseVelocity = false;
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

	ImGui::Separator();
	ImGui::Text("Palette");
	ImGui::Checkbox("Palette Particles", &paletteEffect.enabledParticles);
	ImGui::Checkbox("Palette Game", &paletteEffect.enabledGame);
	if (!paletteEffect.hasPalette())
	{
		ImGui::Text("Palette: not loaded");
	}

	if (ImGui::Button("Exit"))
	{
		exitDungeon = true;
	}

		ImGui::End();
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

	// pause gameplay updates while inventory is open
	float simDelta = inventoryOpen ? 0.0f : deltaTime;

	{
		auto statusTick = updateStatusEffects(player.statusEffects, player.statusImmunities, simDelta);
		player.statusSpeedMultiplier = statusTick.speedMultiplier;
		if (statusTick.damage > 0.0f)
		{
			player.life -= statusTick.damage;
			glm::vec2 damagePos = player.physics.getPos();
			damagePos.y -= player.physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(statusTick.damage, damagePos);
		}
		updateStatusEffectParticles(player.statusEffects, particleSystem, rng, player.physics.getPos(), simDelta);
	}

#pragma region input
	{

		if (!inventoryOpen)
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
				if (l <= 0.000000001)
				{
					fireDirection = {1,0};
				}
				else
				{
					fireDirection /= l;
				}

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
		else
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
	droppedItems.update(simDelta);

	summons.update(simDelta, map, particleSystem, projectiles, rng, player, entityHolder);

#pragma endregion


	renderer.currentCamera.zoom = zoom;
	renderer.currentCamera.follow(player.physics.transform.getCenter(),
		simDelta * 4.f, 0.00001, 0,
		renderer.windowW, renderer.windowH);

	particlePostProcessRenderer.updateWindowMetrics(renderer);

#pragma region rendering
	bool paletteGame = paletteEffect.enabledGame && paletteEffect.hasPalette();
	bool paletteParticles = paletteEffect.enabledParticles && paletteEffect.hasPalette() && !paletteGame;
	if (paletteGame)
	{
		gameFbo.resize(renderer.windowW, renderer.windowH);
		gameFbo.clear();
		gameFbo.bind();
	}

	map.renderMap(renderer, assetsManager);

	map.renderWallShadows(renderer, assetsManager);

	spellsHolder.renderBeforeEntities(renderer, particlePostProcessRenderer);
	droppedItems.render(renderer, assetsManager);

	entityHolder.update(simDelta, map, particleSystem, rng, player, summons);
	resolveEntityPush(entityHolder, player);
	resolveSummonEntityPush(entityHolder, summons);

	// contact damage from enemies
	playerDamageCooldown = std::max(0.0f, playerDamageCooldown - simDelta);
	if (playerDamageCooldown <= 0.0f)
	{
		for (auto &entity : entityHolder.entities)
		{
			if (player.physics.transform.intersectTransform(entity->physics.transform))
			{
				player.life -= 1.0f;
				playerDamageCooldown = 0.6f;
				glm::vec2 damagePos = player.physics.getPos();
				damagePos.y -= player.physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(1.0f, damagePos);
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
				entityHolder.addEntity(EnemyTypes::getSkeletonEnemy(), spawnPos);
				break;
			}
		}
	}
	#pragma endregion
	entityHolder.render(renderer, particlePostProcessRenderer);
	summons.render(renderer, particlePostProcessRenderer);


	//renderer.renderRectangle(player.physical.getAABB(), Colors_Red);
	player.update(simDelta);
	player.render(renderer, assetsManager, currentWand, fireDirection);

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
	projectiles.render(renderer, assetsManager, particlePostProcessRenderer);

	particleSystem.render(renderer, particlePostProcessRenderer, {});

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

	map.renderMapAfterEntities(renderer, assetsManager);
	damageViewerSystem.render(renderer, assetsManager.font);

#pragma endregion

	// player life
	{
		float cameraZoom = renderer.currentCamera.zoom;
		renderer.pushCamera();
		float padding = PIXEL_SIZE * 3.0f * cameraZoom;
		float barWidth = PIXEL_SIZE * 48.0f * cameraZoom;
		float barHeight = PIXEL_SIZE * 6.0f * cameraZoom;
		float x = renderer.windowW - padding - barWidth;
		float y = padding;

		glm::vec4 barRect = {x, y, barWidth, barHeight};
		renderer.renderRectangle(barRect, {0.15f, 0.05f, 0.05f, 0.85f});
		float lifeDisplay = std::max(0.0f, player.life);
		float lifeRatio = player.maxLife > 0.0f ? (lifeDisplay / player.maxLife) : 0.0f;
		lifeRatio = glm::clamp(lifeRatio, 0.0f, 1.0f);
		glm::vec4 fillRect = {x, y, barWidth * lifeRatio, barHeight};
		renderer.renderRectangle(fillRect, {0.9f, 0.1f, 0.1f, 0.9f});
		renderer.renderRectangleOutline(barRect, {0.4f, 0.1f, 0.1f, 0.9f}, PIXEL_SIZE * cameraZoom);

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

	if (!inventoryOpen)
	{
		// wand slots ui
		{
			float cameraZoom = renderer.currentCamera.zoom;
			renderer.pushCamera();
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

		// magic ui
		{
			spellSelectionLogic[activeWandIndex].update(simDelta, renderer, assetsManager,
				spellRecepies[activeWandIndex], spellsHolder, map, projectiles, entityHolder,
				player, fireDirection, usesController, currentWand, input);
		}
	}

	if (inventoryOpen)
	{
		// inventory overlay
		float cameraZoom = renderer.currentCamera.zoom;
		float uiScale = std::min(renderer.windowW / 1280.0f, renderer.windowH / 720.0f);
		float uiZoom = cameraZoom * uiScale;
		renderer.pushCamera();
		renderer.renderRectangle({0, 0, (float)renderer.windowW, (float)renderer.windowH},
			{0.02f, 0.02f, 0.03f, 0.75f});
		// inventory book background
		float bookScale = 0.8f;
		float bookW = renderer.windowW * bookScale;
		float bookH = renderer.windowH * bookScale;
		glm::vec2 bookPos = {(renderer.windowW - bookW) * 0.5f, (renderer.windowH - bookH) * 0.5f};
		bookPos.y += renderer.windowH * 0.03f;
		renderer.renderRectangle({bookPos.x, bookPos.y, bookW, bookH},
			assetsManager.book, {1, 1, 1, 1});

		glm::vec2 shadowOffset = {PIXEL_SIZE * 2.0f * uiZoom, PIXEL_SIZE * 2.0f * uiZoom};
		glm::vec2 wandShadowOffset = {-shadowOffset.x, shadowOffset.y * 0.9f};
		float shadowAlpha = 0.45f;

		auto isInsideRect = [&](glm::vec4 rect, glm::vec2 pos)
		{
			return pos.x >= rect.x && pos.x <= rect.x + rect.z &&
				pos.y >= rect.y && pos.y <= rect.y + rect.w;
		};

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
			glm::vec4 wandRect = {center.x - drawW * 0.5f, center.y - drawH * 0.5f, drawW, drawH};
			glm::vec4 wandShadowRect = {wandRect.x + wandShadowOffset.x, wandRect.y + wandShadowOffset.y,
				wandRect.z, wandRect.w};
			gl2d::Color4f tint = selected ? gl2d::Color4f{1, 1, 1, 1}
				: gl2d::Color4f{0.35f, 0.35f, 0.35f, 0.85f};
			renderer.renderRectangle(wandShadowRect, wandTexture, {0, 0, 0, shadowAlpha});
			renderer.renderRectangle(wandRect, wandTexture, tint);
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
		if (clickedSlot >= 0)
		{
			switchActiveWand(clickedSlot, true);
		}

		float stoneSize = PIXEL_SIZE * 12.0f * uiZoom;
		float stoneSpacing = stoneSize * 1.3f;
		glm::vec2 stoneBase = {bookPos.x + bookW * 0.52f, bookPos.y + bookH * 0.32f};

		auto renderStone = [&](glm::vec4 rect, const MagicStone &stone, float alpha)
		{
			renderer.renderRectangle(rect, {0.2f, 0.2f, 0.2f, 0.85f * alpha});
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

		if (draggingStone && (draggingStoneIndex < 0 || draggingStoneIndex >= (int)stoneInventory.size()))
		{
			draggingStoneIndex = -1;
			draggingStoneOffset = {};
			draggingStone = false;
		}

		for (int i = 0; i < (int)stoneInventory.size(); i++)
		{
			glm::vec4 stoneRect = {stoneBase.x, stoneBase.y + stoneSpacing * i, stoneSize, stoneSize};
			if (draggingStone && draggingStoneIndex == i)
			{
				continue;
			}
			renderStone(stoneRect, stoneInventory[i], 1.0f);
			if (!draggingStone && input.lMouse.pressed && isInsideRect(stoneRect, cursorPos))
			{
				draggingStone = true;
				draggingStoneIndex = i;
				draggingStoneOffset = cursorPos - glm::vec2(stoneRect.x, stoneRect.y);
			}
		}

		glm::vec4 ringSlotRects[4] = {};
		bool ringSlotRectsReady = false;

		// wand stats ring (right side)
		{
			Wand &inventoryWand = wands[activeWandIndex];
			int upRemaining = inventoryWand.up.type == WandSlotType::Element ? inventoryWand.up.castCount : 0;
			int downRemaining = inventoryWand.down.type == WandSlotType::Element ? inventoryWand.down.castCount : 0;
			int leftRemaining = inventoryWand.left.type == WandSlotType::Element ? inventoryWand.left.castCount : 0;
			int rightRemaining = inventoryWand.right.type == WandSlotType::Element ? inventoryWand.right.castCount : 0;

			glm::vec2 ringCenter = {renderer.windowW * 0.66f, renderer.windowH * 0.52f};
			float ringSize = PIXEL_SIZE * 44.0f * uiZoom * (2.0f / 2.2f);
			float ringOffset = ringSize * 0.4f;
			float iconSize = ringSize * 0.34f;
			float textSize = iconSize * 0.55f;
			float textOffset = iconSize * 0.6f;
			glm::vec2 slotDirs[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
			for (int i = 0; i < 4; i++)
			{
				glm::vec2 center = ringCenter + slotDirs[i] * ringOffset;
				ringSlotRects[i] = {center.x - iconSize * 0.5f, center.y - iconSize * 0.5f,
					iconSize, iconSize};
			}
			ringSlotRectsReady = true;

			glm::vec4 ringRect = {ringCenter.x - ringSize * 0.5f, ringCenter.y - ringSize * 0.5f,
				ringSize, ringSize};
			glm::vec4 ringShadowRect = {ringRect.x + shadowOffset.x, ringRect.y + shadowOffset.y,
				ringRect.z, ringRect.w};

			auto renderRingPiece = [&](gl2d::Texture t, const WandSlot &slot, int remaining)
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
				if (hasElement && remaining <= 0)
				{
					baseColor = glm::mix(baseColor, glm::vec3{0.2f, 0.2f, 0.2f}, 0.6f);
				}
				float opacity = 1.0f;
				renderer.renderRectangle(ringShadowRect, t, {0, 0, 0, shadowAlpha});
				renderer.renderRectangle(ringRect, t, {baseColor, opacity});
			};

			renderRingPiece(assetsManager.upCircle, inventoryWand.up, upRemaining);
			renderRingPiece(assetsManager.downCircle, inventoryWand.down, downRemaining);
			renderRingPiece(assetsManager.leftCircle, inventoryWand.left, leftRemaining);
			renderRingPiece(assetsManager.rightCircle, inventoryWand.right, rightRemaining);

			auto renderRingIcon = [&](const WandSlot &slot, int remaining, int slotIndex)
			{
				glm::vec4 iconRect = ringSlotRects[slotIndex];
				glm::vec2 center = {iconRect.x + iconRect.z * 0.5f, iconRect.y + iconRect.w * 0.5f};
				if (slot.type == WandSlotType::Element)
				{
					glm::vec4 iconShadowRect = {iconRect.x + shadowOffset.x, iconRect.y + shadowOffset.y,
						iconRect.z, iconRect.w};
					renderer.renderRectangle(iconShadowRect, assetsManager.elements.texture,
						{0, 0, 0, shadowAlpha}, {}, 0,
						assetsManager.elements.atlas.get(slot.element, 0));
					renderer.renderRectangle(iconRect, assetsManager.elements.texture,
						{1, 1, 1, 1}, {}, 0,
						assetsManager.elements.atlas.get(slot.element, 0));

					char countText[8] = {};
					snprintf(countText, sizeof(countText), "%d", std::max(0, remaining));
					glm::vec2 textPos = {center.x + textOffset, center.y + textSize * 0.35f};
					float textAlpha = remaining > 0 ? 0.9f : 0.5f;
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

			renderRingIcon(inventoryWand.up, upRemaining, 0);
			renderRingIcon(inventoryWand.down, downRemaining, 1);
			renderRingIcon(inventoryWand.left, leftRemaining, 2);
			renderRingIcon(inventoryWand.right, rightRemaining, 3);

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

		renderer.popCamera();
	}


	//we want the first frame of the spell to happen in the same frame it was cast
	spellsHolder.update(simDelta, map, particleSystem,
		projectiles, rng, player, entityHolder, fireDirection);


	renderer.flush();
	return !exitDungeon;
}

void GameLogic::close()
{

	*this = {};
	inGame = 0;
}
