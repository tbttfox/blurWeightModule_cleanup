
/*
#computeHit
#drawMeshWhileDrag
#expandHit
#getASoloColor
#getColorWithMirror
#getCommandIndexModifiers
#getFalloffValue
#getMirrorHit
#getSurroundingVerticesPerVert
#growArrayOfHitsFromCenters
#addBrushShapeFallof
#mergeMirrorArray
#preparePaint
#refreshPointsNormals
#refreshColors
#applyCommand
#applyCommandMirror
#editArrayMirror
#doDrag
#doDragCommon
#doPerformPaint
#maya2019RefreshColors

#doPress
#doPressCommon

#doRelease
#doReleaseCommon

doTheAction

*/

#include "enums.h"
#include "skinBrushStructured.h"
#include <math.h>

#include <maya/M3dView.h>
#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MCursor.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MEulerRotation.h>
#include <maya/MEvent.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnCamera.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnDoubleIndexedComponent.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFrameContext.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItMeshVertex.h>
#include <maya/MItSelectionList.h>
#include <maya/MMatrix.h>
#include <maya/MMeshIntersector.h>
#include <maya/MPointArray.h>
#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MPxToolCommand.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MSyntax.h>
#include <maya/MThreadUtils.h>
#include <maya/MToolsInfo.h>
#include <maya/MUIDrawManager.h>
#include <maya/MUintArray.h>
#include <maya/MUserEventMessage.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define CHECK_MSTATUS_AND_RETURN_SILENT(status)                                                    \
    if (status != MStatus::kSuccess)                                                               \
        return MStatus::kSuccess;

typedef float coord_t;
typedef std::array<coord_t, 3> point_t;

MColor getASoloColor(double val, const UserInputData &ui, const InfluenceData &infl)
{
    if (val == 0) {
        return MColor(0, 0, 0);
    }
    val = (ui.maxSoloColor - ui.minSoloColor) * val + ui.minSoloColor;
    MColor soloColor;
    if (ui.soloColorTypeVal == 0) { // black and white
        soloColor = MColor(val, val, val);
    }
    else if (ui.soloColorTypeVal == 1) { // lava
        val *= 2;
        if (val > 1) {
            soloColor = MColor(val, (val - 1), 0);
        }
        else {
            soloColor = MColor(val, 0, 0);
        }
    }
    else { // influence
        soloColor = val * infl.jointsColors[ui.influenceIndex];
    }
    return soloColor;
}

ModifierCommands
getCommandIndexModifiers(const UserInputData &ui, const InteractionPerFrameData &frame)
{
    // 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen - 6 LockVertices - 7
    // unlockVertices
    ModifierCommands theCommandIndex = ui.commandIndex;

    if (ui.commandIndex == ModifierCommands::Add) {
        if (frame.modifierNoneShiftControl == ui.smoothModifier) {
            theCommandIndex = ModifierCommands::Smooth;
        }
        else if (frame.modifierNoneShiftControl == ui.removeModifier) {
            theCommandIndex = ModifierCommands::Remove;
        }
    }
    else if (ui.commandIndex == ModifierCommands::LockVertices) {
        if (frame.modifierNoneShiftControl == ModifierKeys::Shift) {
            theCommandIndex = ModifierCommands::UnlockVertices;
        }
    }
    if (frame.modifierNoneShiftControl == ModifierKeys::ControlShift) {
        theCommandIndex = ModifierCommands::Sharpen;
    }

    return theCommandIndex;
}

void getColorWithMirror(
    int vertexIndex, float valueBase, float valueMirror, MColor &multColor, MColor &soloColor,
    const UserInputData &ui, const InfluenceData &infl, const WeightData &weights,
    const InteractionPerFrameData &frame
)
{
    MColor white(1, 1, 1, 1);
    MColor black(0, 0, 0, 1);
    ModifierCommands theCommandIndex = getCommandIndexModifiers(ui, frame);

    float sumValue = valueBase + valueMirror;
    sumValue = std::min(float(1.0), sumValue);
    float biggestValue = std::max(valueBase, valueMirror);

    if ((theCommandIndex == ModifierCommands::LockVertices) ||
        (theCommandIndex == ModifierCommands::UnlockVertices)) {
        if (theCommandIndex == ModifierCommands::LockVertices) { // lock verts if not already locked
            soloColor = weights.lockVertColor;
            multColor = weights.lockVertColor;
        }
        else { // unlock verts
            multColor = weights.multiCurrentColors[vertexIndex];
            soloColor = weights.soloCurrentColors[vertexIndex];
        }
    }
    else if (!weights.lockVertices[vertexIndex]) {
        MColor currentColor = weights.multiCurrentColors[vertexIndex];
        int influenceMirrorColorIndex = ui.mirrorInfluences[ui.influenceIndex];
        MColor jntColor = infl.jointsColors[ui.influenceIndex];
        MColor jntMirrorColor = infl.jointsColors[influenceMirrorColorIndex];
        if (infl.lockJoints[ui.influenceIndex] == 1) {
            jntColor = infl.lockJntColor;
        }
        if (infl.lockJoints[influenceMirrorColorIndex] == 1) {
            jntMirrorColor = infl.lockJntColor;
        }
        // 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen - 6 LockVertices - 7
        // UnLockVertices

        if (theCommandIndex == ModifierCommands::Smooth ||
            theCommandIndex == ModifierCommands::Sharpen) {
            soloColor = biggestValue * white +
                        (1.0 - biggestValue) * weights.soloCurrentColors[vertexIndex];
            multColor = biggestValue * white +
                        (1.0 - biggestValue) * weights.multiCurrentColors[vertexIndex];
        }
        else {
            double newW = 0.0;
            int ind_swl = vertexIndex * infl.nbJoints + ui.influenceIndex;
            if (ind_swl < weights.skinWeightList.length()) {
                newW = weights.skinWeightList[ind_swl];
            }
            double newWMirror = 0.0;
            int ind_swlM = vertexIndex * infl.nbJoints + influenceMirrorColorIndex;
            if (ind_swlM < weights.skinWeightList.length()) {
                newWMirror = weights.skinWeightList[ind_swlM];
            }
            double sumNewWs = newW + newWMirror;

            if (theCommandIndex == ModifierCommands::Remove) {
                newW -= biggestValue;
                newW = std::max(0.0, newW);
                multColor = currentColor * (1.0 - biggestValue) + black * biggestValue; // white
            }
            else {
                if (theCommandIndex == ModifierCommands::Add) {
                    newW += double(valueBase);
                    newWMirror += double(valueMirror);
                }
                else if (theCommandIndex == ModifierCommands::AddPercent) {
                    newW += valueBase * newW;
                    newWMirror += valueMirror * newWMirror;
                }
                else if (theCommandIndex == ModifierCommands::Absolute) {
                    newW = valueBase;
                    newWMirror = valueMirror;
                }

                newW = std::min(1.0, newW);
                newWMirror = std::min(1.0, newWMirror);
                sumNewWs = newW + newWMirror;
                double currentColorVal = 1.0 - sumNewWs;
                if (sumNewWs > 1.0) {
                    newW /= sumNewWs;
                    newWMirror /= sumNewWs;
                    currentColorVal = 0.0;
                }
                multColor = currentColor * currentColorVal + jntColor * newW +
                            jntMirrorColor * newWMirror; // white
            }
            soloColor = getASoloColor(newW, ui, infl);
        }
    }
}

MStatus drawMeshWhileDrag(
    ModifierCommands theCommandIndex, const MeshState &mesh, const WeightData &weights,
    const InfluenceData &infl, const UserInputData &ui, const InteractionPersistentData &persist,
    MHWRender::MUIDrawManager &drawManager
)
{
    // This function is the hottest path when painting
    // So it can and should be optimized more
    // I think the endgame for this is to only update the changed vertices each runthrough
    int nbVtx = persist.verticesPainted.size();

    MFloatPointArray points(nbVtx);
    MFloatVectorArray normals(nbVtx);
    MColor theCol(1, 1, 1), white(1, 1, 1, 1), black(0, 0, 0, 1);

    MColorArray pointsColors(nbVtx, theCol);

    MUintArray indices, indicesEdges; // (nbVtx);
    MColorArray darkEdges;            // (nbVtx, MColor(0.5, 0.5, 0.5));

    MColor newCol, col;

    std::unordered_map<int, unsigned int> verticesMap;
    std::vector<bool> fatFaces_bitset;
    std::vector<bool> fatEdges_bitset;
    std::vector<bool> vertMap_bitset;

    fatFaces_bitset.resize(mesh.numFaces);
    fatEdges_bitset.resize(mesh.numEdges);
    vertMap_bitset.resize(mesh.numVertices);

    MColor baseColor, baseMirrorColor;
    float h, s, v;
    // get baseColor ----------------------------------
    // 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen - 6 LockVertices - 7
    // UnLockVertices

    if (ui.drawTransparency || ui.drawPoints) {
        switch (theCommandIndex) {
        case ModifierCommands::LockVertices:
            baseColor = weights.lockVertColor;
            break;
        case ModifierCommands::Remove:
            baseColor = black;
            break;
        case ModifierCommands::UnlockVertices:
            baseColor = white;
            break;
        case ModifierCommands::Smooth:
            baseColor = white;
            break;
        case ModifierCommands::Sharpen:
            baseColor = white;
            break;
        default:
            baseColor = infl.jointsColors[ui.influenceIndex];
            if (ui.paintMirror != 0) {
                baseMirrorColor = infl.jointsColors[ui.mirrorInfluences[ui.influenceIndex]];
                baseMirrorColor.get(MColor::kHSV, h, s, v);
                baseMirrorColor.set(MColor::kHSV, h, pow(s, 0.8), pow(v, 0.15));
            }
        }

        baseColor.get(MColor::kHSV, h, s, v);
        baseColor.set(MColor::kHSV, h, pow(s, 0.8), pow(v, 0.15));
        if ((theCommandIndex != ModifierCommands::Add) &&
            (theCommandIndex != ModifierCommands::AddPercent)) {
            baseMirrorColor = baseColor;
        }
    }

    // pull data out of the dictionary
    // TODO: There's probably a copy-less way to do this
    std::vector<std::pair<int, std::pair<float, float>>> mja;
    mja.reserve(weights.mirroredJoinedArray.size());
    for (const auto &pt : weights.mirroredJoinedArray) {
        mja.push_back(pt);
    }

    MColorArray colors, colorsSolo;
    colors.setLength(mja.size());
    colorsSolo.setLength(mja.size());

    MColorArray *usedColors;
    MColorArray *currentColors;
    if (ui.soloColorVal == 1) {
        usedColors = &colorsSolo;
        currentColors = const_cast<MColorArray *>(&weights.soloCurrentColors);
    }
    else {
        usedColors = &colors;
        currentColors = const_cast<MColorArray *>(&weights.multiCurrentColors);
    }

    bool doTransparency = ui.drawTransparency;
    bool applyGamma = true;
    if (theCommandIndex == ModifierCommands::LockVertices ||
        theCommandIndex == ModifierCommands::UnlockVertices) {
        // Don't do transparency when locking/unlocking vertices
        doTransparency = false;
        applyGamma = false;
    }

#pragma omp parallel for
    for (unsigned i = 0; i < mja.size(); ++i) {
        const auto &pt = mja[i];
        int ptIndex = pt.first;
        MFloatPoint posPoint(
            mesh.mayaRawPoints[ptIndex * 3], mesh.mayaRawPoints[ptIndex * 3 + 1],
            mesh.mayaRawPoints[ptIndex * 3 + 2]
        );
        posPoint = posPoint * mesh.inclusiveMatrix;
        points.set(posPoint, i);
        normals.set(mesh.verticesNormals[ptIndex], i);
    }

    if (ui.drawTriangles) {
#pragma omp parallel for
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            int ptIndex = pt.first;
            float weightBase = pt.second.first;
            float weightMirror = pt.second.second;
            MColor multColor, soloColor;
            // TODO: Extract
            // getColorWithMirror(ptIndex, weightBase, weightMirror, colors, colorsSolo, multColor,
            // soloColor);
            colors.set(multColor, i);
            colorsSolo.set(soloColor, i);
        }

        if (applyGamma) {
#pragma omp parallel for
            for (unsigned i = 0; i < mja.size(); ++i) {
                const auto &pt = mja[i];
                float weightBase = pt.second.first;
                float weightMirror = pt.second.second;
                float transparency = (doTransparency) ? weightBase + weightMirror : 1.0;
                MColor &colRef = (*usedColors)[i];
                colRef.get(MColor::kHSV, h, s, v);
                colRef.set(MColor::kHSV, h, pow(s, 0.8), pow(v, 0.15), transparency);
            }
        }
    }

    if (ui.drawPoints) {
#pragma omp parallel for
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            float weight = pt.second.first + pt.second.second;
            pointsColors[i] = weight * baseColor + (1.0 - weight) * (*currentColors)[pt.first];
        }
    }

    if (ui.drawEdges) {
        darkEdges.setLength(mja.size());
#pragma omp parallel for
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            float transparency = (doTransparency) ? pt.second.first + pt.second.second : 1.0;
            darkEdges.set(i, 0.5f, 0.5f, 0.5f, transparency);
        }
    }

    if (ui.drawTriangles || ui.drawEdges) {
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            verticesMap[pt.first] = i;
            vertMap_bitset[pt.first] = true;
        }
    }

    if (ui.drawTriangles) {
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            int ptIndex = pt.first;
            for (int fi = mesh.perVertexFacesSetINDEX[ptIndex];
                 fi < mesh.perVertexFacesSetINDEX[ptIndex + 1]; ++fi) {
                fatFaces_bitset[mesh.perVertexFacesSetFLAT[fi]] = true;
            }
        }
    }

    if (ui.drawEdges) {
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            int ptIndex = pt.first;
            for (int ei = mesh.perVertexEdgesSetINDEX[ptIndex];
                 ei < mesh.perVertexEdgesSetINDEX[ptIndex + 1]; ++ei) {
                fatEdges_bitset[mesh.perVertexEdgesSetFLAT[ei]] = true;
            }
        }
    }

    if (ui.drawTriangles) {
        // bitset is faster than an unordered_set in this case
        // may be worth keeping the bitsets around on the brush
        // so we don't have to constantly allocate memory
        for (unsigned f = 0; f < fatFaces_bitset.size(); ++f) {
            if (!fatFaces_bitset[f]) {
                continue;
            }
            for (int t = mesh.perFaceTriangleStartINDEX[f];
                 t < mesh.perFaceTriangleStartINDEX[f + 1]; ++t) {
                int v0 = mesh.perFaceTriangleVerticesFLAT[t * 3 + 0];
                int v1 = mesh.perFaceTriangleVerticesFLAT[t * 3 + 1];
                int v2 = mesh.perFaceTriangleVerticesFLAT[t * 3 + 2];
                if (!vertMap_bitset[v0]) {
                    continue;
                }
                if (!vertMap_bitset[v1]) {
                    continue;
                }
                if (!vertMap_bitset[v2]) {
                    continue;
                }
                auto it0 = verticesMap.find(v0);
                auto it1 = verticesMap.find(v1);
                auto it2 = verticesMap.find(v2);
                indices.append(it0->second);
                indices.append(it1->second);
                indices.append(it2->second);
            }
        }

        auto style = MHWRender::MUIDrawManager::kFlat;
        drawManager.setPaintStyle(style); // kFlat // kShaded // kStippled
        drawManager.mesh(
            MHWRender::MUIDrawManager::kTriangles, points, &normals, usedColors, &indices
        );
    }

    if (ui.drawEdges) {
        // bitset is faster than an unordered_set in this case
        // may be worth keeping the bitsets around on the brush
        // so we don't have to constantly allocate memory
        for (unsigned e = 0; e < fatEdges_bitset.size(); ++e) {
            if (!fatEdges_bitset[e]) {
                continue;
            }
            auto &pairEdges = mesh.perEdgeVertices[e];

            if (!vertMap_bitset[pairEdges.first]) {
                continue;
            }
            if (!vertMap_bitset[pairEdges.second]) {
                continue;
            }
            auto it0 = verticesMap.find(pairEdges.first);
            auto it1 = verticesMap.find(pairEdges.second);
            indicesEdges.append(it0->second);
            indicesEdges.append(it1->second);
        }

        drawManager.setDepthPriority(2);
        drawManager.mesh(
            MHWRender::MUIDrawManager::kLines, points, &normals, &darkEdges, &indicesEdges
        );
    }

    if (ui.drawPoints) {
        drawManager.setPointSize(4);
        drawManager.mesh(MHWRender::MUIDrawManager::kPoints, points, NULL, &pointsColors);
    }
    return MStatus::kSuccess;
}

