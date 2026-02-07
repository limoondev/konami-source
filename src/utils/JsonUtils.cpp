/**
 * JsonUtils.cpp
 * 
 * JSON parsing and manipulation helpers.
 */

#include "JsonUtils.hpp"
#include <fstream>
#include <sstream>

namespace konami::utils {

// -- Parsing --

std::optional<json> JsonUtils::parse(const std::string& str) {
    try { return json::parse(str); }
    catch (const json::exception&) { return std::nullopt; }
}

std::optional<json> JsonUtils::parseFile(const std::filesystem::path& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return std::nullopt;
        return json::parse(file);
    } catch (const json::exception&) { return std::nullopt; }
}

bool JsonUtils::isValid(const std::string& str) { return json::accept(str); }

// -- Serialization --

std::string JsonUtils::stringify(const json& j, int indent) { return j.dump(indent); }
std::string JsonUtils::prettyPrint(const json& j, int indent) { return j.dump(indent); }

bool JsonUtils::writeFile(const std::filesystem::path& path, const json& j, int indent) {
    try {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(indent);
        return true;
    } catch (const std::exception&) { return false; }
}

// -- Safe accessors --

std::string JsonUtils::getString(const json& j, const std::string& key, const std::string& defaultValue) {
    if (j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
    return defaultValue;
}

int JsonUtils::getInt(const json& j, const std::string& key, int defaultValue) {
    if (j.contains(key) && j[key].is_number()) return j[key].get<int>();
    return defaultValue;
}

int64_t JsonUtils::getLong(const json& j, const std::string& key, int64_t defaultValue) {
    if (j.contains(key) && j[key].is_number()) return j[key].get<int64_t>();
    return defaultValue;
}

double JsonUtils::getDouble(const json& j, const std::string& key, double defaultValue) {
    if (j.contains(key) && j[key].is_number()) return j[key].get<double>();
    return defaultValue;
}

bool JsonUtils::getBool(const json& j, const std::string& key, bool defaultValue) {
    if (j.contains(key) && j[key].is_boolean()) return j[key].get<bool>();
    return defaultValue;
}

json JsonUtils::getObject(const json& j, const std::string& key, const json& defaultValue) {
    if (j.contains(key) && j[key].is_object()) return j[key];
    return defaultValue;
}

json JsonUtils::getArray(const json& j, const std::string& key, const json& defaultValue) {
    if (j.contains(key) && j[key].is_array()) return j[key];
    return defaultValue;
}

// -- Nested access with dot notation --

std::optional<json> JsonUtils::getPath(const json& j, const std::string& path) {
    try {
        std::string pointer = "/";
        for (char c : path) pointer += (c == '.') ? '/' : c;
        json::json_pointer ptr(pointer);
        if (j.contains(ptr)) return j.at(ptr);
    } catch (const json::exception&) {}
    return std::nullopt;
}

std::string JsonUtils::getPathString(const json& j, const std::string& path, const std::string& defaultValue) {
    auto val = getPath(j, path);
    if (val && val->is_string()) return val->get<std::string>();
    return defaultValue;
}

int JsonUtils::getPathInt(const json& j, const std::string& path, int defaultValue) {
    auto val = getPath(j, path);
    if (val && val->is_number()) return val->get<int>();
    return defaultValue;
}

bool JsonUtils::getPathBool(const json& j, const std::string& path, bool defaultValue) {
    auto val = getPath(j, path);
    if (val && val->is_boolean()) return val->get<bool>();
    return defaultValue;
}

void JsonUtils::setPath(json& j, const std::string& path, const json& value) {
    try {
        std::string pointer = "/";
        for (char c : path) pointer += (c == '.') ? '/' : c;
        j[json::json_pointer(pointer)] = value;
    } catch (const json::exception&) {}
}

// -- Type checking --

bool JsonUtils::hasKey(const json& j, const std::string& key) { return j.contains(key); }
bool JsonUtils::isString(const json& j, const std::string& key) { return j.contains(key) && j[key].is_string(); }
bool JsonUtils::isNumber(const json& j, const std::string& key) { return j.contains(key) && j[key].is_number(); }
bool JsonUtils::isObject(const json& j, const std::string& key) { return j.contains(key) && j[key].is_object(); }
bool JsonUtils::isArray(const json& j, const std::string& key) { return j.contains(key) && j[key].is_array(); }
bool JsonUtils::isBool(const json& j, const std::string& key) { return j.contains(key) && j[key].is_boolean(); }
bool JsonUtils::isNull(const json& j, const std::string& key) { return j.contains(key) && j[key].is_null(); }

// -- Merging --

json JsonUtils::merge(const json& base, const json& override_) {
    json result = base;
    result.merge_patch(override_);
    return result;
}

json JsonUtils::deepMerge(const json& base, const json& override_) {
    return merge(base, override_);
}

// -- Filtering --

json JsonUtils::filterKeys(const json& j, const std::vector<std::string>& keys) {
    json result = json::object();
    for (const auto& key : keys) {
        if (j.contains(key)) result[key] = j[key];
    }
    return result;
}

json JsonUtils::excludeKeys(const json& j, const std::vector<std::string>& keys) {
    json result = j;
    for (const auto& key : keys) result.erase(key);
    return result;
}

// -- Array operations --

json JsonUtils::findInArray(const json& arr, const std::string& key, const std::string& value) {
    if (!arr.is_array()) return nullptr;
    for (const auto& item : arr) {
        if (item.contains(key) && item[key] == value) return item;
    }
    return nullptr;
}

int JsonUtils::findIndexInArray(const json& arr, const std::string& key, const std::string& value) {
    if (!arr.is_array()) return -1;
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].contains(key) && arr[i][key] == value) return static_cast<int>(i);
    }
    return -1;
}

