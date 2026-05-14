#include "ExportPanel.h"
#include "SceneUtils.h"
#include "UIWidgets.h"
#include "FileDialogs.h"
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/StateSet>
#include <osgDB/WriteFile>
#include <osg/MatrixTransform>
#include <osgUtil/Optimizer>
#include <osgText/Text>

ExportPanel::ExportPanel(int winW, int winH, int drawableCount)
	: _winW(winW), _winH(winH), _drawableCount(drawableCount), _exportHovered(false)
{
	_buildCamera();
	_buildScene();
}

osg::Camera* ExportPanel::getCamera() { return _hudCamera.get(); }

int ExportPanel::panelX() const { return _winW - RPANEL_W; }

int ExportPanel::hitTest(int sx, int sy) const
{
	if (sx < panelX() || sx >= _winW)
		return EP_HIT_NONE;
	if (sy < 0 || sy >= _winH)
		return EP_HIT_NONE;
	if (sy >= 0 && sy < RP_EXPORT_H)
		return EP_HIT_EXPORT;
	for (const auto& hz : _hitZones)
		if (sx >= hz.x && sx < hz.x + hz.w && sy >= hz.y && sy < hz.y + hz.h)
			return hz.code;
	return EP_HIT_NONE;
}

void ExportPanel::handleHit(int code, osg::Node* scene)
{
	switch (code)
	{
	case EP_HIT_FMT_OSGB:
		_settings.format = ExportSettings::FMT_OSGB;
		break;
	case EP_HIT_FMT_OSG:
		_settings.format = ExportSettings::FMT_OSG;
		break;
	case EP_HIT_FMT_OSGT:
		_settings.format = ExportSettings::FMT_OSGT;
		break;
	case EP_HIT_COMP_NONE:
		_settings.compression = ExportSettings::COMP_NONE;
		break;
	case EP_HIT_COMP_FAST:
		_settings.compression = ExportSettings::COMP_ZLIB_FAST;
		break;
	case EP_HIT_COMP_ZLIB:
		_settings.compression = ExportSettings::COMP_ZLIB;
		break;
	case EP_HIT_COMP_BEST:
		_settings.compression = ExportSettings::COMP_ZLIB_BEST;
		break;
	case EP_HIT_IMG_INLINE:
		_settings.imageMode = ExportSettings::IMG_INLINE;
		break;
	case EP_HIT_IMG_INCFILE:
		_settings.imageMode = ExportSettings::IMG_INCLUDE_FILE;
		break;
	case EP_HIT_IMG_EXT:
		_settings.imageMode = ExportSettings::IMG_EXTERNAL;
		break;
	case EP_HIT_SCALE_0001:
		_settings.scale = ExportSettings::SCALE_0001;
		break;
	case EP_HIT_SCALE_1:
		_settings.scale = ExportSettings::SCALE_1;
		break;
	case EP_HIT_SCALE_1000:
		_settings.scale = ExportSettings::SCALE_1000;
		break;
	case EP_HIT_AXIS_NONE:
		_settings.axis = ExportSettings::AXIS_NONE;
		break;
	case EP_HIT_AXIS_Y2Z:
		_settings.axis = ExportSettings::AXIS_Y2Z;
		break;
	case EP_HIT_AXIS_Z2Y:
		_settings.axis = ExportSettings::AXIS_Z2Y;
		break;
	case EP_HIT_CB_MERGE:
		_settings.mergeGeom = !_settings.mergeGeom;
		break;
	case EP_HIT_CB_SHARE:
		_settings.shareState = !_settings.shareState;
		break;
	case EP_HIT_CB_STRIP:
		_settings.tristrip = !_settings.tristrip;
		break;
	case EP_HIT_CB_FLATTEN:
		_settings.flattenTransforms = !_settings.flattenTransforms;
		break;
	case EP_HIT_CB_INDEX:
		_settings.indexMesh = !_settings.indexMesh;
		break;
	case EP_HIT_CB_REDUND:
		_settings.removeRedundant = !_settings.removeRedundant;
		break;
	case EP_HIT_EXPORT:
		_doExport(scene);
		return;
	default:
		return;
	}
	_hudCamera->removeChildren(0, _hudCamera->getNumChildren());
	_exportBtnGeode = nullptr;
	_exportBtnTextGeode = nullptr;
	_buildScene();
}

