/**
 * SkinEngine.cpp
 * 
 * Skin rendering, editing, and management.
 */

#include "SkinEngine.hpp"
#include "../Logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <fstream>
#include <algorithm>
#include <cstring>

namespace konami::skin {

// -- SkinLayers --

nlohmann::json SkinLayers::toJson() const {
    return {
        {"hat", hat}, {"jacket", jacket},
        {"leftSleeve", leftSleeve}, {"rightSleeve", rightSleeve},
        {"leftPants", leftPants}, {"rightPants", rightPants}
    };
}

SkinLayers SkinLayers::fromJson(const nlohmann::json& j) {
    SkinLayers l;
    l.hat = j.value("hat", true);
    l.jacket = j.value("jacket", true);
    l.leftSleeve = j.value("leftSleeve", true);
    l.rightSleeve = j.value("rightSleeve", true);
    l.leftPants = j.value("leftPants", true);
    l.rightPants = j.value("rightPants", true);
    return l;
}

// -- SkinInfo --

nlohmann::json SkinInfo::toJson() const {
    return {
        {"id", id}, {"name", name}, {"filePath", filePath}, {"url", url},
        {"sha256Hash", sha256Hash}, {"model", model == SkinModel::Slim ? "slim" : "classic"},
        {"layers", layers.toJson()}, {"width", width}, {"height", height},
        {"isSlim", isSlim}, {"isHD", isHD}, {"source", source}
    };
}

SkinInfo SkinInfo::fromJson(const nlohmann::json& j) {
    SkinInfo s;
    s.id = j.value("id", "");
    s.name = j.value("name", "");
    s.filePath = j.value("filePath", "");
    s.url = j.value("url", "");
    s.sha256Hash = j.value("sha256Hash", "");
    s.model = j.value("model", "classic") == "slim" ? SkinModel::Slim : SkinModel::Classic;
    if (j.contains("layers")) s.layers = SkinLayers::fromJson(j["layers"]);
    s.width = j.value("width", 64);
    s.height = j.value("height", 64);
    s.isSlim = j.value("isSlim", false);
    s.isHD = j.value("isHD", false);
    s.source = j.value("source", "local");
    return s;
}

// -- CapeInfo --

nlohmann::json CapeInfo::toJson() const {
    return {
        {"id", id}, {"name", name}, {"filePath", filePath}, {"url", url},
        {"type", static_cast<int>(type)}, {"width", width}, {"height", height},
        {"animated", animated}
    };
}

CapeInfo CapeInfo::fromJson(const nlohmann::json& j) {
    CapeInfo c;
    c.id = j.value("id", "");
    c.name = j.value("name", "");
    c.filePath = j.value("filePath", "");
    c.url = j.value("url", "");
    c.type = static_cast<CapeType>(j.value("type", 0));
    c.width = j.value("width", 64);
    c.height = j.value("height", 32);
    c.animated = j.value("animated", false);
    return c;
}

// -- ImageBuffer --

ImageBuffer::ImageBuffer(int width, int height)
    : m_width(width), m_height(height), m_pixels(width * height) {}

ImageBuffer::~ImageBuffer() = default;

Pixel ImageBuffer::getPixel(int x, int y) const {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return {0,0,0,0};
    return m_pixels[y * m_width + x];
}

void ImageBuffer::setPixel(int x, int y, const Pixel& pixel) {
    if (x < 0 || x >= m_width || y < 0 || y >= m_height) return;
    m_pixels[y * m_width + x] = pixel;
}

std::vector<uint8_t> ImageBuffer::toRGBA() const {
    std::vector<uint8_t> data(m_width * m_height * 4);
    for (int i = 0; i < m_width * m_height; ++i) {
        data[i*4+0] = m_pixels[i].r;
        data[i*4+1] = m_pixels[i].g;
        data[i*4+2] = m_pixels[i].b;
        data[i*4+3] = m_pixels[i].a;
    }
    return data;
}

bool ImageBuffer::loadFromFile(const std::filesystem::path& path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
    if (!data) return false;

    m_width = w;
    m_height = h;
    m_pixels.resize(w * h);

    for (int i = 0; i < w * h; ++i) {
        m_pixels[i] = {data[i*4], data[i*4+1], data[i*4+2], data[i*4+3]};
    }

    stbi_image_free(data);
    return true;
}

bool ImageBuffer::saveToFile(const std::filesystem::path& path) const {
    auto rgba = toRGBA();
    return stbi_write_png(path.string().c_str(), m_width, m_height, 4, rgba.data(), m_width * 4) != 0;
}

void ImageBuffer::clear(const Pixel& color) {
    std::fill(m_pixels.begin(), m_pixels.end(), color);
}

void ImageBuffer::flipVertically() {
    for (int y = 0; y < m_height / 2; ++y) {
        for (int x = 0; x < m_width; ++x) {
            std::swap(m_pixels[y * m_width + x], m_pixels[(m_height - 1 - y) * m_width + x]);
        }
    }
}

void ImageBuffer::flipHorizontally() {
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width / 2; ++x) {
            std::swap(m_pixels[y * m_width + x], m_pixels[y * m_width + (m_width - 1 - x)]);
        }
    }
}

