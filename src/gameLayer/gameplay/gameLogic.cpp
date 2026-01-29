#include <gameplay/gameLogic.h>
#include <gameplay/map.h>
#include <imgui.h>
#include <platformInput.h>
#include <gameLayer.h>
#include <glui/glui.h>
#include <imguiTools.h>
#include <iostream>
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

	currentWand = getRandomWand(0, rng);
	droppedItems.clear();

	particlePostProcessRenderer.init();

	if (floorInfo.playerSpawnPos)
	{
		player.physics.teleport(*floorInfo.playerSpawnPos);
	}
	else
	{
		player.physics.getPos() = {35, 35};
	}

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
		currentWand = getRandomWand(randomWandTier, rng);
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
		auto statusTick = updateStatusEffects(player.statusEffects, player.statusImmunities, deltaTime);
		player.statusSpeedMultiplier = statusTick.speedMultiplier;
		if (statusTick.damage > 0.0f)
		{
			player.life -= statusTick.damage;
			glm::vec2 damagePos = player.physics.getPos();
			damagePos.y -= player.physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(statusTick.damage, damagePos);
		}
		updateStatusEffectParticles(player.statusEffects, particleSystem, rng, player.physics.getPos(), deltaTime);
	}

#pragma region input
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

		move += platform::getControllerButtons().LStick * glm::vec2(1, -1);

		if (glm::length(move) != 0)
		{
			move = glm::normalize(move);
			move *= deltaTime * 6.f * player.statusSpeedMultiplier; //player speed
		}

		player.physics.getPos() += move;
		player.animator.setAnimationBasedOnMovement(move);

		bool swapWand = input.buttons[platform::Button::E].pressed ||
			input.controller.buttons[platform::Controller::A].pressed;
		droppedItems.trySwapWithPlayer(player.physics.getPos(), currentWand, swapWand);

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
		}

		platform::showMouse(!usesController);

	}
#pragma endregion

#pragma region updates

	player.physics.resolveConstrains(map);

	player.physics.updateMove();


	projectiles.update(deltaTime, map, particleSystem, rng, entityHolder);


	particleSystem.update(deltaTime);
	damageViewerSystem.update(deltaTime);
	droppedItems.update(deltaTime);

#pragma endregion


	renderer.currentCamera.zoom = zoom;
	renderer.currentCamera.follow(player.physics.transform.getCenter(),
		deltaTime * 4.f, 0.00001, 0,
		renderer.windowW, renderer.windowH);

	particlePostProcessRenderer.updateWindowMetrics(renderer);

#pragma region rendering

	map.renderMap(renderer, assetsManager);

	map.renderWallShadows(renderer, assetsManager);

	spellsHolder.renderBeforeEntities(renderer, particlePostProcessRenderer);
	droppedItems.render(renderer, assetsManager);

	entityHolder.update(deltaTime, map, particleSystem, rng, player);
	resolveEntityPush(entityHolder, player);

	#pragma region temp enemy spawner
	// temporary: spawn enemies for testing
	{
		static float spawnTimer = 0.0f;
		spawnTimer -= deltaTime;

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


	//renderer.renderRectangle(player.physical.getAABB(), Colors_Red);
	player.update(deltaTime);
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

	projectiles.render(renderer, assetsManager, particlePostProcessRenderer);

	particleSystem.render(renderer, particlePostProcessRenderer, {});

	particlePostProcessRenderer.finalRender(renderer);

	map.renderMapAfterEntities(renderer, assetsManager);
	damageViewerSystem.render(renderer, assetsManager.font);

#pragma endregion

	// current wand display
	{
		float cameraZoom = renderer.currentCamera.zoom;
		renderer.pushCamera();
		glui::Frame screenFrame({0, 0, renderer.windowW, renderer.windowH});

		int size = (int)(PIXEL_SIZE * 16 * cameraZoom);
		int padding = (int)(PIXEL_SIZE * 3 * cameraZoom);
		auto wandBox = glui::Box().xLeft(padding).yTop(padding)
			.xDimensionPixels(size).yDimensionPixels(size)();

		renderer.renderRectangle(wandBox, assetsManager.wands.texture, {1, 1, 1, 1}, {}, 0,
			assetsManager.wands.atlas.get(currentWand.wandSprite, 0));

		renderer.popCamera();
	}

	// magic ui
	{
		spellSelectionInputLogic.update(deltaTime, renderer, assetsManager,
			spellRecepie, spellsHolder, player, fireDirection, usesController, currentWand, input);
	}


	//we want the first frame of the spell to happen in the same frame it was cast
	spellsHolder.update(deltaTime, map, particleSystem,
		projectiles, rng, player, entityHolder, fireDirection);


	renderer.flush();
	return !exitDungeon;
}

void GameLogic::close()
{

	*this = {};
	inGame = 0;
}
