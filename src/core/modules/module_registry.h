#pragma once

#include <ariadnis/module_api.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class Terrain;

class ModuleRegistry {
public:
    struct LoadedModule {
        const AriadnisModuleApi* api = nullptr;
        std::filesystem::path path;
        void* libraryHandle = nullptr;
        bool enabled = true;
        bool active = false;
        bool panelVisible = true;
    };

    static ModuleRegistry& instance();

    void loadModulesFromDirectory(const std::filesystem::path& directoryPath);
    void loadModulesNextToExecutable();
    void loadConfig(const std::filesystem::path& configPath = {});
    void saveConfig() const;

    void initAll();
    void shutdownAll();

    bool hasAnyModules() const;
    const std::vector<std::unique_ptr<LoadedModule>>& getAllModules() const;
    std::vector<LoadedModule*> getModulesByType(AriadnisModuleType type) const;
    std::vector<std::string> getSupportedExtensions(const LoadedModule& module) const;

    bool setModuleEnabled(LoadedModule& module, bool enabled);

    bool exportTerrain(LoadedModule& module, const Terrain& terrain, const std::string& extension);
    bool importTerrain(LoadedModule& module, Terrain& terrain, const std::string& extension);
    void renderPanels();

    LoadedModule* getActiveBrushModule() const;
    void setActiveBrushModule(LoadedModule* module);
    float getActiveBrushRadius() const;
    float getActiveBrushStrength() const;
    bool applyActiveBrush(Terrain& terrain, float hitX, float hitY, float hitZ, float deltaTime, bool invert);

    LoadedModule* getActiveRenderModule() const;
    void setActiveRenderModule(LoadedModule* module);
    bool renderActiveModule(Terrain& terrain, int colorLocation);

    const std::filesystem::path& getModuleDirectory() const;

private:
    ModuleRegistry();
    ~ModuleRegistry();
    ModuleRegistry(const ModuleRegistry&) = delete;
    ModuleRegistry& operator=(const ModuleRegistry&) = delete;

    bool registerLibrary(const std::filesystem::path& libraryPath);
    bool isValidModule(const AriadnisModuleApi& api, std::string& reason) const;
    void log(AriadnisLogLevel level, const std::string& message) const;
    void unloadAll();

    static void hostLog(void* userData, AriadnisLogLevel level, const char* message);
    static bool hostUiBeginWindow(void* userData, const char* title, bool* visible);
    static void hostUiText(void* userData, const char* text);
    static void hostUiSeparator(void* userData);
    static void hostUiEndWindow(void* userData);
    static bool replaceTerrainFromModule(void* userData, const AriadnisTerrainData* data);
    static void renderSetPolygonMode(void* userData, AriadnisPolygonMode mode);
    static void renderSetColor(void* userData, float red, float green, float blue, float alpha);
    static void renderSetLineWidth(void* userData, float width);
    static void renderDrawTerrain(void* userData);

    std::vector<std::unique_ptr<LoadedModule>> m_modules;
    AriadnisHostApi m_hostApi{};
    std::filesystem::path m_moduleDirectory;
    std::filesystem::path m_configPath;
    LoadedModule* m_activeBrushModule = nullptr;
    LoadedModule* m_activeRenderModule = nullptr;
    bool m_initialized = false;
};
