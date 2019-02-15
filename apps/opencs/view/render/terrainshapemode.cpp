#include "terrainshapemode.hpp"

#include <string>
#include <sstream>
#include <memory>

#include <QWidget>
#include <QIcon>
#include <QEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDrag>

#include <osg/Group>

#include <components/esm/loadland.hpp>

#include "../widget/modebutton.hpp"
#include "../widget/scenetoolbar.hpp"
#include "../widget/scenetoolshapebrush.hpp"

#include "../../model/doc/document.hpp"
#include "../../model/prefs/state.hpp"
#include "../../model/world/columnbase.hpp"
#include "../../model/world/commandmacro.hpp"
#include "../../model/world/commands.hpp"
#include "../../model/world/data.hpp"
#include "../../model/world/idtable.hpp"
#include "../../model/world/idtree.hpp"
#include "../../model/world/land.hpp"
#include "../../model/world/resourcetable.hpp"
#include "../../model/world/tablemimedata.hpp"
#include "../../model/world/universalid.hpp"

#include "editmode.hpp"
#include "pagedworldspacewidget.hpp"
#include "mask.hpp"
#include "object.hpp" // Something small needed regarding pointers from here ()
#include "terrainselection.hpp"
#include "worldspacewidget.hpp"

CSVRender::TerrainShapeMode::TerrainShapeMode (WorldspaceWidget *worldspaceWidget, osg::Group* parentNode, QWidget *parent)
: EditMode (worldspaceWidget, QIcon {":scenetoolbar/editing-terrain-shape"}, Mask_Terrain | Mask_Reference, "Terrain land editing", parent),
    mBrushSize(0),
    mBrushShape(0),
    mShapeBrushScenetool(0),
    mDragMode(InteractionType_None),
    mParentNode(parentNode),
    mIsEditing(false),
    mTotalDiffY(0)
{
}

void CSVRender::TerrainShapeMode::activate(CSVWidget::SceneToolbar* toolbar)
{
    if (!mTerrainShapeSelection)
    {
        mTerrainShapeSelection.reset(new TerrainSelection(mParentNode, &getWorldspaceWidget(), TerrainSelectionType::Shape));
    }

    if(!mShapeBrushScenetool)
    {
        mShapeBrushScenetool = new CSVWidget::SceneToolShapeBrush (toolbar, "scenetoolshapebrush", getWorldspaceWidget().getDocument());
        connect(mShapeBrushScenetool, SIGNAL (clicked()), mShapeBrushScenetool, SLOT (activate()));
        connect(mShapeBrushScenetool->mShapeBrushWindow, SIGNAL(passBrushSize(int)), this, SLOT(setBrushSize(int)));
        connect(mShapeBrushScenetool->mShapeBrushWindow, SIGNAL(passBrushShape(int)), this, SLOT(setBrushShape(int)));
        connect(mShapeBrushScenetool->mShapeBrushWindow->mSizeSliders->mBrushSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(setBrushSize(int)));
    }

    EditMode::activate(toolbar);
    toolbar->addTool (mShapeBrushScenetool);
}

void CSVRender::TerrainShapeMode::deactivate(CSVWidget::SceneToolbar* toolbar)
{
    if(mShapeBrushScenetool)
    {
        toolbar->removeTool (mShapeBrushScenetool);
        delete mShapeBrushScenetool;
        mShapeBrushScenetool = 0;
    }
    EditMode::deactivate(toolbar);
}

