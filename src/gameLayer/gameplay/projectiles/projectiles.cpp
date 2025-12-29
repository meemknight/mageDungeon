#include "projectiles.h"



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

			return true;
		}


	}

	return false;
}