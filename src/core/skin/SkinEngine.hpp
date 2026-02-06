#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <filesystem>
#include <functional>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace konami::skin {

// Skin model type
enum class SkinModel {
    Classic,    // Steve model (4px arms)
    Slim        // Alex model (3px arms)
};

// Cape type
enum class CapeType {
    None,
    Minecraft,
    Optifine,
    MinecraftCapes,
    Custom
};

// Skin layer visibility
struct SkinLayers {
    bool hat = true;
    bool jacket = true;
    bool leftSleeve = true;
    bool rightSleeve = true;
    bool leftPants = true;
    bool rightPants = true;
    
    nlohmann::json toJson() const;
    static SkinLayers fromJson(const nlohmann::json& j);
};

// Skin metadata
struct SkinInfo {
    std::string id;
    std::string name;
    std::string filePath;
    std::string url;
    std::string sha256Hash;
    
    SkinModel model = SkinModel::Classic;
    SkinLayers layers;
    
    int width = 64;
    int height = 64;
    bool isSlim = false;
    bool isHD = false;
    
    std::chrono::system_clock::time_point addedAt;
    std::string source; // local, minecraft.net, ely.by, namemc
    
    nlohmann::json toJson() const;
    static SkinInfo fromJson(const nlohmann::json& j);
};

// Cape metadata
struct CapeInfo {
    std::string id;
    std::string name;
    std::string filePath;
    std::string url;
    
    CapeType type = CapeType::None;
    int width = 64;
    int height = 32;
    bool animated = false;
    
    nlohmann::json toJson() const;
    static CapeInfo fromJson(const nlohmann::json& j);
};

// Skin animation frame
struct AnimationFrame {
    float duration = 1.0f;
    float rotation = 0.0f;
    float armSwing = 0.0f;
    float legSwing = 0.0f;
    bool running = false;
    bool sneaking = false;
};

// Animation preset
struct AnimationPreset {
    std::string name;
    std::vector<AnimationFrame> frames;
    bool loop = true;
    float speed = 1.0f;
};

// RGBA pixel
struct Pixel {
    uint8_t r, g, b, a;
};

// Image buffer
class ImageBuffer {
public:
    ImageBuffer(int width, int height);
    ~ImageBuffer();
    
    Pixel getPixel(int x, int y) const;
    void setPixel(int x, int y, const Pixel& pixel);
    
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    
    std::vector<uint8_t> toRGBA() const;
    bool loadFromFile(const std::filesystem::path& path);
    bool saveToFile(const std::filesystem::path& path) const;
    
    void clear(const Pixel& color = {0, 0, 0, 0});
    void flipVertically();
    void flipHorizontally();
    
private:
    int m_width;
    int m_height;
    std::vector<Pixel> m_pixels;
};

// 3D skin renderer interface for Slint
class SkinRenderer {
public:
    SkinRenderer();
    ~SkinRenderer();
    
    // Skin loading
    bool loadSkin(const SkinInfo& skin);
    bool loadSkinFromFile(const std::filesystem::path& path);
    bool loadSkinFromUrl(const std::string& url);
    bool loadSkinFromBuffer(const ImageBuffer& buffer, SkinModel model);
    
    // Cape loading
    bool loadCape(const CapeInfo& cape);
    bool loadCapeFromFile(const std::filesystem::path& path);
    
    // Rendering
    void setRotation(float yaw, float pitch);
    void setZoom(float zoom);
    void setAnimation(const AnimationPreset& animation);
    void setAnimationTime(float time);
    void setLayers(const SkinLayers& layers);
    
    // Export
    bool exportToImage(const std::filesystem::path& path, int width, int height);
    std::vector<uint8_t> renderToBuffer(int width, int height);
    
    // Animation control
    void play();
    void pause();
    void stop();
    bool isPlaying() const;
    