void CSVRender::TerrainShapeMode::primaryEditPressed(const WorldspaceHitResult& hit)
{
    mCellId = getWorldspaceWidget().getCellId (hit.worldPos);

    if (hit.hit && hit.tag == 0)
    {
    }
    if (CSVRender::PagedWorldspaceWidget *paged =
        dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
    {
        paged->resetAllAlteredHeights();
        mTotalDiffY = 0;
    }
}

void CSVRender::TerrainShapeMode::primarySelectPressed(const WorldspaceHitResult& hit)
{
    if(hit.hit && hit.tag == 0)
    {
        selectTerrainShapes(CSMWorld::CellCoordinates::toVertexCoords(hit.worldPos), 0, false);
    }
}

void CSVRender::TerrainShapeMode::secondarySelectPressed(const WorldspaceHitResult& hit)
{
    if(hit.hit && hit.tag == 0)
    {
        selectTerrainShapes(CSMWorld::CellCoordinates::toVertexCoords(hit.worldPos), 1, false);
    }
}

bool CSVRender::TerrainShapeMode::primaryEditStartDrag (const QPoint& pos)
{
    WorldspaceHitResult hit = getWorldspaceWidget().mousePick (pos, getWorldspaceWidget().getInteractionMask());

    mCellId = getWorldspaceWidget().getCellId (hit.worldPos);

    mDragMode = InteractionType_PrimaryEdit;

    if (hit.hit && hit.tag == 0)
    {
        mEditingPos = hit.worldPos;
        mIsEditing = true;
    }

    return true;
}

bool CSVRender::TerrainShapeMode::secondaryEditStartDrag (const QPoint& pos)
{
    return false;
}

bool CSVRender::TerrainShapeMode::primarySelectStartDrag (const QPoint& pos)
{
    WorldspaceHitResult hit = getWorldspaceWidget().mousePick (pos, getWorldspaceWidget().getInteractionMask());
    mDragMode = InteractionType_PrimarySelect;
    if (!hit.hit || hit.tag != 0)
    {
        mDragMode = InteractionType_None;
        return false;
    }
    selectTerrainShapes(CSMWorld::CellCoordinates::toVertexCoords(hit.worldPos), 0, true);
    return false;
}

bool CSVRender::TerrainShapeMode::secondarySelectStartDrag (const QPoint& pos)
{
    WorldspaceHitResult hit = getWorldspaceWidget().mousePick (pos, getWorldspaceWidget().getInteractionMask());
    mDragMode = InteractionType_SecondarySelect;
    if (!hit.hit || hit.tag != 0)
    {
        mDragMode = InteractionType_None;
        return false;
    }
    selectTerrainShapes(CSMWorld::CellCoordinates::toVertexCoords(hit.worldPos), 1, true);
    return false;
}

void CSVRender::TerrainShapeMode::drag (const QPoint& pos, int diffX, int diffY, double speedFactor)
{
    if (mDragMode == InteractionType_PrimaryEdit)
    {
        WorldspaceHitResult hit = getWorldspaceWidget().mousePick (pos, getWorldspaceWidget().getInteractionMask());
        std::string cellId = getWorldspaceWidget().getCellId (hit.worldPos);
        mTotalDiffY += diffY;
        if (mIsEditing == true) editTerrainShapeGrid(CSMWorld::CellCoordinates::toVertexCoords(mEditingPos), true);
    }

    if (mDragMode == InteractionType_PrimarySelect)
    {
        WorldspaceHitResult hit = getWorldspaceWidget().mousePick (pos, getWorldspaceWidget().getInteractionMask());
        if (hit.hit && hit.tag == 0) selectTerrainShapes(CSMWorld::CellCoordinates::toVertexCoords(hit.worldPos), 0, true);
    }

    if (mDragMode == InteractionType_SecondarySelect)
    {
        WorldspaceHitResult hit = getWorldspaceWidget().mousePick (pos, getWorldspaceWidget().getInteractionMask());
        if (hit.hit && hit.tag == 0) selectTerrainShapes(CSMWorld::CellCoordinates::toVertexCoords(hit.worldPos), 1, true);
    }
}

void CSVRender::TerrainShapeMode::dragCompleted(const QPoint& pos)
{
    if (mDragMode == InteractionType_PrimaryEdit)
    {
        if (mIsEditing == true)
        {
            mTotalDiffY = 0;
            mIsEditing = false;
        }

        sort(mAlteredCells.begin(), mAlteredCells.end());
        mAlteredCells.erase(unique(mAlteredCells.begin(), mAlteredCells.end()), mAlteredCells.end());

        CSMDoc::Document& document = getWorldspaceWidget().getDocument();
        CSMWorld::IdTable& landTable = dynamic_cast<CSMWorld::IdTable&> (
            *document.getData().getTableModel (CSMWorld::UniversalId::Type_Land));

        int landshapeColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandHeightsIndex);
        int landnormalsColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandNormalsIndex);

        QUndoStack& undoStack = document.getUndoStack();

        undoStack.beginMacro ("Edit shape and normal records");

        for(CSMWorld::CellCoordinates cellCoordinates: mAlteredCells)
        {
            std::string cellId = CSMWorld::CellCoordinates::generateId(cellCoordinates.getX(), cellCoordinates.getY());

            if (allowLandShapeEditing(cellId) == true)
            {
                const CSMWorld::LandHeightsColumn::DataType landShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                CSMWorld::LandHeightsColumn::DataType landShapeNew(landShapePointer);
                const CSMWorld::LandNormalsColumn::DataType landNormalsPointer = landTable.data(landTable.getModelIndex(cellId, landnormalsColumn)).value<CSMWorld::LandNormalsColumn::DataType>();
                CSMWorld::LandNormalsColumn::DataType landNormalsNew(landNormalsPointer);

                //Normals code modified from tesanwynn
                float v1[3],
                    v2[3],
                    normal[3],
                    hyp;

                for(int i = 0; i < landSize; ++i)
                {
                    for(int j = 0; j < landSize; ++j)
                    {
                        if (CSVRender::PagedWorldspaceWidget *paged =
                            dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
                        {
                            landShapeNew[j * landSize + i] = landShapePointer[j * landSize + i] + std::floor(paged->getCellAlteredHeights(cellCoordinates)[j * landSize + i]);
                        }

                        v1[0] = 128;
                        v1[1] = 0;
                        v1[2] = landShapeNew[j*landSize+i+1] - landShapeNew[j*landSize+i];

                        v2[0] = 0;
                        v2[1] = 128;
                        v2[2] = landShapeNew[(j+1)*landSize+i] - landShapeNew[j*landSize+i];

                        normal[1] = v1[2]*v2[0] - v1[0]*v2[2];
                        normal[0] = v1[1]*v2[2] - v1[2]*v2[1];
                        normal[2] = v1[0]*v2[1] - v1[1]*v2[0];

                        hyp = sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]) / 127.0f;

                        normal[0] /= hyp; normal[1] /= hyp; normal[2] /= hyp;
                        landNormalsNew[(j*landSize+i)*3+0] = normal[0];
                        landNormalsNew[(j*landSize+i)*3+1] = normal[1];
                        landNormalsNew[(j*landSize+i)*3+2] = normal[2];
                    }
                }
                pushNormalsEditToCommand(landNormalsNew, document, landTable, cellId);
                pushEditToCommand(landShapeNew, document, landTable, cellId);
            }
        }
        undoStack.endMacro();

        if (CSVRender::PagedWorldspaceWidget *paged =
            dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
        {
            paged->resetAllAlteredHeights();
            mTotalDiffY = 0;
        }
    }
}