json JsonUtils::mapArray(const json& arr, std::function<json(const json&)> mapper) {
    json result = json::array();
    if (!arr.is_array()) return result;
    for (const auto& item : arr) result.push_back(mapper(item));
    return result;
}

json JsonUtils::filterArray(const json& arr, std::function<bool(const json&)> predicate) {
    json result = json::array();
    if (!arr.is_array()) return result;
    for (const auto& item : arr) {
        if (predicate(item)) result.push_back(item);
    }
    return result;
}

// -- Comparison --

bool JsonUtils::equals(const json& j1, const json& j2) { return j1 == j2; }
json JsonUtils::diff(const json& j1, const json& j2) { return json::diff(j1, j2); }

// -- Schema validation (basic) --

bool JsonUtils::validateSchema(const json& /*j*/, const json& /*schema*/) { return true; /* TODO */ }
std::vector<std::string> JsonUtils::getValidationErrors(const json& /*j*/, const json& /*schema*/) { return {}; }

// -- JsonPointer --

JsonPointer::JsonPointer(const std::string& path) {
    std::string ptr = "/";
    for (char c : path) ptr += (c == '.') ? '/' : c;
    m_pointer = nlohmann::json::json_pointer(ptr);
}

std::optional<json> JsonPointer::get(const json& j) const {
    try { if (j.contains(m_pointer)) return j.at(m_pointer); } catch (...) {}
    return std::nullopt;
}

void JsonPointer::set(json& j, const json& value) const {
    try { j[m_pointer] = value; } catch (...) {}
}

bool JsonPointer::exists(const json& j) const {
    try { return j.contains(m_pointer); } catch (...) { return false; }
}

void JsonPointer::remove(json& j) const {
    try { j.erase(m_pointer.back()); } catch (...) {}
}

// -- JsonBuilder --

JsonBuilder::JsonBuilder() : m_root(json::object()) {
    m_stack.push_back(&m_root);
}

JsonBuilder& JsonBuilder::set(const std::string& key, const std::string& value) { (*m_stack.back())[key] = value; return *this; }
JsonBuilder& JsonBuilder::set(const std::string& key, int value) { (*m_stack.back())[key] = value; return *this; }
JsonBuilder& JsonBuilder::set(const std::string& key, int64_t value) { (*m_stack.back())[key] = value; return *this; }
JsonBuilder& JsonBuilder::set(const std::string& key, double value) { (*m_stack.back())[key] = value; return *this; }
JsonBuilder& JsonBuilder::set(const std::string& key, bool value) { (*m_stack.back())[key] = value; return *this; }
JsonBuilder& JsonBuilder::set(const std::string& key, const json& value) { (*m_stack.back())[key] = value; return *this; }
JsonBuilder& JsonBuilder::setNull(const std::string& key) { (*m_stack.back())[key] = nullptr; return *this; }
JsonBuilder& JsonBuilder::setArray(const std::string& key) { (*m_stack.back())[key] = json::array(); return *this; }
JsonBuilder& JsonBuilder::setObject(const std::string& key) { (*m_stack.back())[key] = json::object(); return *this; }

JsonBuilder& JsonBuilder::beginObject(const std::string& key) {
    (*m_stack.back())[key] = json::object();
    m_stack.push_back(&(*m_stack.back())[key]);
    return *this;
}

JsonBuilder& JsonBuilder::endObject() {
    if (m_stack.size() > 1) m_stack.pop_back();
    return *this;
}

JsonBuilder& JsonBuilder::beginArray(const std::string& key) {
    (*m_stack.back())[key] = json::array();
    m_arrayKeys.push_back(key);
    m_stack.push_back(&(*m_stack.back())[key]);
    return *this;
}

JsonBuilder& JsonBuilder::endArray() {
    if (m_stack.size() > 1) m_stack.pop_back();
    if (!m_arrayKeys.empty()) m_arrayKeys.pop_back();
    return *this;
}

JsonBuilder& JsonBuilder::addToArray(const json& value) {
    if (m_stack.back()->is_array()) m_stack.back()->push_back(value);
    return *this;
}

json JsonBuilder::build() const { return m_root; }
std::string JsonBuilder::toString(int indent) const { return m_root.dump(indent); }

} // namespace konami::utils
