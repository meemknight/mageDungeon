#include <gameplay/spellRecepieWindow.h>
#include <gameplay/spellRecepieWindow.h>
#include <gameplay/spells/spells.h>
#include <gameplay/spells/spellTypes.h>
#include <gameplay/player.h>
#include <gameplay/elements.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <string>
#include <vector>

namespace
{
	// Keeps pan/zoom state for the spell recepie graph canvas.
	struct SpellRecepieGraphView
	{
		ImVec2 pan = {};
		float zoom = 1.0f;
		bool centered = false;
	};

	// Holds layout data for a spell recepie node in the graph.
	struct SpellRecepieNode
	{
		int spellType = -1;
		SpellRecepie recepie = {};
		std::string name;
		int depth = 0;
		int element = Elements::NoneElement;
		int parentIndex = -1;
		bool isMissing = false;
		ImVec2 center = {};
	};

	bool isRecepiePrefix(const SpellRecepie &full, const SpellRecepie &prefix)
	{
		if (prefix.count + 1 != full.count) { return false; }
		for (int i = 0; i < prefix.count; i++)
		{
			if (full.elements[i] != prefix.elements[i]) { return false; }
		}
		return true;
	}

	bool recepieLess(const SpellRecepie &a, const SpellRecepie &b)
	{
		int maxCount = std::min(a.count, b.count);
		for (int i = 0; i < maxCount; i++)
		{
			if (a.elements[i] != b.elements[i])
			{
				return a.elements[i] < b.elements[i];
			}
		}
		return a.count < b.count;
	}

	ImU32 toImColor(const glm::vec4 &color, float alphaScale = 1.0f)
	{
		return ImGui::ColorConvertFloat4ToU32({color.r, color.g, color.b, color.a * alphaScale});
	}

	ImU32 pickTextColor(const glm::vec4 &color)
	{
		float brightness = color.r * 0.299f + color.g * 0.587f + color.b * 0.114f;
		if (brightness > 0.62f)
		{
			return IM_COL32(20, 20, 20, 255);
		}
		return IM_COL32(255, 255, 255, 255);
	}

	const char *elementShortName(int element)
	{
		switch (element)
		{
			case Elements::Fire: return "F";
			case Elements::Water: return "W";
			case Elements::Earth: return "E";
			case Elements::Ice: return "I";
			default: return "?";
		}
	}

	std::string buildMissingLabel(const SpellRecepie &recepie)
	{
		std::string label;
		for (int i = 0; i < recepie.count; i++)
		{
			if (!label.empty()) { label += " "; }
			label += elementShortName(recepie.elements[i]);
		}
		return label;
	}

	ImVec2 graphToScreen(const ImVec2 &origin, const ImVec2 &pan, float zoom, const ImVec2 &pos)
	{
		return {origin.x + pan.x + pos.x * zoom, origin.y + pan.y + pos.y * zoom};
	}

	void drawGrid(ImDrawList *drawList, const ImVec2 &origin, const ImVec2 &size,
		const ImVec2 &pan, float zoom)
	{
		float gridStep = 64.0f * zoom;
		if (gridStep < 16.0f) { gridStep = 16.0f; }
		ImU32 gridColor = IM_COL32(255, 255, 255, 18);

		float startX = std::fmod(pan.x, gridStep);
		if (startX < 0.0f) { startX += gridStep; }
		for (float x = origin.x + startX; x < origin.x + size.x; x += gridStep)
		{
			drawList->AddLine({x, origin.y}, {x, origin.y + size.y}, gridColor);
		}

		float startY = std::fmod(pan.y, gridStep);
		if (startY < 0.0f) { startY += gridStep; }
		for (float y = origin.y + startY; y < origin.y + size.y; y += gridStep)
		{
			drawList->AddLine({origin.x, y}, {origin.x + size.x, y}, gridColor);
		}
	}
}