void CSVRender::TerrainShapeMode::dragAborted()
{
    if (CSVRender::PagedWorldspaceWidget *paged =
        dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
    {
        paged->resetAllAlteredHeights();
        mTotalDiffY = 0;
    }
}

void CSVRender::TerrainShapeMode::dragWheel (int diff, double speedFactor)
{
}

void CSVRender::TerrainShapeMode::editTerrainShapeGrid(std::pair<int, int> vertexCoords, bool dragOperation)
{
    int r = mBrushSize / 2;

    std::string cellId = CSMWorld::CellCoordinates::vertexGlobalToCellId(vertexCoords);
    CSMWorld::CellCoordinates cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;

    if (CSVRender::PagedWorldspaceWidget *paged =
        dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
    {
        paged->resetAllAlteredHeights();
    }

    if(allowLandShapeEditing(cellId)==true)
    {
        if (mBrushShape == 0)
        {
            int x = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(vertexCoords.first);
            int y = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(vertexCoords.second);
            alterHeight(cellCoords, x, y, mTotalDiffY);
        }

        if (mBrushShape == 1)
        {
            for(int i = vertexCoords.first - r; i <= vertexCoords.first + r; ++i)
            {
                for(int j = vertexCoords.second - r; j <= vertexCoords.second + r; ++j)
                {
                    cellId = CSMWorld::CellCoordinates::vertexGlobalToCellId(std::make_pair(i, j));
                    cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                    int x = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(i);
                    int y = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(j);
                    alterHeight(cellCoords, x, y, mTotalDiffY);
                }
            }
        }

        if (mBrushShape == 2)
        {
            for(int i = vertexCoords.first - r; i <= vertexCoords.first + r; ++i)
            {
                for(int j = vertexCoords.second - r; j <= vertexCoords.second + r; ++j)
                {
                    cellId = CSMWorld::CellCoordinates::vertexGlobalToCellId(std::make_pair(i, j));
                    cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                    int distanceX = abs(i - vertexCoords.first);
                    int distanceY = abs(j - vertexCoords.second);
                    int distance = std::round(sqrt(pow(distanceX, 2)+pow(distanceY, 2)));
                    int x = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(i);
                    int y = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(j);
                    float distancePerRadius = 1.0f * distance / r;
                    float smoothedByDistance = mTotalDiffY - mTotalDiffY * (3 * distancePerRadius * distancePerRadius - 2 * distancePerRadius * distancePerRadius * distancePerRadius);
                    if (distance < r) alterHeight(cellCoords, x, y, smoothedByDistance);
                }
            }
        }

        if (mBrushShape == 3)
        {
            if(!mCustomBrushShape.empty())
            {
                for(auto const& value: mCustomBrushShape)
                {
                    cellId = CSMWorld::CellCoordinates::vertexGlobalToCellId(std::make_pair(vertexCoords.first + value.first, vertexCoords.second + value.second));
                    cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                    int x = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(vertexCoords.first + value.first);
                    int y = CSMWorld::CellCoordinates::vertexSelectionToInCellCoords(vertexCoords.second + value.second);
                    alterHeight(cellCoords, x, y, mTotalDiffY);
                }
            }
        }

        sort(mAlteredCells.begin(), mAlteredCells.end());
        mAlteredCells.erase(unique(mAlteredCells.begin(), mAlteredCells.end()), mAlteredCells.end());

        for(CSMWorld::CellCoordinates const& cellCoordinates: mAlteredCells)
        {
            limitHeightChanges(cellCoordinates);
        }

    }
}

void CSVRender::TerrainShapeMode::alterHeight(CSMWorld::CellCoordinates cellCoords, int inCellX, int inCellY, float alteredHeight)
{
    std::string cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX(), cellCoords.getY());
    if(allowLandShapeEditing(cellId)==true)
    {
        mAlteredCells.emplace_back(cellCoords);
        if (CSVRender::PagedWorldspaceWidget *paged =
            dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
        {
            // Get distance from modified land, alter land change based on zoom
            osg::Vec3d eye, center, up;
            paged->getCamera()->getViewMatrixAsLookAt(eye, center, up);
            osg::Vec3d distance = eye - mEditingPos;
            alteredHeight = alteredHeight * (distance.length() / 500);
            paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
            if (inCellX == 0 && inCellY == 0) // Bind the corner
            {
                cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() - 1, cellCoords.getY());
                cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                if(allowLandShapeEditing(cellId)==true) // Open new cell
                {
                    inCellX = landSize - 1;
                    mAlteredCells.emplace_back(cellCoords);
                    paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
                }

                cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() + 1, cellCoords.getY() - 1); // move one cell back and up
                cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                if(allowLandShapeEditing(cellId)==true)  // Open new cell
                {
                    inCellX = 0;
                    inCellY = landSize - 1;
                    mAlteredCells.emplace_back(cellCoords);
                    paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
                }

                cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() - 1, cellCoords.getY());  // move one cell left
                cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                if(allowLandShapeEditing(cellId)==true)  // Open new cell
                {
                    inCellX = landSize - 1;
                    inCellY = landSize - 1;
                    mAlteredCells.emplace_back(cellCoords);
                    paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
                }
            }
            else if (inCellX == 0 && inCellY != 0) // Bind the last row to first row
            {
                cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() - 1, cellCoords.getY());
                cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                if(allowLandShapeEditing(cellId)==true) {} // Open new cell
                inCellX = landSize - 1;
                mAlteredCells.emplace_back(cellCoords);
                paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
            }
            else if (inCellX != 0 && inCellY == 0) // Bind the last row to first row
            {
                cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX(), cellCoords.getY() - 1);
                cellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                if(allowLandShapeEditing(cellId)==true) {} // Open new cell
                inCellY = landSize - 1;
                mAlteredCells.emplace_back(cellCoords);
                paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
            }
        }
    }
}