std::vector<int> getSurroundingVerticesPerVert(int vertexIndex, const MeshState &mesh)
{
    auto first =
        mesh.perVertexVerticesSetFLAT.begin() + mesh.perVertexVerticesSetINDEX[vertexIndex];
    auto last = mesh.perVertexVerticesSetFLAT.begin() +
                mesh.perVertexVerticesSetINDEX[vertexIndex + (int)1];
    std::vector<int> newVec(first, last);
    return newVec;
};

coord_t distance_sq(const point_t &a, const point_t &b)
{
    coord_t x = a[0] - b[0];
    coord_t y = a[1] - b[1];
    coord_t z = a[2] - b[2];
    return x * x + y * y + z * z;
}

void growArrayOfHitsFromCenters(
    bool coverageVal, const UserInputData &ui, const MeshState &mesh,
    const InteractionPerFrameData &frame, const MFloatPointArray &AllHitPoints,
    std::unordered_map<int, float> &dicVertsDist
)
{
    if (AllHitPoints.length() == 0) {
        return;
    }

    // set of visited vertices
    std::unordered_set<int> vertsVisited, vertsWithinDistance;

    for (const auto &element : dicVertsDist) {
        vertsVisited.insert(element.first);
    }
    vertsWithinDistance = vertsVisited;

    // start of growth
    std::unordered_set<int> borderOfGrowth;
    borderOfGrowth = vertsVisited;

    // make the std vector points for faster sorting
    std::vector<point_t> points;
    for (auto hitPt : AllHitPoints) {
        point_t tmp = {hitPt.x, hitPt.y, hitPt.z};
        points.push_back(tmp);
    }

    bool keepGoing = true;
    while (keepGoing) {
        keepGoing = false;

        // grow the vertices
        // std::vector<int> setOfVertsGrow;
        std::unordered_set<int> setOfVertsGrow;
        for (const int &vertexIndex : borderOfGrowth) {
            std::vector<int> ttt = getSurroundingVerticesPerVert(vertexIndex, mesh);
            setOfVertsGrow.insert(ttt.begin(), ttt.end());
        }

        // get the vertices that are grown
        std::vector<int> verticesontheborder;
        std::set_difference(
            setOfVertsGrow.begin(), setOfVertsGrow.end(), vertsVisited.begin(), vertsVisited.end(),
            std::inserter(verticesontheborder, verticesontheborder.end())
        );

        std::unordered_set<int> foundGrowVertsWithinDistance;

        // for all vertices grown
        for (int vertexBorder : verticesontheborder) {
            // First check the normal
            if (!coverageVal) {
                MVector vertexBorderNormal = mesh.verticesNormals[vertexBorder];
                double multVal = frame.worldVector * vertexBorderNormal;
                if (multVal > 0.0) {
                    continue;
                }
            }
            float closestDist = -1;
            // find the closestDistance and closest Vertex from visited vertices
            point_t thisPoint;
            thisPoint[0] = mesh.mayaRawPoints[vertexBorder * 3];
            thisPoint[1] = mesh.mayaRawPoints[vertexBorder * 3 + 1];
            thisPoint[2] = mesh.mayaRawPoints[vertexBorder * 3 + 2];

            auto glambda = [&thisPoint](const point_t &a, const point_t &b) {
                float aRes = distance_sq(a, thisPoint);
                float bRes = distance_sq(b, thisPoint);
                return aRes < bRes;
            };
            std::partial_sort(points.begin(), points.begin() + 1, points.end(), glambda);
            auto closestPoint = points.front();
            closestDist = std::sqrt(distance_sq(closestPoint, thisPoint));
            // get the new distance between the closest visited vertex and the grow vertex
            if (closestDist <= ui.sizeVal) { // if in radius of the brush
                // we found a vertex in the radius
                // now add to the visited and add the distance to the dictionnary
                keepGoing = true;
                foundGrowVertsWithinDistance.insert(vertexBorder);
                auto ret = dicVertsDist.insert(std::make_pair(vertexBorder, closestDist));
                if (!ret.second) {
                    ret.first->second = std::min(closestDist, ret.first->second);
                }
            }
        }
        // this vertices has been visited, let's not consider them anymore

        vertsVisited.insert(verticesontheborder.begin(), verticesontheborder.end());
        vertsWithinDistance.insert(
            foundGrowVertsWithinDistance.begin(), foundGrowVertsWithinDistance.end()
        );
        borderOfGrowth = foundGrowVertsWithinDistance;
    }
}

static void copyToFloatMatrix(const MMatrix &src, MFloatMatrix &dst)
{
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 4; ++j) {
            dst[i][j] = (float)src[i][j];
        }
    }
}

template <typename T = int> class FlatCounts {
  private:
    std::vector<size_t> offsets; // actually offsets
    std::vector<T> values;

  public:
    // Setter from counts/vals pair of vectors
    void set(const std::vector<T> &inCounts, const std::vector<T> &inVals)
    {
        offsets.clear();
        values.clear();
        values = inVals;

        offsets.resize(inCounts.size() + 1);
        offsets[0] = 0;
        size_t i = 1, v = 0;
        for (const auto &c : inCounts) {
            v += c;
            offsets[i++] = v;
        }
    }

    // Setter from vector-of-vectors
    void set(const std::vector<std::vector<T>> &inVals)
    {
        offsets.clear();
        values.clear();
        offsets.reserve(inVals.size() + 1);
        offsets.push_back(0);
        size_t i = 1, v = 0;
        for (const auto &sub : inVals) {
            v += sub.size();
            offsets[i++] = v;
            values.insert(values.end(), sub.begin(), sub.end());
        }
    }

    // Setter from unordered_map of vectors
    template <typename K> void set(const std::unordered_map<K, std::vector<T>> &inVals)
    {
        offsets.clear();
        values.clear();

        const auto maxit = std::max_element(inVals.begin(), inVals.end());
        K maxkey = maxit->first;

        offsets.reserve(maxkey + 1);
        offsets.push_back(0);
        size_t offset = 0;
        for (size_t i = 0; i < maxkey; ++i) {
            auto search = inVals.find(i);
            if (search != inVals.end()) {
                offset += search->second.size();
                values.insert(values.end(), search->second.begin(), search->second.end());
            }
            offsets.push_back(offset);
        }
    }

    size_t length() const { return offsets.size() - 1; }

    const std::span<const T> operator[](size_t i) const
    {
        return std::span<const T>(values.begin() + offsets[i], values.begin() + offsets[i + 1]);
    }
};

template <typename T = int, size_t C = 2> class FlatChunks {
  private:
    std::vector<T> values;

  public:
    void set(const std::vector<std::array<int, C>> &invals)
    {
        values.reserve(invals.size() * C);
        for (const auto &ev : invals) {
            for (const auto &v : ev) {
                values.push_back(v);
            }
        }
    }

    void set(const T *invals, size_t length)
    {
        values.clear();
        values.insert(values.end(), invals, invals + (length * C));
    }

    size_t length() const { return values.size() / 2; }

    const std::span<const T> operator[](size_t i) const
    {
        return std::span<const T>(values.begin() + (C * i), values.begin() + (C * (i + 1)));
    }
};

template <typename T = int, size_t C = 3> class DoubleChunks {
  private:
    std::vector<size_t> offsets;
    std::vector<T> values;

  public:
    void set(const std::vector<std::vector<std::array<T, C>>> &perEdgeVertices)
    {
        values.clear();
        offsets.clear();
        offsets.push_back(0);
        size_t offset = 0;
        values.reserve(perEdgeVertices.size() * C);
        for (const auto &eev : perEdgeVertices) {
            offset += eev.size();
            offsets.push_back(offset);
            for (const auto &ev : eev) {
                for (const auto &v : ev) {
                    values.push_back(v);
                }
            }
        }
    }

    size_t length() const { return offsets.size() - 1; }

    size_t length2(size_t i) const { return offsets[i + 1] - offsets[i]; }

    const std::span<const T> operator()(size_t i, size_t j) const
    {
        return std::span<const T>(
            values.begin() + offsets[i] + (C * j), values.begin() + offsets[i] + (C * (j + 1))
        );
    }
};