// -- SkinRenderer (stub) --

class SkinRenderer::Impl {
public:
    float yaw = 0.0f;
    float pitch = 0.0f;
    float zoom = 1.0f;
    bool playing = false;
    SkinLayers layers;
    ImageBuffer skinBuffer{64, 64};
};

SkinRenderer::SkinRenderer() : m_impl(std::make_unique<Impl>()) {}
SkinRenderer::~SkinRenderer() = default;

bool SkinRenderer::loadSkin(const SkinInfo& skin) {
    return m_impl->skinBuffer.loadFromFile(skin.filePath);
}
bool SkinRenderer::loadSkinFromFile(const std::filesystem::path& path) {
    return m_impl->skinBuffer.loadFromFile(path);
}
bool SkinRenderer::loadSkinFromUrl(const std::string& /*url*/) { return false; /* TODO */ }
bool SkinRenderer::loadSkinFromBuffer(const ImageBuffer& buffer, SkinModel /*model*/) {
    m_impl->skinBuffer = buffer;
    return true;
}
bool SkinRenderer::loadCape(const CapeInfo& /*cape*/) { return false; /* TODO */ }
bool SkinRenderer::loadCapeFromFile(const std::filesystem::path& /*path*/) { return false; /* TODO */ }

void SkinRenderer::setRotation(float yaw, float pitch) { m_impl->yaw = yaw; m_impl->pitch = pitch; }
void SkinRenderer::setZoom(float zoom) { m_impl->zoom = zoom; }
void SkinRenderer::setAnimation(const AnimationPreset& /*animation*/) {}
void SkinRenderer::setAnimationTime(float /*time*/) {}
void SkinRenderer::setLayers(const SkinLayers& layers) { m_impl->layers = layers; }

bool SkinRenderer::exportToImage(const std::filesystem::path& path, int width, int height) {
    ImageBuffer buf(width, height);
    buf.clear({128, 128, 128, 255});
    return buf.saveToFile(path);
}

std::vector<uint8_t> SkinRenderer::renderToBuffer(int width, int height) {
    ImageBuffer buf(width, height);
    buf.clear({128, 128, 128, 255});
    return buf.toRGBA();
}

void SkinRenderer::play() { m_impl->playing = true; }
void SkinRenderer::pause() { m_impl->playing = false; }
void SkinRenderer::stop() { m_impl->playing = false; }
bool SkinRenderer::isPlaying() const { return m_impl->playing; }

AnimationPreset SkinRenderer::getIdleAnimation() { return {"idle", {}, true, 1.0f}; }
AnimationPreset SkinRenderer::getWalkAnimation() { return {"walk", {}, true, 1.0f}; }
AnimationPreset SkinRenderer::getRunAnimation() { return {"run", {}, true, 1.5f}; }
AnimationPreset SkinRenderer::getWaveAnimation() { return {"wave", {}, false, 1.0f}; }

// -- SkinEditor (stub) --

class SkinEditor::Impl {
public:
    ImageBuffer buffer{64, 64};
    SkinModel model = SkinModel::Classic;
    std::vector<ImageBuffer> undoStack;
    std::vector<ImageBuffer> redoStack;
    int activeLayer = 0;
};

SkinEditor::SkinEditor() : m_impl(std::make_unique<Impl>()) {}
SkinEditor::~SkinEditor() = default;

bool SkinEditor::loadSkin(const std::filesystem::path& path) { return m_impl->buffer.loadFromFile(path); }
bool SkinEditor::saveSkin(const std::filesystem::path& path) { return m_impl->buffer.saveToFile(path); }
bool SkinEditor::createNew(SkinModel model, int width, int height) {
    m_impl->buffer = ImageBuffer(width, height);
    m_impl->buffer.clear({0,0,0,0});
    m_impl->model = model;
    return true;
}