void CSVRender::TerrainShapeMode::limitHeightChanges(CSMWorld::CellCoordinates cellCoords)
{
    if (CSVRender::PagedWorldspaceWidget *paged =
        dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
    {
        CSMDoc::Document& document = getWorldspaceWidget().getDocument();
        CSMWorld::IdTable& landTable = dynamic_cast<CSMWorld::IdTable&> (
            *document.getData().getTableModel (CSMWorld::UniversalId::Type_Land));
        int landshapeColumn = landTable.findColumnIndex(CSMWorld::Columns::ColumnId_LandHeightsIndex);

        std::string cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX(), cellCoords.getY());
        CSMWorld::CellCoordinates changingCellCoords = cellCoords;
        const CSMWorld::LandHeightsColumn::DataType landShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();

        int limitHeightChange = 1000.0f; // Limited by save format
        int limitingPasses = 1;
        std::vector<CSMWorld::CellCoordinates> tempAlteredCells;

        //Multiple passes may be required if height limit is breached in multiple consecutive vertexes
        for(int i = 0; i < limitingPasses && limitingPasses < 50; ++i)
        {
            // Default check
            for(int inCellX = 0; inCellX < landSize; ++inCellX)
            {
                for(int inCellY = 0; inCellY < landSize; ++inCellY)
                {
                    float alteredHeight = *paged->getCellAlteredHeight(cellCoords, inCellX, inCellY);
                    float thisHeight = landShapePointer[inCellY * landSize + inCellX] + alteredHeight;
                    float leftHeight = 0.0f;
                    float upHeight = 0.0f;

                    if (inCellX == 0)
                    {
                        cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() - 1, cellCoords.getY());
                        changingCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                        const CSMWorld::LandHeightsColumn::DataType landLeftShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                        leftHeight = landLeftShapePointer[inCellY * landSize + (landSize - 2)];
                        if (paged->getCellAlteredHeight(changingCellCoords, landSize - 2, inCellY) != nullptr)
                            leftHeight = landLeftShapePointer[inCellY * landSize + (landSize - 2)] + *paged->getCellAlteredHeight(changingCellCoords, landSize - 2, inCellY);
                    }
                    if (inCellY == 0)
                    {
                        cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX(), cellCoords.getY() - 1);
                        changingCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                        const CSMWorld::LandHeightsColumn::DataType landUpShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                        upHeight = landUpShapePointer[(landSize - 2) * landSize + inCellX];
                        if (paged->getCellAlteredHeight(changingCellCoords, inCellX, landSize - 2) != nullptr)
                            upHeight = landUpShapePointer[(landSize - 2) * landSize + inCellX] + *paged->getCellAlteredHeight(changingCellCoords, inCellX, landSize - 2);
                    }
                    if (inCellX != 0)
                    {
                        leftHeight = landShapePointer[inCellY * landSize + inCellX - 1] + *paged->getCellAlteredHeight(cellCoords, inCellX - 1, inCellY);
                    }
                    if (inCellY != 0)
                    {
                        upHeight = landShapePointer[(inCellY - 1) * landSize + inCellX] + *paged->getCellAlteredHeight(cellCoords, inCellX, inCellY - 1);
                    }
                    bool doChange = false;
                    if (thisHeight - upHeight > limitHeightChange) {alteredHeight = upHeight + limitHeightChange - landShapePointer[inCellY * landSize + inCellX]; doChange = true;}
                    if (thisHeight - upHeight < -limitHeightChange) {alteredHeight = upHeight - limitHeightChange - landShapePointer[inCellY * landSize + inCellX]; doChange = true;}
                    if (thisHeight - leftHeight > limitHeightChange) {alteredHeight = leftHeight + limitHeightChange - landShapePointer[inCellY * landSize + inCellX]; doChange = true;}
                    if (thisHeight - leftHeight < -limitHeightChange) {alteredHeight = leftHeight - limitHeightChange - landShapePointer[inCellY * landSize + inCellX]; doChange = true;}
                    if (doChange == true) paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
                }
            }

            // Inverse check
            for(int inCellX = landSize - 1; inCellX >= 0; --inCellX)
            {
                for(int inCellY = landSize - 1; inCellY >= 0; --inCellY)
                {
                    float alteredHeight = *paged->getCellAlteredHeight(cellCoords, inCellX, inCellY);
                    float thisHeight = landShapePointer[inCellY * landSize + inCellX] + alteredHeight;
                    float rightHeight = 0.0f;
                    float downHeight = 0.0f;

                    if (inCellX == landSize - 1)
                    {
                        cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() + 1, cellCoords.getY());
                        changingCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                        const CSMWorld::LandHeightsColumn::DataType landRightShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                        rightHeight = landRightShapePointer[inCellY * landSize + 1];
                        if (paged->getCellAlteredHeight(changingCellCoords, 1, inCellY) != nullptr)
                            rightHeight = landRightShapePointer[inCellY * landSize + 1] + *paged->getCellAlteredHeight(changingCellCoords, 1, inCellY);
                    }
                    if (inCellY == landSize - 1)
                    {
                        cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX(), cellCoords.getY() + 1);
                        changingCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                        const CSMWorld::LandHeightsColumn::DataType landDownShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                        downHeight = landDownShapePointer[1 * landSize + inCellX];
                        if (paged->getCellAlteredHeight(changingCellCoords, inCellX, 1) != nullptr)
                            downHeight = landDownShapePointer[1 * landSize + inCellX] + *paged->getCellAlteredHeight(changingCellCoords, inCellX, 1);
                    }
                    if (inCellX < landSize - 1)
                    {
                        rightHeight = landShapePointer[inCellY * landSize + inCellX + 1] + *paged->getCellAlteredHeight(cellCoords, inCellX + 1, inCellY);
                    }
                    if (inCellY < landSize - 1)
                    {
                        downHeight = landShapePointer[(inCellY + 1) * landSize + inCellX] + *paged->getCellAlteredHeight(cellCoords, inCellX, inCellY + 1);
                    }

                    bool doChange = false;
                    if (thisHeight - downHeight > limitHeightChange)
                    {
                        alteredHeight = downHeight + limitHeightChange - landShapePointer[inCellY * landSize + inCellX];
                        doChange = true;
                    }
                    if (thisHeight - downHeight < -limitHeightChange)
                    {
                        alteredHeight = downHeight - limitHeightChange - landShapePointer[inCellY * landSize + inCellX];
                        doChange = true;
                    }
                    if (thisHeight - rightHeight > limitHeightChange)
                    {
                        alteredHeight = rightHeight + limitHeightChange - landShapePointer[inCellY * landSize + inCellX];
                        doChange = true;
                    }
                    if (thisHeight - rightHeight < -limitHeightChange)
                    {
                        alteredHeight = rightHeight - limitHeightChange - landShapePointer[inCellY * landSize + inCellX];
                        doChange = true;
                    }

                    if (doChange == true)
                    {
                        paged->setCellAlteredHeight(cellCoords, inCellX, inCellY, alteredHeight);
                        ++limitingPasses;

                        // Fix edges
                        if (inCellX == 0)
                        {
                            cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() - 1, cellCoords.getY());
                            CSMWorld::CellCoordinates leftCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                            const CSMWorld::LandHeightsColumn::DataType landLeftShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                            if(landShapePointer[inCellY * landSize + inCellX] != landLeftShapePointer[inCellY * landSize + (landSize - 1)]) paged->setCellAlteredHeight(leftCellCoords, landSize - 1, inCellY, alteredHeight);
                            tempAlteredCells.emplace_back(leftCellCoords);
                        }
                        if (inCellY == 0)
                        {
                            cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX(), cellCoords.getY() - 1);
                            CSMWorld::CellCoordinates upCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                            const CSMWorld::LandHeightsColumn::DataType landUpShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                            if(landShapePointer[inCellY * landSize + inCellX] != landUpShapePointer[inCellX]) paged->setCellAlteredHeight(upCellCoords, inCellX, landSize - 1, alteredHeight);
                            tempAlteredCells.emplace_back(upCellCoords);
                        }
                        if (inCellX == 0 && inCellY == 0)
                        {
                            cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() - 1, cellCoords.getY() - 1);
                            CSMWorld::CellCoordinates upLeftCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                            const CSMWorld::LandHeightsColumn::DataType landUpLeftShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                            if(landShapePointer[inCellY * landSize + inCellX] != landUpLeftShapePointer[landSize - 1 * landSize + landSize - 1]) paged->setCellAlteredHeight(upLeftCellCoords, landSize - 1, landSize - 1, alteredHeight);
                            tempAlteredCells.emplace_back(upLeftCellCoords);
                        }
                        if (inCellX == landSize - 1)
                        {
                            cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX() + 1, cellCoords.getY());
                            CSMWorld::CellCoordinates rightCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                            const CSMWorld::LandHeightsColumn::DataType landRightShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                            if(landShapePointer[inCellY * landSize + inCellX] != landRightShapePointer[inCellY * landSize]) paged->setCellAlteredHeight(rightCellCoords, 0, inCellY, alteredHeight);
                            tempAlteredCells.emplace_back(rightCellCoords);
                        }
                        if (inCellY == landSize - 1)
                        {
                            cellId = CSMWorld::CellCoordinates::generateId(cellCoords.getX(), cellCoords.getY() + 1);
                            CSMWorld::CellCoordinates downCellCoords = CSMWorld::CellCoordinates::fromId(cellId).first;
                            const CSMWorld::LandHeightsColumn::DataType landDownShapePointer = landTable.data(landTable.getModelIndex(cellId, landshapeColumn)).value<CSMWorld::LandHeightsColumn::DataType>();
                            if(landShapePointer[inCellY * landSize + inCellX] != landDownShapePointer[inCellX]) paged->setCellAlteredHeight(downCellCoords, inCellX, 0, alteredHeight);
                            tempAlteredCells.emplace_back(downCellCoords);
                        }
                    }
                }
            }
        }
        mAlteredCells.insert(std::end(mAlteredCells), std::begin(tempAlteredCells), std::end(tempAlteredCells));
    }
}

