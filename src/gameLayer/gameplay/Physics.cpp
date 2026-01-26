#include "Physics.h"
#include <glm/glm.hpp>


//from raylib
bool checkCollisionPointRec(glm::vec2 point, glm::vec4 rec)
{
	bool collision = false;

	if ((point.x >= rec.x) && (point.x < (rec.x + rec.z)) && (point.y >= rec.y) && (point.y < (rec.y + rec.w))) collision = true;

	return collision;
}

//from raylib
bool checkCollisionRecs(glm::vec4 rec1, glm::vec4 rec2)
{
	bool collision = false;

	if ((rec1.x < (rec2.x + rec2.z) && (rec1.x + rec1.z) > rec2.x) &&
		(rec1.y < (rec2.y + rec2.w) && (rec1.y + rec1.w) > rec2.y)) collision = true;

	return collision;
}

bool checkCollisionCircles(glm::vec2 ca, float da,
	glm::vec2 cb, float db)
{
	const float r = 0.5f * (da + db);
	const float d2 = glm::dot(ca - cb, ca - cb);
	return d2 <= r * r;
}


inline float clampf(float v, float lo, float hi)
{
	return glm::max(lo, glm::min(v, hi));
}

// rect = {x,y,w,h}, circle center + DIAMETER
bool checkCollisionRectCircle(glm::vec4 rect,
	glm::vec2 c, float d)
{
	const float r = 0.5f * d;

	const float closestX = clampf(c.x, rect.x, rect.x + rect.z);
	const float closestY = clampf(c.y, rect.y, rect.y + rect.w);

	const glm::vec2 p(closestX, closestY);
	const glm::vec2 diff = c - p;

	return glm::dot(diff, diff) <= r * r;
}

void PhysicalEntity::updateMove()
{

	if (lastPos.x - transform.pos.x < 0)
	{
		movingRight = -1;
	}
	else if (lastPos.x - transform.pos.x > 0)
	{
		movingRight = 0;
	}

	lastPos = transform.pos;

}

glm::vec4 PhysicalEntity::getAABB()
{
	return transform.getAABB();
}

void PhysicalEntity::resolveConstrains(Map &mapData)
{

	upTouch = false;
	downTouch = false;
	leftTouch = false;
	rightTouch = false;

	glm::vec2 &pos = transform.pos;

	float distance = glm::distance(lastPos, pos);

	if (distance == 0)
	{
		//no movement happened
		return;
	}

	float GRANULARITY = 0.8;

	if (distance <= GRANULARITY)
	{
		checkCollisionOnce(pos,
			mapData);
	}
	else
	{
		glm::vec2 newPos = lastPos;
		glm::vec2 delta = pos - lastPos;

		if (glm::length(delta) <= 0) { goto end; }

		delta = glm::normalize(delta);
		delta *= GRANULARITY;

		do
		{
			newPos += delta;
			glm::vec2 posTest = newPos;
			checkCollisionOnce(newPos, mapData);

			//here we check if checkCollisionOnce changed
			//the position and hence a collision happened
			if (newPos != posTest)
			{
				pos = newPos;
				goto end;
			}

		} while (glm::length((newPos + delta) - pos) > GRANULARITY);

		//we check one last time
		checkCollisionOnce(pos,
			mapData);
	}

end:

	//don't get out of the world on x y
	if (pos.x - transform.size.x / 2 < 0) { pos.x = transform.size.x / 2; leftTouch = true; }
	if (pos.x + transform.size.x / 2 > (mapData.size.x)) { pos.x = mapData.size.x - transform.size.x / 2; rightTouch = true; }

	if (pos.y - transform.size.y / 2 < 0) { pos.y = transform.size.y / 2; upTouch = true; }
	if (pos.y + transform.size.y / 2 > (mapData.size.y)) { pos.y = mapData.size.y - transform.size.y / 2; downTouch = true; }

	if (leftTouch && velocity.x < 0) { velocity.x = 0; }
	if (rightTouch && velocity.x > 0) { velocity.x = 0; }
	if (upTouch && velocity.y < 0) { velocity.y = 0; }
	if (downTouch && velocity.y > 0) { velocity.y = 0; }



}

