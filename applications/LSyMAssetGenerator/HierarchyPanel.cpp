#include "HierarchyPanel.h"
#include "UIWidgets.h"
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/Group>
#include <osg/Scissor>
#include <osg/StateSet>
#include <osgText/Text>
#include <algorithm>

HierarchyPanel::HierarchyPanel(const std::vector<HItem>& items, int winW, int winH)
	: _items(items), _winW(winW), _winH(winH), _scroll(0), _selIdx(-1)
{
	_buildCamera();
	_buildScene();
}

osg::Camera* HierarchyPanel::getCamera() { return _hudCamera.get(); }

int HierarchyPanel::listAreaTop() const { return _winH - HEADER_H - LOAD_H; }
int HierarchyPanel::listAreaBottom() const { return RP_PAD; }
int HierarchyPanel::listAreaH() const { return listAreaTop() - listAreaBottom(); }
int HierarchyPanel::totalListH() const { return static_cast<int>(_items.size()) * ITEM_H; }
int HierarchyPanel::maxScroll() const { return std::max(0, totalListH() - listAreaH()); }

void HierarchyPanel::scroll(int delta)
{
	_scroll = std::max(0, std::min(maxScroll(), _scroll + delta));
	_applyScroll();
	_rebuildScrollbar();
}

int HierarchyPanel::hitTest(int sx, int sy) const
{
	if (sx < 0 || sx >= PANEL_W)
		return -1;
	if (sy >= _winH - HEADER_H)
		return -1;
	if (sy >= listAreaTop() && sy < _winH - HEADER_H)
		return HP_HIT_LOAD;
	if (sy < listAreaBottom())
		return -1;
	int fromTop = (listAreaTop() - 1) - sy;
	int idx = (fromTop + _scroll) / ITEM_H;
	if (idx < 0 || idx >= static_cast<int>(_items.size()))
		return -1;
	return idx;
}

void HierarchyPanel::setSelected(int idx)
{
	_selIdx = idx;
	_updateHighlight();
}

const HItem* HierarchyPanel::selectedItem() const
{
	if (_selIdx < 0 || _selIdx >= static_cast<int>(_items.size()))
		return nullptr;
	return &_items[_selIdx];
}

int HierarchyPanel::findItemByNode(osg::Node* node) const
{
	if (!node)
		return -1;
	for (int i = 0; i < (int)_items.size(); ++i)
		if (_items[i].node.get() == node)
			return i;
	return -1;
}

void HierarchyPanel::scrollToSelected()
{
	if (_selIdx < 0 || _selIdx >= (int)_items.size())
		return;
	// Scroll range that keeps item i fully visible:
	//   lower bound: scroll >= (i+1)*ITEM_H - listAreaH()
	//   upper bound: scroll <= i*ITEM_H
	int lo = (_selIdx + 1) * ITEM_H - listAreaH();
	int hi = _selIdx * ITEM_H;
	int target = std::max(lo, std::min(hi, _scroll));
	target = std::max(0, std::min(maxScroll(), target));
	if (target != _scroll)
	{
		_scroll = target;
		_applyScroll();
		_rebuildScrollbar();
	}
}

void HierarchyPanel::onResize(int w, int h)
{
	_winW = w;
	_winH = h;
	_hudCamera->setProjectionMatrixAsOrtho2D(0, w, 0, h);
	_hudCamera->setViewport(0, 0, w, h);
	_hudCamera->removeChildren(0, _hudCamera->getNumChildren());
	_buildScene();
}

void HierarchyPanel::reload(const std::vector<HItem>& items)
{
	_items = items;
	_scroll = 0;
	_selIdx = -1;
	_hudCamera->removeChildren(0, _hudCamera->getNumChildren());
	_buildScene();
}

void HierarchyPanel::_buildCamera()
{
	_hudCamera = new osg::Camera;
	_hudCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
	_hudCamera->setProjectionMatrixAsOrtho2D(0, _winW, 0, _winH);
	_hudCamera->setViewMatrix(osg::Matrix::identity());
	_hudCamera->setViewport(0, 0, _winW, _winH);
	_hudCamera->setRenderOrder(osg::Camera::POST_RENDER);
	_hudCamera->setClearMask(0);

	osg::StateSet* ss = _hudCamera->getOrCreateStateSet();
	ss->setMode(GL_BLEND, osg::StateAttribute::ON);
	ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	ss->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
	ss->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
	ss->setAttributeAndModes(new osg::Depth(osg::Depth::ALWAYS, 0.0, 1.0, false));
}

