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

#include <gameplay/player.h>
#include <gameplay/aStar.h>

struct SummonHolder;

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

	float setLifeAndMaxLife(float l)
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

	Entity()
	{
	}

	void basicPhysicsAndCollisionsCheck(float deltaTime, Map &map)
	{
		physics.updateForces(deltaTime, 1);
		physics.resolveConstrains(map);
		physics.updateMove();

	}


	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player, SummonHolder &summons) = 0;

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
		std::ranlux24_base &rng, Player &player, SummonHolder &summons)
	{

		for (auto it = entities.begin(); it != entities.end(); )
		{
			Entity &p = **it;

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

			if ((p.life.life <= 0) ||  !p.update(deltaTime, map, mainParticleSystem, rng, player, summons) ||
				(p.life.life <= 0)
				)
			{
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
		for (size_t j = i + 1; j < holder.entities.size(); j++)
		{
			applyPush(holder.entities[i]->physics, entityWeight,
				holder.entities[j]->physics, entityWeight);
		}
	}

	for (auto &entity : holder.entities)
	{
		applyPush(player.physics, playerWeight, entity->physics, entityWeight);
	}
}

struct BasicMeleEnemy : public Entity
{

	TileSet tileSet;

	enum class AIState { Wander, Chase, Investigate };

	AIState aiState = AIState::Wander;

	std::vector<glm::ivec2> pathTiles;
	int pathIndex = 0;

	float repathTimer = 0.0f;
	float forgetTimer = 0.0f;
	float wanderTimer = 0.0f;
	glm::vec2 idleDir = glm::vec2(0.0f);

	bool wanderWhenIdle = false;          // option requested
	bool chasing = false;
	float timeSinceSeen = 0.0f;
	glm::ivec2 lastGoalTile = {-9999,-9999};
	std::vector<glm::ivec2> path;
	glm::vec2 moveDir = glm::vec2(1,0);


	glm::vec2 wanderTargetWorld = glm::vec2(0.0f);

	// Tuning
	float speed = 2.2f;
	float chaseAcquireRange = 13.0f;      // start chasing if within this distance
	float chaseLoseRange    = 15.0f;     // keep chasing until beyond this distance
	float summonAggroRange = 5.0f;       // switch to summon target if very close
	float turnRate          = 14.0f;     // higher = snappier steering
	float noLOSTimer = 0.0f;
	float forgetAfterNoLOS = 2.0f;     // seconds with no LOS before forgetting
	float repathInterval = 0.25f;      // how often to rebuild A* when needed
	bool seeThroughWalls = false;

	glm::vec2 lastSeenPlayerPos = glm::vec2(0.0f);
	glm::ivec2 lastSeenPlayerTile = glm::ivec2(0);
	bool hasLastSeen = false;

	glm::ivec2 lastPathGoalTile = glm::ivec2(999999); // forces first rebuild


	static inline glm::ivec2 worldToTile(glm::vec2 p) { return glm::ivec2((int)std::floor(p.x), (int)std::floor(p.y)); }
	static inline glm::vec2  tileCenter(glm::ivec2 t) { return glm::vec2(t) + glm::vec2(0.5f, 0.5f); }

	static inline bool isCornerCutBlocked(Map &map, glm::ivec2 from, glm::ivec2 to)
	{
		glm::ivec2 d = to - from;
		if (std::abs(d.x) == 1 && std::abs(d.y) == 1)
		{
			// If moving diagonally, disallow squeezing through a corner:
			// both adjacent orthogonal tiles must be free.
			if (IsBlockedTile(map, from.x + d.x, from.y) || IsBlockedTile(map, from.x, from.y + d.y))
				return true;
		}
		return false;
	}

	// Supercover line walk on grid (good for "direct chase" feel). Returns false if any tile along the line is blocked.
	// Also checks the "no corner cutting" rule for diagonal steps.
	static bool hasClearGridLine8(Map &map, glm::ivec2 start, glm::ivec2 goal)
	{
		int x0 = start.x, y0 = start.y;
		int x1 = goal.x, y1 = goal.y;

		int dx = std::abs(x1 - x0);
		int dy = std::abs(y1 - y0);
		int sx = (x0 < x1) ? 1 : -1;
		int sy = (y0 < y1) ? 1 : -1;

		int err = dx - dy;

		glm::ivec2 prev(x0, y0);

		// Check start (optional)
		if (IsBlockedTile(map, prev.x, prev.y)) return false;

		while (!(x0 == x1 && y0 == y1))
		{
			int e2 = err * 2;

			int nx = x0;
			int ny = y0;

			if (e2 > -dy) { err -= dy; nx += sx; }
			if (e2 < dx) { err += dx; ny += sy; }

			glm::ivec2 curr(nx, ny);

			if (IsBlockedTile(map, curr.x, curr.y)) return false;
			if (isCornerCutBlocked(map, prev, curr)) return false;

			prev = curr;
			x0 = nx; y0 = ny;
		}
		return true;
	}

	static inline glm::vec2 safeNormalize(glm::vec2 v)
	{
		float len2 = glm::dot(v, v);
		if (len2 <= 1e-8f) return glm::vec2(0, 0);
		return v * (1.0f / std::sqrt(len2));
	}


	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		std::ranlux24_base &rng, Player &player, SummonHolder &summons) override;

	void render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer) override;

};