void getAllConnections(
    MFnMesh &meshFn,   // For getting the mesh data
    MDagPath &meshDag, // So I can get the "maya canonical" edges

    FlatCounts<int> &pvFaces,   // x[vertIdx] -> [list-of-faceIdxs]
    FlatCounts<int> &pvEdges,   // x[vertIdx] -> [list-of-edgeIdxs]
    FlatCounts<int> &pvVerts,   // x[vertIdx] -> [list-of-vertIdxs that share a face with the input]
    FlatCounts<int> &pfVerts,   // x[faceIdx] -> [list-of-vertIdxs]
    FlatChunks<int> &peVerts,   // x[edgeIdx] -> [pair-of-vertIdxs]
    DoubleChunks<int> &pftVerts // x(face, triIdx) -> [list_of_vertIdxs]
)
{
    // Get the data from the mesh and dag path
    MIntArray counts, flatFaces, triangleCounts, triangleVertices;
    meshFn.getVertices(counts, flatFaces);
    meshFn.getTriangles(triangleCounts, triangleVertices);
    int numVertices = meshFn.numVertices();
    int numFaces = meshFn.numPolygons();
    int numEdges = meshFn.numEdges();

    std::vector<std::vector<int>> perVertexFaces;
    std::vector<std::vector<int>> perVertexEdges;
    std::vector<std::vector<int>> perVertexVertices;
    std::vector<std::vector<int>> perFaceVertices;
    std::vector<std::array<int, 2>> perEdgeVertices;
    std::vector<std::vector<std::array<int, 3>>> perFaceTriangleVertices;

    // Reset all the output vectors
    perVertexFaces.resize(numVertices);
    perVertexEdges.resize(numVertices);
    perVertexVertices.resize(numVertices);
    perFaceVertices.resize(numFaces);
    perFaceTriangleVertices.resize(numFaces);
    perEdgeVertices.resize(numEdges);

    // Get the face/vert correlations
    unsigned int iter = 0, triIter = 0;
    for (unsigned int faceId = 0; faceId < numFaces; ++faceId) {
        for (int i = 0; i < counts[faceId]; ++i, ++iter) {
            int indVertex = flatFaces[iter];
            perFaceVertices[faceId].push_back(indVertex);
            perVertexFaces[indVertex].push_back(faceId);
        }
        perFaceTriangleVertices[faceId].resize(triangleCounts[faceId]);
        for (int triId = 0; triId < triangleCounts[faceId]; ++triId) {
            perFaceTriangleVertices[faceId][triId][0] = triangleVertices[triIter++];
            perFaceTriangleVertices[faceId][triId][1] = triangleVertices[triIter++];
            perFaceTriangleVertices[faceId][triId][2] = triangleVertices[triIter++];
        }
    }

    // Get the edge/vert correlations
    MItMeshEdge edgeIter(meshDag);
    for (unsigned i = 0; !edgeIter.isDone(); edgeIter.next(), ++i) {
        int pt0Index = edgeIter.index(0);
        int pt1Index = edgeIter.index(1);
        perVertexEdges[pt0Index].push_back(i);
        perVertexEdges[pt1Index].push_back(i);
        perEdgeVertices[i][0] = pt0Index;
        perEdgeVertices[i][1] = pt1Index;
    }

    // Build the face-growing neighbors
#pragma omp parallel for
    for (int vertIdx = 0; vertIdx < numVertices; ++vertIdx) {
        std::vector<int> &toAdd = perVertexVertices[vertIdx];
        for (int faceIdx : perVertexFaces[vertIdx]) {
            std::vector<int> &faceVerts = perFaceVertices[faceIdx];
            toAdd.insert(toAdd.end(), faceVerts.begin(), faceVerts.end());
        }
        // For such a short vec, sorting then erasing is the fastest
        std::sort(toAdd.begin(), toAdd.end());
        toAdd.erase(std::unique(toAdd.begin(), toAdd.end()), toAdd.end());
    }

    // Put all the data in flattened arrays
    pvFaces.set(perVertexFaces);
    pvEdges.set(perVertexEdges);
    pvVerts.set(perVertexVertices);
    pfVerts.set(perFaceVertices);
    peVerts.set(perEdgeVertices);
    pftVerts.set(perFaceTriangleVertices);
}

double getFalloffValue(int curveVal, double value, double strength)
{
    switch (curveVal) {
    case 0: // no falloff
        return strength;
    case 1: // linear
        return value * strength;
    case 2: // smoothstep
        return (value * value * (3 - 2 * value)) * strength;
    case 3: // narrow - quadratic
        return (1 - pow((1 - value) / 1, 0.4)) * strength;
    default:
        return value;
    }
}

void getVerticesInVolumeRange(
    int index, double rangeVal, const UserInputData &ui, const MeshState &mesh,
    const MIntArray &volumeIndices, MIntArray &rangeIndices, MFloatArray &values
)
{
    unsigned int i;

    double radius = ui.sizeVal * rangeVal;
    radius *= radius;

    double smoothStrength = ui.strengthVal;
    if (ui.fractionOversamplingVal) {
        smoothStrength /= ui.oversamplingVal;
    }

    MItMeshVertex vtxIter(mesh.meshDag);
    int prevIndex;
    vtxIter.setIndex(index, prevIndex);

    MPoint point = vtxIter.position(MSpace::kWorld);

    for (i = 0; i < volumeIndices.length(); i++) {
        int volumeIndex = volumeIndices[i];

        vtxIter.setIndex(volumeIndex, prevIndex);

        MPoint pnt = vtxIter.position(MSpace::kWorld);

        double x = pnt.x - point.x;
        double y = pnt.y - point.y;
        double z = pnt.z - point.z;
        double delta = x * x + y * y + z * z;

        if (volumeIndex != index && delta <= radius) {
            rangeIndices.append(volumeIndex);

            float value = (float)(1 - (delta / radius));
            value = (float)getFalloffValue(ui.curveVal, value, smoothStrength);
            values.append(value);
        }

        vtxIter.next();
    }
}

bool getMirrorHit(
    const UserInputData &ui, const MeshState &mesh, const InteractionStartData &start,
    const InteractionPerFrameData &frame, const MirrorableData &mdata, int &faceHit,
    MFloatPoint &hitPoint
)
{
    MStatus stat;

    MMatrix mirrorMatrix;
    double XVal = 1., YVal = 1.0, ZVal = 1.0;
    if ((ui.paintMirror == 1) || (ui.paintMirror == 4) || (ui.paintMirror == 7)) {
        XVal = -1.;
    }
    if ((ui.paintMirror == 2) || (ui.paintMirror == 5) || (ui.paintMirror == 8)) {
        YVal = -1.;
    }
    if ((ui.paintMirror == 3) || (ui.paintMirror == 6) || (ui.paintMirror == 9)) {
        ZVal = -1.;
    }
    mirrorMatrix.matrix[0][0] = XVal;
    mirrorMatrix.matrix[0][1] = 0;
    mirrorMatrix.matrix[0][2] = 0;
    mirrorMatrix.matrix[1][0] = 0;
    mirrorMatrix.matrix[1][1] = YVal;
    mirrorMatrix.matrix[1][2] = 0;
    mirrorMatrix.matrix[2][0] = 0;
    mirrorMatrix.matrix[2][1] = 0;
    mirrorMatrix.matrix[2][2] = ZVal;

    // we're going to mirror by x -1'
    MPointOnMesh pointInfo;
    if (ui.paintMirror > 0 && ui.paintMirror < 4) { // if we compute the orig mesh
        MPoint pointToMirror = MPoint(frame.origHitPoint);
        MPoint mirrorPoint = pointToMirror * mirrorMatrix;

        stat = start.intersectorOrigShape.getClosestPoint(mirrorPoint, pointInfo, ui.mirrorMinDist);
        if (MS::kSuccess != stat) {
            return false;
        }

        faceHit = pointInfo.faceIndex();
        int hitTriangle = pointInfo.triangleIndex();
        float hitBary1, hitBary2;
        pointInfo.getBarycentricCoords(hitBary1, hitBary2);

        int triBase = (mesh.perFaceTriangleStartINDEX[faceHit] + hitTriangle) * 3;
        int t0 = mesh.perFaceTriangleVerticesFLAT[triBase + 0];
        int t1 = mesh.perFaceTriangleVerticesFLAT[triBase + 1];
        int t2 = mesh.perFaceTriangleVerticesFLAT[triBase + 2];

        float hitBary3 = (1 - hitBary1 - hitBary2);
        float x = mesh.mayaRawPoints[t0 * 3] * hitBary1 + mesh.mayaRawPoints[t1 * 3] * hitBary2 +
                  mesh.mayaRawPoints[t2 * 3] * hitBary3;
        float y = mesh.mayaRawPoints[t0 * 3 + 1] * hitBary1 +
                  mesh.mayaRawPoints[t1 * 3 + 1] * hitBary2 +
                  mesh.mayaRawPoints[t2 * 3 + 1] * hitBary3;
        float z = mesh.mayaRawPoints[t0 * 3 + 2] * hitBary1 +
                  mesh.mayaRawPoints[t1 * 3 + 2] * hitBary2 +
                  mesh.mayaRawPoints[t2 * 3 + 2] * hitBary3;
        hitPoint = MFloatPoint(x, y, z) * mesh.inclusiveMatrix;
    }
    else {
        MPoint mirrorPoint = MPoint(mdata.centerOfBrush) * mirrorMatrix;
        stat = start.intersector.getClosestPoint(mirrorPoint, pointInfo, ui.mirrorMinDist);
        if (MS::kSuccess != stat) {
            return false;
        }

        faceHit = pointInfo.faceIndex();
        hitPoint = MFloatPoint(mirrorPoint);
    }
    return true;
}

bool computeHit(
    short screenPixelX, short screenPixelY, bool getNormal, M3dView &view, const UserInputData &ui,
    MeshState &mesh, InteractionPerFrameData &frame, int &faceHit, MFloatPoint &hitPoint
)
{
    MStatus stat;

    view.viewToWorld(screenPixelX, screenPixelY, frame.worldPoint, frame.worldVector);

    // float hitRayParam;
    float hitBary1;
    float hitBary2;
    int hitTriangle;
    // If v1, v2, and v3 vertices of that triangle,
    // then the barycentric coordinates are such that
    // hitPoint = (*hitBary1)*v1 + (*hitBary2)*v2 + (1 - *hitBary1 - *hitBary2)*v3;
    // If no hit was found, the referenced value will not be modified,

    bool foundIntersect = mesh.meshFn.closestIntersection(
        frame.worldPoint, frame.worldVector, nullptr, nullptr, false, MSpace::kWorld, 9999, false,
        &mesh.accelParams, hitPoint, &frame.pressDistance, &faceHit, &hitTriangle, &hitBary1,
        &hitBary2, 0.0001f, &stat
    );

    if (!foundIntersect) {
        return false;
    }

    if (ui.paintMirror > 0 && ui.paintMirror < 4) { // if we compute the orig
        int triBase = (mesh.perFaceTriangleStartINDEX[faceHit] + hitTriangle) * 3;
        int t0 = mesh.perFaceTriangleVerticesFLAT[triBase + 0];
        int t1 = mesh.perFaceTriangleVerticesFLAT[triBase + 1];
        int t2 = mesh.perFaceTriangleVerticesFLAT[triBase + 2];
        float hitBary3 = (1 - hitBary1 - hitBary2);
        float x = mesh.mayaOrigRawPoints[t0 * 3] * hitBary1 +
                  mesh.mayaOrigRawPoints[t1 * 3] * hitBary2 +
                  mesh.mayaOrigRawPoints[t2 * 3] * hitBary3;
        float y = mesh.mayaOrigRawPoints[t0 * 3 + 1] * hitBary1 +
                  mesh.mayaOrigRawPoints[t1 * 3 + 1] * hitBary2 +
                  mesh.mayaOrigRawPoints[t2 * 3 + 1] * hitBary3;
        float z = mesh.mayaOrigRawPoints[t0 * 3 + 2] * hitBary1 +
                  mesh.mayaOrigRawPoints[t1 * 3 + 2] * hitBary2 +
                  mesh.mayaOrigRawPoints[t2 * 3 + 2] * hitBary3;
        frame.origHitPoint = MFloatPoint(x, y, z);
    }

    // ----------- get normal for display ---------------------
    if (getNormal) {
        mesh.meshFn.getPolygonNormal(faceHit, frame.normalVector, MSpace::kWorld);
    }
    return true;
}

std::vector<int> getSurroundingVerticesPerFace(int vertexIndex, const MeshState &mesh)
{
    auto first = mesh.perFaceVerticesSetFLAT.begin() + mesh.perFaceVerticesSetINDEX[vertexIndex];
    auto last =
        mesh.perFaceVerticesSetFLAT.begin() + mesh.perFaceVerticesSetINDEX[vertexIndex + (int)1];
    std::vector<int> newVec(first, last);
    return newVec;
}

bool expandHit(
    int faceHit, const MFloatPoint &hitPoint, const UserInputData &ui, const MeshState &mesh,
    std::unordered_map<int, float> &dicVertsDist
)
{
    // ----------- compute the vertices around ---------------------
    std::vector<int> verticesSet = getSurroundingVerticesPerFace(faceHit, mesh);
    bool foundHit = false;
    for (int ptIndex : verticesSet) {
        MFloatPoint posPoint(
            mesh.mayaRawPoints[ptIndex * 3], mesh.mayaRawPoints[ptIndex * 3 + 1],
            mesh.mayaRawPoints[ptIndex * 3 + 2]
        );
        float dist = posPoint.distanceTo(hitPoint);
        if (dist <= ui.sizeVal) {
            foundHit = true;
            auto ret = dicVertsDist.insert(std::make_pair(ptIndex, dist));
            if (!ret.second) {
                ret.first->second = std::min(dist, ret.first->second);
            }
        }
    }
    return foundHit;
}