Pixel SkinEditor::getPixel(int x, int y) const { return m_impl->buffer.getPixel(x, y); }
void SkinEditor::setPixel(int x, int y, const Pixel& color) { m_impl->buffer.setPixel(x, y, color); }
void SkinEditor::fill(int /*x*/, int /*y*/, const Pixel& /*color*/) { /* TODO flood fill */ }
void SkinEditor::drawLine(int /*x1*/, int /*y1*/, int /*x2*/, int /*y2*/, const Pixel& /*color*/) { /* TODO */ }
void SkinEditor::drawRect(int x, int y, int w, int h, const Pixel& color, bool filled) {
    if (filled) {
        for (int dy = 0; dy < h; ++dy)
            for (int dx = 0; dx < w; ++dx)
                m_impl->buffer.setPixel(x + dx, y + dy, color);
    }
}
void SkinEditor::copyRegion(int /*srcX*/, int /*srcY*/, int /*destX*/, int /*destY*/, int /*w*/, int /*h*/) {}
void SkinEditor::mirrorRegion(int /*x*/, int /*y*/, int /*w*/, int /*h*/, bool /*horizontal*/) {}
void SkinEditor::rotateRegion(int /*x*/, int /*y*/, int /*w*/, int /*h*/, int /*degrees*/) {}
void SkinEditor::adjustBrightness(float /*factor*/) {}
void SkinEditor::adjustContrast(float /*factor*/) {}
void SkinEditor::adjustSaturation(float /*factor*/) {}
void SkinEditor::replaceColor(const Pixel& /*oldColor*/, const Pixel& /*newColor*/, int /*tolerance*/) {}
void SkinEditor::setActiveLayer(int layer) { m_impl->activeLayer = layer; }
int SkinEditor::getActiveLayer() const { return m_impl->activeLayer; }
void SkinEditor::mergeLayerDown(int /*layer*/) {}
void SkinEditor::mergeFlatten() {}
void SkinEditor::applyTemplate(const std::string& /*templateName*/) {}
std::vector<std::string> SkinEditor::getAvailableTemplates() const { return {}; }
void SkinEditor::undo() {}
void SkinEditor::redo() {}
bool SkinEditor::canUndo() const { return false; }
bool SkinEditor::canRedo() const { return false; }
void SkinEditor::clearHistory() {}
void SkinEditor::setModel(SkinModel model) { m_impl->model = model; }
SkinModel SkinEditor::getModel() const { return m_impl->model; }
const ImageBuffer& SkinEditor::getBuffer() const { return m_impl->buffer; }
ImageBuffer& SkinEditor::getBuffer() { return m_impl->buffer; }

// -- SkinManager --

class SkinManager::Impl {
public:
    std::filesystem::path skinsDir;
    std::vector<SkinInfo> skins;
    std::vector<CapeInfo> capes;
    std::string activeSkinId;
    std::string activeCapeId;
    SkinRenderer renderer;
    SkinEditor editor;
    std::function<void(const SkinInfo&)> onSkinChanged;
    std::function<void(const CapeInfo&)> onCapeChanged;
};

SkinManager::SkinManager() : m_impl(std::make_unique<Impl>()) {}
SkinManager::~SkinManager() = default;

bool SkinManager::initialize(const std::filesystem::path& skinsDirectory) {
    m_impl->skinsDir = skinsDirectory;
    std::filesystem::create_directories(skinsDirectory);
    // Scan existing skins
    for (const auto& entry : std::filesystem::directory_iterator(skinsDirectory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".png") {
            SkinInfo info;
            info.id = entry.path().stem().string();
            info.name = info.id;
            info.filePath = entry.path().string();
            info.source = "local";
            m_impl->skins.push_back(info);
        }
    }
    return true;
}

void SkinManager::shutdown() {}

bool SkinManager::addSkin(const std::filesystem::path& skinPath, const std::string& name) {
    SkinInfo info;
    info.id = name.empty() ? skinPath.stem().string() : name;
    info.name = info.id;
    auto dest = m_impl->skinsDir / (info.id + ".png");
    std::filesystem::copy_file(skinPath, dest, std::filesystem::copy_options::overwrite_existing);
    info.filePath = dest.string();
    info.source = "local";
    m_impl->skins.push_back(info);
    return true;
}

