#include "skinBrushFlags.h"
#include "skinBrushTool.h"
#include <limits>
#include <span>

// Forward declarations for free functions defined in skinBrushImperative.cpp
MColor getASoloColor(double val, const UserInputData &ui, const InfluenceData &infl);
ModifierCommands
getCommandIndexModifiers(const UserInputData &ui, const InteractionPerFrameData &frame);
void getColorWithMirror(
    int vertexIndex, float valueBase, float valueMirror, MColor &multColor, MColor &soloColor,
    const UserInputData &ui, const InfluenceData &infl, const WeightData &weights,
    const InteractionPerFrameData &frame
);
void mergeMirrorArray(
    WeightData &weights, const MirrorableData &base, const MirrorableData &mirror
);
MStatus refreshColors(
    MIntArray &editVertsIndices, MColorArray &multiEditColors, MColorArray &soloEditColors,
    const UserInputData &ui, const InfluenceData &infl, WeightData &weights
);
MStatus doPerformPaint(
    bool postSetting, bool &toggleColorState, const UserInputData &ui, const InfluenceData &infl,
    WeightData &weights, const InteractionPerFrameData &frame, MeshState &mesh
);
MStatus editSoloColorSet(
    bool doBlack, const UserInputData &ui, const InfluenceData &infl, WeightData &weights,
    MeshState &mesh
);
std::span<const int> getSurroundingVerticesPerVert(int vertexIndex, const MeshState &mesh);
std::span<const int> getSurroundingVerticesPerFace(int faceIndex, const MeshState &mesh);

// ---------------------------------------------------------------------
// the context
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// general methods when calling the context
// ---------------------------------------------------------------------
SkinBrushContext::SkinBrushContext()
{
    setTitleString("Smooth Weights");
    setImage("brSkinBrush.svg", MPxContext::kImage1);
    setCursor(MCursor::editCursor);

    // Define the default values for the context.
    // These values will be used to reset the tool from the tool
    // properties window.
    performRefreshViewPort = 0;

    input.colorVal = MColor(1.0, 0.0, 0.0);
    input.curveVal = 2;
    input.drawBrushVal = true;
    input.drawRangeVal = true;
    input.moduleImportString = MString("from mPaintEditor.brushTools.brushPythonFunctions import ");
    input.enterToolCommandVal = "";
    input.exitToolCommandVal = "";
    input.fractionOversamplingVal = false;
    input.ignoreLockVal = false;
    input.lineWidthVal = 1;
    input.messageVal = 2;
    input.oversamplingVal = 1;
    rangeVal = 0.5;
    input.sizeVal = 5.0;
    input.strengthVal = 0.25;
    input.smoothStrengthVal = 1.0;

    pruneWeight = 0.0001;

    input.undersamplingVal = 2;
    input.volumeVal = false;
    coverageVal = true;
    input.postSetting = true;

    input.commandIndex = ModifierCommands::Add;
    input.soloColorTypeVal = 1; // 1 lava
    input.soloColorVal = 0;
    // True, only if the smoothing is performed. False when adjusting
    // the brush settings. It's used to control whether undo/redo needs
    // to get called.
    interPersist.performBrush = false;
    interPersist.firstPaintDone = false;
}

void SkinBrushContext::toolOnSetup(MEvent &)
{
    MStatus status = MStatus::kSuccess;

    setHelpString("it's a custom brush weights.");
    setInViewMessage(true);

    if (input.enterToolCommandVal.length() > 5) {
        MGlobal::executeCommand(input.enterToolCommandVal);
    }
    MGlobal::executePythonCommand(
        input.moduleImportString + MString(
                                       "toolOnSetupEnd, "
                                       "toolOnSetupStart\n"
                                   )
    );
    MGlobal::executePythonCommand("toolOnSetupStart()");
    MUserEventMessage::postUserEvent("brSkinBrush_toolOnSetupStart");

    this->interPersist.firstPaintDone = false;
    this->input.pickMaxInfluenceVal = false;
    this->input.pickInfluenceVal = false;

    // read from selection or the input variables
    getDagMesh();
    getObjSkinCluster();
    this->skipSkinValues = true;

    bool meshReentry = reenterMesh && (this->previousBrushDagPath.isValid() &&
                                       this->mesh.meshDag == this->previousBrushDagPath);
    if (meshReentry) {
        if (verbose) {
            MGlobal::displayInfo(" - MESH REENTRY TEST -");
        }
        refreshPointsNormals();
    }
    else {
        catchTimeStamp();
        status = getMesh();
        endTimeStamp(MString("getMesh"));
    }
    // first clear a bit the air --------------
    bool skinReentry =
        meshReentry && reenterSkin &&
        (!this->previousSkinMObject.isNull() && this->weights.skinObj == this->previousSkinMObject);
    MIntArray editVertsIndices;
    if (skinReentry) {
        std::pair<unsigned int, unsigned int> infosPlugs =
            infosSkinClusterPlugs(this->weights.skinObj);
        skinReentry = infosPlugs == storedPlugCountSkinObj;
        if (!skinReentry) {
            MGlobal::displayInfo(MString("-- not same number of plugs--"));
        }
        else {
            // catchTimeStamp();
            MDagPathArray currentDagPaths;
            MFnSkinCluster skinFn(this->weights.skinObj);
            skinFn.influenceObjects(currentDagPaths);
            skinReentry = areDagPathArraysEqual(currentDagPaths, this->influence.inflDagPaths);
            if (!skinReentry) {
                MGlobal::displayInfo(MString("-- not same dagPath for joints --"));
            }
            // endTimeStamp(MString("get joints dagPath for Check Objects"));
        }
    }
    if (!skinReentry) {
        if (verbose) {
            MGlobal::displayInfo(MString("-- skin FAIL reentry --"));
        }
        secondPartSkincluster();
        this->weights.multiCurrentColors.clear();
        this->influence.jointsColors.clear();
        this->weights.soloCurrentColors.clear();

        if (!weights.skinObj.isNull()) {
            getListColorsJoints(
                weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
                influence.jointsColors
            ); // get the joints colors
            this->weights.skinWeightList.clear();
            this->influence.ignoreLockJoints = MIntArray(this->influence.nbJoints, 0);
            if (this->input.mirrorInfluences.length() == 0) {
                this->input.mirrorInfluences = MIntArray(this->influence.nbJoints, 0);
                for (unsigned int i = 0; i < this->influence.nbJoints; ++i) {
                    this->input.mirrorInfluences.set(i, i);
                }
            }
            getListLockJoints(
                weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
                this->influence.lockJoints
            );
            getListLockVertices(weights.skinObj, this->weights.lockVertices, editVertsIndices);
            storedPlugCountSkinObj = infosSkinClusterPlugs(this->weights.skinObj);

            if (!skinReentry) {
                status = fillArrayValues(weights.skinObj, true);
            }

            if (verbose) {
                MGlobal::displayInfo(
                    MString("nb found joints colors ") + influence.jointsColors.length()
                );
            }
        }
        else {
            MGlobal::displayInfo(MString("FAILED toolOnSetup: weights.skinObj.isNull"));
            abortAction();
            return;
        }
    }
    else {
        if (verbose) {
            MGlobal::displayInfo(MString("-- SKIN REENTRY --"));
        }
        getListLockVertices(weights.skinObj, this->weights.lockVertices, editVertsIndices);
    }

    // solo colors -----------------------
    this->weights.soloCurrentColors = MColorArray(this->mesh.numVertices, MColor(0.0, 0, 0.0));
    this->weights.soloColorsValues = MDoubleArray(this->mesh.numVertices, 0.0);

    MStringArray currentColorSets;
    mesh.meshFn.getColorSetNames(currentColorSets);
    if (currentColorSets.indexOf(this->interFrame.fullColorSet) == -1) { // multiColor
        mesh.meshFn.createColorSetWithName(this->interFrame.fullColorSet);
    }

    if (currentColorSets.indexOf(this->interFrame.soloColorSet) == -1) { // soloColor
        mesh.meshFn.createColorSetWithName(this->interFrame.soloColorSet);
    }

    if (currentColorSets.indexOf(this->interFrame.fullColorSet2) == -1) { // multiColor
        mesh.meshFn.createColorSetWithName(this->interFrame.fullColorSet2);
    }

    if (currentColorSets.indexOf(this->interFrame.soloColorSet2) == -1) { // soloColor
        mesh.meshFn.createColorSetWithName(this->interFrame.soloColorSet2);
    }

    mesh.meshFn.setColors(
        this->weights.multiCurrentColors, &this->interFrame.fullColorSet
    ); // set the multi assignation
    mesh.meshFn.assignColors(fullVertexList, &this->interFrame.fullColorSet);

    mesh.meshFn.setColors(
        this->weights.soloCurrentColors, &this->interFrame.soloColorSet
    ); // set the solo assignation
    mesh.meshFn.assignColors(fullVertexList, &this->interFrame.soloColorSet);

    mesh.meshFn.setColors(
        this->weights.multiCurrentColors, &this->interFrame.fullColorSet2
    ); // set the multi assignation
    mesh.meshFn.assignColors(fullVertexList, &this->interFrame.fullColorSet2);

    mesh.meshFn.setColors(
        this->weights.soloCurrentColors, &this->interFrame.soloColorSet2
    ); // set the solo assignation
    mesh.meshFn.assignColors(fullVertexList, &this->interFrame.soloColorSet2);

    MString currentColorSet = mesh.meshFn.currentColorSetName(); // set multiColor as current Color
    if (input.soloColorVal == 1) {                               // solo
        if (currentColorSet != this->interFrame.soloColorSet) {
            mesh.meshFn.setCurrentColorSetName(this->interFrame.soloColorSet);
        }
        editSoloColorSet(true);
    }
    else {
        if (currentColorSet != this->interFrame.fullColorSet) {
            mesh.meshFn.setCurrentColorSetName(
                this->interFrame.fullColorSet
            ); // , &this->colorSetMod);
        }
    }

    // display the locks
    MColorArray multiEditColors, soloEditColors;
    refreshColors(editVertsIndices, multiEditColors, soloEditColors);
    this->paint.skinValuesToSet.clear();

    applyVertexColors(editVertsIndices, multiEditColors, soloEditColors);
    mesh.meshFn.setDisplayColors(true);

    view = M3dView::active3dView();
    view.refresh(false, true);

    MGlobal::executePythonCommand("toolOnSetupEnd()");
    MUserEventMessage::postUserEvent("brSkinBrush_toolOnSetupEnd");
}

void SkinBrushContext::toolOffCleanup()
{
    setInViewMessage(false);
    mesh.meshFn.updateSurface(); // try avoiding crashes
    if (input.exitToolCommandVal.length() > 5) {
        MGlobal::executeCommand(input.exitToolCommandVal);
    }
    MUserEventMessage::postUserEvent("brSkinBrush_toolOffCleanup");
    if (!this->interPersist.firstPaintDone) {
        this->interPersist.firstPaintDone = true;
        MUserEventMessage::postUserEvent("brSkinBrush_cleanCloseUndo");
    }
    getSkinFromName = false;
    getMeshFromName = false;
    // for reentry
    this->previousBrushDagPath = this->mesh.meshDag;
    this->previousSkinMObject = this->weights.skinObj;
}

void SkinBrushContext::getClassName(MString &name) const { name.set("brSkinBrush"); }

void SkinBrushContext::refreshJointsLocks()
{
    if (!weights.skinObj.isNull()) {
        // Get the skin cluster node from the history of the mesh.
        getListLockJoints(
            weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
            this->influence.lockJoints
        );
    }
}

void SkinBrushContext::refreshMirrorInfluences(MIntArray &inputMirrorInfluences)
{
    this->input.mirrorInfluences.clear();
    this->input.mirrorInfluences.copy(inputMirrorInfluences);
}

void SkinBrushContext::refreshTheseVertices(MIntArray &verticesIndices)
{
    // this command is used when undo is called
    querySkinClusterValues(
        this->weights.skinObj, verticesIndices, this->weights.skinWeightList, true
    );
    // query the Locks
    getListLockJoints(
        weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
        this->influence.lockJoints
    );
    MIntArray editVertsIndices;
    getListLockVertices(weights.skinObj, this->weights.lockVertices, editVertsIndices);

    if (!mesh.meshDag.isValid()) {
        return;
    }
    // points and normals
    refreshPointsNormals();

    MColorArray multiEditColors, soloEditColors;
    refreshColors(verticesIndices, multiEditColors, soloEditColors);
    this->paint.skinValuesToSet.clear();
    applyVertexColors(verticesIndices, multiEditColors, soloEditColors);

    // if locking or unlocking
    // without that it doesn't refresh because mesh is not invalidated, meaning the skinCluster
    // hasn't changed
    mesh.meshFn.updateSurface();

    // refresh view and display
    maya2019RefreshColors();

    this->paint.previousPaint.clear();
    this->mirror.previousPaint.clear();
}

void SkinBrushContext::refreshDeformerColor(int deformerInd)
{
    if (!weights.skinObj.isNull()) {
        getListLockJoints(
            weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
            this->influence.lockJoints
        );
        getListColorsJoints(
            weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
            this->influence.jointsColors
        );
    }
    else {
        MGlobal::displayInfo(MString("FAILED : weights.skinObj.isNull"));
        return;
    }

    // get the vertices indices to edit -------------------
    MIntArray editVertsIndices;
    for (unsigned int theVert = 0; theVert < this->mesh.numVertices; ++theVert) {
        double theWeight = 0.0;
        int ind_swl = theVert * this->influence.nbJoints + deformerInd;
        if (ind_swl < this->weights.skinWeightList.length()) {
            theWeight = this->weights.skinWeightList[ind_swl];
        }
        if (theWeight != 0.0) {
            editVertsIndices.append(theVert);
        }
    }

    // display the locks ----------------------
    MColorArray multiEditColors, soloEditColors;
    refreshColors(editVertsIndices, multiEditColors, soloEditColors);
    applyVertexColors(editVertsIndices, multiEditColors, soloEditColors);

    if (input.soloColorVal == 1) {
        editSoloColorSet(true); // solo
    }
    // refresh view and display
    mesh.meshFn.updateSurface();
    maya2019RefreshColors();
}