void HierarchyPanel::_buildScene()
{
	// Background
	{
		osg::Geode* bg = new osg::Geode;
		bg->addDrawable(makeQuad(0, 0, PANEL_W, _winH, C_PANEL_BG));
		_hudCamera->addChild(bg);
	}
	// Right-edge divider
	{
		osg::Geode* div = new osg::Geode;
		div->addDrawable(makeQuad(PANEL_W - 1, 0, 1, _winH, C_DIVIDER));
		_hudCamera->addChild(div);
	}
	// Header
	{
		osg::Geode* hdrBg = new osg::Geode;
		hdrBg->addDrawable(makeQuad(0, _winH - HEADER_H, PANEL_W, HEADER_H, C_HEADER_BG));
		hdrBg->addDrawable(makeQuad(0, _winH - HEADER_H - 1, PANEL_W, 1, C_DIVIDER));
		_hudCamera->addChild(hdrBg);

		osg::Geode* hdrText = new osg::Geode;
		hdrText->addDrawable(makeText("Scene Hierarchy",
			10.f, (float)(_winH - HEADER_H + 13),
			FONT_SZ + 2.f, C_TEXT));
		_hudCamera->addChild(hdrText);
	}
	// Load File button strip
	{
		int stripY = listAreaTop();
		osg::Geode* loadBg = new osg::Geode;
		loadBg->addDrawable(makeQuad(6.f, stripY + 4.f,
			PANEL_W - 12.f, LOAD_H - 8.f, C_LOAD));
		_hudCamera->addChild(loadBg);

		osg::Geode* loadText = new osg::Geode;
		loadText->addDrawable(makeText("Open File",
			PANEL_W / 2.f, stripY + 8.f,
			FONT_SZ, C_TEXT,
			osgText::Text::CENTER_BOTTOM));
		_hudCamera->addChild(loadText);

		osg::Geode* div = new osg::Geode;
		div->addDrawable(makeQuad(0, stripY - 1, PANEL_W, 1, C_DIVIDER));
		_hudCamera->addChild(div);
	}
	// Scrollable list area
	{
		osg::Group* clip = new osg::Group;
		osg::Scissor* sc = new osg::Scissor(0, listAreaBottom(), PANEL_W, listAreaH());
		clip->getOrCreateStateSet()->setAttributeAndModes(
			sc, osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);

		_listXform = new osg::MatrixTransform;
		_listXform->setDataVariance(osg::Object::DYNAMIC);
		_applyScroll();

		osg::Geode* hlGeode = new osg::Geode;
		_hlQuad = makeQuad(0, 0, PANEL_W - 8, ITEM_H, osg::Vec4(0, 0, 0, 0));
		hlGeode->addDrawable(_hlQuad.get());
		_listXform->addChild(hlGeode);

		osg::Geode* textGeode = new osg::Geode;
		_labelTexts.clear();
		_labelTexts.resize(_items.size());
		for (int i = 0; i < (int)_items.size(); ++i)
		{
			const HItem& it = _items[i];
			float y = (float)(listAreaTop() - (i + 1) * ITEM_H);
			float tx = 8.f + it.depth * INDENT_PX;
			if (it.isGroup)
				textGeode->addDrawable(makeText(">", tx, y + 6.f, FONT_SZ, C_TEXT_DIM));
			osgText::Text* lbl = makeText(it.label, tx + INDENT_PX, y + 4.f, FONT_SZ, C_TEXT);
			textGeode->addDrawable(lbl);
			_labelTexts[i] = lbl;
		}
		_listXform->addChild(textGeode);
		clip->addChild(_listXform.get());

		_scrollGeode = new osg::Geode;
		_rebuildScrollbar();
		clip->addChild(_scrollGeode.get());

		_hudCamera->addChild(clip);
	}
}

void HierarchyPanel::_applyScroll()
{
	if (_listXform.valid())
		_listXform->setMatrix(osg::Matrix::translate(0.0, _scroll, 0.0));
}

void HierarchyPanel::_updateHighlight()
{
	if (!_hlQuad.valid())
		return;
	osg::Vec3Array* verts = static_cast<osg::Vec3Array*>(_hlQuad->getVertexArray());
	osg::Vec4Array* colors = static_cast<osg::Vec4Array*>(_hlQuad->getColorArray());

	if (_selIdx < 0 || _selIdx >= (int)_items.size())
	{
		(*colors)[0] = osg::Vec4(0, 0, 0, 0);
		colors->dirty();
		// Reset all label colours
		for (auto& t : _labelTexts)
			if (t.valid())
				t->setColor(C_TEXT);
		return;
	}

	// Move selection quad to the selected row
	float y = (float)(listAreaTop() - (_selIdx + 1) * ITEM_H);
	(*verts)[0].set(0, y, 0.f);
	(*verts)[1].set(PANEL_W - 8, y, 0.f);
	(*verts)[2].set(PANEL_W - 8, y + ITEM_H, 0.f);
	(*verts)[3].set(0, y + ITEM_H, 0.f);
	verts->dirty();
	(*colors)[0] = C_SEL;
	colors->dirty();
	_hlQuad->dirtyBound();

	// Highlight selected label gold, restore all others
	for (int i = 0; i < (int)_labelTexts.size(); ++i)
	{
		if (_labelTexts[i].valid())
			_labelTexts[i]->setColor(i == _selIdx ? C_TEXT_SEL : C_TEXT);
	}
}

void HierarchyPanel::_rebuildScrollbar()
{
	if (!_scrollGeode.valid())
		return;
	_scrollGeode->removeDrawables(0, _scrollGeode->getNumDrawables());
	int total = totalListH(), avail = listAreaH();
	if (total <= avail)
		return;
	int barH = std::max(24, avail * avail / total);
	float t = (total > avail) ? (float)_scroll / (float)(total - avail) : 0.f;
	int barY = listAreaBottom() + avail - barH - (int)(t * (avail - barH));
	_scrollGeode->addDrawable(makeQuad(PANEL_W - 6, barY, 4, barH, C_SCROLLBAR));
}
