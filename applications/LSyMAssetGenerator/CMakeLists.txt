SET(TARGET_SRC
    LSyMAssetGenerator.cpp
    UIWidgets.cpp
    SceneUtils.cpp
    FileDialogs.cpp
    HierarchyPanel.cpp
    ExportPanel.cpp
    PanelHandler.cpp
)

SET(TARGET_ADDED_LIBRARIES osgAnimation)

SETUP_APPLICATION(LSyMAssetGenerator)
