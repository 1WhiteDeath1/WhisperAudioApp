#define IMGUI_DEFINE_MATH_OPERATORS // lets ImVec2 use + - * so the drawing code stays readable
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include "imgui.h"
#include "imgui-SFML.h"
#include "AudioEngine.h"
#include <algorithm>
#include <SFML/Audio.hpp>
#include "Spectrogram.h"
#include "AudioEngine/DSP/FFT.h"
#include "ImGuiFileDialog.h"
#include <functional>
#include <string>
#include <vector>
#include <deque>
#include <future>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>

static const float kPi = 3.14159265358979f;


// THEME

// colours for everything drawn by hand (waveform, ruler, markers), filled in by applyTheme
struct Theme {
    bool light = false;
    ImVec4 accent;
    ImVec4 textOnAccent;
    ImVec4 error;
    ImU32 canvasBg, rulerBg, laneBorder, centerLine, grid, gridText;
    ImU32 wavePeak, waveRms, selFill, selEdge, noiseFill, noiseEdge, playhead, hoverLine;
};
static Theme g_theme;

static ImU32 rgba(float r, float g, float b, float a = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
}

static ImVec4 shade(const ImVec4& c, float k) {
    return ImVec4(std::min(c.x * k, 1.0f), std::min(c.y * k, 1.0f), std::min(c.z * k, 1.0f), c.w);
}

static ImVec4 withAlpha(const ImVec4& c, float a) {
    return ImVec4(c.x, c.y, c.z, a);
}

static void applyTheme(bool light) {
    if (light) ImGui::StyleColorsLight();
    else ImGui::StyleColorsDark();

    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0.0f;
    s.ChildRounding = 0.0f;
    s.FrameRounding = 0.0f;
    s.PopupRounding = 0.0f;
    s.GrabRounding = 0.0f;
    s.TabRounding = 0.0f;
    s.ScrollbarRounding = 0.0f;
    s.WindowPadding = ImVec2(2, 2);
    s.FramePadding = ImVec2(4, 2);
    s.ItemSpacing = ImVec2(4, 3);
    s.ItemInnerSpacing = ImVec2(4, 4);
    s.ScrollbarSize = 14.0f;
    s.GrabMinSize = 10.0f;
    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize = 1.0f;
    s.PopupBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.TabBorderSize = 0.0f;
    s.SeparatorTextBorderSize = 1.0f;
    s.SeparatorTextPadding = ImVec2(0, 2);

    Theme& t = g_theme;
    t.light = light;
    ImVec4* c = s.Colors;

    ImVec4 bg0, bg1, bg2, bg3, bg4, tabActive;
    if (!light) {
        t.accent = ImVec4(0.20f, 0.82f, 0.70f, 1.0f);
        t.textOnAccent = ImVec4(0.04f, 0.08f, 0.08f, 1.0f);
        t.error = ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
        bg0 = ImVec4(0.090f, 0.095f, 0.110f, 1.0f);
        bg1 = ImVec4(0.120f, 0.127f, 0.145f, 1.0f);
        bg2 = ImVec4(0.170f, 0.178f, 0.200f, 1.0f);
        bg3 = ImVec4(0.215f, 0.225f, 0.255f, 1.0f);
        bg4 = ImVec4(0.265f, 0.277f, 0.310f, 1.0f);
        tabActive = ImVec4(0.13f, 0.34f, 0.31f, 1.0f);
        c[ImGuiCol_Text] = ImVec4(0.90f, 0.91f, 0.93f, 1.0f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.57f, 1.0f);
        c[ImGuiCol_Border] = ImVec4(0.21f, 0.22f, 0.25f, 1.0f);

        t.canvasBg = rgba(0.070f, 0.074f, 0.086f);
        t.rulerBg = rgba(0.105f, 0.110f, 0.127f);
        t.laneBorder = rgba(0.20f, 0.21f, 0.24f);
        t.centerLine = rgba(1, 1, 1, 0.10f);
        t.grid = rgba(1, 1, 1, 0.045f);
        t.gridText = rgba(0.58f, 0.60f, 0.65f);
        t.wavePeak = rgba(0.20f, 0.82f, 0.70f, 0.50f);
        t.waveRms = rgba(0.50f, 0.96f, 0.87f, 0.95f);
        t.selFill = rgba(0.35f, 0.60f, 1.00f, 0.13f);
        t.selEdge = rgba(0.45f, 0.68f, 1.00f, 0.90f);
        t.noiseFill = rgba(1.00f, 0.70f, 0.20f, 0.10f);
        t.noiseEdge = rgba(1.00f, 0.72f, 0.25f, 0.85f);
        t.playhead = rgba(1.00f, 0.38f, 0.38f);
        t.hoverLine = rgba(1, 1, 1, 0.22f);
    }
    else {
        t.accent = ImVec4(0.02f, 0.56f, 0.48f, 1.0f);
        t.textOnAccent = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        t.error = ImVec4(0.80f, 0.12f, 0.12f, 1.0f);
        bg0 = ImVec4(1.000f, 1.000f, 1.000f, 1.0f);
        bg1 = ImVec4(0.955f, 0.958f, 0.965f, 1.0f);
        bg2 = ImVec4(0.905f, 0.910f, 0.925f, 1.0f);
        bg3 = ImVec4(0.860f, 0.868f, 0.890f, 1.0f);
        bg4 = ImVec4(0.800f, 0.812f, 0.840f, 1.0f);
        tabActive = ImVec4(0.78f, 0.92f, 0.89f, 1.0f);
        c[ImGuiCol_Text] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.50f, 0.54f, 1.0f);
        c[ImGuiCol_Border] = ImVec4(0.82f, 0.83f, 0.86f, 1.0f);

        t.canvasBg = rgba(1.0f, 1.0f, 1.0f);
        t.rulerBg = rgba(0.945f, 0.948f, 0.957f);
        t.laneBorder = rgba(0.84f, 0.85f, 0.88f);
        t.centerLine = rgba(0, 0, 0, 0.12f);
        t.grid = rgba(0, 0, 0, 0.06f);
        t.gridText = rgba(0.40f, 0.42f, 0.46f);
        t.wavePeak = rgba(0.02f, 0.56f, 0.48f, 0.40f);
        t.waveRms = rgba(0.00f, 0.42f, 0.36f, 0.95f);
        t.selFill = rgba(0.15f, 0.40f, 0.95f, 0.10f);
        t.selEdge = rgba(0.15f, 0.40f, 0.95f, 0.90f);
        t.noiseFill = rgba(0.95f, 0.55f, 0.00f, 0.12f);
        t.noiseEdge = rgba(0.90f, 0.50f, 0.00f, 0.90f);
        t.playhead = rgba(0.90f, 0.15f, 0.15f);
        t.hoverLine = rgba(0, 0, 0, 0.25f);
    }

    c[ImGuiCol_WindowBg] = bg1;
    c[ImGuiCol_ChildBg] = bg0;
    c[ImGuiCol_PopupBg] = bg1;
    c[ImGuiCol_MenuBarBg] = bg0;
    c[ImGuiCol_TitleBg] = bg0;
    c[ImGuiCol_TitleBgActive] = bg1;
    c[ImGuiCol_TitleBgCollapsed] = bg0;
    c[ImGuiCol_FrameBg] = bg2;
    c[ImGuiCol_FrameBgHovered] = bg3;
    c[ImGuiCol_FrameBgActive] = bg4;
    c[ImGuiCol_Button] = bg2;
    c[ImGuiCol_ButtonHovered] = bg3;
    c[ImGuiCol_ButtonActive] = bg4;
    c[ImGuiCol_Header] = bg2;
    c[ImGuiCol_HeaderHovered] = bg3;
    c[ImGuiCol_HeaderActive] = bg4;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = bg3;
    c[ImGuiCol_ScrollbarGrabHovered] = bg4;
    c[ImGuiCol_ScrollbarGrabActive] = t.accent;
    c[ImGuiCol_Separator] = c[ImGuiCol_Border];
    c[ImGuiCol_Tab] = bg2;
    c[ImGuiCol_TabHovered] = bg4;
    c[ImGuiCol_TabActive] = tabActive;
    c[ImGuiCol_TabUnfocused] = bg2;
    c[ImGuiCol_TabUnfocusedActive] = tabActive;
    c[ImGuiCol_CheckMark] = t.accent;
    c[ImGuiCol_SliderGrab] = t.accent;
    c[ImGuiCol_SliderGrabActive] = shade(t.accent, 1.15f);
    c[ImGuiCol_ResizeGrip] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_NavHighlight] = t.accent;
    c[ImGuiCol_TextSelectedBg] = withAlpha(t.accent, 0.35f);
}


// SMALL WIDGETS

static bool accentButton(const char* label, const ImVec2& size = ImVec2(-1, 0)) {
    ImGui::PushStyleColor(ImGuiCol_Button, g_theme.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, shade(g_theme.accent, 1.12f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, shade(g_theme.accent, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Text, g_theme.textOnAccent);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

// row of joined buttons where exactly one is selected, like the mode switches in most DAWs
static bool segmented(const char* id, int* value, const char* const labels[], int count, float width) {
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    float buttonWidth = (width - 2.0f * (count - 1)) / count;
    bool changed = false;
    for (int i = 0; i < count; i++) {
        bool active = (*value == i);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, g_theme.accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_theme.accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_theme.accent);
            ImGui::PushStyleColor(ImGuiCol_Text, g_theme.textOnAccent);
        }
        if (ImGui::Button(labels[i], ImVec2(buttonWidth, 0)) && !active) {
            *value = i;
            changed = true;
        }
        if (active) ImGui::PopStyleColor(4);
        if (i < count - 1) ImGui::SameLine();
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
    return changed;
}

static bool toggleSwitch(const char* label, bool* value) {
    return ImGui::Checkbox(label, value);
}

static bool toggleSwitchFancy(const char* label, bool* value) {
    ImGui::PushID(label);
    const float h = ImGui::GetFrameHeight();
    const float trackH = h * 0.72f, trackW = trackH * 1.9f;
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton("##toggle", ImVec2(trackW, h));
    bool hovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 t0(p.x, p.y + (h - trackH) * 0.5f);
    ImVec2 t1(p.x + trackW, t0.y + trackH);
    ImVec4 track = *value ? g_theme.accent : ImGui::GetStyleColorVec4(hovered ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBgHovered);
    dl->AddRectFilled(t0, t1, ImGui::GetColorU32(track), trackH * 0.5f);
    float knobX = *value ? t1.x - trackH * 0.5f : t0.x + trackH * 0.5f;
    dl->AddCircleFilled(ImVec2(knobX, t0.y + trackH * 0.5f), trackH * 0.5f - 3.0f, IM_COL32(255, 255, 255, 255));

    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (ImGui::IsItemClicked()) pressed = true; // clicking the label works too
    if (pressed) *value = !*value;
    ImGui::PopID();
    return pressed;
}

enum class Icon { Play, Pause, Stop, Loop };

// round transport button with the icon drawn by hand (the default font has no play/pause glyphs)
static bool iconButton(const char* id, Icon icon, float size, bool highlighted) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
    bool hovered = ImGui::IsItemHovered();
    bool held = ImGui::IsItemActive();

    ImU32 bg, fg;
    if (highlighted) {
        ImVec4 a = held ? shade(g_theme.accent, 0.85f) : hovered ? shade(g_theme.accent, 1.12f) : g_theme.accent;
        bg = ImGui::GetColorU32(a);
        fg = ImGui::GetColorU32(g_theme.textOnAccent);
    }
    else {
        bg = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        fg = ImGui::GetColorU32(ImGuiCol_Text);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = p + ImVec2(size, size) * 0.5f;
    float r = size * 0.22f;
    dl->AddCircleFilled(c, size * 0.5f, bg, 32);
    switch (icon) {
    case Icon::Play:
        dl->AddTriangleFilled(c + ImVec2(-r * 0.7f, -r), c + ImVec2(-r * 0.7f, r), c + ImVec2(r * 1.1f, 0), fg);
        break;
    case Icon::Pause: {
        float barW = r * 0.6f;
        dl->AddRectFilled(c + ImVec2(-r * 0.85f, -r), c + ImVec2(-r * 0.85f + barW, r), fg, 1.5f);
        dl->AddRectFilled(c + ImVec2(r * 0.85f - barW, -r), c + ImVec2(r * 0.85f, r), fg, 1.5f);
        break;
    }
    case Icon::Stop:
        dl->AddRectFilled(c - ImVec2(r, r) * 0.85f, c + ImVec2(r, r) * 0.85f, fg, 2.0f);
        break;
    case Icon::Loop: {
        // circular arrow
        float end = kPi * 1.75f;
        dl->PathArcTo(c, r, kPi * 0.25f, end, 24);
        dl->PathStroke(fg, ImDrawFlags_None, size * 0.055f);
        ImVec2 tip = c + ImVec2(std::cos(end), std::sin(end)) * r;
        ImVec2 dir(-std::sin(end), std::cos(end));
        ImVec2 out(std::cos(end), std::sin(end));
        float s = size * 0.11f;
        dl->AddTriangleFilled(tip + dir * s, tip + out * s, tip - out * s, fg);
        break;
    }
    }
    return pressed;
}

static void drawLogo(float size) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, p + ImVec2(size, size), ImGui::GetColorU32(g_theme.accent), size * 0.28f);
    const float heights[5] = { 0.35f, 0.75f, 1.0f, 0.6f, 0.3f };
    float barW = size * 0.09f, gap = size * 0.075f;
    float x = p.x + (size - (5 * barW + 4 * gap)) * 0.5f;
    float cy = p.y + size * 0.5f;
    ImU32 barCol = ImGui::GetColorU32(g_theme.textOnAccent);
    for (int i = 0; i < 5; i++) {
        float h = heights[i] * size * 0.55f;
        dl->AddRectFilled(ImVec2(x, cy - h * 0.5f), ImVec2(x + barW, cy + h * 0.5f), barCol, barW * 0.5f);
        x += barW + gap;
    }
    ImGui::Dummy(ImVec2(size, size));
}

