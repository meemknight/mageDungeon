#include "entity.h"
#include "gameplay/entities/entity.h"
#include "gameplay/summons.h"
#include "gameplay/projectiles/projectiles.h"
#include <cmath>

// Start death animation - called when life <= 0
bool BasicMeleEnemy::startDying()
{
	animator.setAnimation(deathAnimationY);
	animator.positionX = 0;
	animator.timer = deathFrameDuration;
	return true;
}

// Update death animation - returns true to keep dying, false when done
bool BasicMeleEnemy::updateDying(float deltaTime)
{
	animator.timer -= deltaTime;
	if (animator.timer <= 0.0f)
	{
		animator.timer += deathFrameDuration;
		animator.positionX++;
	}

	// Death animation finished when we've played all frames
	if (animator.positionX >= deathAnimationFrames)
	{
		return false; // done dying, remove entity
	}
	return true; // still dying
}

bool BasicMeleEnemy::update(float deltaTime, Map &map, ParticleSystem &mainParticleSystem,
	std::ranlux24_base &rng, Player &player, SummonHolder &summons,
	ProjectileHolder &projectiles)
{
	// Handle hit animation timer
	if (playingHitAnimation)
	{
		hitAnimationTimer -= deltaTime;
		if (hitAnimationTimer <= 0.0f)
		{
			playingHitAnimation = false;
		}
	}

	// Update hover timer for flying enemies
	if (hoverEnabled)
	{
		hoverTimer += deltaTime;
	}

	animator.update(deltaTime, 0.12, 6);

	// Update movement behavior and get move direction
	glm::vec2 moveDir = behavior.update(deltaTime, map, rng,
		physics.getPos(), player.physics.getPos(), summons);

	// Shoot if behavior wants to shoot
	if (behavior.wantsToShoot)
	{
		behavior.shoot(physics.getPos(), projectiles, player, summons, rng);
	}

	// Apply movement
	if (glm::dot(moveDir, moveDir) > 0.0f)
	{
		float speedMultiplier = 1.0f;
		if (behavior.dashActive)
		{
			speedMultiplier = behavior.dashSpeedMultiplier;
		}
		else if (behavior.hoverMeleeActive)
		{
			speedMultiplier = behavior.hoverMeleeSpeedMultiplier;
		}
		float finalSpeed = behavior.speed * speedMultiplier * statusSpeedMultiplier;
		physics.getPos() += moveDir * finalSpeed * deltaTime;
	}

	// Dash contact damage to summons
	if (behavior.dashActive && behavior.dashHitCooldownTimer <= 0.0f &&
		behavior.dashContactDamage > 0.0f)
	{
		for (auto &summon : summons.summons)
		{
			if (!summon->canBeTargeted()) { continue; }
			if (physics.transform.intersectTransform(summon->physics.transform))
			{
				summon->life -= behavior.dashContactDamage;
				glm::vec2 damagePos = summon->physics.getPos();
				damagePos.y -= summon->physics.transform.size.y * 0.6f;
				getDamageViewerSystem().addDamage(behavior.dashContactDamage, damagePos);
				behavior.dashHitCooldownTimer = behavior.dashHitCooldown;
				break;
			}
		}
	}

	// Determine current facing direction from movement (0=down, 1=side, 2=up) and flipX
	// Use direction to target if not moving but chasing (for shooting direction)
	int facingDirection = animator.positionY % 3;
	bool facingFlipX = animator.flipX;
	glm::vec2 facingDir = moveDir;
	if (glm::length2(facingDir) < 1e-6f && behavior.chasing)
	{
		facingDir = behavior.directionToTarget;
	}
	if (glm::length2(facingDir) > 1e-6f)
	{
		glm::vec2 dir = glm::normalize(facingDir);
		float dRight = glm::dot(dir, glm::vec2(1.f, 0.f));
		float dLeft = glm::dot(dir, glm::vec2(-1.f, 0.f));
		float dUp = glm::dot(dir, glm::vec2(0.f, -1.f));
		float dDown = glm::dot(dir, glm::vec2(0.f, 1.f));

		float best = dDown;
		int bestDir = 0;
		facingFlipX = false;

		if (dRight > best) { best = dRight; bestDir = 1; facingFlipX = false; }
		if (dLeft > best) { best = dLeft; bestDir = 1; facingFlipX = true; }
		if (dUp > best) { best = dUp; bestDir = 2; facingFlipX = false; }
		facingDirection = bestDir;
	}

	// Detect animation wrap (frame went from non-zero back to 0)
	bool animationWrapped = (lastAnimationFrame > 0 && animator.positionX == 0);
	lastAnimationFrame = animator.positionX;

	// Check if touching player to trigger hit animation (melee attack)
	if (!playingHitAnimation && physics.transform.intersectTransform(player.physics.transform))
	{
		playingHitAnimation = true;
		hitAnimationTimer = hitAnimationDuration;
		hitAnimationBaseY = 6 + facingDirection;
		animator.setAnimation(hitAnimationBaseY);
		animator.flipX = facingFlipX;
	}

	// If playing hit animation and animation just wrapped, allow direction change
	if (playingHitAnimation && animationWrapped)
	{
		int newHitY = 6 + facingDirection;
		hitAnimationBaseY = newHitY;
		animator.positionY = hitAnimationBaseY;
		animator.flipX = facingFlipX;
	}

	// Set animation based on movement, but only if not playing hit animation
	if (!playingHitAnimation)
	{
		animator.setAnimationBasedOnMovement(moveDir);
	}

	basicPhysicsAndCollisionsCheck(deltaTime, map);
	if ((physics.leftTouch || physics.rightTouch || physics.upTouch || physics.downTouch) &&
		glm::dot(moveDir, moveDir) > 0.0001f)
	{
		behavior.onWallHit();
	}
	return true;
}

void BasicMeleEnemy::onDamaged(float damage)
{
	if (damage <= 0.0f)
	{
		return;
	}
	behavior.requestPatrol();
}

float BasicMeleEnemy::getContactDamage() const
{
	if (behavior.dashActive)
	{
		return behavior.dashContactDamage;
	}
	return contactDamage;
}

void BasicMeleEnemy::render(gl2d::Renderer2D &renderer, ParticlePostProcessRenderer &particlePostProcessRenderer)
{
	glm::vec4 aabb = physics.getAABB();

	auto renderPos = aabb;
	renderPos.z = animator.textureSize.x;
	renderPos.w = animator.textureSize.y;

	renderPos.y -= (renderPos.w - physics.transform.size.y);
	renderPos.x -= (renderPos.z - physics.transform.size.x) / 2;

	renderPos.y += renderOffsetY;
	if (hoverEnabled)
	{
		float hover = std::sin(hoverTimer * hoverSpeed) * hoverHeight;
		renderPos.y -= hover;
	}

	glm::vec4 tint = getStatusTint(statusEffects);
	renderer.renderRectangle(renderPos, tileSet.texture,
		tint, {}, {}, tileSet.atlas.get(animator.positionX, animator.positionY,
		animator.flipX));

	physics.renderCollider(renderer);


}