void SkinBrushContext::refresh()
{
    MStatus status;
    refreshPointsNormals();
    MIntArray editVertsIndices;

    if (!weights.skinObj.isNull()) {
        // Get the skin cluster node from the history of the mesh.
        getListLockJoints(
            weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
            this->influence.lockJoints
        );
        getListColorsJoints(
            weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
            this->influence.jointsColors
        );
        status = getListLockVertices(
            weights.skinObj, this->weights.lockVertices, editVertsIndices
        );
        status = fillArrayValues(weights.skinObj, true);
    }
    else {
        MGlobal::displayError(MString("FAILED : weights.skinObj.isNull"));
        return;
    }

    this->paint.skinValuesToSet.clear();

    mesh.meshFn.setColors(
        this->weights.multiCurrentColors, &this->interFrame.fullColorSet
    ); // set the multi assignation
    mesh.meshFn.setColors(
        this->weights.soloCurrentColors, &this->interFrame.soloColorSet
    ); // set the solo assignation

    mesh.meshFn.setColors(
        this->weights.multiCurrentColors, &this->interFrame.fullColorSet2
    ); // set the multi assignation
    mesh.meshFn.setColors(
        this->weights.soloCurrentColors, &this->interFrame.soloColorSet2
    ); // set the solo assignation

    // display the locks ----------------------

    MColorArray multiEditColors, soloEditColors;
    refreshColors(editVertsIndices, multiEditColors, soloEditColors);
    applyVertexColors(editVertsIndices, multiEditColors, soloEditColors);

    if (input.soloColorVal == 1) {
        editSoloColorSet(true); // solo
    }

    // refresh view and display
    mesh.meshFn.updateSurface();

    maya2019RefreshColors();
}

// ---------------------------------------------------------------------
// viewport 2.0
// ---------------------------------------------------------------------

MStatus SkinBrushContext::doPress(
    MEvent &event, MHWRender::MUIDrawManager &drawMgr, const MHWRender::MFrameContext &context
)
{
    interPersist.pressStatus = doPressCommon(event);
    CHECK_MSTATUS_AND_RETURN_SILENT(interPersist.pressStatus);
    doDrag(event, drawMgr, context);
    return MStatus::kSuccess;
}

MStatus SkinBrushContext::doDrag(
    MEvent &event, MHWRender::MUIDrawManager &drawManager, const MHWRender::MFrameContext &context
)
{
    MStatus status = MStatus::kSuccess;
    if (this->input.pickMaxInfluenceVal || this->input.pickInfluenceVal) {
        return MS::kFailure;
    }

    status = doDragCommon(event);
    if (this->input.postSetting && !this->input.useColorSetsWhilePainting) {
        drawManager.beginDrawable();
        drawMeshWhileDrag(drawManager);
        drawManager.endDrawable();
    }
    CHECK_MSTATUS_AND_RETURN_SILENT(status);

    // -----------------------------------------------------------------
    // display when painting or setting the brush size
    // -----------------------------------------------------------------
    if (this->input.drawBrushVal || (event.mouseButton() == MEvent::kMiddleMouse)) {
        CHECK_MSTATUS_AND_RETURN_SILENT(interPersist.pressStatus);
        drawManager.beginDrawable();

        drawManager.setColor(MColor(
            (pow(input.colorVal.r, 0.454f)), (pow(input.colorVal.g, 0.454f)),
            (pow(input.colorVal.b, 0.454f))
        ));
        drawManager.setLineWidth((float)input.lineWidthVal);
        // Draw the circle in regular paint mode.
        // The range circle doens't get drawn here to avoid visual
        // clutter.
        if (event.mouseButton() == MEvent::kLeftMouse) {
            if (this->paint.successFullDragHit) {
                drawManager.circle(
                    this->paint.centerOfBrush, this->interFrame.normalVector, input.sizeVal
                );
            }
        }
        // Adjusting the brush settings with the middle mouse button.
        else if (event.mouseButton() == MEvent::kMiddleMouse) {
            // When adjusting the size the circle needs to remain with
            // a static position but the size needs to change.
            drawManager.setColor(MColor(1, 0, 1));

            if (interPersist.sizeAdjust) {
                drawManager.circle(
                    interStart.surfacePointAdjust, interStart.worldVectorAdjust,
                    interPersist.adjustValue
                );
                if (input.volumeVal && input.drawRangeVal) {
                    drawManager.circle(
                        interStart.surfacePointAdjust, interStart.worldVectorAdjust,
                        interPersist.adjustValue * rangeVal
                    );
                }
            }
            // When adjusting the strength the circle needs to remain
            // fixed and only the strength indicator changes.
            else {
                drawManager.circle(
                    interStart.surfacePointAdjust, interStart.worldVectorAdjust, input.sizeVal
                );
                if (input.volumeVal && input.drawRangeVal) {
                    drawManager.circle(
                        interStart.surfacePointAdjust, interStart.worldVectorAdjust,
                        input.sizeVal * rangeVal
                    );
                }

                MPoint start(interStart.startScreenX, interStart.startScreenY);
                MPoint end(
                    interStart.startScreenX,
                    interStart.startScreenY + interPersist.adjustValue * 500
                );
                drawManager.line2d(start, end);

                drawManager.circle2d(end, input.lineWidthVal + 3.0, true);
            }
        }
        drawManager.endDrawable();
    }

    return status;
}