static void sectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

static void hint(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static void errorText(const std::string& text) {
    if (text.empty()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, g_theme.error);
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
}

static float buttonWidth(const char* label) {
    return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// draws a 1px line along a window edge, ignoring the window clip rect so it isnt cut off by the padding
static void drawEdge(ImVec2 a, ImVec2 b) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRectFullScreen();
    dl->AddLine(a, b, ImGui::GetColorU32(ImGuiCol_Border));
    dl->PopClipRect();
}

// text with a dark outline so it stays readable on top of the spectrogram colours
static void shadowText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text) {
    dl->AddText(pos + ImVec2(1, 1), IM_COL32(0, 0, 0, 200), text);
    dl->AddText(pos, col, text);
}

static std::string formatTime(double seconds, int decimals = 3) {
    if (seconds < 0) seconds = 0;
    int mins = (int)(seconds / 60.0);
    double secs = seconds - mins * 60.0;
    char buf[32];
    if (decimals <= 0) snprintf(buf, sizeof(buf), "%d:%02d", mins, (int)secs);
    else snprintf(buf, sizeof(buf), "%d:%0*.*f", mins, decimals + 3, decimals, secs);
    return buf;
}

static std::string shorten(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 3) + "...";
}

static std::string timestampedName(const char* prefix, const char* ext) {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &local);
    return std::string(prefix) + buf + ext;
}

// imgui-SFML keeps its own converter private, this is the same thing (the gl handle stored in the id)
static ImTextureID toTextureID(const sf::Texture& texture) {
    ImTextureID id{};
    unsigned int handle = texture.getNativeHandle();
    std::memcpy(&id, &handle, sizeof(handle));
    return id;
}

// small plot of gain over frequency (log axis) used to preview the filter and the eq before applying them
static void responseGraph(float height, float maxGain, const std::function<float(float)>& gainAt) {
    const Theme& t = g_theme;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImGui::Dummy(ImVec2(width, height));
    ImVec2 p1 = p0 + ImVec2(width, height);

    dl->AddRectFilled(p0, p1, t.canvasBg, 5.0f);
    dl->AddRect(p0, p1, t.laneBorder, 5.0f);

    const float fMin = 20.0f, fMax = 20000.0f;
    const float logRange = std::log(fMax / fMin);
    const float top = p0.y + 6.0f, bottom = p1.y - 16.0f;
    auto freqToX = [&](float f) { return p0.x + std::log(f / fMin) / logRange * width; };
    auto gainToY = [&](float g) { return bottom - std::clamp(g / maxGain, 0.0f, 1.0f) * (bottom - top); };

    const float marks[] = { 100.0f, 1000.0f, 10000.0f };
    const char* markLabels[] = { "100", "1k", "10k" };
    for (int i = 0; i < 3; i++) {
        float x = freqToX(marks[i]);
        dl->AddLine(ImVec2(x, top), ImVec2(x, bottom), t.grid);
        dl->AddText(ImVec2(x + 3, bottom + 1), t.gridText, markLabels[i]);
    }
    float unityY = gainToY(1.0f);
    dl->AddLine(ImVec2(p0.x, unityY), ImVec2(p1.x, unityY), t.centerLine);

    ImVec4 a = g_theme.accent;
    ImU32 fill = ImGui::GetColorU32(withAlpha(a, 0.18f));
    std::vector<ImVec2> points;
    points.reserve((size_t)width + 1);
    for (int col = 0; col <= (int)width; col++) {
        float f = fMin * std::exp(logRange * col / width);
        float y = gainToY(gainAt(f));
        float x = p0.x + col;
        if (col < (int)width) dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 1, bottom), fill);
        points.push_back(ImVec2(x, y));
    }
    dl->PushClipRect(p0, p1, true);
    dl->AddPolyline(points.data(), (int)points.size(), ImGui::GetColorU32(a), ImDrawFlags_None, 2.0f);
    dl->PopClipRect();
}


// DSP HELPERS FOR THE VISUALS

// runs the engine's fft (bit reversal + butterflies) on one hann windowed frame,
// with the twiddle table built once instead of per call
class FrameFFT {
public:
    explicit FrameFFT(int size) : size(size), window(size), buffer(size) {
        twiddle = buildTwiddleTable(size);
        for (int i = 0; i < size; i++)
            window[i] = 0.5 * (1.0 - std::cos(2.0 * kPi * i / size));
    }
    ~FrameFFT() { delete[] twiddle; }
    FrameFFT(const FrameFFT&) = delete;
    FrameFFT& operator=(const FrameFFT&) = delete;

    int getSize() const { return size; }

    // input has to hold size samples, out gets size/2 magnitudes
    void magnitudes(const float* input, std::vector<float>& out) {
        for (int i = 0; i < size; i++) buffer[i] = Complex(input[i] * window[i], 0.0);
        reorderBitReversal(buffer.data(), size);
        butterfly(buffer.data(), size, true, twiddle);
        out.resize(size / 2);
        for (int i = 0; i < size / 2; i++) out[i] = (float)buffer[i].magnitude();
    }

private:
    int size;
    Complex* twiddle;
    std::vector<double> window;
    std::vector<Complex> buffer;
};

// value of the magnitude curve between binStart and binEnd: max of the bins when the range covers
// several, interpolated when its narrower than a bin (low frequencies on a log axis)
static float bandMagnitude(const std::vector<float>& mags, double binStart, double binEnd) {
    int usable = (int)mags.size();
    if (usable < 2) return 0.0f;
    if (binEnd - binStart < 1.0) {
        int i0 = std::clamp((int)binStart, 0, usable - 2);
        double frac = std::clamp(binStart - i0, 0.0, 1.0);
        return (float)((1.0 - frac) * mags[i0] + frac * mags[i0 + 1]);
    }
    int s = std::clamp((int)binStart, 0, usable - 1);
    int e = std::clamp((int)std::ceil(binEnd), s + 1, usable);
    float m = 0.0f;
    for (int i = s; i < e; i++) m = std::max(m, mags[i]);
    return m;
}

struct AudioStats {
    int key = INT_MIN;
    float peakDb = -INFINITY, rmsDb = -INFINITY;
    int clipped = 0;
};

static void computeStats(AudioStats& s, Whisper& audio, int key) {
    if (s.key == key) return;
    s.key = key;
    const short* d = audio.getDataPointer();
    int n = audio.getNumOfS();
    int peak = 0;
    double sumSq = 0.0;
    int clipped = 0;
    for (int i = 0; i < n; i++) {
        int v = std::abs((int)d[i]);
        peak = std::max(peak, v);
        sumSq += (double)d[i] * d[i];
        if (v >= 32767) clipped++;
    }
    s.peakDb = peak > 0 ? 20.0f * std::log10(peak / 32768.0f) : -INFINITY;
    s.rmsDb = sumSq > 0 ? 20.0f * (float)std::log10(std::sqrt(sumSq / std::max(1, n)) / 32768.0) : -INFINITY;
    s.clipped = clipped;
}

// smooth fade over [start, end] seconds, raised cosine so it doesnt click at either end
static void applyFade(Whisper& audio, double start, double end, bool fadeIn) {
    WavHeader h = audio.getHeader();
    int channels = std::max<int>(1, h.numChannels);
    int frames = audio.getNumOfS() / channels;
    int f0 = std::clamp((int)(start * h.sampleRate), 0, frames);
    int f1 = std::clamp((int)(end * h.sampleRate), 0, frames);
    int length = f1 - f0;
    if (length < 2) return;
    short* d = audio.getDataPointer();
    for (int f = f0; f < f1; f++) {
        double x = (double)(f - f0) / (length - 1);
        double gain = 0.5 - 0.5 * std::cos(kPi * x);
        if (!fadeIn) gain = 1.0 - gain;
        for (int c = 0; c < channels; c++) {
            short& s = d[f * channels + c];
            s = (short)std::lround(s * gain);
        }
    }
}


// SPECTROGRAM

struct SpectrogramPixels {
    int key = 0;
    unsigned width = 0, height = 0;
    float fMin = 0.0f, fMax = 0.0f;
    std::vector<sf::Uint8> rgba;
};

// the whole file rendered once into a texture (time across, log frequency up),
// zooming just shows a part of it. Rendered on a worker thread so the ui doesnt freeze
struct SpectrogramImage {
    int key = INT_MIN; // which audio the texture shows
    bool valid = false;
    float fMin = 0.0f, fMax = 0.0f;
    sf::Texture texture;
    std::future<SpectrogramPixels> job;

    bool busy() const { return job.valid(); }
};

// inferno style colour map, dark purple for quiet to bright yellow for loud
static sf::Color heatColor(float v) {
    static const float keys[9][3] = {
        { 0, 0, 4 }, { 31, 12, 72 }, { 85, 15, 109 }, { 136, 34, 106 }, { 186, 54, 85 },
        { 227, 89, 51 }, { 249, 140, 10 }, { 249, 201, 50 }, { 252, 255, 164 } };
    v = std::clamp(v, 0.0f, 1.0f) * 8.0f;
    int i = std::min((int)v, 7);
    float f = v - i;
    return sf::Color(
        (sf::Uint8)(keys[i][0] + (keys[i + 1][0] - keys[i][0]) * f),
        (sf::Uint8)(keys[i][1] + (keys[i + 1][1] - keys[i][1]) * f),
        (sf::Uint8)(keys[i][2] + (keys[i + 1][2] - keys[i][2]) * f));
}

// runs on a worker thread, gets its own copy of the audio
static SpectrogramPixels renderSpectrogram(Whisper audio, int key) {
    SpectrogramPixels out;
    out.key = key;

    WavHeader h = audio.getHeader();
    int channels = std::max<int>(1, h.numChannels);
    int frames = audio.getNumOfS() / channels;
    int sampleRate = h.sampleRate;
    const int fftSize = 2048;
    if (frames < fftSize || sampleRate <= 0) return out;

    // mix down to mono so one picture shows every channel
    const short* d = audio.getDataPointer();
    std::vector<float> mono(frames);
    for (int f = 0; f < frames; f++) {
        int sum = 0;
        for (int c = 0; c < channels; c++) sum += d[f * channels + c];
        mono[f] = (float)sum / channels;
    }

    // long files get a bigger hop so there are never more frames than texture columns
    const int maxColumns = 4096;
    int hop = std::clamp(frames / maxColumns, 256, fftSize);
    // only frames that fit completely, a zero padded last frame shows up as a bright line at the end
    int numFrames = (frames - fftSize) / hop + 1;
    const unsigned width = (unsigned)std::min(numFrames, maxColumns);
    const unsigned height = 384;

    out.fMin = 30.0f;
    out.fMax = sampleRate * 0.5f;
    double logRange = std::log(out.fMax / out.fMin);
    double binsPerHz = (double)fftSize / sampleRate;
    std::vector<double> rowStart(height), rowEnd(height);
    for (unsigned r = 0; r < height; r++) {
        rowStart[r] = out.fMin * std::exp(logRange * r / height) * binsPerHz;
        rowEnd[r] = out.fMin * std::exp(logRange * (r + 1) / height) * binsPerHz;
    }

    FrameFFT fft(fftSize);
    std::vector<float> frame(fftSize), mags;
    std::vector<float> grid((size_t)width * height, 0.0f);
    float peak = 0.0f;
    for (int k = 0; k < numFrames; k++) {
        int start = k * hop;
        for (int i = 0; i < fftSize; i++) {
            int idx = start + i;
            frame[i] = idx < frames ? mono[idx] : 0.0f;
        }
        fft.magnitudes(frame.data(), mags);
        size_t col = (size_t)((long long)k * width / numFrames);
        for (unsigned r = 0; r < height; r++) {
            float m = bandMagnitude(mags, rowStart[r], rowEnd[r]);
            float& cell = grid[col * height + r];
            cell = std::max(cell, m);
            peak = std::max(peak, m);
        }
    }
    if (peak <= 0.0f) return out;

    const float dbFloor = -90.0f;
    out.width = width;
    out.height = height;
    out.rgba.resize((size_t)width * height * 4);
    for (unsigned col = 0; col < width; col++) {
        for (unsigned r = 0; r < height; r++) {
            float m = grid[(size_t)col * height + r];
            float db = 20.0f * std::log10(std::max(m, 1e-9f) / peak);
            sf::Color c = heatColor(1.0f - db / dbFloor);
            size_t idx = ((size_t)(height - 1 - r) * width + col) * 4;
            out.rgba[idx + 0] = c.r;
            out.rgba[idx + 1] = c.g;
            out.rgba[idx + 2] = c.b;
            out.rgba[idx + 3] = 255;
        }
    }
    return out;
}

