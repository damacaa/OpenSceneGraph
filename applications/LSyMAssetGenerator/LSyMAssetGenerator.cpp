// LSyMAssetGenerator - Load a 3D file and display it with an interactive node-hierarchy panel.
//
// Layout:
//   [Hierarchy Panel | 3-D View]
//   Left panel contains:
//     - Scrollable scene-graph tree (click to focus camera)
//     - "Export as .osgb" button at the bottom

#include <osg/ArgumentParser>
#include <osg/BlendFunc>
#include <osg/Camera>
#include <osg/Depth>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/Group>
#include <osg/LineWidth>
#include <osg/Material>
#include <osg/MatrixTransform>
#include <osg/Scissor>
#include <osg/StateSet>

#include <osgDB/FileNameUtils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <osgGA/TrackballManipulator>

#include <osg/ComputeBoundsVisitor>

#include <osgUtil/Optimizer>

#include <osgText/Text>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

// ─── Layout ──────────────────────────────────────────────────────────────────

static const int   PANEL_W      = 320;
static const int   HEADER_H     = 44;
static const int   EXPORT_H     = 46;
static const int   ITEM_H       = 22;
static const int   INDENT_PX    = 14;
static const float FONT_SZ      = 13.0f;

// ─── Colours ─────────────────────────────────────────────────────────────────

static const osg::Vec4 C_PANEL_BG   (0.11f, 0.11f, 0.13f, 0.93f);
static const osg::Vec4 C_HEADER_BG  (0.07f, 0.07f, 0.09f, 1.00f);
static const osg::Vec4 C_DIVIDER    (0.26f, 0.26f, 0.30f, 1.00f);
static const osg::Vec4 C_SEL        (0.18f, 0.44f, 0.80f, 0.90f);
static const osg::Vec4 C_EXPORT     (0.12f, 0.52f, 0.22f, 0.95f);
static const osg::Vec4 C_EXPORT_HOV (0.16f, 0.65f, 0.28f, 0.95f);
static const osg::Vec4 C_TEXT       (0.92f, 0.92f, 0.92f, 1.00f);
static const osg::Vec4 C_TEXT_DIM   (0.58f, 0.58f, 0.62f, 1.00f);
static const osg::Vec4 C_SCROLLBAR  (0.38f, 0.38f, 0.43f, 0.75f);

// ─── Hierarchy item ───────────────────────────────────────────────────────────

struct HItem {
    osg::observer_ptr<osg::Node> node;
    std::string                  label;
    int                          depth;
    bool                         isGroup;
};

class HierarchyVisitor : public osg::NodeVisitor {
public:
    std::vector<HItem> items;

    HierarchyVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

    void apply(osg::Node& node) override {
        int depth = static_cast<int>(getNodePath().size()) - 1;

        std::string name = node.getName();
        std::string type = node.className();
        std::string label = name.empty() ? ("[" + type + "]") : (name + "  [" + type + "]");

        bool isGroup = (node.asGroup() != nullptr);
        items.push_back({ &node, label, depth, isGroup });
        traverse(node);
    }
};

// ─── Geometry utilities ──────────────────────────────────────────────────────

static osg::Geometry* makeQuad(float x, float y, float w, float h, const osg::Vec4& col)
{
    osg::Geometry* g = new osg::Geometry;
    g->setDataVariance(osg::Object::DYNAMIC);

    osg::Vec3Array* v = new osg::Vec3Array(4);
    (*v)[0].set(x,     y,     0.f);
    (*v)[1].set(x + w, y,     0.f);
    (*v)[2].set(x + w, y + h, 0.f);
    (*v)[3].set(x,     y + h, 0.f);
    g->setVertexArray(v);

    osg::Vec4Array* c = new osg::Vec4Array(1);
    (*c)[0] = col;
    g->setColorArray(c, osg::Array::BIND_OVERALL);

    g->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
    g->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    return g;
}

static osgText::Text* makeText(const std::string& str, float x, float y,
                                float size, const osg::Vec4& col,
                                osgText::Text::AlignmentType align = osgText::Text::LEFT_BOTTOM)
{
    osgText::Text* t = new osgText::Text;
    t->setDataVariance(osg::Object::DYNAMIC);
    t->setText(str);
    t->setPosition(osg::Vec3(x, y, -0.1f));
    t->setCharacterSize(size);
    t->setColor(col);
    t->setAlignment(align);
    t->setFont("fonts/arial.ttf");
    return t;
}

