#include "functions.h"

#include <limits>
#include <math.h>
#include <vector>

#include "enums.h"

coord_t distance_sq(const point_t &a, const point_t &b)
{
    coord_t x = a[0] - b[0];
    coord_t y = a[1] - b[1];
    coord_t z = a[2] - b[2];
    return x * x + y * y + z * z;
}
coord_t distance(const point_t &a, const point_t &b)
{
    return std::sqrt(distance_sq(a, b));
}

unsigned int getMIntArrayIndex(const MIntArray &myArray, int searching)
{
    unsigned int toReturn = -1;
    for (unsigned int element = 0; element < myArray.length(); ++element) {
        if (myArray[element] == searching) {
            toReturn = element;
            break;
        }
    }
    return toReturn;
}

void CVsAround(
    int storedU, int storedV, int numCVsInU, int numCVsInV, bool UIsPeriodic, bool VIsPeriodic,
    MIntArray &vertices
)
{
    // Wrap an index into [0, count) if periodic. Returns -1 if it falls off a non-periodic edge
    auto wrap = [](int idx, int count, bool periodic) {
        if (idx >= 0 && idx < count) {
            return idx;
        }
        if (!periodic) {
            return -1;
        }
        return idx < 0 ? idx + count : idx - count;
    };
    auto addCV = [&](int u, int v) {
        if (u == -1 || v == -1) {
            return;
        }
        int resCV = numCVsInV * u + v;
        if (getMIntArrayIndex(vertices, resCV) == -1) {
            vertices.append(resCV);
        }
    };
    addCV(wrap(storedU + 1, numCVsInU, UIsPeriodic), storedV);
    addCV(wrap(storedU - 1, numCVsInU, UIsPeriodic), storedV);
    addCV(storedU, wrap(storedV + 1, numCVsInV, VIsPeriodic));
    addCV(storedU, wrap(storedV - 1, numCVsInV, VIsPeriodic));
}

MStatus transferPointNurbsToMesh(MFnMesh &msh, MFnNurbsSurface &nurbsFn)
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

MStatus findNurbsTesselate(MDagPath nurbsPath, MObject &meshObj, const char *plugName)
{
    MStatus stat;
    MFnDependencyNode depNode(nurbsPath.node());
    MPlug outMeshPlug = depNode.findPlug(plugName, false, &stat);
    if (stat == MS::kSuccess) {
        MPlugArray connections;
        outMeshPlug.connectedTo(connections, false, true);
        for (int i = 0; i < (int)connections.length(); ++i) {
            MFnDependencyNode sourceNode;
            sourceNode.setObject(connections[i].node());
            meshObj = sourceNode.object();
            return MS::kSuccess;
        }
    }
    return MS::kFailure;
}
// find a dag from name
MStatus getDagPath(MString nodeName, MDagPath &dagPath)
{
    MStatus status = MS::kSuccess;

    MSelectionList selList;
    status = MGlobal::getSelectionListByName(nodeName, selList);
    if (status != MStatus::kSuccess) {
        return status;
    }
    status = selList.getDagPath(0, dagPath);
    return status;
}

MStatus getMObject(MString nodeName, MObject &nodeObj)
{
    MStatus status = MS::kSuccess;

    MSelectionList selList;
    status = MGlobal::getSelectionListByName(nodeName, selList);
    if (status != MStatus::kSuccess) {
        return status;
    }
    status = selList.getDependNode(0, nodeObj);
    return status;
}

// from the mesh retrieves the skinCluster
MStatus findSkinCluster(MDagPath MeshPath, MObject &theSkinCluster, int indSkinCluster)
{
    MStatus stat;

    MFnDagNode dagNode(MeshPath); // path to the visible mesh
    // MFnMesh meshFn(MeshPath, &stat);     // this is the visible mesh
    MObject inObj;
    MObject dataObj1;

    MObjectArray listSkinClusters;
    // the deformed mesh comes into the visible mesh
    // through its "inmesh" plug
    MPlug inMeshPlug;
    if (MeshPath.apiType() == MFn::kMesh) {
        inMeshPlug = dagNode.findPlug("inMesh", false, &stat);
    }
    else if (MeshPath.apiType() == MFn::kNurbsSurface) {
        inMeshPlug = dagNode.findPlug("create", false, &stat);
    }

    if (stat == MS::kSuccess && inMeshPlug.isConnected()) {
        // walk the tree of stuff upstream from this plug
        MItDependencyGraph dgIt(
            inMeshPlug, MFn::kInvalid, MItDependencyGraph::kUpstream,
            MItDependencyGraph::kDepthFirst, MItDependencyGraph::kPlugLevel, &stat
        );
        if (MS::kSuccess == stat) {
            dgIt.disablePruningOnFilter();
            int count = 0;

            for (; !dgIt.isDone(); dgIt.next()) {
                MObject thisNode = dgIt.currentItem();
                // go until we find a skinCluster
                if (thisNode.apiType() == MFn::kSkinClusterFilter) {
                    listSkinClusters.append(thisNode);
                }
            }
        }
        int listSkinClustersLength = listSkinClusters.length();
        if (listSkinClustersLength > indSkinCluster) {
            theSkinCluster = listSkinClusters[indSkinCluster];
            MFnDependencyNode nodeFn(theSkinCluster);
            return MS::kSuccess;
        }
    }
    return MS::kFailure;
}