void ExportPanel::setExportHovered(bool h)
{
	if (_exportHovered == h)
		return;
	_exportHovered = h;
	_rebuildExportButton();
}

void ExportPanel::setDrawableCount(int n)
{
	_drawableCount = n;
	_hudCamera->removeChildren(0, _hudCamera->getNumChildren());
	_exportBtnGeode = nullptr;
	_exportBtnTextGeode = nullptr;
	_buildScene();
}

void ExportPanel::onResize(int w, int h)
{
	if (w <= 0 || h <= 0) return;
	_winW = w;
	_winH = h;
	_hudCamera->setProjectionMatrixAsOrtho2D(0, w, 0, h);
	_hudCamera->setViewport(0, 0, w, h);
	_hudCamera->removeChildren(0, _hudCamera->getNumChildren());
	_exportBtnGeode = nullptr;
	_exportBtnTextGeode = nullptr;
	_buildScene();
}

void ExportPanel::_buildCamera()
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

void ExportPanel::_buildScene()
{
	_hitZones.clear();

	const int px = panelX();
	const int pw = RPANEL_W;
	const int bw = pw - 2 * RP_PAD;
	int y = _winH;

	// Background + left edge divider
	{
		osg::Geode* bg = new osg::Geode;
		bg->addDrawable(makeQuad((float)px, 0, (float)pw, (float)_winH, C_RPANEL_BG));
		bg->addDrawable(makeQuad((float)px, 0, 1.f, (float)_winH, C_DIVIDER));
		_hudCamera->addChild(bg);
	}

	// Header
	y -= HEADER_H;
	{
		osg::Geode* hdr = new osg::Geode;
		hdr->addDrawable(makeQuad((float)px, (float)y, (float)pw, (float)HEADER_H, C_HEADER_BG));
		hdr->addDrawable(makeQuad((float)px, (float)(y - 1), (float)pw, 1.f, C_DIVIDER));
		_hudCamera->addChild(hdr);

		osg::Geode* t = new osg::Geode;
		t->addDrawable(makeText("Export Settings",
			(float)(px + RP_PAD), (float)(y + 13),
			FONT_SZ + 2.f, C_TEXT));
		_hudCamera->addChild(t);
	}

	// Stats section
	y -= RP_SEP;
	y -= RP_LABEL_H;
	_addLabel("STATS", px, y);

	y -= RP_CB_H;
	{
		osg::Geode* t = new osg::Geode;
		t->addDrawable(makeText("Draw Calls",
			(float)(px + RP_PAD), (float)(y + 6), FONT_SZ, C_TEXT));
		t->addDrawable(makeText(std::to_string(_drawableCount),
			(float)(px + pw - RP_PAD), (float)(y + 6),
			FONT_SZ, C_STAT_VAL, osgText::Text::RIGHT_BOTTOM));
		_hudCamera->addChild(t);
	}
	y -= RP_SEP;
	_addDivider(px, pw, y);
	y -= 1;

	// Format section
	y -= RP_SEP;
	y -= RP_LABEL_H;
	_addLabel("FORMAT", px, y);
	y -= RP_BTN_H;
	{
		const int bw3 = (bw - 4) / 3;
		struct
		{
			const char* lbl;
			int code;
			ExportSettings::Format val;
		} b[3] = {
			{"osgb", EP_HIT_FMT_OSGB, ExportSettings::FMT_OSGB},
			{"osg", EP_HIT_FMT_OSG, ExportSettings::FMT_OSG},
			{"osgt", EP_HIT_FMT_OSGT, ExportSettings::FMT_OSGT},
		};
		for (int i = 0; i < 3; ++i)
		{
			int bx = px + RP_PAD + i * (bw3 + 2);
			_drawToggleBtn(bx, y, bw3, RP_BTN_H, b[i].lbl,
				_settings.format == b[i].val);
			_hitZones.push_back({ bx, y, bw3, RP_BTN_H, b[i].code });
		}
	}
	y -= RP_SEP;
	_addDivider(px, pw, y);
	y -= 1;

	// Compression section
	y -= RP_SEP;
	y -= RP_LABEL_H;
	_addLabel("COMPRESSION", px, y);
	y -= RP_BTN_H;
	{
		const int bw2 = (bw - 2) / 2;
		struct
		{
			const char* lbl;
			int code;
			ExportSettings::Compression val;
		} row1[2] = {
			{"None", EP_HIT_COMP_NONE, ExportSettings::COMP_NONE},
			{"Fast", EP_HIT_COMP_FAST, ExportSettings::COMP_ZLIB_FAST},
		};
		for (int i = 0; i < 2; ++i)
		{
			int bx = px + RP_PAD + i * (bw2 + 2);
			_drawToggleBtn(bx, y, bw2, RP_BTN_H, row1[i].lbl,
				_settings.compression == row1[i].val);
			_hitZones.push_back({ bx, y, bw2, RP_BTN_H, row1[i].code });
		}
	}
	y -= RP_BTN_H + 2;
	{
		const int bw2 = (bw - 2) / 2;
		struct
		{
			const char* lbl;
			int code;
			ExportSettings::Compression val;
		} row2[2] = {
			{"Default", EP_HIT_COMP_ZLIB, ExportSettings::COMP_ZLIB},
			{"Best", EP_HIT_COMP_BEST, ExportSettings::COMP_ZLIB_BEST},
		};
		for (int i = 0; i < 2; ++i)
		{
			int bx = px + RP_PAD + i * (bw2 + 2);
			_drawToggleBtn(bx, y, bw2, RP_BTN_H, row2[i].lbl,
				_settings.compression == row2[i].val);
			_hitZones.push_back({ bx, y, bw2, RP_BTN_H, row2[i].code });
		}
	}
	if (_settings.format != ExportSettings::FMT_OSGB)
	{
		osg::Geode* dim = new osg::Geode;
		dim->addDrawable(makeQuad((float)(px + RP_PAD), (float)(y),
			(float)bw, (float)(RP_BTN_H * 2 + 2),
			osg::Vec4(0, 0, 0, 0.45f)));
		_hudCamera->addChild(dim);
	}
	y -= RP_SEP;
	_addDivider(px, pw, y);
	y -= 1;

	// Texture embed section
	y -= RP_SEP;
	y -= RP_LABEL_H;
	_addLabel("TEXTURES", px, y);
	y -= RP_BTN_H;
	{
		const int bw3 = (bw - 4) / 3;
		struct
		{
			const char* lbl;
			int code;
			ExportSettings::ImageMode val;
		} b[3] = {
			{"Inline", EP_HIT_IMG_INLINE, ExportSettings::IMG_INLINE},
			{"Inc.File", EP_HIT_IMG_INCFILE, ExportSettings::IMG_INCLUDE_FILE},
			{"External", EP_HIT_IMG_EXT, ExportSettings::IMG_EXTERNAL},
		};
		for (int i = 0; i < 3; ++i)
		{
			int bx = px + RP_PAD + i * (bw3 + 2);
			_drawToggleBtn(bx, y, bw3, RP_BTN_H, b[i].lbl,
				_settings.imageMode == b[i].val);
			_hitZones.push_back({ bx, y, bw3, RP_BTN_H, b[i].code });
		}
	}
	y -= RP_SEP;
	_addDivider(px, pw, y);
	y -= 1;

	// Scale section
	y -= RP_SEP;
	y -= RP_LABEL_H;
	_addLabel("SCALE", px, y);
	y -= RP_BTN_H;
	{
		const int bw3 = (bw - 4) / 3;
		struct
		{
			const char* lbl;
			int code;
			ExportSettings::Scale val;
		} b[3] = {
			{"x0.001", EP_HIT_SCALE_0001, ExportSettings::SCALE_0001},
			{"x1", EP_HIT_SCALE_1, ExportSettings::SCALE_1},
			{"x1000", EP_HIT_SCALE_1000, ExportSettings::SCALE_1000},
		};
		for (int i = 0; i < 3; ++i)
		{
			int bx = px + RP_PAD + i * (bw3 + 2);
			_drawToggleBtn(bx, y, bw3, RP_BTN_H, b[i].lbl,
				_settings.scale == b[i].val);
			_hitZones.push_back({ bx, y, bw3, RP_BTN_H, b[i].code });
		}
	}
	y -= RP_SEP;
	_addDivider(px, pw, y);
	y -= 1;

	// Axis section
	y -= RP_SEP;
	y -= RP_LABEL_H;
	_addLabel("AXIS", px, y);
	y -= RP_BTN_H;
	{
		const int bw3 = (bw - 4) / 3;
		struct
		{
			const char* lbl;
			int code;
			ExportSettings::Axis val;
		} b[3] = {
			{"None", EP_HIT_AXIS_NONE, ExportSettings::AXIS_NONE},
			{"Y->Z", EP_HIT_AXIS_Y2Z, ExportSettings::AXIS_Y2Z},
			{"Z->Y", EP_HIT_AXIS_Z2Y, ExportSettings::AXIS_Z2Y},
		};
		for (int i = 0; i < 3; ++i)
		{
			int bx = px + RP_PAD + i * (bw3 + 2);
			_drawToggleBtn(bx, y, bw3, RP_BTN_H, b[i].lbl,
				_settings.axis == b[i].val);
			_hitZones.push_back({ bx, y, bw3, RP_BTN_H, b[i].code });
		}
	}
	y -= RP_SEP;
	_addDivider(px, pw, y);
	y -= 1;

	// Optimizer section
	y -= RP_SEP;
	y -= RP_LABEL_H;
	_addLabel("OPTIMIZER", px, y);
	{
		struct
		{
			const char* lbl;
			int code;
			bool active;
		} cb[6] = {
			{"Merge Geometries", EP_HIT_CB_MERGE, _settings.mergeGeom},
			{"Share State", EP_HIT_CB_SHARE, _settings.shareState},
			{"Tristrip", EP_HIT_CB_STRIP, _settings.tristrip},
			{"Flatten Transforms", EP_HIT_CB_FLATTEN, _settings.flattenTransforms},
			{"Index Mesh", EP_HIT_CB_INDEX, _settings.indexMesh},
			{"Remove Redundant", EP_HIT_CB_REDUND, _settings.removeRedundant},
		};
		for (auto& c : cb)
		{
			y -= RP_CB_H;
			_drawCheckbox(px + RP_PAD, y, bw, c.lbl, c.active);
			_hitZones.push_back({ px + RP_PAD, y, bw, RP_CB_H, c.code });
		}
	}
	y -= RP_SEP;
	_addDivider(px, pw, y);

	// Export button (pinned at bottom)
	_exportBtnGeode = new osg::Geode;
	_exportBtnTextGeode = new osg::Geode;
	_hudCamera->addChild(_exportBtnGeode.get());
	_hudCamera->addChild(_exportBtnTextGeode.get());
	_rebuildExportButton();
}

