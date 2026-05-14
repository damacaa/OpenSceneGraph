#include "UserDataPanel.h"
#include "UIWidgets.h"
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/StateSet>
#include <osgText/Text>

static const int UDP_PAD     = 10;
static const int UDP_FIELD_H = 26;
static const int UDP_BTN_H   = 28;
static const int UDP_ROW_H   = 24;

// ─── Construction ─────────────────────────────────────────────────────────────

UserDataPanel::UserDataPanel(int winW, int winH)
    : _winW(winW), _winH(winH)
{
    _hudCamera = new osg::Camera;
    _hudCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
    _hudCamera->setProjectionMatrixAsOrtho2D(0, winW, 0, winH);
    _hudCamera->setViewMatrix(osg::Matrix::identity());
    _hudCamera->setViewport(0, 0, winW, winH);
    _hudCamera->setRenderOrder(osg::Camera::POST_RENDER);
    _hudCamera->setClearMask(0);

    osg::StateSet* ss = _hudCamera->getOrCreateStateSet();
    ss->setMode(GL_BLEND,    osg::StateAttribute::ON);
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    ss->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    ss->setAttributeAndModes(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false));

    _rebuild();
}

osg::Camera* UserDataPanel::getCamera() { return _hudCamera.get(); }

// ─── Public interface ─────────────────────────────────────────────────────────

void UserDataPanel::setSelectedNode(const std::string& nodeName,
                                     osg::Node*         sceneRoot,
                                     const std::string& jsonPath)
{
    _nodeName     = nodeName;
    _jsonPath     = jsonPath;
    _focusedField = -1;
    _keyInput.clear();
    _valInput.clear();

    int count = countNodesWithName(sceneRoot, nodeName);
    _enabled = (count == 1 && !nodeName.empty());

    if (_enabled)
    {
        auto allData = loadUserDataJson(jsonPath);
        auto it = allData.find(nodeName);
        _nodeData = (it != allData.end()) ? it->second : UserDataMap{};
    }
    else
    {
        _nodeData.clear();
    }
    _rebuild();
}

void UserDataPanel::clearSelection()
{
    _nodeName.clear();
    _jsonPath.clear();
    _nodeData.clear();
    _keyInput.clear();
    _valInput.clear();
    _focusedField = -1;
    _enabled      = false;
    _rebuild();
}

int UserDataPanel::hitTest(int sx, int sy) const
{
    if (sx < panelX() || sx >= panelX() + UDPANEL_W) return UDP_HIT_NONE;
    if (sy < 0 || sy >= _winH) return UDP_HIT_NONE;
    for (const auto& hz : _hitZones)
        if (sx >= hz.x && sx < hz.x + hz.w && sy >= hz.y && sy < hz.y + hz.h)
            return hz.code;
    return UDP_HIT_NONE;
}

void UserDataPanel::handleHit(int code)
{
    if (code == UDP_HIT_KEY_FIELD)
    {
        _focusedField = UDP_HIT_KEY_FIELD;
    }
    else if (code == UDP_HIT_VAL_FIELD)
    {
        _focusedField = UDP_HIT_VAL_FIELD;
    }
    else if (code == UDP_HIT_ADD)
    {
        if (!_keyInput.empty() && _enabled)
        {
            _nodeData[_keyInput] = _valInput;
            _keyInput.clear();
            _valInput.clear();
            _focusedField = -1;
            _save();
        }
    }
    else if (code == UDP_HIT_CLEAR_ALL)
    {
        if (_enabled)
        {
            _nodeData.clear();
            _keyInput.clear();
            _valInput.clear();
            _focusedField = -1;
            _save();
        }
    }
    else if (code >= UDP_HIT_DELETE_BASE)
    {
        if (_enabled)
        {
            int idx = code - UDP_HIT_DELETE_BASE;
            auto it = _nodeData.begin();
            std::advance(it, idx);
            if (it != _nodeData.end())
            {
                _nodeData.erase(it);
                _save();
            }
        }
    }
    _rebuild();
}

void UserDataPanel::appendChar(char c)
{
    if (!_enabled) return;
    if      (_focusedField == UDP_HIT_KEY_FIELD) _keyInput += c;
    else if (_focusedField == UDP_HIT_VAL_FIELD) _valInput += c;
    _rebuild();
}

void UserDataPanel::backspace()
{
    if (!_enabled) return;
    if      (_focusedField == UDP_HIT_KEY_FIELD && !_keyInput.empty()) _keyInput.pop_back();
    else if (_focusedField == UDP_HIT_VAL_FIELD && !_valInput.empty()) _valInput.pop_back();
    _rebuild();
}

void UserDataPanel::onResize(int w, int h)
{
    _winW = w;
    _winH = h;
    _hudCamera->setProjectionMatrixAsOrtho2D(0, w, 0, h);
    _hudCamera->setViewport(0, 0, w, h);
    _rebuild();
}

// ─── Private: persist ────────────────────────────────────────────────────────

void UserDataPanel::_save()
{
    if (_jsonPath.empty()) return;
    auto allData = loadUserDataJson(_jsonPath);
    if (_nodeData.empty())
        allData.erase(_nodeName);
    else
        allData[_nodeName] = _nodeData;
    saveUserDataJson(_jsonPath, allData);
}

