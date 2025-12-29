#pragma once
#include <gameplay/particleSystem.h>
#include <gameplay/map.h>
#include <gameplay/player.h>
#include <gameplay/projectiles.h>

struct Spell
{

	int element = 0;

	//how many times it triggers
	int maxFireCount = 1; //tweak
	float triggerDelay = 0.4; //tweak

	float triggerTimer = 0; //the counter
	int currentFireCounter = 0; //the counter

	glm::vec2 createPos = {};
	glm::vec2 createAimDir = {};

	virtual bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, glm::vec2 currentAimDir) = 0;


	bool isFirstTime()
	{
		return currentFireCounter == 0;
	}

	bool isLastTime()
	{
		return currentFireCounter == (maxFireCount - 1);
	}
};

struct SpellsHolder
{

	std::vector<std::unique_ptr<Spell>> spells;

	template <typename T>
	void addSpell(T spell, glm::vec2 createPos, glm::vec2 createAimDir)
	{
		static_assert(std::is_base_of_v<Spell, T>);

		spell.createPos = createPos;
		spell.createAimDir = createAimDir;

		auto ptr = std::make_unique<T>(std::move(spell));
		spells.push_back(std::move(ptr));
	}

	void update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder,
		std::ranlux24_base &rng, Player &player, glm::vec2 currentAimDir)
	{

		for (auto it = spells.begin(); it != spells.end(); )
		{
			Spell &p = **it;
			if (p.currentFireCounter >= p.maxFireCount)
			{
				it = spells.erase(it);
				continue;
			}

			p.triggerTimer -= deltaTime;

			if (p.triggerTimer <= 0)
			{
				p.triggerTimer += p.triggerDelay;

				if (!p.update(deltaTime, map, mainParticleSystem,
					projectileHolder, rng, player, currentAimDir))
				{
					it = spells.erase(it);
					continue;
				}

				p.currentFireCounter++;

				if (p.currentFireCounter >= p.maxFireCount)
				{
					it = spells.erase(it);
					continue;
				}
			}

			++it;
		}
	}


};

//the most basic spell
struct BasicMagicMissleSpell: public Spell
{

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, glm::vec2 currentAimDir)
	{

		auto p = BasicMagicMissle();
		p.physics.velocity = currentAimDir * 10.f; //TODO MOVE, the spell should tell this details
		p.element = element;

		projectileHolder.addProjectile(p, player.physics.getPos());

		return true;
	};

};

//todo move stuff
inline BasicMagicMissleSpell getBasicMagicMissleSpell(int element)
{

	BasicMagicMissleSpell ret;

	ret.element = element;
	ret.maxFireCount = 1;
	ret.triggerDelay = 0.1;
	//todo other stats

	return ret;
}