#pragma once
#include <map>
#include <string>

namespace osg { class Node; }

using UserDataMap    = std::map<std::string, std::string>;   // key  -> value
using NodeUserDataMap = std::map<std::string, UserDataMap>;  // name -> {key->value}

// Returns the .json sidecar path for an asset path (same path, extension replaced)
std::string getUserDataJsonPath(const std::string& assetPath);

// Load / save the two-level JSON sidecar file
NodeUserDataMap loadUserDataJson(const std::string& jsonPath);
void            saveUserDataJson(const std::string& jsonPath, const NodeUserDataMap& data);

// Count how many nodes in the scene tree have the given name
int countNodesWithName(osg::Node* scene, const std::string& name);

// Walk the scene and push user data from the map onto matching nodes
void applyUserDataToScene(osg::Node* scene, const NodeUserDataMap& data);
