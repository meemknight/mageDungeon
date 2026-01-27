#include "spells.h"
#include <randomStuff.h>

glm::vec2 randomlyDriftVector(glm::vec2 in, float driftDegrees, std::ranlux24_base &rng)
{
	if (driftDegrees <= 0.01) { return in; }

	// If zero vector, default to (1, 0)
	if (in.x == 0.f && in.y == 0.f)
		in = {1.f, 0.f};

	// Random angle in [-driftDegrees, +driftDegrees]
	float driftRad = glm::radians(
		getRandomFloat(rng, -driftDegrees, driftDegrees)
	);

	float c = std::cos(driftRad);
	float s = std::sin(driftRad);

	// Rotate vector
	return {
		in.x * c - in.y * s,
		in.x * s + in.y * c
	};
}


void SpellsHolder::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	ProjectileHolder &projectileHolder, std::ranlux24_base &rng, Player &player,
	EntityHolder &entityHolder, glm::vec2 currentAimDir)
{

	for (auto it = spells.begin(); it != spells.end(); )
	{
		Spell &p = **it;

		auto onRemoveSpell = [&]()
		{
		};

		if (p.continuousUpdate)
		{
			if(p.continuousUpdateTimer < 0)
			{
				onRemoveSpell();
				it = spells.erase(it);
				continue;
			}

			p.continuousUpdateTimer -= deltaTime;

			for (int castRepeat = 0; castRepeat < p.elementsPerCast; castRepeat++)
			{
				if (!p.update(deltaTime, map, mainParticleSystem,
					projectileHolder, rng, player, entityHolder,
					randomlyDriftVector(currentAimDir, p.driftAngleDegrees, rng)
					))
				{
					onRemoveSpell();
					it = spells.erase(it);
					goto end;
				}
			}

		}
		else
		{
			if (p.currentFireCounter >= p.maxFireCount)
			{
				onRemoveSpell();
				it = spells.erase(it);
				continue;
			}

			p.triggerTimer -= deltaTime;

			if (p.triggerTimer <= 0)
			{
				p.triggerTimer += p.triggerDelay;

				for (int castRepeat = 0; castRepeat < p.elementsPerCast; castRepeat++)
				{
					if (!p.update(deltaTime, map, mainParticleSystem,
						projectileHolder, rng, player, entityHolder,
						randomlyDriftVector(currentAimDir, p.driftAngleDegrees, rng)
						))
					{
						onRemoveSpell();
						it = spells.erase(it);
						goto end;
					}
				}

				p.currentFireCounter++;

				if (p.currentFireCounter >= p.maxFireCount)
				{
					onRemoveSpell();
					it = spells.erase(it);
					continue;
				}
			}
		}

		p.firstTime = false;

		++it;

		end:
		;
	}
}

void SpellsHolder::renderBeforeEntities(gl2d::Renderer2D &renderer,
	ParticlePostProcessRenderer &particlePostProcessRenderer)
{

	for (auto it = spells.begin(); it != spells.end(); ++it)
	{
		Spell &p = **it;

		p.renderBeforeEntities(renderer);
	}
}
