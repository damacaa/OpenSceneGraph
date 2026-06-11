#pragma once
#include "UIConstants.h"
#include "ExportSettings.h"
#include "NodeUserData.h"
#include <osg/Camera>
#include <osg/Geode>
#include <osg/Node>
#include <osg/ref_ptr>
#include <vector>

class ExportPanel : public osg::Referenced
{
public:
	static const int EP_HIT_NONE = -1;
	static const int EP_HIT_FMT_OSGB = 0;
	static const int EP_HIT_FMT_OSG = 1;
	static const int EP_HIT_FMT_OSGT = 2;
	static const int EP_HIT_COMP_NONE = 3;
	static const int EP_HIT_COMP_FAST = 4;
	static const int EP_HIT_COMP_ZLIB = 5;
	static const int EP_HIT_COMP_BEST = 6;
	static const int EP_HIT_IMG_INLINE = 7;
	static const int EP_HIT_IMG_INCFILE = 8;
	static const int EP_HIT_IMG_EXT = 16;
	static const int EP_HIT_SCALE_0001 = 17;
	static const int EP_HIT_SCALE_1 = 18;
	static const int EP_HIT_SCALE_1000 = 19;
	static const int EP_HIT_AXIS_NONE = 23;
	static const int EP_HIT_AXIS_Y2Z = 24;
	static const int EP_HIT_AXIS_Z2Y = 25;
	static const int EP_HIT_CB_MERGE = 9;
	static const int EP_HIT_CB_SHARE = 10;
	static const int EP_HIT_CB_STRIP = 11;
	static const int EP_HIT_CB_FLATTEN = 12;
	static const int EP_HIT_CB_INDEX = 13;
	static const int EP_HIT_CB_REDUND = 14;
	static const int EP_HIT_EXPORT = 15;
	static const int EP_HIT_EXPORT_DUMMIES = 39;

	ExportPanel(int winW, int winH, int drawableCount);

	osg::Camera* getCamera();

	int hitTest(int sx, int sy) const;
	void handleHit(int code, osg::Node* scene);
	void setExportHovered(bool h);
	void setExportDummiesHovered(bool h);
	void setDrawableCount(int n);
	void setSourcePath(const std::string& path) { _sourcePath = path; }
	void onResize(int w, int h);

private:
	struct HitZone
	{
		int x, y, w, h, code;
	};

	int _winW, _winH;
	int _drawableCount;
	std::string _sourcePath;
	bool _exportHovered;
	bool _exportDummiesHovered;
	ExportSettings _settings;
	std::vector<HitZone> _hitZones;

	osg::ref_ptr<osg::Camera> _hudCamera;
	osg::ref_ptr<osg::Geode> _exportBtnGeode;
	osg::ref_ptr<osg::Geode> _exportBtnTextGeode;
	osg::ref_ptr<osg::Geode> _exportDummiesBtnGeode;
	osg::ref_ptr<osg::Geode> _exportDummiesBtnTextGeode;

	int panelX() const;

	void _buildCamera();
	void _buildScene();
	void _addLabel(const char* text, int px, int y);
	void _addDivider(int px, int pw, int y);
	void _drawToggleBtn(int x, int y, int w, int h, const char* label, bool active);
	void _drawCheckbox(int x, int y, int w, const char* label, bool checked);
	void _rebuildExportButton();
	void _doExport(osg::Node* scene);
	void _doExportDummies(osg::Node* scene);
};