// ─── HierarchyPanel ──────────────────────────────────────────────────────────
// Owns the HUD osg::Camera that renders the left UI panel.

class HierarchyPanel : public osg::Referenced {
public:
    HierarchyPanel(const std::vector<HItem>& items,
                   int winW, int winH,
                   const std::string& exportPath)
        : _items(items)
        , _winW(winW), _winH(winH)
        , _scroll(0)
        , _selIdx(-1)
        , _exportPath(exportPath)
        , _exportHovered(false)
    {
        _buildCamera();
        _buildScene();
    }

    osg::Camera* getCamera() { return _hudCamera.get(); }

    // ── Query helpers ────────────────────────────────────────────────────────

    int listAreaTop()    const { return _winH - HEADER_H; }
    int listAreaBottom() const { return EXPORT_H; }
    int listAreaH()      const { return listAreaTop() - listAreaBottom(); }
    int totalListH()     const { return static_cast<int>(_items.size()) * ITEM_H; }
    int maxScroll()      const { return std::max(0, totalListH() - listAreaH()); }

    // ── Public mutations (called from event handler) ─────────────────────────

    void scroll(int delta)
    {
        _scroll = std::max(0, std::min(maxScroll(), _scroll + delta));
        _applyScroll();
        _rebuildScrollbar();
    }

    // Returns item index, -2 for export button, -1 for miss.
    int hitTest(int sx, int sy) const
    {
        if (sx < 0 || sx >= PANEL_W)          return -1;
        if (sy >= 0 && sy < EXPORT_H)          return -2;   // export button
        if (sy >= _winH - HEADER_H)            return -1;   // header

        int fromTop = (_winH - HEADER_H - 1) - sy;
        int idx     = (fromTop + _scroll) / ITEM_H;
        if (idx < 0 || idx >= static_cast<int>(_items.size())) return -1;
        return idx;
    }

    void setSelected(int idx)
    {
        _selIdx = idx;
        _updateHighlight();
    }

    void setExportHovered(bool h)
    {
        if (_exportHovered == h) return;
        _exportHovered = h;
        _rebuildExportButton();
    }

    const HItem* selectedItem() const
    {
        if (_selIdx < 0 || _selIdx >= static_cast<int>(_items.size())) return nullptr;
        return &_items[_selIdx];
    }

    const std::string& exportPath() const { return _exportPath; }

    void onResize(int w, int h)
    {
        _winW = w;  _winH = h;
        _hudCamera->setProjectionMatrixAsOrtho2D(0, w, 0, h);
        _hudCamera->setViewport(0, 0, w, h);
        // Full rebuild since all absolute-Y positions changed.
        _hudCamera->removeChildren(0, _hudCamera->getNumChildren());
        _buildScene();
    }

private:
    // ── Scene-graph pointers mutated after build ──────────────────────────────
    osg::ref_ptr<osg::Camera>          _hudCamera;
    osg::ref_ptr<osg::MatrixTransform> _listXform;   // translates list for scroll
    osg::ref_ptr<osg::Geometry>        _hlQuad;      // selection highlight quad
    osg::ref_ptr<osg::Geode>           _exportGeode;
    osg::ref_ptr<osg::Geode>           _exportTextGeode;
    osg::ref_ptr<osg::Geode>           _scrollGeode;

    std::vector<HItem> _items;
    int  _winW, _winH;
    int  _scroll;
    int  _selIdx;
    std::string _exportPath;
    bool _exportHovered;

    // ── Build helpers ─────────────────────────────────────────────────────────

    void _buildCamera()
    {
        _hudCamera = new osg::Camera;
        _hudCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        _hudCamera->setProjectionMatrixAsOrtho2D(0, _winW, 0, _winH);
        _hudCamera->setViewMatrix(osg::Matrix::identity());
        _hudCamera->setViewport(0, 0, _winW, _winH);
        _hudCamera->setRenderOrder(osg::Camera::POST_RENDER);
        _hudCamera->setClearMask(0);

        osg::StateSet* ss = _hudCamera->getOrCreateStateSet();
        ss->setMode(GL_BLEND,        osg::StateAttribute::ON);
        ss->setMode(GL_LIGHTING,     osg::StateAttribute::OFF);
        ss->setMode(GL_DEPTH_TEST,   osg::StateAttribute::OFF);
        ss->setAttributeAndModes(
            new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        ss->setAttributeAndModes(
            new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false));
    }

