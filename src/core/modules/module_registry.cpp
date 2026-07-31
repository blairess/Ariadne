#include "module_registry.h"

#include "core/terrain/terrain.h"
#include "imgui.h"

#include <glad/glad.h>
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifdef APIENTRY
    #undef APIENTRY
#endif
#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace {

struct TerrainImportContext {
    Terrain* terrain = nullptr;
    bool replaced = false;
};

struct RenderHostContext {
    Terrain* terrain = nullptr;
    int colorLocation = -1;
};

std::filesystem::path executableDirectory() {
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = 0;

    while (true) {
        length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            return std::filesystem::current_path();
        }
        if (length < path.size() - 1) {
            path.resize(length);
            return std::filesystem::path(path).parent_path();
        }
        path.resize(path.size() * 2);
    }
}

std::string utf8FromPath(const std::filesystem::path& path) {
    const std::wstring widePath = path.wstring();
    if (widePath.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, widePath.data(), static_cast<int>(widePath.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, widePath.data(), static_cast<int>(widePath.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring wideFromUtf8(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

bool isModuleLibrary(const std::filesystem::path& path) {
    std::wstring name = path.filename().wstring();
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t character) {
        return static_cast<wchar_t>(std::towlower(character));
    });
    constexpr std::wstring_view suffix = L".ariadnis.dll";
    return name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isValidModuleId(std::string_view id) {
    if (id.empty()) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '.' || character == '_' || character == '-';
    });
}

std::wstring makeDialogFilter(const std::string& extension) {
    std::wstring wideExtension = wideFromUtf8(extension);
    if (wideExtension.empty() || wideExtension.front() != L'.') {
        wideExtension = L".*";
    }

    std::wstring filter;
    const std::wstring pattern = L"*" + wideExtension;
    filter += wideExtension + L" files (" + pattern + L")";
    filter.push_back(L'\0');
    filter += pattern;
    filter.push_back(L'\0');
    filter += L"All files (*.*)";
    filter.push_back(L'\0');
    filter += L"*.*";
    filter.push_back(L'\0');
    filter.push_back(L'\0');
    return filter;
}

std::optional<std::filesystem::path> chooseFilePath(const std::string& extension, bool save) {
    std::vector<wchar_t> pathBuffer(32768, L'\0');
    std::wstring filter = makeDialogFilter(extension);
    std::wstring wideExtension = wideFromUtf8(extension);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = pathBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(pathBuffer.size());
    dialog.lpstrFilter = filter.c_str();
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (save) {
        dialog.Flags |= OFN_OVERWRITEPROMPT;
        if (wideExtension.size() > 1 && wideExtension.front() == L'.') {
            dialog.lpstrDefExt = wideExtension.c_str() + 1;
        }
        if (!GetSaveFileNameW(&dialog)) {
            return std::nullopt;
        }
    } else {
        dialog.Flags |= OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameW(&dialog)) {
            return std::nullopt;
        }
    }

    return std::filesystem::path(pathBuffer.data());
}

std::optional<std::unordered_map<std::string, bool>> parseEnabledModules(const std::string& json) {
    std::unordered_map<std::string, bool> result;
    const size_t enabledKey = json.find("\"enabled\"");
    if (enabledKey == std::string::npos) {
        return std::nullopt;
    }

    const size_t objectBegin = json.find('{', enabledKey);
    if (objectBegin == std::string::npos) {
        return std::nullopt;
    }

    size_t position = objectBegin + 1;
    while (position < json.size()) {
        while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position]))) {
            ++position;
        }
        if (position >= json.size() || json[position] == '}') {
            break;
        }
        if (json[position] != '"') {
            return std::nullopt;
        }

        const size_t keyEnd = json.find('"', position + 1);
        if (keyEnd == std::string::npos) {
            return std::nullopt;
        }
        const std::string key = json.substr(position + 1, keyEnd - position - 1);
        position = keyEnd + 1;

        while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position]))) {
            ++position;
        }
        if (position >= json.size() || json[position++] != ':') {
            return std::nullopt;
        }
        while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position]))) {
            ++position;
        }

        bool enabled = false;
        if (json.compare(position, 4, "true") == 0) {
            enabled = true;
            position += 4;
        } else if (json.compare(position, 5, "false") == 0) {
            position += 5;
        } else {
            return std::nullopt;
        }

        if (!isValidModuleId(key)) {
            return std::nullopt;
        }
        result.emplace(key, enabled);

        while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position]))) {
            ++position;
        }
        if (position < json.size() && json[position] == ',') {
            ++position;
            continue;
        }
        if (position < json.size() && json[position] == '}') {
            break;
        }
        return std::nullopt;
    }

    return result;
}

} // namespace

