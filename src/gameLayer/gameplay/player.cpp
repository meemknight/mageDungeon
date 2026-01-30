#include "player.h"



void Player::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager,
	const Wand &wand, glm::vec2 aimDirection)
{
	// render wand with a slight aim-driven sway
	float aimLen = glm::length(aimDirection);
	if (aimLen <= 0.0001f)
	{
		aimDirection = {1, 0};
	}
	else
	{
		aimDirection /= aimLen;
	}

	glm::vec4 aabb = physics.getAABB();

	auto renderPos = aabb;
	renderPos.z = animator.textureSize.x;
	renderPos.w = animator.textureSize.y;

	renderPos.y -= (renderPos.w - physics.transform.size.y);
	renderPos.x -= (renderPos.z - physics.transform.size.x) / 2;

	renderPos.y += PIXEL_SIZE * 10;

	int anim = animator.positionY;
	bool facingDown = anim == 0 || anim == 3;
	bool facingUp = anim == 2 || anim == 5;
	bool facingSide = anim == 1 || anim == 4;
	if (!facingDown && !facingUp && !facingSide) { facingDown = true; }
	bool facingLeft = facingSide && animator.flipX;
	bool facingRight = facingSide && !animator.flipX;

	const float wandSize = PIXEL_SIZE * 16;
	const glm::vec2 extraOffset = {-wandSize * 0.2f, -wandSize * 0.4f};
	const float handSide = PIXEL_SIZE * 4.0f;
	const float handDown = PIXEL_SIZE * 6.0f;
	const float handUp = PIXEL_SIZE * 2.0f;
	glm::vec2 baseOffset = {};
	if (facingUp)
	{
		baseOffset = {0, handUp};
	}
	else if (facingDown)
	{
		baseOffset = {handSide, handDown};
	}
	else if (facingLeft)
	{
		baseOffset = {handSide, handDown * 0.9f};
	}
	else if (facingRight)
	{
		baseOffset = {-handSide, handDown * 0.9f};
	}

	glm::vec2 swayOffset = aimDirection * (PIXEL_SIZE * 2.5f);
	float swayRotation = aimDirection.y * 20.0f;
	glm::vec2 facingOffset = {};
	if (facingRight)
	{
		facingOffset.x += wandSize * 0.2f;
	}
	else if (facingLeft)
	{
		facingOffset.x -= wandSize * 0.2f;
	}

	glm::vec2 wandPos = physics.transform.getCenter() + baseOffset + extraOffset + facingOffset + swayOffset;
	glm::vec4 wandRect = {wandPos.x - wandSize * 0.5f, wandPos.y - wandSize * 0.5f,
		wandSize, wandSize};
	glm::vec4 wandUv = assetManager.wands.atlas.get(wand.wandSprite, 0);
	if (facingLeft)
	{
		std::swap(wandUv.x, wandUv.z);
	}

	auto renderWand = [&]()
	{
		renderer.renderRectangle(wandRect, assetManager.wands.texture, {1, 1, 1, 1},
			{wandSize * 0.5f, wandSize * 0.5f}, swayRotation, wandUv);
	};

	if (facingUp)
	{
		renderWand();
	}

	glm::vec4 tint = getStatusTint(statusEffects);
	renderer.renderRectangle(renderPos, assetManager.player.texture,
		tint, {}, {}, assetManager.player.atlas.get(animator.positionX, animator.positionY, 
		animator.flipX));

	if (!facingUp)
	{
		renderWand();
	}

	physics.renderCollider(renderer);

}

void Player::update(float deltaTime)
{

	animator.update(deltaTime, 0.12, 6);
	if (wandFailTimer > 0.0f)
	{
		wandFailTimer -= deltaTime;
		if (wandFailTimer < 0.0f) { wandFailTimer = 0.0f; }
	}

	

}

