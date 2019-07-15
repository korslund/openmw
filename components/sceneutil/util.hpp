#ifndef OPENMW_COMPONENTS_SCENEUTIL_UTIL_H
#define OPENMW_COMPONENTS_SCENEUTIL_UTIL_H

#include <osg/Matrix>
#include <osg/BoundingSphere>
#include <osg/Vec4f>
#include <osg/Vec3f>

namespace ESM {
    struct Cell;
}

namespace SceneUtil
{

    // Transform a bounding sphere by a matrix
    // based off private code in osg::Transform
    // TODO: patch osg to make public
    void transformBoundingSphere (const osg::Matrixf& matrix, osg::BoundingSphere& bsphere);

    osg::Vec4f colourFromRGB (unsigned int clr);

    osg::Vec4f colourFromRGBA (unsigned int value);

    float makeOsgColorComponent (unsigned int value, unsigned int shift);

    bool hasUserDescription(const osg::Node* node, const std::string pattern);

    osg::Vec3f getCellOrigin(const ESM::Cell *);
}

#endif
