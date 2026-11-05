#include "real_instrument/mic_settings.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>
namespace real_instrument {
namespace {
// UTF-8 text of a path in C++17 (std::string) and C++20 (std::u8string).
std::string PathText(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}
void AppendEscaped(std::string& out, const std::string& text) {
    out += '"';
    for (unsigned char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
                out += buffer;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    out += '"';
}
void AppendUtf8(std::string& out, unsigned long cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}
// A minimal reader for one flat JSON object of strings, booleans, numbers and null.
struct Value { enum Kind { String, Bool, Other } kind = Other; std::string text; bool flag = false; };
class Reader {
public:
    explicit Reader(const std::string& text) : s_(text) {}
    bool Object(std::map<std::string, Value>* out, std::string* error) {
        Skip();
        if (!Eat('{')) return Fail("esperado '{'", error);
        Skip();
        if (Eat('}')) return End(error);
        while (true) {
            Skip();
            std::string key;
            if (!String(&key)) return Fail("chave invalida", error);
            Skip();
            if (!Eat(':')) return Fail("esperado ':'", error);
            Skip();
            Value value;
            if (Peek() == '"') {
                value.kind = Value::String;
                if (!String(&value.text)) return Fail("texto invalido", error);
            } else if (Word("true")) { value.kind = Value::Bool; value.flag = true; }
            else if (Word("false")) { value.kind = Value::Bool; value.flag = false; }
            else if (Word("null")) {}
            else if (!Number()) return Fail("valor invalido", error);
            (*out)[key] = value;
            Skip();
            if (Eat(',')) continue;
            if (Eat('}')) return End(error);
            return Fail("esperado ',' ou '}'", error);
        }
    }
private:
    bool End(std::string* error) { Skip(); return i_ == s_.size() ? true : Fail("texto depois do objeto", error); }
    bool Fail(const char* what, std::string* error) {
        if (error) *error = std::string("JSON invalido (") + what + ", posicao " + std::to_string(i_) + ")";
        return false;
    }
    char Peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
    bool Eat(char c) { if (Peek() != c) return false; ++i_; return true; }
    void Skip() { while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_; }
    bool Word(const char* word) {
        const std::string w(word);
        if (s_.compare(i_, w.size(), w) != 0) return false;
        i_ += w.size();
        return true;
    }
    bool Number() {
        const size_t start = i_;
        while (i_ < s_.size() && std::string("+-0123456789.eE").find(s_[i_]) != std::string::npos) ++i_;
        return i_ > start;
    }
    bool Hex4(unsigned long* value) {
        if (i_ + 4 > s_.size()) return false;
        *value = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s_[i_++];
            *value <<= 4;
            if (c >= '0' && c <= '9') *value |= static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f') *value |= static_cast<unsigned long>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') *value |= static_cast<unsigned long>(c - 'A' + 10);
            else return false;
        }
        return true;
    }
    bool String(std::string* out) {
        if (!Eat('"')) return false;
        out->clear();
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c != '\\') { *out += c; continue; }
            if (i_ >= s_.size()) return false;
            const char e = s_[i_++];
            switch (e) {
            case '"': *out += '"'; break;
            case '\\': *out += '\\'; break;
            case '/': *out += '/'; break;
            case 'b': *out += '\b'; break;
            case 'f': *out += '\f'; break;
            case 'n': *out += '\n'; break;
            case 'r': *out += '\r'; break;
            case 't': *out += '\t'; break;
            case 'u': {
                unsigned long cp = 0;
                if (!Hex4(&cp)) return false;
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    unsigned long low = 0;
                    if (!Eat('\\') || !Eat('u') || !Hex4(&low) || low < 0xDC00 || low > 0xDFFF) return false;
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return false;
                }
                AppendUtf8(*out, cp);
                break;
            }
            default: return false;
            }
        }
        return false;
    }
    const std::string& s_;
    size_t i_ = 0;
};
} // namespace

std::string SerializeMicSettings(const MicSettings& settings) {
    std::string out = "{\n  \"format\": ";
    AppendEscaped(out, kMicSettingsFormat);
    out += ",\n  \"enabled\": ";
    out += settings.enabled ? "true" : "false";
    out += ",\n  \"device_id\": ";
    AppendEscaped(out, settings.deviceId);
    out += ",\n  \"device_name\": ";
    AppendEscaped(out, settings.deviceName);
    out += "\n}\n";
    return out;
}

