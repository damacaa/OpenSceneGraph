#include "UIWidgets.h"
#include <osg/Array>
#include <osg/PrimitiveSet>
#include <osg/StateSet>

osg::Geometry* makeQuad(float x, float y, float w, float h, const osg::Vec4& col)
{
	osg::Geometry* g = new osg::Geometry;
	g->setDataVariance(osg::Object::DYNAMIC);

	osg::Vec3Array* v = new osg::Vec3Array(4);
	(*v)[0].set(x, y, 0.f);
	(*v)[1].set(x + w, y, 0.f);
	(*v)[2].set(x + w, y + h, 0.f);
	(*v)[3].set(x, y + h, 0.f);
	g->setVertexArray(v);

	osg::Vec4Array* c = new osg::Vec4Array(1);
	(*c)[0] = col;
	g->setColorArray(c, osg::Array::BIND_OVERALL);

	g->addPrimitiveSet(new osg::DrawArrays(GL_QUADS, 0, 4));
	g->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
	return g;
}

osgText::Text* makeText(const std::string& str, float x, float y,
	float size, const osg::Vec4& col,
	osgText::Text::AlignmentType align)
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