MStatus SkinBrushContext::drawMeshWhileDrag(MHWRender::MUIDrawManager &drawManager)
{
    // This function is the hottest path when painting
    // So it can and should be optimized more
    // I think the endgame for this is to only update the changed vertices each runthrough

    // Everything drawn here is indexed by the entries of mirroredJoinedArray
    unsigned int nbVtx = (unsigned int)this->weights.mirroredJoinedArray.size();

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
    ModifierCommands theCommandIndex = getCommandIndexModifiers();

    if (input.drawTransparency || input.drawPoints) {
        if (theCommandIndex == ModifierCommands::LockVertices) {
            baseColor = this->weights.lockVertColor;
        }
        else if (theCommandIndex == ModifierCommands::Remove) {
            baseColor = black;
        }
        else if (theCommandIndex == ModifierCommands::UnlockVertices) {
            baseColor = white;
        }
        else if (theCommandIndex == ModifierCommands::Smooth) {
            baseColor = white;
        }
        else if (theCommandIndex == ModifierCommands::Sharpen) {
            baseColor = white;
        }
        else {
            baseColor = this->influence.jointsColors[this->input.influenceIndex];
            if (this->input.paintMirror != 0) {
                baseMirrorColor =
                    this->influence
                        .jointsColors[this->input.mirrorInfluences[this->input.influenceIndex]];
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

    // copy to vector for indexed parallel access (unordered_map lacks random access)
    std::vector<std::pair<int, std::pair<float, float>>> mja(
        this->weights.mirroredJoinedArray.begin(), this->weights.mirroredJoinedArray.end()
    );

    MColorArray colors, colorsSolo;
    colors.setLength(mja.size());
    colorsSolo.setLength(mja.size());

    MColorArray *usedColors;
    MColorArray *currentColors;
    if (this->input.soloColorVal == 1) {
        usedColors = &colorsSolo;
        currentColors = &this->weights.soloCurrentColors;
    }
    else {
        usedColors = &colors;
        currentColors = &this->weights.multiCurrentColors;
    }

    bool doTransparency = input.drawTransparency;
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
            this->mesh.mayaRawPoints[ptIndex * 3], this->mesh.mayaRawPoints[ptIndex * 3 + 1],
            this->mesh.mayaRawPoints[ptIndex * 3 + 2]
        );
        posPoint = posPoint * this->mesh.inclusiveMatrix;
        points.set(posPoint, i);
        normals.set(mesh.verticesNormals[ptIndex], i);
    }

    if (input.drawTriangles) {
#pragma omp parallel for
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            int ptIndex = pt.first;
            float weightBase = pt.second.first;
            float weightMirror = pt.second.second;
            MColor multColor, soloColor;
            this->getColorWithMirror(ptIndex, weightBase, weightMirror, multColor, soloColor);
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

    if (input.drawPoints) {
#pragma omp parallel for
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            float weight = pt.second.first + pt.second.second;
            pointsColors[i] = weight * baseColor + (1.0 - weight) * (*currentColors)[pt.first];
        }
    }

    if (input.drawEdges) {
        darkEdges.setLength(mja.size());
#pragma omp parallel for
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            float transparency = (doTransparency) ? pt.second.first + pt.second.second : 1.0;
            darkEdges.set(i, 0.5f, 0.5f, 0.5f, transparency);
        }
    }

    if (input.drawTriangles || input.drawEdges) {
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            verticesMap[pt.first] = i;
            vertMap_bitset[pt.first] = true;
        }
    }

    if (input.drawTriangles) {
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            int ptIndex = pt.first;
            for (int fi : mesh.perVertexFaces[ptIndex]) {
                fatFaces_bitset[fi] = true;
            }
        }
    }

    if (input.drawEdges) {
        for (unsigned i = 0; i < mja.size(); ++i) {
            const auto &pt = mja[i];
            int ptIndex = pt.first;
            for (int ei : mesh.perVertexEdges[ptIndex]) {
                fatEdges_bitset[ei] = true;
            }
        }
    }

    if (input.drawTriangles) {
        // bitset is faster than an unordered_set in this case
        // may be worth keeping the bitsets around on the brush
        // so we don't have to constantly allocate memory
        for (unsigned f = 0; f < fatFaces_bitset.size(); ++f) {
            if (!fatFaces_bitset[f]) {
                continue;
            }
            for (size_t t = 0; t < mesh.perFaceTriangles.length2(f); ++t) {
                auto tri = mesh.perFaceTriangles(f, t);
                int v0 = tri[0], v1 = tri[1], v2 = tri[2];
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

    if (input.drawEdges) {
        // bitset is faster than an unordered_set in this case
        // may be worth keeping the bitsets around on the brush
        // so we don't have to constantly allocate memory
        for (unsigned e = 0; e < fatEdges_bitset.size(); ++e) {
            if (!fatEdges_bitset[e]) {
                continue;
            }
            auto &pairEdges = this->mesh.perEdgeVertices[e];

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

    if (input.drawPoints) {
        drawManager.setPointSize(4);
        drawManager.mesh(MHWRender::MUIDrawManager::kPoints, points, NULL, &pointsColors);
    }
    return MStatus::kSuccess;
}

MStatus SkinBrushContext::doRelease(
    MEvent &event, MHWRender::MUIDrawManager &drawMgr, const MHWRender::MFrameContext &context
)
{
    return doReleaseCommon(event);
}

MStatus SkinBrushContext::refreshPointsNormals()
{
    MStatus status = MStatus::kSuccess;

    if (!weights.skinObj.isNull() && mesh.meshDag.isValid(&status)) {
        this->mesh.meshFn.freeCachedIntersectionAccelerator(); // yes ?
        this->mesh.mayaRawPoints = const_cast<float *>(this->mesh.meshFn.getRawPoints(&status));
        this->mesh.rawNormals = const_cast<float *>(this->mesh.meshFn.getRawNormals(&status));
        int rawNormalsLength = this->mesh.meshFn.numNormals() * 3;

#pragma omp parallel for
        for (int vertexInd = 0; vertexInd < this->mesh.numVertices; vertexInd++) {
            int indNormal = this->mesh.verticesNormalsIndices[vertexInd];
            int rawIndNormal = indNormal * 3 + 2;
            if (rawIndNormal < rawNormalsLength) {
                MVector theNormal(
                    this->mesh.rawNormals[indNormal * 3], this->mesh.rawNormals[indNormal * 3 + 1],
                    this->mesh.rawNormals[indNormal * 3 + 2]
                );
                this->mesh.verticesNormals.set(theNormal, vertexInd);
            }
        }
    }
    return status;
}

// ---------------------------------------------------------------------
// common methods for legacy viewport and viewport 2.0
// ---------------------------------------------------------------------

int SkinBrushContext::getClosestInfluenceToCursor(int screenX, int screenY)
{
    MStatus stat;
    MPoint nearClipPt, farClipPt;
    MVector direction, direction2;
    MPoint orig, orig2;
    view.viewToWorld(screenX, screenY, orig, direction);

    int lent = this->influence.inflDagPaths.length();
    int closestInfluence = -1;
    double closestDistance = -1;

    // We've only ever got a couple hundred of these, so just brute-force it
    for (unsigned int i = 0; i < lent; i++) {
        MMatrix matI = interStart.BBoxOfDeformers[i].mat.inverse();
        orig2 = orig * matI;
        direction2 = direction * matI;

        MPoint minPt = interStart.BBoxOfDeformers[i].minPt;
        MPoint maxPt = interStart.BBoxOfDeformers[i].maxPt;
        MPoint center = interStart.BBoxOfDeformers[i].center;

        bool intersect = RayIntersectsBBox(minPt, maxPt, orig2, direction2);
        if (intersect) {
            double dst = center.distanceTo(orig2);
            if ((closestInfluence == -1) || (dst < closestDistance)) {
                closestDistance = dst;
                closestInfluence = i;
            }
        }
    }
    return closestInfluence;
}

int SkinBrushContext::getHighestInfluence(int faceHit, MFloatPoint &hitPoint)
{
    // get closest vertex
    auto verticesSet = getSurroundingVerticesPerFace(faceHit);
    int indexVertex = -1;
    float closestDist = FLT_MAX;
    for (int ptIndex : verticesSet) {
        MFloatPoint posPoint(
            this->mesh.mayaRawPoints[ptIndex * 3], this->mesh.mayaRawPoints[ptIndex * 3 + 1],
            this->mesh.mayaRawPoints[ptIndex * 3 + 2]
        );
        float dist = posPoint.distanceTo(hitPoint);
        if (indexVertex == -1 || dist < closestDist) {
            indexVertex = ptIndex;
            closestDist = dist;
        }
    }
    // now get highest influence for this vertex

    int biggestInfluence = -1;
    double biggestVal = 0;
    std::vector<double> allWeights;
    for (int indexInfluence = 0; indexInfluence < this->influence.nbJoints; ++indexInfluence) {
        double theWeight = 0.0;
        int ind_swl = indexVertex * this->influence.nbJoints + indexInfluence;
        if (ind_swl < this->weights.skinWeightList.length()) {
            theWeight = this->weights.skinWeightList[ind_swl];
        }
        allWeights.push_back(theWeight);
        if (theWeight > biggestVal) {
            biggestVal = theWeight;
            biggestInfluence = indexInfluence;
        }
    }
    // now sort the allWights array (I found that online hoepfully it works)
    std::vector<int> indices;
    indices.resize(this->influence.nbJoints);
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](int i, int j) {
        return allWeights[i] > allWeights[j];
    });

    // now we transfer that to our UI
    this->interPersist.orderedIndicesByWeights = MString("");
    this->interPersist.orderedIndicesByWeightsVals.clear();
    for (int ind : indices) {
        this->interPersist.orderedIndicesByWeights += MString("") + ind + MString(" ");
        this->interPersist.orderedIndicesByWeightsVals.append(ind);
    }
    return biggestInfluence;
}

MStatus SkinBrushContext::doPtrMoved(
    MEvent &event, MHWRender::MUIDrawManager &drawManager, const MHWRender::MFrameContext &context
)
{
    event.getPosition(interFrame.screenX, interFrame.screenY);
    bool displayPickInfluence = this->input.pickMaxInfluenceVal || this->input.pickInfluenceVal;
    if (this->input.pickInfluenceVal) {
        // -------------------------------------------------------------------------------------------------
        // start fill jnts boundingBox
        // --------------------------------------------------------------------
        if (this->interStart.BBoxOfDeformers.size() == 0) { // fill it
            double jointDisplayVal;
            MGlobal::executeCommand("jointDisplayScale -query", jointDisplayVal);

            int lent = this->influence.inflDagPaths.length();
            MPoint zero(0, 0, 0);
            MVector up(0, 1, 0);
            MVector right(1, 0, 0);
            MVector side(0, 0, 1);
            for (unsigned int i = 0; i < lent; i++) { // for all deformers
                MDagPath path = this->influence.inflDagPaths[i];
                drawingDeformers newDef;

                MMatrix worldMatrix = path.inclusiveMatrix();       // worldMatrix
                MMatrix parentMatrix = path.exclusiveMatrix();      // parentMatrix
                MMatrix mat = worldMatrix * parentMatrix.inverse(); // matrix

                MBoundingBox bbox;

                right = MVector(worldMatrix[0]);
                up = MVector(worldMatrix[1]);
                side = MVector(worldMatrix[2]);

                unsigned int nbShapes;
                path.numberOfShapesDirectlyBelow(nbShapes);
                if (nbShapes != 0) {
                    path.extendToShapeDirectlyBelow(0);
                    MFnDagNode dag(path);
                    bbox = dag.boundingBox(); // Returns the bounding box for the dag node in
                                              // object space.

                    MPoint center = bbox.center() * worldMatrix;
                    newDef.center = center;
                    newDef.width = 0.5 * bbox.width() * right.length();
                    newDef.height = 0.5 * bbox.height() * up.length();
                    newDef.depth = 0.5 * bbox.depth() * side.length();

                    newDef.mat = worldMatrix;
                    newDef.minPt = bbox.min();
                    newDef.maxPt = bbox.max();
                }
                else {
                    MFnDagNode dag(path);
                    MStatus plugStat;
                    MPlug radiusPlug = dag.findPlug("radius", false, &plugStat);
                    double multVal = jointDisplayVal;
                    if (plugStat == MStatus::kSuccess) {
                        multVal *= radiusPlug.asDouble();
                    }

                    newDef.center = zero * worldMatrix;
                    newDef.width = 0.5 * right.length() * multVal;
                    newDef.height = 0.5 * up.length() * multVal;
                    newDef.depth = 0.5 * side.length() * multVal;

                    newDef.mat = worldMatrix;
                    newDef.minPt = multVal * MPoint(-0.5, -0.5, -0.5);
                    newDef.maxPt = multVal * MPoint(0.5, 0.5, 0.5);
                }
                newDef.up = up;
                newDef.right = right;

                interStart.BBoxOfDeformers.push_back(newDef);
            }
        } // end fill it

        // end fill jnts boundingBox
        // --------------------------------------------------------------------
        // -----------------------------------------------------------------------------------------------
        interPersist.biggestInfluence =
            getClosestInfluenceToCursor(interFrame.screenX, interFrame.screenY);
    }

    paint.successfullHit = computeHit(
        interFrame.screenX, interFrame.screenY, true, interFrame.faceHit, this->paint.centerOfBrush
    );

    if (!paint.successfullHit &&
        !this->refreshDone) { // try to re-get mesh.accelParams in case no hit
        refreshPointsNormals();
        paint.successfullHit = computeHit(
            interFrame.screenX, interFrame.screenY, true, interFrame.faceHit,
            this->paint.centerOfBrush
        );
        this->refreshDone = true;
    }

    if (!paint.successfullHit && !displayPickInfluence) {
        return MStatus::kNotFound;
    }

    drawManager.beginDrawable();
    drawManager.setColor(MColor(0.0, 0.0, 1.0));
    drawManager.setLineWidth((float)input.lineWidthVal);
    MColor biggestInfluenceColor(1.0, 0.0, 0.0);

    if (this->input.pickMaxInfluenceVal || this->input.pickInfluenceVal) {
        if (this->input.pickInfluenceVal) {
            // ---------------------------------------------------------------------------------------
            // start reDraw jnts
            // --------------------------------------------------------------------
            int lent = this->influence.inflDagPaths.length();
            drawManager.setColor(MColor(0.0, 0.0, 0.0));
            for (unsigned int i = 0; i < lent; i++) {
                bool fillDraw = i == interPersist.biggestInfluence;
                if (i == interPersist.biggestInfluence) {
                    drawManager.setColor(biggestInfluenceColor);
                }
                else {
                    if (i == this->input.influenceIndex) {
                        fillDraw = true;
                    }
                    drawManager.setColor(influence.jointsColors[i]);
                }
                drawingDeformers bbosDfm = interStart.BBoxOfDeformers[i];
                drawManager.box(
                    bbosDfm.center, bbosDfm.up, bbosDfm.right, bbosDfm.width, bbosDfm.height,
                    bbosDfm.depth, fillDraw
                );
            }
            // end reDraw jnts
            // ----------------------------------------------------------------------
            // ---------------------------------------------------------------------------------------
        }

        drawManager.setFontSize(14);
        drawManager.setFontName(MString("MS Shell Dlg 2"));
        drawManager.setFontWeight(1);
        MColor Yellow(1.0, 1.0, 0.0);

        if (this->input.pickMaxInfluenceVal) {
            Yellow = MColor(1.0, 0.5, 0.0);
            if (paint.successfullHit) {
                interPersist.biggestInfluence =
                    getHighestInfluence(interFrame.faceHit, this->paint.centerOfBrush);
            }
            else {
                interPersist.biggestInfluence = -1;
            }
        }
        MString text("--");
        drawManager.setColor(MColor(0.0, 0.0, 0.0));

        int backgroundSize[] = {60, 20};
        if (interPersist.biggestInfluence != -1) {
            text = this->influence.inflNames[interPersist.biggestInfluence];
            interFrame.worldPoint = interFrame.worldPoint + .1 * interFrame.worldVector.normal();
            // A null background size lets Maya fit the background to the text
            drawManager.text(
                interFrame.worldPoint, text, MHWRender::MUIDrawManager::TextAlignment::kCenter,
                nullptr, &Yellow
            );
            // drawing full front camera
        }
        else {
            drawManager.text2d(
                MPoint(this->interFrame.screenX, this->interFrame.screenY, 0.0), text,
                MHWRender::MUIDrawManager::TextAlignment::kCenter, backgroundSize, &Yellow
            );
            // drawing behind bboxes
        }
    }
    else {
        drawManager.circle(this->paint.centerOfBrush, this->interFrame.normalVector, input.sizeVal);
        view.viewToWorld(
            this->interFrame.screenX, this->interFrame.screenY, interFrame.worldPoint,
            interFrame.worldVector
        );

        if (input.paintMirror != 0) { // if mirror is not OFf
            // here paint the mirror Brush
            int faceMirrorHit;
            bool mirroredFound = getMirrorHit(faceMirrorHit, this->mirror.centerOfBrush);
            if (mirroredFound) {
                drawManager.setColor(MColor(0.0, 1.0, 1.0));
                drawManager.circle(
                    this->mirror.centerOfBrush, this->normalMirroredVector, input.sizeVal
                );
            }
        }
    }
    drawManager.endDrawable();
    return MS::kSuccess;
}

MStatus SkinBrushContext::doPressCommon(MEvent &event)
{
    MStatus status = MStatus::kSuccess;

    if (mesh.meshDag.node().isNull()) {
        return MStatus::kNotFound;
    }

    view = M3dView::active3dView();

    if (this->input.pickMaxInfluenceVal || this->input.pickInfluenceVal) {
        this->interStart.BBoxOfDeformers.clear();

        if (this->input.pickMaxInfluenceVal && interPersist.biggestInfluence != -1) {
            MUserEventMessage::postUserEvent("brSkinBrush_influencesReordered");
        }

        if (interPersist.biggestInfluence != this->input.influenceIndex &&
            interPersist.biggestInfluence != -1) {
            setInfluenceIndex(interPersist.biggestInfluence, true); // true for select in UI
        }

        return MStatus::kNotFound;
    }

    // store for undo purposes --------------------------------------------------------------
    // only if painting not after
    if (!this->input.postSetting || input.paintMirror != 0) {
        this->interPersist.fullUndoSkinWeightList = MDoubleArray(this->weights.skinWeightList);
    }
    // update values ------------------------------------------------------------------------
    refreshPointsNormals();

    // first reset attribute to paint values off if we're doing that ------------------------
    paintArrayValues.copy(MDoubleArray(mesh.numVertices, 0.0));
    this->paint.skinValuesToSet.clear();
    this->mirror.skinValuesToSet.clear();
    this->interPersist.verticesPainted.clear();

    // reset values ---------------------------------
    this->paint.intensityValuesOrig = std::vector<float>(this->mesh.numVertices, 0);
    this->mirror.intensityValuesOrig = std::vector<float>(this->mesh.numVertices, 0);
    // initialize --
    interPersist.undersamplingSteps = 0;
    interPersist.performBrush = false;

    event.getPosition(this->interFrame.screenX, this->interFrame.screenY);

    // Get the size of the viewport and calculate the center for placing
    // the value messages when adjusting the brush settings.
    unsigned int x;
    unsigned int y;
    view.viewport(x, y, width, height);
    interStart.viewCenterX = (short)width / 2;
    interStart.viewCenterY = (short)height / 2;

    // Store the initial mouse position. These get used when adjusting
    // the brush size and strength values.
    interStart.startScreenX = this->interFrame.screenX;
    interStart.startScreenY = this->interFrame.screenY;
    interPersist.storedDistance = 0.0; // for the drag screen middle click

    // Reset the adjustment from the previous drag.
    interPersist.initAdjust = false;
    interPersist.sizeAdjust = true;
    interPersist.adjustValue = 0.0;

    // -----------------------------------------------------------------
    // closest point on surface
    // -----------------------------------------------------------------
    // Getting the closest index cannot be performed when in flood mode.

    MStatus mbStat;
    if (event.mouseButton(&mbStat)) {
        // init at false
        paint.successFullDragHit = false;
        mirror.successFullDragHit = false;
        this->paint.dicVertsDistSTART.clear();
        this->weights.mirroredJoinedArray.clear();
        paint.successfullHit = computeHit(
            interFrame.screenX, interFrame.screenY, false, this->interPersist.previousfaceHit,
            this->paint.centerOfBrush
        );
        if (!paint.successfullHit) {
            return MStatus::kNotFound;
        }
        this->paint.AllHitPoints.clear();
        this->mirror.AllHitPoints.clear();

        // we put it inside our world matrix
        this->paint.inMatrixHit = this->paint.centerOfBrush * this->mesh.inclusiveMatrixInverse;
        paint.successfullHit = expandHit(
            this->interPersist.previousfaceHit, this->paint.inMatrixHit,
            this->paint.dicVertsDistSTART
        );

        // mirror part -------------------
        if (input.paintMirror != 0) { // if mirror is not OFf
            this->mirror.dicVertsDistSTART.clear();
            int faceMirrorHit;
            mirror.successfullHit = getMirrorHit(faceMirrorHit, this->mirror.centerOfBrush);
            mesh.meshFn.getPolygonNormal(faceMirrorHit, this->normalMirroredVector, MSpace::kWorld);

            this->mirror.inMatrixHit =
                this->mirror.centerOfBrush * this->mesh.inclusiveMatrixInverse;
            if (mirror.successfullHit) {
                expandHit(faceMirrorHit, this->mirror.inMatrixHit, this->mirror.dicVertsDistSTART);
            }
        }
        // Store the initial surface point and view vector to use when
        // the brush settings are adjusted because the brush circle
        // needs to be static during the adjustment.
        interStart.surfacePointAdjust = this->paint.centerOfBrush;
        interStart.worldVectorAdjust = this->interFrame.worldVector;
    }
    return status;
}

void SkinBrushContext::growArrayOfHitsFromCenters(
    std::unordered_map<int, float> &dicVertsDist, MFloatPointArray &AllHitPoints
)
{
    if (AllHitPoints.length() == 0) {
        return;
    }

    // build hit point list for distance queries
    std::vector<point_t> points;
    points.reserve(AllHitPoints.length());
    for (auto hitPt : AllHitPoints) {
        points.push_back({hitPt.x, hitPt.y, hitPt.z});
    }

    // set of visited vertices
    std::unordered_set<int> vertsVisited;
    for (const auto &element : dicVertsDist) {
        vertsVisited.insert(element.first);
    }

    std::unordered_set<int> borderOfGrowth = vertsVisited;
    bool processing = true;

    while (processing) {
        processing = false;
        // grow one ring out from the current frontier
        std::unordered_set<int> setOfVertsGrow;
        for (int vertexIndex : borderOfGrowth) {
            auto neighbors = getSurroundingVerticesPerVert(vertexIndex);
            setOfVertsGrow.insert(neighbors.begin(), neighbors.end());
        }
        // only consider vertices not yet visited
        std::unordered_set<int> verticesontheborder = setOfVertsGrow - vertsVisited;
        std::vector<int> foundGrowVertsWithinDistance;

        // for all vertices grown
        for (int vertexBorder : verticesontheborder) {
            // First check the normal
            if (!this->coverageVal) {
                MVector vertexBorderNormal = this->mesh.verticesNormals[vertexBorder];
                double multVal = interFrame.worldVector * vertexBorderNormal;
                if (multVal > 0.0) {
                    continue;
                }
            }
            point_t thisPoint = {
                this->mesh.mayaRawPoints[vertexBorder * 3],
                this->mesh.mayaRawPoints[vertexBorder * 3 + 1],
                this->mesh.mayaRawPoints[vertexBorder * 3 + 2]
            };
            // Only the distance to the closest hit point matters, no need to reorder
            float closestDistSq = std::numeric_limits<float>::max();
            for (const point_t &pt : points) {
                closestDistSq = std::min(closestDistSq, distance_sq(pt, thisPoint));
            }
            float closestDist = std::sqrt(closestDistSq);
            if (closestDist <= this->input.sizeVal) {
                processing = true;
                foundGrowVertsWithinDistance.push_back(vertexBorder);
                auto ret = dicVertsDist.insert(std::make_pair(vertexBorder, closestDist));
                if (!ret.second) {
                    ret.first->second = std::min(closestDist, ret.first->second);
                }
            }
        }
        vertsVisited.insert(verticesontheborder.begin(), verticesontheborder.end());
        borderOfGrowth.clear();
        borderOfGrowth.insert(
            foundGrowVertsWithinDistance.begin(), foundGrowVertsWithinDistance.end()
        );
    }
}

MStatus SkinBrushContext::doDragCommon(MEvent &event)
{
    MStatus status = MStatus::kSuccess;

    // -----------------------------------------------------------------
    // Dragging with the left mouse button performs the painting.
    // -----------------------------------------------------------------
    if (event.mouseButton() == MEvent::kLeftMouse) {
        // from previous hit get a line----------
        short previousX = this->interFrame.screenX;
        short previousY = this->interFrame.screenY;
        event.getPosition(this->interFrame.screenX, this->interFrame.screenY);

        // dictionnary of visited vertices and distances --- prefill it with the previous hit ---
        std::unordered_map<int, float> dicVertsDistToGrow = this->paint.dicVertsDistSTART;
        std::unordered_map<int, float> dicVertsDistToGrowMirror = this->mirror.dicVertsDistSTART;

        // for linear growth ----------------------------------
        MFloatPointArray lineHitPoints, lineHitPointsMirror;
        lineHitPoints.append(this->paint.inMatrixHit);
        if (input.paintMirror != 0 && mirror.successfullHit) { // if mirror is not OFf
            lineHitPointsMirror.append(this->mirror.inMatrixHit);
        }
        // --------- LINE OF PIXELS --------------------
        std::vector<std::pair<short, short>> line2dOfPixels;
        // get pixels of the line of pixels
        lineC(
            previousX, previousY, this->interFrame.screenX, this->interFrame.screenY, line2dOfPixels
        );
        int nbPixelsOfLine = (int)line2dOfPixels.size();

        MFloatPoint hitPoint, hitMirrorPoint;
        MFloatPoint hitPointIM, hitMirrorPointIM;
        int faceHit, faceMirrorHit;

        bool successFullHit2 = computeHit(
            this->interFrame.screenX, this->interFrame.screenY, this->input.drawBrushVal, faceHit,
            hitPoint
        );
        bool successFullMirrorHit2 = false;
        if (successFullHit2) {
            // stored in start dic for next call of drag function
            this->interPersist.previousfaceHit = faceHit;
            this->paint.dicVertsDistSTART.clear();
            hitPointIM = hitPoint * this->mesh.inclusiveMatrixInverse;
            expandHit(faceHit, hitPointIM, this->paint.dicVertsDistSTART); // for next beginning

            // If the mirror happens -------------------------
            if (input.paintMirror != 0) { // if mirror is not OFf
                successFullMirrorHit2 = getMirrorHit(faceMirrorHit, hitMirrorPoint);
                if (successFullMirrorHit2) {
                    this->mirror.dicVertsDistSTART.clear();
                    hitMirrorPointIM = hitMirrorPoint * this->mesh.inclusiveMatrixInverse;
                    expandHit(faceMirrorHit, hitMirrorPointIM, this->mirror.dicVertsDistSTART);
                }
            }
        }
        if (!this->paint.successFullDragHit && !successFullHit2) { // moving in empty zone
            return MStatus::kNotFound;
        }
        //////////////////////////////////////////////////////////////////////////////
        this->paint.successFullDragHit = successFullHit2;
        this->mirror.successFullDragHit = successFullMirrorHit2;

        if (this->paint.successFullDragHit) {
            this->paint.centerOfBrush = hitPoint;
            this->paint.inMatrixHit = hitPointIM;
            if (input.paintMirror != 0 && this->mirror.successFullDragHit) {
                this->mirror.centerOfBrush = hitMirrorPoint;
                this->mirror.inMatrixHit = hitMirrorPointIM;
            }
        }
        int incrementValue = 1;
        if (incrementValue < nbPixelsOfLine) {
            for (int i = incrementValue; i < nbPixelsOfLine; i += incrementValue) {
                auto myPair = line2dOfPixels[i];
                short x = myPair.first;
                short y = myPair.second;

                bool successFullHit2 = computeHit(x, y, false, faceHit, hitPoint);
                if (successFullHit2) {
                    hitPointIM = hitPoint * this->mesh.inclusiveMatrixInverse;
                    lineHitPoints.append(hitPointIM);
                    successFullHit2 = expandHit(faceHit, hitPointIM, dicVertsDistToGrow);
                    // mirror part -------------------
                    if (input.paintMirror != 0) { // if mirror is not OFf
                        successFullMirrorHit2 = getMirrorHit(faceMirrorHit, hitMirrorPoint);
                        if (successFullMirrorHit2) {
                            hitMirrorPointIM = hitMirrorPoint * this->mesh.inclusiveMatrixInverse;
                            lineHitPointsMirror.append(hitMirrorPointIM);
                            expandHit(faceMirrorHit, hitMirrorPointIM, dicVertsDistToGrowMirror);
                        }
                    }
                }
            }
        }
        // only now add last hit -------------------------
        if (this->paint.successFullDragHit) {
            lineHitPoints.append(this->paint.inMatrixHit);
            expandHit(faceHit, this->paint.inMatrixHit, dicVertsDistToGrow); // to get closest hit
            if (input.paintMirror != 0 && this->mirror.successFullDragHit) { // if mirror is not OFf
                lineHitPointsMirror.append(this->mirror.inMatrixHit);
                expandHit(faceMirrorHit, this->mirror.inMatrixHit, dicVertsDistToGrowMirror);
            }
        }

        this->interFrame.modifierNoneShiftControl = ModifierKeys::NoModifier;
        if (event.isModifierShift()) {
            if (event.isModifierControl()) {
                this->interFrame.modifierNoneShiftControl = ModifierKeys::ControlShift;
            }
            else {
                this->interFrame.modifierNoneShiftControl = ModifierKeys::Shift;
            }
        }
        else if (event.isModifierControl()) {
            this->interFrame.modifierNoneShiftControl = ModifierKeys::Control;
        }

        // let's expand these arrays to the outer part of the brush----------------
        for (auto hp : lineHitPoints) {
            this->paint.AllHitPoints.append(hp);
        }
        for (auto hp : lineHitPointsMirror) {
            this->mirror.AllHitPoints.append(hp);
        }

        growArrayOfHitsFromCenters(dicVertsDistToGrow, lineHitPoints);
        addBrushShapeFallof(dicVertsDistToGrow);
        preparePaint(
            dicVertsDistToGrow, this->paint.previousPaint, this->paint.intensityValuesOrig,
            this->paint.skinValuesToSet, this->interPersist.verticesPainted, false
        );

        if (input.paintMirror != 0) { // mirror
            growArrayOfHitsFromCenters(dicVertsDistToGrowMirror, lineHitPointsMirror);
            addBrushShapeFallof(dicVertsDistToGrowMirror);
            preparePaint(
                dicVertsDistToGrowMirror, this->mirror.previousPaint,
                this->mirror.intensityValuesOrig, this->mirror.skinValuesToSet,
                this->interPersist.verticesPainted, true
            );
        }
        ::mergeMirrorArray(this->weights, this->paint, this->mirror);
        if (this->input.useColorSetsWhilePainting || !this->input.postSetting) {
            doPerformPaint();
        }
        interPersist.performBrush = true;
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
        interPersist.undersamplingSteps++;
        if (interPersist.undersamplingSteps < input.undersamplingVal) {
            return status;
        }
        interPersist.undersamplingSteps = 0;

        // get screen position
        event.getPosition(this->interFrame.screenX, this->interFrame.screenY);
        // Get the current and initial cursor position and calculate the
        // delta movement from them.
        MPoint currentPos(this->interFrame.screenX, this->interFrame.screenY);
        MPoint startPos(interStart.startScreenX, interStart.startScreenY);
        MVector deltaPos(currentPos - startPos);

        // Switch if the size should get adjusted or the strength based
        // on the drag direction. A drag along the x axis defines size
        // and a drag along the y axis defines strength.
        // InitAdjust makes sure that direction gets set on the first
        // drag event and gets reset the next time a mouse button is
        // pressed.
        if (!interPersist.initAdjust) {
            if (deltaPos.length() < 6) {
                return status; // only if we move at least 6 pixels do we know the direction to
                               // pick !
            }
            interPersist.sizeAdjust = (abs(deltaPos.x) > abs(deltaPos.y));
            interPersist.initAdjust = true;
        }
        // Define the settings for either setting the brush size or the
        // brush strength.
        MString message = "Brush Size";
        MString slider = "Size";
        double dragDistance = deltaPos.x;
        double min = 0.001;
        unsigned int max = 1000;
        double baseValue = input.sizeVal;
        // The adjustment speed depends on the distance to the mesh.
        // Closer distances allows for a feiner control whereas larger
        // distances need a coarser control.
        double speed = pow(0.001 * interFrame.pressDistance, 0.9);

        // Vary the settings if the strength gets adjusted.
        if (!interPersist.sizeAdjust) {
            if (event.isModifierControl()) {
                message = "Smooth Strength";
                baseValue = input.smoothStrengthVal;
            }
            else {
                message = "Brush Strength";
                baseValue = input.strengthVal;
            }
            slider = "Strength";
            dragDistance = deltaPos.y;
            max = 1;
            speed *= 0.1; // smaller for the upd and down
        }
        double prevDist = 0.0;
        // The shift modifier scales the speed for a fine adjustment.
        if (event.isModifierShift()) {
            if (!interFrame.shiftMiddleDrag) {              // if we weren't in shift we reset
                interPersist.storedDistance = dragDistance; // store the pixels to remove
                interFrame.shiftMiddleDrag = true;
            }
            prevDist = interPersist.storedDistance * speed; // store the previsou drag done
            speed *= 0.1;
        }
        else {
            if (interFrame.shiftMiddleDrag) {
                interPersist.storedDistance = dragDistance;
                interFrame.shiftMiddleDrag = false;
            }
            prevDist = interPersist.storedDistance * speed; // store the previous drag done
        }
        dragDistance -= interPersist.storedDistance;

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
        interPersist.adjustValue = value;

        // -------------------------------------------------------------
        // value display in the viewport
        // -------------------------------------------------------------
        short offsetX = interStart.startScreenX - interStart.viewCenterX;
        short offsetY = interStart.startScreenY - interStart.viewCenterY - 50;

        int precision = 2;
        if (event.isModifierShift()) {
            precision = 3;
        }

        std::string stdMessage = std::string(message.asChar());

        std::stringstream stream;
        stream << std::fixed << std::setprecision(precision) << interPersist.adjustValue;

        std::string theMessage = stdMessage + ": " + stream.str();
        std::string headsUpFmt = "headsUpMessage -horizontalOffset " + std::to_string(offsetX) +
                                 " -verticalOffset " + std::to_string(offsetY) + " -time 0.1 \"" +
                                 theMessage + "\"";
        MGlobal::executeCommand(MString(headsUpFmt.c_str(), headsUpFmt.length()));

        // Also, adjust the slider in the tool settings window if it's
        // currently open.
        if (interPersist.sizeAdjust) {
            MUserEventMessage::postUserEvent("brSkinBrush_updateDisplaySize");
        }
        else {
            MUserEventMessage::postUserEvent("brSkinBrush_updateDisplayStrength");
        }
    }
    return status;
}

MStatus SkinBrushContext::doReleaseCommon(MEvent &event)
{
    // Don't continue if no mesh has been set.
    if (mesh.meshFn.object().isNull()) {
        return MS::kFailure;
    }
    if (this->input.pickMaxInfluenceVal || this->input.pickInfluenceVal) {
        this->input.pickMaxInfluenceVal = false;
        this->input.pickInfluenceVal = false;
    }
    this->refreshDone = false;
    // Define, which brush setting has been adjusted and needs to get
    // stored.
    if (event.mouseButton() == MEvent::kMiddleMouse && interPersist.initAdjust) {
        CHECK_MSTATUS_AND_RETURN_SILENT(interPersist.pressStatus);
        if (interPersist.sizeAdjust) {
            input.sizeVal = interPersist.adjustValue;
        }
        else {
            if (event.isModifierControl()) {
                input.smoothStrengthVal = interPersist.adjustValue;
            }
            else {
                input.strengthVal = interPersist.adjustValue;
            }
        }
    }
    if (interPersist.performBrush) {
        doTheAction();
    }
    return MS::kSuccess;
}

void SkinBrushContext::doTheAction()
{
    // If the smoothing has been performed send the current values to
    // the tool command along with the necessary data for undo and redo.
    // The same goes for the select mode.
    // CHECK MIRROR
    if (sewVertices) {
        if (this->vertToVertBorder.size() != (size_t)this->mesh.numVertices) {
            getConnectedBorderVertices();
        }

        std::unordered_map<int, float> sewArr;
        std::set<int> processed;
        bool foundOnce = false;
        for (const auto &element : this->paint.skinValuesToSet) {
            int index = element.first;
            // if (std::find(processed.begin(), processed.end(), index) != processed.end()) {
            if (processed.find(index) != processed.end()) {
                continue;
            }
            float value = element.second;
            int sewnVert = this->vertToVertBorder[index];
            if (sewnVert != -1) {
                if (this->interPersist.verticesPainted.find(sewnVert) ==
                    this->interPersist.verticesPainted.end()) {
                    // if (this->paint.skinValuesToSet.find(sewnVert) ==
                    // this->paint.skinValuesToSet.end()) {
                    sewArr.insert(std::make_pair(sewnVert, value));
                    foundOnce = true;
                }
                else {
                    this->paint.skinValuesToSet[sewnVert] = value;
                    processed.insert(sewnVert);
                }
            }
        }
        if (foundOnce) {
            this->paint.skinValuesToSet.insert(sewArr.begin(), sewArr.end());
            for (const auto &element : sewArr) {
                this->interPersist.verticesPainted.insert(element.first);
            }
        }
        if (this->input.paintMirror != 0) {
            std::unordered_map<int, float> sewArr;
            std::set<int> processed;
            bool foundOnce = false;
            for (const auto &element : this->mirror.skinValuesToSet) {
                int index = element.first;
                if (processed.find(index) != processed.end()) {
                    continue;
                }
                float value = element.second;
                int sewnVert = this->vertToVertBorder[index];
                if (sewnVert != -1) {
                    if (this->interPersist.verticesPainted.find(sewnVert) ==
                        this->interPersist.verticesPainted.end()) {
                        sewArr.insert(std::make_pair(sewnVert, value));
                        foundOnce = true;
                    }
                    else {
                        this->mirror.skinValuesToSet[sewnVert] = value;
                        processed.insert(sewnVert);
                    }
                }
            }
            if (foundOnce) {
                this->mirror.skinValuesToSet.insert(sewArr.begin(), sewArr.end());
                for (const auto &element : sewArr) {
                    this->interPersist.verticesPainted.insert(element.first);
                }
            }
        }
    }
    MColorArray multiEditColors, soloEditColors;
    int nbVerticesPainted = (int)this->interPersist.verticesPainted.size();
    MIntArray editVertsIndices(nbVerticesPainted, 0);
    MIntArray undoLocks, redoLocks;

    MStatus status;
    if (this->influence.lockJoints.length() < this->influence.nbJoints) {
        getListLockJoints(
            weights.skinObj, this->influence.nbJoints, influence.indicesForInfluenceObjects,
            this->influence.lockJoints
        );
        if (this->influence.lockJoints.length() < this->influence.nbJoints) {
            this->influence.lockJoints = MIntArray(this->influence.nbJoints, 0);
        }
    }
    MDoubleArray prevWeights(
        (int)this->interPersist.verticesPainted.size() * this->influence.nbJoints, 0
    );

    std::vector<int> intArray;
    intArray.resize(this->interPersist.verticesPainted.size());

    int i = 0;
    for (const auto &theVert : this->interPersist.verticesPainted) {
        editVertsIndices[i] = theVert;
        i++;
    }

    ModifierCommands theCommandIndex = getCommandIndexModifiers();
    if ((theCommandIndex == ModifierCommands::LockVertices) ||
        (theCommandIndex == ModifierCommands::UnlockVertices)) {
        undoLocks.copy(this->weights.lockVertices);
        bool addLocks = theCommandIndex == ModifierCommands::LockVertices;
        editLocks(this->weights.skinObj, editVertsIndices, addLocks, this->weights.lockVertices);
        redoLocks.copy(this->weights.lockVertices);
    }
    else {
        if (this->input.paintMirror != 0) {
            int mirrorInfluenceIndex = this->input.mirrorInfluences[this->input.influenceIndex];
            ::mergeMirrorArray(this->weights, this->paint, this->mirror);

            if (mirrorInfluenceIndex != this->input.influenceIndex) {
                status = applyCommandMirror();
            }
            else { // we merge in one array, it's easier
                for (const auto &element : this->mirror.skinValuesToSet) {
                    int index = element.first;
                    float value = element.second;

                    auto ret = this->paint.skinValuesToSet.insert(std::make_pair(index, value));
                    if (!ret.second) {
                        ret.first->second = std::max(value, ret.first->second);
                    }
                }
                status = applyCommand(this->input.influenceIndex, this->paint.skinValuesToSet); //
            }
        }
        else if (this->paint.skinValuesToSet.size() > 0) {
            status = applyCommand(this->input.influenceIndex, this->paint.skinValuesToSet); //
            if (status == MStatus::kFailure) {
                MGlobal::displayError(
                    MString("Something went wrong. EXIT the brush and RESTART it")
                );
                return;
            }
        }
        if (!this->input.postSetting) { // only store if not constant setting
            int i = 0;
            for (const auto &theVert : this->interPersist.verticesPainted) {
                for (int j = 0; j < this->influence.nbJoints; ++j) {
                    prevWeights[i * this->influence.nbJoints + j] =
                        this->interPersist
                            .fullUndoSkinWeightList[theVert * this->influence.nbJoints + j];
                }
                i++;
            }
        }
    }
    refreshColors(editVertsIndices, multiEditColors, soloEditColors);
    applyVertexColors(editVertsIndices, multiEditColors, soloEditColors);
    if ((theCommandIndex == ModifierCommands::LockVertices) ||
        (theCommandIndex == ModifierCommands::UnlockVertices)) {
        // without that it doesn't refresh because mesh is not invalidated, meaning the skinCluster
        // hasn't changed
        mesh.meshFn.updateSurface();
    }
    this->paint.skinValuesToSet.clear();
    this->mirror.skinValuesToSet.clear();
    this->paint.previousPaint.clear();
    this->mirror.previousPaint.clear();

    if (!this->interPersist.firstPaintDone) {
        this->interPersist.firstPaintDone = true;
        MUserEventMessage::postUserEvent("brSkinBrush_cleanCloseUndo");
    }

    cmd = (skinBrushTool *)newToolCommand();
    cmd->setColor(input.colorVal);
    cmd->setCurve(input.curveVal);
    cmd->setDrawBrush(input.drawBrushVal);
    cmd->setDrawRange(input.drawRangeVal);
    cmd->setPythonImportPath(input.moduleImportString);
    cmd->setEnterToolCommand(input.enterToolCommandVal);
    cmd->setExitToolCommand(input.exitToolCommandVal);
    cmd->setFractionOversampling(input.fractionOversamplingVal);
    cmd->setIgnoreLock(input.ignoreLockVal);
    cmd->setLineWidth(input.lineWidthVal);
    cmd->setOversampling(input.oversamplingVal);
    cmd->setRange(rangeVal);
    cmd->setSize(input.sizeVal);
    cmd->setStrength(input.strengthVal);
    // storing options for the finalize optionVar
    cmd->setMinColor(input.minSoloColor);
    cmd->setMaxColor(input.maxSoloColor);
    cmd->setSoloColor(input.soloColorVal);
    cmd->setSoloColorType(input.soloColorTypeVal);

    cmd->setPaintMirror(input.paintMirror);
    cmd->setUseColorSetsWhilePainting(input.useColorSetsWhilePainting);
    cmd->setDrawTriangles(input.drawTriangles);
    cmd->setDrawEdges(input.drawEdges);
    cmd->setDrawPoints(input.drawPoints);
    cmd->setDrawTransparency(input.drawTransparency);
    cmd->setPostSetting(input.postSetting);
    cmd->setCoverage(coverageVal);
    cmd->setMessage(input.messageVal);
    cmd->setSmoothRepeat(input.smoothRepeat);

    cmd->setSmoothStrength(input.smoothStrengthVal);
    cmd->setUndersampling(input.undersamplingVal);
    cmd->setVolume(input.volumeVal);
    cmd->setCommandIndex(theCommandIndex);

    cmd->setUndoLocks(undoLocks);
    cmd->setRedoLocks(redoLocks);

    MFnDependencyNode skinDep(this->weights.skinObj);
    MString skinName = skinDep.name();
    cmd->setMesh(mesh.meshDag);

    if (interFrame.isNurbs) {
        cmd->setNurbs(nurbs.nurbsDag);
        cmd->setnumCVInV(nurbs.numCVsInV_);
    }
    cmd->setSkinCluster(weights.skinObj);
    cmd->setIsNurbs(interFrame.isNurbs);

    cmd->setInfluenceIndices(influence.influenceIndices);
    MString iname = getInfluenceName();
    cmd->setInfluenceName(iname);

    cmd->setUndoVertices(editVertsIndices);
    if (!this->input.postSetting) {
        cmd->setWeights(prevWeights);
    }
    else {
        cmd->setWeights(this->interPersist.skinWeightsForUndo);
    }
    cmd->setNormalize(normalize);
    cmd->setContextPointer(this);

    // Regular context implementations usually call
    // (MPxToolCommand)::redoIt at this point but in this case it
    // is not necessary since the the smoothing already has been
    // performed. There is no need to apply the values twice.
    cmd->finalize();
    maya2019RefreshColors();
    MUserEventMessage::postUserEvent("brSkinBrush_afterPaint");
}

ModifierCommands SkinBrushContext::getCommandIndexModifiers() const
{
    return ::getCommandIndexModifiers(this->input, this->interFrame);
}

MStatus SkinBrushContext::applyCommandMirror()
{
    MStatus status;
    std::map<int, std::pair<float, float>> mirroredJoinedArrayOrdered(
        weights.mirroredJoinedArray.begin(), weights.mirroredJoinedArray.end()
    );

    ModifierCommands theCommandIndex = getCommandIndexModifiers();
    double multiplier = 1.0;

    int influence = this->input.influenceIndex;
    int influenceMirror = this->input.mirrorInfluences[this->input.influenceIndex];

    if ((theCommandIndex == ModifierCommands::LockVertices) ||
        (theCommandIndex == ModifierCommands::UnlockVertices)) {
        return MStatus::kSuccess;
    }

    MDoubleArray theWeights((int)this->influence.nbJoints * mirroredJoinedArrayOrdered.size(), 0.0);
    int repeatLimit = 1;
    if (theCommandIndex == ModifierCommands::Smooth ||
        theCommandIndex == ModifierCommands::Sharpen) {
        repeatLimit = this->input.smoothRepeat;
    }

    MIntArray objVertices;
    for (int repeat = 0; repeat < repeatLimit; ++repeat) {
        if (theCommandIndex == ModifierCommands::Smooth) {
            int indexCurrVert = 0;
            for (const auto &elem : mirroredJoinedArrayOrdered) {
                int theVert = elem.first;
                float valueBase = elem.second.first;
                float valueMirror = elem.second.second;
                float biggestValue = std::max(valueBase, valueMirror);

                double theWeight = (double)biggestValue;
                std::vector<int> vertsAround = getSurroundingVerticesPerVert(theVert);

                if (sewVertices) {
                    int sewnVert = this->vertToVertBorder[theVert];
                    if (sewnVert != -1) {
                        std::vector<int> v2 = getSurroundingVerticesPerVert(sewnVert);
                        vertsAround.insert(vertsAround.end(), v2.begin(), v2.end());
                    }
                }

                status = setAverageWeight(
                    vertsAround, theVert, indexCurrVert, this->influence.nbJoints,
                    this->influence.lockJoints, this->weights.skinWeightList, theWeights,
                    this->input.smoothStrengthVal * theWeight
                );
                indexCurrVert++;
            }
        }
        else {
            if (this->input.ignoreLockVal) {
                status = editArrayMirror(
                    theCommandIndex, influence, influenceMirror, this->influence.nbJoints,
                    this->influence.ignoreLockJoints, this->weights.skinWeightList,
                    mirroredJoinedArrayOrdered, theWeights, this->doNormalize, multiplier
                );
            }
            else {
                if (this->influence.lockJoints[influence] == 1 &&
                    theCommandIndex != ModifierCommands::Sharpen) {
                    return status; //  if locked and it's not sharpen --> do nothing
                }
                status = editArrayMirror(
                    theCommandIndex, influence, influenceMirror, this->influence.nbJoints,
                    this->influence.lockJoints, this->weights.skinWeightList,
                    mirroredJoinedArrayOrdered, theWeights, this->doNormalize, multiplier
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

            for (int j = 0; j < this->influence.nbJoints; ++j) {
                int ind_swl = theVert * this->influence.nbJoints + j;
                if (ind_swl >= this->weights.skinWeightList.length()) {
                    this->weights.skinWeightList.setLength(ind_swl + 1);
                }
                double val = 0.0;
                int ind_tw = i * this->influence.nbJoints + j;
                if (ind_tw < theWeights.length()) {
                    val = theWeights[ind_tw];
                }
                this->weights.skinWeightList[ind_swl] = val;
            }
            i++;
        }
    }

    MFnSingleIndexedComponent compFn;
    MObject weightsObj = compFn.create(MFn::kMeshVertComponent);
    compFn.addElements(objVertices);
    MFnSkinCluster skinFn(weights.skinObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    this->interPersist.skinWeightsForUndo.clear();
    if (!interFrame.isNurbs) {
        skinFn.setWeights(
            mesh.meshDag, weightsObj, this->influence.influenceIndices, theWeights, normalize,
            &this->interPersist.skinWeightsForUndo
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
            nurbs.nurbsDag, weightsObjNurbs, this->influence.influenceIndices, theWeights,
            normalize, &this->interPersist.skinWeightsForUndo
        );
        transferPointNurbsToMesh(mesh.meshFn, nurbs.nurbsFn); // we transfer the points postions
        mesh.meshFn.updateSurface();
    }
    refreshPointsNormals();
    return status;
}

MStatus SkinBrushContext::applyCommand(int influence, std::unordered_map<int, float> &valuesToSet)
{
    MStatus status;
    /*
    we need to edit this->verticesPainted for sew vertices, meaning addind the mirror verts
    this->verticesPainted
    */

    // ------------------------------------------
    // we need to sort all of that one way or another ---------------- here it is ------
    std::map<int, double> valuesToSetOrdered(valuesToSet.begin(), valuesToSet.end());

    ModifierCommands theCommandIndex = getCommandIndexModifiers();
    double multiplier = 1.0;

    if ((theCommandIndex != ModifierCommands::LockVertices) &&
        (theCommandIndex != ModifierCommands::UnlockVertices)) {
        MDoubleArray theWeights((int)this->influence.nbJoints * valuesToSetOrdered.size(), 0.0);
        int repeatLimit = 1;
        if (theCommandIndex == ModifierCommands::Smooth ||
            theCommandIndex == ModifierCommands::Sharpen) {
            repeatLimit = this->input.smoothRepeat;
        }

        for (int repeat = 0; repeat < repeatLimit; ++repeat) {
            if (theCommandIndex == ModifierCommands::Smooth) {
                int i = 0;
                for (const auto &elem : valuesToSetOrdered) {
                    int theVert = elem.first;
                    double theWeight = elem.second;
                    std::vector<int> vertsAround = getSurroundingVerticesPerVert(theVert);

                    if (sewVertices) {
                        int sewnVert = this->vertToVertBorder[theVert];
                        if (sewnVert != -1) {
                            std::vector<int> v2 = getSurroundingVerticesPerVert(sewnVert);
                            vertsAround.insert(vertsAround.end(), v2.begin(), v2.end());
                        }
                    }
                    status = setAverageWeight(
                        vertsAround, theVert, i, this->influence.nbJoints,
                        this->influence.lockJoints, this->weights.skinWeightList, theWeights,
                        this->input.smoothStrengthVal * theWeight
                    );
                    i++;
                }
            }
            else {
                if (this->input.ignoreLockVal) {
                    status = editArray(
                        theCommandIndex, influence, this->influence.nbJoints,
                        this->influence.ignoreLockJoints, this->weights.skinWeightList,
                        valuesToSetOrdered, theWeights, this->doNormalize, multiplier
                    );
                }
                else {
                    if (this->influence.lockJoints[influence] == 1 &&
                        theCommandIndex != ModifierCommands::Sharpen) {
                        return status; //  if locked and it's not sharpen --> do nothing
                    }
                    status = editArray(
                        theCommandIndex, influence, this->influence.nbJoints,
                        this->influence.lockJoints, this->weights.skinWeightList,
                        valuesToSetOrdered, theWeights, this->doNormalize, multiplier
                    );
                }
                if (status == MStatus::kFailure) {
                    return status;
                }
            }
            // now set the weights -----------------------------------------------------
            // here we should normalize -----------------------------------------------------
            int i = 0;
            for (const auto &elem : valuesToSetOrdered) {
                int theVert = elem.first;
                for (int j = 0; j < this->influence.nbJoints; ++j) {
                    int ind_swl = theVert * this->influence.nbJoints + j;
                    if (ind_swl >= this->weights.skinWeightList.length()) {
                        this->weights.skinWeightList.setLength(ind_swl + 1);
                    }
                    double val = 0.0;
                    int ind_tw = i * this->influence.nbJoints + j;
                    if (ind_tw < theWeights.length()) {
                        val = theWeights[ind_tw];
                    }
                    this->weights.skinWeightList[ind_swl] = val;
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
        this->interPersist.skinWeightsForUndo.clear();
        if (!interFrame.isNurbs) {
            skinFn.setWeights(
                mesh.meshDag, weightsObj, this->influence.influenceIndices, theWeights, normalize,
                &this->interPersist.skinWeightsForUndo
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
                nurbs.nurbsDag, weightsObjNurbs, this->influence.influenceIndices, theWeights,
                normalize, &this->interPersist.skinWeightsForUndo
            );
            transferPointNurbsToMesh(mesh.meshFn, nurbs.nurbsFn); // we transfer the points postions
        }
        // in do press common
        // update values ---------------
        refreshPointsNormals();
    }
    return status;
}
// ---------------------------------------------------------------------
// COLORS
// ---------------------------------------------------------------------

MStatus SkinBrushContext::editSoloColorSet(bool doBlack)
{
    return ::editSoloColorSet(doBlack, this->input, this->influence, this->weights, this->mesh);
}

void SkinBrushContext::applyVertexColors(
    const MIntArray &indices, const MColorArray &multi, const MColorArray &solo
)
{
    mesh.meshFn.setSomeColors(indices, multi, &interFrame.fullColorSet);
    mesh.meshFn.setSomeColors(indices, solo, &interFrame.soloColorSet);
    mesh.meshFn.setSomeColors(indices, multi, &interFrame.fullColorSet2);
    mesh.meshFn.setSomeColors(indices, solo, &interFrame.soloColorSet2);
}

MStatus SkinBrushContext::refreshColors(
    MIntArray &editVertsIndices, MColorArray &multiEditColors, MColorArray &soloEditColors
)
{
    return ::refreshColors(
        editVertsIndices, multiEditColors, soloEditColors, this->input, this->influence,
        this->weights
    );
}

MColor SkinBrushContext::getASoloColor(double val)
{
    return ::getASoloColor(val, this->input, this->influence);
}

static void copyToFloatMatrix(const MMatrix &src, MFloatMatrix &dst) { src.get(dst.matrix); }

// ---------------------------------------------------------------------
// brush methods
// ---------------------------------------------------------------------
MStatus SkinBrushContext::getDagMesh()
{
    MStatus status = MStatus::kSuccess;
    // Clear the previous data.
    this->mesh.meshDag = MDagPath();
    this->nurbs.nurbsDag = MDagPath();
    this->weights.skinObj = MObject();
    // -----------------------------------------------------------------
    // mesh
    // -----------------------------------------------------------------
    MDagPath dagPath;
    if (getMeshFromName) {
        getDagPath(passedMeshName, this->mesh.meshDag);
        MGlobal::displayInfo(
            MString("direct passed mesh: ") + this->mesh.meshDag.partialPathName()
        );
    }
    else {
        status = getSelection(mesh.meshDag);
    }
    CHECK_MSTATUS_AND_RETURN_IT(status);
    if (mesh.meshDag.apiType() == MFn::kNurbsSurface) { // if is nurbs
        interFrame.isNurbs = true;
        MObject foundMesh;
        findNurbsTesselate(mesh.meshDag, foundMesh);
        // nurbs.nurbsDag = mesh.meshDag;
        status = MDagPath::getAPathTo(foundMesh, mesh.meshDag);
        // MFnNurbsSurface MfnSurface(dagPath);
        status = getSelection(nurbs.nurbsDag);
        nurbs.nurbsFn.setObject(nurbs.nurbsDag);

        nurbs.numCVsInV_ = nurbs.nurbsFn.numCVsInV();
        nurbs.numCVsInU_ = nurbs.nurbsFn.numCVsInU();
        UIsPeriodic_ = nurbs.nurbsFn.formInU() == MFnNurbsSurface::kPeriodic;
        VIsPeriodic_ = nurbs.nurbsFn.formInV() == MFnNurbsSurface::kPeriodic;
        UDeg_ = nurbs.nurbsFn.degreeU();
        VDeg_ = nurbs.nurbsFn.degreeV();
        // int vertInd;
        if (VIsPeriodic_) {
            nurbs.numCVsInV_ -= VDeg_;
        }
        if (UIsPeriodic_) {
            nurbs.numCVsInU_ -= UDeg_;
        }
    }
    else {
        interFrame.isNurbs = false;
    }
    return status;
}
MStatus SkinBrushContext::getObjSkinCluster()
{
    MStatus status = MStatus::kSuccess;
    // Get the skin cluster node from the history of the mesh.
    if (getSkinFromName) {
        getMObject(passedSkinName, this->weights.skinObj);
        MString skinName = getSkinClusterName();
        MGlobal::displayInfo(MString("direct passed skin: ") + skinName);
    }
    else {
        MObject skinClusterObj;
        if (interFrame.isNurbs) {
            status = getSkinCluster(nurbs.nurbsDag, skinClusterObj);
        }
        else {
            status = getSkinCluster(mesh.meshDag, skinClusterObj);
        }
        CHECK_MSTATUS_AND_RETURN_IT(status);
        // Store the skin cluster for undo.
        weights.skinObj = skinClusterObj;
        MString skinName = getSkinClusterName();
        MGlobal::displayInfo(MString("skinned found from shape : ") + skinName);
    }
    return status;
}

MStatus SkinBrushContext::getMesh()
{
    MStatus status = MStatus::kSuccess;
    // get the matrix
    MMatrix MIM = mesh.meshDag.inclusiveMatrix();
    MMatrix MIMI = mesh.meshDag.inclusiveMatrixInverse();

    copyToFloatMatrix(MIM, this->mesh.inclusiveMatrix);
    copyToFloatMatrix(MIMI, this->mesh.inclusiveMatrixInverse);

    // Set the mesh.
    mesh.meshFn.setObject(this->mesh.meshDag);
    mesh.numVertices = (unsigned)mesh.meshFn.numVertices();
    mesh.numFaces = (unsigned)mesh.meshFn.numPolygons();
    mesh.numEdges = (unsigned)mesh.meshFn.numEdges();
    mesh.meshFn.freeCachedIntersectionAccelerator();

    // I dont know why, but '33' seems to work well
    this->mesh.accelParams = mesh.meshFn.uniformGridParams(33, 33, 33);

    // getConnected vertices Guillaume function

    getConnectedVertices();
    getFromMeshNormals();
    getDerivedConnectivity();
    this->mesh.mayaRawPoints = const_cast<float *>(mesh.meshFn.getRawPoints(&status));
    this->weights.lockVertices = MIntArray(this->mesh.numVertices, 0);
    getTheOrigMeshForMirror();
    return status;
}

void SkinBrushContext::secondPartSkincluster()
{

    // Create a component object representing all vertices of the mesh.
    allVtxCompObj = allVertexComponents();
    MFnSingleIndexedComponent compFn;
    // Get the indices of all influences.
    influence.influenceIndices =
        getInfluenceIndices(); // this->weights.skinObj, this->influence.inflDagPaths);

    // Get the skin cluster settings.
    unsigned int normalizeValue;
    getSkinClusterAttributes(weights.skinObj, maxInfluences, maintainMaxInfluences, normalizeValue);
    normalize = (normalizeValue > 0);
}

MStatus SkinBrushContext::swapSkinCluster()
{
    MStatus status = MStatus::kSuccess;

    this->input.pickMaxInfluenceVal = false;
    this->input.pickInfluenceVal = false;

    this->weights.multiCurrentColors.clear();
    this->influence.jointsColors.clear();
    this->weights.soloCurrentColors.clear();

    getMObject(passedSkinName, this->weights.skinObj);
    if (verbose) {
        MGlobal::displayInfo(MString("SWAP skinCluster: ") + passedSkinName);
    }

    unsigned int normalizeValue;
    influence.influenceIndices = getInfluenceIndices();
    if (verbose) {
        MGlobal::displayInfo(MString("nbJoints: ") + this->influence.nbJoints);
    }
    getSkinClusterAttributes(
        weights.skinObj, maxInfluences, maintainMaxInfluences, normalizeValue
    );
    normalize = (normalizeValue > 0);

    if (weights.skinObj.isNull()) {
        MGlobal::displayInfo(MString("FAILED swapSkinCluster: weights.skinObj.isNull"));
        abortAction();
        return MStatus::kFailure;
    }
    this->weights.skinWeightList.clear();
    this->weights.multiCurrentColors.clear();
    this->influence.jointsColors.clear();
    this->weights.soloCurrentColors.clear();
    this->paint.previousPaint.clear();
    this->mirror.previousPaint.clear();

    refresh();
    if (verbose) {
        MGlobal::displayInfo(MString("nbJoints after refresh: ") + this->influence.nbJoints);
    }


    return status;
}

MStatus SkinBrushContext::getTheOrigMeshForMirror()
{
    MStatus status;

    // get the origMesh ----------------------------------------
    MObject origObj;
    if (this->interFrame.isNurbs) {
        findNurbsTesselate(this->mesh.meshDag, origObj, "origMeshNurbs");
    }
    else {
        findOrigMesh(weights.skinObj, origObj);
    }
    status = MDagPath::getAPathTo(origObj, origMeshDag);
    // get the orgi vertices -----------------------------------
    meshOrigFn.setObject(origMeshDag);
    mesh.mayaOrigRawPoints = const_cast<float *>(meshOrigFn.getRawPoints(&status));
    this->mesh.accelParamsOrigMesh = meshOrigFn.uniformGridParams(33, 33, 33);

    MObject origMeshNode = origMeshDag.node();
    status = interStart.intersectorOrigShape.create(origMeshNode); // , matrix);

    // Create the interStart.intersector for the closest point operation for
    // keeping the shells together.
    MObject meshObj = mesh.meshDag.node();
    status = interStart.intersector.create(meshObj, mesh.meshDag.inclusiveMatrix());
    CHECK_MSTATUS_AND_RETURN_IT(status); // only returns if bad
    return status;
}

void SkinBrushContext::getConnectedVertices()
{
    MStatus status;

    MIntArray VertexCountPerPolygon;
    status = mesh.meshFn.getVertices(VertexCountPerPolygon, fullVertexList);

    MIntArray triangleCounts, triangleVertices;
    status = mesh.meshFn.getTriangles(triangleCounts, triangleVertices);

    // face -> vertices
    mesh.perFaceVertices.set(VertexCountPerPolygon, fullVertexList);

    // face -> triangles
    mesh.perFaceTriangles.set(triangleCounts, triangleVertices);

    // vertex -> faces
    std::vector<std::vector<int>> perVertexFaces(mesh.numVertices);
    for (int faceId = 0, iter = 0; faceId < mesh.numFaces; ++faceId) {
        for (int i = 0; i < VertexCountPerPolygon[faceId]; ++i, ++iter) {
            perVertexFaces[fullVertexList[iter]].push_back(faceId);
        }
    }
    mesh.perVertexFaces.set(perVertexFaces);

    // edges + vertex -> edges
    MItMeshEdge edgeIter(mesh.meshDag);
    mesh.perEdgeVertices.clear();
    mesh.perEdgeVertices.resize(mesh.numEdges);
    this->borderVertices.clear();
    std::vector<std::vector<int>> perVertexEdges(mesh.numVertices);

    for (unsigned i = 0; !edgeIter.isDone(); edgeIter.next(), ++i) {
        int pt0 = edgeIter.index(0), pt1 = edgeIter.index(1);
        mesh.perEdgeVertices[i] = {pt0, pt1};
        perVertexEdges[pt0].push_back(i);
        perVertexEdges[pt1].push_back(i);
        if (edgeIter.onBoundary()) {
            borderVertices.push_back(pt0);
            borderVertices.push_back(pt1);
        }
    }
    mesh.perVertexEdges.set(perVertexEdges);
}

void SkinBrushContext::getConnectedBorderVertices()
{
    this->vertToVertBorder = findClosestWithinThreshold(
        borderVertices, this->mesh.mayaOrigRawPoints, mesh.perVertexVertices, sewVerticesMinDist,
        this->mesh.numVertices
    );
    if (verbose) {
        for (size_t i = 0; i < this->vertToVertBorder.size(); ++i) {
            if (this->vertToVertBorder[i] != -1) {
                MGlobal::displayInfo(
                    MString("vertex [") + (int)i + MString("] with vertex [") +
                    this->vertToVertBorder[i] + MString("] ;")
                );
            }
        }
    }
}

void SkinBrushContext::getDerivedConnectivity()
{
    // Build vertex -> face-sharing-vertices, excluding self.
    std::vector<std::unordered_set<int>> vertNeighbors(mesh.numVertices);
    for (int f = 0; f < mesh.numFaces; ++f) {
        for (int v : mesh.perFaceVertices[f]) {
            for (int u : mesh.perFaceVertices[f]) {
                if (v != u) vertNeighbors[v].insert(u);
            }
        }
    }
    std::vector<std::vector<int>> perVertexVertices(mesh.numVertices);
    for (int v = 0; v < mesh.numVertices; ++v) {
        perVertexVertices[v].assign(vertNeighbors[v].begin(), vertNeighbors[v].end());
    }
    mesh.perVertexVertices.set(perVertexVertices);
}

void SkinBrushContext::getFromMeshNormals()
{
    this->mesh.verticesNormals.clear();
    this->mesh.verticesNormals.setLength(this->mesh.numVertices);

    MIntArray normalCounts, normals;
    this->mesh.meshFn.getNormalIds(normalCounts, normals);
    mesh.perFaceNormalIds.set(normalCounts, normals);

    MStatus stat;
    this->mesh.rawNormals = const_cast<float *>(this->mesh.meshFn.getRawNormals(&stat));

    // get vertexNormalIndex --------------------------------------------------
    this->mesh.verticesNormalsIndices.clear();
    this->mesh.verticesNormalsIndices.setLength(mesh.numVertices);
#pragma omp parallel for
    for (int vertexInd = 0; vertexInd < this->mesh.numVertices; ++vertexInd) {
        auto faces = mesh.perVertexFaces[vertexInd];
        if (!faces.empty()) {
            int indFace = faces[0];
            int indNormal = -1;
            auto faceVerts = mesh.perFaceVertices[indFace];
            auto faceNormals = mesh.perFaceNormalIds[indFace];
            for (size_t k = 0; k < faceVerts.size(); ++k) {
                if (faceVerts[k] == vertexInd) {
                    indNormal = faceNormals[k];
                    break;
                }
            }
            if (indNormal == -1) {
                MGlobal::displayInfo(
                    MString("cant find vertex [") + vertexInd + MString("] in face [") + indFace +
                    MString("] ;")
                );
            }
            this->mesh.verticesNormalsIndices.set(indNormal, vertexInd);
        }
    }
}

std::vector<int> SkinBrushContext::getSurroundingVerticesPerVert(int vertexIndex) const
{
    auto sp = ::getSurroundingVerticesPerVert(vertexIndex, this->mesh);
    return {sp.begin(), sp.end()};
}

std::vector<int> SkinBrushContext::getSurroundingVerticesPerFace(int faceIndex) const
{
    auto sp = ::getSurroundingVerticesPerFace(faceIndex, this->mesh);
    return {sp.begin(), sp.end()};
}

//
// Description:
//      Get the dagPath of the currently selected object's shape node.
//      If there are multiple shape nodes return the first
//      non-intermediate shape. Return kNotFound if the object is not a
//      mesh.
//
// Input Arguments:
//      dagPath             The MDagPath of the selected mesh.
//
// Return Value:
//      MStatus             Return kNotFound if nothing is selected or
//                          the selection is not a mesh.
//
MStatus SkinBrushContext::getSelection(MDagPath &dagPath)
{
    MStatus status = MStatus::kSuccess;

    unsigned int i;

    MSelectionList sel;
    status = MGlobal::getActiveSelectionList(sel);

    if (sel.isEmpty()) {
        return MStatus::kNotFound;
    }

    // Get the dagPath of the mesh before evaluating any selected
    // components. If there are no components selected the dagPath would
    // be empty and the command would fail to apply the smoothing to the
    // entire mesh.
    sel.getDagPath(0, dagPath);
    status = dagPath.extendToShape();

    // If there is more than one shape node extend to shape will fail.
    // In this case the shape node needs to be found differently.
    if (status != MStatus::kSuccess) {
        unsigned int numShapes;
        dagPath.numberOfShapesDirectlyBelow(numShapes);
        for (i = 0; i < numShapes; i++) {
            status = dagPath.extendToShapeDirectlyBelow(i);
            if (status == MStatus::kSuccess) {
                MFnDagNode shapeDag(dagPath);
                if (!shapeDag.isIntermediateObject()) {
                    break;
                }
            }
        }
    }

    if (!(dagPath.hasFn(MFn::kMesh) || dagPath.hasFn(MFn::kNurbsSurface))) {
        dagPath = MDagPath();
        MGlobal::displayWarning("Only mesh and nurbs objects are supported.");
        return MStatus::kNotFound;
    }

    if (!status) {
        MGlobal::clearSelectionList();
        dagPath = MDagPath();
    }

    return status;
}

//
// Description:
//      Parse the history of the mesh at the given dagPath and return
//      MObject of the skin cluster node.
//
// Input Arguments:
//      dagPath             The MDagPath of the selected mesh.
//      skinClusterObj      The MObject of the found skin cluster node.
//
// Return Value:
//      MStatus             The MStatus for the setting up the
//                          dependency graph iterator.
//
MStatus SkinBrushContext::getSkinCluster(MDagPath &meshDag, MObject &skinClusterObj)
{
    MStatus status;

    MObject meshObj = meshDag.node();

    MItDependencyGraph dependIter(
        meshObj, MFn::kSkinClusterFilter, MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst, MItDependencyGraph::kPlugLevel, &status
    );
    if (!status) {
        MGlobal::displayError("Failed setting up the dependency graph iterator.");
        return status;
    }

    if (!dependIter.isDone()) {
        skinClusterObj = dependIter.currentItem();
        MFnDependencyNode skinDep(skinClusterObj);
    }

    // Make sure that the mesh is bound to a skin cluster.
    if (skinClusterObj.isNull()) {
        MGlobal::displayWarning("The selected mesh is not bound to a skin cluster.");
        return MStatus::kNotFound;
    }

    return status;
}

MStatus SkinBrushContext::fillArrayValues(MObject &skinCluster, bool doColors)
{
    MStatus status = MS::kSuccess;
    MFnSkinCluster skinFn(skinCluster, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    unsigned int infCount;
    catchTimeStamp();
    if (!interFrame.isNurbs) {
        status =
            skinFn.getWeights(mesh.meshDag, allVtxCompObj, this->weights.skinWeightList, infCount);
    }
    else {
        status = skinFn.getWeights(
            nurbs.nurbsDag, allVtxCompObj, this->weights.skinWeightList, infCount
        );
    }
    this->skipSkinValues = false; // make sure we got the skin values
    endTimeStamp(MString("skinFn.getWeights"));
    catchTimeStamp();

    CHECK_MSTATUS_AND_RETURN_IT(status);
    this->influence.nbJoints = infCount;

    // quickly the ignore locks
    this->influence.ignoreLockJoints.clear();
    this->influence.ignoreLockJoints = MIntArray(this->influence.nbJoints, 0);

    if (doColors) {
        this->weights.multiCurrentColors.clear();
        this->weights.multiCurrentColors.setLength(this->mesh.numVertices);
        // get values for array --
        for (unsigned int vertexIndex = 0; vertexIndex < this->mesh.numVertices; ++vertexIndex) {
            MColor theColor(0.0, 0.0, 0.0);
            for (unsigned int indexInfluence = 0; indexInfluence < infCount;
                 indexInfluence++) { // for each joint

                int ind_swl = vertexIndex * infCount + indexInfluence;
                double theWeight = 0.0;
                if (ind_swl < this->weights.skinWeightList.length()) {
                    theWeight = this->weights.skinWeightList[ind_swl];
                }

                if (doColors) {
                    if (influence.lockJoints[indexInfluence] == 1) {
                        theColor += influence.lockJntColor * theWeight;
                    }
                    else {
                        theColor += this->influence.jointsColors[indexInfluence] * theWeight;
                    }
                }
            }
            if (doColors) { // not store lock vert color
                this->weights.multiCurrentColors[vertexIndex] = theColor;
            }
        }
    }
    endTimeStamp(MString("fillArrayValues colors"));

    return status;
}

MStatus SkinBrushContext::fillArrayValuesDEP(MObject &skinCluster, bool doColors)
{
    MStatus status = MS::kSuccess;

    MFnDependencyNode skinClusterDep(skinCluster);

    MPlug weight_list_plug = skinClusterDep.findPlug("weightList", false);
    MPlug matrix_plug = skinClusterDep.findPlug("matrix", false);
    // MGlobal::displayInfo(weight_list_plug.name());
    int nbElements = weight_list_plug.numElements();
    unsigned int infCount = matrix_plug.numElements();

    matrix_plug.getExistingArrayAttributeIndices(this->deformersIndices);

    this->nbJointsBig = 0;
    for (int el : this->deformersIndices) {
        if (el > this->nbJointsBig) {
            this->nbJointsBig = el;
        }
    }
    this->nbJointsBig += 1;
    this->influence.nbJoints = infCount;

    // For the first component, the weights are ordered by influence object in the same order that
    // is returned by the MFnSkinCluster::influenceObjects method.
    // use influence.influenceIndices
    if (doColors) {
        this->weights.multiCurrentColors.clear();
        this->weights.multiCurrentColors.setLength(nbElements);
    }
    this->weights.skinWeightList = MDoubleArray(nbElements * this->influence.nbJoints, 0.0);

// MThreadUtils::syncNumOpenMPThreads();
// in option C/C++ turn omp on !!!!
#pragma omp parallel for // collapse(2)
    for (int i = 0; i < nbElements; ++i) {
        // weightList[i]
        MPlug ith_weights_plug = weight_list_plug.elementByPhysicalIndex(i);
        int vertexIndex = ith_weights_plug.logicalIndex();

        // weightList[i].weight
        MPlug plug_weights = ith_weights_plug.child(0); // access first compound child
        int nb_weights = plug_weights.numElements();

        MColor theColor(0, 0, 0, 1);
        for (int j = 0; j < nb_weights; j++) { // for each joint
            MPlug weight_plug = plug_weights.elementByPhysicalIndex(j);
            // weightList[i].weight[j]
            int indexInfluence = weight_plug.logicalIndex();
            double theWeight = weight_plug.asDouble();
            // store in the correct Spot --
            indexInfluence = this->influence.indicesForInfluenceObjects[indexInfluence];
            int ind_swl = vertexIndex * this->influence.nbJoints + indexInfluence;
            if (ind_swl >= this->weights.skinWeightList.length()) {
                this->weights.skinWeightList.setLength(ind_swl + 1);
            }
            this->weights.skinWeightList[ind_swl] = theWeight;
            if (doColors) { // and not locked
                if (this->influence.lockJoints[indexInfluence] == 1) {
                    theColor += influence.lockJntColor * theWeight;
                }
                else {
                    theColor += this->influence.jointsColors[indexInfluence] * theWeight;
                }
            }
        }
        if (doColors) { // not store lock vert color
            this->weights.multiCurrentColors[vertexIndex] = theColor;
        }
    }
    return status;
}
//
// Description:
//      Return the influence indices of all influences of the given
//      skin cluster node.
//
// Input Arguments:
//      skinCluster         The MObject of the skin cluster node.
//
// Return Value:
//      int array           The array of all influence indices.
//
MIntArray SkinBrushContext::getInfluenceIndices()
{
    MFnSkinCluster skinFn(this->weights.skinObj);

    this->influence.influenceIndices.clear();
    this->influence.inflDagPaths.clear();
    skinFn.influenceObjects(this->influence.inflDagPaths);
    int lent = this->influence.inflDagPaths.length();
    // first clear --------------------------
    this->influence.inflNames.clear();
    this->influence.indicesForInfluenceObjects.clear();

    this->influence.inflNames.setLength(lent);
    MStatus stat;
    this->influence.nbJoints = lent;

    // Logical indices can be sparse (eg: after removing an influence), so size the
    // logical->physical map by the largest logical index. Unused entries map to -1
    unsigned int maxLogical = 0;
    for (unsigned i = 0; i < lent; i++) {
        maxLogical = std::max(
            maxLogical, skinFn.indexForInfluenceObject(this->influence.inflDagPaths[i])
        );
    }
    this->influence.indicesForInfluenceObjects = MIntArray(lent ? maxLogical + 1 : 0, -1);

    for (unsigned i = 0; i < lent; i++) {
        influence.influenceIndices.append((int)i);
        MFnDependencyNode influenceFn(this->influence.inflDagPaths[i].node(), &stat);
        if (stat != MS::kSuccess) {
            MGlobal::displayError(MString("Crashing query influence ") + i);
        }
        MString iname = influenceFn.name();
        this->influence.inflNames[i] = iname;

        int indexLogical = skinFn.indexForInfluenceObject(this->influence.inflDagPaths[i]);
        this->influence.indicesForInfluenceObjects[indexLogical] = i;
    }
    return influence.influenceIndices;
}

MStatus SkinBrushContext::querySkinClusterValues(
    MObject &skinCluster, MIntArray &verticesIndices, MDoubleArray &theSkinWeightList, bool doColors
) const
{
    MStatus status = MS::kSuccess;

    if (mesh.meshDag.node().isNull()) {
        return MStatus::kNotFound;
    }

    MFnSkinCluster skinFn(skinCluster, &status);
    MDoubleArray weightsVertices;
    unsigned int infCount;

    if (!interFrame.isNurbs) {
        MFnSingleIndexedComponent compFn;
        MObject weightsObj = compFn.create(MFn::kMeshVertComponent);
        compFn.addElements(verticesIndices);
        status = skinFn.getWeights(mesh.meshDag, weightsObj, weightsVertices, infCount);
    }
    else {
        MFnDoubleIndexedComponent doubleFn;
        MObject weightsObjNurbs = doubleFn.create(MFn::kSurfaceCVComponent);
        int uVal, vVal;
        for (int vert : verticesIndices) {
            vVal = (int)vert % (int)nurbs.numCVsInV_;
            uVal = (int)vert / (int)nurbs.numCVsInV_;
            doubleFn.addElement(uVal, vVal);
        }
        status = skinFn.getWeights(nurbs.nurbsDag, weightsObjNurbs, weightsVertices, infCount);
    }
    if (status != MS::kSuccess) {
        MGlobal::displayError("querySkinClusterValues | can't Query skin values \n");
    }

    for (unsigned int i = 0; i < verticesIndices.length(); ++i) {
        int vertexIndex = verticesIndices[i];
        for (unsigned int j = 0; j < infCount; j++) { // for each joint
            double theWeight = weightsVertices[i * infCount + j];
            int ind_swl = vertexIndex * this->influence.nbJoints + j;
            if (ind_swl >= (int)theSkinWeightList.length()) {
                theSkinWeightList.setLength(ind_swl + 1);
            }
            theSkinWeightList[ind_swl] = theWeight;
        }
    }
    return status;
}

//
// Description:
//      Get the influence attributes from the given skin cluster object.
//
// Input Arguments:
//      skinCluster             The MObject of the skin cluster node.
//      maxInfluences           The max number of influences per vertex.
//      maintainMaxInfluences   True, if the max influences count should
//                              be maintained.
//      normalize               True, if the weights are normalized.
//
// Return Value:
//      None
//
void SkinBrushContext::getSkinClusterAttributes(
    MObject &skinCluster, unsigned int &maxInfluences, bool &maintainMaxInfluences,
    unsigned int &normalize
) const
{
    // Get the settings from the skin cluster node.
    MFnDependencyNode skinMFn(skinCluster);

    MPlug maxInflPlug = skinMFn.findPlug("maxInfluences", false);
    maxInfluences = (unsigned)maxInflPlug.asInt();

    MPlug maintainInflPlug = skinMFn.findPlug("maintainMaxInfluences", false);
    maintainMaxInfluences = maintainInflPlug.asBool();

    MPlug normalizePlug = skinMFn.findPlug("normalizeWeights", false);
    normalize = (unsigned)normalizePlug.asInt();
}

bool SkinBrushContext::getMirrorHit(int &faceHit, MFloatPoint &hitPoint) const
{
    MStatus stat;

    MMatrix mirrorMatrix;
    double XVal = 1., YVal = 1.0, ZVal = 1.0;
    if ((input.paintMirror == 1) || (input.paintMirror == 4) || (input.paintMirror == 7)) {
        XVal = -1.;
    }
    if ((input.paintMirror == 2) || (input.paintMirror == 5) || (input.paintMirror == 8)) {
        YVal = -1.;
    }
    if ((input.paintMirror == 3) || (input.paintMirror == 6) || (input.paintMirror == 9)) {
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
    if (input.paintMirror > 0 && input.paintMirror < 4) { // if we compute the orig mesh
        MPoint pointToMirror = MPoint(this->interFrame.origHitPoint);
        MPoint mirrorPoint = pointToMirror * mirrorMatrix;

        stat = interStart.intersectorOrigShape.getClosestPoint(
            mirrorPoint, pointInfo, input.mirrorMinDist
        );
        if (MS::kSuccess != stat) {
            return false;
        }

        faceHit = pointInfo.faceIndex();
        int hitTriangle = pointInfo.triangleIndex();
        float hitBary1, hitBary2;
        pointInfo.getBarycentricCoords(hitBary1, hitBary2);

        auto tri = mesh.perFaceTriangles(faceHit, hitTriangle);
        int t0 = tri[0], t1 = tri[1], t2 = tri[2];

        hitPoint =
            barycentricInterpolate(this->mesh.mayaRawPoints, t0, t1, t2, hitBary1, hitBary2) *
            this->mesh.inclusiveMatrix;
    }
    else {
        MPoint mirrorPoint = MPoint(this->paint.centerOfBrush) * mirrorMatrix;
        stat = interStart.intersector.getClosestPoint(mirrorPoint, pointInfo, input.mirrorMinDist);
        if (MS::kSuccess != stat) {
            return false;
        }

        faceHit = pointInfo.faceIndex();
        hitPoint = MFloatPoint(mirrorPoint);
    }
    return true;
}

bool SkinBrushContext::computeHit(
    short screenPixelX, short screenPixelY, bool getNormal, int &faceHit, MFloatPoint &hitPoint
)
{
    MStatus stat;

    view.viewToWorld(screenPixelX, screenPixelY, interFrame.worldPoint, interFrame.worldVector);

    // float hitRayParam;
    float hitBary1;
    float hitBary2;
    int hitTriangle;
    // If v1, v2, and v3 vertices of that triangle,
    // then the barycentric coordinates are such that
    // interFrame.hitPoint = (*hitBary1)*v1 + (*hitBary2)*v2 + (1 - *hitBary1 - *hitBary2)*v3;
    // If no hit was found, the referenced value will not be modified,

    bool foundIntersect = mesh.meshFn.closestIntersection(
        interFrame.worldPoint, interFrame.worldVector, nullptr, nullptr, false, MSpace::kWorld,
        9999, false, &this->mesh.accelParams, hitPoint, &this->interFrame.pressDistance, &faceHit,
        &hitTriangle, &hitBary1, &hitBary2, 0.0001f, &stat
    );

    if (!foundIntersect) {
        return false;
    }

    if (input.paintMirror > 0 && input.paintMirror < 4) { // if we compute the orig
        auto tri = mesh.perFaceTriangles(faceHit, hitTriangle);
        int t0 = tri[0], t1 = tri[1], t2 = tri[2];
        interFrame.origHitPoint =
            barycentricInterpolate(this->mesh.mayaOrigRawPoints, t0, t1, t2, hitBary1, hitBary2);
    }

    // ----------- get normal for display ---------------------
    if (getNormal) {
        mesh.meshFn.getPolygonNormal(faceHit, this->interFrame.normalVector, MSpace::kWorld);
    }
    return true;
}

bool SkinBrushContext::expandHit(
    int faceHit, MFloatPoint &hitPoint, std::unordered_map<int, float> &dicVertsDist
) const
{
    // ----------- compute the vertices around ---------------------
    auto verticesSet = getSurroundingVerticesPerFace(faceHit);
    bool foundHit = false;
    for (int ptIndex : verticesSet) {
        MFloatPoint posPoint(
            this->mesh.mayaRawPoints[ptIndex * 3], this->mesh.mayaRawPoints[ptIndex * 3 + 1],
            this->mesh.mayaRawPoints[ptIndex * 3 + 2]
        );
        float dist = posPoint.distanceTo(hitPoint);
        if (dist <= this->input.sizeVal) {
            foundHit = true;
            auto ret = dicVertsDist.insert(std::make_pair(ptIndex, dist));
            if (!ret.second) {
                ret.first->second = std::min(dist, ret.first->second);
            }
        }
    }
    return foundHit;
}

// Just get a reference to the function for reuse
double fo_noFalloff(double v, double s) { return s; };
double fo_linearFalloff(double v, double s) { return v * s; };
double fo_smoothstepFalloff(double v, double s) { return (v * v * (3 - 2 * v)) * s; };
double fo_narrowFalloff(double v, double s) { return (1 - pow((1 - v) / 1, 0.4)) * s; };
double fo_defaultFalloff(double v, double s) { return v; };

void SkinBrushContext::addBrushShapeFallof(std::unordered_map<int, float> &dicVertsDist) const
{
    double valueStrength = input.strengthVal;
    if (this->interFrame.modifierNoneShiftControl == ModifierKeys::ControlShift ||
        this->input.commandIndex == ModifierCommands::Smooth) {
        valueStrength = input.smoothStrengthVal; // smooth always we use the smooth value different
                                                 // of the regular value
    }

    if (input.fractionOversamplingVal) {
        valueStrength /= input.oversamplingVal;
    }

    // PROFILED HOT PATH
    // Should be faster than putting this switch case in the tight loop
    // if not, then we could just make a bunch of tight loops
    double (*fo_pointer)(double, double);
    switch (input.curveVal) {
    case 0:
        fo_pointer = fo_noFalloff;
        break;
    case 1:
        fo_pointer = fo_linearFalloff;
        break;
    case 2:
        fo_pointer = fo_smoothstepFalloff;
        break;
    case 3:
        fo_pointer = fo_narrowFalloff;
        break;
    default:
        fo_pointer = fo_defaultFalloff;
        break;
    }

    double svi = 1.0 / this->input.sizeVal;
    for (auto &element : dicVertsDist) {
        double val = 1.0 - (element.second * svi);
        element.second = (float)fo_pointer(val, valueStrength);
    }
}

void SkinBrushContext::getColorWithMirror(
    int vertexIndex, float valueBase, float valueMirror, MColor &multColor, MColor &soloColor
)
{
    ::getColorWithMirror(
        vertexIndex, valueBase, valueMirror, multColor, soloColor, this->input, this->influence,
        this->weights, this->interFrame
    );
}

void SkinBrushContext::preparePaint(
    std::unordered_map<int, float> &dicVertsDist,
    std::unordered_map<int, float> &dicVertsDistPrevPaint, std::vector<float> &intensityValues,
    std::unordered_map<int, float> &skinValToSet, std::set<int> &theVerticesPainted, bool mirror
)
{

    // MGlobal::displayInfo("perform Paint");
    double multiplier = 1.0;
    if (!input.postSetting && input.commandIndex != ModifierCommands::Smooth) {
        multiplier = .1; // less applying if dragging paint
    }

    bool isCommandLock = ((input.commandIndex == ModifierCommands::LockVertices) ||
                          (input.commandIndex == ModifierCommands::UnlockVertices)) &&
                         (this->interFrame.modifierNoneShiftControl != ModifierKeys::Control);

    auto endOfFind = dicVertsDistPrevPaint.end();
    for (const auto &element : dicVertsDist) {
        int index = element.first;
        float value = element.second * multiplier;
        // check if need to set this color, we store in intensityValues to check if it's already at
        // 1 -------
        if ((this->weights.lockVertices[index] == 1 && !isCommandLock) ||
            intensityValues[index] == 1) {
            continue;
        }
        // get the correct value of paint by adding this value -----
        value += intensityValues[index];
        auto res = dicVertsDistPrevPaint.find(index);
        if (res != endOfFind) { // we substract the smallest
            value -= std::min(res->second, element.second);
        }
        value = std::min(value, (float)1.0);
        intensityValues[index] = value;

        // add to array of values to set at the end---------------
        // we need to check if it is in the regular array and make adjustements
        auto ret = skinValToSet.insert(std::make_pair(index, value));
        if (!ret.second) {
            ret.first->second = std::max(value, ret.first->second);
        }
        else {
            theVerticesPainted.insert(index);
        }
        // end add to array of values to set at the end--------------------
    }
    dicVertsDistPrevPaint = dicVertsDist;

    if (!this->input.postSetting) {
        // MGlobal::displayInfo("apply the skin stuff");
        // still have to deal with the colors damn it
        if (skinValToSet.size() > 0) {
            int theInfluence = this->input.influenceIndex;
            if (mirror) {
                theInfluence = this->input.mirrorInfluences[this->input.influenceIndex];
            }
            applyCommand(theInfluence, skinValToSet);
            intensityValues = std::vector<float>(this->mesh.numVertices, 0);
            dicVertsDistPrevPaint.clear();
            skinValToSet.clear();
        }
    }
}

MStatus SkinBrushContext::doPerformPaint()
{
    return ::doPerformPaint(
        this->input.postSetting, this->toggleColorState, this->input, this->influence,
        this->weights, this->interFrame, this->mesh
    );
}

//
// Description:
//      Return a component MObject for all vertex components of the
//      given mesh.
//
// Input Arguments:
//      mesh.meshDag             The dagPath of the mesh object.
//
// Return Value:
//      MObject             The component object for all mesh vertices.
//
MObject SkinBrushContext::allVertexComponents()
{
    MObject vtxComponents;
    if (!interFrame.isNurbs) {
        MFnSingleIndexedComponent compFn;
        vtxComponents = compFn.create(MFn::kMeshVertComponent);
        compFn.setCompleteData((int)mesh.numVertices);
    }
    else {
        MFnDoubleIndexedComponent allCVs;
        int sizeInV = nurbs.nurbsFn.numCVsInV();
        int sizeInU = nurbs.nurbsFn.numCVsInU();

        vtxComponents = allCVs.create(MFn::kSurfaceCVComponent);
        allCVs.setCompleteData(sizeInU, sizeInV);
    }
    return vtxComponents;
}

//
// Description:
//      Calculate the brush weight value based on the given linear
//      falloff value.
//
// Input Arguments:
//      value               The linear falloff value.
//      strength            The brush strength value.
//
// Return Value:
//      double              The brush curve-based falloff value.
//

void SkinBrushContext::setInViewMessage(bool display) const
{
    if (display && input.messageVal) {
        MString cmd = "inViewMessage -position topCenter -statusMessage \""
                      "<hl>LMB</hl> to add  |  "
                      "<hl>MMB</hl> to adjust  |  ";
        if (input.smoothModifier == ModifierKeys::Shift) {
            cmd += "<hl>Ctrl</hl> to remove  |  "
                   "<hl>Shift</hl> to smooth"
                   "\"";
        }
        else {
            cmd += "<hl>Ctrl</hl> to smooth  |  "
                   "<hl>Shift</hl> to remove"
                   "\"";
        }
        MGlobal::executeCommand(cmd);
    }
    else {
        MGlobal::executeCommand("inViewMessage -clear topCenter");
    }
}

MString SkinBrushContext::getValuesForOptionVar()
{
    // Store the current settings as an option var. This way they are
    // properly available for the next usage.
    // MGlobal::displayInfo("skinBrushTool::finalize\n");
    MString cmd;
    cmd = "";
    cmd += " " + MString(kCurveFlagLong) + " ";
    cmd += getCurve();
    cmd += " " + MString(kCommandIndexFlagLong) + " ";
    cmd += static_cast<int>(input.commandIndex);
    cmd += " " + MString(kSoloColorFlagLong) + " ";
    cmd += getSoloColor();
    cmd += " " + MString(kSoloColorTypeFlagLong) + " ";
    cmd += getSoloColorType();
    cmd += " " + MString(kCoverageLong) + " ";
    cmd += getCoverage();
    cmd += " " + MString(kMessageFlagLong) + " ";
    cmd += getMessage();
    cmd += " " + MString(kDrawBrushFlagLong) + " ";
    cmd += getDrawBrush();
    cmd += " " + MString(kDrawRangeFlagLong) + " ";
    cmd += getDrawRange();
    cmd += " " + MString(kFractionOversamplingFlagLong) + " ";
    cmd += getFractionOversampling();
    cmd += " " + MString(kIgnoreLockFlagLong) + " ";
    cmd += getIgnoreLock();
    cmd += " " + MString(kLineWidthFlagLong) + " ";
    cmd += getLineWidth();
    cmd += " " + MString(kOversamplingFlagLong) + " ";
    cmd += getOversampling();
    cmd += " " + MString(kRangeFlagLong) + " ";
    cmd += getRange();
    cmd += " " + MString(kSizeFlagLong) + " ";
    cmd += getSize();
    cmd += " " + MString(kStrengthFlagLong) + " ";
    cmd += getStrength();
    cmd += " " + MString(kSmoothStrengthFlagLong) + " ";
    cmd += getSmoothStrength();
    // cmd += " " + MString(kPruneWeightsFlagLong) + " ";
    // cmd += pruneWeights;
    cmd += " " + MString(kUndersamplingFlagLong) + " ";
    cmd += getUndersampling();
    cmd += " " + MString(kVolumeFlagLong) + " ";
    cmd += getVolume();
    cmd += " " + MString(kPostSettingFlagLong) + " ";
    cmd += getPostSetting();
    cmd += " " + MString(kInfluenceNameFlagLong) + " ";
    cmd += getInfluenceName();
    cmd += " " + MString(kSmoothRepeatFlagLong) + " ";
    cmd += getSmoothRepeat();
    cmd += " " + MString(kPaintMirrorFlagLong) + " ";
    cmd += getPaintMirror();
    cmd += " " + MString(kPaintMirrorToleranceFlagLong) + " ";
    cmd += getMirrorTolerance();
    cmd += " " + MString(kUseColorSetWhilePaintingFlagLong) + " ";
    cmd += getUseColorSetsWhilePainting();
    cmd += " " + MString(kMeshDragDrawTrianglesFlagLong) + " ";
    cmd += getDrawTriangles();
    cmd += " " + MString(kMeshDragDrawEdgesFlagLong) + " ";
    cmd += getDrawEdges();
    cmd += " " + MString(kMeshDragDrawPointsFlagLong) + " ";
    cmd += getDrawPoints();
    cmd += " " + MString(kMeshDragDrawTransFlagLong) + " ";
    cmd += getDrawTransparency();
    cmd += " " + MString(kMinColorFlagLong) + " ";
    cmd += getMinColor();
    cmd += " " + MString(kMaxColorFlagLong) + " ";
    cmd += getMaxColor();
    cmd += " " + MString(kSewVerticesFlagLong) + " ";
    cmd += getSewVertices();
    cmd += " " + MString(kSewVerticesOffsetFlagLong) + " ";
    cmd += getSewVerticesOffset();
    cmd += " " + MString(kSkinClusterNameFlagLong) + " ";
    cmd += getSkinClusterName();
    cmd += " " + MString(kMeshNameFlagLong) + " ";
    cmd += getMeshName();
    cmd += " " + MString(kVerboseFlagLong) + " ";
    cmd += verbose;
    cmd += " " + MString(kFastReEnterFlagLong) + " ";
    cmd += getFastReenter();

    return cmd;
}

void SkinBrushContext::storeValuesInOptionVar(MString nameOptionVar)
{
    MString cmd = getValuesForOptionVar();
    MGlobal::setOptionVarValue(nameOptionVar, cmd);
}