MStatus findMesh(MObject &skinCluster, MDagPath &theMeshPath)
{
    MFnSkinCluster theSkinCluster(skinCluster);
    MObjectArray objectsDeformed;
    theSkinCluster.getOutputGeometry(objectsDeformed);
    int objectsDeformedCount = objectsDeformed.length();
    bool doContinue = false;
    if (objectsDeformedCount != 0) {
        int j = 0;
        MDagPath::getAPathTo(objectsDeformed[j], theMeshPath);
        return MS::kSuccess;
    }
    return MS::kFailure;
}

MStatus findOrigMesh(MObject &skinCluster, MObject &origMesh)
{
    MFnSkinCluster theSkinCluster(skinCluster);
    MObjectArray objectsDeformed;
    theSkinCluster.getInputGeometry(objectsDeformed);
    if (objectsDeformed.length() == 0) {
        return MS::kFailure;
    }
    origMesh = objectsDeformed[0];
    return MS::kSuccess;
}

// Map a plug's logical influence index to the physical index used by our arrays.
// Returns -1 if the logical index has no influence object.
static int logicalToPhysical(const MIntArray &indicesForInfluenceObjects, unsigned int logicalInd)
{
    if (logicalInd >= indicesForInfluenceObjects.length()) {
        return -1;
    }
    return indicesForInfluenceObjects[logicalInd];
}

MStatus getListColorsJoints(
    MObject &skinCluster, int nbJoints, const MIntArray &indicesForInfluenceObjects,
    MColorArray &jointsColors
)
{
    MStatus stat = MS::kSuccess;

    // start
    jointsColors.clear();
    jointsColors.setLength(nbJoints);
    float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < nbJoints; ++i) {
        jointsColors.set(black, i);
    }

    //----------------------------------------------------------------
    MFnDependencyNode skinClusterDep(skinCluster);
    MPlug influenceColor_plug = skinClusterDep.findPlug("influenceColor", false, &stat);
    if (stat != MS::kSuccess) {
        MGlobal::displayError(MString("fail finding influenceColor plug "));
        return stat;
    }
    int nbElements = influenceColor_plug.numElements();

    for (int i = 0; i < nbElements; ++i) { // for each joint

        MPlug colorPlug = influenceColor_plug.elementByPhysicalIndex(i);
        int logicalInd = logicalToPhysical(indicesForInfluenceObjects, colorPlug.logicalIndex());
        if (logicalInd < 0 || logicalInd >= nbJoints) {
            MGlobal::displayError(
                MString("CRASH i : ") + i + MString("logical Index: ") + colorPlug.logicalIndex() +
                MString(" | indicesForInfluenceObjects ") + logicalInd
            );
            continue;
        }

        if (colorPlug.isConnected()) {
            MPlugArray connections;
            colorPlug.connectedTo(connections, true, false);
            if (connections.length() > 0) {
                MPlug theConn = connections[0];
                float element[4] = {
                    theConn.child(0).asFloat(), theConn.child(1).asFloat(),
                    theConn.child(2).asFloat(), 1
                };
                jointsColors.set(element, logicalInd);
            }
            else {
                jointsColors.set(black, logicalInd);
            }
        }
        else {
            float element[4] = {
                colorPlug.child(0).asFloat(), colorPlug.child(1).asFloat(),
                colorPlug.child(2).asFloat(), 1
            };
            jointsColors.set(element, logicalInd);
        }
    }
    return stat;
}

