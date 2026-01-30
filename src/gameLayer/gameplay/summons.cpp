#include "gameplay/summons.h"
#include "gameplay/summons.h"
#include "gameplay/projectiles/projectiles.h"
#include <particles/particleCreator.h>
#include "gameplay/entities/entity.h"
#include "gameplay/aStar.h"
#include "gameplay/damageViewerSystem.h"
#include <randomStuff.h>
#include <algorithm>
#include <cmath>

static inline glm::vec2 safeNormalize(glm::vec2 v)
{
	float len2 = glm::dot(v, v);
	if (len2 <= 1e-8f) { return glm::vec2(0.0f); }
	return v * (1.0f / std::sqrt(len2));
}

static inline glm::ivec2 worldToTile(glm::vec2 p)
{
	return glm::ivec2((int)std::floor(p.x), (int)std::floor(p.y));
}

void SummonHolder::addSummonAsPtr(std::unique_ptr<SummonEntity> summon, glm::vec2 pos)
{
	if (!summon)
	{
		return;
	}

	summon->physics.teleport(pos);

	int activeCount = 0;
	int removeIndex = -1;
	float lowestTime = 0.0f;

	for (int i = 0; i < (int)summons.size(); i++)
	{
		if (summons[i]->isDying()) { continue; }
		activeCount++;
		float timeLeft = summons[i]->timeLeft;
		if (removeIndex == -1 || timeLeft < lowestTime)
		{
			lowestTime = timeLeft;
			removeIndex = i;
		}
	}

	if (activeCount >= maxSummons && removeIndex != -1)
	{
		summons[removeIndex]->forceKill();
	}

	summons.push_back(std::move(summon));
}

void SummonHolder::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
	Player &player, EntityHolder &entityHolder)
{
	for (int i = 0; i < (int)summons.size(); )
	{
		auto &summon = summons[i];
		if (!summon->isDying())
		{
			summon->timeLeft -= deltaTime;
			if (summon->timeLeft <= 0.0f)
			{
				summon->forceKill();
			}
		}

		if (!summon->update(deltaTime, map, mainParticleSystem,
			projectileHolder, rng, player, entityHolder))
		{
			summons[i] = std::move(summons.back());
			summons.pop_back();
			continue;
		}

		++i;
	}
}

void SummonHolder::render(gl2d::Renderer2D &renderer,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	for (auto &summon : summons)
	{
		summon->render(renderer, particlePostProcessRenderer);
	}
}

void SummonHolder::clear()
{
	summons.clear();
}

void resolveSummonEntityPush(EntityHolder &entityHolder, SummonHolder &summons)
{
	const float entityWeight = 1.0f;
	const float summonWeight = 1.0f;
	const float pushFactor = 0.5f;

	auto getRadius = [&](const PhysicalEntity &entity)
	{
		if (entity.transform.isCircleCollider)
		{
			return entity.transform.size.x * 0.5f;
		}
		return 0.5f * std::max(entity.transform.size.x, entity.transform.size.y);
	};

	auto applyPush = [&](PhysicalEntity &a, float weightA, PhysicalEntity &b, float weightB)
	{
		if (!a.transform.intersectTransform(b.transform))
		{
			return;
		}

		glm::vec2 diff = b.getPos() - a.getPos();
		float dist = glm::length(diff);
		if (dist < 0.0001f)
		{
			diff = {1.0f, 0.0f};
			dist = 0.0001f;
		}

		float overlap = (getRadius(a) + getRadius(b)) - dist;
		if (overlap <= 0.0f)
		{
			return;
		}

		glm::vec2 dir = diff / dist;
		float totalWeight = weightA + weightB;
		if (totalWeight <= 0.0001f)
		{
			return;
		}

		float pushA = overlap * pushFactor * (weightA / totalWeight);
		float pushB = overlap * pushFactor * (weightB / totalWeight);

		a.getPos() -= dir * pushA;
		b.getPos() += dir * pushB;
	};

	for (size_t i = 0; i < summons.summons.size(); i++)
	{
		if (summons.summons[i]->isDying()) { continue; }
		for (size_t j = i + 1; j < summons.summons.size(); j++)
		{
			if (summons.summons[j]->isDying()) { continue; }
			applyPush(summons.summons[i]->physics, summonWeight,
				summons.summons[j]->physics, summonWeight);
		}
	}

	for (auto &summon : summons.summons)
	{
		if (summon->isDying()) { continue; }
		for (auto &entity : entityHolder.entities)
		{
			applyPush(summon->physics, summonWeight, entity->physics, entityWeight);
		}
	}
}