// ImGui window that draws a graph of spell recepies by element sequence.
void renderSpellRecepieWindow(SpellsHolder &spellsHolder, Player &player,
	const glm::vec2 &fireDirection)
{
	ImGui::SetNextWindowSize({900, 640}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Spell Recepies"))
	{
		ImGui::End();
		return;
	}

	ImGui::Text("Middle/Right drag to pan, wheel to zoom, right double-click to recenter.");
	static bool showMissingNodes = true;
	ImGui::Checkbox("Show Missing Recipes", &showMissingNodes);
	ImGui::Separator();

	if (ImGui::BeginChild("SpellRecepieGraph", {0, 0}, true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		static SpellRecepieGraphView view;
		ImDrawList *drawList = ImGui::GetWindowDrawList();
		ImGuiIO &io = ImGui::GetIO();

		ImVec2 canvasPos = ImGui::GetCursorScreenPos();
		ImVec2 canvasSize = ImGui::GetContentRegionAvail();
		if (canvasSize.x < 1.0f) { canvasSize.x = 1.0f; }
		if (canvasSize.y < 1.0f) { canvasSize.y = 1.0f; }

		ImGui::InvisibleButton("spell_recepie_canvas", canvasSize,
			ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
		bool canvasHovered = ImGui::IsItemHovered();

		if (canvasHovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)
			|| ImGui::IsMouseDragging(ImGuiMouseButton_Right)))
		{
			view.pan.x += io.MouseDelta.x;
			view.pan.y += io.MouseDelta.y;
		}

		if (canvasHovered && io.MouseWheel != 0.0f)
		{
			float zoomTarget = view.zoom * (1.0f + io.MouseWheel * 0.12f);
			zoomTarget = std::clamp(zoomTarget, 0.4f, 2.5f);
			if (zoomTarget != view.zoom)
			{
				ImVec2 mouse = io.MousePos;
				ImVec2 graphPos = {
					(mouse.x - canvasPos.x - view.pan.x) / view.zoom,
					(mouse.y - canvasPos.y - view.pan.y) / view.zoom
				};
				view.zoom = zoomTarget;
				view.pan = {
					mouse.x - canvasPos.x - graphPos.x * view.zoom,
					mouse.y - canvasPos.y - graphPos.y * view.zoom
				};
			}
		}

		if (canvasHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Right))
		{
			view.centered = false;
		}

		std::vector<SpellRecepieNode> nodes;
		nodes.reserve(SpellTypes::SPELLS_COUNT + 100);

		int maxDepth = 0;
		float maxLabelWidth = 0.0f;
		for (int spellType = 0; spellType < SpellTypes::SPELLS_COUNT; spellType++)
		{
			SpellRecepie recepie = SpellTypes::getSpellRecepie(spellType);
			if (recepie.count <= 0) { continue; }

			SpellRecepieNode node;
			node.spellType = spellType;
			node.recepie = recepie;
			node.name = SpellTypes::getSpellName(spellType);
			node.depth = recepie.count;
			node.element = recepie.elements[recepie.count - 1];
			nodes.push_back(node);

			ImVec2 labelSize = ImGui::CalcTextSize(node.name.c_str());
			maxLabelWidth = std::max(maxLabelWidth, labelSize.x);
			maxDepth = std::max(maxDepth, node.depth);
		}

		auto hasRecepie = [&](const SpellRecepie &recepie)
		{
			for (const auto &node : nodes)
			{
				if (node.recepie == recepie)
				{
					return true;
				}
			}
			return false;
		};

		int elements[] = {Elements::Fire, Elements::Water, Elements::Earth, Elements::Ice};
		if (showMissingNodes)
		{
			for (int elementA : elements)
			{
				SpellRecepie recepie;
				recepie.add(elementA);
				if (!hasRecepie(recepie))
				{
					SpellRecepieNode node;
					node.recepie = recepie;
					node.depth = recepie.count;
					node.element = recepie.elements[recepie.count - 1];
					node.isMissing = true;
					node.name = buildMissingLabel(recepie);
					nodes.push_back(node);
					ImVec2 labelSize = ImGui::CalcTextSize(node.name.c_str());
					maxLabelWidth = std::max(maxLabelWidth, labelSize.x);
					maxDepth = std::max(maxDepth, node.depth);
				}
			}

			for (int elementA : elements)
			{
				for (int elementB : elements)
				{
					SpellRecepie recepie;
					recepie.add(elementA);
					recepie.add(elementB);
					if (!hasRecepie(recepie))
					{
						SpellRecepieNode node;
						node.recepie = recepie;
						node.depth = recepie.count;
						node.element = recepie.elements[recepie.count - 1];
						node.isMissing = true;
						node.name = buildMissingLabel(recepie);
						nodes.push_back(node);
						ImVec2 labelSize = ImGui::CalcTextSize(node.name.c_str());
						maxLabelWidth = std::max(maxLabelWidth, labelSize.x);
						maxDepth = std::max(maxDepth, node.depth);
					}
				}
			}

			for (int elementA : elements)
			{
				for (int elementB : elements)
				{
					for (int elementC : elements)
					{
						SpellRecepie recepie;
						recepie.add(elementA);
						recepie.add(elementB);
						recepie.add(elementC);
						if (!hasRecepie(recepie))
						{
							SpellRecepieNode node;
							node.recepie = recepie;
							node.depth = recepie.count;
							node.element = recepie.elements[recepie.count - 1];
							node.isMissing = true;
							node.name = buildMissingLabel(recepie);
							nodes.push_back(node);
							ImVec2 labelSize = ImGui::CalcTextSize(node.name.c_str());
							maxLabelWidth = std::max(maxLabelWidth, labelSize.x);
							maxDepth = std::max(maxDepth, node.depth);
						}
					}
				}
			}
		}

		if (!nodes.empty())
		{
			std::vector<std::vector<int>> columns(maxDepth + 1);
			for (int i = 0; i < (int)nodes.size(); i++)
			{
				columns[nodes[i].depth].push_back(i);
			}

			std::sort(columns[1].begin(), columns[1].end(), [&](int a, int b)
			{
				if (nodes[a].element != nodes[b].element)
				{
					return nodes[a].element < nodes[b].element;
				}
				return recepieLess(nodes[a].recepie, nodes[b].recepie);
			});

			for (int depth = 2; depth <= maxDepth; depth++)
			{
				for (int nodeIndex : columns[depth])
				{
					for (int parentIndex : columns[depth - 1])
					{
						if (isRecepiePrefix(nodes[nodeIndex].recepie, nodes[parentIndex].recepie))
						{
							nodes[nodeIndex].parentIndex = parentIndex;
							break;
						}
					}
				}
			}

			struct ColumnGroup
			{
				std::vector<int> nodes;
				int parentIndex = -1;
			};

			std::vector<std::vector<int>> columnOrder(maxDepth + 1);
			std::vector<std::vector<ColumnGroup>> columnGroups(maxDepth + 1);

			float paddingX = 12.0f;
			float paddingY = 6.0f;
			float nodeHeight = ImGui::GetFontSize() + paddingY * 2.0f;
			float nodeWidth = std::max(160.0f, maxLabelWidth + paddingX * 2.0f);
			float columnGap = 110.0f;
			float rowGap = 42.0f;
			float groupGap = 34.0f;
			float marginX = 30.0f;
			float marginY = 20.0f;
			float columnSpacing = nodeWidth + columnGap;
			float rounding = 8.0f;
			float edgePad = 6.0f;
			float minSpacing = nodeHeight + rowGap;

			columnOrder[1] = columns[1];
			for (int index : columnOrder[1])
			{
				columnGroups[1].push_back({{index}, -1});
			}

			std::vector<std::vector<int>> children(nodes.size());
			for (int nodeIndex = 0; nodeIndex < (int)nodes.size(); nodeIndex++)
			{
				int parentIndex = nodes[nodeIndex].parentIndex;
				if (parentIndex >= 0)
				{
					children[parentIndex].push_back(nodeIndex);
				}
			}

			for (int depth = 2; depth <= maxDepth; depth++)
			{
				std::vector<int> orphans;
				for (int nodeIndex : columns[depth])
				{
					if (nodes[nodeIndex].parentIndex < 0)
					{
						orphans.push_back(nodeIndex);
					}
				}

				for (int parentIndex : columnOrder[depth - 1])
				{
					auto group = children[parentIndex];
					if (!group.empty())
					{
						std::sort(group.begin(), group.end(), [&](int a, int b)
						{
							return recepieLess(nodes[a].recepie, nodes[b].recepie);
						});
						columnGroups[depth].push_back({group, parentIndex});
						columnOrder[depth].insert(columnOrder[depth].end(), group.begin(), group.end());
					}
				}

				if (!orphans.empty())
				{
					std::sort(orphans.begin(), orphans.end(), [&](int a, int b)
					{
						return recepieLess(nodes[a].recepie, nodes[b].recepie);
					});
					columnGroups[depth].push_back({orphans, -1});
					columnOrder[depth].insert(columnOrder[depth].end(), orphans.begin(), orphans.end());
				}
			}

			for (auto &node : nodes)
			{
				node.center.x = marginX + (node.depth - 1) * columnSpacing + nodeWidth * 0.5f;
			}

			auto applySpacing = [&](const std::vector<int> &order, const std::vector<float> &desired)
			{
				if (order.empty()) { return; }
				std::vector<float> result = desired;
				for (int i = 1; i < (int)result.size(); i++)
				{
					float minY = result[i - 1] + minSpacing;
					if (result[i] < minY) { result[i] = minY; }
				}
				for (int i = (int)result.size() - 2; i >= 0; i--)
				{
					float maxY = result[i + 1] - minSpacing;
					if (result[i] > maxY) { result[i] = maxY; }
				}
				for (int i = 0; i < (int)order.size(); i++)
				{
					nodes[order[i]].center.y = result[i];
				}
			};

			std::vector<float> desired;
			desired.reserve(columnOrder[1].size());
			float startY = marginY + nodeHeight * 0.5f;
			for (int i = 0; i < (int)columnOrder[1].size(); i++)
			{
				desired.push_back(startY + i * minSpacing);
			}
			applySpacing(columnOrder[1], desired);

			for (int depth = 2; depth <= maxDepth; depth++)
			{
				desired.clear();
				desired.reserve(columnOrder[depth].size());
				for (const auto &group : columnGroups[depth])
				{
					float parentY = marginY + nodeHeight * 0.5f;
					if (group.parentIndex >= 0)
					{
						parentY = nodes[group.parentIndex].center.y;
					}
					float groupStart = parentY - (group.nodes.size() - 1) * minSpacing * 0.5f;
					if (!desired.empty())
					{
						float minStart = desired.back() + minSpacing + groupGap;
						if (groupStart < minStart) { groupStart = minStart; }
					}
					for (int i = 0; i < (int)group.nodes.size(); i++)
					{
						desired.push_back(groupStart + i * minSpacing);
					}
				}
				applySpacing(columnOrder[depth], desired);
			}

			int relaxSteps = 4;
			for (int iter = 0; iter < relaxSteps; iter++)
			{
				for (int depth = 2; depth <= maxDepth; depth++)
				{
					desired.clear();
					desired.reserve(columnOrder[depth].size());
					for (const auto &group : columnGroups[depth])
					{
						float parentY = marginY + nodeHeight * 0.5f;
						if (group.parentIndex >= 0)
						{
							parentY = nodes[group.parentIndex].center.y;
						}
						float groupStart = parentY - (group.nodes.size() - 1) * minSpacing * 0.5f;
						if (!desired.empty())
						{
							float minStart = desired.back() + minSpacing + groupGap;
							if (groupStart < minStart) { groupStart = minStart; }
						}
						for (int i = 0; i < (int)group.nodes.size(); i++)
						{
							desired.push_back(groupStart + i * minSpacing);
						}
					}
					applySpacing(columnOrder[depth], desired);
				}

				for (int depth = maxDepth - 1; depth >= 1; depth--)
				{
					desired.clear();
					desired.reserve(columnOrder[depth].size());
					for (int nodeIndex : columnOrder[depth])
					{
						float targetY = nodes[nodeIndex].center.y;
						const auto &childList = children[nodeIndex];
						if (!childList.empty())
						{
							float sum = 0.0f;
							for (int childIndex : childList)
							{
								sum += nodes[childIndex].center.y;
							}
							targetY = sum / (float)childList.size();
						}
						desired.push_back(targetY);
					}
					applySpacing(columnOrder[depth], desired);
				}
			}

			float boundsMinX = FLT_MAX;
			float boundsMinY = FLT_MAX;
			float boundsMaxX = -FLT_MAX;
			float boundsMaxY = -FLT_MAX;
			for (const auto &node : nodes)
			{
				boundsMinX = std::min(boundsMinX, node.center.x - nodeWidth * 0.5f);
				boundsMaxX = std::max(boundsMaxX, node.center.x + nodeWidth * 0.5f);
				boundsMinY = std::min(boundsMinY, node.center.y - nodeHeight * 0.5f);
				boundsMaxY = std::max(boundsMaxY, node.center.y + nodeHeight * 0.5f);
			}

			boundsMinX -= marginX;
			boundsMaxX += marginX;
			boundsMinY -= marginY;
			boundsMaxY += marginY;
			float graphWidth = boundsMaxX - boundsMinX;
			float graphHeight = boundsMaxY - boundsMinY;

			if (!view.centered)
			{
				view.pan = {
					(canvasSize.x - graphWidth * view.zoom) * 0.5f - boundsMinX * view.zoom,
					(canvasSize.y - graphHeight * view.zoom) * 0.5f - boundsMinY * view.zoom
				};
				view.centered = true;
			}

			drawGrid(drawList, canvasPos, canvasSize, view.pan, view.zoom);

			float scaledNodeWidth = nodeWidth * view.zoom;
			float scaledNodeHeight = nodeHeight * view.zoom;
			float scaledRounding = rounding * view.zoom;
			float fontSize = ImGui::GetFontSize() * view.zoom;
			ImFont *font = ImGui::GetFont();
			ImVec2 mousePos = io.MousePos;

			int hoveredIndex = -1;
			for (int i = 0; i < (int)nodes.size(); i++)
			{
				if (nodes[i].isMissing) { continue; }
				ImVec2 center = graphToScreen(canvasPos, view.pan, view.zoom, nodes[i].center);
				ImVec2 min = {center.x - scaledNodeWidth * 0.5f,
					center.y - scaledNodeHeight * 0.5f};
				ImVec2 max = {min.x + scaledNodeWidth, min.y + scaledNodeHeight};
				if (mousePos.x >= min.x && mousePos.x <= max.x
					&& mousePos.y >= min.y && mousePos.y <= max.y)
				{
					hoveredIndex = i;
				}
			}

			std::vector<bool> highlightNode(nodes.size(), false);
			std::vector<bool> highlightEdge(nodes.size(), false);
			if (hoveredIndex >= 0 && !nodes[hoveredIndex].isMissing
				&& ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				// Cast the selected spell directly from the graph.
				auto spell = SpellTypes::getSpellFromRecepie(nodes[hoveredIndex].recepie);
				spellsHolder.addSpell(std::move(spell), player.physics.getPos(), fireDirection);
			}
			if (hoveredIndex >= 0)
			{
				int current = hoveredIndex;
				while (current >= 0)
				{
					if (!nodes[current].isMissing)
					{
						highlightNode[current] = true;
					}
					int parent = nodes[current].parentIndex;
					if (parent >= 0)
					{
						if (!nodes[current].isMissing)
						{
							highlightEdge[current] = true;
						}
						current = parent;
					}
					else
					{
						break;
					}
				}

				std::vector<int> stack = {hoveredIndex};
				while (!stack.empty())
				{
					int index = stack.back();
					stack.pop_back();
					for (int childIndex : children[index])
					{
						if (!nodes[childIndex].isMissing)
						{
							highlightNode[childIndex] = true;
							highlightEdge[childIndex] = true;
						}
						stack.push_back(childIndex);
					}
				}
			}

			float lineThickness = std::max(2.5f, 3.5f * view.zoom);
			float highlightThickness = std::max(3.5f, 5.5f * view.zoom);
			for (int i = 0; i < (int)nodes.size(); i++)
			{
				const auto &node = nodes[i];
				if (node.parentIndex < 0) { continue; }
				const auto &parent = nodes[node.parentIndex];
				ImVec2 startGraph = {parent.center.x + nodeWidth * 0.5f + edgePad, parent.center.y};
				ImVec2 endGraph = {node.center.x - nodeWidth * 0.5f - edgePad, node.center.y};
				ImVec2 start = graphToScreen(canvasPos, view.pan, view.zoom, startGraph);
				ImVec2 end = graphToScreen(canvasPos, view.pan, view.zoom, endGraph);
				glm::vec4 lineColor = node.isMissing
					? glm::vec4{0.55f, 0.55f, 0.55f, 0.6f}
					: elementToSecondaryColor(node.element);
				ImU32 edgeColor = toImColor(lineColor, node.isMissing ? 0.55f : 0.6f);
				drawList->AddLine(start, end, edgeColor, lineThickness);
			}

			if (hoveredIndex >= 0)
			{
				for (int i = 0; i < (int)nodes.size(); i++)
				{
					if (!highlightEdge[i]) { continue; }
					const auto &node = nodes[i];
					if (node.parentIndex < 0) { continue; }
					const auto &parent = nodes[node.parentIndex];
					ImVec2 startGraph = {parent.center.x + nodeWidth * 0.5f + edgePad, parent.center.y};
					ImVec2 endGraph = {node.center.x - nodeWidth * 0.5f - edgePad, node.center.y};
					ImVec2 start = graphToScreen(canvasPos, view.pan, view.zoom, startGraph);
					ImVec2 end = graphToScreen(canvasPos, view.pan, view.zoom, endGraph);
					glm::vec4 lineColor = elementToColor(node.element);
					ImU32 edgeColor = toImColor(lineColor, 0.95f);
					drawList->AddLine(start, end, edgeColor, highlightThickness);
				}
			}

			for (int i = 0; i < (int)nodes.size(); i++)
			{
				const auto &node = nodes[i];
				ImVec2 center = graphToScreen(canvasPos, view.pan, view.zoom, node.center);
				ImVec2 min = {center.x - scaledNodeWidth * 0.5f,
					center.y - scaledNodeHeight * 0.5f};
				ImVec2 max = {min.x + scaledNodeWidth, min.y + scaledNodeHeight};

				glm::vec4 fillColor = node.isMissing
					? glm::vec4{0.28f, 0.28f, 0.28f, 0.85f}
					: elementToColor(node.element);
				glm::vec4 outlineColor = node.isMissing
					? elementToColor(node.element)
					: elementToSecondaryColor(node.element);
				float dimAlpha = (hoveredIndex >= 0 && !highlightNode[i]) ? 0.25f : 0.9f;
				float outlineAlpha = (hoveredIndex >= 0 && !highlightNode[i]) ? 0.35f : 1.0f;
				ImU32 fill = toImColor(fillColor, dimAlpha);
				ImU32 outline = toImColor(outlineColor, outlineAlpha);
				ImU32 textColor = node.isMissing
					? IM_COL32(210, 210, 210, 220)
					: pickTextColor(fillColor);
				if (hoveredIndex >= 0 && !highlightNode[i])
				{
					textColor = IM_COL32(180, 180, 180, 200);
				}

				drawList->AddRectFilled(min, max, fill, scaledRounding);
				drawList->AddRect(min, max, outline, scaledRounding, 0, 1.0f);
				if (hoveredIndex >= 0 && highlightNode[i])
				{
					ImU32 highlightColor = toImColor(outlineColor, 1.0f);
					drawList->AddRect(min, max, highlightColor, scaledRounding, 0, 2.0f * view.zoom);
				}

				ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, node.name.c_str());
				ImVec2 textPos = {center.x - textSize.x * 0.5f,
					center.y - textSize.y * 0.5f};
				drawList->AddText(font, fontSize, textPos, textColor, node.name.c_str());
			}
		}
	}

	ImGui::EndChild();
	ImGui::End();
}