bool ParseMicSettings(const std::string& text, MicSettings* settings, std::string* error) {
    std::string body = text;
    if (body.size() >= 3 && static_cast<unsigned char>(body[0]) == 0xEF && static_cast<unsigned char>(body[1]) == 0xBB &&
        static_cast<unsigned char>(body[2]) == 0xBF) body.erase(0, 3);  // a BOM from Notepad
    std::map<std::string, Value> values;
    Reader reader(body);
    if (!reader.Object(&values, error)) return false;
    const auto format = values.find("format");
    if (format != values.end() && (format->second.kind != Value::String || format->second.text != kMicSettingsFormat)) {
        if (error) *error = "formato desconhecido (esperado " + std::string(kMicSettingsFormat) + ")";
        return false;
    }
    MicSettings result;
    if (const auto it = values.find("enabled"); it != values.end()) {
        if (it->second.kind != Value::Bool) { if (error) *error = "\"enabled\" deve ser true ou false"; return false; }
        result.enabled = it->second.flag;
    }
    for (const auto& [key, field] : {std::pair<const char*, std::string*>{"device_id", &result.deviceId},
                                     std::pair<const char*, std::string*>{"device_name", &result.deviceName}}) {
        const auto it = values.find(key);
        if (it == values.end()) continue;
        if (it->second.kind != Value::String) { if (error) *error = std::string("\"") + key + "\" deve ser texto"; return false; }
        *field = it->second.text;
    }
    *settings = result;
    return true;
}

MicFileLoad LoadMicSettingsFile(const std::filesystem::path& path, MicSettings* settings, std::string* error) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return MicFileLoad::Missing;
    std::ifstream file(path, std::ios::binary);
    if (!file) { if (error) *error = "nao foi possivel ler " + PathText(path); return MicFileLoad::Invalid; }
    std::ostringstream text;
    text << file.rdbuf();
    if (text.str().size() > 64 * 1024) { if (error) *error = "arquivo grande demais"; return MicFileLoad::Invalid; }
    return ParseMicSettings(text.str(), settings, error) ? MicFileLoad::Loaded : MicFileLoad::Invalid;
}

bool SaveMicSettingsFile(const std::filesystem::path& path, const MicSettings& settings, std::string* error) {
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) { if (error) *error = "nao foi possivel criar " + PathText(temporary); return false; }
        file << SerializeMicSettings(settings);
        file.flush();
        if (!file) { if (error) *error = "falha ao escrever " + PathText(temporary); return false; }
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        std::filesystem::remove(temporary, ec);
        if (error) *error = "nao foi possivel substituir " + PathText(path);
        return false;
    }
    return true;
}

MicDeviceResolution ResolveMicDevice(const std::vector<MicDeviceEntry>& devices, const MicSettings& wanted) {
    MicDeviceResolution result;
    result.name = kMicDefaultDeviceLabel;
    if (wanted.deviceId.empty() && wanted.deviceName.empty()) return result;
    result.requestedDefault = false;
    for (const auto& device : devices) {
        if (!wanted.deviceId.empty() && device.id == wanted.deviceId) {
            result.id = device.id;
            result.name = device.name;
            return result;
        }
    }
    if (!wanted.deviceName.empty()) {
        for (const auto& device : devices) {
            if (device.name == wanted.deviceName) {
                result.id = device.id;
                result.name = device.name;
                result.matchedByName = true;
                return result;
            }
        }
    }
    result.found = false;
    return result;
}

std::optional<std::size_t> ParseMicDeviceIndex(const std::string& text, std::size_t count) {
    if (text.empty() || text.size() > 6) return std::nullopt;
    std::size_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9') return std::nullopt;
        value = value * 10 + static_cast<std::size_t>(c - '0');
    }
    if (value >= count) return std::nullopt;
    return value;
}