// ─── Private: rebuild UI ─────────────────────────────────────────────────────

void UserDataPanel::_rebuild()
{
    _hudCamera->removeChildren(0, _hudCamera->getNumChildren());
    _hitZones.clear();

    const int px = panelX();
    const int pw = UDPANEL_W;
    const int bw = pw - 2 * UDP_PAD;
    int y = _winH;

    // ── Background ──────────────────────────────────────────────────────────
    {
        osg::Geode* bg = new osg::Geode;
        bg->addDrawable(makeQuad((float)px, 0.f, (float)pw, (float)_winH, C_PANEL_BG));
        // left border line
        bg->addDrawable(makeQuad((float)px, 0.f, 1.f, (float)_winH, C_DIVIDER));
        _hudCamera->addChild(bg);
    }

    // ── Header ──────────────────────────────────────────────────────────────
    y -= HEADER_H;
    {
        osg::Geode* hdr = new osg::Geode;
        hdr->addDrawable(makeQuad((float)px, (float)y, (float)pw, (float)HEADER_H, C_HEADER_BG));
        hdr->addDrawable(makeQuad((float)px, (float)(y - 1), (float)pw, 1.f, C_DIVIDER));
        _hudCamera->addChild(hdr);

        osg::Geode* t = new osg::Geode;
        t->addDrawable(makeText("Node User Data",
            (float)(px + UDP_PAD), (float)(y + 14), FONT_SZ + 1.f, C_TEXT));
        _hudCamera->addChild(t);
    }

    y -= RP_SEP;

    // ── Node name row ───────────────────────────────────────────────────────
    y -= RP_LABEL_H;
    {
        std::string label = _nodeName.empty() ? "(no selection)" : _nodeName;
        // Truncate long names
        if ((int)label.size() > 22) label = label.substr(0, 19) + "...";
        osg::Geode* t = new osg::Geode;
        t->addDrawable(makeText("Node:", (float)(px + UDP_PAD), (float)(y + 4),
            FONT_SZ - 1.f, C_TEXT_DIM));
        t->addDrawable(makeText(label,   (float)(px + UDP_PAD + 48), (float)(y + 4),
            FONT_SZ - 1.f, _enabled ? C_TEXT : C_TEXT_DIM));
        _hudCamera->addChild(t);
    }

    if (!_nodeName.empty() && !_enabled)
    {
        y -= RP_LABEL_H;
        osg::Geode* t = new osg::Geode;
        t->addDrawable(makeText("(name not unique - feature disabled)",
            (float)(px + UDP_PAD), (float)(y + 4),
            FONT_SZ - 2.f, osg::Vec4(1.f, 0.4f, 0.3f, 1.f)));
        _hudCamera->addChild(t);
    }

    y -= RP_SEP;
    _addDivider(px, y, pw);
    y -= 1;

    if (!_enabled) return;

    // ── Stored entries ──────────────────────────────────────────────────────
    y -= RP_SEP;
    y -= RP_LABEL_H;
    _addLabel(px + UDP_PAD, y, "STORED DATA");

    if (_nodeData.empty())
    {
        y -= UDP_ROW_H;
        osg::Geode* t = new osg::Geode;
        t->addDrawable(makeText("(none)", (float)(px + UDP_PAD), (float)(y + 5),
            FONT_SZ - 1.f, C_TEXT_DIM));
        _hudCamera->addChild(t);
    }
    else
    {
        int idx = 0;
        for (UserDataMap::const_iterator eit = _nodeData.begin(); eit != _nodeData.end(); ++eit)
        {
            const std::string& k = eit->first;
            const std::string& v = eit->second;
            y -= UDP_ROW_H;

            // key: value text (truncated)
            std::string kv = k + ": " + v;
            if ((int)kv.size() > 26) kv = kv.substr(0, 23) + "...";
            osg::Geode* t = new osg::Geode;
            t->addDrawable(makeText(kv, (float)(px + UDP_PAD), (float)(y + 5),
                FONT_SZ - 1.f, C_TEXT));
            _hudCamera->addChild(t);

            // Delete [X] button
            const int DBW = 20, DBH = 18;
            int dbx = px + pw - UDP_PAD - DBW;
            int dby = y + (UDP_ROW_H - DBH) / 2;
            osg::Geode* dbg = new osg::Geode;
            dbg->addDrawable(makeQuad((float)dbx, (float)dby,
                (float)DBW, (float)DBH,
                osg::Vec4(0.45f, 0.12f, 0.12f, 0.9f)));
            _hudCamera->addChild(dbg);
            osg::Geode* xl = new osg::Geode;
            xl->addDrawable(makeText("X", (float)(dbx + DBW / 2), (float)(dby + 3),
                FONT_SZ - 2.f, C_TEXT, osgText::Text::CENTER_BOTTOM));
            _hudCamera->addChild(xl);

            _hitZones.push_back({ dbx, dby, DBW, DBH, UDP_HIT_DELETE_BASE + idx });
            ++idx;
        }
    }

    y -= RP_SEP;
    _addDivider(px, y, pw);
    y -= 1;

    // ── Add-entry section ───────────────────────────────────────────────────
    y -= RP_SEP;
    y -= RP_LABEL_H;
    _addLabel(px + UDP_PAD, y, "ADD ENTRY");

    // Key field
    y -= RP_SEP;
    y -= UDP_FIELD_H;
    _drawField(px + UDP_PAD, y, bw, UDP_FIELD_H, "Key", _keyInput,
               _focusedField == UDP_HIT_KEY_FIELD);
    _hitZones.push_back({ px + UDP_PAD, y, bw, UDP_FIELD_H, UDP_HIT_KEY_FIELD });

    // Value field
    y -= RP_SEP;
    y -= UDP_FIELD_H;
    _drawField(px + UDP_PAD, y, bw, UDP_FIELD_H, "Value", _valInput,
               _focusedField == UDP_HIT_VAL_FIELD);
    _hitZones.push_back({ px + UDP_PAD, y, bw, UDP_FIELD_H, UDP_HIT_VAL_FIELD });

    // Add / Clear All buttons
    y -= RP_SEP + 4;
    y -= UDP_BTN_H;
    const int addW = (bw - 4) * 2 / 3;
    const int clrW = bw - addW - 4;

    _drawButton(px + UDP_PAD, y, addW, UDP_BTN_H, "Add");
    _hitZones.push_back({ px + UDP_PAD, y, addW, UDP_BTN_H, UDP_HIT_ADD });

    _drawButton(px + UDP_PAD + addW + 4, y, clrW, UDP_BTN_H, "Clear All", /*danger=*/true);
    _hitZones.push_back({ px + UDP_PAD + addW + 4, y, clrW, UDP_BTN_H, UDP_HIT_CLEAR_ALL });
}