MStatus getListLockJoints(
    MObject &skinCluster, int nbJoints, const MIntArray &indicesForInfluenceObjects,
    MIntArray &jointsLocks
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
        int logicalInd = logicalToPhysical(indicesForInfluenceObjects, lockPlug.logicalIndex());
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

MStatus getListLockVertices(MObject &skinCluster, MIntArray &vertsLocks, MIntArray &lockedIndices)
{
    MStatus stat;

    MFnSkinCluster theSkinCluster(skinCluster);
    MObjectArray objectsDeformed;
    theSkinCluster.getOutputGeometry(objectsDeformed);
    MFnDependencyNode deformedNameMesh(objectsDeformed[0]);
    MPlug lockedVerticesPlug = deformedNameMesh.findPlug("lockedVertices", false, &stat);
    if (MS::kSuccess != stat) {
        MGlobal::displayInfo(MString("cant find lockerdVertices plug"));
        return stat;
    }

    MFnDependencyNode skinClusterDep(skinCluster);
    MPlug weight_list_plug = skinClusterDep.findPlug("weightList", false);

    int nbVertices = weight_list_plug.numElements();

    MObject Data;
    stat = lockedVerticesPlug.getValue(Data); // to get the attribute

    MFnIntArrayData intData(Data);
    MIntArray vertsLocksIndices = intData.array(&stat);
    vertsLocks = MIntArray(nbVertices, 0);
    for (unsigned int i = 0; i < vertsLocksIndices.length(); ++i) {
        int vtx = vertsLocksIndices[i];
        if (vtx < 0 || vtx >= nbVertices) {
            continue; // stale lock data from a previous topology
        }
        vertsLocks[vtx] = 1;
        lockedIndices.append(vtx);
    }
    return stat;
}

MStatus getSymetryAttributes(MObject &skinCluster, MIntArray &symetryList)
{
    MStatus stat;

    MFnSkinCluster theSkinCluster(skinCluster);
    MObjectArray objectsDeformed;
    theSkinCluster.getOutputGeometry(objectsDeformed);

    MFnDagNode deformedNameMesh(objectsDeformed[0]);
    MObject prt = deformedNameMesh.parent(0);

    MFnDependencyNode prtDep(prt);
    MPlug symVerticesPlug = prtDep.findPlug("symmetricVertices", false, &stat);
    if (MS::kSuccess != stat) {
        MGlobal::displayError(MString("cant find symmetricVertices plug"));
        return stat;
    }

    MObject Data;
    stat = symVerticesPlug.getValue(Data); // to get the attribute

    MFnIntArrayData intData(Data);
    symetryList = intData.array(&stat);
    return stat;
}

MStatus getMirrorVertices(
    const MIntArray &mirrorVertices, MIntArray &theEditVerts, MIntArray &theMirrorVerts,
    MIntArray &editAndMirrorVerts, MDoubleArray &editVertsWeights, MDoubleArray &mirrorVertsWeights,
    MDoubleArray &editAndMirrorWeights, bool doMerge
)
{
    // doMerge do we merge the weights ? if painting the same influence or smooth
    MStatus status;

    MIntArray vertExists(mirrorVertices.length(), -1);

    editAndMirrorVerts.copy(theEditVerts);
    editAndMirrorWeights.copy(editVertsWeights);
    if (!doMerge) { // mirror verts same length and weights
        mirrorVertsWeights.copy(editVertsWeights);
        theMirrorVerts.setLength(theEditVerts.length());
    }

    for (unsigned int i = 0; i < theEditVerts.length(); ++i) {
        vertExists[theEditVerts[i]] = i;
    }
    for (unsigned int i = 0; i < theEditVerts.length(); ++i) {
        int theVert = theEditVerts[i];
        int theMirroredVert = mirrorVertices[theVert];

        if (!doMerge) {
            theMirrorVerts[i] = theMirroredVert; // to
        }
        double theWeight = editVertsWeights[i];
        int indVertExists = vertExists[theMirroredVert];
        if (indVertExists == -1) { // not in first array
            if (doMerge) {
                theMirrorVerts.append(theMirroredVert);
                mirrorVertsWeights.append(theWeight);
            }
            editAndMirrorVerts.append(theMirroredVert);
            editAndMirrorWeights.append(theWeight);
        }
        else if (doMerge && theWeight < 1.0) { // clip weight at 1
            double prevWeight = editVertsWeights[indVertExists];
            if (theWeight > prevWeight) { // add the remaining if existing weight is less than this
                                          // new weight
                theMirrorVerts.append(theMirroredVert);
                mirrorVertsWeights.append(theWeight - prevWeight);
                editAndMirrorWeights[indVertExists] = theWeight; // edit weight
            }
        }
    }

    return status;
}

MStatus
editLocks(MObject &skinCluster, MIntArray &inputVertsToLock, bool addToLock, MIntArray &vertsLocks)
{
    MStatus stat;

    MFnSkinCluster theSkinCluster(skinCluster);
    MObjectArray objectsDeformed;
    theSkinCluster.getOutputGeometry(objectsDeformed);
    MFnDependencyNode deformedNameMesh(objectsDeformed[0]);
    MPlug lockedVerticesPlug = deformedNameMesh.findPlug("lockedVertices", false, &stat);
    if (MS::kSuccess != stat) {
        MGlobal::displayError(MString("cant find lockerdVertices plug"));
        return stat;
    }

    // now expand the array -----------------------
    int val = 0;
    if (addToLock) {
        val = 1;
    }
    for (unsigned int i = 0; i < inputVertsToLock.length(); ++i) {
        int vtx = inputVertsToLock[i];
        vertsLocks[vtx] = val;
    }
    MIntArray theArrayValues;
    for (unsigned int vtx = 0; vtx < vertsLocks.length(); ++vtx) {
        if (vertsLocks[vtx] == 1) {
            theArrayValues.append(vtx);
        }
    }
    // now set the value ---------------------------
    MFnIntArrayData tmpIntArray;
    auto tmpAttrSetter = tmpIntArray.create(theArrayValues);
    stat = lockedVerticesPlug.setValue(tmpAttrSetter); // to set the attribute
    return stat;
}

static void applyLockNormalize(
    int i, int theVert, int nbJoints, const MIntArray &lockJoints,
    const MDoubleArray &fullWeightArray, const MDoubleArray &producedWeights,
    MDoubleArray &theWeights, double totalBaseVtxLock, double totalVtxUnlock
)
{
    double available = 1.0 - totalBaseVtxLock;
    if (available > 0.0 && totalVtxUnlock > 0.0) {
        double mult = available / totalVtxUnlock;
        for (unsigned int j = 0; j < (unsigned)nbJoints; ++j) {
            double currentW = fullWeightArray[theVert * nbJoints + j];
            double targetW = producedWeights[j];
            if (lockJoints[j] == 0) {
                targetW *= mult;
                theWeights[i * nbJoints + j] = targetW;
            }
            else {
                theWeights[i * nbJoints + j] = currentW;
            }
        }
    }
    else {
        for (unsigned int j = 0; j < (unsigned)nbJoints; ++j) {
            theWeights[i * nbJoints + j] = fullWeightArray[theVert * nbJoints + j];
        }
    }
}

static void finalizeWeightRow(
    int i, int theVert, int nbJoints, const MIntArray &lockJoints,
    const MDoubleArray &fullWeightArray, MDoubleArray &theWeights, double sum,
    double sumUnlockWeights, bool normalize
)
{
    if (sum == 0 || sum < 0.5 * sumUnlockWeights) {
        for (int jnt = 0; jnt < nbJoints; ++jnt) {
            theWeights[i * nbJoints + jnt] = fullWeightArray[theVert * nbJoints + jnt];
        }
    }
    else if (normalize && sum != sumUnlockWeights) {
        for (int jnt = 0; jnt < nbJoints; ++jnt) {
            if (lockJoints[jnt] == 0) {
                theWeights[i * nbJoints + jnt] /= sum;
                theWeights[i * nbJoints + jnt] *= sumUnlockWeights;
            }
        }
    }
}

MStatus editArray(
    ModifierCommands command, int influence, int nbJoints, MIntArray &lockJoints,
    MDoubleArray &fullWeightArray, std::map<int, double> &valuesToSet, MDoubleArray &theWeights,
    bool normalize, double multiplier
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
            double theVal = multiplier * elem.second + 1.0;
            MDoubleArray producedWeigths(nbJoints, 0.0);
            double totalBaseVtxUnlock = 0.0, totalBaseVtxLock = 0.0;
            ;
            double totalVtxUnlock = 0.0, totalVtxLock = 0.0;
            for (int j = 0; j < nbJoints; ++j) {
                // check the zero val ----------
                double currentW = std::max(0.0, fullWeightArray[theVert * nbJoints + j]);
                double targetW = std::pow(currentW, theVal);
                targetW = std::max(0.0, std::min(1.0, targetW)); // clamp
                producedWeigths.set(targetW, j);

                if (lockJoints[j] == 0) { // unlock
                    totalBaseVtxUnlock += currentW;
                    totalVtxUnlock += targetW;
                }
                else {
                    totalBaseVtxLock += currentW;
                    totalVtxLock += targetW;
                }
            }
            applyLockNormalize(
                i, theVert, nbJoints, lockJoints, fullWeightArray, producedWeigths, theWeights,
                totalBaseVtxLock, totalVtxUnlock
            );
            i++;
        }
    }
    else {
        // do the command --------------------------
        int i = -1; // i is a short index instead of theVert
        for (const auto &elem : valuesToSet) {
            i++;
            int theVert = elem.first;
            double theVal = multiplier * elem.second;
            // get the sum of weights

            double sumUnlockWeights = 0.0;
            for (int jnt = 0; jnt < nbJoints; ++jnt) {
                int indexArray_theWeight = i * nbJoints + jnt;
                int indexArray_fullWeightArray = theVert * nbJoints + jnt;

                if (indexArray_theWeight >= (int)theWeights.length()) {
                    MGlobal::displayInfo(
                        MString(
                            "-> editArray FAILED | indexArray_theWeight >= theWeights.length()"
                        ) +
                        indexArray_theWeight + MString(" > ") + theWeights.length()
                    );
                    return MStatus::kFailure;
                }
                if (indexArray_fullWeightArray >= (int)fullWeightArray.length()) {
                    MGlobal::displayInfo(
                        MString(
                            "-> editArray FAILED | indexArray_fullWeightArray "
                            " >= fullWeightArray.length()"
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

            finalizeWeightRow(
                i, theVert, nbJoints, lockJoints, fullWeightArray, theWeights, sum,
                sumUnlockWeights, normalize
            );
        }
    }
    return stat;
}

MStatus editArrayMirror(
    ModifierCommands command, int influence, int influenceMirror, int nbJoints,
    MIntArray &lockJoints, MDoubleArray &fullWeightArray,
    std::map<int, std::pair<float, float>> &valuesToSetMirror, MDoubleArray &theWeights,
    bool normalize, double multiplier
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

            float sumValue = std::min(float(1.0), valueBase + valueMirror);
            float biggestValue = std::max(valueBase, valueMirror);

            double theVal = multiplier * (double)biggestValue + 1.0;
            double substract = theVal / nbJoints;

            MDoubleArray producedWeigths(nbJoints, 0.0);
            double totalBaseVtxUnlock = 0.0, totalBaseVtxLock = 0.0;
            ;
            double totalVtxUnlock = 0.0, totalVtxLock = 0.0;
            for (int j = 0; j < nbJoints; ++j) {
                double currentW = fullWeightArray[theVert * nbJoints + j];
                double targetW = (currentW * theVal) - substract;
                targetW = std::max(0.0, std::min(targetW, 1.0)); // clamp
                producedWeigths.set(targetW, j);
                if (lockJoints[j] == 0) { // unlock
                    totalBaseVtxUnlock += currentW;
                    totalVtxUnlock += targetW;
                }
                else {
                    totalBaseVtxLock += currentW;
                    totalVtxLock += targetW;
                }
            }
            applyLockNormalize(
                i, theVert, nbJoints, lockJoints, fullWeightArray, producedWeigths, theWeights,
                totalBaseVtxLock, totalVtxUnlock
            );
            i++;
        }
    }
    else {
        // do the other command --------------------------
        int i = -1; // i is a short index instead of theVert
        for (const auto &elem : valuesToSetMirror) {
            i++;
            int theVert = elem.first;
            double valueBase = multiplier * (double)elem.second.first;
            double valueMirror = multiplier * (double)elem.second.second;

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
            // When both sides paint the same influence, there is no separate mirror weight.
            // Treat it as zero so the influence isn't counted twice in the rest computation
            double currentWMirror = (influenceMirror == influence)
                                        ? 0.0
                                        : fullWeightArray[theVert * nbJoints + influenceMirror];
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
            finalizeWeightRow(
                i, theVert, nbJoints, lockJoints, fullWeightArray, theWeights, sum,
                sumUnlockWeights, normalize
            );
        }
    }
    return stat;
}

MStatus setAverageWeight(
    std::span<const int> verticesAround, int currentVertex, int indexCurrVert, int nbJoints,
    MIntArray &lockJoints, MDoubleArray &fullWeightArray, MDoubleArray &theWeights,
    double strengthVal
)
{
    int sizeVertices = (int)verticesAround.size();
    int jnt;

    if (sizeVertices == 0) { // nothing to average with, keep the current weights
        for (jnt = 0; jnt < nbJoints; jnt++) {
            theWeights[indexCurrVert * nbJoints + jnt] =
                fullWeightArray[currentVertex * nbJoints + jnt];
        }
        return MS::kSuccess;
    }

    MDoubleArray sumWeigths(nbJoints, 0.0);
    // compute sum weights
    for (int vertIndex : verticesAround) {
        for (jnt = 0; jnt < nbJoints; jnt++) {
            sumWeigths[jnt] += fullWeightArray[vertIndex * nbJoints + jnt];
        }
    }
    double totalBaseVtxUnlock = 0.0, totalBaseVtxLock = 0.0;
    double totalVtxUnlock = 0.0, totalVtxLock = 0.0;

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
            totalBaseVtxUnlock += currentW;
            totalVtxUnlock += targetW;
        }
        else {
            totalBaseVtxLock += currentW;
            totalVtxLock += targetW;
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
                targetW *= mult; // divide by 1 unless locked joints reduce available room
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

MStatus doPruneWeight(MDoubleArray &theWeights, int nbJoints, double pruneCutWeight)
{
    MStatus stat;

    int vertIndex, jnt, posiInArray;
    int nbElements = theWeights.length();
    int nbVertices = nbElements / nbJoints;
    double total = 0.0, val;

    for (vertIndex = 0; vertIndex < nbVertices; ++vertIndex) {
        total = 0.0;
        for (jnt = 0; jnt < nbJoints; jnt++) {
            posiInArray = vertIndex * nbJoints + jnt;
            val = theWeights[posiInArray];
            if (val > pruneCutWeight) {
                total += val;
            }
            else {
                theWeights[posiInArray] = 0.0;
            }
        }
        // now normalize
        if (total > 0.0 && total != 1.0) {
            for (jnt = 0; jnt < nbJoints; jnt++) {
                posiInArray = vertIndex * nbJoints + jnt;
                theWeights[posiInArray] /= total; // that should normalize
            }
        }
    }
    return MS::kSuccess;
}

void lineC(short x0, short y0, short x1, short y1, std::vector<std::pair<short, short>> &posi)
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

bool RayIntersectsBBox(
    const MPoint &minPt, const MPoint &maxPt, const MPoint &orig, const MVector &direction
)
{
    // Slab test: intersect the ray's parameter range with each axis-aligned slab
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        double t0 = (minPt[axis] - orig[axis]) / direction[axis];
        double t1 = (maxPt[axis] - orig[axis]) / direction[axis];
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        if (tmin > t1 || t0 > tmax) {
            return false;
        }
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
    }
    return true;
}

MPoint
offsetIntersection(const MPoint &rayPoint, const MVector &rayVector, const MVector &originNormal)
{
    // A little hack to shift the input ray point around to get the intersections with the offset
    // planes
    MVector diff = rayPoint - originNormal;
    double prod = (diff * originNormal) / (rayVector * originNormal);
    return rayPoint - (rayVector * prod) + originNormal;
}

MMatrix bboxMatrix(const MPoint &minPoint, const MPoint &maxPoint, const MMatrix &bbSpace)
{
    // Build the matrix of the bounding box as if it were a transformed 2x2x2 cube centered at the
    // origin
    MPoint c = (minPoint + maxPoint) / 2.0;
    MVector s = maxPoint - c;
    double matVals[4][4] = {
        {s.x, 0.0, 0.0, 0.0}, {0.0, s.y, 0.0, 0.0}, {0.0, 0.0, s.z, 0.0}, {c.x, c.y, c.z, 1.0}
    };
    return bbSpace * MMatrix(matVals);
}

inline bool inUnitPlane(const MPoint &inter)
{
    // Quickly check if the intersection happened in the unit plane
    return (inter.x <= 1.0 && inter.x >= -1.0) && (inter.y <= 1.0 && inter.y >= -1.0) &&
           (inter.z <= 1.0 && inter.z >= -1.0);
}

bool bboxIntersection(
    const MPoint &minPoint, const MPoint &maxPoint, const MMatrix &bbSpace, const MPoint &rayPoint,
    const MVector &rayVector, MPoint &intersection
)
{
    // Get the bbox matrix and its inverse
    MMatrix bbm = bboxMatrix(minPoint, maxPoint, bbSpace);
    MMatrix bbmi = bbm.inverse();

    // Transform the ray point/vector into the bbox matrix space
    MPoint rp = rayPoint * bbmi;
    MVector rv = rayVector * bbmi;

    MPoint rawInter;
    double rawDist = std::numeric_limits<double>::infinity();
    bool found = false;

    // For x/y/z
    for (int i = 0; i < 3; ++i) {
        // for +- 1
        for (int j = 0; j < 2; ++j) {
            MVector nrm;
            nrm[i] = 2 * j - 1;
            // Get the offset intersection
            MPoint inter = offsetIntersection(rp, rv, nrm);
            if (inUnitPlane(inter)) {
                // Keep track of the closest point
                double dist = inter.distanceTo(rp);
                if (dist < rawDist) {
                    rawDist = dist;
                    rawInter = inter;
                    found = true;
                }
            }
        }
    }

    // Only do the transformation if we found something
    if (found) {
        intersection = rawInter * bbm;
    }

    return found;
}

// Tyler find Functions
void getRawNeighbors(
    const MIntArray &counts, const MIntArray &indices, int numVerts,
    std::vector<int> &faceNeighborsFLAT, std::vector<int> &faceNeighborsINDEX,
    std::vector<int> &edgeNeighborsFLAT, std::vector<int> &edgeNeighborsINDEX
)
{
    std::vector<std::unordered_set<int>> faceNeighbors(numVerts);
    std::vector<std::unordered_set<int>> edgeNeighbors(numVerts);
    size_t ptr = 0;
    for (const int &c : counts) {
        for (int i = 0; i < c; ++i) {
            int j = (i + 1) % c;
            int rgt = indices[ptr + i];
            int lft = indices[ptr + j];
            edgeNeighbors[rgt].insert(lft);
            edgeNeighbors[lft].insert(rgt);
            for (int x = 0; x < c; ++x) {
                if (x == j) { // don't make a vertex its own neighbor
                    continue;
                }
                faceNeighbors[lft].insert(indices[ptr + x]);
            }
        }
        ptr += c;
    }
    faceNeighborsFLAT.clear();
    faceNeighborsINDEX.resize(numVerts + 1, 0);
    edgeNeighborsFLAT.clear();
    edgeNeighborsINDEX.resize(numVerts + 1, 0);
    for (int v = 0; v < numVerts; ++v) {
        faceNeighborsINDEX[v + 1] = faceNeighborsINDEX[v] + (int)faceNeighbors[v].size();
        faceNeighborsFLAT.insert(faceNeighborsFLAT.end(), faceNeighbors[v].begin(), faceNeighbors[v].end());
        edgeNeighborsINDEX[v + 1] = edgeNeighborsINDEX[v] + (int)edgeNeighbors[v].size();
        edgeNeighborsFLAT.insert(edgeNeighborsFLAT.end(), edgeNeighbors[v].begin(), edgeNeighbors[v].end());
    }
}

std::pair<unsigned int, unsigned int> infosSkinClusterPlugs(MObject skinCluster)
{

    MFnDependencyNode skinClusterDep(skinCluster);

    MPlug weight_list_plug = skinClusterDep.findPlug("weightList", false);
    MPlug matrix_plug = skinClusterDep.findPlug("matrix", false);
    unsigned int weightList_count = weight_list_plug.numElements();
    unsigned int matrix_count = matrix_plug.numElements();

    return std::make_pair(matrix_count, weightList_count);
}

bool areDagPathArraysEqual(const MDagPathArray &a, const MDagPathArray &b)
{
    // 1. Quick length check
    if (a.length() != b.length()) {
        return false;
    }
    // 2. Element-wise comparison
    for (unsigned int i = 0; i < a.length(); ++i) {
        // MDagPath has a built-in operator== that checks if paths are identical
        if (!(a[i] == b[i])) {
            return false;
        }
    }
    return true;
}

std::vector<int> findClosestWithinThreshold(
    const std::vector<int> &indices, const float *pos, const FlatCounts<int> &connVerts,
    float threshold, int nbVertices
)
{
    // Pair up each vertex with its closest unconnected, unpaired vertex within the threshold.
    // Sort by x so the search for each vertex can stop once dx exceeds the best distance
    std::vector<int> sorted(indices);
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    std::sort(sorted.begin(), sorted.end(), [pos](int a, int b) {
        return pos[a * 3] < pos[b * 3];
    });

    std::vector<int> results(nbVertices, -1);
    const float thresholdSq = threshold * threshold;
    const int count = (int)sorted.size();

    for (int i = 0; i < count; ++i) {
        int idxA = sorted[i];
        if (results[idxA] != -1) {
            continue;
        }
        float ax = pos[idxA * 3], ay = pos[idxA * 3 + 1], az = pos[idxA * 3 + 2];
        auto neighbors = connVerts[idxA];

        float minDistSq = thresholdSq;
        int bestNeighbor = -1;
        auto consider = [&](int idxB) {
            if (results[idxB] != -1) {
                return;
            }
            if (std::find(neighbors.begin(), neighbors.end(), idxB) != neighbors.end()) {
                return;
            }
            float dx = pos[idxB * 3] - ax;
            float dy = pos[idxB * 3 + 1] - ay;
            float dz = pos[idxB * 3 + 2] - az;
            float distSq = dx * dx + dy * dy + dz * dz;
            if (distSq < minDistSq) {
                minDistSq = distSq;
                bestNeighbor = idxB;
            }
        };

        // Walk outward in both directions until the x-distance alone rules out a better match
        for (int j = i + 1; j < count; ++j) {
            float dx = pos[sorted[j] * 3] - ax;
            if (dx * dx >= minDistSq) {
                break;
            }
            consider(sorted[j]);
        }
        for (int j = i - 1; j >= 0; --j) {
            float dx = pos[sorted[j] * 3] - ax;
            if (dx * dx >= minDistSq) {
                break;
            }
            consider(sorted[j]);
        }

        if (bestNeighbor != -1) {
            results[idxA] = bestNeighbor;
            results[bestNeighbor] = idxA;
        }
    }
    return results;
}

// https://rodolphe-vaillant.fr/entry/79/maya-c-api-set-skinning-weight-attributes

// TO TEST

// The fastest way to set multi attributes / array attributes is done through a DAG node. (100 times
// faster than MPlugs!) const std::vector<std::map<int/*joint id*/, float/*skin weight*/> >&
// weights,
/*
void set_skinning_weights(
        const std::vector<std::map<int, float> >& weights,
        MDataBlock& block)
{
    MStatus status = MS::kSuccess;
    MArrayDataHandle array_hdl = block.outputArrayValue(_s_skin_weights, &status);
    mayaCheck(status);
    for(unsigned i = 0; i < weights.size(); i++)
    {
        mayaCheck( array_hdl.jumpToArrayElement( i ) );

        // weightList[i]
        MDataHandle element_hdl = array_hdl.outputValue( &status );
        mayaCheck(status);
        // weightList[i].weight
        MDataHandle child = element_hdl.child( _s_per_joint_weights );

        MArrayDataHandle weight_list_hdl(child, &status);
        mayaCheck(status);

        MArrayDataBuilder weight_list_builder = weight_list_hdl.builder(&status);
        mayaCheck(status);

        unsigned handle_count = weight_list_hdl.elementCount(&status);
        mayaCheck(status);

        unsigned builder_count = weight_list_builder.elementCount(&status);
        mayaCheck(status);
        mayaAssert( builder_count == handle_count);

        std::map<int, float> map = weights[i];
        //std::map<int influence obj id / joint id, float> map = weights[i];

        std::vector to_remove;
        to_remove.reserve( map.size() );

        // Scan array, update existing element, remove unsused ones
        for(unsigned j = 0; j < handle_count; ++j)
        {
            // weightList[i].weight[j]
            mayaCheck( weight_list_hdl.jumpToArrayElement(j) );
            unsigned index = weight_list_hdl.elementIndex(&status);
            mayaCheck(status);

            auto elt = map.find( index );

            if( elt != map.end() )
            {
                MDataHandle hdl = weight_list_builder.addElement(index, &status);
                mayaCheck(status);
                hdl.setDouble( (double)elt->second );
                map.erase( elt );
            }
            else
            {
                to_remove.push_back( index );
            }
        }

        for( unsigned idx : to_remove ){
            mayaCheck( weight_list_builder.removeElement( idx ) );
        }

        mayaCheck( weight_list_hdl.set( weight_list_builder ) );
    }

}
*/
