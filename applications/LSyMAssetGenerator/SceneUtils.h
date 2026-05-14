#pragma once
#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/observer_ptr>
#include <osg/ref_ptr>
#include <osgGA/CameraManipulator>
#include <string>
#include <vector>

// ─── Hierarchy item ───────────────────────────────────────────────────────────

struct HItem
{
	osg::observer_ptr<osg::Node> node;
	std::string label;
	int depth;
	bool isGroup;
};

// ─── Hierarchy visitor ───────────────────────────────────────────────────────

class HierarchyVisitor : public osg::NodeVisitor
{
public:
	std::vector<HItem> items;

	HierarchyVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

	void apply(osg::Node& node) override
	{
		int depth = static_cast<int>(getNodePath().size()) - 1;
		std::string name = node.getName();
		std::string type = node.className();
		std::string label = name.empty() ? ("[" + type + "]") : (name + "  [" + type + "]");
		bool isGroup = (node.asGroup() != nullptr);
		items.push_back({ &node, label, depth, isGroup });
		traverse(node);
	}
};

// ─── Drawable counter ─────────────────────────────────────────────────────────

class CountDrawablesVisitor : public osg::NodeVisitor
{
public:
	int drawables = 0;
	int geodes = 0;

	CountDrawablesVisitor() : osg::NodeVisitor(TRAVERSE_ACTIVE_CHILDREN) {}

	void apply(osg::Geode& g) override
	{
		++geodes;
		for (unsigned int i = 0; i < g.getNumDrawables(); ++i)
		{
			osg::Drawable* d = g.getDrawable(i);
			osg::Geometry* geom = d ? d->asGeometry() : nullptr;
			if (geom)
			{
				if (geom->getNumPrimitiveSets() > 0)
					++drawables;
			}
			else if (d)
			{
				++drawables;
			}
		}
		traverse(g);
	}
};

// ─── Scene stats ─────────────────────────────────────────────────────────────

struct SceneStats
{
	int nodes = 0;
	int drawables = 0;
	int vertices = 0;
};

SceneStats collectSceneStats(osg::Node* node);

// ─── Free functions ───────────────────────────────────────────────────────────

osg::ref_ptr<osg::Node> loadAndProcessScene(const std::string& path);

osg::Geode* makeSelectionBox(osg::Node* node);

void focusCamera(osg::Node* node,
	osgGA::CameraManipulator* manip,
	osg::Node* sceneRoot);
