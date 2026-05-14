#include "NodeUserData.h"
#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/UserDataContainer>
#include <osg/ValueObject>
#include <fstream>
#include <iterator>

// ─── JSON string utilities ────────────────────────────────────────────────────

static std::string jsonEscape(const std::string& s)
{
    std::string r;
    r.reserve(s.size());
    for (char c : s)
    {
        if      (c == '"')  r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\t') r += "\\t";
        else if (c == '\r') r += "\\r";
        else                r += c;
    }
    return r;
}

// ─── Minimal recursive-descent JSON parser ───────────────────────────────────

namespace {
struct Parser
{
    const std::string& s;
    size_t p = 0;

    void ws()
    {
        while (p < s.size() &&
               (s[p] == ' ' || s[p] == '\t' || s[p] == '\n' || s[p] == '\r'))
            ++p;
    }

    bool eat(char c)
    {
        ws();
        if (p < s.size() && s[p] == c) { ++p; return true; }
        return false;
    }

    std::string str()
    {
        ws();
        if (p >= s.size() || s[p] != '"') return {};
        ++p;
        std::string r;
        while (p < s.size() && s[p] != '"')
        {
            if (s[p] == '\\' && p + 1 < s.size())
            {
                ++p;
                switch (s[p])
                {
                case '"':  r += '"';  break;
                case '\\': r += '\\'; break;
                case 'n':  r += '\n'; break;
                case 't':  r += '\t'; break;
                case 'r':  r += '\r'; break;
                default:   r += s[p]; break;
                }
            }
            else
            {
                r += s[p];
            }
            ++p;
        }
        if (p < s.size()) ++p;   // consume closing "
        return r;
    }
};
} // namespace

// ─── Path utility ─────────────────────────────────────────────────────────────

std::string getUserDataJsonPath(const std::string& assetPath)
{
    std::string p = assetPath;
    auto dot = p.rfind('.');
    if (dot != std::string::npos)
        p = p.substr(0, dot);
    return p + ".json";
}

// ─── Load ─────────────────────────────────────────────────────────────────────

NodeUserDataMap loadUserDataJson(const std::string& jsonPath)
{
    NodeUserDataMap result;
    std::ifstream f(jsonPath);
    if (!f.is_open()) return result;

    std::string src((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());

    Parser ps{src};
    if (!ps.eat('{')) return result;

    while (true)
    {
        ps.ws();
        if (ps.p >= ps.s.size() || ps.s[ps.p] == '}') break;

        std::string nodeName = ps.str();
        if (!ps.eat(':')) break;
        if (!ps.eat('{')) break;

        UserDataMap kvMap;
        while (true)
        {
            ps.ws();
            if (ps.p >= ps.s.size() || ps.s[ps.p] == '}') break;
            std::string key = ps.str();
            if (!ps.eat(':')) break;
            std::string val = ps.str();
            if (!key.empty()) kvMap[key] = val;
            ps.ws();
            if (!ps.eat(',')) break;
        }
        ps.eat('}');

        if (!nodeName.empty())
            result[nodeName] = std::move(kvMap);

        ps.ws();
        if (!ps.eat(',')) break;
    }
    ps.eat('}');
    return result;
}

// ─── Save ─────────────────────────────────────────────────────────────────────

void saveUserDataJson(const std::string& jsonPath, const NodeUserDataMap& data)
{
    std::ofstream f(jsonPath);
    if (!f.is_open()) return;

    f << "{\n";
    bool firstNode = true;
    for (NodeUserDataMap::const_iterator nit = data.begin(); nit != data.end(); ++nit)
    {
        const std::string& nodeName = nit->first;
        const UserDataMap& kvMap    = nit->second;
        if (!firstNode) f << ",\n";
        firstNode = false;
        f << "  \"" << jsonEscape(nodeName) << "\": {\n";
        bool firstKv = true;
        for (UserDataMap::const_iterator kit = kvMap.begin(); kit != kvMap.end(); ++kit)
        {
            if (!firstKv) f << ",\n";
            firstKv = false;
            f << "    \"" << jsonEscape(kit->first) << "\": \"" << jsonEscape(kit->second) << "\"";
        }
        f << "\n  }";
    }
    f << "\n}\n";
}

// ─── Count nodes by name ──────────────────────────────────────────────────────

int countNodesWithName(osg::Node* scene, const std::string& name)
{
    if (!scene || name.empty()) return 0;

    struct Counter : osg::NodeVisitor
    {
        const std::string& name;
        int count = 0;
        explicit Counter(const std::string& n)
            : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), name(n) {}
        void apply(osg::Node& node) override
        {
            if (node.getName() == name) ++count;
            traverse(node);
        }
    };

    Counter c(name);
    scene->accept(c);
    return c.count;
}

// ─── Apply user data ──────────────────────────────────────────────────────────

void applyUserDataToScene(osg::Node* scene, const NodeUserDataMap& data)
{
    if (!scene || data.empty()) return;

    struct Applier : osg::NodeVisitor
    {
        const NodeUserDataMap& data;
        explicit Applier(const NodeUserDataMap& d)
            : osg::NodeVisitor(TRAVERSE_ALL_CHILDREN), data(d) {}

        void apply(osg::Node& node) override
        {
            auto it = data.find(node.getName());
            if (it != data.end() && !it->first.empty())
            {
                osg::UserDataContainer* udc = node.getOrCreateUserDataContainer();
                const UserDataMap& kv = it->second;
                for (UserDataMap::const_iterator kit = kv.begin(); kit != kv.end(); ++kit)
                {
                    const std::string& k = kit->first;
                    const std::string& v = kit->second;
                    // Remove any existing entry with this name, then add fresh
                    unsigned int n = udc->getNumUserObjects();
                    for (unsigned int i = n; i > 0; --i)
                    {
                        osg::Object* obj = udc->getUserObject(i - 1);
                        if (obj && obj->getName() == k)
                            udc->removeUserObject(i - 1);
                    }
                    udc->addUserObject(new osg::StringValueObject(k, v));
                }
            }
            traverse(node);
        }
    };

    Applier a(data);
    scene->accept(a);
}