    void _buildScene()
    {
        // ── Panel background ──────────────────────────────────────────────────
        {
            osg::Geode* bg = new osg::Geode;
            bg->addDrawable(makeQuad(0, 0, PANEL_W, _winH, C_PANEL_BG));
            _hudCamera->addChild(bg);
        }

        // ── Right-edge divider ────────────────────────────────────────────────
        {
            osg::Geode* div = new osg::Geode;
            div->addDrawable(makeQuad(PANEL_W - 1, 0, 1, _winH, C_DIVIDER));
            _hudCamera->addChild(div);
        }

        // ── Header ────────────────────────────────────────────────────────────
        {
            osg::Geode* hdrBg = new osg::Geode;
            hdrBg->addDrawable(makeQuad(0, _winH - HEADER_H, PANEL_W, HEADER_H, C_HEADER_BG));
            // Bottom divider line of header
            hdrBg->addDrawable(makeQuad(0, _winH - HEADER_H - 1, PANEL_W, 1, C_DIVIDER));
            _hudCamera->addChild(hdrBg);

            osg::Geode* hdrText = new osg::Geode;
            hdrText->addDrawable(makeText("Scene Hierarchy",
                                          10.f, (float)(_winH - HEADER_H + 13),
                                          FONT_SZ + 2.f, C_TEXT));
            _hudCamera->addChild(hdrText);
        }

        // ── Export button ─────────────────────────────────────────────────────
        {
            // Divider above the button
            osg::Geode* div = new osg::Geode;
            div->addDrawable(makeQuad(0, EXPORT_H, PANEL_W, 1, C_DIVIDER));
            _hudCamera->addChild(div);

            _exportGeode = new osg::Geode;
            _hudCamera->addChild(_exportGeode.get());

            _exportTextGeode = new osg::Geode;
            _hudCamera->addChild(_exportTextGeode.get());

            _rebuildExportButton();
        }

        // ── Scrollable list area ──────────────────────────────────────────────
        {
            // Scissor clips everything to the list area rectangle.
            osg::Group* clip = new osg::Group;
            osg::Scissor* sc = new osg::Scissor(0, listAreaBottom(),
                                                 PANEL_W, listAreaH());
            clip->getOrCreateStateSet()->setAttributeAndModes(
                sc, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

            // Selection highlight (inside listXform so it scrolls with items)
            _listXform = new osg::MatrixTransform;
            _listXform->setDataVariance(osg::Object::DYNAMIC);
            _applyScroll();

            osg::Geode* hlGeode = new osg::Geode;
            _hlQuad = makeQuad(0, 0, PANEL_W - 8, ITEM_H, osg::Vec4(0,0,0,0));
            hlGeode->addDrawable(_hlQuad.get());
            _listXform->addChild(hlGeode);

            // Text items
            osg::Geode* textGeode = new osg::Geode;
            for (int i = 0; i < (int)_items.size(); ++i) {
                const HItem& it = _items[i];
                // Y position in list-local space (no scroll — xform handles that)
                float y = (float)(_winH - HEADER_H - (i + 1) * ITEM_H);

                float tx = 8.f + it.depth * INDENT_PX;

                std::string groupIcon = ">";

                // Group triangle indicator
                if (it.isGroup)
                {
                    textGeode->addDrawable(
                        makeText(groupIcon, tx, y + 6.f, FONT_SZ, C_TEXT_DIM));
                }
                textGeode->addDrawable(
                    makeText(it.label, tx + INDENT_PX, y + 4.f, FONT_SZ, C_TEXT));
            }
            _listXform->addChild(textGeode);
            clip->addChild(_listXform.get());

            // Scroll bar (fixed position, not inside listXform)
            _scrollGeode = new osg::Geode;
            _rebuildScrollbar();
            clip->addChild(_scrollGeode.get());

            _hudCamera->addChild(clip);
        }
    }

    // ── Dynamic update helpers ────────────────────────────────────────────────

    void _applyScroll()
    {
        if (_listXform.valid())
            _listXform->setMatrix(osg::Matrix::translate(0.0, _scroll, 0.0));
    }

    void _updateHighlight()
    {
        if (!_hlQuad.valid()) return;

        osg::Vec3Array* verts  = static_cast<osg::Vec3Array*>(_hlQuad->getVertexArray());
        osg::Vec4Array* colors = static_cast<osg::Vec4Array*>(_hlQuad->getColorArray());

        if (_selIdx < 0 || _selIdx >= (int)_items.size()) {
            (*colors)[0] = osg::Vec4(0, 0, 0, 0);
            colors->dirty();
            return;
        }

        // Local Y in list-transform space (scroll transform handles offset)
        float y = (float)(_winH - HEADER_H - (_selIdx + 1) * ITEM_H);
        (*verts)[0].set(0,          y,          0.f);
        (*verts)[1].set(PANEL_W - 8, y,          0.f);
        (*verts)[2].set(PANEL_W - 8, y + ITEM_H, 0.f);
        (*verts)[3].set(0,          y + ITEM_H, 0.f);
        verts->dirty();
        (*colors)[0] = C_SEL;
        colors->dirty();
        _hlQuad->dirtyBound();
    }

    void _rebuildExportButton()
    {
        if (!_exportGeode.valid()) return;
        _exportGeode->removeDrawables(0, _exportGeode->getNumDrawables());

        osg::Vec4 col = _exportHovered ? C_EXPORT_HOV : C_EXPORT;
        _exportGeode->addDrawable(makeQuad(6, 5, PANEL_W - 12, EXPORT_H - 10, col));

        if (_exportTextGeode.valid()) {
            _exportTextGeode->removeDrawables(0, _exportTextGeode->getNumDrawables());
            _exportTextGeode->addDrawable(makeText("Export as .osgb",
                                                    (float)(PANEL_W / 2), 9.f,
                                                    FONT_SZ, C_TEXT,
                                                    osgText::Text::CENTER_BOTTOM));
        }
    }

    void _rebuildScrollbar()
    {
        if (!_scrollGeode.valid()) return;
        _scrollGeode->removeDrawables(0, _scrollGeode->getNumDrawables());

        int total = totalListH();
        int avail = listAreaH();
        if (total <= avail) return;

        int barH  = std::max(24, avail * avail / total);
        float t   = (total > avail) ? (float)_scroll / (float)(total - avail) : 0.f;
        int barY  = listAreaBottom() + avail - barH - (int)(t * (avail - barH));

        _scrollGeode->addDrawable(makeQuad(PANEL_W - 6, barY, 4, barH, C_SCROLLBAR));
    }
};

// ─── Selection box ────────────────────────────────────────────────────────────
// Builds a bright yellow wireframe AABB around a node in world space.

static osg::Geode* makeSelectionBox(osg::Node* node)
{
    if (!node) return nullptr;

    // Compute AABB in the node's local space
    osg::ComputeBoundsVisitor cbv;
    node->accept(cbv);
    osg::BoundingBox bb = cbv.getBoundingBox();
    if (!bb.valid()) {
        osg::BoundingSphere bs = node->getBound();
        bb.expandBy(bs);
    }
    if (!bb.valid()) return nullptr;

    // Transform corners to world space
    osg::NodePathList paths = node->getParentalNodePaths();
    osg::Matrix localToWorld;
    if (!paths.empty())
        localToWorld = osg::computeLocalToWorld(paths[0]);

    osg::Vec3 c[8] = {
        osg::Vec3(bb.xMin(), bb.yMin(), bb.zMin()),
        osg::Vec3(bb.xMax(), bb.yMin(), bb.zMin()),
        osg::Vec3(bb.xMax(), bb.yMax(), bb.zMin()),
        osg::Vec3(bb.xMin(), bb.yMax(), bb.zMin()),
        osg::Vec3(bb.xMin(), bb.yMin(), bb.zMax()),
        osg::Vec3(bb.xMax(), bb.yMin(), bb.zMax()),
        osg::Vec3(bb.xMax(), bb.yMax(), bb.zMax()),
        osg::Vec3(bb.xMin(), bb.yMax(), bb.zMax()),
    };
    for (int i = 0; i < 8; ++i)
        c[i] = c[i] * localToWorld;

    // 12 edges of the box as GL_LINES pairs
    static const int edges[24] = {
        0,1, 1,2, 2,3, 3,0,   // bottom face
        4,5, 5,6, 6,7, 7,4,   // top face
        0,4, 1,5, 2,6, 3,7    // vertical edges
    };

    osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
    verts->reserve(24);
    for (int i = 0; i < 24; ++i)
        verts->push_back(c[edges[i]]);

    osg::ref_ptr<osg::Vec4Array> color = new osg::Vec4Array;
    color->push_back(osg::Vec4(1.0f, 0.85f, 0.0f, 1.0f));  // bright gold

    osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;
    geom->setVertexArray(verts.get());
    geom->setColorArray(color.get());
    geom->setColorBinding(osg::Geometry::BIND_OVERALL);
    geom->addPrimitiveSet(new osg::DrawArrays(GL_LINES, 0, 24));

    osg::StateSet* ss = geom->getOrCreateStateSet();
    ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::ON);
    ss->setAttribute(new osg::LineWidth(2.5f));
    ss->setRenderBinDetails(10, "RenderBin");

