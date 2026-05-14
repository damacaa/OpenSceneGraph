#pragma once
#include "UIConstants.h"
#include <osg/Geometry>
#include <osgText/Text>
#include <string>

osg::Geometry* makeQuad(float x, float y, float w, float h, const osg::Vec4& col);

osgText::Text* makeText(const std::string& str, float x, float y,
	float size, const osg::Vec4& col,
	osgText::Text::AlignmentType align = osgText::Text::LEFT_BOTTOM);