void addBrushShapeFallof(
    const UserInputData &ui, const InteractionPerFrameData &frame,
    std::unordered_map<int, float> &dicVertsDist
)
{
    double valueStrength = ui.strengthVal;
    if (frame.modifierNoneShiftControl == ModifierKeys::ControlShift ||
        ui.commandIndex == ModifierCommands::Smooth) {
        valueStrength = ui.smoothStrengthVal; // smooth always we use the smooth value different of
                                              // the regular value
    }

    if (ui.fractionOversamplingVal) {
        valueStrength /= ui.oversamplingVal;
    }

    for (auto &element : dicVertsDist) {
        float value = 1.0 - (element.second / ui.sizeVal);
        value = (float)getFalloffValue(ui.curveVal, value, valueStrength);

        element.second = value;
    }
}

void mergeMirrorArray(WeightData &weights, const MirrorableData &base, const MirrorableData &mirror)
{
    weights.mirroredJoinedArray.clear();
    for (const auto &elem : base.skinValuesToSet) {
        int theVert = elem.first;
        float theWeight = elem.second;
        std::pair<float, float> secondElem(theWeight, 0.0);
        std::pair<int, std::pair<float, float>> toAdd(theVert, secondElem);
        weights.mirroredJoinedArray.insert(toAdd);
    }

    for (const auto &elem : mirror.skinValuesToSet) {
        int theVert = elem.first;
        float theWeight = elem.second;
        std::pair<float, float> secondElem(0.0, theWeight);
        std::pair<int, std::pair<float, float>> toAdd(theVert, secondElem);
        auto ret = weights.mirroredJoinedArray.insert(toAdd);
        if (!ret.second) {
            std::pair<float, float> origSecondElem = ret.first->second;
            origSecondElem.second = theWeight;
            ret.first->second = origSecondElem;
        }
    }
}

static MStatus setAverageWeight(
    std::vector<int> &verticesAround, int currentVertex, int indexCurrVert, int nbJoints,
    MIntArray &lockJoints, MDoubleArray &fullWeightArray, MDoubleArray &theWeights,
    double strengthVal
)
{
    MStatus stat;
    int sizeVertices = verticesAround.size();
    unsigned int jnt, posi;

    MDoubleArray sumWeigths(nbJoints, 0.0);
    // compute sum weights
    for (int vertIndex : verticesAround) {
        for (jnt = 0; jnt < nbJoints; jnt++) {
            posi = vertIndex * nbJoints + jnt;
            sumWeigths[jnt] += fullWeightArray[posi];
        }
    }
    double totalBaseVtxLock = 0.0;
    double totalVtxUnlock = 0.0;

    for (jnt = 0; jnt < nbJoints; jnt++) {
        // get if jnt is locked
        bool isLockJnt = lockJoints[jnt] == 1;
        int posi = currentVertex * nbJoints + jnt;
        // get currentWeight of currentVtx
        double currentW = fullWeightArray[posi];

        sumWeigths[jnt] /= sizeVertices;
        sumWeigths[jnt] =
            strengthVal * sumWeigths[jnt] + (1.0 - strengthVal) * currentW; // add with strength
        double targetW = sumWeigths[jnt];

        // sum it all
        if (!isLockJnt) {
            totalVtxUnlock += targetW;
        }
        else {
            totalBaseVtxLock += currentW;
        }
    }
    // setting part ---------------
    double normalizedValueAvailable = 1.0 - totalBaseVtxLock;

    if (normalizedValueAvailable > 0.0 && totalVtxUnlock > 0.0) { // we have room to set weights
        double mult = normalizedValueAvailable / totalVtxUnlock;
        for (jnt = 0; jnt < nbJoints; jnt++) {
            bool isLockJnt = lockJoints[jnt] == 1;
            int posiToSet = indexCurrVert * nbJoints + jnt;
            int posi = currentVertex * nbJoints + jnt;

            double currentW = fullWeightArray[posi];
            double targetW = sumWeigths[jnt];

            if (isLockJnt) {
                theWeights[posiToSet] = currentW;
            }
            else {
                targetW *= mult; // normalement divide par 1, sauf cas lock joints
                theWeights[posiToSet] = targetW;
            }
        }
    }
    else { // normalize problem let's revert
        for (jnt = 0; jnt < nbJoints; jnt++) {
            int posiToSet = indexCurrVert * nbJoints + jnt;
            int posi = currentVertex * nbJoints + jnt;

            double currentW = fullWeightArray[posi];
            theWeights[posiToSet] = currentW; // set the base Weight
        }
    }
    return MS::kSuccess;
}

static MStatus editArray(
    ModifierCommands command, int influence, int nbJoints, MIntArray &lockJoints,
    MDoubleArray &fullWeightArray, std::map<int, double> &valuesToSet, MDoubleArray &theWeights,
    bool normalize, double mutliplier
)
{
    MStatus stat;
    // 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen - 6 LockVertices - 7
    // UnLockVertices
    //
    if (lockJoints.length() < nbJoints) {
        MGlobal::displayInfo(
            MString("-> editArray FAILED | nbJoints ") + nbJoints + MString(" | lockJoints ") +
            lockJoints.length()
        );
        return MStatus::kFailure;
    }
    if (command == ModifierCommands::Sharpen) {
        int i = 0;
        for (const auto &elem : valuesToSet) {
            int theVert = elem.first;
            double theVal = mutliplier * elem.second + 1.0;
            double substract = theVal / nbJoints;
            MDoubleArray producedWeigths(nbJoints, 0.0);
            double totalBaseVtxLock = 0.0;
            double totalVtxUnlock = 0.0;
            for (int j = 0; j < nbJoints; ++j) {
                // check the zero val ----------
                double currentW = fullWeightArray[theVert * nbJoints + j];
                double targetW = (currentW * theVal) - substract;
                targetW = std::max(0.0, std::min(targetW, 1.0)); // clamp
                producedWeigths.set(targetW, j);

                if (lockJoints[j] == 0) { // unlock
                    totalVtxUnlock += targetW;
                }
                else {
                    totalBaseVtxLock += currentW;
                }
            }
            // now normalize for lockJoints
            double normalizedValueAvailable = 1.0 - totalBaseVtxLock;
            if (normalizedValueAvailable > 0.0 &&
                totalVtxUnlock > 0.0) { // we have room to set weights
                double mult = normalizedValueAvailable / totalVtxUnlock;
                for (unsigned int j = 0; j < nbJoints; ++j) {
                    double currentW = fullWeightArray[theVert * nbJoints + j];
                    double targetW = producedWeigths[j];
                    if (lockJoints[j] == 0) { // unlock
                        targetW *= mult;      // normalement divide par 1, sauf cas lock joints
                        theWeights[i * nbJoints + j] = targetW;
                    }
                    else {
                        theWeights[i * nbJoints + j] = currentW;
                    }
                }
            }
            else {
                for (unsigned int j = 0; j < nbJoints; ++j) {
                    theWeights[i * nbJoints + j] = fullWeightArray[theVert * nbJoints + j];
                }
            }
            i++;
        }
    }
    else {
        // do the command --------------------------
        int i = -1; // i is a short index instead of theVert
        for (const auto &elem : valuesToSet) {
            i++;
            int theVert = elem.first;
            double theVal = mutliplier * elem.second;
            // get the sum of weights

            double sumUnlockWeights = 0.0;
            for (int jnt = 0; jnt < nbJoints; ++jnt) {
                int indexArray_theWeight = i * nbJoints + jnt;
                int indexArray_fullWeightArray = theVert * nbJoints + jnt;

                if (indexArray_theWeight > theWeights.length()) {
                    MGlobal::displayInfo(
                        MString(
                            "-> editArray FAILED | indexArray_theWeight  > theWeights.length()"
                        ) +
                        indexArray_theWeight + MString(" > ") + theWeights.length()
                    );
                    return MStatus::kFailure;
                }
                if (indexArray_fullWeightArray > fullWeightArray.length()) {
                    MGlobal::displayInfo(
                        MString(
                            "-> editArray FAILED | indexArray_fullWeightArray "
                            " > fullWeightArray.length()"
                        ) +
                        indexArray_fullWeightArray + MString(" > ") + fullWeightArray.length()
                    );
                    return MStatus::kFailure;
                }

                if (lockJoints[jnt] == 0) { // not locked
                    sumUnlockWeights += fullWeightArray[indexArray_fullWeightArray];
                }
                theWeights[indexArray_theWeight] =
                    fullWeightArray[indexArray_fullWeightArray]; // preset array
            }
            double currentW = fullWeightArray[theVert * nbJoints + influence];

            if (((command == ModifierCommands::Remove) ||
                 (command == ModifierCommands::Absolute)) &&
                (currentW > (sumUnlockWeights - .0001))) { // value is 1(max) we cant do anything
                continue;                                  // we pass to next vertex
            }

            double newW = currentW;
            if (command == ModifierCommands::Add) {
                newW += theVal;
            }
            else if (command == ModifierCommands::Remove) {
                newW -= theVal;
            }
            else if (command == ModifierCommands::AddPercent) {
                newW += theVal * newW;
            }
            else if (command == ModifierCommands::Absolute) {
                newW = theVal;
            }

            newW = std::max(0.0, std::min(newW, sumUnlockWeights)); // clamp

            double newRest = sumUnlockWeights - newW;
            double oldRest = sumUnlockWeights - currentW;
            double div = sumUnlockWeights;

            if (newRest != 0.0) {
                div = oldRest / newRest; // produit en croix
            }

            // do the locks !!
            double sum = 0.0;
            for (int jnt = 0; jnt < nbJoints; ++jnt) {
                if (lockJoints[jnt] == 1) {
                    continue;
                }
                // check the zero val ----------
                double weightValue = fullWeightArray[theVert * nbJoints + jnt];
                if (jnt == influence) {
                    weightValue = newW;
                }
                else {
                    if (newW == sumUnlockWeights) {
                        weightValue = 0.0;
                    }
                    else {
                        weightValue /= div;
                    }
                }
                if (normalize) {
                    weightValue = std::max(0.0, std::min(weightValue, sumUnlockWeights)); // clamp
                }
                sum += weightValue;
                theWeights[i * nbJoints + jnt] = weightValue;
            }

            if ((sum == 0) ||
                (sum <
                 0.5 * sumUnlockWeights)) { // zero problem revert weights ----------------------
                for (int jnt = 0; jnt < nbJoints; ++jnt) {
                    theWeights[i * nbJoints + jnt] = fullWeightArray[theVert * nbJoints + jnt];
                }
            }
            else if (normalize && (sum != sumUnlockWeights)) { // normalize ---------------
                for (int jnt = 0; jnt < nbJoints; ++jnt) {
                    if (lockJoints[jnt] == 0) {
                        theWeights[i * nbJoints + jnt] /= sum;              // to 1
                        theWeights[i * nbJoints + jnt] *= sumUnlockWeights; // to sum weights
                    }
                }
            }
        }
    }
    return stat;
}

