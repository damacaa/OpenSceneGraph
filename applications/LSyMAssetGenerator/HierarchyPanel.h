#pragma once
#include "UIConstants.h"
#include "SceneUtils.h"
#include <osg/Camera>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/MatrixTransform>
#include <osg/ref_ptr>
#include <osgText/Text>
#include <vector>

class HierarchyPanel : public osg::Referenced
{
public:
	HierarchyPanel(const std::vector<HItem>& items, int winW, int winH);

	osg::Camera* getCamera();

	static const int HP_HIT_LOAD = -2;

	int listAreaTop() const;
	int listAreaBottom() const;
	int listAreaH() const;
	int totalListH() const;
	int maxScroll() const;

	void scroll(int delta);

	// Returns item index (>=0), HP_HIT_LOAD (-2), or -1 for miss.
	int hitTest(int sx, int sy) const;

	void setSelected(int idx);
	const HItem* selectedItem() const;

	// Find item index for a given node pointer, or -1 if not found.
	int findItemByNode(osg::Node* node) const;

	// Scroll the list so the selected item is visible.
	void scrollToSelected();

	void onResize(int w, int h);
	void reload(const std::vector<HItem>& items);

private:
	osg::ref_ptr<osg::Camera> _hudCamera;
	osg::ref_ptr<osg::MatrixTransform> _listXform;
	osg::ref_ptr<osg::Geometry> _hlQuad;
	osg::ref_ptr<osg::Geode> _scrollGeode;

	std::vector<HItem> _items;
	std::vector<osg::ref_ptr<osgText::Text>> _labelTexts; // one per item, for colour updates
	int _winW, _winH;
	int _scroll;
	int _selIdx;

	void _buildCamera();
	void _buildScene();
	void _applyScroll();
	void _updateHighlight();
	void _rebuildScrollbar();
};
