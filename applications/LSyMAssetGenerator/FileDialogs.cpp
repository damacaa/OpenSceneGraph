#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include "FileDialogs.h"

std::string openFileDialog()
{
	char buf[MAX_PATH] = {};
	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFilter =
		"3D Files\0*.fbx;*.osg;*.osgt;*.osgb;*.ive\0"
		"FBX Files\0*.fbx\0"
		"OSG Files\0*.osg;*.osgt;*.osgb;*.ive\0"
		"All Files\0*.*\0";
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrTitle = "Open 3D File";
	return GetOpenFileNameA(&ofn) ? buf : std::string{};
}

std::string saveFileDialog(const std::string& ext)
{
	char buf[MAX_PATH] = {};
	std::string defName = "output" + ext;
	strncpy_s(buf, defName.c_str(), MAX_PATH - 1);

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	if (ext == ".osgb")
		ofn.lpstrFilter = "OSG Binary\0*.osgb\0All Files\0*.*\0";
	else if (ext == ".osg")
		ofn.lpstrFilter = "OSG Text\0*.osg\0All Files\0*.*\0";
	else
		ofn.lpstrFilter = "OSG Text ASCII\0*.osgt\0All Files\0*.*\0";
	ofn.lpstrFile = buf;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrDefExt = ext.c_str() + 1;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
	ofn.lpstrTitle = "Export 3D File";
	return GetSaveFileNameA(&ofn) ? buf : std::string{};
}