// picks up a finished render and uploads it to the gpu
static void pollSpectrogram(SpectrogramImage& s) {
    if (!s.job.valid() || s.job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    SpectrogramPixels px = s.job.get();
    s.key = px.key;
    s.valid = false;
    if (px.width > 0 && s.texture.create(px.width, px.height)) {
        s.texture.update(px.rgba.data());
        s.texture.setSmooth(true);
        s.fMin = px.fMin;
        s.fMax = px.fMax;
        s.valid = true;
    }
}

static void requestSpectrogram(SpectrogramImage& s, Whisper& audio, int key) {
    if (s.key == key || s.busy()) return;
    s.job = std::async(std::launch::async, renderSpectrogram, audio, key);
}

// 1600x900 png with the spectrogram before and after processing, ready to post
static bool exportBeforeAfter(const SpectrogramImage& before, const SpectrogramImage& after,
                              const std::wstring& title, const std::wstring& subtitle, const std::string& path) {
    const unsigned W = 1600, H = 900;
    sf::RenderTexture rt;
    if (!rt.create(W, H)) return false;
    rt.clear(sf::Color(16, 17, 21));

    sf::Font bold, regular;
    bool hasBold = bold.loadFromFile("C:\\Windows\\Fonts\\segoeuib.ttf");
    bool hasRegular = regular.loadFromFile("C:\\Windows\\Fonts\\segoeui.ttf");
    auto text = [&](bool useBold, const sf::String& s, unsigned size, float x, float y, sf::Color c) {
        const sf::Font* f = useBold && hasBold ? &bold : hasRegular ? &regular : hasBold ? &bold : nullptr;
        if (!f) return;
        sf::Text t(s, *f, size);
        t.setFillColor(c);
        t.setPosition(x, y);
        rt.draw(t);
    };
    const sf::Color accent(51, 209, 179);
    const sf::Color dim(140, 145, 158);

    text(true, title, 40, 48, 26, sf::Color(235, 237, 240));
    text(false, subtitle, 22, 50, 80, dim);

    const float panelX = 48, panelW = W - 96.0f, panelH = 330;
    const float panelY[2] = { 136, 136 + panelH + 44 };
    const SpectrogramImage* images[2] = { &before, &after };
    const char* labels[2] = { "BEFORE", "AFTER" };
    for (int i = 0; i < 2; i++) {
        sf::Sprite sprite(images[i]->texture);
        sf::Vector2u ts = images[i]->texture.getSize();
        sprite.setPosition(panelX, panelY[i]);
        sprite.setScale(panelW / ts.x, panelH / ts.y);
        rt.draw(sprite);

        // frequency labels up the left edge
        float logRange = std::log(images[i]->fMax / images[i]->fMin);
        const float marks[] = { 100.0f, 1000.0f, 10000.0f };
        const char* markLabels[] = { "100 Hz", "1 kHz", "10 kHz" };
        for (int m = 0; m < 3; m++) {
            if (marks[m] >= images[i]->fMax) continue;
            float y = panelY[i] + panelH - std::log(marks[m] / images[i]->fMin) / logRange * panelH;
            sf::RectangleShape tick(sf::Vector2f(10, 1));
            tick.setPosition(panelX, y);
            tick.setFillColor(sf::Color(255, 255, 255, 160));
            rt.draw(tick);
            text(false, markLabels[m], 15, panelX + 14, y - 11, sf::Color(255, 255, 255, 200));
        }

        sf::RectangleShape badge(sf::Vector2f(i == 0 ? 96.0f : 80.0f, 30));
        badge.setPosition(panelX + panelW - badge.getSize().x - 12, panelY[i] + 12);
        badge.setFillColor(i == 0 ? sf::Color(60, 63, 72, 230) : accent);
        rt.draw(badge);
        text(true, labels[i], 17, badge.getPosition().x + 12, badge.getPosition().y + 3,
            i == 0 ? sf::Color(230, 232, 236) : sf::Color(10, 20, 20));
    }
    text(false, "Spectrogram  |  2048-point STFT, log frequency  |  hand-written C++ DSP", 16, 50, H - 38.0f, dim);

    rt.display();
    return rt.getTexture().copyToImage().saveToFile(path);
}


// LIVE ANALYZER + METERS

struct LiveAnalyzer {
    static const int kBars = 48;
    static constexpr float kFloor = -72.0f; // bars
    static constexpr float kMeterFloor = -60.0f;

    float bars[kBars], caps[kBars], capHold[kBars];
    float level[2], hold[2], holdTimer[2];
    bool clip[2] = { false, false };
    bool live = false;

    FrameFFT fft{ 2048 };
    std::vector<float> frame, mags;

    LiveAnalyzer() : frame(2048) {
        std::fill(std::begin(bars), std::end(bars), kFloor);
        std::fill(std::begin(caps), std::end(caps), kFloor);
        std::fill(std::begin(capHold), std::end(capHold), 0.0f);
        std::fill(std::begin(level), std::end(level), kMeterFloor);
        std::fill(std::begin(hold), std::end(hold), kMeterFloor);
        std::fill(std::begin(holdTimer), std::end(holdTimer), 0.0f);
    }
};

// looks at the audio around the playhead, when not playing everything just falls back down
static void updateAnalyzer(LiveAnalyzer& a, Whisper& audio, float playheadTime, bool playing, float dt) {
    float target[LiveAnalyzer::kBars];
    std::fill(std::begin(target), std::end(target), LiveAnalyzer::kFloor);
    float meterTarget[2] = { LiveAnalyzer::kMeterFloor, LiveAnalyzer::kMeterFloor };

    WavHeader h = audio.getHeader();
    int channels = std::max<int>(1, h.numChannels);
    int frames = audio.getNumOfS() / channels;
    const short* d = audio.getDataPointer();
    a.live = playing && d && frames > 0 && h.sampleRate > 0;

    if (a.live) {
        int n = a.fft.getSize();
        int center = (int)(playheadTime * h.sampleRate);
        for (int i = 0; i < n; i++) {
            int f = center - n / 2 + i;
            float sum = 0.0f;
            if (f >= 0 && f < frames)
                for (int c = 0; c < channels; c++) sum += d[f * channels + c];
            a.frame[i] = sum / channels;
        }
        a.fft.magnitudes(a.frame.data(), a.mags);

        // a full scale sine through a hann window peaks at 32768 * n / 4
        const float fullScale = 32768.0f * n / 4.0f;
        const float fLow = 30.0f, fHigh = std::min(16000.0f, h.sampleRate * 0.5f);
        double binsPerHz = (double)n / h.sampleRate;
        for (int b = 0; b < LiveAnalyzer::kBars; b++) {
            double f0 = fLow * std::pow(fHigh / fLow, (double)b / LiveAnalyzer::kBars);
            double f1 = fLow * std::pow(fHigh / fLow, (double)(b + 1) / LiveAnalyzer::kBars);
            float m = bandMagnitude(a.mags, f0 * binsPerHz, f1 * binsPerHz);
            target[b] = std::max(LiveAnalyzer::kFloor, 20.0f * std::log10(m / fullScale + 1e-9f));
        }

        // meters use the last 50ms before the playhead
        int window = std::max(1, h.sampleRate / 20);
        for (int c = 0; c < std::min(channels, 2); c++) {
            int peak = 0;
            for (int f = std::max(0, center - window); f < std::min(center, frames); f++) {
                int v = std::abs((int)d[f * channels + c]);
                peak = std::max(peak, v);
            }
            if (peak >= 32767) a.clip[c] = true;
            if (peak > 0) meterTarget[c] = std::max(LiveAnalyzer::kMeterFloor, 20.0f * std::log10(peak / 32768.0f));
        }
        if (channels == 1) meterTarget[1] = meterTarget[0];
    }

    for (int b = 0; b < LiveAnalyzer::kBars; b++) {
        // fast attack, slow release, like a hardware analyzer
        if (target[b] > a.bars[b]) a.bars[b] += (target[b] - a.bars[b]) * 0.6f;
        else a.bars[b] = std::max(target[b], a.bars[b] - 45.0f * dt);

        if (a.bars[b] >= a.caps[b]) { a.caps[b] = a.bars[b]; a.capHold[b] = 0.6f; }
        else if (a.capHold[b] > 0.0f) a.capHold[b] -= dt;
        else a.caps[b] = std::max(LiveAnalyzer::kFloor, a.caps[b] - 25.0f * dt);
    }
    for (int c = 0; c < 2; c++) {
        if (meterTarget[c] > a.level[c]) a.level[c] = meterTarget[c];
        else a.level[c] = std::max(meterTarget[c], a.level[c] - 30.0f * dt);

        if (a.level[c] >= a.hold[c]) { a.hold[c] = a.level[c]; a.holdTimer[c] = 1.2f; }
        else if (a.holdTimer[c] > 0.0f) a.holdTimer[c] -= dt;
        else a.hold[c] = std::max(LiveAnalyzer::kMeterFloor, a.hold[c] - 20.0f * dt);
    }
}

static void drawAnalyzerStrip(LiveAnalyzer& a, const AudioStats& stats, int channels, float height) {
    const Theme& t = g_theme;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImVec2 p1 = p0 + ImVec2(width, height);
    dl->AddRectFilled(p0, p1, t.canvasBg, 6.0f);
    dl->AddRect(p0, p1, t.laneBorder, 6.0f);

    const float pad = 12.0f;
    const float statsW = 190.0f;
    const int meterCount = std::min(std::max(channels, 1), 2);
    const float meterW = 14.0f, meterGap = 5.0f, scaleW = 30.0f;
    const float metersW = meterCount * meterW + (meterCount - 1) * meterGap + scaleW;

    // header
    ImU32 dim = t.gridText;
    dl->AddText(p0 + ImVec2(pad, 8), dim, "ANALYZER");
    if (a.live) {
        float x = p0.x + pad + ImGui::CalcTextSize("ANALYZER").x + 12;
        dl->AddCircleFilled(ImVec2(x, p0.y + 8 + ImGui::GetFontSize() * 0.5f), 4.0f, t.playhead);
        dl->AddText(ImVec2(x + 8, p0.y + 8), t.playhead, "LIVE");
    }

    // spectrum bars
    ImVec2 b0(p0.x + pad, p0.y + 32);
    ImVec2 b1(p1.x - pad - statsW - pad - metersW - pad * 1.5f, p1.y - 24);
    float barsW = b1.x - b0.x, barsH = b1.y - b0.y;
    if (barsW > 60.0f && barsH > 20.0f) {
        auto dbToY = [&](float db) { return b1.y - (1.0f - db / LiveAnalyzer::kFloor) * barsH; };
        float slot = barsW / LiveAnalyzer::kBars;
        ImVec4 acc = t.accent;
        ImU32 top = ImGui::GetColorU32(acc);
        ImU32 bottom = ImGui::GetColorU32(withAlpha(acc, 0.25f));
        for (int b = 0; b < LiveAnalyzer::kBars; b++) {
            float x0 = b0.x + b * slot + 1.0f, x1 = b0.x + (b + 1) * slot - 1.0f;
            float y = dbToY(a.bars[b]);
            dl->AddRectFilled(ImVec2(x0, b0.y), ImVec2(x1, b1.y), t.grid); // empty slot
            if (y < b1.y) dl->AddRectFilledMultiColor(ImVec2(x0, y), ImVec2(x1, b1.y), top, top, bottom, bottom);
            float cy = dbToY(a.caps[b]);
            if (cy < b1.y - 1) dl->AddRectFilled(ImVec2(x0, cy - 2), ImVec2(x1, cy), ImGui::GetColorU32(ImGuiCol_Text));
        }
        // frequency labels matching the 30 Hz..16 kHz log spacing
        const float marks[] = { 100.0f, 1000.0f, 10000.0f };
        const char* markLabels[] = { "100", "1k", "10k" };
        for (int m = 0; m < 3; m++) {
            float x = b0.x + std::log(marks[m] / 30.0f) / std::log(16000.0f / 30.0f) * barsW;
            dl->AddText(ImVec2(x - ImGui::CalcTextSize(markLabels[m]).x * 0.5f, b1.y + 3), dim, markLabels[m]);
        }
    }

    // level meters, drawn as LED segments
    float mx = p1.x - pad - statsW - pad - metersW;
    ImVec2 m0(mx, p0.y + 32), m1(mx, p1.y - 24);
    float meterH = m1.y - m0.y;
    const float segH = 3.0f, segGap = 1.0f;
    int segments = std::max(1, (int)(meterH / (segH + segGap)));
    auto meterY = [&](float db) { return m1.y - (1.0f - db / LiveAnalyzer::kMeterFloor) * meterH; };
    const char* meterNames[2] = { meterCount == 1 ? "M" : "L", "R" };
    for (int c = 0; c < meterCount; c++) {
        float x0 = mx + c * (meterW + meterGap), x1 = x0 + meterW;
        int holdSeg = (int)((1.0f - a.hold[c] / LiveAnalyzer::kMeterFloor) * segments);
        for (int s = 0; s < segments; s++) {
            float segDb = LiveAnalyzer::kMeterFloor * (1.0f - (s + 0.5f) / segments);
            ImVec4 col = segDb > -3.0f ? ImVec4(1.0f, 0.30f, 0.30f, 1.0f)
                       : segDb > -12.0f ? ImVec4(1.0f, 0.80f, 0.25f, 1.0f)
                       : ImVec4(0.30f, 0.90f, 0.50f, 1.0f);
            bool lit = segDb <= a.level[c] || s == holdSeg - 1;
            float y1 = m1.y - s * (segH + segGap);
            dl->AddRectFilled(ImVec2(x0, y1 - segH), ImVec2(x1, y1), ImGui::GetColorU32(withAlpha(col, lit ? 1.0f : 0.10f)));
        }
        // clip light, click to reset
        ImVec2 c0(x0, p0.y + 12), c1(x1, p0.y + 22);
        dl->AddRectFilled(c0, c1, a.clip[c] ? IM_COL32(255, 60, 60, 255) : t.grid, 2.0f);
        ImGui::SetCursorScreenPos(c0);
        ImGui::PushID(c);
        if (ImGui::InvisibleButton("##clip", c1 - c0)) a.clip[c] = false;
        if (a.clip[c]) ImGui::SetItemTooltip("Clipped! Click to reset");
        ImGui::PopID();
        dl->AddText(ImVec2(x0 + (meterW - ImGui::CalcTextSize(meterNames[c]).x) * 0.5f, m1.y + 3), dim, meterNames[c]);
    }
    float scaleX = mx + meterCount * meterW + (meterCount - 1) * meterGap + 6;
    const int scaleMarks[] = { 0, -12, -24, -36, -48 };
    for (int db : scaleMarks) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", db);
        float y = meterY((float)db);
        dl->AddText(ImVec2(scaleX, y - ImGui::GetFontSize() * 0.5f), dim, buf);
    }

    // file loudness numbers
    float sx = p1.x - pad - statsW;
    dl->AddLine(ImVec2(sx - pad * 0.5f, p0.y + 10), ImVec2(sx - pad * 0.5f, p1.y - 10), t.laneBorder);
    dl->AddText(ImVec2(sx + 4, p0.y + 8), dim, "FILE");
    auto row = [&](int i, const char* label, const std::string& value, ImU32 col) {
        float y = p0.y + 34 + i * (ImGui::GetFontSize() + 7);
        dl->AddText(ImVec2(sx + 4, y), dim, label);
        dl->AddText(ImVec2(p1.x - pad - ImGui::CalcTextSize(value.c_str()).x, y), col, value.c_str());
    };
    auto dbText = [](float db) {
        if (!std::isfinite(db)) return std::string("-inf dBFS");
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f dBFS", db);
        return std::string(buf);
    };
    ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    row(0, "Peak", dbText(stats.peakDb), stats.peakDb > -0.1f ? IM_COL32(255, 90, 90, 255) : text);
    row(1, "RMS", dbText(stats.rmsDb), text);
    char buf[32];
    if (std::isfinite(stats.peakDb) && std::isfinite(stats.rmsDb)) snprintf(buf, sizeof(buf), "%.1f dB", stats.peakDb - stats.rmsDb);
    else snprintf(buf, sizeof(buf), "-");
    row(2, "Crest", buf, text);
    snprintf(buf, sizeof(buf), "%d", stats.clipped);
    row(3, "Clipped", buf, stats.clipped > 0 ? IM_COL32(255, 90, 90, 255) : text);

    ImGui::SetCursorScreenPos(p0);
    ImGui::Dummy(ImVec2(width, height));
}