MicStartupPlan PlanMicStartup(const std::string& environmentEnabled, const std::string& environmentDevice,
                              const std::optional<MicSettings>& saved, const std::vector<MicDeviceEntry>& devices) {
    MicStartupPlan plan;
    if (environmentEnabled == "1") plan.enabled = true;
    else if (environmentEnabled == "0") plan.enabled = false;
    else plan.enabled = saved.has_value() && saved->enabled;
    if (!environmentDevice.empty()) {
        if (const auto index = ParseMicDeviceIndex(environmentDevice, devices.size())) {
            plan.wanted.deviceId = devices[*index].id;
            plan.wanted.deviceName = devices[*index].name;
            plan.source = MicSelectionSource::Environment;
        } else {
            plan.notice = "TRIAEVUM_OCARINA_MIC_DEVICE=" + environmentDevice + " n\xC3\xA3o existe na lista; ";
            plan.notice += saved.has_value() ? "usando a escolha salva" : "usando o padr\xC3\xA3o do Windows";
        }
    }
    if (plan.source != MicSelectionSource::Environment && saved.has_value()) {
        plan.wanted.deviceId = saved->deviceId;
        plan.wanted.deviceName = saved->deviceName;
        plan.source = saved->deviceId.empty() && saved->deviceName.empty() ? MicSelectionSource::Default : MicSelectionSource::Saved;
    }
    plan.wanted.enabled = plan.enabled;
    plan.device = ResolveMicDevice(devices, plan.wanted);
    if (!plan.device.found) {
        const std::string name = plan.wanted.deviceName.empty() ? plan.wanted.deviceId : plan.wanted.deviceName;
        if (!plan.notice.empty()) plan.notice += ". ";
        plan.notice += "Microfone salvo \"" + name + "\" n\xC3\xA3o encontrado; usando o padr\xC3\xA3o do Windows";
    }
    return plan;
}

std::vector<std::string> MicChoiceLabels(const std::vector<MicDeviceEntry>& devices) {
    std::vector<std::string> labels{kMicDefaultDeviceLabel};
    for (std::size_t i = 0; i < devices.size(); ++i) {
        const auto& name = devices[i].name.empty() ? devices[i].id : devices[i].name;
        const auto total = std::count_if(devices.begin(), devices.end(), [&](const MicDeviceEntry& d) { return d.name == devices[i].name; });
        if (total > 1) {
            const auto ordinal = std::count_if(devices.begin(), devices.begin() + static_cast<std::ptrdiff_t>(i) + 1,
                                               [&](const MicDeviceEntry& d) { return d.name == devices[i].name; });
            labels.push_back(name + " (" + std::to_string(ordinal) + ")");
        } else {
            labels.push_back(name);
        }
    }
    return labels;
}

int MicChoiceIndex(const std::vector<MicDeviceEntry>& devices, const std::string& deviceId) {
    if (deviceId.empty()) return 0;
    for (std::size_t i = 0; i < devices.size(); ++i)
        if (devices[i].id == deviceId) return static_cast<int>(i) + 1;
    return 0;
}

MicSettings MicSettingsForChoice(const std::vector<MicDeviceEntry>& devices, int choice, bool enabled) {
    MicSettings settings;
    settings.enabled = enabled;
    if (choice >= 1 && static_cast<std::size_t>(choice) <= devices.size()) {
        settings.deviceId = devices[static_cast<std::size_t>(choice) - 1].id;
        settings.deviceName = devices[static_cast<std::size_t>(choice) - 1].name;
    }
    return settings;
}

namespace {
double Decibels(float rms) {
    if (!std::isfinite(rms) || rms <= 1e-6f) return -120.0;
    return 20.0 * std::log10(static_cast<double>(rms));
}
}

std::string MicLevelBar(float rms, int cells) {
    cells = std::clamp(cells, 1, 64);
    const double fraction = std::clamp((Decibels(rms) + 60.0) / 60.0, 0.0, 1.0);
    const int filled = static_cast<int>(std::lround(fraction * cells));
    std::string bar;
    for (int i = 0; i < cells; ++i) bar += i < filled ? "|" : "\xC2\xB7";  // | and middle dot
    return bar;
}

std::string MicLevelDecibels(float rms) {
    const double db = Decibels(rms);
    if (db <= -90.0) return "sil\xC3\xAAncio";
    return std::to_string(static_cast<int>(std::lround(db))) + " dB";
}

std::string MicNoteName(float midiPitch) {
    if (!std::isfinite(midiPitch) || midiPitch < 0 || midiPitch > 140) return "?";
    static constexpr const char* names[] = {"D\xC3\xB3", "D\xC3\xB3#", "R\xC3\xA9", "R\xC3\xA9#", "Mi", "F\xC3\xA1",
                                            "F\xC3\xA1#", "Sol", "Sol#", "L\xC3\xA1", "L\xC3\xA1#", "Si"};
    const int midi = static_cast<int>(std::lround(midiPitch));
    return std::string(names[midi % 12]) + std::to_string(midi / 12 - 1);
}

}
