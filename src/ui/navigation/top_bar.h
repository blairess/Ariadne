#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "../../core/history/history_manager.h"
#include "../../core/terrain/terrain.h"
#include "../../core/io/export/obj_exporter.h"

#include <string>

namespace UI {

	// Tracks which sub-window is currently open
	enum class ActiveModal {
		None,
		ProjectSettings,
		Customization
	};

	class TopBar {
	private:
		// Holds the active window state for this bar
		ActiveModal m_ActiveModal = ActiveModal::None;
		bool m_RoundingApplied = false;

		// Applies corner rounding to windows, buttons, and popups
		void applyRounding() {
			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowRounding = 6.0f;
			style.FrameRounding = 4.0f;
			style.PopupRounding = 6.0f;
			style.ScrollbarRounding = 4.0f;
			style.GrabRounding = 4.0f;
		}

	public:
		void render(GLFWwindow* window, HistoryManager& history, const Terrain& terrain) {

			// Apply rounding once on the first render frame
			if (!m_RoundingApplied) {
				applyRounding();
				m_RoundingApplied = true;
			}

			// Render the top main menu bar
			if (ImGui::BeginMainMenuBar()) {

				// File menu options
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem("New Project")) { /* TODO */ }
					if (ImGui::MenuItem("Open Project")) { /* TODO */ }
					ImGui::Separator();
					if (ImGui::MenuItem("Save")) { /* TODO */ }
					if (ImGui::MenuItem("Save As...")) { /* TODO */ }
					ImGui::Separator();
					if (ImGui::MenuItem("Import...")) { /* TODO */ }

					// Sub-menu with arrow on hover
					if (ImGui::BeginMenu("Export")) {
						if (ImGui::MenuItem("Export as .obj")) {
							Core::IO::Export::OBJExporter::exportWithDialog(terrain);
						}
						ImGui::EndMenu(); // Closes the Export sub-menu
					}

					ImGui::Separator();

					// Set active modal to open Project Settings window
					if (ImGui::MenuItem("Project Settings")) {
						m_ActiveModal = ActiveModal::ProjectSettings;
					}
					// Set active modal to open Customization window
					if (ImGui::MenuItem("Customization")) {
						m_ActiveModal = ActiveModal::Customization;
					}

					ImGui::Separator();
					if (ImGui::MenuItem("Exit", "Esc")) {
						glfwSetWindowShouldClose(window, true);
					}
					ImGui::EndMenu();
				}

				// Edit menu options
				if (ImGui::BeginMenu("Edit")) {
					if (ImGui::MenuItem("Undo", "CTRL + Z", false, history.canUndo())) {
						history.undo();
					}
					if (ImGui::MenuItem("Redo", "CTRL + Y", false, history.canRedo())) {
						history.redo();
					}
					ImGui::EndMenu();
				}

				// View menu options
				if (ImGui::BeginMenu("View")) {
					if (ImGui::MenuItem("Edit Mode")) { /* TODO */ }
					if (ImGui::MenuItem("Painting Mode")) { /* TODO */ }
					if (ImGui::MenuItem("Spectator Mode")) { /* TODO */ }
					ImGui::EndMenu();
				}

				// Help menu options
				if (ImGui::BeginMenu("Help")) {
					if (ImGui::MenuItem("Controls")) { /* TODO */ }
					if (ImGui::MenuItem("Support")) { /* TODO */ }
					if (ImGui::MenuItem("Community")) { /* TODO */ }
					ImGui::Separator();
					if (ImGui::MenuItem("Bug Report")) { /* TODO */ }
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}

			// Render active sub-window if one is open
			renderActiveModal();
		}

	private:
		// Handles drawing the active window
		void renderActiveModal() {
			// Do nothing if no window is open
			if (m_ActiveModal == ActiveModal::None) return;

			bool keepOpen = true;
			// NoCollapse removes arrow, AlwaysAutoResize fits contents
			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;

			switch (m_ActiveModal) {
			case ActiveModal::ProjectSettings:
				if (ImGui::Begin("Project Settings", &keepOpen, windowFlags)) {
					ImGui::Text("Project Configurations");
				}
				ImGui::End();
				break;

			case ActiveModal::Customization:
				if (ImGui::Begin("Customization", &keepOpen, windowFlags)) {
					ImGui::Text("Customization Options");
				}
				ImGui::End();
				break;

			default:
				break;
			}

			// Reset to None when window 'X' button is clicked
			if (!keepOpen) {
				m_ActiveModal = ActiveModal::None;
			}
		}
	};
}