void PhysicalEntity::checkCollisionOnce(glm::vec2 &pos, Map &mapData)
{
	glm::vec2 delta = pos - lastPos;

	glm::vec2 newPos = performCollision(mapData, {pos.x, lastPos.y}, {delta.x, 0});

	pos = performCollision(mapData, {newPos.x, pos.y}, {0, delta.y});
}


//TODO fix this, 
glm::vec2 PhysicalEntity::performCollision(Map &mapData, glm::vec2 pos, glm::vec2 delta)
{
	int minX = 0;
	int minY = 0;
	int maxX = mapData.size.x;
	int maxY = mapData.size.y;

	if (delta.x == 0 && delta.y == 0) { return pos; }

	glm::vec2 dimensions = transform.size;

	minX = floor(pos.x - dimensions.x / 2.f - 1);
	maxX = ceil((pos.x + dimensions.x / 2.f + 1));

	minY = floor(pos.y - dimensions.y / 2.f - 1);
	maxY = ceil((pos.y + dimensions.y / 2.f + 1));

	minX = std::max(0, minX);
	minY = std::max(0, minY);
	maxX = std::min(mapData.size.x, maxX);
	maxY = std::min(mapData.size.y, maxY);

	for (int y = minY; y < maxY; y++)
		for (int x = minX; x < maxX; x++)
		{
			if (mapData.isCollidableAtPosUnsafe(x, y))
			{
				Transform2D entity;
				entity.pos = pos;
				entity.size = dimensions;
				entity.isCircleCollider = transform.isCircleCollider;

				Transform2D block;
				block.pos = {x + 0.5f, y + 0.5f};
				block.size = {1, 1};

				if (entity.intersectTransform(block, -0.00005f))
				{
					if (entity.isCircleCollider)
					{
						const float radius = dimensions.x * 0.5f;
						const glm::vec4 rect = block.getAABB();
						const float closestX = clampf(pos.x, rect.x, rect.x + rect.z);
						const float closestY = clampf(pos.y, rect.y, rect.y + rect.w);

						if (delta.x != 0.0f)
						{
							const float dy = pos.y - closestY;
							const float push = glm::sqrt(glm::max(0.0f, radius * radius - dy * dy));

							if (delta.x < 0.0f) // moving left
							{
								leftTouch = 1;
								pos.x = (rect.x + rect.z) + push; // block right + radius at contact
								return pos;
							}
							else // moving right
							{
								rightTouch = 1;
								pos.x = rect.x - push;         // block left - radius at contact
								return pos;
							}
						}
						else if (delta.y != 0.0f)
						{
							const float dx = pos.x - closestX;
							const float push = glm::sqrt(glm::max(0.0f, radius * radius - dx * dx));

							if (delta.y < 0.0f) // moving up
							{
								upTouch = 1;
								pos.y = (rect.y + rect.w) + push; // block top + radius at contact
								return pos;
							}
							else // moving down
							{
								downTouch = 1;
								pos.y = rect.y - push;          // block bottom - radius at contact
								return pos;
							}
						}
					}
					else
					{
						if (delta.x != 0.0f)
						{
							const float extentX = dimensions.x * 0.5f;

							if (delta.x < 0.0f) // moving left
							{
								leftTouch = 1;
								pos.x = (x + 1.0f) + extentX; // block right + extent
								return pos;
							}
							else // moving right
							{
								rightTouch = 1;
								pos.x = (float)x - extentX;   // block left - extent
								return pos;
							}
						}
						else if (delta.y != 0.0f)
						{
							const float extentY = dimensions.y * 0.5f;

							if (delta.y < 0.0f) // moving up
							{
								upTouch = 1;
								pos.y = (y + 1.0f) + extentY; // block top + extent
								return pos;
							}
							else // moving down
							{
								downTouch = 1;
								pos.y = (float)y - extentY;   // block bottom - extent
								return pos;
							}
						}
					}
				}
			}

		}

	return pos;
}



