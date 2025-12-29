#pragma once
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <gameplay/map.h>

//from raylib
bool checkCollisionPointRec(glm::vec2 point, glm::vec4 rec);

//from raylib
bool checkCollisionRecs(glm::vec4 rec1, glm::vec4 rec2);

bool checkCollisionCircles(glm::vec2 ca, float da, glm::vec2 cb, float db);

bool checkCollisionRectCircle(glm::vec4 rect, glm::vec2 c, float d);

constexpr static float PIXEL_SIZE = (1.f / 16.f);

struct Transform2D
{

	glm::vec2 pos = {}; // center
	glm::vec2 size = {};

	glm::vec2 getCenter()       const { return {pos.x, pos.y}; }
	glm::vec2 getTop()          const { return {pos.x, pos.y - size.y * 0.5f}; }
	glm::vec2 getBottom()       const { return {pos.x, pos.y + size.y * 0.5f}; }
	glm::vec2 getLeft()         const { return {pos.x - size.x * 0.5f, pos.y}; }
	glm::vec2 getRight()        const { return {pos.x + size.x * 0.5f, pos.y}; }
	glm::vec2 getTopLeft()      const { return {pos.x - size.x * 0.5f, pos.y - size.y * 0.5f}; }
	glm::vec2 getTopRight()     const { return {pos.x + size.x * 0.5f, pos.y - size.y * 0.5f}; }
	glm::vec2 getBottomLeft()   const { return {pos.x - size.x * 0.5f, pos.y + size.y * 0.5f}; }
	glm::vec2 getBottomRight()  const { return {pos.x + size.x * 0.5f, pos.y + size.y * 0.5f}; }

	//also usefull for rendering
	glm::vec4 getAABB() const
	{
		return {pos.x - size.x * 0.5f, pos.y - size.y * 0.5f, size};
	}

	bool intersectPoint(glm::vec4 point, float delta = 0) const
	{
		glm::vec4 aabb = getAABB();
		aabb.x -= delta;
		aabb.y -= delta;
		aabb.z += 2 * delta;
		aabb.w += 2 * delta;

		return checkCollisionPointRec(point, aabb);

	}

	bool intersectTransform(const Transform2D &other, float delta = 0.0f) const
	{
		const bool aCircle = this->isCircleCollider;
		const bool bCircle = other.isCircleCollider;

		if (!aCircle && !bCircle)
		{
			// Rect vs Rect
			glm::vec4 a = getAABB();
			glm::vec4 b = other.getAABB();

			a.x -= delta; a.y -= delta; a.z += 2.0f * delta; a.w += 2.0f * delta;
			b.x -= delta; b.y -= delta; b.z += 2.0f * delta; b.w += 2.0f * delta;

			return checkCollisionRecs(a, b);
		}
		else if (aCircle && bCircle)
		{
			// Circle vs Circle (DIAMETER)
			const float da = this->size.x + 2.0f * delta;
			const float db = other.size.x + 2.0f * delta;

			return checkCollisionCircles(this->pos, da, other.pos, db);
		}
		else
		{
			// Rect vs Circle
			const Transform2D &rectT = aCircle ? other : *this;
			const Transform2D &circleT = aCircle ? *this : other;

			glm::vec4 rect = rectT.getAABB();
			rect.x -= delta; rect.y -= delta; rect.z += 2.0f * delta; rect.w += 2.0f * delta;

			const float d = circleT.size.x + 2.0f * delta;

			return checkCollisionRectCircle(rect, circleT.pos, d);
		}
	}

	void renderCollider(gl2d::Renderer2D &renderer)
	{
		if (isCircleCollider)
		{
			renderer.renderCircleOutline(pos, size.x / 2, Colors_Blue, 0.02);
		}
		else
		{
			renderer.renderRectangleOutline(getAABB(), Colors_Blue, 0.02);
		}

	}

	bool isCircleCollider = 0;
};


struct PhysicalEntity
{
	PhysicalEntity() {};

	PhysicalEntity(glm::vec2 size) { transform.size = size; };
	PhysicalEntity(glm::vec2 size, bool circleCollider) { transform.size = size; transform.isCircleCollider = circleCollider; };


	Transform2D transform;
	glm::vec2 lastPos = {};


	glm::vec2 velocity = {};
	glm::vec2 acceleration = {};


	//used to display the sprite
	bool movingRight = 0;

	bool upTouch = 0;
	bool downTouch = 0;
	bool leftTouch = 0;
	bool rightTouch = 0;

	//should be called only once per frame
	void updateMove();

	//AABB works like this: {X, Y, Width, Height}
	glm::vec4 getAABB();


	void resolveConstrains(Map &mapData);

	void checkCollisionOnce(glm::vec2 &pos, Map &mapData);
	glm::vec2 performCollision(Map &mapData, glm::vec2 pos, glm::vec2 delta);
	
	glm::vec2 &getPos() { return transform.pos;	}

	void teleport(glm::vec2 pos)
	{
		transform.pos = pos;
		lastPos = pos;
	}

	void updateForces(float deltaTime, float drag = 0.04)
	{
		velocity += acceleration * deltaTime;
		transform.pos += velocity * deltaTime;

		// Universal drag (air resistance / friction)
		velocity -= glm::vec2{velocity.x * std::abs(velocity.x),
			velocity.y * std::abs(velocity.y)} *drag * deltaTime;

		//#include <raymath.h>
		if (glm::length(velocity) < 0.01)
		{
			velocity = {};
		}

		acceleration = {};
	}
	
	void renderCollider(gl2d::Renderer2D &renderer)
	{
		transform.renderCollider(renderer);
	}

};


