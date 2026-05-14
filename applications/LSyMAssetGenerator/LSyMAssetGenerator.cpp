// LSyMAssetGenerator - Load a 3D file and display it with an interactive node-hierarchy panel.
//
// Layout:
//   [Left: Hierarchy Panel | 3-D View | User-Data Panel | Right: Export Panel]

#include "PanelHandler.h"
#include "ConsistentManipulator.h"
#include "HierarchyPanel.h"
#include "ExportPanel.h"
#include "UserDataPanel.h"
#include "SceneUtils.h"
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

int main(int argc, char** argv)
{
	const int WIN_W = 1440, WIN_H = 900;

	osgViewer::Viewer viewer;
	viewer.setThreadingModel(osgViewer::Viewer::SingleThreaded);
	viewer.setUpViewInWindow(50, 50, WIN_W, WIN_H);

	const int VP_W = WIN_W - PANEL_W - RPANEL_W - UDPANEL_W;
	viewer.getCamera()->setViewport(PANEL_W, 0, VP_W, WIN_H);
	double fovY = 45.0;
	double ar = (double)VP_W / (double)WIN_H;
	viewer.getCamera()->setProjectionMatrixAsPerspective(fovY, ar, 0.1, 1e6);

	osg::ref_ptr<ConsistentManipulator> manip = new ConsistentManipulator;
	viewer.setCameraManipulator(manip.get());

	osg::ref_ptr<osg::Node> scene = new osg::Group;
	osg::ref_ptr<osg::Group> selectionGroup = new osg::Group;
	osg::ref_ptr<osg::Group> root = new osg::Group;
	root->addChild(scene.get());
	root->addChild(selectionGroup.get());

	osg::ref_ptr<HierarchyPanel> panel =
		new HierarchyPanel(std::vector<HItem>{}, WIN_W, WIN_H);
	osg::ref_ptr<ExportPanel> exportPanel =
		new ExportPanel(WIN_W, WIN_H, 0);
	osg::ref_ptr<UserDataPanel> udPanel =
		new UserDataPanel(WIN_W, WIN_H);

	root->addChild(panel->getCamera());
	root->addChild(udPanel->getCamera());
	root->addChild(exportPanel->getCamera());
	viewer.setSceneData(root.get());

	viewer.addEventHandler(new PanelHandler(
		panel.get(), exportPanel.get(), udPanel.get(), manip.get(),
		scene.get(), selectionGroup.get(), root.get()));
	viewer.addEventHandler(new ResizeHandler(viewer.getCamera()));
	viewer.addEventHandler(new osgViewer::StatsHandler);
	viewer.addEventHandler(new osgViewer::WindowSizeHandler);

	return viewer.run();
}
