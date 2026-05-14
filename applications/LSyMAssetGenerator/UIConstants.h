#pragma once
#include <osg/Vec4>

// ─── Layout ──────────────────────────────────────────────────────────────────

static const int PANEL_W = 320;
static const int RPANEL_W = 280;
static const int HEADER_H = 44;
static const int LOAD_H = 36;
static const int ITEM_H = 22;
static const int INDENT_PX = 14;
static const float FONT_SZ = 13.0f;

// Right-panel layout
static const int RP_EXPORT_H = 46;
static const int RP_PAD = 10;
static const int RP_LABEL_H = 20;
static const int RP_BTN_H = 28;
static const int RP_CB_H = 26;
static const int RP_SEP = 8;

// ─── Colours ─────────────────────────────────────────────────────────────────

static const osg::Vec4 C_PANEL_BG(0.11f, 0.11f, 0.13f, 0.93f);
static const osg::Vec4 C_RPANEL_BG(0.09f, 0.09f, 0.11f, 0.95f);
static const osg::Vec4 C_HEADER_BG(0.07f, 0.07f, 0.09f, 1.00f);
static const osg::Vec4 C_DIVIDER(0.26f, 0.26f, 0.30f, 1.00f);
static const osg::Vec4 C_SEL(0.18f, 0.44f, 0.80f, 0.90f);
static const osg::Vec4 C_BTN_ACTIVE(0.18f, 0.44f, 0.80f, 0.95f);
static const osg::Vec4 C_BTN_IDLE(0.18f, 0.18f, 0.22f, 0.95f);
static const osg::Vec4 C_LOAD(0.22f, 0.22f, 0.28f, 0.95f);
static const osg::Vec4 C_LOAD_HOV(0.30f, 0.30f, 0.38f, 0.95f);
static const osg::Vec4 C_EXPORT(0.12f, 0.52f, 0.22f, 0.95f);
static const osg::Vec4 C_EXPORT_HOV(0.16f, 0.65f, 0.28f, 0.95f);
static const osg::Vec4 C_TEXT(0.92f, 0.92f, 0.92f, 1.00f);
static const osg::Vec4 C_TEXT_DIM(0.58f, 0.58f, 0.62f, 1.00f);
static const osg::Vec4 C_SCROLLBAR(0.38f, 0.38f, 0.43f, 0.75f);
static const osg::Vec4 C_STAT_VAL(0.40f, 0.85f, 0.50f, 1.00f);
static const osg::Vec4 C_TEXT_SEL(1.00f, 0.85f, 0.20f, 1.00f); // selected-item label (gold)
