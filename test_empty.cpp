#include <osg/Group>
#include <osg/MatrixTransform>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

int main() {
    osg::ref_ptr<osg::Group> root = new osg::Group;
    osg::ref_ptr<osg::MatrixTransform> mt1 = new osg::MatrixTransform;
    mt1->setName(\
EmptyTransform1\);
    osg::ref_ptr<osg::MatrixTransform> mt2 = new osg::MatrixTransform;
    mt2->setName(\EmptyTransform2\);
    root->addChild(mt1.get());
    root->addChild(mt2.get());
    osgDB::writeNodeFile(*root, \test_empty.osgb\);
    return 0;
}
EOF