ModuleRegistry::ModuleRegistry()
    : m_moduleDirectory(executableDirectory() / "modules"),
      m_configPath(executableDirectory() / "modules_config.json") {
    m_hostApi.abi_version = ARIADNIS_MODULE_ABI_VERSION;
    m_hostApi.struct_size = sizeof(AriadnisHostApi);
    m_hostApi.user_data = this;
    m_hostApi.log = &ModuleRegistry::hostLog;
    m_hostApi.ui_begin_window = &ModuleRegistry::hostUiBeginWindow;
    m_hostApi.ui_text = &ModuleRegistry::hostUiText;
    m_hostApi.ui_separator = &ModuleRegistry::hostUiSeparator;
    m_hostApi.ui_end_window = &ModuleRegistry::hostUiEndWindow;
}

ModuleRegistry::~ModuleRegistry() {
    unloadAll();
}

ModuleRegistry& ModuleRegistry::instance() {
    static ModuleRegistry registry;
    return registry;
}

void ModuleRegistry::loadModulesNextToExecutable() {
    loadModulesFromDirectory(m_moduleDirectory);
}

void ModuleRegistry::loadModulesFromDirectory(const std::filesystem::path& directoryPath) {
    m_moduleDirectory = directoryPath;
    std::error_code error;
    std::filesystem::create_directories(directoryPath, error);
    if (error) {
        log(ARIADNIS_LOG_ERROR, "Unable to create module directory: " + utf8FromPath(directoryPath));
        return;
    }

    std::filesystem::recursive_directory_iterator iterator(
        directoryPath,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        const auto& entry = *iterator;
        if (entry.is_regular_file(error) && !error && isModuleLibrary(entry.path())) {
            registerLibrary(entry.path());
        }
        iterator.increment(error);
    }
    if (error) {
        log(ARIADNIS_LOG_WARNING, "Could not finish scanning module directory: " + utf8FromPath(directoryPath));
    }
}

bool ModuleRegistry::registerLibrary(const std::filesystem::path& libraryPath) {
    const std::filesystem::path normalizedPath = std::filesystem::absolute(libraryPath).lexically_normal();
    const auto duplicate = std::find_if(m_modules.begin(), m_modules.end(), [&](const auto& module) {
        return module->path == normalizedPath;
    });
    if (duplicate != m_modules.end()) {
        return true;
    }

    HMODULE library = LoadLibraryExW(normalizedPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (library == nullptr) {
        log(ARIADNIS_LOG_ERROR, "Failed to load module '" + utf8FromPath(normalizedPath) + "' (Windows error " + std::to_string(GetLastError()) + ").");
        return false;
    }

    auto getApi = reinterpret_cast<AriadnisGetModuleApiFn>(GetProcAddress(library, "Ariadnis_GetModuleApi"));
    if (getApi == nullptr) {
        log(ARIADNIS_LOG_ERROR, "Module '" + utf8FromPath(normalizedPath) + "' does not export Ariadnis_GetModuleApi.");
        FreeLibrary(library);
        return false;
    }

    const AriadnisModuleApi* api = nullptr;
    try {
        api = getApi();
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + utf8FromPath(normalizedPath) + "' threw while returning its API.");
        FreeLibrary(library);
        return false;
    }

    std::string validationError;
    if (api == nullptr || !isValidModule(*api, validationError)) {
        log(ARIADNIS_LOG_ERROR, "Rejected module '" + utf8FromPath(normalizedPath) + "': " + validationError);
        FreeLibrary(library);
        return false;
    }

    const auto duplicateId = std::find_if(m_modules.begin(), m_modules.end(), [&](const auto& module) {
        return std::string_view(module->api->id) == api->id;
    });
    if (duplicateId != m_modules.end()) {
        log(ARIADNIS_LOG_ERROR, "Rejected module '" + utf8FromPath(normalizedPath) + "': duplicate module id '" + api->id + "'.");
        FreeLibrary(library);
        return false;
    }

    if (api->on_load != nullptr) {
        bool loaded = false;
        try {
            loaded = api->on_load(&m_hostApi);
        } catch (...) {
            log(ARIADNIS_LOG_ERROR, "Module '" + std::string(api->id) + "' threw during on_load.");
        }
        if (!loaded) {
            log(ARIADNIS_LOG_ERROR, "Module '" + std::string(api->id) + "' declined to load.");
            FreeLibrary(library);
            return false;
        }
    }

    auto module = std::make_unique<LoadedModule>();
    module->api = api;
    module->path = normalizedPath;
    module->libraryHandle = library;
    m_modules.push_back(std::move(module));
    log(ARIADNIS_LOG_INFO, "Loaded module '" + std::string(api->id) + "' from " + utf8FromPath(normalizedPath));
    return true;
}