static MStatus editArrayMirror(
    ModifierCommands command, int influence, int influenceMirror, int nbJoints,
    MIntArray &lockJoints, MDoubleArray &fullWeightArray,
    std::map<int, std::pair<float, float>> &valuesToSetMirror, MDoubleArray &theWeights,
    bool normalize, double mutliplier
)
{
    MStatus stat;
    // 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen - 6 LockVertices - 7
    // UnLockVertices
    //
    if (lockJoints.length() < nbJoints) {
        MGlobal::displayInfo(
            MString("-> editArrayMirror FAILED | nbJoints ") + nbJoints +
            MString(" | lockJoints ") + lockJoints.length()
        );
        return MStatus::kFailure;
    }
    if (command == ModifierCommands::Sharpen) {
        int i = 0;
        for (const auto &elem : valuesToSetMirror) {
            int theVert = elem.first;
            float valueBase = elem.second.first;
            float valueMirror = elem.second.second;

            float biggestValue = std::max(valueBase, valueMirror);

            double theVal = mutliplier * (double)biggestValue + 1.0;
            double substract = theVal / nbJoints;

            MDoubleArray producedWeigths(nbJoints, 0.0);
            double totalBaseVtxLock = 0.0;
            double totalVtxUnlock = 0.0;
            for (int j = 0; j < nbJoints; ++j) {
                double currentW = fullWeightArray[theVert * nbJoints + j];
                double targetW = (currentW * theVal) - substract;
                targetW = std::max(0.0, std::min(targetW, 1.0)); // clamp
                producedWeigths.set(targetW, j);
                if (lockJoints[j] == 0) { // unlock
                    totalVtxUnlock += targetW;
                }
                else {
                    totalBaseVtxLock += currentW;
                }
            }
            // now normalize
            double normalizedValueAvailable = 1.0 - totalBaseVtxLock;
            if (normalizedValueAvailable > 0.0 &&
                totalVtxUnlock > 0.0) { // we have room to set weights
                double mult = normalizedValueAvailable / totalVtxUnlock;
                for (unsigned int j = 0; j < nbJoints; ++j) {
                    double currentW = fullWeightArray[theVert * nbJoints + j];
                    double targetW = producedWeigths[j];
                    if (lockJoints[j] == 0) { // unlock
                        targetW *= mult;      // normalement divide par 1, sauf cas lock joints
                        theWeights[i * nbJoints + j] = targetW;
                    }
                    else {
                        theWeights[i * nbJoints + j] = currentW;
                    }
                }
            }
            else {
                for (unsigned int j = 0; j < nbJoints; ++j) {
                    theWeights[i * nbJoints + j] = fullWeightArray[theVert * nbJoints + j];
                }
            }
            i++;
        }
    }
    else {
        // do the other command --------------------------
        int i = -1; // i is a short index instead of theVert
        for (const auto &elem : valuesToSetMirror) {
            i++;
            int theVert = elem.first;
            double valueBase = mutliplier * (double)elem.second.first;
            double valueMirror = mutliplier * (double)elem.second.second;

            if (influenceMirror == influence) {
                valueBase = std::max(valueBase, valueMirror);
                valueMirror = 0.0;
            }

            double sumUnlockWeights = 0.0;
            for (int jnt = 0; jnt < nbJoints; ++jnt) {
                int indexArray_theWeight = i * nbJoints + jnt;
                int indexArray_fullWeightArray = theVert * nbJoints + jnt;
                if (lockJoints[jnt] == 0) { // not locked
                    sumUnlockWeights += fullWeightArray[indexArray_fullWeightArray];
                }
                theWeights[indexArray_theWeight] =
                    fullWeightArray[indexArray_fullWeightArray]; // preset array
            }

            double currentW = fullWeightArray[theVert * nbJoints + influence];
            double currentWMirror = fullWeightArray[theVert * nbJoints + influenceMirror];
            // 1 Remove 3 Absolute
            double newW = currentW;
            double newWMirror = currentWMirror;
            double sumNewWs = newW + newWMirror;

            if (command == ModifierCommands::Add) {
                newW = std::min(1.0, newW + valueBase);
                newWMirror = std::min(1.0, newWMirror + valueMirror);
                sumNewWs = newW + newWMirror;

                if (sumNewWs > 1.0) {
                    newW /= sumNewWs;
                    newWMirror /= sumNewWs;
                }
            }
            else if (command == ModifierCommands::Remove) {
                newW = std::max(0.0, newW - valueBase);
                newWMirror = std::max(0.0, newWMirror - valueMirror);
            }
            else if (command == ModifierCommands::AddPercent) {
                newW += valueBase * newW;
                newW = std::min(1.0, newW);
                newWMirror += valueMirror * newWMirror;
                newWMirror = std::min(1.0, newWMirror);
                sumNewWs = newW + newWMirror;
                if (sumNewWs > 1.0) {
                    newW /= sumNewWs;
                    newWMirror /= sumNewWs;
                }
            }
            else if (command == ModifierCommands::Absolute) {
                newW = valueBase;
                newWMirror = valueMirror;
            }
            newW = std::min(newW, sumUnlockWeights);             // clamp to max sumUnlockWeights
            newWMirror = std::min(newWMirror, sumUnlockWeights); // clamp to max sumUnlockWeights

            double newRest = sumUnlockWeights - newW - newWMirror;
            double oldRest = sumUnlockWeights - currentW - currentWMirror;
            double div = sumUnlockWeights;

            if (newRest != 0.0) { // produit en croix
                div = oldRest / newRest;
            }
            // do the locks !!
            double sum = 0.0;
            for (int jnt = 0; jnt < nbJoints; ++jnt) {
                if (lockJoints[jnt] == 1) {
                    continue;
                }
                // check the zero val ----------
                double weightValue = fullWeightArray[theVert * nbJoints + jnt];
                if (jnt == influence) {
                    weightValue = newW;
                }
                else if (jnt == influenceMirror) {
                    weightValue = newWMirror;
                }
                else {
                    if ((newW + newWMirror) == sumUnlockWeights) {
                        weightValue = 0.0;
                    }
                    else {
                        weightValue /= div;
                    }
                }
                if (normalize) {
                    weightValue = std::max(0.0, std::min(weightValue, sumUnlockWeights)); // clamp
                }
                sum += weightValue;
                theWeights[i * nbJoints + jnt] = weightValue;
            }
            if ((sum == 0) || (sum < 0.5 * sumUnlockWeights)) { // zero problem revert weights
                for (int jnt = 0; jnt < nbJoints; ++jnt) {
                    theWeights[i * nbJoints + jnt] = fullWeightArray[theVert * nbJoints + jnt];
                }
            }
            else if (normalize && (sum != sumUnlockWeights)) { // normalize
                for (int jnt = 0; jnt < nbJoints; ++jnt) {
                    if (lockJoints[jnt] == 0) {
                        theWeights[i * nbJoints + jnt] /= sum;              // to 1
                        theWeights[i * nbJoints + jnt] *= sumUnlockWeights; // to sum weights
                    }
                }
            }
        }
    }
    return stat;
}

static MStatus transferPointNurbsToMesh(MFnMesh &msh, MFnNurbsSurface &nurbsFn)
{
    MStatus stat = MS::kSuccess;
    MPlug mshPnts = msh.findPlug("pnts", false, &stat);
    MPointArray allpts;

    bool VIsPeriodic_ = nurbsFn.formInV() == MFnNurbsSurface::kPeriodic;
    bool UIsPeriodic_ = nurbsFn.formInU() == MFnNurbsSurface::kPeriodic;
    if (VIsPeriodic_ || UIsPeriodic_) {
        int numCVsInV_ = nurbsFn.numCVsInV();
        int numCVsInU_ = nurbsFn.numCVsInU();
        int UDeg_ = nurbsFn.degreeU();
        int VDeg_ = nurbsFn.degreeV();
        if (VIsPeriodic_) {
            numCVsInV_ -= VDeg_;
        }
        if (UIsPeriodic_) {
            numCVsInU_ -= UDeg_;
        }
        for (int uIndex = 0; uIndex < numCVsInU_; uIndex++) {
            for (int vIndex = 0; vIndex < numCVsInV_; vIndex++) {
                MPoint pt;
                nurbsFn.getCV(uIndex, vIndex, pt);
                allpts.append(pt);
            }
        }
    }
    else {
        stat = nurbsFn.getCVs(allpts);
    }
    msh.setPoints(allpts);
    return stat;
}

MStatus refreshPointsNormals(MeshState &mesh, const WeightData &weights)
{
    MStatus status = MStatus::kSuccess;

    if (!weights.skinObj.isNull() && mesh.meshDag.isValid(&status)) {
        mesh.meshFn.freeCachedIntersectionAccelerator(); // yes ?
        mesh.mayaRawPoints = const_cast<float *>(mesh.meshFn.getRawPoints(&status));
        mesh.rawNormals = const_cast<float *>(mesh.meshFn.getRawNormals(&status));
        int rawNormalsLength = sizeof(mesh.rawNormals);

#pragma omp parallel for
        for (int vertexInd = 0; vertexInd < mesh.numVertices; vertexInd++) {
            int indNormal = mesh.verticesNormalsIndices[vertexInd];
            int rawIndNormal = indNormal * 3 + 2;
            if (rawIndNormal < rawNormalsLength) {
                MVector theNormal(
                    mesh.rawNormals[indNormal * 3], mesh.rawNormals[indNormal * 3 + 1],
                    mesh.rawNormals[indNormal * 3 + 2]
                );
                mesh.verticesNormals.set(theNormal, vertexInd);
            }
        }
    }
    return status;
}

MStatus applyCommand(
    int influence, std::unordered_map<int, float> &valuesToSet, bool doNormalize,
    const UserInputData &ui, InfluenceData &infl, WeightData &weights, MeshState &mesh,
    NurbsData &nurbs, InteractionPersistentData &persist, const InteractionPerFrameData &frame
)
{
    MStatus status;
    // we need to sort all of that one way or another ---------------- here it is ------
    std::map<int, double> valuesToSetOrdered(valuesToSet.begin(), valuesToSet.end());

    ModifierCommands theCommandIndex = getCommandIndexModifiers(ui, frame);

    double multiplier = 1.0;

    if ((theCommandIndex != ModifierCommands::LockVertices) &&
        (theCommandIndex != ModifierCommands::UnlockVertices)) {
        MDoubleArray theWeights((int)infl.nbJoints * valuesToSetOrdered.size(), 0.0);
        int repeatLimit = 1;
        if (theCommandIndex == ModifierCommands::Smooth ||
            theCommandIndex == ModifierCommands::Sharpen) {
            repeatLimit = ui.smoothRepeat;
        }

        for (int repeat = 0; repeat < repeatLimit; ++repeat) {
            if (theCommandIndex == ModifierCommands::Smooth) {
                int i = 0;
                for (const auto &elem : valuesToSetOrdered) {
                    int theVert = elem.first;
                    double theWeight = elem.second;
                    std::vector<int> vertsAround = getSurroundingVerticesPerVert(theVert, mesh);

                    status = setAverageWeight(
                        vertsAround, theVert, i, infl.nbJoints, infl.lockJoints,
                        weights.skinWeightList, theWeights, ui.smoothStrengthVal * theWeight
                    );
                    i++;
                }
            }
            else {
                if (ui.ignoreLockVal) {
                    status = editArray(
                        theCommandIndex, influence, infl.nbJoints, infl.ignoreLockJoints,
                        weights.skinWeightList, valuesToSetOrdered, theWeights, doNormalize,
                        multiplier
                    );
                }
                else {
                    if (infl.lockJoints[influence] == 1 &&
                        theCommandIndex != ModifierCommands::Sharpen) {
                        return status; //  if locked and it's not sharpen --> do nothing
                    }
                    status = editArray(
                        theCommandIndex, influence, infl.nbJoints, infl.lockJoints,
                        weights.skinWeightList, valuesToSetOrdered, theWeights, doNormalize,
                        multiplier
                    );
                }
                if (status == MStatus::kFailure) {
                    return status;
                }
            }
            // now set the weights -----------------------------------------------------
            // here we should normalize -----------------------------------------------------
            int i = 0;
            // int prevVert = -1;
            for (const auto &elem : valuesToSetOrdered) {
                int theVert = elem.first;
                for (int j = 0; j < infl.nbJoints; ++j) {
                    int ind_swl = theVert * infl.nbJoints + j;
                    if (ind_swl >= weights.skinWeightList.length()) {
                        weights.skinWeightList.setLength(ind_swl + 1);
                    }
                    double val = 0.0;
                    int ind_tw = i * infl.nbJoints + j;
                    if (ind_tw < theWeights.length()) {
                        val = theWeights[ind_tw];
                    }
                    weights.skinWeightList[ind_swl] = val;
                }
                i++;
            }
        }
        MIntArray objVertices;
        for (const auto &elem : valuesToSetOrdered) {
            int theVert = elem.first;
            objVertices.append(theVert);
        }

        MFnSingleIndexedComponent compFn;
        MObject weightsObj = compFn.create(MFn::kMeshVertComponent);
        compFn.addElements(objVertices);

        // Set the new weights.
        // Initialize the skin cluster.
        MFnSkinCluster skinFn(weights.skinObj, &status);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        persist.skinWeightsForUndo.clear();
        if (!frame.isNurbs) {
            skinFn.setWeights(
                mesh.meshDag, weightsObj, infl.influenceIndices, theWeights, doNormalize,
                &persist.skinWeightsForUndo
            );
        }
        else {
            MFnDoubleIndexedComponent doubleFn;
            MObject weightsObjNurbs = doubleFn.create(MFn::kSurfaceCVComponent);
            int uVal, vVal;
            for (int vert : objVertices) {

                vVal = (int)vert % (int)nurbs.numCVsInV_;
                uVal = (int)vert / (int)nurbs.numCVsInV_;
                doubleFn.addElement(uVal, vVal);
            }
            skinFn.setWeights(
                nurbs.nurbsDag, weightsObjNurbs, infl.influenceIndices, theWeights, doNormalize,
                &persist.skinWeightsForUndo
            );
            transferPointNurbsToMesh(mesh.meshFn, nurbs.nurbsFn); // we transfer the points postions
        }
        // in do press common
        // update values ---------------
        refreshPointsNormals(mesh, weights);
    }
    return status;
}