    osg::ref_ptr<osg::Geode> geode = new osg::Geode;
    geode->addDrawable(geom.get());
    return geode.release();
}

// ─── Camera focus ─────────────────────────────────────────────────────────────

static void focusCamera(osg::Node* node,
                         osgGA::TrackballManipulator* manip,
                         osg::Node* sceneRoot)
{
    if (!node || !manip) return;

    osg::BoundingSphere bs = node->getBound();
    osg::Vec3d center      = bs.center();
    double     radius      = bs.radius();

    // Transform center to world space
    osg::NodePathList paths = node->getParentalNodePaths();
    if (!paths.empty()) {
        osg::Matrix w = osg::computeLocalToWorld(paths[0]);
        osg::Vec4d  c4(center.x(), center.y(), center.z(), 1.0);
        c4 = c4 * w;
        center.set(c4.x(), c4.y(), c4.z());

        double sx = std::abs(w(0,0));
        double sy = std::abs(w(1,1));
        double sz = std::abs(w(2,2));
        radius   *= std::max({sx, sy, sz});
    }

    if (radius < 1e-4) {
        // Degenerate node — fall back to full scene
        if (sceneRoot) {
            osg::BoundingSphere sb = sceneRoot->getBound();
            center = sb.center();
            radius = sb.radius();
        } else {
            radius = 1.0;
        }
    }

    double dist = radius * 6.0;
    osg::Vec3d eye = center + osg::Vec3d(0.0, -dist, dist * 0.5);
    manip->setHomePosition(eye, center, osg::Vec3d(0, 0, 1));
    manip->home(0.0);
}