void ExportPanel::_addLabel(const char* text, int px, int y)
{
	osg::Geode* t = new osg::Geode;
	t->addDrawable(makeText(text, (float)(px + RP_PAD), (float)(y + 4),
		FONT_SZ - 1.f, C_TEXT_DIM));
	_hudCamera->addChild(t);
}

void ExportPanel::_addDivider(int px, int pw, int y)
{
	osg::Geode* div = new osg::Geode;
	div->addDrawable(makeQuad((float)px, (float)y, (float)pw, 1.f, C_DIVIDER));
	_hudCamera->addChild(div);
}

void ExportPanel::_drawToggleBtn(int x, int y, int w, int h, const char* label, bool active)
{
	osg::Geode* bg = new osg::Geode;
	bg->addDrawable(makeQuad((float)x, (float)y, (float)w, (float)h,
		active ? C_BTN_ACTIVE : C_BTN_IDLE));
	_hudCamera->addChild(bg);

	osg::Geode* t = new osg::Geode;
	t->addDrawable(makeText(label, (float)(x + w / 2), (float)(y + 7),
		FONT_SZ - 1.f, active ? C_TEXT : C_TEXT_DIM,
		osgText::Text::CENTER_BOTTOM));
	_hudCamera->addChild(t);
}

void ExportPanel::_drawCheckbox(int x, int y, int /*w*/, const char* label, bool checked)
{
	const int BOX = 14;
	int by = y + (RP_CB_H - BOX) / 2;
	osg::Geode* bg = new osg::Geode;
	bg->addDrawable(makeQuad((float)x, (float)by, (float)BOX, (float)BOX,
		checked ? C_BTN_ACTIVE : C_BTN_IDLE));
	_hudCamera->addChild(bg);

	if (checked)
	{
		osg::Geode* t = new osg::Geode;
		t->addDrawable(makeText("X", (float)(x + BOX / 2), (float)(by + 2),
			FONT_SZ - 2.f, C_TEXT,
			osgText::Text::CENTER_BOTTOM));
		_hudCamera->addChild(t);
	}

	osg::Geode* lt = new osg::Geode;
	lt->addDrawable(makeText(label, (float)(x + BOX + 6), (float)(y + 5),
		FONT_SZ, C_TEXT));
	_hudCamera->addChild(lt);
}