// WAVEFORM EDITOR

// min/max/rms per pixel column for every channel. Only rebuilt when the audio, the zoom or the size changes,
// scanning every sample each frame gets slow on long files
struct WaveformCache {
    int version = INT_MIN;
    double viewStart = -1, viewEnd = -1;
    int columns = 0, channels = 0;
    bool sparse = false; // less than 2 samples per column, drawn as a line instead of bars
    std::vector<float> minV, maxV, rmsV; // [channel * columns + column], -1..1
};

struct EditorView {
    double viewStart = 0.0, viewEnd = 1.0; // visible time range in seconds
    float dragAnchor = 0.0f;
    bool dragNoise = false;
    WaveformCache cache;
};

static void buildWaveformCache(WaveformCache& cache, Whisper& audio, int version, double viewStart, double viewEnd, int columns) {
    int channels = std::max<int>(1, audio.getHeader().numChannels);
    if (cache.version == version && cache.viewStart == viewStart && cache.viewEnd == viewEnd &&
        cache.columns == columns && cache.channels == channels) return;

    cache.version = version;
    cache.viewStart = viewStart;
    cache.viewEnd = viewEnd;
    cache.columns = columns;
    cache.channels = channels;
    cache.minV.assign((size_t)channels * columns, 0.0f);
    cache.maxV.assign((size_t)channels * columns, 0.0f);
    cache.rmsV.assign((size_t)channels * columns, 0.0f);

    const short* data = audio.getDataPointer();
    int frames = audio.getNumOfS() / channels;
    double sampleRate = audio.getHeader().sampleRate;
    if (!data || frames <= 0 || sampleRate <= 0) return;

    double framesPerCol = (viewEnd - viewStart) * sampleRate / columns;
    cache.sparse = framesPerCol < 2.0;

    for (int c = 0; c < channels; c++) {
        for (int col = 0; col < columns; col++) {
            size_t idx = (size_t)c * columns + col;
            if (cache.sparse) {
                // zoomed in far enough to see single samples, interpolate between them
                double pos = viewStart * sampleRate + (col + 0.5) * framesPerCol;
                int i0 = (int)std::floor(pos);
                if (i0 < 0 || i0 >= frames) continue;
                int i1 = std::min(i0 + 1, frames - 1);
                double frac = pos - i0;
                float v = (float)((1.0 - frac) * data[i0 * channels + c] + frac * data[i1 * channels + c]) / 32768.0f;
                cache.minV[idx] = cache.maxV[idx] = v;
                cache.rmsV[idx] = std::fabs(v);
                continue;
            }

            double f0 = viewStart * sampleRate + col * framesPerCol;
            int start = std::clamp((int)std::floor(f0), 0, frames);
            int end = std::clamp((int)std::floor(f0 + framesPerCol), 0, frames);
            if (start >= end) continue;

            short mn = 32767, mx = -32768;
            double sumSq = 0.0;
            for (int f = start; f < end; f++) {
                short s = data[f * channels + c];
                if (s < mn) mn = s;
                if (s > mx) mx = s;
                sumSq += (double)s * s;
            }
            cache.minV[idx] = mn / 32768.0f;
            cache.maxV[idx] = mx / 32768.0f;
            cache.rmsV[idx] = (float)std::sqrt(sumSq / (end - start)) / 32768.0f;
        }
    }
}

static void clampView(EditorView& v, double duration, double minSpan) {
    if (duration <= 0.0) { v.viewStart = 0.0; v.viewEnd = 1.0; return; }
    double span = std::clamp(v.viewEnd - v.viewStart, minSpan, duration);
    v.viewStart = std::clamp(v.viewStart, 0.0, duration - span);
    v.viewEnd = v.viewStart + span;
}

// picks a ruler step so labels are at least minPixels apart
static double niceTimeStep(double span, float width, float minPixels) {
    static const double steps[] = { 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5,
                                    1, 2, 5, 10, 15, 30, 60, 120, 300, 600 };
    for (double s : steps)
        if (s / span * width >= minPixels) return s;
    return 1200;
}