// ─── Event handler ────────────────────────────────────────────────────────────

class PanelHandler : public osgGA::GUIEventHandler {
public:
    PanelHandler(HierarchyPanel*            panel,
                 osgGA::TrackballManipulator* manip,
                 osg::Node*                   scene,
                 osg::Group*                  selectionGroup)
        : _panel(panel), _manip(manip), _scene(scene)
        , _selectionGroup(selectionGroup)
    {}

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter& /*aa*/) override
    {
        const int sx = (int)ea.getX();
        const int sy = (int)ea.getY();

        switch (ea.getEventType())
        {
        // ── Mouse wheel scroll ─────────────────────────────────────────────
        case osgGA::GUIEventAdapter::SCROLL:
            if (sx >= 0 && sx < PANEL_W) {
                int d = (ea.getScrollingMotion() ==
                         osgGA::GUIEventAdapter::SCROLL_UP)
                        ? -(ITEM_H * 3) : (ITEM_H * 3);
                _panel->scroll(d);
                return true;
            }
            break;

        // ── Click ──────────────────────────────────────────────────────────
        case osgGA::GUIEventAdapter::PUSH:
            if (sx >= 0 && sx < PANEL_W) {
                int hit = _panel->hitTest(sx, sy);
                if (hit == -2) {
                    _doExport();
                } else if (hit >= 0) {
                    _panel->setSelected(hit);
                    const HItem* item = _panel->selectedItem();
                    if (item) {
                        osg::ref_ptr<osg::Node> n = item->node.get();
                        if (n.valid()) {
                            focusCamera(n.get(), _manip.get(), _scene.get());
                            _updateSelectionBox(n.get());
                            _applyTint(n.get());
                        }
                    }
                }
                return true;  // consume — don't pass to camera manipulator
            }
            break;

        // ── Hover (export button highlight) ───────────────────────────────
        case osgGA::GUIEventAdapter::MOVE:
        case osgGA::GUIEventAdapter::DRAG:
            _panel->setExportHovered(
                sx >= 0 && sx < PANEL_W && sy >= 0 && sy < EXPORT_H);
            if (sx >= 0 && sx < PANEL_W)
                return true;  // consume mouse-move so camera doesn't rotate
            break;

        // ── Window resize ──────────────────────────────────────────────────
        case osgGA::GUIEventAdapter::RESIZE:
            _panel->onResize((int)ea.getWindowWidth(), (int)ea.getWindowHeight());
            break;

        default: break;
        }
        return false;
    }

