#include "bulletnifloader.hpp"

#include <cstdio>
#include <vector>
#include <list>
#include <stdexcept>

#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/CollisionShapes/btScaledBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>

#include <components/misc/stringops.hpp>

#include <components/nif/node.hpp>
#include <components/nif/data.hpp>
#include <components/nif/property.hpp>
#include <components/nif/controller.hpp>
#include <components/nif/extra.hpp>


namespace
{

osg::Matrixf getWorldTransform(const Nif::Node *node)
{
    if(node->parent != NULL)
        return node->trafo.toMatrix() * getWorldTransform(node->parent);
    return node->trafo.toMatrix();
}

btVector3 getbtVector(const osg::Vec3f &v)
{
    return btVector3(v.x(), v.y(), v.z());
}

}

namespace NifBullet
{

BulletNifLoader::BulletNifLoader()
    : mCompoundShape(NULL)
    , mStaticMesh(NULL)
{
}

BulletNifLoader::~BulletNifLoader()
{
}

osg::ref_ptr<Resource::BulletShape> BulletNifLoader::load(const Nif::NIFFilePtr nif)
{
    mShape = new Resource::BulletShape;

    mCompoundShape = NULL;
    mStaticMesh = NULL;

    if (nif->numRoots() < 1)
    {
        warn("Found no root nodes in NIF.");
        return mShape;
    }

    Nif::Record *r = nif->getRoot(0);
    assert(r != NULL);

    Nif::Node *node = dynamic_cast<Nif::Node*>(r);
    if (node == NULL)
    {
        warn("First root in file was not a node, but a " +
             r->recName + ". Skipping file.");
        return mShape;
    }

    if (findBoundingBox(node))
    {
        std::auto_ptr<btCompoundShape> compound (new btCompoundShape);

        btBoxShape* boxShape = new btBoxShape(getbtVector(mShape->mCollisionBoxHalfExtents));
        btTransform transform = btTransform::getIdentity();
        transform.setOrigin(getbtVector(mShape->mCollisionBoxTranslate));
        compound->addChildShape(transform, boxShape);

        mShape->mCollisionShape = compound.release();
        return mShape;
    }
    else
    {
        bool autogenerated = hasAutoGeneratedCollision(node);
        bool isAnimated = false;

        // files with the name convention xmodel.nif usually have keyframes stored in a separate file xmodel.kf (see Animation::addAnimSource).
        // assume all nodes in the file will be animated
        std::string filename = nif->getFilename();
        size_t slashpos = filename.find_last_of("/\\");
        if (slashpos == std::string::npos)
            slashpos = 0;
        if (slashpos+1 < filename.size() && (filename[slashpos+1] == 'x' || filename[slashpos+1] == 'X'))
        {
            isAnimated = true;
        }

        handleNode(node, 0, autogenerated, isAnimated, autogenerated);

        if (mCompoundShape)
        {
            mShape->mCollisionShape = mCompoundShape;
            if (mStaticMesh)
            {
                btTransform trans;
                trans.setIdentity();
                mCompoundShape->addChildShape(trans, new Resource::TriangleMeshShape(mStaticMesh,true));
            }
        }
        else if (mStaticMesh)
            mShape->mCollisionShape = new Resource::TriangleMeshShape(mStaticMesh,true);

        return mShape;
    }
}

// Find a boundingBox in the node hierarchy.
// Return: use bounding box for collision?
bool BulletNifLoader::findBoundingBox(const Nif::Node* node, int flags)
{
    flags |= node->flags;

    if (node->hasBounds)
    {
        mShape->mCollisionBoxHalfExtents = node->boundXYZ;
        mShape->mCollisionBoxTranslate = node->boundPos;

        if (flags & Nif::NiNode::Flag_BBoxCollision)
        {
            return true;
        }
    }

    const Nif::NiNode *ninode = dynamic_cast<const Nif::NiNode*>(node);
    if(ninode)
    {
        const Nif::NodeList &list = ninode->children;
        for(size_t i = 0;i < list.length();i++)
        {
            if(!list[i].empty())
            {
                bool found = findBoundingBox (list[i].getPtr());
                if (found)
                    return true;
            }
        }
    }
    return false;
}

bool BulletNifLoader::hasAutoGeneratedCollision(const Nif::Node* rootNode)
{
    const Nif::NiNode *ninode = dynamic_cast<const Nif::NiNode*>(rootNode);
    if(ninode)
    {
        const Nif::NodeList &list = ninode->children;
        for(size_t i = 0;i < list.length();i++)
        {
            if(!list[i].empty())
            {
                if(list[i].getPtr()->recType == Nif::RC_RootCollisionNode)
                    return false;
            }
        }
    }
    return true;
}

void BulletNifLoader::handleNode(const Nif::Node *node, int flags,
                                         bool isCollisionNode, bool isAnimated, bool autogenerated)
{
    // Accumulate the flags from all the child nodes. This works for all
    // the flags we currently use, at least.
    flags |= node->flags;

    if (!node->controller.empty() && node->controller->recType == Nif::RC_NiKeyframeController
            && (node->controller->flags & Nif::NiNode::ControllerFlag_Active))
        isAnimated = true;

    isCollisionNode = isCollisionNode || (node->recType == Nif::RC_RootCollisionNode);

    // Don't collide with AvoidNode shapes
    if(node->recType == Nif::RC_AvoidNode)
        flags |= 0x800;

    // Check for extra data
    Nif::Extra const *e = node;
    while (!e->extra.empty())
    {
        // Get the next extra data in the list
        e = e->extra.getPtr();
        assert(e != NULL);

        if (e->recType == Nif::RC_NiStringExtraData)
        {
            // String markers may contain important information
            // affecting the entire subtree of this node
            Nif::NiStringExtraData *sd = (Nif::NiStringExtraData*)e;

            // not sure what the difference between NCO and NCC is, or if there even is one
            if (sd->string == "NCO" || sd->string == "NCC")
            {
                // No collision. Use an internal flag setting to mark this.
                flags |= 0x800;
            }
            else if (sd->string == "MRK" && autogenerated)
            {
                // Marker can still have collision if the model explicitely specifies it via a RootCollisionNode.
                return;
            }

        }
    }

    if (isCollisionNode)
    {
        // NOTE: a trishape with hasBounds=true, but no BBoxCollision flag should NOT go through handleNiTriShape!
        // It must be ignored completely.
        // (occurs in tr_ex_imp_wall_arch_04.nif)
        if(!node->hasBounds && node->recType == Nif::RC_NiTriShape)
        {
            handleNiTriShape(static_cast<const Nif::NiTriShape*>(node), flags, getWorldTransform(node), isAnimated);
        }
    }

    // For NiNodes, loop through children
    const Nif::NiNode *ninode = dynamic_cast<const Nif::NiNode*>(node);
    if(ninode)
    {
        const Nif::NodeList &list = ninode->children;
        for(size_t i = 0;i < list.length();i++)
        {
            if(!list[i].empty())
                handleNode(list[i].getPtr(), flags, isCollisionNode, isAnimated, autogenerated);
        }
    }
}

void BulletNifLoader::handleNiTriShape(const Nif::NiTriShape *shape, int flags, const osg::Matrixf &transform, bool isAnimated)
{
    assert(shape != NULL);

    // Interpret flags
    bool hidden    = (flags&Nif::NiNode::Flag_Hidden) != 0;
    bool collide   = (flags&Nif::NiNode::Flag_MeshCollision) != 0;
    bool bbcollide = (flags&Nif::NiNode::Flag_BBoxCollision) != 0;

    // If the object was marked "NCO" earlier, it shouldn't collide with
    // anything. So don't do anything.
    if ((flags & 0x800))
    {
        return;
    }

    if (!collide && !bbcollide && hidden)
        // This mesh apparently isn't being used for anything, so don't
        // bother setting it up.
        return;

    if (!shape->skin.empty())
        isAnimated = false;

    if (isAnimated)
    {
        if (!mCompoundShape)
            mCompoundShape = new btCompoundShape();

        btTriangleMesh* childMesh = new btTriangleMesh();

        const Nif::NiTriShapeData *data = shape->data.getPtr();

        childMesh->preallocateVertices(data->vertices->size());
        childMesh->preallocateIndices(data->triangles->size());

        const osg::Vec3Array& vertices = *data->vertices;
        const osg::DrawElementsUShort& triangles = *data->triangles;

        size_t numtris = data->triangles->size();
        for(size_t i = 0;i < numtris;i+=3)
        {
            osg::Vec3f b1 = vertices[triangles[i+0]];
            osg::Vec3f b2 = vertices[triangles[i+1]];
            osg::Vec3f b3 = vertices[triangles[i+2]];
            childMesh->addTriangle(getbtVector(b1), getbtVector(b2), getbtVector(b3));
        }

        Resource::TriangleMeshShape* childShape = new Resource::TriangleMeshShape(childMesh,true);

        float scale = shape->trafo.scale;
        const Nif::Node* parent = shape;
        while (parent->parent)
        {
            parent = parent->parent;
            scale *= parent->trafo.scale;
        }
        osg::Quat q = transform.getRotate();
        osg::Vec3f v = transform.getTrans();
        childShape->setLocalScaling(btVector3(scale, scale, scale));

        btTransform trans(btQuaternion(q.x(), q.y(), q.z(), q.w()), btVector3(v.x(), v.y(), v.z()));

        mShape->mAnimatedShapes.insert(std::make_pair(shape->recIndex, mCompoundShape->getNumChildShapes()));

        mCompoundShape->addChildShape(trans, childShape);
    }
    else
    {
        if (!mStaticMesh)
            mStaticMesh = new btTriangleMesh(false);

        // Static shape, just transform all vertices into position
        const Nif::NiTriShapeData *data = shape->data.getPtr();
        const osg::Vec3Array& vertices = *data->vertices;
        const osg::DrawElementsUShort& triangles = *data->triangles;

        mStaticMesh->preallocateVertices(data->vertices->size());
        mStaticMesh->preallocateIndices(data->triangles->size());

        size_t numtris = data->triangles->size();
        for(size_t i = 0;i < numtris;i+=3)
        {
            osg::Vec3f b1 = vertices[triangles[i+0]]*transform;
            osg::Vec3f b2 = vertices[triangles[i+1]]*transform;
            osg::Vec3f b3 = vertices[triangles[i+2]]*transform;
            mStaticMesh->addTriangle(getbtVector(b1), getbtVector(b2), getbtVector(b3));
        }
    }
}

} // namespace NifBullet