bool ModuleRegistry::isValidModule(const AriadnisModuleApi& api, std::string& reason) const {
    if (api.abi_version != ARIADNIS_MODULE_ABI_VERSION) {
        reason = "uses ABI version " + std::to_string(api.abi_version) + ", but the editor requires " + std::to_string(ARIADNIS_MODULE_ABI_VERSION) + ".";
        return false;
    }
    if (api.struct_size != sizeof(AriadnisModuleApi)) {
        reason = "has an incompatible API structure size.";
        return false;
    }
    if (api.id == nullptr || api.name == nullptr || api.version == nullptr || api.description == nullptr || !isValidModuleId(api.id)) {
        reason = "is missing metadata or has an invalid id.";
        return false;
    }

    switch (api.type) {
    case ARIADNIS_MODULE_EXPORTER:
        if (api.get_supported_extensions == nullptr || api.export_terrain == nullptr) {
            reason = "exporter modules must provide extensions and export_terrain.";
            return false;
        }
        break;
    case ARIADNIS_MODULE_IMPORTER:
        if (api.get_supported_extensions == nullptr || api.import_terrain == nullptr) {
            reason = "importer modules must provide extensions and import_terrain.";
            return false;
        }
        break;
    case ARIADNIS_MODULE_BRUSH:
        if (api.get_brush_name == nullptr || api.apply_brush == nullptr) {
            reason = "brush modules must provide a name and apply_brush.";
            return false;
        }
        break;
    case ARIADNIS_MODULE_RENDERER:
        if (api.get_render_mode_name == nullptr || api.render_terrain == nullptr) {
            reason = "renderer modules must provide a name and render_terrain.";
            return false;
        }
        break;
    case ARIADNIS_MODULE_PANEL:
        if (api.render_panel == nullptr) {
            reason = "panel modules must provide render_panel.";
            return false;
        }
        break;
    default:
        reason = "has an unknown module type.";
        return false;
    }
    return true;
}

void ModuleRegistry::loadConfig(const std::filesystem::path& configPath) {
    if (!configPath.empty()) {
        m_configPath = configPath;
    }

    std::ifstream file(m_configPath, std::ios::binary);
    if (!file) {
        return;
    }

    const std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto enabledById = parseEnabledModules(contents);
    if (!enabledById) {
        log(ARIADNIS_LOG_WARNING, "Ignoring invalid module configuration: " + utf8FromPath(m_configPath));
        return;
    }

    for (const auto& module : m_modules) {
        const auto setting = enabledById->find(module->api->id);
        if (setting != enabledById->end()) {
            module->enabled = setting->second;
        }
    }
}

void ModuleRegistry::saveConfig() const {
    std::error_code error;
    std::filesystem::create_directories(m_configPath.parent_path(), error);
    if (error) {
        log(ARIADNIS_LOG_ERROR, "Unable to create the configuration directory.");
        return;
    }

    std::filesystem::path temporaryPath = m_configPath;
    temporaryPath += L".tmp";
    std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!file) {
        log(ARIADNIS_LOG_ERROR, "Unable to save module configuration.");
        return;
    }

    file << "{\n  \"schema_version\": 1,\n  \"enabled\": {\n";
    for (size_t index = 0; index < m_modules.size(); ++index) {
        const auto& module = m_modules[index];
        file << "    \"" << module->api->id << "\": " << (module->enabled ? "true" : "false");
        if (index + 1 < m_modules.size()) {
            file << ',';
        }
        file << '\n';
    }
    file << "  }\n}\n";
    file.close();

    if (!file || !MoveFileExW(temporaryPath.c_str(), m_configPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        log(ARIADNIS_LOG_ERROR, "Unable to replace module configuration (Windows error " + std::to_string(GetLastError()) + ").");
    }
}

void ModuleRegistry::initAll() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;
    for (const auto& module : m_modules) {
        if (module->enabled) {
            setModuleEnabled(*module, true);
        }
    }
}

