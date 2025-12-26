#pragma once
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <gameplay/map.h>

//from raylib
bool checkCollisionPointRec(glm::vec2 point, glm::vec4 rec);

//from raylib
bool checkCollisionRecs(glm::vec4 rec1, glm::vec4 rec2);

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
	glm::vec4 getAABB()
	{
		return {pos.x - size.x * 0.5f, pos.y - size.y * 0.5f, size};
	}

	bool intersectPoint(glm::vec4 point, float delta = 0)
	{
		glm::vec4 aabb = getAABB();
		aabb.x -= delta;
		aabb.y -= delta;
		aabb.z += 2 * delta;
		aabb.w += 2 * delta;

		return checkCollisionPointRec(point, aabb);

	}

	bool intersectTransform(Transform2D other, float delta = 0)
	{
		glm::vec4 a = getAABB();
		glm::vec4 b = other.getAABB();

		a.x -= delta;
		a.y -= delta;
		a.z += 2 * delta;
		a.w += 2 * delta;

		b.x -= delta;
		b.y -= delta;
		b.z += 2 * delta;
		b.w += 2 * delta;

		return checkCollisionRecs(a, b);
	}

};


struct PhysicalEntity
{
	PhysicalEntity() {};

	PhysicalEntity(glm::vec2 size) {transform.size = size; };


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

	void updateForces(float deltaTime)
	{
		velocity += acceleration * deltaTime;
		transform.pos += velocity * deltaTime;

		// Universal drag (air resistance / friction)
		float drag = 0.04f; // tweak this for your needs
		velocity -= glm::vec2{velocity.x * std::abs(velocity.x),
			velocity.y * std::abs(velocity.y)} *drag * deltaTime;

		//#include <raymath.h>
		if (glm::length(velocity) < 0.01)
		{
			velocity = {};
		}

		acceleration = {};
	}

};


