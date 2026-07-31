#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "../../core/history/history_manager.h"
#include "../../core/modules/module_registry.h"

#include <string>

class Terrain;

namespace UI {
class TopBar {
public:
    void render(GLFWwindow* window, HistoryManager& history, Terrain& terrain) {
        if (!ImGui::BeginMainMenuBar()) {
            return;
        }

        auto& registry = ModuleRegistry::instance();

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project")) {
                // TODO: project persistence is outside the module system.
            }
            if (ImGui::MenuItem("Open Project")) {
                // TODO: project persistence is outside the module system.
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save")) {
                // TODO: project persistence is outside the module system.
            }
            if (ImGui::MenuItem("Save As...")) {
                // TODO: project persistence is outside the module system.
            }
            ImGui::Separator();

            const auto importModules = registry.getModulesByType(ARIADNIS_MODULE_IMPORTER);
            if (ImGui::BeginMenu("Import...", !importModules.empty())) {
                for (auto* module : importModules) {
                    for (const auto& extension : registry.getSupportedExtensions(*module)) {
                        const std::string label = std::string(module->api->name) + " (" + extension + ")##" + module->api->id + extension;
                        if (ImGui::MenuItem(label.c_str())) {
                            if (registry.importTerrain(*module, terrain, extension)) {
                                history.clear();
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }

            const auto exportModules = registry.getModulesByType(ARIADNIS_MODULE_EXPORTER);
            if (ImGui::BeginMenu("Export...", !exportModules.empty())) {
                for (auto* module : exportModules) {
                    for (const auto& extension : registry.getSupportedExtensions(*module)) {
                        const std::string label = std::string(module->api->name) + " (" + extension + ")##" + module->api->id + extension;
                        if (ImGui::MenuItem(label.c_str())) {
                            registry.exportTerrain(*module, terrain, extension);
                        }
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();
            ImGui::MenuItem("Project Settings");
            ImGui::MenuItem("Customization");
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Esc")) {
                glfwSetWindowShouldClose(window, true);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "CTRL + Z", false, history.canUndo())) {
                history.undo();
            }
            if (ImGui::MenuItem("Redo", "CTRL + Y", false, history.canRedo())) {
                history.redo();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Edit Mode");
            ImGui::MenuItem("Painting Mode");
            ImGui::MenuItem("Spectator Mode");
            ImGui::Separator();
            if (ImGui::BeginMenu("Render Mode")) {
                if (ImGui::MenuItem("Built-in renderer", nullptr, registry.getActiveRenderModule() == nullptr)) {
                    registry.setActiveRenderModule(nullptr);
                }
                for (auto* module : registry.getModulesByType(ARIADNIS_MODULE_RENDERER)) {
                    const std::string label = std::string(module->api->name) + "##" + module->api->id;
                    if (ImGui::MenuItem(label.c_str(), nullptr, registry.getActiveRenderModule() == module)) {
                        registry.setActiveRenderModule(module);
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            const auto brushModules = registry.getModulesByType(ARIADNIS_MODULE_BRUSH);
            if (brushModules.empty()) {
                ImGui::TextDisabled("No brush modules loaded");
            }
            for (auto* module : brushModules) {
                const std::string label = std::string(module->api->name) + "##" + module->api->id;
                if (ImGui::MenuItem(label.c_str(), nullptr, registry.getActiveBrushModule() == module)) {
                    registry.setActiveBrushModule(module);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Controls");
            ImGui::MenuItem("Support");
            ImGui::MenuItem("Community");
            ImGui::Separator();
            ImGui::MenuItem("Bug Report");
            ImGui::EndMenu();
        }

        if (registry.hasAnyModules() && ImGui::BeginMenu("Modules")) {
            for (const auto& module : registry.getAllModules()) {
                bool isEnabled = module->enabled;
                const std::string label = std::string(module->api->name) + "##" + module->api->id;
                if (ImGui::MenuItem(label.c_str(), nullptr, &isEnabled)) {
                    registry.setModuleEnabled(*module, isEnabled);
                    registry.saveConfig();
                }
                if (ImGui::IsItemHovered()) {
                    const std::string tooltip = "v" + std::string(module->api->version) + " - " + module->api->description;
                    ImGui::SetTooltip("%s", tooltip.c_str());
                }
            }
            ImGui::Separator();
            ImGui::TextDisabled("Module settings are saved automatically");
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
};
} // namespace UI