// ruler + channel lanes (or the spectrogram) + overview scrollbar. Handles drag selection, zoom and panning
static void timelineEditor(EditorView& v, Whisper& audio, int version, float duration,
                           float& cropStart, float& cropEnd, float& noiseStart, float& noiseEnd,
                           bool showNoise, float playheadTime, bool isPlaying, const SpectrogramImage* spec, int specKey) {
    const Theme& t = g_theme;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    WavHeader header = audio.getHeader();
    int channels = std::max<int>(1, header.numChannels);
    int sampleRate = std::max(1, header.sampleRate);
    double minSpan = std::min<double>(duration, std::max(0.002, 32.0 / sampleRate));

    const float rulerH = 26.0f, overviewH = 12.0f, gap = 10.0f;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 100.0f);
    avail.y = std::max(avail.y, rulerH + overviewH + gap + 60.0f);

    ImVec2 canvasMin = origin;
    ImVec2 canvasMax = origin + ImVec2(avail.x, avail.y - overviewH - gap);
    ImVec2 lanesMin(canvasMin.x, canvasMin.y + rulerH);
    ImVec2 lanesMax = canvasMax;
    float width = lanesMax.x - lanesMin.x;
    int columns = std::max(1, (int)width);

    // input first, so this frame already draws the new selection / zoom
    ImGui::SetCursorScreenPos(canvasMin);
    ImGui::InvisibleButton("##wavecanvas", canvasMax - canvasMin);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    clampView(v, duration, minSpan);
    double span = v.viewEnd - v.viewStart;
    auto xToTime = [&](float x) { return v.viewStart + (double)(x - lanesMin.x) / width * span; };

    if (ImGui::IsItemActivated()) {
        v.dragAnchor = (float)std::clamp(xToTime(io.MousePos.x), 0.0, (double)duration);
        v.dragNoise = io.KeyShift;
    }
    if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
        // scroll the view when dragging past either edge
        if (io.MousePos.x > lanesMax.x) { v.viewStart += span * 0.02; v.viewEnd += span * 0.02; }
        if (io.MousePos.x < lanesMin.x) { v.viewStart -= span * 0.02; v.viewEnd -= span * 0.02; }
        clampView(v, duration, minSpan);

        float now = (float)std::clamp(xToTime(io.MousePos.x), 0.0, (double)duration);
        float a = std::min(v.dragAnchor, now), b = std::max(v.dragAnchor, now);
        if (v.dragNoise) { noiseStart = a; noiseEnd = b; }
        else { cropStart = a; cropEnd = b; }
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        cropStart = 0.0f;
        cropEnd = duration;
    }
    float wheel = io.MouseWheel != 0.0f ? io.MouseWheel : io.MouseWheelH;
    if (hovered && wheel != 0.0f) {
        if (io.KeyShift) {
            v.viewStart -= wheel * span * 0.1;
            v.viewEnd = v.viewStart + span;
        }
        else {
            // zoom around the mouse so the point under the cursor stays put
            double anchor = xToTime(io.MousePos.x);
            double newSpan = std::clamp(span * std::pow(0.8, (double)wheel), minSpan, (double)duration);
            v.viewStart = anchor - (anchor - v.viewStart) * (newSpan / span);
            v.viewEnd = v.viewStart + newSpan;
        }
    }
    // page along with the playhead while playing
    if (isPlaying && playheadTime >= 0.0f && !active && (playheadTime > v.viewEnd || playheadTime < v.viewStart)) {
        double s = v.viewEnd - v.viewStart;
        v.viewStart = playheadTime - s * 0.05;
        v.viewEnd = v.viewStart + s;
    }
    clampView(v, duration, minSpan);
    span = v.viewEnd - v.viewStart;
    auto timeToX = [&](double tm) { return lanesMin.x + (float)((tm - v.viewStart) / span * width); };

    // background + ruler
    dl->AddRectFilled(canvasMin, canvasMax, t.canvasBg, 6.0f);
    dl->AddRectFilled(canvasMin, ImVec2(canvasMax.x, lanesMin.y), t.rulerBg, 6.0f, ImDrawFlags_RoundCornersTop);
    dl->AddLine(ImVec2(canvasMin.x, lanesMin.y), ImVec2(canvasMax.x, lanesMin.y), t.laneBorder);
    dl->PushClipRect(canvasMin, canvasMax, true);

    // spectrogram goes under the grid lines
    if (spec) {
        if (spec->valid) {
            float u0 = (float)(v.viewStart / duration), u1 = (float)(v.viewEnd / duration);
            dl->AddImage(toTextureID(spec->texture), lanesMin, lanesMax, ImVec2(u0, 0), ImVec2(u1, 1));
            float logRange = std::log(spec->fMax / spec->fMin);
            const float marks[] = { 100.0f, 1000.0f, 10000.0f };
            const char* markLabels[] = { "100 Hz", "1 kHz", "10 kHz" };
            for (int m = 0; m < 3; m++) {
                if (marks[m] >= spec->fMax) continue;
                float y = lanesMax.y - std::log(marks[m] / spec->fMin) / logRange * (lanesMax.y - lanesMin.y);
                dl->AddLine(ImVec2(lanesMin.x, y), ImVec2(lanesMin.x + 10, y), IM_COL32(255, 255, 255, 170));
                shadowText(dl, ImVec2(lanesMin.x + 14, y - ImGui::GetFontSize() * 0.5f), IM_COL32(255, 255, 255, 210), markLabels[m]);
            }
        }
        const char* note = nullptr;
        if (spec->busy()) note = "Rendering spectrogram...";
        else if (spec->key == specKey && !spec->valid) note = "File too short for a spectrogram";
        if (note) {
            ImVec2 size = ImGui::CalcTextSize(note);
            ImVec2 pos = spec->valid ? ImVec2(lanesMax.x - size.x - 12, lanesMin.y + 8)
                                     : (lanesMin + lanesMax) * 0.5f - size * 0.5f;
            shadowText(dl, pos, IM_COL32(255, 255, 255, 230), note);
        }
    }

    double step = niceTimeStep(span, width, 90.0f);
    int decimals = step < 0.001 ? 4 : step < 0.01 ? 3 : step < 0.1 ? 2 : step < 1.0 ? 1 : 0;
    double minor = step / 5.0;
    for (long long i = (long long)std::floor(v.viewStart / minor); i * minor <= v.viewEnd; i++) {
        float x = timeToX(i * minor);
        if (i % 5 == 0) {
            dl->AddLine(ImVec2(x, lanesMin.y - 9), ImVec2(x, lanesMin.y), t.gridText);
            if (!spec) dl->AddLine(ImVec2(x, lanesMin.y), ImVec2(x, lanesMax.y), t.grid);
            dl->AddText(ImVec2(x + 4, canvasMin.y + 3), t.gridText, formatTime(i * minor, decimals).c_str());
        }
        else {
            dl->AddLine(ImVec2(x, lanesMin.y - 4), ImVec2(x, lanesMin.y), t.laneBorder);
        }
    }

    // channel lanes
    if (!spec) {
        buildWaveformCache(v.cache, audio, version, v.viewStart, v.viewEnd, columns);
        float laneH = (lanesMax.y - lanesMin.y) / channels;
        for (int c = 0; c < channels; c++) {
            float top = lanesMin.y + c * laneH;
            float mid = top + laneH * 0.5f;
            float half = laneH * 0.5f - 6.0f;
            if (c > 0) dl->AddLine(ImVec2(lanesMin.x, top), ImVec2(lanesMax.x, top), t.laneBorder);
            dl->AddLine(ImVec2(lanesMin.x, mid), ImVec2(lanesMax.x, mid), t.centerLine);

            const float* mn = &v.cache.minV[(size_t)c * columns];
            const float* mx = &v.cache.maxV[(size_t)c * columns];
            const float* rms = &v.cache.rmsV[(size_t)c * columns];
            if (v.cache.sparse) {
                std::vector<ImVec2> points(columns);
                for (int col = 0; col < columns; col++)
                    points[col] = ImVec2(lanesMin.x + col + 0.5f, mid - mn[col] * half);
                dl->AddPolyline(points.data(), columns, t.waveRms, ImDrawFlags_None, 1.5f);
            }
            else {
                for (int col = 0; col < columns; col++) {
                    float x = lanesMin.x + col;
                    float yTop = mid - mx[col] * half;
                    float yBottom = std::max(mid - mn[col] * half, yTop + 1.0f);
                    dl->AddRectFilled(ImVec2(x, yTop), ImVec2(x + 1, yBottom), t.wavePeak);
                    // brighter rms band inside the peaks, the classic two tone DAW look
                    float r = rms[col] * half;
                    float rTop = std::max(mid - r, yTop), rBottom = std::min(mid + r, yBottom);
                    if (rBottom > rTop) dl->AddRectFilled(ImVec2(x, rTop), ImVec2(x + 1, rBottom), t.waveRms);
                }
            }

            char name[16];
            if (channels == 1) snprintf(name, sizeof(name), "MONO");
            else if (channels == 2) snprintf(name, sizeof(name), c == 0 ? "L" : "R");
            else snprintf(name, sizeof(name), "CH %d", c + 1);
            dl->AddText(ImVec2(lanesMin.x + 8, top + 5), t.gridText, name);
        }
    }
    else {
        shadowText(dl, ImVec2(lanesMin.x + 8, lanesMin.y + 5), IM_COL32(255, 255, 255, 210), "MIX");
    }

    // noise sample region
    if (showNoise && noiseEnd > noiseStart) {
        float x0 = timeToX(noiseStart), x1 = timeToX(noiseEnd);
        dl->AddRectFilled(ImVec2(x0, lanesMin.y), ImVec2(x1, lanesMax.y), t.noiseFill);
        dl->AddLine(ImVec2(x0, lanesMin.y), ImVec2(x0, lanesMax.y), t.noiseEdge, 1.0f);
        dl->AddLine(ImVec2(x1, lanesMin.y), ImVec2(x1, lanesMax.y), t.noiseEdge, 1.0f);
        dl->AddText(ImVec2(x0 + 5, lanesMax.y - ImGui::GetFontSize() - 4), t.noiseEdge, "NOISE");
    }

    // crop / playback selection
    if (cropEnd > cropStart) {
        float x0 = timeToX(cropStart), x1 = timeToX(cropEnd);
        dl->AddRectFilled(ImVec2(x0, canvasMin.y), ImVec2(x1, lanesMax.y), t.selFill);
        dl->AddLine(ImVec2(x0, canvasMin.y), ImVec2(x0, lanesMax.y), t.selEdge, 1.5f);
        dl->AddLine(ImVec2(x1, canvasMin.y), ImVec2(x1, lanesMax.y), t.selEdge, 1.5f);
        dl->AddTriangleFilled(ImVec2(x0, lanesMin.y - 8), ImVec2(x0 + 7, lanesMin.y - 8), ImVec2(x0, lanesMin.y - 1), t.selEdge);
        dl->AddTriangleFilled(ImVec2(x1, lanesMin.y - 8), ImVec2(x1 - 7, lanesMin.y - 8), ImVec2(x1, lanesMin.y - 1), t.selEdge);
    }

    // hover cursor with its time shown in the ruler
    if (hovered && !active) {
        float x = io.MousePos.x;
        dl->AddLine(ImVec2(x, lanesMin.y), ImVec2(x, lanesMax.y), t.hoverLine);
        std::string label = formatTime(xToTime(x), 3);
        ImVec2 size = ImGui::CalcTextSize(label.c_str());
        ImVec2 boxMin(x + 6, canvasMin.y + 3), boxMax = boxMin + size + ImVec2(8, 0);
        dl->AddRectFilled(boxMin, boxMax, ImGui::GetColorU32(ImGuiCol_FrameBgHovered), 3.0f);
        dl->AddText(boxMin + ImVec2(4, 0), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    }

    // playhead
    if (playheadTime >= 0.0f) {
        float x = timeToX(playheadTime);
        dl->AddLine(ImVec2(x, canvasMin.y), ImVec2(x, lanesMax.y), t.playhead, 1.5f);
        dl->AddTriangleFilled(ImVec2(x - 6, canvasMin.y), ImVec2(x + 6, canvasMin.y), ImVec2(x, canvasMin.y + 8), t.playhead);
    }

    dl->PopClipRect();
    dl->AddRect(canvasMin, canvasMax, t.laneBorder, 6.0f);

    // overview bar: shows which part of the file is visible, drag it to scroll
    ImVec2 ovMin(canvasMin.x, canvasMax.y + gap);
    ImVec2 ovMax(canvasMax.x, ovMin.y + overviewH);
    float ovW = ovMax.x - ovMin.x;
    auto ovX = [&](double tm) { return ovMin.x + (float)(tm / duration * ovW); };

    ImGui::SetCursorScreenPos(ovMin);
    ImGui::InvisibleButton("##overview", ovMax - ovMin);
    if (ImGui::IsItemActivated() && (io.MousePos.x < ovX(v.viewStart) || io.MousePos.x > ovX(v.viewEnd))) {
        // clicked next to the thumb, jump there
        double s = v.viewEnd - v.viewStart;
        v.viewStart = (io.MousePos.x - ovMin.x) / ovW * duration - s * 0.5;
        v.viewEnd = v.viewStart + s;
    }
    else if (ImGui::IsItemActive() && io.MouseDelta.x != 0.0f) {
        double d = io.MouseDelta.x / ovW * duration;
        v.viewStart += d;
        v.viewEnd += d;
    }
    bool ovHot = ImGui::IsItemHovered() || ImGui::IsItemActive();
    clampView(v, duration, minSpan);

    dl->AddRectFilled(ovMin, ovMax, ImGui::GetColorU32(ImGuiCol_FrameBg), overviewH * 0.5f);
    if (cropEnd > cropStart)
        dl->AddRectFilled(ImVec2(ovX(cropStart), ovMin.y), ImVec2(ovX(cropEnd), ovMax.y), t.selFill);
    float thumb0 = ovX(v.viewStart), thumb1 = std::max(ovX(v.viewEnd), thumb0 + 10.0f);
    dl->AddRectFilled(ImVec2(thumb0, ovMin.y + 2), ImVec2(thumb1, ovMax.y - 2),
        ImGui::GetColorU32(ovHot ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab), overviewH * 0.5f);
    if (playheadTime >= 0.0f) {
        float x = ovX(playheadTime);
        dl->AddLine(ImVec2(x, ovMin.y), ImVec2(x, ovMax.y), t.playhead, 1.5f);
    }
}


// SPECTRUM VIEW

// magnitude spectrum on a log frequency axis in dB, like the analyzers in most audio tools
static void spectrumView(Spectrum& spec) {
    const Theme& t = g_theme;
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    avail.x = std::max(avail.x, 100.0f);
    avail.y = std::max(avail.y, 100.0f);
    ImVec2 p1 = p0 + avail;
    ImGui::InvisibleButton("##spectrum", avail);
    bool hovered = ImGui::IsItemHovered();

    dl->AddRectFilled(p0, p1, t.canvasBg, 6.0f);
    dl->AddRect(p0, p1, t.laneBorder, 6.0f);

    Complex* bins = spec.getComplexData();
    int numBins = spec.getNumOfB();
    int sampleRate = spec.getSampleRate();
    int usable = numBins / 2;
    if (!bins || usable < 2 || sampleRate <= 0) return;

    std::vector<float> mags(usable);
    float peak = 0.0f;
    for (int i = 0; i < usable; i++) {
        mags[i] = (float)bins[i].magnitude();
        if (i > 0) peak = std::max(peak, mags[i]);
    }
    if (peak <= 0.0f) return;

    const float fMin = 20.0f, fMax = sampleRate * 0.5f;
    if (fMax <= fMin) return;
    const float dbFloor = -90.0f;
    const float logRange = std::log(fMax / fMin);

    ImVec2 g0(p0.x + 52.0f, p0.y + 14.0f);
    ImVec2 g1(p1.x - 16.0f, p1.y - 28.0f);
    float gw = g1.x - g0.x, gh = g1.y - g0.y;
    auto freqToX = [&](float f) { return g0.x + std::log(f / fMin) / logRange * gw; };
    auto dbToY = [&](float db) { return g0.y + (db / dbFloor) * gh; };

    char buf[32];
    for (int db = 0; db >= (int)dbFloor; db -= 10) {
        float y = dbToY((float)db);
        dl->AddLine(ImVec2(g0.x, y), ImVec2(g1.x, y), t.grid);
        snprintf(buf, sizeof(buf), "%d dB", db);
        ImVec2 size = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(g0.x - size.x - 8, y - size.y * 0.5f), t.gridText, buf);
    }
    const float marks[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : marks) {
        if (f > fMax) continue;
        float x = freqToX(f);
        dl->AddLine(ImVec2(x, g0.y), ImVec2(x, g1.y), t.grid);
        if (f >= 1000) snprintf(buf, sizeof(buf), "%gk", f / 1000.0f);
        else snprintf(buf, sizeof(buf), "%g", f);
        ImVec2 size = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(x - size.x * 0.5f, g1.y + 6), t.gridText, buf);
    }

    int cols = std::max(1, (int)gw);
    double binsPerHz = (double)numBins / sampleRate;
    std::vector<ImVec2> points;
    std::vector<float> dbs;
    points.reserve(cols);
    dbs.reserve(cols);
    ImU32 fillTop = ImGui::GetColorU32(withAlpha(t.accent, 0.45f));
    ImU32 fillBottom = ImGui::GetColorU32(withAlpha(t.accent, 0.03f));

    for (int col = 0; col < cols; col++) {
        double ba = fMin * std::exp(logRange * col / cols) * binsPerHz;
        double bb = fMin * std::exp(logRange * (col + 1) / cols) * binsPerHz;
        float mag = bandMagnitude(mags, ba, bb);
        float db = mag > 0.0f ? 20.0f * std::log10(mag / peak) : dbFloor;
        db = std::clamp(db, dbFloor, 0.0f);
        float x = g0.x + col, y = dbToY(db);
        dl->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + 1, g1.y), fillTop, fillTop, fillBottom, fillBottom);
        points.push_back(ImVec2(x + 0.5f, y));
        dbs.push_back(db);
    }
    dl->AddPolyline(points.data(), (int)points.size(), t.waveRms, ImDrawFlags_None, 1.5f);

    snprintf(buf, sizeof(buf), "%d-point FFT, middle of the file", numBins);
    ImVec2 capSize = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(g1.x - capSize.x - 4, g0.y + 2), t.gridText, buf);

    // readout under the mouse
    if (hovered && io.MousePos.x >= g0.x && io.MousePos.x < g0.x + cols) {
        int col = (int)(io.MousePos.x - g0.x);
        float f = fMin * std::exp(logRange * col / cols);
        float x = g0.x + col, y = dbToY(dbs[col]);
        dl->AddLine(ImVec2(x, g0.y), ImVec2(x, g1.y), t.hoverLine);
        dl->AddCircleFilled(ImVec2(x, y), 4.0f, t.waveRms);
        if (f >= 1000.0f) snprintf(buf, sizeof(buf), "%.2f kHz   %.1f dB", f / 1000.0f, dbs[col]);
        else snprintf(buf, sizeof(buf), "%.0f Hz   %.1f dB", f, dbs[col]);
        ImVec2 size = ImGui::CalcTextSize(buf);
        ImVec2 boxMin(std::min(x + 10, g1.x - size.x - 10), g0.y + 24);
        dl->AddRectFilled(boxMin, boxMin + size + ImVec2(12, 6), ImGui::GetColorU32(ImGuiCol_FrameBgHovered), 4.0f);
        dl->AddText(boxMin + ImVec2(6, 3), ImGui::GetColorU32(ImGuiCol_Text), buf);
    }
}

