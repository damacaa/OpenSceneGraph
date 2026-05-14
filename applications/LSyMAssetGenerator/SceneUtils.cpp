#include "SceneUtils.h"
#include <osg/Array>
#include <osg/BoundingBox>
#include <osg/BoundingSphere>
#include <osg/ComputeBoundsVisitor>
#include <osg/Group>
#include <osg/LineWidth>
#include <osg/MatrixTransform>
#include <osg/PrimitiveSet>
#include <osg/StateSet>
#include <osgDB/ReadFile>
#include <osgAnimation/BasicAnimationManager>
#include <osgUtil/Optimizer>
#include <algorithm>
#include <cmath>

// ─── Scene stats ─────────────────────────────────────────────────────────────

namespace
{
struct StatsVisitor : public osg::NodeVisitor
{
	int nodes = 0;
	int drawables = 0;
	int vertices = 0;

	StatsVisitor() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}

	void apply(osg::Node& n) override
	{
		++nodes;
		traverse(n);
	}

	void apply(osg::Geode& g) override
	{
		++nodes;
		for (unsigned int i = 0; i < g.getNumDrawables(); ++i)
		{
			osg::Geometry* geom = g.getDrawable(i) ? g.getDrawable(i)->asGeometry() : nullptr;
			if (!geom)
			{
				++drawables;
				continue;
			}
			++drawables;
			osg::Array* verts = geom->getVertexArray();
			if (verts)
				vertices += static_cast<int>(verts->getNumElements());
		}
		traverse(g);
	}
};
} // namespace

SceneStats collectSceneStats(osg::Node* node)
{
	SceneStats s;
	if (!node)
		return s;
	StatsVisitor v;
	node->accept(v);
	s.nodes = v.nodes;
	s.drawables = v.drawables;
	s.vertices = v.vertices;
	return s;
}

// ─── Scene loading ────────────────────────────────────────────────────────────

osg::ref_ptr<osg::Node> loadAndProcessScene(const std::string& path)
{
	osg::ref_ptr<osg::Node> scene = osgDB::readNodeFile(path);
	if (!scene.valid())
		return {};

	// Name the root node after the bare filename (no path, no extension)
	{
		std::string name = path;
		size_t slash = name.find_last_of("/\\");
		if (slash != std::string::npos) name = name.substr(slash + 1);
		size_t dot = name.rfind('.');
		if (dot != std::string::npos) name = name.substr(0, dot);
		scene->setName(name);
	}

	// The FBX plugin attaches BasicAnimationManager as an update callback on
	// the root or on a child node (when an axis-correction MatrixTransform wraps
	// it). Walk the whole tree to find it, then start all registered animations.
	struct FindAnimMgr : public osg::NodeVisitor
	{
		osgAnimation::BasicAnimationManager* mgr = nullptr;
		FindAnimMgr() : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN) {}
		void apply(osg::Node& n) override
		{
			if (!mgr)
				mgr = dynamic_cast<osgAnimation::BasicAnimationManager*>(
					n.getUpdateCallback());
			traverse(n);
		}
	};

	FindAnimMgr finder;
	scene->accept(finder);
	if (finder.mgr)
		for (auto& anim : finder.mgr->getAnimationList())
		{
			anim->setPlayMode(osgAnimation::Animation::LOOP);
			finder.mgr->playAnimation(anim.get());
		}

	return scene;
}

// ─── Selection box ────────────────────────────────────────────────────────────

osg::Geode* makeSelectionBox(osg::Node* node)
{
	if (!node)
		return nullptr;

	osg::ComputeBoundsVisitor cbv;
	node->accept(cbv);
	osg::BoundingBox bb = cbv.getBoundingBox();
	if (!bb.valid())
	{
		osg::BoundingSphere bs = node->getBound();
		bb.expandBy(bs);
	}
	if (!bb.valid())
		return nullptr;

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

	static const int edges[24] = {
		0, 1, 1, 2, 2, 3, 3, 0,
		4, 5, 5, 6, 6, 7, 7, 4,
		0, 4, 1, 5, 2, 6, 3, 7 };

	osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
	verts->reserve(24);
	for (int i = 0; i < 24; ++i)
		verts->push_back(c[edges[i]]);

	osg::ref_ptr<osg::Vec4Array> color = new osg::Vec4Array;
	color->push_back(osg::Vec4(1.0f, 0.85f, 0.0f, 1.0f));

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

void focusCamera(osg::Node* node,
	osgGA::CameraManipulator* manip,
	osg::Node* sceneRoot)
{
	if (!node || !manip)
		return;

	osg::BoundingSphere bs = node->getBound();
	osg::Vec3d center = bs.center();
	double radius = bs.radius();

	osg::NodePathList paths = node->getParentalNodePaths();
	if (!paths.empty())
	{
		osg::Matrix w = osg::computeLocalToWorld(paths[0]);
		osg::Vec4d c4(center.x(), center.y(), center.z(), 1.0);
		c4 = c4 * w;
		center.set(c4.x(), c4.y(), c4.z());

		double sx = std::abs(w(0, 0));
		double sy = std::abs(w(1, 1));
		double sz = std::abs(w(2, 2));
		radius *= std::max({ sx, sy, sz });
	}

	if (radius < 1e-4)
	{
		if (sceneRoot)
		{
			osg::BoundingSphere sb = sceneRoot->getBound();
			center = sb.center();
			radius = sb.radius();
		}
		else
		{
			radius = 1.0;
		}
	}

	double dist = radius * 6.0;
	osg::Vec3d eye = center + osg::Vec3d(0.0, -dist, dist * 0.5);
	manip->setHomePosition(eye, center, osg::Vec3d(0, 0, 1));
	manip->home(0.0);
}
