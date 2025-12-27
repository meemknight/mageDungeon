#include "player.h"



void Player::render(gl2d::Renderer2D &renderer, AssetsManager &assetManager)
{

	glm::vec4 aabb = physical.getAABB();

	auto renderPos = aabb;
	renderPos.z = animator.textureSize.x;
	renderPos.w = animator.textureSize.y;

	renderPos.y -= (renderPos.w - physical.transform.size.y);
	renderPos.x -= (renderPos.z - physical.transform.size.x) / 2;

	renderPos.y += PIXEL_SIZE * 2;

	renderer.renderRectangle(renderPos, assetManager.player.texture,
		Colors_White, {}, {}, assetManager.player.atlas.get(animator.positionX, animator.positionY, 
		animator.flipX));

	renderer.renderRectangleOutline(aabb, Colors_Blue, 0.02);

}

void Player::update(float deltaTime)
{

	animator.update(deltaTime, 0.12, 6);

	

}