void preparePaint(
    bool postSetting, bool mirror, std::unordered_map<int, float> &dicVertsDist,
    const UserInputData &ui, InfluenceData &infl, WeightData &weights, MeshState &mesh,
    NurbsData &nurbs, InteractionPersistentData &persist, const InteractionPerFrameData &frame,
    MirrorableData &mdata
)
{
    // MGlobal::displayInfo("perform Paint");
    double multiplier = 1.0;
    ModifierCommands commandIndex = getCommandIndexModifiers(ui, frame);
    if (!postSetting && commandIndex != ModifierCommands::Smooth) {
        multiplier = .1; // less applying if dragging paint
    }

    bool isCommandLock = ((commandIndex == ModifierCommands::LockVertices) ||
                          (commandIndex == ModifierCommands::UnlockVertices)) &&
                         (frame.modifierNoneShiftControl != ModifierKeys::Control);

    auto endOfFind = mdata.previousPaint.end();
    for (const auto &element : dicVertsDist) {
        int index = element.first;
        float value = element.second * multiplier;
        // check if need to set this color, we store in intensityValues to check if it's already at
        // 1 -------
        if ((weights.lockVertices[index] == 1 && !isCommandLock) ||
            mdata.intensityValuesOrig[index] == 1) {
            continue;
        }
        // get the correct value of paint by adding this value -----
        value += mdata.intensityValuesOrig[index];
        auto res = mdata.previousPaint.find(index);
        if (res != endOfFind) { // we substract the smallest
            value -= std::min(res->second, element.second);
        }
        value = std::min(value, (float)1.0);
        mdata.intensityValuesOrig[index] = value;

        // add to array of values to set at the end---------------
        // we need to check if it is in the regular array and make adjustements
        auto ret = mdata.skinValuesToSet.insert(std::make_pair(index, value));
        if (!ret.second) {
            ret.first->second = std::max(value, ret.first->second);
        }
        else {
            persist.verticesPainted.insert(index);
        }
        // end add to array of values to set at the end--------------------
    }
    mdata.previousPaint = dicVertsDist;

    if (!postSetting) {
        // MGlobal::displayInfo("apply the skin stuff");
        // still have to deal with the colors damn it
        if (mdata.skinValuesToSet.size() > 0) {
            int theInfluence = ui.influenceIndex;
            if (mirror) {
                theInfluence = ui.mirrorInfluences[ui.influenceIndex];
            }
            applyCommand(
                theInfluence, mdata.skinValuesToSet,
                true, // doNormalize: hard-coded per UserInputData comment
                ui, infl, weights, mesh, nurbs, persist, frame
            );

            mdata.intensityValuesOrig = std::vector<float>(mesh.numVertices, 0);
            mdata.previousPaint.clear();
            mdata.skinValuesToSet.clear();
        }
    }
}

MStatus refreshColors(
    MIntArray &editVertsIndices, MColorArray &multiEditColors, MColorArray &soloEditColors,
    const UserInputData &ui, const InfluenceData &infl, WeightData &weights
)
{
    MStatus status = MS::kSuccess;
    if (multiEditColors.length() != editVertsIndices.length()) {
        multiEditColors.setLength(editVertsIndices.length());
    }
    if (soloEditColors.length() != editVertsIndices.length()) {
        soloEditColors.setLength(editVertsIndices.length());
    }

    for (unsigned int i = 0; i < editVertsIndices.length(); ++i) {
        int theVert = editVertsIndices[i];
        MColor multiColor, soloColor;
        bool isVtxLocked = weights.lockVertices[theVert] == 1;

        for (int j = 0; j < infl.nbJoints; ++j) { // for each joint
            int ind_swl = theVert * infl.nbJoints + j;
            if (ind_swl < weights.skinWeightList.length()) {
                double val = weights.skinWeightList[ind_swl];
                if (infl.lockJoints[j] == 1) {
                    multiColor += infl.lockJntColor * val;
                }
                else {
                    multiColor += infl.jointsColors[j] * val;
                }
                if (j == ui.influenceIndex) {
                    weights.soloColorsValues[theVert] = val;
                    soloColor = getASoloColor(val, ui, infl);
                }
            }
        }
        weights.multiCurrentColors[theVert] = multiColor;
        weights.soloCurrentColors[theVert] = soloColor;
        if (isVtxLocked) {
            multiEditColors[i] = weights.lockVertColor;
            soloEditColors[i] = weights.lockVertColor;
        }
        else {
            multiEditColors[i] = multiColor;
            soloEditColors[i] = soloColor;
        }
    }
    return status;
}

