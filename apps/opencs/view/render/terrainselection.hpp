#ifndef CSV_RENDER_TERRAINSELECTION_H
#define CSV_RENDER_TERRAINSELECTION_H

#include <utility>
#include <vector>

#include <osg/Vec3d>
#include <osg/ref_ptr>
#include <osg/PositionAttitudeTransform>

#include <components/esm/loadland.hpp>
#include "../../model/world/cellcoordinates.hpp"

namespace osg
{
    class Group;
}

namespace CSVRender
{
    struct WorldspaceHitResult;
    class WorldspaceWidget;

    /// \brief Class handling the terrain selection data and rendering
    class TerrainSelection
    {
        public:

            TerrainSelection(osg::Group* parentNode, WorldspaceWidget *worldspaceWidget);
            ~TerrainSelection();

            void selectTerrainTexture(const WorldspaceHitResult&);
            void onlyAddSelect(const WorldspaceHitResult&);
            void toggleSelect(const WorldspaceHitResult&);

            void activate();
            void deactivate();

            std::vector<std::pair<int, int>> getTerrainSelection() const;

        protected:

            void addToSelection(osg::Vec3d worldPos);
            void toggleSelection(osg::Vec3d worldPos);
            void deselect();

            void update();

            ///Converts Worldspace coordinates to global texture selection, modified by texture offset.
            std::pair<int, int> toTextureCoords(osg::Vec3d worldPos) const;

            ///Converts Worldspace coordinates to global vertex selection.
            std::pair<int, int> toVertexCoords(osg::Vec3d worldPos) const;

            ///Converts Global texture coordinates to Worldspace coordinate at upper left corner of the selected texture.
            static double texSelectionToWorldCoords(int);

            int calculateLandHeight(int x, int y);

        private:

            osg::Group* mParentNode;
            WorldspaceWidget *mWorldspaceWidget;
            osg::ref_ptr<osg::PositionAttitudeTransform> mBaseNode;
            osg::ref_ptr<osg::Geometry> mGeometry;
            osg::ref_ptr<osg::Group> mSelectionNode;
            std::vector<std::pair<int, int>> mSelection; // Global terrain selection coordinate in either vertex or texture units
    };
}

#endif