private:
    void _doExport()
    {
        const std::string& path = _panel->exportPath();
        OSG_NOTICE << "Exporting scene to: " << path << std::endl;
        if (osgDB::writeNodeFile(*_scene, path))
            OSG_NOTICE << "Export successful: " << path << std::endl;
        else
            OSG_WARN  << "Export failed: "      << path << std::endl;
    }

    void _updateSelectionBox(osg::Node* node)
    {
        _selectionGroup->removeChildren(0, _selectionGroup->getNumChildren());
        osg::ref_ptr<osg::Geode> box = makeSelectionBox(node);
        if (box.valid())
            _selectionGroup->addChild(box.get());
    }

    void _applyTint(osg::Node* node)
    {
        _removeTint();
        if (!node) return;

        osg::ref_ptr<osg::Material> mat = new osg::Material;
        // Keep lighting but add a strong blue-tinted emission so the tint is
        // visible even over textures, regardless of the node's own material.
        mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.05f, 0.30f, 0.80f, 1.0f));
        mat->setAmbient (osg::Material::FRONT_AND_BACK, osg::Vec4(0.10f, 0.35f, 0.85f, 1.0f));
        mat->setDiffuse (osg::Material::FRONT_AND_BACK, osg::Vec4(0.40f, 0.60f, 1.00f, 1.0f));

        node->getOrCreateStateSet()->setAttributeAndModes(
            mat.get(),
            osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

        _tintedNode = node;
        _tintMat    = mat;
    }

    void _removeTint()
    {
        if (!_tintedNode.valid()) return;
        osg::StateSet* ss = _tintedNode->getStateSet();
        if (ss && ss->getAttribute(osg::StateAttribute::MATERIAL) == _tintMat.get())
            ss->removeAttribute(osg::StateAttribute::MATERIAL);
        _tintedNode = nullptr;
        _tintMat    = nullptr;
    }

    osg::ref_ptr<HierarchyPanel>               _panel;
    osg::ref_ptr<osgGA::TrackballManipulator>  _manip;
    osg::ref_ptr<osg::Node>                    _scene;
    osg::ref_ptr<osg::Group>                   _selectionGroup;
    osg::ref_ptr<osg::Node>                    _tintedNode;
    osg::ref_ptr<osg::Material>                _tintMat;
};

// ─── Resize handler ───────────────────────────────────────────────────────────
// Updates the 3D camera viewport whenever the window is resized.

class ResizeHandler : public osgGA::GUIEventHandler {
public:
    explicit ResizeHandler(osg::Camera* cam3D) : _cam3D(cam3D) {}

