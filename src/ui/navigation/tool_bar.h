#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"

namespace UI {
    enum class TerrainTool {
        None,
        RaiseLower,
        Smooth,
        Flatten
    };

    class ToolBar {
    public:
        ToolBar() = default;

        void render() {
            float topBarHeight = ImGui::GetFrameHeight();

            // Get window display size
            ImGuiIO& io = ImGui::GetIO();
            float panelWidth = 60.0f; // Width for tool icons/buttons
            float panelHeight = io.DisplaySize.y - topBarHeight;

            // Pin position beneath top bar and lock dimensions
            ImGui::SetNextWindowPos(ImVec2(0.0f, topBarHeight), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

            // Lock panel flags (non-movable, non-resizable UI panel)
            ImGuiWindowFlags windowFlags =
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoBringToFrontOnFocus |
                ImGuiWindowFlags_NoNavFocus;

            // Radius is removed atm (looks bad)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

            if (ImGui::Begin("ToolBarPanel", nullptr, windowFlags)) {
                const ImVec2 buttonSize(48.0f, 48.0f);

                // Raise / Lower
                renderToolButton("R", TerrainTool::RaiseLower, buttonSize, "Raise / Lower Brush");
                renderSeparator();

                // Smooth
                renderToolButton("S", TerrainTool::Smooth, buttonSize, "Smooth Terrain Tool");
                renderSeparator();

                // Flatten
                renderToolButton("F", TerrainTool::Flatten, buttonSize, "Flatten Terrain Tool");

                ImGui::End();
            }

            ImGui::PopStyleVar(4);
        }

        TerrainTool getActiveTool() const { return m_activeTool; }
        void setActiveTool(TerrainTool tool) { m_activeTool = tool; }

    private:
        TerrainTool m_activeTool = TerrainTool::None;

        void renderSeparator() {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
            ImGui::Separator();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        }

        void renderToolButton(const char* label, TerrainTool tool, const ImVec2& size, const char* tooltip) {
            bool isActive = (m_activeTool == tool);

            // Highlight button background if currently active
            if (isActive) {
                ImGuiStyle& style = ImGui::GetStyle();
                ImVec4 activeBg = style.Colors[ImGuiCol_ButtonActive];
                ImGui::PushStyleColor(ImGuiCol_Button, activeBg);
            }

            if (ImGui::Button(label, size)) {
                // Toggle tool off if clicked again, otherwise activate
                m_activeTool = isActive ? TerrainTool::None : tool;
            }

            if (isActive) {
                ImGui::PopStyleColor();
            }

            ImGui::SetItemTooltip("%s", tooltip);
        }
    };
}