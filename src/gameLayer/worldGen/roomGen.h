#pragma once
#include <FastNoiseSIMD.h>
#include <gameplay/map.h>

struct RoomGenerator
{
	FastNoiseSIMD *grassDecorNoise = nullptr;

	FastNoiseSIMD *dirtNoise = nullptr;
	FastNoiseSIMD *dirtDecorNoise = nullptr;

	void init()
	{
		grassDecorNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		grassDecorNoise->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
		grassDecorNoise->SetFrequency(0.1f);
		grassDecorNoise->SetFractalOctaves(3);
		grassDecorNoise->SetFractalLacunarity(2.0f);
		grassDecorNoise->SetFractalGain(0.5f);

		// --- Dirt blobs (small circular-ish patches) ---
		dirtNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		dirtNoise->SetNoiseType(FastNoiseSIMD::NoiseType::SimplexFractal);
		dirtNoise->SetFrequency(0.08f);        // higher than grass -> smaller blobs
		dirtNoise->SetFractalOctaves(2);       // less detail -> fewer weird tendrils
		dirtNoise->SetFractalLacunarity(2.0f);
		dirtNoise->SetFractalGain(0.5f);

		// --- Dirt decoration (sparse random dots, not clumps) ---
		dirtDecorNoise = FastNoiseSIMD::NewFastNoiseSIMD();
		dirtDecorNoise->SetNoiseType(FastNoiseSIMD::NoiseType::Simplex); // no fractal -> less clumping
		dirtDecorNoise->SetFrequency(0.75f);   // high freq -> small isolated hits
	}

	void placeGrassLayer(Map &map, int seed)
	{
		for (int y = 0; y < map.size.y; y++)
			for (int x = 0; x < map.size.x; x++)
				map.firstLayer.getBlockUnsafe(x, y).type = Blocks::grass;

		if (!grassDecorNoise) return;

		grassDecorNoise->SetSeed(seed);

		const int sx = map.size.x;
		const int sy = map.size.y;

		float *n = grassDecorNoise->GetNoiseSet(0, 0, 0, sx, sy, 1);

		const float threshold = 0.6f;

		for (int y = 0; y < sy; y++)
		{
			for (int x = 0; x < sx; x++)
			{
				const int idx = x + y * sx;
				const float v = (n[idx] + 1.f) * 0.5f;

				if (v > threshold)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::grass)
						b.type = Blocks::grassDecoration;
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(n);
	}

	// tresshold 0..1
	// low tresshold => few small patches (as you expect with 0.2f)
	void placeRandomDirtSpots(Map &map, int seed, float tresshold)
	{
		if (!dirtNoise || !dirtDecorNoise) return;

		// clamp
		if (tresshold < 0.f) tresshold = 0.f;
		if (tresshold > 1.f) tresshold = 1.f;

		const int sx = map.size.x;
		const int sy = map.size.y;

		// ---- Dirt blobs ----
		dirtNoise->SetSeed(seed);

		// Make low tresshold => HIGH cutoff => fewer hits.
		// 0.2 -> about 0.90 (few patches)
		const float dirtCutoff = 0.95f - (tresshold * 0.45f); // range ~[0.95 .. 0.50]

		float *dn = dirtNoise->GetNoiseSet(0, 0, 0, sx, sy, 1);

		for (int y = 0; y < sy; y++)
		{
			for (int x = 0; x < sx; x++)
			{
				const int idx = x + y * sx;
				const float v = (dn[idx] + 1.f) * 0.5f;

				if (v > dirtCutoff)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);

					// only paint dirt on top of grass layer types
					if (b.type == Blocks::grass || b.type == Blocks::grassDecoration)
						b.type = Blocks::dirt;
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(dn);

		// ---- Dirt decoration (sparse dots, not clumps) ----
		// Use a different seed so it doesn't correlate with blobs too much
		dirtDecorNoise->SetSeed(seed + 1337);

		const float decorCutoff = 0.80f;

		float *ddn = dirtDecorNoise->GetNoiseSet(0, 0, 0, sx, sy, 1);

		for (int y = 0; y < sy; y++)
		{
			for (int x = 0; x < sx; x++)
			{
				const int idx = x + y * sx;
				const float v = (ddn[idx] + 1.f) * 0.5f;

				if (v > decorCutoff)
				{
					auto &b = map.firstLayer.getBlockUnsafe(x, y);
					if (b.type == Blocks::dirt)
						b.type = Blocks::dirtDecoration;
				}
			}
		}

		FastNoiseSIMD::FreeNoiseSet(ddn);
	}

	void clear()
	{
		delete grassDecorNoise;
		delete dirtNoise;
		delete dirtDecorNoise;
		*this = {};
	}

	void generatePlainsRoom(int sizeX, int sizeY, Map &map, int seed)
	{
		map.create(sizeX, sizeY);

		placeGrassLayer(map, seed);

		placeRandomDirtSpots(map, seed + 10, 0.4f);


		map.firstLayer.getBlockUnsafe(32, 30).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(34, 30).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(35, 30).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(36, 30).type = Blocks::dungeonWall;

		map.firstLayer.getBlockUnsafe(5, 3).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(5, 7).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(4, 7).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(6, 7).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(8, 5).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(8, 6).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(8, 7).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(12, 5).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(12, 6).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(12, 7).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(11, 6).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(13, 6).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(14, 6).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(14, 5).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(3, 11).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(4, 11).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(6, 11).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(7, 11).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(3, 12).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(7, 12).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(3, 15).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(7, 15).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(3, 16).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(4, 16).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(6, 16).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(7, 16).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(16, 16).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(17, 16).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(16, 17).type = Blocks::dungeonWall;
		map.firstLayer.getBlockUnsafe(17, 17).type = Blocks::dungeonWall;


	}
};
