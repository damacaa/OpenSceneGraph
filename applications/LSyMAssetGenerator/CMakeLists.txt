SET(TARGET_SRC
    LSyMAssetGenerator.cpp
    UIWidgets.cpp
    SceneUtils.cpp
    FileDialogs.cpp
    HierarchyPanel.cpp
    ExportPanel.cpp
    PanelHandler.cpp
    NodeUserData.cpp
    UserDataPanel.cpp
)

SET(TARGET_ADDED_LIBRARIES osgAnimation)

SETUP_APPLICATION(LSyMAssetGenerator)

# Ensure required plugins are built before the application
if(TARGET osgdb_fbx)
    add_dependencies(${TARGET_TARGETNAME} osgdb_fbx)
endif()
if(TARGET osgdb_png)
    add_dependencies(${TARGET_TARGETNAME} osgdb_png)
endif()
if(TARGET osgdb_jpeg)
    add_dependencies(${TARGET_TARGETNAME} osgdb_jpeg)
endif()
if(TARGET osgdb_freetype)
    add_dependencies(${TARGET_TARGETNAME} osgdb_freetype)
endif()

# Native OSG serializers and formats
if(TARGET osgdb_osg)
    add_dependencies(${TARGET_TARGETNAME} osgdb_osg)
endif()

set(OSG_SERIALIZERS 
    osg osganimation osgfx osgga osgmanipulator osgparticle 
    osgshadow osgsim osgterrain osgtext osgui osgutil osgviewer osgvolume
)
foreach(SER ${OSG_SERIALIZERS})
    if(TARGET osgdb_serializers_${SER})
        add_dependencies(${TARGET_TARGETNAME} osgdb_serializers_${SER})
    endif()
endforeach()

# Copy 3rd-party dependency DLLs from the plugins directory to the executable directory
# so that the OS can find them when the plugins are dynamically loaded.
# Uses -ErrorAction SilentlyContinue to prevent build failure if a DLL is currently locked by a running process.
if(WIN32)
    add_custom_command(TARGET ${TARGET_TARGETNAME} POST_BUILD
        COMMAND powershell -NoProfile -Command "if (Test-Path -Path '$<TARGET_FILE_DIR:${TARGET_TARGETNAME}>/osgPlugins-${OPENSCENEGRAPH_VERSION}') { Get-ChildItem -Path '$<TARGET_FILE_DIR:${TARGET_TARGETNAME}>/osgPlugins-${OPENSCENEGRAPH_VERSION}/*.dll' -Exclude 'osgdb_*.dll' | Copy-Item -Destination '$<TARGET_FILE_DIR:${TARGET_TARGETNAME}>' -Force -ErrorAction SilentlyContinue }"
    )
endif()
