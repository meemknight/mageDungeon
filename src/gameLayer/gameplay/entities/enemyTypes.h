#include <gameplay/enemy.h>


namespace EnemyTypes
{

	inline BasicMeleEnemy getSkeletonEnemy()
	{
	
		BasicMeleEnemy ret;
	
		ret.tileSet = getAssetManager().skeleton;
	
		return ret;
	}


};