// shown in the editor area before a file is loaded, returns true when the open button is pressed
static bool emptyState() {
    const Theme& t = g_theme;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 p1 = p0 + avail;
    dl->AddRectFilled(p0, p1, t.canvasBg, 6.0f);
    dl->AddRect(p0, p1, t.laneBorder, 6.0f);

    ImVec2 c = (p0 + p1) * 0.5f;
    // decorative waveform
    const int bars = 48;
    float barW = 4.0f, gap = 4.0f;
    float x = c.x - bars * (barW + gap) * 0.5f;
    for (int i = 0; i < bars; i++) {
        float h = 8.0f + 50.0f * std::fabs(std::sin(i * 0.37f) * std::cos(i * 0.11f));
        dl->AddRectFilled(ImVec2(x, c.y - 40 - h * 0.5f), ImVec2(x + barW, c.y - 40 + h * 0.5f), t.laneBorder, 2.0f);
        x += barW + gap;
    }

    const char* title = "No audio loaded";
    const char* sub = "Open an 8 or 16-bit PCM WAV file to start editing";
    float y = c.y + 20;
    ImGui::SetCursorScreenPos(ImVec2(c.x - ImGui::CalcTextSize(title).x * 0.5f, y));
    ImGui::TextUnformatted(title);
    ImGui::SetCursorScreenPos(ImVec2(c.x - ImGui::CalcTextSize(sub).x * 0.5f, y + 26));
    ImGui::TextDisabled("%s", sub);
    const float bw = 180.0f;
    ImGui::SetCursorScreenPos(ImVec2(c.x - bw * 0.5f, y + 64));
    return accentButton("Open WAV...", ImVec2(bw, 0));
}


// samples are stored interleaved (L R L R...) so the fft cant work on stereo directly
// this splits the audio into channels, runs the effect on each one separately and then puts them back
void applyPerChannel(Whisper& audio, const std::function<void(Spectrogram&, int)>& effect) {
    int numChannels = audio.getHeader().numChannels;
    if (numChannels < 1 || audio.getNumOfS() == 0) return;
    int channelLength = audio.getNumOfS() / numChannels;

    for (int c = 0; c < numChannels; c++) {
        Whisper channel = audio.extractChannel(c);
        Spectrogram sg = channel.decoupleSTFT(2048, 512);
        effect(sg, c);
        //cutting back to the original length so the audio doesnt get longer after every effect
        Whisper processed = sg.synthesize(channel.getHeader(), channelLength);
        audio.setChannel(c, processed);
    }
}