void ModuleRegistry::shutdownAll() {
    unloadAll();
}

void ModuleRegistry::unloadAll() {
    for (auto it = m_modules.rbegin(); it != m_modules.rend(); ++it) {
        LoadedModule& module = **it;
        if (module.active && module.api->on_disable != nullptr) {
            try {
                module.api->on_disable();
            } catch (...) {
                log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' threw during on_disable.");
            }
        }
        module.active = false;

        if (module.api->on_unload != nullptr) {
            try {
                module.api->on_unload();
            } catch (...) {
                log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' threw during on_unload.");
            }
        }
        if (module.libraryHandle != nullptr) {
            FreeLibrary(static_cast<HMODULE>(module.libraryHandle));
            module.libraryHandle = nullptr;
        }
    }

    m_modules.clear();
    m_activeBrushModule = nullptr;
    m_activeRenderModule = nullptr;
    m_initialized = false;
}

bool ModuleRegistry::hasAnyModules() const {
    return !m_modules.empty();
}

const std::vector<std::unique_ptr<ModuleRegistry::LoadedModule>>& ModuleRegistry::getAllModules() const {
    return m_modules;
}

std::vector<ModuleRegistry::LoadedModule*> ModuleRegistry::getModulesByType(AriadnisModuleType type) const {
    std::vector<LoadedModule*> result;
    for (const auto& module : m_modules) {
        if (module->enabled && module->api->type == type) {
            result.push_back(module.get());
        }
    }
    return result;
}

std::vector<std::string> ModuleRegistry::getSupportedExtensions(const LoadedModule& module) const {
    std::vector<std::string> result;
    if (module.api->get_supported_extensions == nullptr) {
        return result;
    }

    size_t count = 0;
    const char* const* extensions = nullptr;
    try {
        extensions = module.api->get_supported_extensions(&count);
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' threw while listing extensions.");
        return result;
    }

    if (extensions == nullptr || count == 0 || count > 64) {
        log(ARIADNIS_LOG_WARNING, "Module '" + std::string(module.api->id) + "' returned an invalid extension list.");
        return result;
    }
    for (size_t index = 0; index < count; ++index) {
        if (extensions[index] == nullptr) {
            continue;
        }
        std::string extension = extensions[index];
        const bool valid = extension.size() > 1 && extension[0] == '.' && std::all_of(extension.begin() + 1, extension.end(), [](unsigned char character) {
            return std::isalnum(character) || character == '_' || character == '-';
        });
        if (valid && std::find(result.begin(), result.end(), extension) == result.end()) {
            result.push_back(std::move(extension));
        }
    }
    return result;
}

bool ModuleRegistry::setModuleEnabled(LoadedModule& module, bool enabled) {
    if (module.enabled == enabled && (!m_initialized || module.active == enabled)) {
        return true;
    }

    if (!m_initialized) {
        module.enabled = enabled;
        return true;
    }

    if (enabled) {
        if (module.api->on_enable != nullptr) {
            bool initialized = false;
            try {
                initialized = module.api->on_enable();
            } catch (...) {
                log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' threw during on_enable.");
            }
            if (!initialized) {
                log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' declined to enable.");
                module.enabled = false;
                module.active = false;
                return false;
            }
        }
        module.enabled = true;
        module.active = true;
        if (module.api->type == ARIADNIS_MODULE_PANEL) {
            module.panelVisible = true;
        }
        return true;
    }

    if (module.active && module.api->on_disable != nullptr) {
        try {
            module.api->on_disable();
        } catch (...) {
            log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' threw during on_disable.");
        }
    }
    module.enabled = false;
    module.active = false;
    if (m_activeBrushModule == &module) {
        m_activeBrushModule = nullptr;
    }
    if (m_activeRenderModule == &module) {
        m_activeRenderModule = nullptr;
    }
    return true;
}

bool ModuleRegistry::exportTerrain(LoadedModule& module, const Terrain& terrain, const std::string& extension) {
    if (!module.enabled || module.api->type != ARIADNIS_MODULE_EXPORTER) {
        return false;
    }
    const auto path = chooseFilePath(extension, true);
    if (!path) {
        return false;
    }

    const AriadnisTerrainView view{
        terrain.width, terrain.depth, terrain.cellSize,
        terrain.vertices.data(), terrain.vertices.size(),
        terrain.indices.data(), terrain.indices.size()
    };
    const std::string utf8Path = utf8FromPath(*path);
    try {
        if (module.api->export_terrain(&view, utf8Path.c_str())) {
            log(ARIADNIS_LOG_INFO, "Exported terrain through '" + std::string(module.api->id) + "' to " + utf8Path);
            return true;
        }
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' threw while exporting terrain.");
        return false;
    }
    log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' failed to export terrain.");
    return false;
}

