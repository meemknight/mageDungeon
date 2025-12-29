#pragma once
#include <gameplay/particleSystem.h>
#include <gameplay/map.h>
#include <gameplay/player.h>
#include <gameplay/projectiles/projectiles.h>

struct Spell
{

	int element = 0;

	//how many times it triggers
	int maxFireCount = 1; //tweak
	float triggerDelay = 0.4; //tweak
	float driftAngleDegrees = 0; //tweak
	int elementsPerCast = 1; //tweak


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

	virtual ~Spell() = default;
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

	void addSpell(std::unique_ptr<Spell> spell, glm::vec2 createPos, glm::vec2 createAimDir)
	{
		spell->createPos = createPos;
		spell->createAimDir = createAimDir;

		spells.push_back(std::move(spell));
	}

	void update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder,
		std::ranlux24_base &rng, Player &player, glm::vec2 currentAimDir);


};

//the most basic spell
struct BasicMagicMissleSpell: public Spell
{

	std::unique_ptr<Projectile> projectile;

	bool update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
		ProjectileHolder &projectileHolder, std::ranlux24_base &rng,
		Player &player, glm::vec2 currentAimDir)
	{

		auto pptr = projectile->clone(); // copy dynamic type
		pptr->physics.velocity = currentAimDir * 10.f;
		pptr->element = element;

		projectileHolder.addProjectileAsPtr(std::move(pptr), player.physics.getPos());

		return true;
	};

};
