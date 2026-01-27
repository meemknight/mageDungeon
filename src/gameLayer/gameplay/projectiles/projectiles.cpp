#include "projectiles.h"
#include <gameplay/damageViewerSystem.h>
#include <gameplay/statusEffects.h>



bool basicProjectileHitEntitiesLogic(PhysicalEntity &physics,
	glm::vec2 projectileMoveDirection, char projectileElement,
	EntityHolder &entities, HitStats hitStats)
{

	auto projectile = physics.transform;

	for (auto &e : entities.entities)
	{

		if (projectile.intersectTransform(e->physics.transform))
		{
			//hit enemy
			glm::vec2 pushBack = {};

			e->life.computeHit(hitStats, projectileElement, e->element, projectileMoveDirection, pushBack);
			e->physics.velocity += pushBack;
			addStatusEffectFromElement(e->statusEffects, e->statusImmunities, projectileElement, 5.0f);

			glm::vec2 damagePos = e->physics.getPos();
			damagePos.y -= e->physics.transform.size.y * 0.6f;
			getDamageViewerSystem().addDamage(hitStats.damage, damagePos);

			return true;
		}


	}

	return false;
}