SlimeSummon::SlimeSummon()
{
	maxLife = 5.0f;
	life = maxLife;
	timeLeft = 50.0f;
	tileSet = getAssetManager().waterSlime;
	physics.transform.size = {12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE};
	physics.transform.isCircleCollider = true;
	particleSystem.maxCount = 80;
}

void SlimeSummon::forceKill()
{
	if (dying)
	{
		return;
	}

	dying = true;
	deathFrameIndex = 0;
	deathFrameTimer = deathFrameDuration;
	deathHoldTimer = deathHoldDuration;
	fadeAlpha = 1.0f;
	animator.setAnimation(6);
	animator.positionX = 0;
}

bool SlimeSummon::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
	Player &player, EntityHolder &entityHolder)
{
	(void)mainParticleSystem;

	if (dying)
	{
		if (deathFrameIndex < deathFrames - 1)
		{
			deathFrameTimer -= deltaTime;
			if (deathFrameTimer <= 0.0f)
			{
				deathFrameIndex++;
				deathFrameTimer += deathFrameDuration;
				animator.positionX = std::min(deathFrameIndex, deathFrames - 1);
			}
		}
		else
		{
			deathHoldTimer -= deltaTime;
			if (deathHoldTimer <= 0.0f)
			{
				float fadeStep = deltaTime / std::max(0.001f, deathFadeDuration);
				fadeAlpha = std::max(0.0f, fadeAlpha - fadeStep);
			}
		}

		particleSystem.update(deltaTime);
		return fadeAlpha > 0.0f;
	}

	if (life <= 0.0f)
	{
		forceKill();
		return true;
	}

	if (!particlesInitialized)
	{
		glm::vec4 startColor = elementToSecondaryColor(element);
		glm::vec4 endColor = elementToColor(element);
		ambientParticle = getSmallSquareParticle(startColor, endColor);
		ambientParticle.onCreateCount = 1;
		ambientParticle.particleLifeTime = {0.35f, 0.6f};
		ambientParticle.createApearence.size = glm::vec2{2.0f, 3.0f} * PIXEL_SIZE;
		ambientParticle.endApearence.size = glm::vec2{1.4f, 2.4f} * PIXEL_SIZE;
		ambientParticle.positionX = glm::vec2{-4.2f, 4.2f} * PIXEL_SIZE;
		ambientParticle.folowParent = true;
		ambientParticle.texture = getAssetManager().particleCircle;
		particlesInitialized = true;
		particleTimer = getRandomFloat(rng, 0.0f, particleInterval);
	}

	contactCooldown = std::max(0.0f, contactCooldown - deltaTime);
	attackCooldown = std::max(0.0f, attackCooldown - deltaTime);

	if (contactCooldown <= 0.0f)
	{
		for (auto &enemy : entityHolder.entities)
		{
			if (physics.transform.intersectTransform(enemy->physics.transform))
			{
				life -= 1.0f;
				contactCooldown = contactDamageInterval;
				glm::vec2 damagePos = physics.getPos();
				damagePos.y -= physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(1.0f, damagePos);
				break;
			}
		}
	}

	if (life <= 0.0f)
	{
		forceKill();
		return true;
	}

	particleTimer -= deltaTime;
	while (particleTimer <= 0.0f)
	{
		particleTimer += particleInterval;
		ParticleSettings particle = ambientParticle;
		glm::vec2 spawnPos = physics.getPos();
		if (element == Elements::Fire)
		{
			particle.velocityY = glm::vec2{-6.0f, -12.0f} * PIXEL_SIZE;
			particle.velocityX = glm::vec2{-2.0f, 2.0f} * PIXEL_SIZE;
			particle.positionY = glm::vec2{2.0f, 5.0f} * PIXEL_SIZE;
		}
		else if (element == Elements::Ice)
		{
			particle.velocityY = glm::vec2{6.0f, 10.0f} * PIXEL_SIZE;
			particle.velocityX = glm::vec2{-1.5f, 1.5f} * PIXEL_SIZE;
			particle.positionY = glm::vec2{-6.0f, -2.5f} * PIXEL_SIZE;
		}
		else
		{
			particle.velocityY = glm::vec2{-2.0f, -5.0f} * PIXEL_SIZE;
			particle.velocityX = glm::vec2{-2.5f, 2.5f} * PIXEL_SIZE;
			particle.positionY = glm::vec2{3.0f, 6.0f} * PIXEL_SIZE;
		}
		particleSystem.emitParticles(particle, spawnPos, rng, physics.getPos());
	}

	glm::vec2 playerPos = player.physics.getPos();
	glm::vec2 summonPos = physics.getPos();
	glm::vec2 toPlayer = playerPos - summonPos;
	float distToPlayer = glm::length(toPlayer);
	if (distToPlayer > teleportDistance)
	{
		physics.teleport(playerPos);
		returningToPlayer = false;
		pathTiles.clear();
		pathIndex = 0;
		repathTimer = 0.0f;
		lastPathGoalTile = {999999, 999999};
		return true;
	}

	if (distToPlayer > returnDistance)
	{
		returningToPlayer = true;
	}
	if (returningToPlayer && distToPlayer <= returnStopDistance)
	{
		returningToPlayer = false;
	}

	glm::vec2 moveDir = {};
	float moveSpeed = baseSpeed;

	auto resetPath = [&]()
	{
		pathTiles.clear();
		pathIndex = 0;
		repathTimer = 0.0f;
		lastPathGoalTile = {999999, 999999};
	};

	auto computePathMove = [&](glm::vec2 targetPos)
	{
		glm::vec2 toTarget = targetPos - summonPos;
		float distTarget2 = glm::length2(toTarget);
		if (distTarget2 <= 0.0001f)
		{
			resetPath();
			return glm::vec2(0.0f);
		}

		if (CanChaseDirect(map, summonPos, targetPos))
		{
			resetPath();
			return glm::normalize(toTarget);
		}

		repathTimer -= deltaTime;
		glm::ivec2 goalTile = worldToTile(targetPos);
		if (repathTimer <= 0.0f || pathTiles.empty() ||
			pathIndex >= (int)pathTiles.size() || goalTile != lastPathGoalTile)
		{
			repathTimer = repathInterval;
			lastPathGoalTile = goalTile;
			pathTiles = findPathAStar8(map, worldToTile(summonPos), goalTile);
			pathIndex = 0;
		}

		if (!pathTiles.empty() && pathIndex < (int)pathTiles.size())
		{
			glm::ivec2 currT = worldToTile(summonPos);
			while (pathIndex < (int)pathTiles.size() && pathTiles[pathIndex] == currT)
			{
				pathIndex++;
			}

			if (pathIndex < (int)pathTiles.size())
			{
				const glm::vec2 nextCenter = glm::vec2(pathTiles[pathIndex]) + glm::vec2(0.5f);
				glm::vec2 toNext = nextCenter - summonPos;
				if (glm::length2(toNext) < 0.05f * 0.05f)
				{
					pathIndex++;
				}
				else
				{
					return glm::normalize(toNext);
				}
			}
		}

		return safeNormalize(toTarget);
	};

	if (returningToPlayer)
	{
		moveDir = computePathMove(playerPos);
		moveSpeed = catchupSpeed;
	}
	else
	{
		glm::vec2 targetPos = {};
		bool hasTarget = false;
		float bestDist2 = attackRange * attackRange;
		for (auto &enemy : entityHolder.entities)
		{
			glm::vec2 diff = enemy->physics.getPos() - summonPos;
			float dist2 = glm::dot(diff, diff);
			if (dist2 > bestDist2)
			{
				continue;
			}

			if (!HasLineOfSightTiles(map, worldToTile(summonPos), worldToTile(enemy->physics.getPos())))
			{
				continue;
			}

			bestDist2 = dist2;
			targetPos = enemy->physics.getPos();
			hasTarget = true;
		}

		if (hasTarget)
		{
			resetPath();
			glm::vec2 toTarget = targetPos - summonPos;
			float dist = glm::length(toTarget);
			glm::vec2 aimDir = dist > 0.0001f ? (toTarget / dist) : glm::vec2(1.0f, 0.0f);
			if (attackCooldown <= 0.0f)
			{
				HitStats hitStats;
				hitStats.damage = attackDamage;
				hitStats.pushBack = attackPushBack;
				auto bolt = std::make_unique<BasicMagicMissle>(hitStats, projectileSizeBias);
				bolt->element = element;
				bolt->statusAmount = statusAmount;
				bolt->physics.velocity = aimDir * attackProjectileSpeed;
				projectileHolder.addProjectileDeferredAsPtr(std::move(bolt), summonPos);
				attackCooldown = attackInterval;
			}

			if (dist > 1.4f)
			{
				moveDir = aimDir;
			}
		}
		else if (distToPlayer > followDistance)
		{
			moveDir = computePathMove(playerPos);
		}
		else
		{
			resetPath();
		}
	}

	if (glm::dot(moveDir, moveDir) > 0.0f)
	{
		physics.getPos() += moveDir * moveSpeed * deltaTime;
	}

	animator.setAnimationBasedOnMovement(moveDir);
	float animSpeed = animator.positionY >= 3 ? moveAnimSpeed : idleAnimSpeed;
	animator.update(deltaTime, 0.12f / std::max(0.01f, animSpeed), 6);

	physics.resolveConstrains(map);
	physics.updateMove();

	if ((physics.leftTouch || physics.rightTouch || physics.upTouch || physics.downTouch) &&
		glm::dot(moveDir, moveDir) > 0.0001f)
	{
		repathTimer = 0.0f;
		lastPathGoalTile = {999999, 999999};
		if (!pathTiles.empty() && pathIndex < (int)pathTiles.size())
		{
			pathIndex++;
		}
	}

	particleSystem.update(deltaTime);

	return true;
}

void SlimeSummon::render(gl2d::Renderer2D &renderer,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	particleSystem.render(renderer, particlePostProcessRenderer, physics.getPos());
	glm::vec4 aabb = physics.getAABB();
	glm::vec4 renderPos = aabb;
	float renderScale = 0.5f;
	renderPos.z = animator.textureSize.x * renderScale;
	renderPos.w = animator.textureSize.y * renderScale;
	renderPos.y -= (renderPos.w - physics.transform.size.y);
	renderPos.x -= (renderPos.z - physics.transform.size.x) / 2;
	renderPos.y += PIXEL_SIZE * 6;

	glm::vec4 tint = {1.0f, 1.0f, 1.0f, fadeAlpha};
	renderer.renderRectangle(renderPos, tileSet.texture,
		tint, {}, {}, tileSet.atlas.get(animator.positionX, animator.positionY,
		animator.flipX));

	physics.renderCollider(renderer);
	(void)particlePostProcessRenderer;
}
