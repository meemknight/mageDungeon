// ImGui window that visualizes spell recepie chains and allows quick casting.
struct SpellsHolder;
struct Player;

#include <glm/vec2.hpp>

void renderSpellRecepieWindow(SpellsHolder &spellsHolder, Player &player,
	const glm::vec2 &fireDirection);