int main() {
    sf::RenderWindow window(sf::VideoMode(1280, 820), "Whisper Advanced Audio Editor");
    window.setFramerateLimit(60);

    if (!ImGui::SFML::Init(window)) return -1;

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // the layout is fixed so theres nothing worth saving

    // windows system fonts look a lot better than the built in pixel font, fall back to it if theyre missing
    ImFont* titleFont = nullptr;
    ImFont* monoFont = nullptr;
    auto fontExists = [](const char* path) { return std::ifstream(path).good(); };
    if (fontExists("C:\\Windows\\Fonts\\segoeui.ttf"))
        io.FontDefault = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f);
    if (fontExists("C:\\Windows\\Fonts\\segoeuib.ttf"))
        titleFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 19.0f);
    if (fontExists("C:\\Windows\\Fonts\\consola.ttf"))
        monoFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 24.0f);
    if (!ImGui::SFML::UpdateFontTexture()) return -1;

    bool isLightMode = false;
    applyTheme(isLightMode);

    // Track state of our audio data
    Whisper activeWhisper;   // B: the processed audio every edit works on
    Whisper originalWhisper; // A: the file exactly as it was loaded, for comparing
    bool showOriginal = false;
    Spectrum activeSpectrumDisplay;
    bool isFileLoaded = false;
    int audioVersion = 0; // bumped on every edit so the caches know to rebuild
    int loadCount = 0;
    std::string filePath, fileName;
    std::string loadError, noiseError;

    // undo keeps whole copies of the audio, so only a few steps
    std::deque<Whisper> undoStack, redoStack;
    std::vector<std::string> editLog, redoLog; // names of the edits, for the before/after image
    const size_t maxUndo = 10;

    std::string statusMessage;
    sf::Clock statusClock;

    float maxDuration = 10.0f;
    float cropStart = 0.0f, cropEnd = 10.0f;
    float noiseStart = 0.0f, noiseEnd = 0.5f;
    bool showNoise = true;
    float noiseStrength = 1.0f;
    float threshold = 0.0f;
    float gainFactor = 1.0f;
    int filterMode = 0; // 0 = low pass, 1 = high pass, 2 = band pass
    float cutOff = 1000.0f, bpLow = 500.0f, bpHigh = 2000.0f;
    float eqGains[10] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    char exportPath[256] = "output.wav";

    int displayMode = 0; // 0 = waveform, 1 = spectrogram, 2 = spectrum
    EditorView view;
    SpectrogramImage processedSpec, originalSpec;
    int spectrumKey = INT_MIN;
    AudioStats stats;
    LiveAnalyzer analyzer;
    bool beforeAfterPending = false;
    bool snapshotRequested = false;

    // playback runs from memory so A/B can swap the audio without a temp file
    sf::SoundBuffer previewBuffer;
    sf::Sound player;
    bool loopPlayback = false;
    float playStart = 0.0f, playEnd = 0.0f; // the range thats loaded into the player

    auto originalKey = [&]() { return -2 - loadCount; }; // never equal to an audioVersion
    auto shown = [&]() -> Whisper& { return showOriginal ? originalWhisper : activeWhisper; };
    auto shownKey = [&]() { return showOriginal ? originalKey() : audioVersion; };

    auto setStatus = [&](const std::string& msg) { statusMessage = msg; statusClock.restart(); };
    auto markModified = [&]() { audioVersion++; };
    auto pushUndo = [&]() {
        undoStack.push_back(activeWhisper);
        if (undoStack.size() > maxUndo) undoStack.pop_front();
        redoStack.clear();
        redoLog.clear();
    };
    // every destructive edit goes through here so it can be undone
    auto applyEdit = [&](const std::string& name, const std::function<void()>& edit) {
        if (!isFileLoaded) return;
        showOriginal = false; // edits always go to B, so show B
        pushUndo();
        edit();
        editLog.push_back(name);
        markModified();
        setStatus("Applied: " + name);
    };
    auto undo = [&]() {
        if (undoStack.empty()) return;
        redoStack.push_back(activeWhisper);
        activeWhisper = undoStack.back();
        undoStack.pop_back();
        if (!editLog.empty()) { redoLog.push_back(editLog.back()); editLog.pop_back(); }
        showOriginal = false;
        markModified();
        setStatus("Undo");
    };
    auto redo = [&]() {
        if (redoStack.empty()) return;
        undoStack.push_back(activeWhisper);
        activeWhisper = redoStack.back();
        redoStack.pop_back();
        if (!redoLog.empty()) { editLog.push_back(redoLog.back()); redoLog.pop_back(); }
        showOriginal = false;
        markModified();
        setStatus("Redo");
    };

    auto loadFile = [&](const std::string& path) {
        // loading into a temp one first so a bad file doesnt delete the audio thats already loaded
        Whisper loadedWhisper(path.c_str());
        if (loadedWhisper.getNumOfS() <= 0) {
            loadError = "Could not load file (only 8 or 16 bit PCM wav files work)";
            return;
        }
        player.stop();
        activeWhisper = loadedWhisper;
        originalWhisper = loadedWhisper;
        showOriginal = false;
        isFileLoaded = true;
        loadCount++;
        filePath = path;
        fileName = path.substr(path.find_last_of("/\\") + 1);
        undoStack.clear();
        redoStack.clear();
        editLog.clear();
        redoLog.clear();
        maxDuration = (float)activeWhisper.getDurationInSeconds();
        cropStart = 0.0f;
        cropEnd = maxDuration;
        noiseStart = 0.0f;
        noiseEnd = std::min(0.5f, maxDuration);
        view.viewStart = 0.0;
        view.viewEnd = maxDuration;
        loadError = "";
        markModified();
        setStatus("Loaded " + fileName);
    };

    auto dialogDir = [&]() {
        size_t slash = filePath.find_last_of("/\\");
        return slash == std::string::npos ? std::string(".") : filePath.substr(0, slash);
    };
    auto openLoadDialog = [&]() {
        IGFD::FileDialogConfig config;
        config.path = dialogDir();
        ImGuiFileDialog::Instance()->OpenDialog("ChooseAudioDlg", "Open WAV File", ".wav", config);
    };
    auto openSaveDialog = [&]() {
        IGFD::FileDialogConfig config;
        config.path = dialogDir();
        config.fileName = exportPath;
        config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
        ImGuiFileDialog::Instance()->OpenDialog("SaveAudioDlg", "Export As", ".wav", config);
    };

    auto loadPreview = [&](float start, float end) {
        Whisper chunk = shown().hardSplice((double)start, (double)end);
        if (chunk.getNumOfS() <= 0) return false;
        WavHeader h = chunk.getHeader();
        player.stop();
        player.resetBuffer();
        if (!previewBuffer.loadFromSamples(chunk.getDataPointer(), (sf::Uint64)chunk.getNumOfS(),
                (unsigned)std::max<int>(1, h.numChannels), (unsigned)h.sampleRate)) return false;
        player.setBuffer(previewBuffer);
        player.setLoop(loopPlayback);
        playStart = start;
        playEnd = end;
        return true;
    };
    auto startPlayback = [&]() {
        if (!isFileLoaded) return;
        if (player.getStatus() == sf::Sound::Paused) {
            player.play();
            return;
        }
        if (loadPreview(cropStart, cropEnd)) player.play();
    };
    auto togglePlayback = [&]() {
        if (player.getStatus() == sf::Sound::Playing) player.pause();
        else startPlayback();
    };
    auto toggleLoop = [&]() {
        loopPlayback = !loopPlayback;
        player.setLoop(loopPlayback);
        setStatus(loopPlayback ? "Loop on" : "Loop off");
    };
    // swaps between original and processed without stopping, so the difference is easy to hear
    auto setABMode = [&](bool original) {
        if (!isFileLoaded || showOriginal == original) return;
        sf::Sound::Status status = player.getStatus();
        sf::Time offset = player.getPlayingOffset();
        showOriginal = original;
        if (status != sf::Sound::Stopped && loadPreview(playStart, playEnd)) {
            player.play();
            player.setPlayingOffset(offset);
            if (status == sf::Sound::Paused) player.pause();
        }
        setStatus(original ? "A: original audio" : "B: processed audio");
    };

    auto exportAudio = [&](bool selectionOnly) {
        if (!isFileLoaded) return;
        if (selectionOnly) activeWhisper.hardSplice((double)cropStart, (double)cropEnd).save(exportPath);
        else activeWhisper.save(exportPath);
        setStatus(std::string("Exported ") + exportPath);
    };

    sf::Clock deltaClock;
    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::Resized)
                window.setView(sf::View(sf::FloatRect(0.0f, 0.0f, (float)event.size.width, (float)event.size.height)));
        }

        sf::Time dt = deltaClock.restart();
        ImGui::SFML::Update(window, dt);

        // background work: spectrograms, stats, the static spectrum
        pollSpectrogram(processedSpec);
        pollSpectrogram(originalSpec);
        if (isFileLoaded) {
            if ((displayMode == 1 && !showOriginal) || beforeAfterPending) requestSpectrogram(processedSpec, activeWhisper, audioVersion);
            if ((displayMode == 1 && showOriginal) || beforeAfterPending) requestSpectrogram(originalSpec, originalWhisper, originalKey());
            computeStats(stats, shown(), shownKey());
            if (spectrumKey != shownKey()) {
                activeSpectrumDisplay = shown().decoupleWindow(4096);
                spectrumKey = shownKey();
            }
        }
        if (beforeAfterPending && processedSpec.key == audioVersion && originalSpec.key == originalKey() &&
            !processedSpec.busy() && !originalSpec.busy()) {
            beforeAfterPending = false;
            if (processedSpec.valid && originalSpec.valid) {
                std::wstring subtitle(fileName.begin(), fileName.end());
                if (editLog.empty()) subtitle += L"   |   no processing applied yet";
                else {
                    subtitle += L"   |   ";
                    for (size_t i = 0; i < editLog.size(); i++) {
                        if (i > 0) subtitle += L"  \u2192  ";
                        subtitle += std::wstring(editLog[i].begin(), editLog[i].end());
                    }
                }
                std::string path = timestampedName("whisper_before_after_", ".png");
                if (exportBeforeAfter(originalSpec, processedSpec, L"Before / After", subtitle, path)) setStatus("Saved " + path);
                else setStatus("Could not save the before/after image");
            }
            else setStatus("File too short for a spectrogram");
        }

        // keyboard shortcuts
        bool dialogOpen = ImGuiFileDialog::Instance()->IsOpened("ChooseAudioDlg") ||
                          ImGuiFileDialog::Instance()->IsOpened("SaveAudioDlg");
        if (!io.WantTextInput && !dialogOpen) {
            if (isFileLoaded && ImGui::IsKeyPressed(ImGuiKey_Space, false)) togglePlayback();
            if (isFileLoaded && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) setABMode(!showOriginal);
            if (isFileLoaded && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L, false)) toggleLoop();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) redo(); else undo(); }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) openLoadDialog();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F12, false)) snapshotRequested = true;

        bool isPlaying = player.getStatus() == sf::Sound::Playing;
        float playheadTime = -1.0f;
        if (isFileLoaded && player.getStatus() != sf::Sound::Stopped)
            playheadTime = playStart + player.getPlayingOffset().asSeconds();
        if (isFileLoaded) updateAnalyzer(analyzer, shown(), playheadTime, isPlaying, dt.asSeconds());

        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 disp = io.DisplaySize;
        const float topH = 48.0f, statusH = 22.0f, sideW = 350.0f;
        const ImGuiWindowFlags panelFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;

        // TOP BAR: file, undo, transport, time
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(disp.x, topH));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, style.Colors[ImGuiCol_ChildBg]);
        ImGui::Begin("##topbar", nullptr, panelFlags);
        ImGui::PopStyleColor();
        {
            drawEdge(ImVec2(0, topH - 1), ImVec2(disp.x, topH - 1));
            const float frameH = ImGui::GetFrameHeight();

            ImGui::SetCursorPos(ImVec2(16, (topH - 30) * 0.5f));
            drawLogo(30);
            ImGui::SameLine(0, 10);
            float titleH = (titleFont ? titleFont->FontSize : ImGui::GetFontSize()) + ImGui::GetFontSize();
            ImGui::SetCursorPosY((topH - titleH) * 0.5f);
            ImGui::BeginGroup();
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            if (titleFont) ImGui::PushFont(titleFont);
            ImGui::TextUnformatted("Whisper");
            if (titleFont) ImGui::PopFont();
            ImGui::TextDisabled("%s", isFileLoaded ? shorten(fileName, 22).c_str() : "No file loaded");
            ImGui::PopStyleVar();
            ImGui::EndGroup();
            if (isFileLoaded) ImGui::SetItemTooltip("%s", filePath.c_str());

            ImGui::SameLine(250);
            ImGui::SetCursorPosY((topH - frameH) * 0.5f);
            if (accentButton("Open...", ImVec2(0, 0))) openLoadDialog();
            ImGui::SetItemTooltip("Open a WAV file (Ctrl+O)");
            ImGui::SameLine();
            ImGui::BeginDisabled(undoStack.empty());
            if (ImGui::Button("Undo")) undo();
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Ctrl+Z");
            ImGui::SameLine();
            ImGui::BeginDisabled(redoStack.empty());
            if (ImGui::Button("Redo")) redo();
            ImGui::EndDisabled();
            ImGui::SetItemTooltip("Ctrl+Y");
            ImGui::SameLine();
            if (ImGui::Button("Snapshot")) snapshotRequested = true;
            ImGui::SetItemTooltip("Save a PNG of the whole window (F12)");

            // transport, centred in the window
            const float iconSize = 40.0f;
            float centerX = std::max(disp.x * 0.5f, 640.0f);
            ImGui::SameLine(centerX - iconSize * 1.5f - 8);
            ImGui::SetCursorPosY((topH - iconSize) * 0.5f);
            ImGui::BeginDisabled(!isFileLoaded);
            if (iconButton("##play", isPlaying ? Icon::Pause : Icon::Play, iconSize, true)) togglePlayback();
            ImGui::SetItemTooltip(isPlaying ? "Pause (Space)" : "Play selection (Space)");
            ImGui::SameLine(0, 8);
            if (iconButton("##stop", Icon::Stop, iconSize, false)) player.stop();
            ImGui::SetItemTooltip("Stop");
            ImGui::SameLine(0, 8);
            if (iconButton("##loop", Icon::Loop, iconSize, loopPlayback)) toggleLoop();
            ImGui::SetItemTooltip(loopPlayback ? "Loop is on (L)" : "Loop the selection (L)");
            ImGui::EndDisabled();

            ImGui::SameLine(0, 18);
            float monoH = monoFont ? monoFont->FontSize : ImGui::GetFontSize();
            ImGui::SetCursorPosY((topH - monoH) * 0.5f);
            if (monoFont) ImGui::PushFont(monoFont);
            ImGui::TextUnformatted(formatTime(playheadTime >= 0.0f ? playheadTime : cropStart, 3).c_str());
            if (monoFont) ImGui::PopFont();
            ImGui::SameLine(0, 8);
            ImGui::SetCursorPosY((topH - ImGui::GetFontSize()) * 0.5f + 2);
            ImGui::TextDisabled("/ %s", formatTime(isFileLoaded ? maxDuration : 0.0f, 3).c_str());

            float toggleW = ImGui::CalcTextSize("Light theme").x + frameH * 1.4f + style.ItemInnerSpacing.x;
            ImGui::SameLine(disp.x - toggleW - 20);
            ImGui::SetCursorPosY((topH - frameH) * 0.5f);
            if (toggleSwitch("Light theme", &isLightMode)) applyTheme(isLightMode);
        }
        ImGui::End();

        // SIDEBAR: tools
        ImGui::SetNextWindowPos(ImVec2(0, topH));
        ImGui::SetNextWindowSize(ImVec2(sideW, disp.y - topH - statusH));
        ImGui::Begin("##sidebar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
        {
            drawEdge(ImVec2(sideW - 1, topH), ImVec2(sideW - 1, disp.y - statusH));
            errorText(loadError);

            if (ImGui::BeginTabBar("##tools")) {
                float half = 0.0f;
                auto halfWidth = [&]() { return (ImGui::GetContentRegionAvail().x - style.ItemSpacing.x) * 0.5f; };

                if (ImGui::BeginTabItem("Edit")) {
                    ImGui::BeginDisabled(!isFileLoaded);
                    sectionHeader("SELECTION");
                    ImGui::PushItemWidth(-60);
                    // Start cannot go past the end, end cannot go past the file length
                    ImGui::DragFloat("Start", &cropStart, 0.01f, 0.0f, cropEnd, "%.3f s");
                    ImGui::DragFloat("End", &cropEnd, 0.01f, cropStart, maxDuration, "%.3f s");
                    ImGui::PopItemWidth();
                    ImGui::TextDisabled("Length  %s", formatTime(cropEnd - cropStart, 3).c_str());
                    half = halfWidth();
                    if (ImGui::Button("Select All", ImVec2(half, 0))) { cropStart = 0.0f; cropEnd = maxDuration; }
                    ImGui::SameLine();
                    if (ImGui::Button("Zoom to Selection", ImVec2(half, 0))) { view.viewStart = cropStart; view.viewEnd = cropEnd; }
                    hint("Drag on the waveform to select. Double-click selects everything. Playback and export use the selection.");

                    sectionHeader("TRANSFORM");
                    half = halfWidth();
                    if (ImGui::Button("Reverse", ImVec2(half, 0)))
                        applyEdit("Reverse", [&]() { activeWhisper.reverse(); });
                    ImGui::SameLine();
                    if (ImGui::Button("Peak Normalize", ImVec2(half, 0)))
                        applyEdit("Peak normalize", [&]() { activeWhisper.peakNormalize(); });

                    sectionHeader("FADE");
                    half = halfWidth();
                    if (ImGui::Button("Fade In", ImVec2(half, 0)))
                        applyEdit("Fade in", [&]() { applyFade(activeWhisper, cropStart, cropEnd, true); });
                    ImGui::SameLine();
                    if (ImGui::Button("Fade Out", ImVec2(half, 0)))
                        applyEdit("Fade out", [&]() { applyFade(activeWhisper, cropStart, cropEnd, false); });
                    hint("Fades over the selected range.");

                    sectionHeader("GAIN");
                    ImGui::SetNextItemWidth(-70);
                    ImGui::SliderFloat("##gain", &gainFactor, 0.0f, 3.0f, "x %.2f");
                    ImGui::SameLine();
                    if (gainFactor > 0.0001f) ImGui::TextDisabled("%+.1f dB", 20.0f * std::log10(gainFactor));
                    else ImGui::TextDisabled("-inf dB");
                    if (accentButton("Apply Gain")) {
                        char name[32];
                        snprintf(name, sizeof(name), "Gain x%.2f", gainFactor);
                        applyEdit(name, [&]() { activeWhisper *= gainFactor; });
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Filter")) {
                    ImGui::BeginDisabled(!isFileLoaded);
                    sectionHeader("TYPE");
                    static const char* filterNames[] = { "Low-pass", "High-pass", "Band-pass" };
                    segmented("##filtermode", &filterMode, filterNames, 3, ImGui::GetContentRegionAvail().x);

                    sectionHeader("RESPONSE");
                    float bandLow = std::min(bpLow, bpHigh), bandHigh = std::max(bpLow, bpHigh);
                    responseGraph(100.0f, 1.3f, [&](float f) {
                        if (filterMode == 0) return f <= cutOff ? 1.0f : 0.0f;
                        if (filterMode == 1) return f >= cutOff ? 1.0f : 0.0f;
                        return (f >= bandLow && f <= bandHigh) ? 1.0f : 0.0f;
                    });

                    sectionHeader("FREQUENCY");
                    ImGui::PushItemWidth(-60);
                    if (filterMode == 2) {
                        ImGui::SliderFloat("Low", &bpLow, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
                        ImGui::SliderFloat("High", &bpHigh, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
                        bpLow = std::clamp(bpLow, 20.0f, 20000.0f);
                        bpHigh = std::clamp(bpHigh, 20.0f, 20000.0f);
                    }
                    else {
                        ImGui::SliderFloat("Cutoff", &cutOff, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
                        cutOff = std::clamp(cutOff, 20.0f, 20000.0f);
                    }
                    ImGui::PopItemWidth();
                    hint("Ctrl+click a slider to type an exact value.");

                    ImGui::Spacing();
                    if (accentButton("Apply Filter")) {
                        char name[48];
                        if (filterMode == 2) {
                            //if low ended up above high just swap them so the band still makes sense
                            float lo = std::min(bpLow, bpHigh);
                            float hi = std::max(bpLow, bpHigh);
                            snprintf(name, sizeof(name), "Band-pass %.0f-%.0f Hz", lo, hi);
                            applyEdit(name, [&]() {
                                applyPerChannel(activeWhisper, [&](Spectrogram& sg, int) {
                                    Filter filter = Filter::bandPass(sg.getFftSize(), sg.getSampleRate(), lo, hi);
                                    for (int f = 0; f < sg.getNumFrames(); f++)
                                        sg.getFrame(f).filterOn(filter);
                                });
                            });
                        }
                        else {
                            snprintf(name, sizeof(name), "%s %.0f Hz", filterMode == 0 ? "Low-pass" : "High-pass", cutOff);
                            applyEdit(name, [&]() {
                                applyPerChannel(activeWhisper, [&](Spectrogram& sg, int) {
                                    Filter filter = (filterMode == 0)
                                        ? Filter::lowPass(sg.getFftSize(), sg.getSampleRate(), cutOff)
                                        : Filter::highPass(sg.getFftSize(), sg.getSampleRate(), cutOff);
                                    for (int f = 0; f < sg.getNumFrames(); f++)
                                        sg.getFrame(f).filterOn(filter);
                                });
                            });
                        }
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("EQ")) {
                    ImGui::BeginDisabled(!isFileLoaded);
                    static const char* bandLabels[10] = { "31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
                    static const float bandFreqs[10] = { 31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };

                    sectionHeader("10-BAND GRAPHIC EQ");
                    // same band edges as Filter::tenBandEQ (halfway between centres, in octaves)
                    responseGraph(90.0f, 2.0f, [&](float f) {
                        for (int i = 0; i < 9; i++)
                            if (f <= 1.41421356f * bandFreqs[i]) return eqGains[i];
                        return eqGains[9];
                    });
                    ImGui::Spacing();

                    const float sliderSpacing = 6.0f;
                    float startX = ImGui::GetCursorPosX();
                    float sliderWidth = std::floor((ImGui::GetContentRegionAvail().x - 9 * sliderSpacing) / 10.0f);
                    for (int i = 0; i < 10; i++) {
                        ImGui::PushID(i);
                        ImGui::VSliderFloat("##eq", ImVec2(sliderWidth, 150), &eqGains[i], 0.0f, 2.0f, "");
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                            float db = eqGains[i] > 0.0001f ? 20.0f * std::log10(eqGains[i]) : -INFINITY;
                            ImGui::SetTooltip("%s Hz\nx %.2f  (%+.1f dB)", bandLabels[i], eqGains[i], db);
                        }
                        if (i < 9) ImGui::SameLine(0, sliderSpacing);
                        ImGui::PopID();
                    }
                    // labels centred under each slider
                    for (int i = 0; i < 10; i++) {
                        float labelWidth = ImGui::CalcTextSize(bandLabels[i]).x;
                        ImGui::SetCursorPosX(startX + i * (sliderWidth + sliderSpacing) + (sliderWidth - labelWidth) * 0.5f);
                        ImGui::TextDisabled("%s", bandLabels[i]);
                        if (i < 9) ImGui::SameLine(0, 0);
                    }
                    ImGui::Spacing();
                    half = halfWidth();
                    if (ImGui::Button("Flat", ImVec2(half, 0)))
                        std::fill(std::begin(eqGains), std::end(eqGains), 1.0f);
                    ImGui::SameLine();
                    if (accentButton("Apply EQ", ImVec2(half, 0))) {
                        applyEdit("10-band EQ", [&]() {
                            applyPerChannel(activeWhisper, [&](Spectrogram& sg, int) {
                                Filter eqFilter = Filter::tenBandEQ(sg.getFftSize(), sg.getSampleRate(), eqGains);
                                for (int f = 0; f < sg.getNumFrames(); f++)
                                    sg.getFrame(f).filterOn(eqFilter);
                            });
                        });
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Noise")) {
                    ImGui::BeginDisabled(!isFileLoaded);
                    sectionHeader("NOISE SAMPLE");
                    hint("Shift+drag on the waveform over a quiet part (only background noise) to mark it.");
                    ImGui::PushItemWidth(-60);
                    ImGui::DragFloat("Start##noise", &noiseStart, 0.01f, 0.0f, noiseEnd, "%.3f s");
                    ImGui::DragFloat("End##noise", &noiseEnd, 0.01f, noiseStart, maxDuration, "%.3f s");
                    ImGui::PopItemWidth();
                    toggleSwitch("Show on waveform", &showNoise);

                    sectionHeader("NOISE REDUCTION");
                    ImGui::SetNextItemWidth(-60);
                    ImGui::SliderFloat("Strength", &noiseStrength, 0.0f, 2.0f, "%.2f");
                    if (accentButton("Apply Noise Reduction")) {
                        double length = activeWhisper.getDurationInSeconds();
                        double nStart = noiseStart;
                        double nEnd = std::min((double)noiseEnd, length);//cant go past the end of the file
                        //the noise sample has to be at least one full fft window long or theres nothing to learn from
                        double minNoise = 2048.0 / activeWhisper.getHeader().sampleRate;
                        if (length < 1) {
                            noiseError = "File too short for noise reduction";
                        }
                        else if (nEnd - nStart < minNoise) {
                            noiseError = "Noise selection too short, pick a longer quiet part";
                        }
                        else {
                            noiseError = "";
                            applyEdit("Noise reduction", [&]() {
                                applyPerChannel(activeWhisper, [&](Spectrogram& sg, int channel) {
                                    //every channel gets its own noise profile from its own noise sample
                                    Whisper noiseSegment = activeWhisper.extractChannel(channel).hardSplice(nStart, nEnd);
                                    Spectrogram noiseSG = noiseSegment.decoupleSTFT(2048, 512);
                                    double* noiseProfile = buildNoiseProfile(noiseSG, 2048);
                                    for (int i = 0; i < sg.getNumFrames(); i++) {
                                        spectralSubtract(sg.getFrame(i), noiseProfile, 2048, noiseStrength);
                                    }
                                    delete[] noiseProfile;
                                });
                            });
                        }
                    }
                    errorText(noiseError);

                    sectionHeader("SPECTRAL GATE");
                    hint("Removes every frequency quieter than this fraction of the loudest one in each frame.");
                    ImGui::SetNextItemWidth(-60);
                    ImGui::SliderFloat("Threshold", &threshold, 0.0f, 0.3f, "%.3f");
                    if (accentButton("Apply Spectral Gate")) {
                        applyEdit("Spectral gate", [&]() {
                            applyPerChannel(activeWhisper, [&](Spectrogram& sg, int) {
                                for (int i = 0; i < sg.getNumFrames(); i++) {
                                    spectralZero(sg.getFrame(i), 2048, threshold);
                                }
                            });
                        });
                    }
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Export")) {
                    ImGui::BeginDisabled(!isFileLoaded);
                    sectionHeader("OUTPUT FILE");
                    ImGui::SetNextItemWidth(-buttonWidth("Browse...") - style.ItemSpacing.x);
                    ImGui::InputText("##exportpath", exportPath, sizeof(exportPath));
                    ImGui::SameLine();
                    if (ImGui::Button("Browse...")) openSaveDialog();

                    sectionHeader("AUDIO");
                    ImGui::TextDisabled("Selection  %s - %s  (%s)", formatTime(cropStart, 3).c_str(),
                        formatTime(cropEnd, 3).c_str(), formatTime(cropEnd - cropStart, 3).c_str());
                    if (accentButton("Export Selection")) exportAudio(true);
                    if (ImGui::Button("Export Entire File", ImVec2(-1, 0))) exportAudio(false);

                    sectionHeader("IMAGES");
                    ImGui::BeginDisabled(beforeAfterPending);
                    if (accentButton(beforeAfterPending ? "Rendering..." : "Save Before/After PNG")) {
                        beforeAfterPending = true;
                        setStatus("Rendering spectrograms...");
                    }
                    ImGui::EndDisabled();
                    if (ImGui::Button("Save Window Snapshot (F12)", ImVec2(-1, 0))) snapshotRequested = true;
                    hint("Before/After puts the spectrogram of the original file above the processed one, with the list of edits. Saved next to the program.");
                    ImGui::EndDisabled();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        // EDITOR: waveform / spectrogram / spectrum, analyzer strip underneath
        ImGui::SetNextWindowPos(ImVec2(sideW, topH));
        ImGui::SetNextWindowSize(ImVec2(disp.x - sideW, disp.y - topH - statusH));
        ImGui::Begin("##editor", nullptr, panelFlags | ImGuiWindowFlags_NoScrollWithMouse);
        {
            static const char* viewNames[] = { "Waveform", "Spectrogram", "Spectrum" };
            segmented("##view", &displayMode, viewNames, 3, 330.0f);

            if (isFileLoaded) {
                ImGui::SameLine(0, 16);
                int ab = showOriginal ? 0 : 1;
                static const char* abNames[] = { "A  Original", "B  Processed" };
                if (segmented("##ab", &ab, abNames, 2, 230.0f)) setABMode(ab == 0);
                ImGui::SetItemTooltip("Compare with the original file (A)");
            }

            if (isFileLoaded && displayMode != 2) {
                // zoom controls, right aligned
                float w = buttonWidth("-") + buttonWidth("+") + buttonWidth("Fit") + style.ItemSpacing.x * 2;
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - w);
                auto zoomBy = [&](double factor) {
                    double c = (view.viewStart + view.viewEnd) * 0.5;
                    double s = (view.viewEnd - view.viewStart) * factor;
                    view.viewStart = c - s * 0.5;
                    view.viewEnd = c + s * 0.5;
                };
                if (ImGui::Button("-")) zoomBy(1.5);
                ImGui::SetItemTooltip("Zoom out (mouse wheel)");
                ImGui::SameLine();
                if (ImGui::Button("+")) zoomBy(1.0 / 1.5);
                ImGui::SetItemTooltip("Zoom in (mouse wheel)");
                ImGui::SameLine();
                if (ImGui::Button("Fit")) { view.viewStart = 0.0; view.viewEnd = maxDuration; }
                ImGui::SetItemTooltip("Show the whole file");
            }
            ImGui::Spacing();

            if (!isFileLoaded) {
                if (emptyState()) openLoadDialog();
            }
            else {
                const float stripH = 150.0f;
                bool showStrip = ImGui::GetContentRegionAvail().y > stripH + 220.0f;
                float mainH = ImGui::GetContentRegionAvail().y - (showStrip ? stripH + style.ItemSpacing.y : 0.0f);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
                ImGui::BeginChild("##main", ImVec2(0, mainH), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                if (displayMode == 2) {
                    spectrumView(activeSpectrumDisplay);
                }
                else {
                    const SpectrogramImage* spec = nullptr;
                    if (displayMode == 1) spec = showOriginal ? &originalSpec : &processedSpec;
                    timelineEditor(view, shown(), shownKey(), maxDuration, cropStart, cropEnd,
                        noiseStart, noiseEnd, showNoise, playheadTime, isPlaying, spec, shownKey());
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                if (showStrip) drawAnalyzerStrip(analyzer, stats, shown().getHeader().numChannels, stripH);
            }
        }
        ImGui::End();

        // STATUS BAR
        ImGui::SetNextWindowPos(ImVec2(0, disp.y - statusH));
        ImGui::SetNextWindowSize(ImVec2(disp.x, statusH));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, style.Colors[ImGuiCol_ChildBg]);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 2));
        ImGui::Begin("##status", nullptr, panelFlags);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        {
            drawEdge(ImVec2(0, disp.y - statusH), ImVec2(disp.x, disp.y - statusH));
            if (isFileLoaded) {
                WavHeader h = activeWhisper.getHeader();
                std::string channelsText = h.numChannels == 1 ? "Mono" : h.numChannels == 2 ? "Stereo" : std::to_string(h.numChannels) + " ch";
                ImGui::TextDisabled("%d Hz   |   %s   |   %d-bit   |   %s   |   %d samples   |   %s",
                    h.sampleRate, channelsText.c_str(), h.bitsPerSample, formatTime(maxDuration, 3).c_str(),
                    activeWhisper.getNumOfS(), showOriginal ? "A: original" : "B: processed");
            }
            else {
                ImGui::TextDisabled("No file loaded");
            }

            // recent action for a few seconds, otherwise the controls cheat sheet
            bool showMessage = !statusMessage.empty() && statusClock.getElapsedTime().asSeconds() < 4.0f;
            const char* rightText = showMessage ? statusMessage.c_str()
                : "Space play   A compare   L loop   Drag select   Shift+Drag noise   Wheel zoom   Ctrl+Z undo   F12 snapshot";
            float rightW = ImGui::CalcTextSize(rightText).x;
            ImGui::SameLine(std::max(ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + 30.0f, disp.x - rightW - 14));
            if (showMessage) ImGui::TextColored(g_theme.accent, "%s", rightText);
            else ImGui::TextDisabled("%s", rightText);
        }
        ImGui::End();

        // FILE DIALOGS (drawn last so they sit on top)
        ImVec2 dlgMin(std::min(720.0f, disp.x * 0.9f), std::min(460.0f, disp.y * 0.9f));
        ImVec2 dlgMax(disp.x * 0.95f, disp.y * 0.95f);
        for (const char* key : { "ChooseAudioDlg", "SaveAudioDlg" }) {
            if (!ImGuiFileDialog::Instance()->IsOpened(key)) continue;
            ImGui::SetNextWindowPos(disp * 0.5f, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGuiFileDialog::Instance()->Display(key, ImGuiWindowFlags_NoCollapse, dlgMin, dlgMax)) {
                // If the user clicked "OK" (and didn't hit cancel)
                if (ImGuiFileDialog::Instance()->IsOk()) {
                    std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                    if (std::string(key) == "ChooseAudioDlg") loadFile(path);
                    else strncpy_s(exportPath, sizeof(exportPath), path.c_str(), _TRUNCATE);
                }
                ImGuiFileDialog::Instance()->Close();
            }
        }

        window.clear(sf::Color(0, 0, 0));
        ImGui::SFML::Render(window);

        // grab the finished frame before it is shown
        if (snapshotRequested) {
            snapshotRequested = false;
            sf::Texture capture;
            std::string path = timestampedName("whisper_snapshot_", ".png");
            if (capture.create(window.getSize().x, window.getSize().y)) {
                capture.update(window);
                if (capture.copyToImage().saveToFile(path)) setStatus("Saved " + path);
                else setStatus("Could not save the snapshot");
            }
        }
        window.display();
    }

    player.stop();
    ImGui::SFML::Shutdown();
    return 0;
}