void CSVRender::TerrainShapeMode::selectTerrainShapes(std::pair<int, int> vertexCoords, unsigned char selectMode, bool dragOperation)
{
    int r = mBrushSize / 2;
    std::vector<std::pair<int, int>> selections;

    if (mBrushShape == 0)
    {
        selections.emplace_back(vertexCoords);
    }

    if (mBrushShape == 1)
    {
        for(int i = vertexCoords.first - r; i <= vertexCoords.first + r; ++i)
        {
            for(int j = vertexCoords.second - r; j <= vertexCoords.second + r; ++j)
            {
                selections.emplace_back(std::make_pair(i, j));
            }
        }
    }

    if (mBrushShape == 2)
    {
        for(int i = vertexCoords.first - r; i <= vertexCoords.first + r; ++i)
        {
            for(int j = vertexCoords.second - r; j <= vertexCoords.second + r; ++j)
            {
                int distanceX = abs(i - vertexCoords.first);
                int distanceY = abs(j - vertexCoords.second);
                int distance = std::round(sqrt(pow(distanceX, 2)+pow(distanceY, 2)));
                if (distance < r) selections.emplace_back(std::make_pair(i, j));
            }
        }
    }

    if (mBrushShape == 3)
    {
        if(!mCustomBrushShape.empty())
        {
            for(auto const& value: mCustomBrushShape)
            {
                selections.emplace_back(std::make_pair(vertexCoords.first + value.first, vertexCoords.second + value.second));
            }
        }
    }

    if(selectMode == 0) mTerrainShapeSelection->onlySelect(selections);
    if(selectMode == 1) mTerrainShapeSelection->toggleSelect(selections, dragOperation);

}