// ─── Drawing helpers ─────────────────────────────────────────────────────────

void UserDataPanel::_drawField(int x, int y, int w, int h,
                                const std::string& label,
                                const std::string& content,
                                bool focused)
{
    osg::Vec4 bgCol = focused
        ? osg::Vec4(0.15f, 0.15f, 0.22f, 0.97f)
        : osg::Vec4(0.13f, 0.13f, 0.16f, 0.97f);
    osg::Vec4 borderCol = focused ? C_BTN_ACTIVE : C_DIVIDER;

    osg::Geode* bg = new osg::Geode;
    bg->addDrawable(makeQuad((float)x,       (float)y,       (float)w, (float)h, bgCol));
    // 1-px border
    bg->addDrawable(makeQuad((float)x,       (float)(y+h-1), (float)w,      1.f, borderCol));
    bg->addDrawable(makeQuad((float)x,       (float)y,       (float)w,      1.f, borderCol));
    bg->addDrawable(makeQuad((float)x,       (float)y,           1.f, (float)h,  borderCol));
    bg->addDrawable(makeQuad((float)(x+w-1), (float)y,           1.f, (float)h,  borderCol));
    _hudCamera->addChild(bg);

    // Small label in top-left corner of the field
    osg::Geode* lbl = new osg::Geode;
    lbl->addDrawable(makeText(label + ":", (float)(x + 4), (float)(y + h - 5),
        FONT_SZ - 4.f, C_TEXT_DIM));
    _hudCamera->addChild(lbl);

    // Typed content (show cursor pipe when focused, truncate from left)
    std::string display = content;
    if (focused) display += "|";
    const int MAX_CHARS = 23;
    if ((int)display.size() > MAX_CHARS)
        display = display.substr(display.size() - MAX_CHARS);

    osg::Geode* ct = new osg::Geode;
    ct->addDrawable(makeText(display, (float)(x + 5), (float)(y + 5),
        FONT_SZ, focused ? C_TEXT : C_TEXT_DIM));
    _hudCamera->addChild(ct);
}

void UserDataPanel::_drawButton(int x, int y, int w, int h,
                                 const char* label, bool danger)
{
    osg::Vec4 col = danger
        ? osg::Vec4(0.38f, 0.10f, 0.10f, 0.92f)
        : C_BTN_IDLE;

    osg::Geode* bg = new osg::Geode;
    bg->addDrawable(makeQuad((float)x, (float)y, (float)w, (float)h, col));
    _hudCamera->addChild(bg);

    osg::Geode* t = new osg::Geode;
    t->addDrawable(makeText(label, (float)(x + w / 2), (float)(y + 8),
        FONT_SZ - 1.f, C_TEXT, osgText::Text::CENTER_BOTTOM));
    _hudCamera->addChild(t);
}

void UserDataPanel::_addLabel(int x, int y, const char* text)
{
    osg::Geode* t = new osg::Geode;
    t->addDrawable(makeText(text, (float)x, (float)(y + 4),
        FONT_SZ - 1.f, C_TEXT_DIM));
    _hudCamera->addChild(t);
}

void UserDataPanel::_addDivider(int x, int y, int w)
{
    osg::Geode* div = new osg::Geode;
    div->addDrawable(makeQuad((float)x, (float)y, (float)w, 1.f, C_DIVIDER));
    _hudCamera->addChild(div);
}
