#include "PanelHandler.h"
#include "FileDialogs.h"
#include <osg/Material>
#include <osg/StateSet>
#include <osgUtil/IntersectionVisitor>
#include <osgUtil/LineSegmentIntersector>
#include <osgViewer/View>

// ─── PanelHandler ─────────────────────────────────────────────────────────────

PanelHandler::PanelHandler(HierarchyPanel* panel,
	ExportPanel* exportPanel,
	ConsistentManipulator* manip,
	osg::Node* scene,
	osg::Group* selectionGroup,
	osg::Group* root)
	: _panel(panel), _exportPanel(exportPanel), _manip(manip), _scene(scene), _selectionGroup(selectionGroup), _root(root)
{
}

bool PanelHandler::handle(const osgGA::GUIEventAdapter& ea,
	osgGA::GUIActionAdapter& aa)
{
	const int sx = (int)ea.getX();
	const int sy = (int)ea.getY();
	const bool inLeft = sx >= 0 && sx < PANEL_W;
	const bool inRight = sx >= (_winW - RPANEL_W) && sx < _winW;

	switch (ea.getEventType())
	{
	case osgGA::GUIEventAdapter::SCROLL:
		if (inLeft)
		{
			int d = (ea.getScrollingMotion() == osgGA::GUIEventAdapter::SCROLL_UP)
				? -(ITEM_H * 3)
				: (ITEM_H * 3);
			_panel->scroll(d);
			return true;
		}
		break;

	case osgGA::GUIEventAdapter::PUSH:
		if (inLeft)
		{
			int hit = _panel->hitTest(sx, sy);
			if (hit == HierarchyPanel::HP_HIT_LOAD)
			{
				_loadFile();
			}
			else if (hit >= 0)
			{
				_panel->setSelected(hit);
				_panel->scrollToSelected();
				const HItem* item = _panel->selectedItem();
				if (item && item->node.valid())
				{
					focusCamera(item->node.get(), _manip.get(), _scene.get());
					_updateSelectionBox(item->node.get());
					_applyTint(item->node.get());
				}
			}
			return true;
		}
		if (inRight)
		{
			int hit = _exportPanel->hitTest(sx, sy);
			if (hit != ExportPanel::EP_HIT_NONE)
			{
				if (hit == ExportPanel::EP_HIT_EXPORT)
				{
					// Strip tint so it isn't serialised, then restore it
					osg::ref_ptr<osg::Node> savedNode = _tintedNode;
					osg::ref_ptr<osg::Material> savedMat = _tintMat;
					_removeTint();
					_exportPanel->handleHit(hit, _scene.get());
					if (savedNode.valid())
					{
						_tintedNode = savedNode;
						_tintMat = savedMat;
						savedNode->getOrCreateStateSet()->setAttributeAndModes(
							savedMat.get(),
							osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
					}
				}
				else
				{
					_exportPanel->handleHit(hit, _scene.get());
				}
			}
			return true;
		}
		// Click in 3D viewport — record push position for click detection
		if (ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON)
		{
			_pushX = sx;
			_pushY = sy;
		}
		break;

	case osgGA::GUIEventAdapter::RELEASE:
		if (!inLeft && !inRight && ea.getButton() == osgGA::GUIEventAdapter::LEFT_MOUSE_BUTTON && sx == _pushX && sy == _pushY)
		{
			_pick3D(sx, sy, aa);
		}
		_pushX = _pushY = -1;
		break;

	case osgGA::GUIEventAdapter::MOVE:
	case osgGA::GUIEventAdapter::DRAG:
		_exportPanel->setExportHovered(
			inRight && sy >= 0 && sy < RP_EXPORT_H);
		if (inLeft || inRight)
			return true;
		break;

	case osgGA::GUIEventAdapter::RESIZE:
	{
		_winW = (int)ea.getWindowWidth();
		int nh = (int)ea.getWindowHeight();
		_panel->onResize(_winW, nh);
		_exportPanel->onResize(_winW, nh);
		break;
	}

	case osgGA::GUIEventAdapter::KEYDOWN:
		if (ea.getKey() == 's' || ea.getKey() == 'S')
		{
			_statsVisible = !_statsVisible;
			osg::Node::NodeMask m = _statsVisible ? 0x0 : ~0x0;
			_panel->getCamera()->setNodeMask(m);
			_exportPanel->getCamera()->setNodeMask(m);
			return false;
		}
		break;

	default:
		break;
	}
	return false;
}

void PanelHandler::_pick3D(int sx, int sy, osgGA::GUIActionAdapter& aa)
{
	osgViewer::View* view = dynamic_cast<osgViewer::View*>(&aa);
	if (!view)
		return;

	osg::ref_ptr<osgUtil::LineSegmentIntersector> picker =
		new osgUtil::LineSegmentIntersector(
			osgUtil::Intersector::WINDOW, (double)sx, (double)sy);
	osgUtil::IntersectionVisitor iv(picker.get());
	view->getCamera()->accept(iv);

	if (!picker->containsIntersections())
		return;

	const osgUtil::LineSegmentIntersector::Intersection& hit =
		picker->getFirstIntersection();

	// Walk node path from leaf to root looking for a known hierarchy item
	const osg::NodePath& nodePath = hit.nodePath;
	for (int k = (int)nodePath.size() - 1; k >= 0; --k)
	{
		int idx = _panel->findItemByNode(nodePath[k]);
		if (idx >= 0)
		{
			_panel->setSelected(idx);
			_panel->scrollToSelected();
			const HItem* item = _panel->selectedItem();
			if (item && item->node.valid())
			{
				// Re-centre camera on the clicked world point without changing distance
				_manip->setCenter(hit.getWorldIntersectPoint());
				_updateSelectionBox(item->node.get());
				_applyTint(item->node.get());
			}
			return;
		}
	}
}

void PanelHandler::_loadFile()
{
	std::string path = openFileDialog();
	if (path.empty())
		return;

	OSG_NOTICE << "Loading: " << path << std::endl;
	osg::ref_ptr<osg::Node> newScene = loadAndProcessScene(path);
	if (!newScene.valid())
	{
		OSG_WARN << "Failed to load: " << path << std::endl;
		return;
	}

	_root->replaceChild(_scene.get(), newScene.get());
	_scene = newScene;

	_selectionGroup->removeChildren(0, _selectionGroup->getNumChildren());
	_removeTint();

	HierarchyVisitor hv;
	_scene->accept(hv);
	_panel->reload(hv.items);

	CountDrawablesVisitor cdv;
	_scene->accept(cdv);
	_exportPanel->setDrawableCount(cdv.drawables);

	_manip->setNode(_scene.get());
	_manip->home(0.0);
}

void PanelHandler::_updateSelectionBox(osg::Node* node)
{
	_selectionGroup->removeChildren(0, _selectionGroup->getNumChildren());
	osg::ref_ptr<osg::Geode> box = makeSelectionBox(node);
	if (box.valid())
		_selectionGroup->addChild(box.get());
}

void PanelHandler::_applyTint(osg::Node* node)
{
	_removeTint();
	if (!node)
		return;

	osg::ref_ptr<osg::Material> mat = new osg::Material;
	mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4(0.05f, 0.30f, 0.80f, 1.0f));
	mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4(0.10f, 0.35f, 0.85f, 1.0f));
	mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4(0.40f, 0.60f, 1.00f, 1.0f));

	node->getOrCreateStateSet()->setAttributeAndModes(
		mat.get(),
		osg::StateAttribute::ON | osg::StateAttribute::OVERRIDE);
	_tintedNode = node;
	_tintMat = mat;
}

void PanelHandler::_removeTint()
{
	if (!_tintedNode.valid())
		return;
	osg::StateSet* ss = _tintedNode->getStateSet();
	if (ss && ss->getAttribute(osg::StateAttribute::MATERIAL) == _tintMat.get())
		ss->removeAttribute(osg::StateAttribute::MATERIAL);
	_tintedNode = nullptr;
	_tintMat = nullptr;
}

// ─── ResizeHandler ────────────────────────────────────────────────────────────

ResizeHandler::ResizeHandler(osg::Camera* cam3D) : _cam3D(cam3D) {}

bool ResizeHandler::handle(const osgGA::GUIEventAdapter& ea,
	osgGA::GUIActionAdapter& /*aa*/)
{
	if (ea.getEventType() == osgGA::GUIEventAdapter::RESIZE)
	{
		int nw = (int)ea.getWindowWidth();
		int nh = (int)ea.getWindowHeight();
		_cam3D->setViewport(PANEL_W, 0, nw - PANEL_W - RPANEL_W, nh);
	}
	return false;
}