bool SkinManager::removeSkin(const std::string& skinId) {
    auto it = std::find_if(m_impl->skins.begin(), m_impl->skins.end(),
        [&](const SkinInfo& s) { return s.id == skinId; });
    if (it == m_impl->skins.end()) return false;
    std::filesystem::remove(it->filePath);
    m_impl->skins.erase(it);
    return true;
}

bool SkinManager::renameSkin(const std::string& skinId, const std::string& newName) {
    auto it = std::find_if(m_impl->skins.begin(), m_impl->skins.end(),
        [&](const SkinInfo& s) { return s.id == skinId; });
    if (it == m_impl->skins.end()) return false;
    it->name = newName;
    return true;
}

std::vector<SkinInfo> SkinManager::getAllSkins() const { return m_impl->skins; }

std::optional<SkinInfo> SkinManager::getSkin(const std::string& skinId) const {
    for (const auto& s : m_impl->skins) {
        if (s.id == skinId) return s;
    }
    return std::nullopt;
}

std::optional<SkinInfo> SkinManager::getActiveSkin() const {
    return getSkin(m_impl->activeSkinId);
}

bool SkinManager::setActiveSkin(const std::string& skinId) {
    if (!getSkin(skinId)) return false;
    m_impl->activeSkinId = skinId;
    if (m_impl->onSkinChanged) {
        m_impl->onSkinChanged(*getSkin(skinId));
    }
    return true;
}

std::string SkinManager::getActiveSkinId() const { return m_impl->activeSkinId; }

std::future<std::optional<SkinInfo>> SkinManager::fetchFromMinecraft(const std::string& /*uuid*/) {
    return std::async(std::launch::async, []() -> std::optional<SkinInfo> { return std::nullopt; });
}
std::future<std::optional<SkinInfo>> SkinManager::fetchFromElyBy(const std::string& /*username*/) {
    return std::async(std::launch::async, []() -> std::optional<SkinInfo> { return std::nullopt; });
}
std::future<std::optional<SkinInfo>> SkinManager::fetchFromNameMC(const std::string& /*username*/) {
    return std::async(std::launch::async, []() -> std::optional<SkinInfo> { return std::nullopt; });
}
std::future<std::optional<SkinInfo>> SkinManager::fetchFromUrl(const std::string& /*url*/) {
    return std::async(std::launch::async, []() -> std::optional<SkinInfo> { return std::nullopt; });
}
std::future<bool> SkinManager::uploadToMinecraft(const std::string& /*skinId*/, const std::string& /*accessToken*/) {
    return std::async(std::launch::async, []() { return false; });
}
std::future<bool> SkinManager::uploadToElyBy(const std::string& /*skinId*/, const std::string& /*accessToken*/) {
    return std::async(std::launch::async, []() { return false; });
}

bool SkinManager::addCape(const std::filesystem::path& /*capePath*/, const std::string& /*name*/) { return false; }
bool SkinManager::removeCape(const std::string& /*capeId*/) { return false; }
std::vector<CapeInfo> SkinManager::getAllCapes() const { return m_impl->capes; }
std::optional<CapeInfo> SkinManager::getActiveCape() const { return std::nullopt; }
bool SkinManager::setActiveCape(const std::string& /*capeId*/) { return false; }

SkinRenderer& SkinManager::getRenderer() { return m_impl->renderer; }
SkinEditor& SkinManager::getEditor() { return m_impl->editor; }

void SkinManager::setOnSkinChanged(std::function<void(const SkinInfo&)> callback) {
    m_impl->onSkinChanged = std::move(callback);
}
void SkinManager::setOnCapeChanged(std::function<void(const CapeInfo&)> callback) {
    m_impl->onCapeChanged = std::move(callback);
}

// -- Utility functions --

SkinModel detectSkinModel(const ImageBuffer& buffer) {
    // Check pixel at (50, 16) - if transparent, it's slim
    auto pixel = buffer.getPixel(50, 16);
    return pixel.a == 0 ? SkinModel::Slim : SkinModel::Classic;
}

bool validateSkinDimensions(int width, int height) {
    return (width == 64 && (height == 64 || height == 32)) ||
           (width == 128 && height == 128);
}

std::string skinModelToString(SkinModel model) {
    return model == SkinModel::Slim ? "slim" : "classic";
}

SkinModel stringToSkinModel(const std::string& str) {
    return str == "slim" ? SkinModel::Slim : SkinModel::Classic;
}

} // namespace konami::skin
