#include "skinBrushTool.h"

#include "skinBrushFlags.h"

// Macro for the press/drag/release methods in case there is nothing
// selected or the tool gets applied outside any geometry. If the actual
// MStatus would get returned an error can get listed in terminal on
// Linux. But it's unnecessary and needs to be avoided. Therefore a
// kSuccess is returned just for the sake of being invisible.
#define CHECK_MSTATUS_AND_RETURN_SILENT(status)                                                    \
    if (status != MStatus::kSuccess)                                                               \
        return MStatus::kSuccess;

// ---------------------------------------------------------------------
// the tool
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// general methods for the tool command
// ---------------------------------------------------------------------

skinBrushTool::skinBrushTool()
{
    setCommandString("brSkinBrushCmd");

    input.colorVal = MColor(1.0, 0.0, 0.0);
    input.curveVal = 2;
    input.drawBrushVal = true;
    input.drawRangeVal = true;
    input.moduleImportString = MString("from brSkinBrush_pythonFunctions import ");
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
    input.undersamplingVal = 2;
    input.volumeVal = false;
    coverageVal = true;

    input.commandIndex = ModifierCommands::Add;
    input.soloColorTypeVal = 1; // 1 lava
    input.soloColorVal = 0;
    input.postSetting = true;
}

skinBrushTool::~skinBrushTool() {}

void *skinBrushTool::creator() { return new skinBrushTool; }

bool skinBrushTool::isUndoable() const { return true; }

MSyntax skinBrushTool::newSyntax()
{
    MSyntax syntax;

    syntax.addFlag(kColorRFlag, kColorRFlagLong, MSyntax::kDouble);
    syntax.addFlag(kColorGFlag, kColorGFlagLong, MSyntax::kDouble);
    syntax.addFlag(kColorBFlag, kColorBFlagLong, MSyntax::kDouble);
    syntax.addFlag(kCurveFlag, kCurveFlagLong, MSyntax::kLong);
    syntax.addFlag(kDrawBrushFlag, kDrawBrushFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kDrawRangeFlag, kDrawRangeFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kImportPythonFlag, kImportPythonFlagLong, MSyntax::kString);
    syntax.addFlag(kEnterToolCommandFlag, kEnterToolCommandFlagLong, MSyntax::kString);
    syntax.addFlag(kExitToolCommandFlag, kExitToolCommandFlagLong, MSyntax::kString);
    syntax.addFlag(kFractionOversamplingFlag, kFractionOversamplingFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kIgnoreLockFlag, kIgnoreLockFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kLineWidthFlag, kLineWidthFlagLong, MSyntax::kLong);
    syntax.addFlag(kMessageFlag, kMessageFlagLong, MSyntax::kLong);
    syntax.addFlag(kOversamplingFlag, kOversamplingFlagLong, MSyntax::kLong);
    syntax.addFlag(kRangeFlag, kRangeFlagLong, MSyntax::kDouble);
    syntax.addFlag(kSizeFlag, kSizeFlagLong, MSyntax::kDouble);
    syntax.addFlag(kStrengthFlag, kStrengthFlagLong, MSyntax::kDouble);
    syntax.addFlag(kUndersamplingFlag, kUndersamplingFlagLong, MSyntax::kLong);
    syntax.addFlag(kVolumeFlag, kVolumeFlagLong, MSyntax::kBoolean);

    syntax.addFlag(kSmoothStrengthFlag, kSmoothStrengthFlagLong, MSyntax::kDouble);
    syntax.addFlag(kCommandIndexFlag, kCommandIndexFlagLong, MSyntax::kLong);
    syntax.addFlag(kSoloColorFlag, kSoloColorFlagLong, MSyntax::kLong);
    syntax.addFlag(kSoloColorTypeFlag, kSoloColorTypeFlagLong, MSyntax::kLong);
    syntax.addFlag(kCoverageFlag, kCoverageLong, MSyntax::kBoolean);

    syntax.addFlag(kPaintMirrorToleranceFlag, kPaintMirrorToleranceFlagLong, MSyntax::kDouble);
    syntax.addFlag(kPaintMirrorFlag, kPaintMirrorFlagLong, MSyntax::kLong);
    syntax.addFlag(
        kUseColorSetWhilePaintingFlag, kUseColorSetWhilePaintingFlagLong, MSyntax::kBoolean
    );
    syntax.addFlag(kMeshDragDrawTrianglesFlag, kMeshDragDrawTrianglesFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kMeshDragDrawEdgesFlag, kMeshDragDrawEdgesFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kMeshDragDrawPointsFlag, kMeshDragDrawPointsFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kMeshDragDrawTransFlag, kMeshDragDrawTransFlagLong, MSyntax::kBoolean);

    syntax.addFlag(kMinColorFlag, kMinColorFlagLong, MSyntax::kDouble);
    syntax.addFlag(kMaxColorFlag, kMaxColorFlagLong, MSyntax::kDouble);

    syntax.addFlag(kSmoothRepeatFlag, kSmoothRepeatFlagLong, MSyntax::kLong);

    syntax.addFlag(kInfluenceIndexFlag, kInfluenceIndexFlagLong, MSyntax::kLong);
    syntax.addFlag(kPostSettingFlag, kPostSettingFlagLong, MSyntax::kBoolean);

    return syntax;
}