void CSVRender::TerrainShapeMode::pushEditToCommand(CSMWorld::LandHeightsColumn::DataType& newLandGrid, CSMDoc::Document& document,
    CSMWorld::IdTable& landTable, std::string cellId)
{
    QVariant changedLand;
    changedLand.setValue(newLandGrid);

    QModelIndex index(landTable.getModelIndex (cellId, landTable.findColumnIndex (CSMWorld::Columns::ColumnId_LandHeightsIndex)));

    QUndoStack& undoStack = document.getUndoStack();
    undoStack.push (new CSMWorld::ModifyCommand(landTable, index, changedLand));
}

void CSVRender::TerrainShapeMode::pushNormalsEditToCommand(CSMWorld::LandNormalsColumn::DataType& newLandGrid, CSMDoc::Document& document,
    CSMWorld::IdTable& landTable, std::string cellId)
{
    QVariant changedLand;
    changedLand.setValue(newLandGrid);

    QModelIndex index(landTable.getModelIndex (cellId, landTable.findColumnIndex (CSMWorld::Columns::ColumnId_LandNormalsIndex)));

    QUndoStack& undoStack = document.getUndoStack();
    undoStack.push (new CSMWorld::ModifyCommand(landTable, index, changedLand));
}

bool CSVRender::TerrainShapeMode::allowLandShapeEditing(std::string cellId)
{
    CSMDoc::Document& document = getWorldspaceWidget().getDocument();
    CSMWorld::IdTable& landTable = dynamic_cast<CSMWorld::IdTable&> (
        *document.getData().getTableModel (CSMWorld::UniversalId::Type_Land));
    CSMWorld::IdTree& cellTable = dynamic_cast<CSMWorld::IdTree&> (
            *document.getData().getTableModel (CSMWorld::UniversalId::Type_Cells));

    bool noCell = document.getData().getCells().searchId (cellId)==-1;
    bool noLand = document.getData().getLand().searchId (cellId)==-1;

    if (noCell)
    {
        std::string mode = CSMPrefs::get()["3D Scene Editing"]["outside-landedit"].toString();

        // target cell does not exist
        if (mode=="Discard")
            return false;

        if (mode=="Create cell and land, then edit")
        {
            std::unique_ptr<CSMWorld::CreateCommand> createCommand (
                new CSMWorld::CreateCommand (cellTable, cellId));
            int parentIndex = cellTable.findColumnIndex (CSMWorld::Columns::ColumnId_Cell);
            int index = cellTable.findNestedColumnIndex (parentIndex, CSMWorld::Columns::ColumnId_Interior);
            createCommand->addNestedValue (parentIndex, index, false);
            document.getUndoStack().push (createCommand.release());

            if (CSVRender::PagedWorldspaceWidget *paged =
                dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
            {
                CSMWorld::CellSelection selection = paged->getCellSelection();
                selection.add (CSMWorld::CellCoordinates::fromId (cellId).first);
                paged->setCellSelection (selection);
            }
        }
    }
    else if (CSVRender::PagedWorldspaceWidget *paged =
        dynamic_cast<CSVRender::PagedWorldspaceWidget *> (&getWorldspaceWidget()))
    {
        CSMWorld::CellSelection selection = paged->getCellSelection();
        if (!selection.has (CSMWorld::CellCoordinates::fromId (cellId).first))
        {
            // target cell exists, but is not shown
            std::string mode =
                CSMPrefs::get()["3D Scene Editing"]["outside-visible-landedit"].toString();

            if (mode=="Discard")
                return false;

            if (mode=="Show cell and edit")
            {
                selection.add (CSMWorld::CellCoordinates::fromId (cellId).first);
                paged->setCellSelection (selection);
            }
        }
    }

    if (noLand)
    {
        std::string mode = CSMPrefs::get()["3D Scene Editing"]["outside-landedit"].toString();

        // target cell does not exist
        if (mode=="Discard")
            return false;

        if (mode=="Create cell and land, then edit")
        {
            document.getUndoStack().push (new CSMWorld::CreateCommand (landTable, cellId));
        }
    }

    return true;
}

void CSVRender::TerrainShapeMode::dragMoveEvent (QDragMoveEvent *event)
{
}

void CSVRender::TerrainShapeMode::setBrushSize(int brushSize)
{
    mBrushSize = brushSize;
}

void CSVRender::TerrainShapeMode::setBrushShape(int brushShape)
{
    mBrushShape = brushShape;

    //Set custom brush shape
    if (mBrushShape == 3 && !mTerrainShapeSelection->getTerrainSelection().empty())
    {
        auto terrainSelection = mTerrainShapeSelection->getTerrainSelection();
        int selectionCenterX = 0;
        int selectionCenterY = 0;
        int selectionAmount = 0;

        for(auto const& value: terrainSelection)
        {
            selectionCenterX = selectionCenterX + value.first;
            selectionCenterY = selectionCenterY + value.second;
            ++selectionAmount;
        }
        selectionCenterX = selectionCenterX / selectionAmount;
        selectionCenterY = selectionCenterY / selectionAmount;

        mCustomBrushShape.clear();
        std::pair<int, int> differentialPos {};
        for(auto const& value: terrainSelection)
        {
            differentialPos.first = value.first - selectionCenterX;
            differentialPos.second = value.second - selectionCenterY;
            mCustomBrushShape.push_back(differentialPos);
        }
    }
}

CSVRender::PagedWorldspaceWidget& CSVRender::TerrainShapeMode::getPagedWorldspaceWidget()
{
    return dynamic_cast<PagedWorldspaceWidget&>(getWorldspaceWidget());
}