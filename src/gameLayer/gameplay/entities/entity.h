#pragma once
#include "gameplay/Physics.h"
#include "gameplay/elements.h"
#include <map>
#include <algorithm>
#include <memory>
#include <gameplay/particleSystem.h>
#include <gameplay/statusEffects.h>
#include <gameplay/damageViewerSystem.h>
#include <random>
#include <particles/particleCreator.h>
#include <gameplay/characterAnimator.h>
#include <gameplay/entities/enemyBehavior.h>
#include <gameplay/assetsManager.h>

#include <gameplay/player.h>
#include <gameplay/aStar.h>

struct SummonHolder;
struct ProjectileHolder;

struct HitStats
{
	float damage = 0;
	float pushBack = 0;
};

inline bool beatsElement(int target, int attacker)
{
	switch (attacker)
	{
		case Fire:
			return target == Earth || target == Ice;
		case Water:
			return target == Fire;
		case Ice:
			return target == Earth || target == Water;
		case Earth:
			return target == Water;
		default:
			return false;
	}

	return false;
}

struct EntityLifeThings
{

	EntityLifeThings() {};
	EntityLifeThings(float l) { setLifeAndMaxLife(l); };

	float life = 10;
	float maxLife = 10;

	void setLifeAndMaxLife(float l)
	{
		life = l;
		maxLife = l;
	}

	void computeHit(HitStats hitStats, char hitElement, char hostElement,
		glm::vec2 hitDirecton,
		glm::vec2 &outPushBack)
	{
		outPushBack = {};
		if (glm::length(hitDirecton) > 0.00001)
		{
			hitDirecton = glm::normalize(hitDirecton);
			outPushBack = hitDirecton * hitStats.pushBack;
		}


		float extraDamage = 0;

		if (beatsElement(hostElement, hitElement))
		{
			extraDamage = std::ceil(hitStats.damage * 0.25f);
			extraDamage = std::max(extraDamage, 1.f);
		}

		life -= hitStats.damage;


	}

};


struct Entity
{

	PhysicalEntity physics{glm::vec2{12.f * PIXEL_SIZE, 12.f * PIXEL_SIZE}, true};
	CharacterAnimator animator{glm::vec2(48.f * PIXEL_SIZE,48.f * PIXEL_SIZE)};
	ParticleSystem particleSystem;
	StatusEffects statusEffects;
	StatusImmunities statusImmunities;
	float statusSpeedMultiplier = 1.0f;

	EntityLifeThings life;

	int element = 0;

	// Death animation state - when true, entity is playing death animation
	// and should not interact with anything (no collision, no damage, etc.)
	bool dying = false;

	Entity()
	{
	}

	void basicPhysicsAndCollisionsCheck(float deltaTime, Map &map)
	{
		physics.updateForces(deltaTime, 1);
		physics.resolveConstrains(map);
		physics.updateMove();

	}

	// Called when entity should start dying (life <= 0). Override to start death animation.
	// Return true if death animation is used, false to die immediately.
	virtual bool startDying() { return false; }

	// Called each frame while dying. Return true to keep dying, false to remove entity.
	virtual bool updateDying(float deltaTime) { return false; }

	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player, SummonHolder &summons,
		ProjectileHolder &projectiles) = 0;

	virtual void render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer) = 0;

	virtual void onKill() {};

	virtual ~Entity() = default;

};

struct EntityHolder
{

	std::vector<std::unique_ptr<Entity>> entities;

	//void addEnemy(std::unique_ptr<Enemy> &e, glm::vec2 pos)
	//{
	//	e->physics.teleport(pos);
	//	enemies.push_back(std::move(e));
	//}

	template <typename T>
	void addEntity(T entity, glm::vec2 pos)
	{
		static_assert(std::is_base_of_v<Entity, T>);

		auto ptr = std::make_unique<T>(std::move(entity));
		ptr->physics.teleport(pos);
		entities.push_back(std::move(ptr));
	}