MStatus applyCommandMirror(
    bool doNormalize, const UserInputData &ui, InfluenceData &infl, WeightData &weights,
    MeshState &mesh, NurbsData &nurbs, InteractionPersistentData &persist,
    const InteractionPerFrameData &frame
)
{
    MStatus status;
    MGlobal::displayInfo(MString("applyCommandMirror "));
    std::map<int, std::pair<float, float>> mirroredJoinedArrayOrdered(
        weights.mirroredJoinedArray.begin(), weights.mirroredJoinedArray.end()
    );
    ModifierCommands theCommandIndex = getCommandIndexModifiers(ui, frame);

    double multiplier = 1.0;

    int influence = ui.influenceIndex;
    int influenceMirror = ui.mirrorInfluences[ui.influenceIndex];

    if ((theCommandIndex == ModifierCommands::LockVertices) ||
        (theCommandIndex == ModifierCommands::UnlockVertices)) {
        return MStatus::kSuccess;
    }

    MDoubleArray theWeights((int)infl.nbJoints * mirroredJoinedArrayOrdered.size(), 0.0);
    int repeatLimit = 1;
    if (theCommandIndex == ModifierCommands::Smooth ||
        theCommandIndex == ModifierCommands::Sharpen) {
        repeatLimit = ui.smoothRepeat;
    }

    MIntArray objVertices;
    for (int repeat = 0; repeat < repeatLimit; ++repeat) {
        if (theCommandIndex == ModifierCommands::Smooth) {
            int indexCurrVert = 0;
            for (const auto &elem : mirroredJoinedArrayOrdered) {
                int theVert = elem.first;
                if (repeat == 0) {
                    objVertices.append(theVert);
                }
                float valueBase = elem.second.first;
                float valueMirror = elem.second.second;
                float biggestValue = std::max(valueBase, valueMirror);

                double theWeight = (double)biggestValue;
                std::vector<int> vertsAround = getSurroundingVerticesPerVert(theVert, mesh);

                status = setAverageWeight(
                    vertsAround, theVert, indexCurrVert, infl.nbJoints, infl.lockJoints,
                    weights.skinWeightList, theWeights, ui.smoothStrengthVal * theWeight
                );
                indexCurrVert++;
            }
        }
        else {
            if (ui.ignoreLockVal) {
                status = editArrayMirror(
                    theCommandIndex, influence, influenceMirror, infl.nbJoints,
                    infl.ignoreLockJoints, weights.skinWeightList, mirroredJoinedArrayOrdered,
                    theWeights, doNormalize, multiplier
                );
            }
            else {
                if (infl.lockJoints[influence] == 1 &&
                    theCommandIndex != ModifierCommands::Sharpen) {
                    return status; //  if locked and it's not sharpen --> do nothing
                }
                status = editArrayMirror(
                    theCommandIndex, influence, influenceMirror, infl.nbJoints, infl.lockJoints,
                    weights.skinWeightList, mirroredJoinedArrayOrdered, theWeights, doNormalize,
                    multiplier
                );
            }
        }
        if (status == MStatus::kFailure) {
            return status;
        }
        // here we should normalize -----------------------------------------------------
        int i = 0;
        for (const auto &elem : mirroredJoinedArrayOrdered) {
            int theVert = elem.first;
            if (repeat == 0) {
                objVertices.append(theVert);
            }

            for (int j = 0; j < infl.nbJoints; ++j) {
                int ind_swl = theVert * infl.nbJoints + j;
                if (ind_swl >= weights.skinWeightList.length()) {
                    weights.skinWeightList.setLength(ind_swl + 1);
                }
                double val = 0.0;
                int ind_tw = i * infl.nbJoints + j;
                if (ind_tw < theWeights.length()) {
                    val = theWeights[ind_tw];
                }
                weights.skinWeightList[ind_swl] = val;
            }
            i++;
        }
    }

    MFnSingleIndexedComponent compFn;
    MObject weightsObj = compFn.create(MFn::kMeshVertComponent);
    compFn.addElements(objVertices);
    MFnSkinCluster skinFn(weights.skinObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    persist.skinWeightsForUndo.clear();
    if (!frame.isNurbs) {
        skinFn.setWeights(
            mesh.meshDag, weightsObj, infl.influenceIndices, theWeights, doNormalize,
            &persist.skinWeightsForUndo
        );
    }
    else {
        MFnDoubleIndexedComponent doubleFn;
        MObject weightsObjNurbs = doubleFn.create(MFn::kSurfaceCVComponent);
        int uVal, vVal;
        for (int vert : objVertices) {
            vVal = (int)vert % (int)nurbs.numCVsInV_;
            uVal = (int)vert / (int)nurbs.numCVsInV_;
            doubleFn.addElement(uVal, vVal);
        }
        skinFn.setWeights(
            nurbs.nurbsDag, weightsObjNurbs, infl.influenceIndices, theWeights, doNormalize,
            &persist.skinWeightsForUndo
        );
        transferPointNurbsToMesh(mesh.meshFn, nurbs.nurbsFn); // we transfer the points postions
        mesh.meshFn.updateSurface();
    }
    refreshPointsNormals(mesh, weights);
    return status;
}

static void
lineC(short x0, short y0, short x1, short y1, std::vector<std::pair<short, short>> &posi)
{
    short dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    short dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    short err = (dx > dy ? dx : -dy) / 2, e2;

    for (;;) {
        // setPixel(x0, y0);
        posi.push_back(std::make_pair(x0, y0));

        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}

MString fullColorSet = MString("multiColorsSet");
MString soloColorSet = MString("soloColorsSet");
MString fullColorSet2 = MString("multiColorsSet2");
MString soloColorSet2 = MString("soloColorsSet2");

void maya2019RefreshColors(
    bool toggle, bool &toggleColorState, const UserInputData &ui, MeshState &mesh
)
{
    mesh.meshFn.updateSurface();
    // first swap
    if (toggle) {
        toggleColorState = !toggleColorState;
    }

    if (!toggle || toggleColorState) {
        if (ui.soloColorVal == 1) {
            mesh.meshFn.setCurrentColorSetName(soloColorSet2);
        }
        else {
            mesh.meshFn.setCurrentColorSetName(fullColorSet2);
        }
        M3dView::active3dView().refresh(false, true);
    }
    if (!toggle || !toggleColorState) {
        if (ui.soloColorVal == 1) {
            mesh.meshFn.setCurrentColorSetName(soloColorSet);
        }
        else {
            mesh.meshFn.setCurrentColorSetName(fullColorSet);
        }
        M3dView::active3dView().refresh(false, true);
    }
}

MStatus doPerformPaint(
    bool postSetting, bool &toggleColorState, const UserInputData &ui, const InfluenceData &infl,
    WeightData &weights, const InteractionPerFrameData &frame, MeshState &mesh
)
{
    MStatus status = MStatus::kSuccess;

    MColorArray multiEditColors, soloEditColors;
    MIntArray editVertsIndices;

    ModifierCommands commandIndex = getCommandIndexModifiers(ui, frame);

    for (const auto &pt : weights.mirroredJoinedArray) {
        int ptIndex = pt.first;
        float weightBase = pt.second.first;
        float weightMirror = pt.second.second;
        MColor multColor, soloColor;
        getColorWithMirror(
            ptIndex, weightBase, weightMirror, multColor, soloColor, ui, infl, weights, frame
        );

        editVertsIndices.append(ptIndex);
        multiEditColors.append(multColor);
        soloEditColors.append(soloColor);
    }

    // do actually set colors -----------------------------------
    if (ui.soloColorVal == 0) {
        mesh.meshFn.setSomeColors(editVertsIndices, multiEditColors, &fullColorSet);
        mesh.meshFn.setSomeColors(editVertsIndices, multiEditColors, &fullColorSet2);
    }
    else {
        mesh.meshFn.setSomeColors(editVertsIndices, soloEditColors, &soloColorSet);
        mesh.meshFn.setSomeColors(editVertsIndices, soloEditColors, &soloColorSet2);
    }

    if (ui.useColorSetsWhilePainting || !postSetting) {
        if ((commandIndex == ModifierCommands::LockVertices) ||
            (commandIndex == ModifierCommands::UnlockVertices)) {
            // without that it doesn't refresh because mesh is not invalidated, meaning the
            // skinCluster hasn't changed
            mesh.meshFn.updateSurface();
        }
        maya2019RefreshColors(true, toggleColorState, ui, mesh);
    }
    return status;
}

MStatus doDragCommon(
    bool &toggleColorState, const UserInputData &ui, InfluenceData &infl, WeightData &weights,
    MeshState &mesh, NurbsData &nurbs, InteractionPersistentData &persist,
    InteractionPerFrameData &frame, const InteractionStartData &start, MirrorableData &base,
    MirrorableData &mirror, const MEvent &event
)
{
    MStatus status = MStatus::kSuccess;

    // -----------------------------------------------------------------
    // Dragging with the left mouse button performs the painting.
    // -----------------------------------------------------------------
    if (event.mouseButton() == MEvent::kLeftMouse) {
        // from previous hit get a line----------
        short previousX = frame.screenX;
        short previousY = frame.screenY;
        event.getPosition(frame.screenX, frame.screenY);

        // dictionnary of visited vertices and distances --- prefill it with the previous hit ---
        std::unordered_map<int, float> dicVertsDistToGrow = base.dicVertsDistSTART;
        std::unordered_map<int, float> dicVertsDistToGrowMirror = mirror.dicVertsDistSTART;

        // for linear growth ----------------------------------
        MFloatPointArray lineHitPoints, lineHitPointsMirror;
        lineHitPoints.append(base.inMatrixHit);
        if (ui.paintMirror != 0 && mirror.successfullHit) { // if mirror is not OFf
            lineHitPointsMirror.append(mirror.inMatrixHit);
        }
        // --------- LINE OF PIXELS --------------------
        std::vector<std::pair<short, short>> line2dOfPixels;
        // get pixels of the line of pixels
        lineC(previousX, previousY, frame.screenX, frame.screenY, line2dOfPixels);
        int nbPixelsOfLine = (int)line2dOfPixels.size();

        MFloatPoint hitPoint, hitMirrorPoint;
        MFloatPoint hitPointIM, hitMirrorPointIM;
        int faceHit, faceMirrorHit;

        M3dView view = M3dView::active3dView();
        bool successFullHit2 = computeHit(
            frame.screenX, frame.screenY, ui.drawBrushVal, view, ui, mesh, frame, faceHit, hitPoint
        );

        bool successFullMirrorHit2 = false;
        if (successFullHit2) {
            // stored in start dic for next call of drag function
            persist.previousfaceHit = faceHit;
            base.dicVertsDistSTART.clear();
            hitPointIM = hitPoint * mesh.inclusiveMatrixInverse;
            expandHit(faceHit, hitPointIM, ui, mesh, base.dicVertsDistSTART);

            // If the mirror happens -------------------------
            if (ui.paintMirror != 0) { // if mirror is not OFf
                successFullMirrorHit2 =
                    getMirrorHit(ui, mesh, start, frame, base, faceMirrorHit, hitMirrorPoint);

                if (successFullMirrorHit2) {
                    hitMirrorPointIM = hitMirrorPoint * mesh.inclusiveMatrixInverse;
                    expandHit(faceMirrorHit, hitMirrorPointIM, ui, mesh, mirror.dicVertsDistSTART);
                }
            }
        }
        if (!base.successFullDragHit && !successFullHit2) { // moving in empty zone
            return MStatus::kNotFound;
        }
        //////////////////////////////////////////////////////////////////////////////
        base.successFullDragHit = successFullHit2;
        mirror.successFullDragHit = successFullMirrorHit2;

        if (base.successFullDragHit) {
            base.centerOfBrush = hitPoint;
            base.inMatrixHit = hitPointIM;
            if (ui.paintMirror != 0 && mirror.successFullDragHit) {
                mirror.centerOfBrush = hitMirrorPoint;
                mirror.inMatrixHit = hitMirrorPointIM;
            }
        }
        int incrementValue = 1;
        if (incrementValue < nbPixelsOfLine) {
            for (int i = incrementValue; i < nbPixelsOfLine; i += incrementValue) {
                auto myPair = line2dOfPixels[i];
                short x = myPair.first;
                short y = myPair.second;

                bool successFullHit2 =
                    computeHit(x, y, false, view, ui, mesh, frame, faceHit, hitPoint);
                if (successFullHit2) {
                    hitPointIM = hitPoint * mesh.inclusiveMatrixInverse;
                    lineHitPoints.append(hitPointIM);
                    successFullHit2 = expandHit(faceHit, hitPointIM, ui, mesh, dicVertsDistToGrow);
                    // mirror part -------------------
                    if (ui.paintMirror != 0) { // if mirror is not OFf
                        successFullMirrorHit2 = getMirrorHit(
                            ui, mesh, start, frame, base, faceMirrorHit, hitMirrorPoint
                        );

                        if (successFullMirrorHit2) {
                            hitMirrorPointIM = hitMirrorPoint * mesh.inclusiveMatrixInverse;
                            lineHitPointsMirror.append(hitMirrorPointIM);
                            expandHit(
                                faceMirrorHit, hitMirrorPointIM, ui, mesh, dicVertsDistToGrowMirror
                            );
                        }
                    }
                }
            }
        }
        // only now add last hit -------------------------
        if (base.successFullDragHit) {
            lineHitPoints.append(base.inMatrixHit);
            expandHit(
                faceHit, base.inMatrixHit, ui, mesh, dicVertsDistToGrow
            );                                                      // to get closest hit
            if (ui.paintMirror != 0 && mirror.successFullDragHit) { // if mirror is not OFf
                lineHitPointsMirror.append(mirror.inMatrixHit);
                expandHit(faceMirrorHit, mirror.inMatrixHit, ui, mesh, dicVertsDistToGrowMirror);
            }
        }

        frame.modifierNoneShiftControl = ModifierKeys::NoModifier;
        if (event.isModifierShift()) {
            if (event.isModifierControl()) {
                frame.modifierNoneShiftControl = ModifierKeys::ControlShift;
            }
            else {
                frame.modifierNoneShiftControl = ModifierKeys::Shift;
            }
        }
        else if (event.isModifierControl()) {
            frame.modifierNoneShiftControl = ModifierKeys::Control;
        }

        // let's expand these arrays to the outer part of the brush----------------
        for (auto hp : lineHitPoints) {
            base.AllHitPoints.append(hp);
        }
        for (auto hp : lineHitPointsMirror) {
            mirror.AllHitPoints.append(hp);
        }

        growArrayOfHitsFromCenters(true, ui, mesh, frame, lineHitPoints, dicVertsDistToGrow);
        addBrushShapeFallof(ui, frame, dicVertsDistToGrow);
        preparePaint(
            ui.postSetting, false, dicVertsDistToGrow, ui, infl, weights, mesh, nurbs, persist,
            frame, base
        );

        if (ui.paintMirror != 0) { // mirror
            growArrayOfHitsFromCenters(
                true, ui, mesh, frame, lineHitPointsMirror, dicVertsDistToGrowMirror
            );
            addBrushShapeFallof(ui, frame, dicVertsDistToGrowMirror);
            preparePaint(
                ui.postSetting, true, dicVertsDistToGrowMirror, ui, infl, weights, mesh, nurbs,
                persist, frame, mirror
            );
        }
        mergeMirrorArray(weights, base, mirror);

        if (ui.useColorSetsWhilePainting || !ui.postSetting) {
            doPerformPaint(ui.postSetting, toggleColorState, ui, infl, weights, frame, mesh);
        }
        persist.performBrush = true;
    }
    // -----------------------------------------------------------------
    // Dragging with the middle mouse button adjusts the settings.
    // -----------------------------------------------------------------
    else if (event.mouseButton() == MEvent::kMiddleMouse) {
        // Skip several evaluation steps. This has several reasons:
        // - It reduces the smoothing strength because not every evaluation
        //   triggers a calculation.
        // - It lets adjusting the brush appear smoother because the lines
        //   show less flicker.
        // - It also improves the differentiation between horizontal and
        //   vertical dragging when adjusting.
        persist.undersamplingSteps++;
        if (persist.undersamplingSteps < ui.undersamplingVal) {
            return status;
        }
        persist.undersamplingSteps = 0;

        // get screen position
        event.getPosition(frame.screenX, frame.screenY);
        // Get the current and initial cursor position and calculate the
        // delta movement from them.
        MPoint currentPos(frame.screenX, frame.screenY);
        MPoint startPos(start.startScreenX, start.startScreenY);
        MVector deltaPos(currentPos - startPos);

        // Switch if the size should get adjusted or the strength based
        // on the drag direction. A drag along the x axis defines size
        // and a drag along the y axis defines strength.
        // InitAdjust makes sure that direction gets set on the first
        // drag event and gets reset the next time a mouse button is
        // pressed.
        if (!persist.initAdjust) {
            if (deltaPos.length() < 6) {
                return status; // only if we move at least 6 pixels do we know the direction to
                               // pick !
            }
            persist.sizeAdjust = (abs(deltaPos.x) > abs(deltaPos.y));
            persist.initAdjust = true;
        }
        // Define the settings for either setting the brush size or the
        // brush strength.
        MString message = "Brush Size";
        MString slider = "Size";
        double dragDistance = deltaPos.x;
        double min = 0.001;
        unsigned int max = 1000;
        double baseValue = ui.sizeVal;
        // The adjustment speed depends on the distance to the mesh.
        // Closer distances allows for a feiner control whereas larger
        // distances need a coarser control.
        double speed = pow(0.001 * frame.pressDistance, 0.9);

        // Vary the settings if the strength gets adjusted.
        if (!persist.sizeAdjust) {
            if (event.isModifierControl()) {
                message = "Smooth Strength";
                baseValue = ui.smoothStrengthVal;
            }
            else {
                message = "Brush Strength";
                baseValue = ui.strengthVal;
            }
            slider = "Strength";
            dragDistance = deltaPos.y;
            max = 1;
            speed *= 0.1; // smaller for the upd and down
        }
        double prevDist = 0.0;
        // The shift modifier scales the speed for a fine adjustment.
        if (event.isModifierShift()) {
            if (!frame.shiftMiddleDrag) {              // if we weren't in shift we reset
                persist.storedDistance = dragDistance; // store the pixels to remove
                frame.shiftMiddleDrag = true;
            }
            prevDist = persist.storedDistance * speed; // store the previsou drag done
            speed *= 0.1;
        }
        else {
            if (frame.shiftMiddleDrag) {
                persist.storedDistance = dragDistance;
                frame.shiftMiddleDrag = false;
            }
            prevDist = persist.storedDistance * speed; // store the previous drag done
        }
        dragDistance -= persist.storedDistance;

        // Calculate the new value by adding the drag distance to the
        // start value.
        double value = baseValue + prevDist + dragDistance * speed;

        // Clamp the values to the min/max range.
        if (value < min) {
            value = min;
        }
        else if (value > max) {
            value = max;
        }

        // Store the modified value for drawing and for setting the
        // values when releasing the mouse button.
        persist.adjustValue = value;

        // -------------------------------------------------------------
        // value display in the viewport
        // -------------------------------------------------------------
        short offsetX = start.startScreenX - start.viewCenterX;
        short offsetY = start.startScreenY - start.viewCenterY - 50;

        int precision = 2;
        if (event.isModifierShift()) {
            precision = 3;
        }

        std::string stdMessage = std::string(message.asChar());

        std::stringstream stream;
        stream << std::fixed << std::setprecision(precision) << persist.adjustValue;

        std::string theMessage = stdMessage + ": " + stream.str();
        std::string headsUpFmt = "headsUpMessage -horizontalOffset " + std::to_string(offsetX) +
                                 " -verticalOffset " + std::to_string(offsetY) + " -time 0.1 \"" +
                                 theMessage + "\"";
        MGlobal::executeCommand(MString(headsUpFmt.c_str(), headsUpFmt.length()));

        // Also, adjust the slider in the tool settings window if it's
        // currently open.
        if (persist.sizeAdjust) {
            MUserEventMessage::postUserEvent("brSkinBrush_updateDisplaySize");
        }
        else {
            MUserEventMessage::postUserEvent("brSkinBrush_updateDisplayStrength");
        }
    }
    return status;
}

MStatus editSoloColorSet(
    bool doBlack, const UserInputData &ui, const InfluenceData &infl, WeightData &weights,
    MeshState &mesh
)
{
    MStatus status;

    MColorArray colToSet;
    MIntArray vtxToSet;
    for (unsigned int theVert = 0; theVert < (unsigned int)mesh.numVertices; ++theVert) {
        double val = 0.0;
        int ind_swl = theVert * infl.nbJoints + ui.influenceIndex;
        if (ind_swl < weights.skinWeightList.length()) {
            val = weights.skinWeightList[ind_swl];
        }
        bool isVtxLocked = weights.lockVertices[theVert] == 1;
        bool update = doBlack || !(weights.soloColorsValues[theVert] == 0 && val == 0);
        if (update) { // dont update the black
            MColor soloColor = getASoloColor(val, ui, infl);
            weights.soloCurrentColors[theVert] = soloColor;
            weights.soloColorsValues[theVert] = val;
            if (isVtxLocked) {
                colToSet.append(weights.lockVertColor);
            }
            else {
                colToSet.append(soloColor);
            }
            vtxToSet.append(theVert);
        }
    }
    mesh.meshFn.setSomeColors(vtxToSet, colToSet, &soloColorSet);
    mesh.meshFn.setSomeColors(vtxToSet, colToSet, &soloColorSet2);

    return status;
}

void setInfluenceIndex(
    int value, bool selectInUI, UserInputData &ui, const InfluenceData &infl, WeightData &weights,
    MeshState &mesh, InteractionPersistentData &persist
)
{
    if (value != ui.influenceIndex) {
        if (value < infl.inflNames.length()) {
            ui.influenceIndex = value;
            persist.pickedInfluence = infl.inflNames[value];
            if (selectInUI) {
                MUserEventMessage::postUserEvent("brSkinBrush_pickedInfluence");
            }
        }

        if (ui.soloColorVal == 1) { // solo IF NOT IT CRASHES on a first pick before paint
            MString currentColorSet = mesh.meshFn.currentColorSetName(); // get current soloColor
            if (currentColorSet != soloColorSet) {
                mesh.meshFn.setCurrentColorSetName(soloColorSet);
            }
            editSoloColorSet(false, ui, infl, weights, mesh);
        }
        mesh.meshFn.updateSurface(); // for proper redraw hopefully
    }
}

MStatus doPressCommon(
    bool &toggleColorState, UserInputData &ui, const InfluenceData &infl, WeightData &weights,
    MeshState &mesh, InteractionPersistentData &persist, InteractionPerFrameData &frame,
    InteractionStartData &start, MirrorableData &base, MirrorableData &mirror, const MEvent &event
)
{
    MStatus status = MStatus::kSuccess;

    if (mesh.meshDag.node().isNull()) {
        return MStatus::kNotFound;
    }

    if (ui.pickMaxInfluenceVal || ui.pickInfluenceVal) {
        start.BBoxOfDeformers.clear();

        if (ui.pickMaxInfluenceVal && persist.biggestInfluence != -1) {
            MUserEventMessage::postUserEvent("brSkinBrush_influencesReordered");
        }

        if (persist.biggestInfluence != ui.influenceIndex && persist.biggestInfluence != -1) {
            setInfluenceIndex(persist.biggestInfluence, true, ui, infl, weights, mesh, persist);
            maya2019RefreshColors(true, toggleColorState, ui, mesh);
        }

        return MStatus::kNotFound;
    }

    // store for undo purposes --------------------------------------------------------------
    // only if painting not after
    if (!ui.postSetting || ui.paintMirror != 0) {
        persist.fullUndoSkinWeightList = MDoubleArray(weights.skinWeightList);
    }
    // update values ------------------------------------------------------------------------
    refreshPointsNormals(mesh, weights);

    // first reset attribute to paint values off if we're doing that ------------------------
    base.skinValuesToSet.clear();
    mirror.skinValuesToSet.clear();
    persist.verticesPainted.clear();

    // reset values ---------------------------------
    base.intensityValuesOrig = std::vector<float>(mesh.numVertices, 0);
    mirror.intensityValuesOrig = std::vector<float>(mesh.numVertices, 0);
    // initialize --
    persist.undersamplingSteps = 0;
    persist.performBrush = false;

    event.getPosition(frame.screenX, frame.screenY);

    // Get the size of the viewport and calculate the center for placing
    // the value messages when adjusting the brush settings.
    unsigned int x, y, width, height;
    M3dView::active3dView().viewport(x, y, width, height);
    start.viewCenterX = (short)width / 2;
    start.viewCenterY = (short)height / 2;

    // Store the initial mouse position. These get used when adjusting
    // the brush size and strength values.
    start.startScreenX = frame.screenX;
    start.startScreenY = frame.screenY;
    persist.storedDistance = 0.0; // for the drag screen middle click

    // Reset the adjustment from the previous drag.
    persist.initAdjust = false;
    persist.sizeAdjust = true;
    persist.adjustValue = 0.0;

    // -----------------------------------------------------------------
    // closest point on surface
    // -----------------------------------------------------------------
    // Getting the closest index cannot be performed when in flood mode.

    MStatus mbStat;
    if (event.mouseButton(&mbStat)) {
        // init at false
        base.successFullDragHit = false;
        mirror.successFullDragHit = false;
        base.dicVertsDistSTART.clear();
        weights.mirroredJoinedArray.clear();
        // TODO: these need full arg lists - view, accelParams, pressDistance,
        // perFaceTriangleVertices, mayaOrigRawPoints, origHitPoint, normalVector, sizeVal,
        // perFaceVerticesSetFLAT, perFaceVerticesSetINDEX, inclusiveMatrix, mirrorMinDist, etc.
        base.successfullHit = false;
        // base.successfullHit = computeHit(frame.screenX, frame.screenY, false,
        //     M3dView::active3dView(), ui, mesh, frame, persist.previousfaceHit,
        //     base.centerOfBrush);
        if (!base.successfullHit) {
            return MStatus::kNotFound;
        }
        base.AllHitPoints.clear();
        mirror.AllHitPoints.clear();

        // we put it inside our world matrix
        base.inMatrixHit = base.centerOfBrush * mesh.inclusiveMatrixInverse;
        base.successfullHit = false;
        // base.successfullHit = expandHit(persist.previousfaceHit, base.inMatrixHit, ui, mesh,
        // base.dicVertsDistSTART);

        // mirror part -------------------
        if (ui.paintMirror != 0) { // if mirror is not OFf
            mirror.dicVertsDistSTART.clear();
            int faceMirrorHit = 0;
            mirror.successfullHit = false;
            // mirror.successfullHit = getMirrorHit(ui, mesh, start, frame, base, faceMirrorHit,
            // mirror.centerOfBrush);
            MVector normalMirroredVector;
            mesh.meshFn.getPolygonNormal(faceMirrorHit, normalMirroredVector, MSpace::kWorld);

            mirror.inMatrixHit = mirror.centerOfBrush * mesh.inclusiveMatrixInverse;
            if (mirror.successfullHit) {
                // TODO: expandHit(faceMirrorHit, mirror.inMatrixHit, ui, mesh,
                // mirror.dicVertsDistSTART);
            }
        }
        // Store the initial surface point and view vector to use when
        // the brush settings are adjusted because the brush circle
        // needs to be static during the adjustment.
        start.surfacePointAdjust = base.centerOfBrush;
        start.worldVectorAdjust = frame.worldVector;
    }
    return status;
}

MStatus doReleaseCommon(
    bool &refreshDone, UserInputData &ui, const MeshState &mesh, InteractionPersistentData &persist,
    const MEvent &event
)
{
    // Don't continue if no mesh has been set.
    if (mesh.meshFn.object().isNull()) {
        return MS::kFailure;
    }
    refreshDone = false;
    // Define, which brush setting has been adjusted and needs to get
    // stored.
    if (event.mouseButton() == MEvent::kMiddleMouse && persist.initAdjust) {
        CHECK_MSTATUS_AND_RETURN_SILENT(persist.pressStatus);
        if (persist.sizeAdjust) {
            ui.sizeVal = persist.adjustValue;
        }
        else {
            if (event.isModifierControl()) {
                ui.smoothStrengthVal = persist.adjustValue;
            }
            else {
                ui.strengthVal = persist.adjustValue;
            }
        }
    }
    if (persist.performBrush) {
        // TODO: doTheAction() needs all its params threaded through here
        // doTheAction(ui, infl, weights, mesh, nurbs, persist, frame, base, mirror);
    }
    return MS::kSuccess;
}

static MStatus getListLockJoints(
    MObject &skinCluster, int nbJoints, MIntArray indicesForInfluenceObjects, MIntArray &jointsLocks
)
{
    MStatus stat;

    MFnDependencyNode skinClusterDep(skinCluster);
    MPlug influenceLock_plug = skinClusterDep.findPlug("lockWeights", false);

    int nbPlugs = influenceLock_plug.numElements();
    jointsLocks.clear();
    jointsLocks.setLength(nbJoints);
    for (int i = 0; i < nbJoints; ++i) {
        jointsLocks.set(0, i);
    }

    for (int i = 0; i < nbPlugs; ++i) {
        MPlug lockPlug = influenceLock_plug.elementByPhysicalIndex(i);
        int isLocked = 0;
        if (lockPlug.isConnected()) {
            MPlugArray connections;
            lockPlug.connectedTo(connections, true, false);
            if (connections.length() > 0) {
                MPlug theConn = connections[0];
                isLocked = theConn.asInt();
            }
        }
        else {
            isLocked = lockPlug.asInt();
        }
        int logicalInd = lockPlug.logicalIndex();
        logicalInd = indicesForInfluenceObjects[logicalInd];
        if (logicalInd < 0 || logicalInd >= nbJoints) {
            MGlobal::displayError(
                MString("CRASH i : ") + i + MString("logical Index: ") + lockPlug.logicalIndex() +
                MString(" | indicesForInfluenceObjects ") + logicalInd
            );
            continue;
        }
        jointsLocks.set(isLocked, logicalInd);
    }
    return stat;
}

void doTheAction(
    const UserInputData &ui, InfluenceData &infl, WeightData &weights, MeshState &mesh,
    NurbsData &nurbs, InteractionPersistentData &persist, const InteractionPerFrameData &frame,
    MirrorableData &base, MirrorableData &mirror
)
{
    // If the smoothing has been performed send the current values to
    // the tool command along with the necessary data for undo and redo.
    // The same goes for the select mode.
    MColorArray multiEditColors, soloEditColors;
    int nbVerticesPainted = (int)persist.verticesPainted.size();
    MIntArray editVertsIndices(nbVerticesPainted, 0);
    MIntArray undoLocks, redoLocks;

    MStatus status;
    if (infl.lockJoints.length() < infl.nbJoints) {
        MIntArray lockJointsCopy = infl.lockJoints;
        getListLockJoints(
            weights.skinObj, infl.nbJoints, infl.indicesForInfluenceObjects, lockJointsCopy
        );
    }
    MDoubleArray prevWeights((int)persist.verticesPainted.size() * infl.nbJoints, 0);

    std::vector<int> intArray;
    intArray.resize(persist.verticesPainted.size());

    int i = 0;
    for (const auto &theVert : persist.verticesPainted) {
        editVertsIndices[i] = theVert;
        i++;
    }

    ModifierCommands theCommandIndex = getCommandIndexModifiers(ui, frame);
    if ((theCommandIndex == ModifierCommands::LockVertices) ||
        (theCommandIndex == ModifierCommands::UnlockVertices)) {
        undoLocks.copy(weights.lockVertices);
        bool addLocks = theCommandIndex == ModifierCommands::LockVertices;
        // TODO: editLocks needs forward declaration (defined in functions.cpp)
        // editLocks(weights.skinObj, editVertsIndices, addLocks, weights.lockVertices);
        redoLocks.copy(weights.lockVertices);
    }
    else {
        if (ui.paintMirror != 0) {
            int mirrorInfluenceIndex = ui.mirrorInfluences[ui.influenceIndex];
            mergeMirrorArray(weights, base, mirror);

            if (mirrorInfluenceIndex != ui.influenceIndex) {
                status = applyCommandMirror(true, ui, infl, weights, mesh, nurbs, persist, frame);
            }
            else { // we merge in one array, it's easier
                for (const auto &element : mirror.skinValuesToSet) {
                    int index = element.first;
                    float value = element.second;

                    auto ret = base.skinValuesToSet.insert(std::make_pair(index, value));
                    if (!ret.second) {
                        ret.first->second = std::max(value, ret.first->second);
                    }
                }
                status = applyCommand(
                    ui.influenceIndex, base.skinValuesToSet, true, ui, infl, weights, mesh, nurbs,
                    persist, frame
                );
            }
        }
        else if (base.skinValuesToSet.size() > 0) {
            status = applyCommand(
                ui.influenceIndex, base.skinValuesToSet, true, ui, infl, weights, mesh, nurbs,
                persist, frame
            );
            if (status == MStatus::kFailure) {
                MGlobal::displayError(
                    MString("Something went wrong. EXIT the brush and RESTART it")
                );
                return;
            }
        }
        if (!ui.postSetting) { // only store if not constant setting
            int i = 0;
            for (const auto &theVert : persist.verticesPainted) {
                for (int j = 0; j < infl.nbJoints; ++j) {
                    prevWeights[i * infl.nbJoints + j] =
                        persist.fullUndoSkinWeightList[theVert * infl.nbJoints + j];
                }
                i++;
            }
        }
    }
    // TODO: refreshColors - needs editVertsIndices, multiEditColors, soloEditColors populated first
    // refreshColors(editVertsIndices, multiEditColors, soloEditColors, ui, infl, weights);
    mesh.meshFn.setSomeColors(editVertsIndices, multiEditColors, &fullColorSet);
    mesh.meshFn.setSomeColors(editVertsIndices, soloEditColors, &soloColorSet);

    mesh.meshFn.setSomeColors(editVertsIndices, multiEditColors, &fullColorSet2);
    mesh.meshFn.setSomeColors(editVertsIndices, soloEditColors, &soloColorSet2);
    if ((theCommandIndex == ModifierCommands::LockVertices) ||
        (theCommandIndex == ModifierCommands::UnlockVertices)) {
        // without that it doesn't refresh because mesh is not invalidated, meaning the skinCluster
        // hasn't changed
        mesh.meshFn.updateSurface();
    }
    base.skinValuesToSet.clear();
    mirror.skinValuesToSet.clear();
    base.previousPaint.clear();
    mirror.previousPaint.clear();

    if (!persist.firstPaintDone) {
        persist.firstPaintDone = true;
        MUserEventMessage::postUserEvent("brSkinBrush_cleanCloseUndo");
    }

    // TODO: maya2019RefreshColors needs toggleColorState threaded through here
    // maya2019RefreshColors(false, toggleColorState, ui, mesh);
    MUserEventMessage::postUserEvent("brSkinBrush_afterPaint");
}