    // Preset animations
    static AnimationPreset getIdleAnimation();
    static AnimationPreset getWalkAnimation();
    static AnimationPreset getRunAnimation();
    static AnimationPreset getWaveAnimation();
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Skin Editor
class SkinEditor {
public:
    SkinEditor();
    ~SkinEditor();
    
    // Load/Save
    bool loadSkin(const std::filesystem::path& path);
    bool saveSkin(const std::filesystem::path& path);
    bool createNew(SkinModel model, int width = 64, int height = 64);
    
    // Basic editing
    Pixel getPixel(int x, int y) const;
    void setPixel(int x, int y, const Pixel& color);
    void fill(int x, int y, const Pixel& color);
    void drawLine(int x1, int y1, int x2, int y2, const Pixel& color);
    void drawRect(int x, int y, int w, int h, const Pixel& color, bool filled = false);
    
    // Advanced editing
    void copyRegion(int srcX, int srcY, int destX, int destY, int w, int h);
    void mirrorRegion(int x, int y, int w, int h, bool horizontal);
    void rotateRegion(int x, int y, int w, int h, int degrees);
    
    // Color operations
    void adjustBrightness(float factor);
    void adjustContrast(float factor);
    void adjustSaturation(float factor);
    void replaceColor(const Pixel& oldColor, const Pixel& newColor, int tolerance = 0);
    
    // Layers (for HD skins)
    void setActiveLayer(int layer);
    int getActiveLayer() const;
    void mergeLayerDown(int layer);
    void mergeFlatten();
    
    // Templates
    void applyTemplate(const std::string& templateName);
    std::vector<std::string> getAvailableTemplates() const;
    
    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;
    void clearHistory();
    
    // Model
    void setModel(SkinModel model);
    SkinModel getModel() const;
    
    // Buffer access
    const ImageBuffer& getBuffer() const;
    ImageBuffer& getBuffer();
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Main Skin Manager
class SkinManager {
public:
    SkinManager();
    ~SkinManager();
    
    // Initialization
    bool initialize(const std::filesystem::path& skinsDirectory);
    void shutdown();
    
    // Skin management
    bool addSkin(const std::filesystem::path& skinPath, const std::string& name = "");
    bool removeSkin(const std::string& skinId);
    bool renameSkin(const std::string& skinId, const std::string& newName);
    
    // Skin retrieval
    std::vector<SkinInfo> getAllSkins() const;
    std::optional<SkinInfo> getSkin(const std::string& skinId) const;
    std::optional<SkinInfo> getActiveSkin() const;
    
    // Active skin
    bool setActiveSkin(const std::string& skinId);
    std::string getActiveSkinId() const;
    
    // Online skin sources
    std::future<std::optional<SkinInfo>> fetchFromMinecraft(const std::string& uuid);
    std::future<std::optional<SkinInfo>> fetchFromElyBy(const std::string& username);
    std::future<std::optional<SkinInfo>> fetchFromNameMC(const std::string& username);
    std::future<std::optional<SkinInfo>> fetchFromUrl(const std::string& url);
    
    // Upload to services
    std::future<bool> uploadToMinecraft(const std::string& skinId, const std::string& accessToken);
    std::future<bool> uploadToElyBy(const std::string& skinId, const std::string& accessToken);
    
    // Cape management
    bool addCape(const std::filesystem::path& capePath, const std::string& name = "");
    bool removeCape(const std::string& capeId);
    std::vector<CapeInfo> getAllCapes() const;
    std::optional<CapeInfo> getActiveCape() const;
    bool setActiveCape(const std::string& capeId);
    
    // Renderer access
    SkinRenderer& getRenderer();
    SkinEditor& getEditor();
    
    // Events
    void setOnSkinChanged(std::function<void(const SkinInfo&)> callback);
    void setOnCapeChanged(std::function<void(const CapeInfo&)> callback);
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

// Utility functions
SkinModel detectSkinModel(const ImageBuffer& buffer);
bool validateSkinDimensions(int width, int height);
std::string skinModelToString(SkinModel model);
SkinModel stringToSkinModel(const std::string& str);

} // namespace konami::skin