void ExportPanel::_rebuildExportButton()
{
	if (!_exportBtnGeode.valid())
		return;
	_exportBtnGeode->removeDrawables(0, _exportBtnGeode->getNumDrawables());
	_exportBtnTextGeode->removeDrawables(0, _exportBtnTextGeode->getNumDrawables());

	const int px = panelX();
	_exportBtnGeode->addDrawable(
		makeQuad((float)(px + RP_PAD), 5.f,
			(float)(RPANEL_W - 2 * RP_PAD), (float)(RP_EXPORT_H - 10),
			_exportHovered ? C_EXPORT_HOV : C_EXPORT));
	_exportBtnTextGeode->addDrawable(
		makeText("Export" + _settings.extension(),
			(float)(px + RPANEL_W / 2), 9.f,
			FONT_SZ, C_TEXT, osgText::Text::CENTER_BOTTOM));
}

void ExportPanel::_doExport(osg::Node* scene)
{
	if (!scene)
		return;

	std::string outPath = saveFileDialog(_settings.extension());
	if (outPath.empty())
		return;

	OSG_NOTICE << "Exporting to: " << outPath << std::endl;

	osg::ref_ptr<osg::Node> out = scene;
	unsigned int flags = _settings.optimizerFlags();
	if (flags != 0 || _settings.needsTransform())
	{
		out = static_cast<osg::Node*>(scene->clone(osg::CopyOp::DEEP_COPY_ALL));
	}

	if (_settings.needsTransform())
	{
		std::cout << "Applying transform: " << std::endl;
		osg::Matrix m = osg::Matrix::identity();
		if (_settings.axis == ExportSettings::AXIS_Y2Z)
			m = osg::Matrix::rotate(osg::Vec3d(0, 1, 0), osg::Vec3d(0, 0, 1));
		else if (_settings.axis == ExportSettings::AXIS_Z2Y)
			m = osg::Matrix::rotate(osg::Vec3d(0, 0, 1), osg::Vec3d(0, 1, 0));
		float sf = _settings.scaleFactor();
		if (sf != 1.0f)
			m = m * osg::Matrix::scale(sf, sf, sf);

		osg::ref_ptr<osg::MatrixTransform> xform = new osg::MatrixTransform(m);
		xform->setDataVariance(osg::Object::STATIC);
		xform->addChild(out.get());
		osg::ref_ptr<osg::Group> tmp = new osg::Group;
		tmp->addChild(xform.get());
		osgUtil::Optimizer::FlattenStaticTransformsVisitor fstv;
		tmp->accept(fstv);
		fstv.removeTransforms(tmp.get());
		out = tmp->getChild(0);
		OSG_NOTICE << "[Transform] scale=" << sf
			<< " axis=" << _settings.axis << std::endl;
	}

	if (flags != 0)
	{
		SceneStats before = collectSceneStats(out.get());

		osgUtil::Optimizer opt;
		opt.optimize(out.get(), flags);

		SceneStats after = collectSceneStats(out.get());
		OSG_NOTICE << "[Optimizer] nodes:     " << before.nodes << " -> " << after.nodes << std::endl;
		OSG_NOTICE << "[Optimizer] drawables: " << before.drawables << " -> " << after.drawables << std::endl;
		OSG_NOTICE << "[Optimizer] vertices:  " << before.vertices << " -> " << after.vertices << std::endl;
	}

	// Apply user data from sidecar JSON (if any) before writing
	if (!_sourcePath.empty())
	{
		NodeUserDataMap userData = loadUserDataJson(getUserDataJsonPath(_sourcePath));
		if (!userData.empty())
			applyUserDataToScene(out.get(), userData);
	}

	osg::ref_ptr<osgDB::ReaderWriter::Options> opts = _settings.makeOptions();
	if (osgDB::writeNodeFile(*out, outPath, opts.get()))
		OSG_NOTICE << "Export OK: " << outPath << std::endl;
	else
		OSG_WARN << "Export FAILED: " << outPath << std::endl;
}
