#pragma once
#include "UIConstants.h"
#include "NodeUserData.h"
#include <osg/Camera>
#include <osg/ref_ptr>
#include <string>
#include <vector>

class UserDataPanel : public osg::Referenced
{
public:
    // Hit-test result codes
    static const int UDP_HIT_NONE        = -1;
    static const int UDP_HIT_KEY_FIELD   =  0;
    static const int UDP_HIT_VAL_FIELD   =  1;
    static const int UDP_HIT_ADD         =  2;
    static const int UDP_HIT_CLEAR_ALL   =  3;
    static const int UDP_HIT_DELETE_BASE = 100;  // 100+i = delete row i

    UserDataPanel(int winW, int winH);

    osg::Camera* getCamera();

    // Call after a node is selected in the hierarchy / 3D viewport
    void setSelectedNode(const std::string& nodeName,
                         osg::Node*         sceneRoot,
                         const std::string& jsonPath);

    // Call when a new file is loaded or the selection is cleared
    void clearSelection();

    const std::string& jsonPath()   const { return _jsonPath; }
    const std::string& nodeName()   const { return _nodeName; }

    // Hit-test: returns UDP_HIT_* code, or UDP_HIT_NONE if outside panel
    int  hitTest(int sx, int sy) const;

    // Process a hit-test result (click handler)
    void handleHit(int code);

    // Keyboard routing — call these when this panel has focus
    bool hasFocus()       const { return _focusedField >= 0 && _enabled; }
    void appendChar(char c);
    void backspace();

    void onResize(int w, int h);

private:
    struct HitZone { int x, y, w, h, code; };

    int  _winW, _winH;

    bool        _enabled     = false;
    std::string _nodeName;
    std::string _jsonPath;
    UserDataMap _nodeData;

    std::string _keyInput;
    std::string _valInput;
    int         _focusedField = -1;

    std::vector<HitZone>      _hitZones;
    osg::ref_ptr<osg::Camera> _hudCamera;

    int panelX() const { return _winW - RPANEL_W - UDPANEL_W; }

    void _rebuild();
    void _save();

    void _drawField (int x, int y, int w, int h,
                     const std::string& label, const std::string& content,
                     bool focused);
    void _drawButton(int x, int y, int w, int h, const char* label, bool danger = false);
    void _addLabel  (int x, int y, const char* text);
    void _addDivider(int x, int y, int w);
};