    bool handle(const osgGA::GUIEventAdapter& ea,
                osgGA::GUIActionAdapter& /*aa*/) override
    {
        if (ea.getEventType() == osgGA::GUIEventAdapter::RESIZE) {
            int nw = (int)ea.getWindowWidth();
            int nh = (int)ea.getWindowHeight();
            _cam3D->setViewport(PANEL_W, 0, nw - PANEL_W, nh);
        }
        return false;
    }

private:
    osg::ref_ptr<osg::Camera> _cam3D;
};

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv)
{
    osg::ArgumentParser args(&argc, argv);
    args.getApplicationUsage()->setApplicationName("LSyMAssetGenerator");
    args.getApplicationUsage()->setDescription(
        "Load an FBX file and inspect its scene graph interactively.");
    args.getApplicationUsage()->setCommandLineUsage(
        "LSyMAssetGenerator [options] <file>");
    args.getApplicationUsage()->addCommandLineOption(
        "-o <file>", "Output path for .osgb export (default: output.osgb)");

    if (args.argc() < 2) {
        args.getApplicationUsage()->write(std::cout,
            osg::ApplicationUsage::COMMAND_LINE_OPTION);
        return 1;
    }

    // ── Export path ───────────────────────────────────────────────────────────
    std::string exportPath = "output.osgb";
    args.read("-o", exportPath);
    if (osgDB::getLowerCaseFileExtension(exportPath) != "osgb")
        exportPath += ".osgb";

    // ── Load FBX ──────────────────────────────────────────────────────────────
    std::string inputFile = args[args.argc() - 1];
    OSG_NOTICE << "Loading: " << inputFile << std::endl;

    osg::ref_ptr<osg::Node> scene = osgDB::readNodeFile(inputFile);
    if (!scene.valid()) {
        OSG_FATAL << "Failed to load: " << inputFile << std::endl;
        return 1;
    }

    // Apply orientation and scale equivalent to: -o 0,1,0-0,0,1 -s 1000,1000,1000
    // This matches the osgconv OrientationConverter logic:
    //   1. Translate to origin, 2. Rotate from Y-up to Z-up, 3. Scale x1000, 4. Translate back
    {
        osg::Matrix R = osg::Matrix::rotate(osg::Vec3(0,1,0), osg::Vec3(0,0,1));
        osg::Matrix S = osg::Matrix::scale(1000.0f, 1000.0f, 1000.0f);
        osg::BoundingSphere bs = scene->getBound();
        osg::Matrix C = osg::Matrix::translate(-bs.center());
        osg::Matrix T = osg::Matrix::translate(bs.center());

        osg::ref_ptr<osg::MatrixTransform> xform = new osg::MatrixTransform;
        xform->setDataVariance(osg::Object::STATIC);
        xform->setMatrix(C * R * S * T);
        xform->getOrCreateStateSet()->setMode(GL_NORMALIZE, osg::StateAttribute::ON);
        xform->addChild(scene.get());

        osg::ref_ptr<osg::Group> tmpRoot = new osg::Group;
        tmpRoot->addChild(xform.get());
        osgUtil::Optimizer::FlattenStaticTransformsVisitor fstv;
        tmpRoot->accept(fstv);
        fstv.removeTransforms(tmpRoot.get());
        scene = tmpRoot->getChild(0);
    }

    // ── Build hierarchy ───────────────────────────────────────────────────────
    HierarchyVisitor hv;
    scene->accept(hv);
    OSG_NOTICE << "Scene nodes: " << hv.items.size() << std::endl;

    // ── Viewer ────────────────────────────────────────────────────────────────
    const int WIN_W = 1440, WIN_H = 900;

    osgViewer::Viewer viewer;
    viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);
    viewer.setUpViewInWindow(50, 50, WIN_W, WIN_H);

    // Restrict the main (3D) camera to the right side of the window
    viewer.getCamera()->setViewport(PANEL_W, 0, WIN_W - PANEL_W, WIN_H);
    // Keep correct aspect ratio for the restricted viewport
    double fovY = 45.0, ar = (double)(WIN_W - PANEL_W) / (double)WIN_H;
    viewer.getCamera()->setProjectionMatrixAsPerspective(fovY, ar, 0.1, 1e6);

    osg::ref_ptr<osgGA::TrackballManipulator> manip =
        new osgGA::TrackballManipulator;
    viewer.setCameraManipulator(manip.get());

    // ── Panel ─────────────────────────────────────────────────────────────────
    osg::ref_ptr<HierarchyPanel> panel =
        new HierarchyPanel(hv.items, WIN_W, WIN_H, exportPath);

    // Root: scene data + selection overlay + HUD camera
    osg::ref_ptr<osg::Group> selectionGroup = new osg::Group;
    osg::ref_ptr<osg::Group> root = new osg::Group;
    root->addChild(scene.get());
    root->addChild(selectionGroup.get());
    root->addChild(panel->getCamera());
    viewer.setSceneData(root.get());

    // ── Event handlers ────────────────────────────────────────────────────────
    viewer.addEventHandler(new PanelHandler(panel.get(), manip.get(), scene.get(), selectionGroup.get()));
    viewer.addEventHandler(new ResizeHandler(viewer.getCamera()));
    viewer.addEventHandler(new osgViewer::StatsHandler);
    viewer.addEventHandler(new osgViewer::WindowSizeHandler);

    // Focus on the full scene at startup
    viewer.home();

    return viewer.run();
}
