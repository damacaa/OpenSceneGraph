#pragma once
#include "HierarchyPanel.h"
#include "ExportPanel.h"
#include "ConsistentManipulator.h"
#include "SceneUtils.h"
#include "UIConstants.h"
#include <osgGA/GUIEventHandler>
#include <osg/Camera>
#include <osg/Group>
#include <osg/Material>
#include <osg/Node>
#include <osg/ref_ptr>

class PanelHandler : public osgGA::GUIEventHandler
{
public:
	PanelHandler(HierarchyPanel* panel,
		ExportPanel* exportPanel,
		ConsistentManipulator* manip,
		osg::Node* scene,
		osg::Group* selectionGroup,
		osg::Group* root);

	bool handle(const osgGA::GUIEventAdapter& ea,
		osgGA::GUIActionAdapter& aa) override;

private:
	void _loadFile();
	void _updateSelectionBox(osg::Node* node);
	void _applyTint(osg::Node* node);
	void _removeTint();
	void _pick3D(int sx, int sy, osgGA::GUIActionAdapter& aa);

	osg::ref_ptr<HierarchyPanel> _panel;
	osg::ref_ptr<ExportPanel> _exportPanel;
	osg::ref_ptr<ConsistentManipulator> _manip;
	osg::ref_ptr<osg::Node> _scene;
	osg::ref_ptr<osg::Group> _selectionGroup;
	osg::ref_ptr<osg::Group> _root;
	osg::ref_ptr<osg::Node> _tintedNode;
	osg::ref_ptr<osg::Material> _tintMat;
	int _winW = 1440;
	bool _statsVisible = false;
	int _pushX = -1;
	int _pushY = -1;
};

class ResizeHandler : public osgGA::GUIEventHandler
{
public:
	explicit ResizeHandler(osg::Camera* cam3D);

	bool handle(const osgGA::GUIEventAdapter& ea,
		osgGA::GUIActionAdapter& aa) override;

private:
	osg::ref_ptr<osg::Camera> _cam3D;
};