bool ModuleRegistry::importTerrain(LoadedModule& module, Terrain& terrain, const std::string& extension) {
    if (!module.enabled || module.api->type != ARIADNIS_MODULE_IMPORTER) {
        return false;
    }
    const auto path = chooseFilePath(extension, false);
    if (!path) {
        return false;
    }

    TerrainImportContext context{ &terrain, false };
    const AriadnisTerrainSink sink{ &context, &ModuleRegistry::replaceTerrainFromModule };
    const std::string utf8Path = utf8FromPath(*path);
    try {
        if (module.api->import_terrain(utf8Path.c_str(), &sink) && context.replaced) {
            log(ARIADNIS_LOG_INFO, "Imported terrain through '" + std::string(module.api->id) + "' from " + utf8Path);
            return true;
        }
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' threw while importing terrain.");
        return false;
    }
    log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module.api->id) + "' failed to import terrain data.");
    return false;
}

void ModuleRegistry::renderPanels() {
    for (const auto& module : m_modules) {
        if (!module->enabled || module->api->type != ARIADNIS_MODULE_PANEL || !module->panelVisible) {
            continue;
        }
        try {
            module->api->render_panel(&module->panelVisible);
        } catch (...) {
            log(ARIADNIS_LOG_ERROR, "Module '" + std::string(module->api->id) + "' threw while rendering its panel.");
        }
    }
}

ModuleRegistry::LoadedModule* ModuleRegistry::getActiveBrushModule() const {
    return m_activeBrushModule;
}

void ModuleRegistry::setActiveBrushModule(LoadedModule* module) {
    if (module != nullptr && (!module->enabled || module->api->type != ARIADNIS_MODULE_BRUSH)) {
        return;
    }
    m_activeBrushModule = module;
}

float ModuleRegistry::getActiveBrushRadius() const {
    if (m_activeBrushModule == nullptr || m_activeBrushModule->api->get_brush_radius == nullptr) {
        return 0.3f;
    }
    try {
        return std::max(0.001f, m_activeBrushModule->api->get_brush_radius());
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + std::string(m_activeBrushModule->api->id) + "' threw while reading its brush radius.");
        return 0.3f;
    }
}

float ModuleRegistry::getActiveBrushStrength() const {
    if (m_activeBrushModule == nullptr || m_activeBrushModule->api->get_brush_strength == nullptr) {
        return 2.0f;
    }
    try {
        return std::max(0.0f, m_activeBrushModule->api->get_brush_strength());
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + std::string(m_activeBrushModule->api->id) + "' threw while reading its brush strength.");
        return 2.0f;
    }
}

bool ModuleRegistry::applyActiveBrush(Terrain& terrain, float hitX, float hitY, float hitZ, float deltaTime, bool invert) {
    if (m_activeBrushModule == nullptr || !m_activeBrushModule->enabled) {
        return false;
    }

    AriadnisMutableTerrainView view{
        terrain.width, terrain.depth, terrain.cellSize,
        terrain.vertices.data(), terrain.vertices.size(),
        terrain.indices.data(), terrain.indices.size()
    };
    const AriadnisBrushInput input{
        hitX, hitY, hitZ, deltaTime,
        getActiveBrushRadius(), getActiveBrushStrength(), invert
    };
    try {
        if (m_activeBrushModule->api->apply_brush(&view, &input)) {
            terrain.updateBuffers();
            return true;
        }
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + std::string(m_activeBrushModule->api->id) + "' threw while applying its brush.");
        setActiveBrushModule(nullptr);
    }
    return false;
}

ModuleRegistry::LoadedModule* ModuleRegistry::getActiveRenderModule() const {
    return m_activeRenderModule;
}

void ModuleRegistry::setActiveRenderModule(LoadedModule* module) {
    if (module != nullptr && (!module->enabled || module->api->type != ARIADNIS_MODULE_RENDERER)) {
        return;
    }
    m_activeRenderModule = module;
}