	void update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player, SummonHolder &summons,
		ProjectileHolder &projectiles)
	{

		for (auto it = entities.begin(); it != entities.end(); )
		{
			Entity &p = **it;

			// If dying, only update the death animation
			if (p.dying)
			{
				if (!p.updateDying(deltaTime))
				{
					// Death animation finished, remove entity
					p.onKill();
					mainParticleSystem.copyParticles(
						p.particleSystem, rng, p.physics.getPos());
					it = entities.erase(it);
					continue;
				}
				++it;
				continue;
			}

			// Normal update - skip status effects and damage if dying
			auto statusTick = updateStatusEffects(p.statusEffects, p.statusImmunities, deltaTime);
			p.statusSpeedMultiplier = statusTick.speedMultiplier;
			if (statusTick.damage > 0.0f)
			{
				p.life.life -= statusTick.damage;
				glm::vec2 damagePos = p.physics.getPos();
				damagePos.y -= p.physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(statusTick.damage, damagePos);
			}
			updateStatusEffectParticles(p.statusEffects, mainParticleSystem, rng, p.physics.getPos(), deltaTime);

			// Check if entity should start dying
			if (p.life.life <= 0)
			{
				if (p.startDying())
				{
					// Entity has death animation, continue to next frame
					p.dying = true;
					++it;
					continue;
				}
				// No death animation, remove immediately
				p.onKill();
				mainParticleSystem.copyParticles(
					p.particleSystem, rng, p.physics.getPos());
				it = entities.erase(it);
				continue;
			}

			if (!p.update(deltaTime, map, mainParticleSystem, rng, player, summons, projectiles))
			{
				p.onKill();
				mainParticleSystem.copyParticles(
					p.particleSystem, rng, p.physics.getPos());
				it = entities.erase(it);
				continue;
			}

			// Check again after update in case update caused death
			if (p.life.life <= 0)
			{
				if (p.startDying())
				{
					p.dying = true;
					++it;
					continue;
				}
				p.onKill();
				mainParticleSystem.copyParticles(
					p.particleSystem, rng, p.physics.getPos());
				it = entities.erase(it);
				continue;
			}

			++it;
		}
	}

	void render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer)
	{

		for (auto &e : entities)
		{
			e->render(renderer, particlePostProcessRenderer);
		}

	}

	

};

inline void resolveEntityPush(EntityHolder &holder, Player &player)
{
	const float entityWeight = 1.0f;
	const float playerWeight = 0.5f;
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

	for (size_t i = 0; i < holder.entities.size(); i++)
	{
		if (holder.entities[i]->dying) continue; // skip dying entities
		for (size_t j = i + 1; j < holder.entities.size(); j++)
		{
			if (holder.entities[j]->dying) continue; // skip dying entities
			applyPush(holder.entities[i]->physics, entityWeight,
				holder.entities[j]->physics, entityWeight);
		}
	}

	for (auto &entity : holder.entities)
	{
		if (entity->dying) continue; // skip dying entities
		applyPush(player.physics, playerWeight, entity->physics, entityWeight);
	}
}

struct BasicMeleEnemy : public Entity
{
	TileSet tileSet;

	// Movement/AI behavior - handles chasing, pathfinding, wandering
	EnemyBehavior behavior;

	// Hit animation state - triggers when enemy touches player
	bool playingHitAnimation = false;
	float hitAnimationTimer = 0.0f;
	float hitAnimationDuration = 0.36f; // 6 frames * 0.06s per frame
	int hitAnimationBaseY = 0; // base Y for current hit animation direction
	int lastAnimationFrame = 0; // track previous frame to detect animation wrap

	// Sprite render offset so taller sprites sit lower on the collider.
	float renderOffsetY = PIXEL_SIZE * 10.0f;

	// Hover render offset for flying enemies.
	bool hoverEnabled = false;
	float hoverTimer = 0.0f;
	float hoverSpeed = 2.6f;
	float hoverHeight = PIXEL_SIZE * 1.4f;

	// Death animation state
	static constexpr int deathAnimationY = 9; // death animation row
	static constexpr int deathAnimationFrames = 6;
	static constexpr float deathFrameDuration = 0.10f;

	bool startDying() override;
	bool updateDying(float deltaTime) override;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player, SummonHolder &summons,
		ProjectileHolder &projectiles) override;

	void render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer) override;
};