MStatus skinBrushTool::parseArgs(const MArgList &args)
{
    MStatus status = MStatus::kSuccess;

    MArgDatabase argData(syntax(), args);

    if (argData.isFlagSet(kColorRFlag)) {
        double value;
        status = argData.getFlagArgument(kColorRFlag, 0, value);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        input.colorVal = MColor((float)value, input.colorVal.g, input.colorVal.b);
    }
    if (argData.isFlagSet(kColorGFlag)) {
        double value;
        status = argData.getFlagArgument(kColorGFlag, 0, value);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        input.colorVal = MColor(input.colorVal.r, (float)value, input.colorVal.b);
    }
    if (argData.isFlagSet(kColorBFlag)) {
        double value;
        status = argData.getFlagArgument(kColorBFlag, 0, value);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        input.colorVal = MColor(input.colorVal.r, input.colorVal.g, (float)value);
    }
    if (argData.isFlagSet(kCurveFlag)) {
        status = argData.getFlagArgument(kCurveFlag, 0, input.curveVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kDrawBrushFlag)) {
        status = argData.getFlagArgument(kDrawBrushFlag, 0, input.drawBrushVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kDrawRangeFlag)) {
        status = argData.getFlagArgument(kDrawRangeFlag, 0, input.drawRangeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kImportPythonFlag)) {
        status = argData.getFlagArgument(kImportPythonFlag, 0, input.moduleImportString);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kEnterToolCommandFlag)) {
        status = argData.getFlagArgument(kEnterToolCommandFlag, 0, input.enterToolCommandVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kExitToolCommandFlag)) {
        status = argData.getFlagArgument(kExitToolCommandFlag, 0, input.exitToolCommandVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kFractionOversamplingFlag)) {
        status =
            argData.getFlagArgument(kFractionOversamplingFlag, 0, input.fractionOversamplingVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kIgnoreLockFlag)) {
        status = argData.getFlagArgument(kIgnoreLockFlag, 0, input.ignoreLockVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kLineWidthFlag)) {
        status = argData.getFlagArgument(kLineWidthFlag, 0, input.lineWidthVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMessageFlag)) {
        status = argData.getFlagArgument(kMessageFlag, 0, input.messageVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kOversamplingFlag)) {
        status = argData.getFlagArgument(kOversamplingFlag, 0, input.oversamplingVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kRangeFlag)) {
        status = argData.getFlagArgument(kRangeFlag, 0, rangeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kSizeFlag)) {
        status = argData.getFlagArgument(kSizeFlag, 0, input.sizeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kStrengthFlag)) {
        status = argData.getFlagArgument(kStrengthFlag, 0, input.strengthVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kUndersamplingFlag)) {
        status = argData.getFlagArgument(kUndersamplingFlag, 0, input.undersamplingVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kVolumeFlag)) {
        status = argData.getFlagArgument(kVolumeFlag, 0, input.volumeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kSmoothStrengthFlag)) {
        status = argData.getFlagArgument(kSmoothStrengthFlag, 0, input.smoothStrengthVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    if (argData.isFlagSet(kCoverageFlag)) {
        status = argData.getFlagArgument(kCoverageFlag, 0, coverageVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kInfluenceIndexFlag)) {
        status = argData.getFlagArgument(kInfluenceIndexFlag, 0, input.influenceIndex);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kPaintMirrorToleranceFlag)) {
        status = argData.getFlagArgument(kPaintMirrorToleranceFlag, 0, input.mirrorMinDist);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kPaintMirrorFlag)) {
        status = argData.getFlagArgument(kPaintMirrorFlag, 0, input.paintMirror);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kUseColorSetWhilePaintingFlag)) {
        status = argData.getFlagArgument(
            kUseColorSetWhilePaintingFlag, 0, input.useColorSetsWhilePainting
        );
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawTrianglesFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawTrianglesFlag, 0, input.drawTriangles);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawEdgesFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawEdgesFlag, 0, input.drawEdges);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawPointsFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawPointsFlag, 0, input.drawPoints);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawTransFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawTransFlag, 0, input.drawTransparency);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    if (argData.isFlagSet(kMinColorFlag)) {
        status = argData.getFlagArgument(kMinColorFlag, 0, input.minSoloColor);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    if (argData.isFlagSet(kMaxColorFlag)) {
        status = argData.getFlagArgument(kMaxColorFlag, 0, input.maxSoloColor);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    return status;
}

// ---------------------------------------------------------------------
// main methods for the tool command
// ---------------------------------------------------------------------
MStatus skinBrushTool::doIt(const MArgList &args)
{
    // MGlobal::displayInfo(MString("---------------- [skinBrushTool::doIt]------------------"));
    MStatus status = MStatus::kSuccess;

    status = parseArgs(args);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return redoIt();
}

MStatus skinBrushTool::redoIt()
{
    MGlobal::displayInfo(
        MString("skinBrushTool::redoIt is CALLED !!!! input.commandIndex : ") +
        static_cast<int>(this->input.commandIndex)
    );
    return setWeightsForDoit(true);
}

MStatus skinBrushTool::setWeightsForDoit(bool isUndo)
{
    MStatus status = MStatus::kSuccess;

    int theWeightsLength;
    if (isUndo) {
        theWeightsLength = this->undoWeights.length();
    }
    else {
        theWeightsLength = this->redoWeights.length();
    }
    if (theWeightsLength == 0) {
        return status;
    }

    status = getSkinClusterObj();
    if (status != MStatus::kSuccess) {
        MGlobal::displayError(MString("skinBrushTool::undoIt error getting the skin "));
        return status;
    }
    MFnSkinCluster skinFn(weights.skinObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    MFnMesh mesh.meshFn;
    bool validMesh = mesh.meshDag.isValid();
    if (validMesh) {
        mesh.meshFn.setObject(mesh.meshDag);
    }
    MFnNurbsSurface nrbsFn;
    if (interFrame.isNurbs) {
        nrbsFn.setObject(nurbs.nurbsDag);
    }

    if (this->input.commandIndex != ModifierCommands::LockVertices &&
        this->input.commandIndex != ModifierCommands::UnlockVertices && theWeightsLength > 0) {
        MObject weightsObj;
        if (!interFrame.isNurbs) {
            MFnSingleIndexedComponent compFn;
            weightsObj = compFn.create(MFn::kMeshVertComponent);
            compFn.addElements(this->undoVertices);
            if (isUndo) {
                skinFn.setWeights(
                    mesh.meshDag, weightsObj, influence.influenceIndices, this->undoWeights, true,
                    &redoWeights
                );
            }
            else {
                skinFn.setWeights(
                    mesh.meshDag, weightsObj, influence.influenceIndices, this->redoWeights, true
                );
            }
        }
        else {
            MFnDoubleIndexedComponent doubleFn;
            weightsObj = doubleFn.create(MFn::kSurfaceCVComponent);
            // MFnSingleIndexedComponent theVertex;
            int uVal, vVal;
            for (int vert : this->undoVertices) {
                vVal = (int)vert % (int)nurbs.numCVsInV_;
                uVal = (int)vert / (int)nurbs.numCVsInV_;
                doubleFn.addElement(uVal, vVal);
            }
            if (isUndo) {
                skinFn.setWeights(
                    nurbs.nurbsDag, weightsObj, influence.influenceIndices, this->undoWeights, true,
                    &redoWeights
                );
            }
            else {
                skinFn.setWeights(
                    nurbs.nurbsDag, weightsObj, influence.influenceIndices, this->redoWeights, true
                );
            }
            if (validMesh) {
                transferPointNurbsToMesh(mesh.meshFn, nrbsFn); // we transfer the points postions
            }
            else {
                MGlobal::displayInfo("mesh not valid need to clean it");
            }
        }
    }
    if (this->input.commandIndex == ModifierCommands::LockVertices ||
        this->input.commandIndex == ModifierCommands::UnlockVertices) {
        MGlobal::displayInfo("undo it with refresh: lock / unlock vertices");

        MObjectArray objectsDeformed;
        skinFn.getOutputGeometry(objectsDeformed);
        MFnDependencyNode deformedNameMesh(objectsDeformed[0]);
        MPlug lockedVerticesPlug = deformedNameMesh.findPlug("lockedVertices", &stat);
        if (MS::kSuccess != status) {
            MGlobal::displayError(MString("cant find lockerdVertices plug"));
            return status;
        }
        // now set the value ---------------------------
        MFnIntArrayData tmpIntArray;

        MIntArray theArrayValues;
        for (unsigned int vtx = 0; vtx < undoLocks.length(); ++vtx) {
            if (undoLocks[vtx] == 1) {
                theArrayValues.append(vtx);
            }
        }
        status =
            lockedVerticesPlug.setValue(tmpIntArray.create(theArrayValues)); // to set the attribute
        // we need a hard refresh of invalidate for the undo / redo ---
    }
    if ((this->input.commandIndex == ModifierCommands::LockVertices ||
         this->input.commandIndex == ModifierCommands::UnlockVertices) ||
        (interFrame.isNurbs && validMesh)) {
        mesh.meshFn.updateSurface();
    }

    callBrushRefresh();
    if (interFrame.isNurbs) {
        MGlobal::executePythonCommand(input.moduleImportString + MString("cleanTheNurbs\n"));
        MGlobal::executePythonCommand("cleanTheNurbs()\n");
    }
    return status;
}

MStatus skinBrushTool::undoIt()
{
    MGlobal::displayInfo(
        MString("skinBrushTool::undoIt is CALLED ! input.commandIndex : ") +
        static_cast<int>(this->input.commandIndex)
    );
    return setWeightsForDoit(true);
}

MStatus skinBrushTool::callBrushRefresh()
{
    /*
    -------
    ------- VERY IMPORTANT
    ------- refresh the tool points positions, normals, skin weight stored, and vertex colors
    -------
    */

    ctxt->refreshTheseVertices(undoVertices);
    MUserEventMessage::postUserEvent("brSkinBrush_afterPaint");
    return MStatus::kSuccess;
}

MStatus skinBrushTool::finalize()
{
    // Store the current settings as an option var. This way they are
    // properly available for the next usage.

    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);
    writer.StartObject();

    writer.Key("-image1");
    writer.String("brSkinBrush.svg");

    writer.Key("-image2");
    writer.String("vacantCell.svg");

    writer.Key("-image3");
    writer.String("vacantCell.svg");

    writer.Key(kColorRFlag);
    writer.Double(input.colorVal.r);

    writer.Key(kColorGFlag);
    writer.Double(input.colorVal.g);

    writer.Key(kColorBFlag);
    writer.Double(input.colorVal.b);

    writer.Key(kCommandIndexFlag);
    writer.Int(static_cast<int>(input.commandIndex));

    writer.Key(kCoverageFlag);
    writer.Bool(coverageVal);

    writer.Key(kCurveFlag);
    writer.Int(input.curveVal);

    writer.Key(kDrawBrushFlag);
    writer.Bool(input.drawBrushVal);

    writer.Key(kDrawRangeFlag);
    writer.Bool(input.drawRangeVal);

    writer.Key(kEnterToolCommandFlag);
    writer.String(input.enterToolCommandVal.asChar());

    writer.Key(kExitToolCommandFlag);
    writer.String(input.exitToolCommandVal.asChar());

    writer.Key(kFractionOversamplingFlag);
    writer.Bool(input.fractionOversamplingVal);

    writer.Key(kIgnoreLockFlag);
    writer.Bool(input.ignoreLockVal);

    writer.Key(kImportPythonFlag);
    writer.String(input.moduleImportString.asChar());

    writer.Key(kInfluenceNameFlag);
    writer.String(influenceName.asChar());

    writer.Key(kLineWidthFlag);
    writer.Int(input.lineWidthVal);

    writer.Key(kMaxColorFlag);
    writer.Double(input.maxSoloColor);

    writer.Key(kMeshDragDrawEdgesFlag);
    writer.Bool(input.drawEdges);

    writer.Key(kMeshDragDrawPointsFlag);
    writer.Bool(input.drawPoints);

    writer.Key(kMeshDragDrawTransFlag);
    writer.Bool(input.drawTransparency);

    writer.Key(kMeshDragDrawTrianglesFlag);
    writer.Bool(input.drawTriangles);

    writer.Key(kMessageFlag);
    writer.Int(input.messageVal);

    writer.Key(kMinColorFlag);
    writer.Double(input.minSoloColor);

    writer.Key(kOversamplingFlag);
    writer.Int(input.oversamplingVal);

    writer.Key(kPaintMirrorFlag);
    writer.Int(input.paintMirror);

    writer.Key(kPaintMirrorToleranceFlag);
    writer.Double(input.mirrorMinDist);

    writer.Key(kPostSettingFlag);
    writer.Bool(input.postSetting);

    writer.Key(kRangeFlag);
    writer.Double(rangeVal);

    writer.Key(kSizeFlag);
    writer.Double(input.sizeVal);

    writer.Key(kSmoothRepeatFlag);
    writer.Int(input.smoothRepeat);

    writer.Key(kSmoothStrengthFlag);
    writer.Double(input.smoothStrengthVal);

    writer.Key(kSoloColorFlag);
    writer.Int(input.soloColorVal);

    writer.Key(kSoloColorTypeFlag);
    writer.Int(input.soloColorTypeVal);

    writer.Key(kStrengthFlag);
    writer.Double(input.strengthVal);

    writer.Key(kUndersamplingFlag);
    writer.Int(input.undersamplingVal);

    writer.Key(kUseColorSetWhilePaintingFlag);
    writer.Bool(input.useColorSetsWhilePainting);

    writer.Key(kVolumeFlag);
    writer.Bool(input.volumeVal);

    writer.EndObject();
    MGlobal::setOptionVarValue("brSkinBrushContextOptions", s.GetString());

    // Finalize the command by adding it to the undo queue and the
    // journal.
    MArgList command;
    command.addArg(commandString());

    return MPxToolCommand::doFinalize(command);
}

// ---------------------------------------------------------------------
// getting values from the command flags
// ---------------------------------------------------------------------

void skinBrushTool::setColor(MColor &value) { input.colorVal = value; }

void skinBrushTool::setCurve(int value) { input.curveVal = value; }

void skinBrushTool::setDrawBrush(bool value) { input.drawBrushVal = value; }

void skinBrushTool::setMinColor(double value) { input.minSoloColor = value; }

void skinBrushTool::setMaxColor(double value) { input.maxSoloColor = value; }

void skinBrushTool::setDrawRange(bool value) { input.drawRangeVal = value; }

void skinBrushTool::setPythonImportPath(MString &value) { input.moduleImportString = value; }

void skinBrushTool::setEnterToolCommand(MString &value) { input.enterToolCommandVal = value; }

void skinBrushTool::setExitToolCommand(MString &value) { input.exitToolCommandVal = value; }

void skinBrushTool::setFractionOversampling(bool value) { input.fractionOversamplingVal = value; }

void skinBrushTool::setIgnoreLock(bool value) { input.ignoreLockVal = value; }

void skinBrushTool::setLineWidth(int value) { input.lineWidthVal = value; }

void skinBrushTool::setMessage(int value) { input.messageVal = value; }

void skinBrushTool::setOversampling(int value) { input.oversamplingVal = value; }

void skinBrushTool::setRange(double value) { rangeVal = value; }

void skinBrushTool::setSize(double value) { input.sizeVal = value; }

void skinBrushTool::setStrength(double value) { input.strengthVal = value; }

void skinBrushTool::setSmoothStrength(double value) { input.smoothStrengthVal = value; }

void skinBrushTool::setUndersampling(int value) { input.undersamplingVal = value; }

void skinBrushTool::setVolume(bool value) { input.volumeVal = value; }

void skinBrushTool::setCommandIndex(ModifierCommands value) { input.commandIndex = value; }

void skinBrushTool::setSmoothRepeat(int value) { input.smoothRepeat = value; }

void skinBrushTool::setMirrorTolerance(double value) { input.mirrorMinDist = value; }

void skinBrushTool::setPaintMirror(int value) { input.paintMirror = value; }

void skinBrushTool::setUseColorSetsWhilePainting(bool value)
{
    input.useColorSetsWhilePainting = value;
}

void skinBrushTool::setDrawTriangles(bool value) { input.drawTriangles = value; }

void skinBrushTool::setDrawEdges(bool value) { input.drawEdges = value; }

void skinBrushTool::setDrawPoints(bool value) { input.drawPoints = value; }

void skinBrushTool::setDrawTransparency(bool value) { input.drawTransparency = value; }

void skinBrushTool::setSoloColorType(int value) { input.soloColorTypeVal = value; }

void skinBrushTool::setSoloColor(int value) { input.soloColorVal = value; }

void skinBrushTool::setCoverage(bool value) { coverageVal = value; }

void skinBrushTool::setPostSetting(bool value) { input.postSetting = value; }

// ---------------------------------------------------------------------
// public methods for setting the undo/redo variables
// ---------------------------------------------------------------------

void skinBrushTool::setInfluenceIndices(MIntArray &indices)
{
    influence.influenceIndices = indices;
}

void skinBrushTool::setInfluenceName(MString &name) { influenceName = name; }

MStatus skinBrushTool::getSkinClusterObj()
{
    MStatus status = MS::kSuccess;

    return status;
    MSelectionList selList;
    status = MGlobal::getSelectionListByName(skinName, selList);
    if (status != MStatus::kSuccess) {
        return status;
    }
    status = selList.getDependNode(0, weights.skinObj);

    MFnDependencyNode nodeFn(weights.skinObj);
    MGlobal::displayInfo(MString("    input skin name: ") + nodeFn.name());

    status = findMesh(weights.skinObj, mesh.meshDag);
    return status;
}

void skinBrushTool::setMesh(MDagPath &dagPath) { mesh.meshDag = dagPath; }

void skinBrushTool::setNurbs(MDagPath &dagPath) { nurbs.nurbsDag = dagPath; }

void skinBrushTool::setNormalize(bool value) { normalize = value; }

void skinBrushTool::setSkinCluster(MObject &skinCluster) { weights.skinObj = skinCluster; }

void skinBrushTool::setIsNurbs(bool value) { interFrame.isNurbs = value; }

void skinBrushTool::setnumCVInV(int value) { nurbs.numCVsInV_ = value; }

void skinBrushTool::setSkinClusterName(MString &skinClusterName) { skinName = skinClusterName; }

void skinBrushTool::setWeights(MDoubleArray &weights) { undoWeights = weights; }

void skinBrushTool::setUndoVertices(MIntArray &editVertsIndices)
{
    undoVertices = editVertsIndices;
}

void skinBrushTool::setUndoLocks(MIntArray &locks) { undoLocks = locks; }

void skinBrushTool::setRedoLocks(MIntArray &locks) { redoLocks = locks; }

void skinBrushTool::setContextPointer(SkinBrushContext *c) { ctxt = c; }