bool ModuleRegistry::renderActiveModule(Terrain& terrain, int colorLocation) {
    if (m_activeRenderModule == nullptr || !m_activeRenderModule->enabled) {
        return false;
    }

    const AriadnisTerrainView view{
        terrain.width, terrain.depth, terrain.cellSize,
        terrain.vertices.data(), terrain.vertices.size(),
        terrain.indices.data(), terrain.indices.size()
    };
    RenderHostContext hostContext{ &terrain, colorLocation };
    const AriadnisRenderContext context{
        &hostContext,
        &ModuleRegistry::renderSetPolygonMode,
        &ModuleRegistry::renderSetColor,
        &ModuleRegistry::renderSetLineWidth,
        &ModuleRegistry::renderDrawTerrain
    };

    bool rendered = false;
    try {
        rendered = m_activeRenderModule->api->render_terrain(&view, &context);
    } catch (...) {
        log(ARIADNIS_LOG_ERROR, "Module '" + std::string(m_activeRenderModule->api->id) + "' threw while rendering.");
        setActiveRenderModule(nullptr);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
    return rendered;
}

const std::filesystem::path& ModuleRegistry::getModuleDirectory() const {
    return m_moduleDirectory;
}

void ModuleRegistry::log(AriadnisLogLevel level, const std::string& message) const {
    const char* prefix = "INFO";
    if (level == ARIADNIS_LOG_WARNING) prefix = "WARNING";
    if (level == ARIADNIS_LOG_ERROR) prefix = "ERROR";
    if (level == ARIADNIS_LOG_DEBUG) prefix = "DEBUG";
    std::cerr << "[Modules/" << prefix << "] " << message << std::endl;
}

void ModuleRegistry::hostLog(void* userData, AriadnisLogLevel level, const char* message) {
    if (userData != nullptr) {
        static_cast<ModuleRegistry*>(userData)->log(level, message == nullptr ? "(null message)" : message);
    }
}

bool ModuleRegistry::hostUiBeginWindow(void*, const char* title, bool* visible) {
    if (title == nullptr || visible == nullptr) {
        return false;
    }
    return ImGui::Begin(title, visible);
}

void ModuleRegistry::hostUiText(void*, const char* text) {
    ImGui::TextUnformatted(text == nullptr ? "" : text);
}

void ModuleRegistry::hostUiSeparator(void*) {
    ImGui::Separator();
}

void ModuleRegistry::hostUiEndWindow(void*) {
    ImGui::End();
}

bool ModuleRegistry::replaceTerrainFromModule(void* userData, const AriadnisTerrainData* data) {
    auto* context = static_cast<TerrainImportContext*>(userData);
    if (context == nullptr || context->terrain == nullptr || data == nullptr || data->width < 2 || data->depth < 2 || data->cell_size <= 0.0f || data->vertices == nullptr || data->indices == nullptr) {
        return false;
    }

    const size_t expectedVertexCount = static_cast<size_t>(data->width) * static_cast<size_t>(data->depth) * 3;
    if (data->vertex_count != expectedVertexCount || data->index_count == 0 || data->index_count % 3 != 0) {
        return false;
    }
    const size_t vertexCount = static_cast<size_t>(data->width) * static_cast<size_t>(data->depth);
    for (size_t index = 0; index < data->index_count; ++index) {
        if (data->indices[index] < 0 || static_cast<size_t>(data->indices[index]) >= vertexCount) {
            return false;
        }
    }

    context->terrain->replaceMesh(data->width, data->depth, data->cell_size,
        data->vertices, data->vertex_count, data->indices, data->index_count);
    context->replaced = true;
    return true;
}

void ModuleRegistry::renderSetPolygonMode(void*, AriadnisPolygonMode mode) {
    glPolygonMode(GL_FRONT_AND_BACK, mode == ARIADNIS_POLYGON_LINE ? GL_LINE : GL_FILL);
}

void ModuleRegistry::renderSetColor(void* userData, float red, float green, float blue, float alpha) {
    const auto* context = static_cast<RenderHostContext*>(userData);
    if (context != nullptr && context->colorLocation >= 0) {
        glUniform4f(context->colorLocation, red, green, blue, alpha);
    }
}

void ModuleRegistry::renderSetLineWidth(void*, float width) {
    glLineWidth(std::max(1.0f, width));
}

void ModuleRegistry::renderDrawTerrain(void* userData) {
    const auto* context = static_cast<RenderHostContext*>(userData);
    if (context != nullptr && context->terrain != nullptr) {
        context->terrain->draw();
    }
}
