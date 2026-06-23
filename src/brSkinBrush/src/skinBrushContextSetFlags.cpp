#include "skinBrushTool.h"

// ---------------------------------------------------------------------
// setting values from the command flags
// ---------------------------------------------------------------------
void SkinBrushContext::setColorR(float value)
{
    input.colorVal.r = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setColorG(float value)
{
    input.colorVal.g = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setColorB(float value)
{
    input.colorVal.b = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setCurve(int value)
{
    input.curveVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setDrawBrush(bool value)
{
    input.drawBrushVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setDrawRange(bool value)
{
    input.drawRangeVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setPythonImportPath(MString &value)
{
    input.moduleImportString = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setEnterToolCommand(MString &value)
{
    input.enterToolCommandVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setExitToolCommand(MString &value)
{
    input.exitToolCommandVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setFlood()
{
    this->interPersist.verticesPainted.clear();
    this->paint.skinValuesToSet.clear();
    double value = input.strengthVal;

    // 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen - 6 LockVertices - 7
    // UnLockVertices
    ModifierCommands theCommandIndex = this->input.commandIndex;
    if (this->interFrame.modifierNoneShiftControl == ModifierKeys::Shift) {
        theCommandIndex = ModifierCommands::Smooth; // smooth always
    }
    if (this->interFrame.modifierNoneShiftControl == ModifierKeys::ControlShift) {
        theCommandIndex = ModifierCommands::Sharpen; // sharpen always
    }

    if (theCommandIndex == ModifierCommands::Smooth ||
        this->interFrame.modifierNoneShiftControl == ModifierKeys::Shift ||
        this->interFrame.modifierNoneShiftControl == ModifierKeys::ControlShift) {
        value = input.smoothStrengthVal;
    }

    for (int i = 0; i < this->mesh.numVertices; ++i) {
        this->interPersist.verticesPainted.insert(i);
        this->paint.skinValuesToSet.insert(std::make_pair(i, value));
    }
    doTheAction();
    if (verbose) {
        MGlobal::displayInfo(
            MString("SET FLOOD IS CALLED command ") + static_cast<int>(theCommandIndex) +
            MString(" value ") + value
        );
    }
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setVerbose(bool value) { verbose = value; }

void SkinBrushContext::setFractionOversampling(bool value)
{
    input.fractionOversamplingVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setIgnoreLock(bool value)
{
    input.ignoreLockVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setLineWidth(int value)
{
    input.lineWidthVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setMessage(int value)
{
    input.messageVal = value;
    MToolsInfo::setDirtyFlag(*this);

    setInViewMessage(true);
}

void SkinBrushContext::setOversampling(int value)
{
    input.oversamplingVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setRange(double value)
{
    rangeVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setSize(double value)
{
    input.sizeVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setStrength(double value)
{
    // 0 Add - 1 Remove - 2 AddPercent - 3 Absolute - 4 Smooth - 5 Sharpen - 6 LockVertices - 7
    // unlockVertices
    if (input.commandIndex == ModifierCommands::Smooth) {
        input.smoothStrengthVal = value;
    }
    else { // others
        input.strengthVal = value;
    }

    MToolsInfo::setDirtyFlag(*this);
}
void SkinBrushContext::setSmoothStrength(double value)
{
    input.smoothStrengthVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setInteractiveValue(double value, int ind)
{
    if (ind == 0) {
        this->interactiveValue = value;
    }
    else if (ind == 1) {
        this->interactiveValue1 = value;
    }
    else if (ind == 2) {
        this->interactiveValue2 = value;
    }
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setUndersampling(int value)
{
    input.undersamplingVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setVolume(bool value)
{
    input.volumeVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setMirrorTolerance(double value)
{
    input.mirrorMinDist = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setPaintMirror(int value)
{
    input.paintMirror = value;
    if (value != 0) {
        getTheOrigMeshForMirror();
    }
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setSewTolerance(double value)
{
    sewVerticesMinDist = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setSewVertices(bool value)
{
    sewVertices = value;
    if (value) {
        getConnectedBorderVertices();
    }
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setUseColorSetsWhilePainting(bool value)
{
    input.useColorSetsWhilePainting = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setDrawTriangles(bool value)
{
    input.drawTriangles = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setDrawEdges(bool value)
{
    input.drawEdges = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setDrawPoints(bool value)
{
    input.drawPoints = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setDrawTransparency(bool value)
{
    input.drawTransparency = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setMinColor(double value)
{
    input.minSoloColor = value;
    refreshDeformerColor(this->input.influenceIndex);
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setMaxColor(double value)
{
    input.maxSoloColor = value;
    refreshDeformerColor(this->input.influenceIndex);
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setCommandIndex(ModifierCommands value)
{
    input.commandIndex = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setSmoothRepeat(int value)
{
    input.smoothRepeat = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setSoloColor(int value)
{
    input.soloColorVal = value;
    MString currentColorSet = mesh.meshFn.currentColorSetName(); // set multiColor as current Color

    if (input.soloColorVal == 1) { // solo
        mesh.meshFn.setCurrentColorSetName(this->interFrame.soloColorSet);
        editSoloColorSet(true);
    }
    else {
        mesh.meshFn.setCurrentColorSetName(this->interFrame.fullColorSet);
    }
    maya2019RefreshColors();
    MToolsInfo::setDirtyFlag(*this);
    //}
}

void SkinBrushContext::maya2019RefreshColors(bool toggle)
{
    mesh.meshFn.updateSurface();
    view = M3dView::active3dView();
    // first swap
    if (toggle) {
        toggleColorState = !toggleColorState;
    }

    if (!toggle || toggleColorState) {
        if (input.soloColorVal == 1) {
            mesh.meshFn.setCurrentColorSetName(this->interFrame.soloColorSet2);
        }
        else {
            mesh.meshFn.setCurrentColorSetName(this->interFrame.fullColorSet2);
        }
        view.refresh(false, true);
    }
    if (!toggle || !toggleColorState) {
        if (input.soloColorVal == 1) {
            mesh.meshFn.setCurrentColorSetName(this->interFrame.soloColorSet);
        }
        else {
            mesh.meshFn.setCurrentColorSetName(this->interFrame.fullColorSet);
        }
        view.refresh(false, true);
    }
}

void SkinBrushContext::setSoloColorType(int value)
{
    if (input.soloColorTypeVal != value) {
        input.soloColorTypeVal = value;
        // here we do the redraw
        mesh.meshFn.updateSurface();
        editSoloColorSet(false);
        maya2019RefreshColors();

        MToolsInfo::setDirtyFlag(*this);
    }
}

void SkinBrushContext::setCoverage(bool value)
{
    coverageVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setPickMaxInfluence(bool value)
{
    input.pickMaxInfluenceVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setPickInfluence(bool value)
{
    input.pickInfluenceVal = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setPostSetting(bool value)
{
    input.postSetting = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setShiftSmooths(bool value)
{
    if (value) {
        input.smoothModifier = ModifierKeys::Shift;
        input.removeModifier = ModifierKeys::Control;
    }
    else {
        input.smoothModifier = ModifierKeys::Control;
        input.removeModifier = ModifierKeys::Shift;
    }
}

void SkinBrushContext::setFastReenter(int value)
{
    MGlobal::displayInfo(MString("setFastReenter CALLED ") + value);
    if (value <= 0) {
        reenterMesh = false;
        reenterSkin = false;
    }
    else if (value == 1) {
        reenterMesh = true;
        reenterSkin = false;
    }
    else {
        reenterMesh = true;
        reenterSkin = true;
    }
    fastReenter = value;
    MToolsInfo::setDirtyFlag(*this);
}

void SkinBrushContext::setInfluenceIndex(int value, bool selectInUI)
{
    if (value == this->input.influenceIndex) {
        return;
    }

    if (value < this->influence.inflNames.length()) {
        this->input.influenceIndex = value;
        interPersist.pickedInfluence = this->influence.inflNames[value];
        if (selectInUI) {
            MUserEventMessage::postUserEvent("brSkinBrush_pickedInfluence");
        }
    }
    // here we do the redraw

    if (input.soloColorVal == 1) { // solo IF NOT IT CRASHES on a first pick before paint
        MString currentColorSet = mesh.meshFn.currentColorSetName(); // get current soloColor
        if (currentColorSet != this->interFrame.soloColorSet) {
            mesh.meshFn.setCurrentColorSetName(this->interFrame.soloColorSet);
        }
        editSoloColorSet(false);
    }

    mesh.meshFn.updateSurface(); // for proper redraw hopefully
    maya2019RefreshColors();
}

void SkinBrushContext::setInfluenceByName(MString &value)
{
    if (this->input.pickMaxInfluenceVal) {
        return;
    }

    int indexInfluence = this->influence.inflNames.indexOf(value);
    if (indexInfluence == -1) {
        return;
    }

    setInfluenceIndex(indexInfluence, false);
}
void SkinBrushContext::setSkinClusterByName(MString &value)
{
    if (verbose) {
        MGlobal::displayInfo("setSkinClusterByName CALLED \"" + value + "\"\n");
    }
    getSkinFromName = true;
    passedSkinName = value;
}
void SkinBrushContext::setMeshByName(MString &value)
{
    if (verbose) {
        MGlobal::displayInfo("setMeshByName CALLED \"" + value + "\"\n");
    }
    getMeshFromName = true;
    passedMeshName = value;
}

// ---------------------------------------------------------------------
// getting values from the command flags
// ---------------------------------------------------------------------
float SkinBrushContext::getColorR() { return input.colorVal.r; }
float SkinBrushContext::getColorG() { return input.colorVal.g; }
float SkinBrushContext::getColorB() { return input.colorVal.b; }

int SkinBrushContext::getCurve() { return input.curveVal; }
bool SkinBrushContext::getDrawBrush() { return input.drawBrushVal; }
bool SkinBrushContext::getDrawRange() { return input.drawRangeVal; }
MString SkinBrushContext::getPythonImportPath() { return input.moduleImportString; }
MString SkinBrushContext::getEnterToolCommand() { return input.enterToolCommandVal; }
MString SkinBrushContext::getExitToolCommand() { return input.exitToolCommandVal; }
bool SkinBrushContext::getFractionOversampling() { return input.fractionOversamplingVal; }

bool SkinBrushContext::getIgnoreLock() { return input.ignoreLockVal; }

int SkinBrushContext::getLineWidth() { return input.lineWidthVal; }
int SkinBrushContext::getMessage() { return input.messageVal; }
int SkinBrushContext::getOversampling() { return input.oversamplingVal; }
double SkinBrushContext::getRange() { return rangeVal; }
double SkinBrushContext::getSize() { return input.sizeVal; }
double SkinBrushContext::getStrength() { return input.strengthVal; }
double SkinBrushContext::getSmoothStrength() { return input.smoothStrengthVal; }

double SkinBrushContext::getInteractiveValue(int ind)
{
    if (ind == 0) {
        return this->interactiveValue;
    }
    else if (ind == 1) {
        return this->interactiveValue1;
    }
    // if (ind == 2)
    return this->interactiveValue2;
}

int SkinBrushContext::getUndersampling() { return input.undersamplingVal; }
bool SkinBrushContext::getVolume() { return input.volumeVal; }
ModifierCommands SkinBrushContext::getCommandIndex() { return input.commandIndex; }
int SkinBrushContext::getSmoothRepeat() { return input.smoothRepeat; }
int SkinBrushContext::getSoloColor() { return input.soloColorVal; }
int SkinBrushContext::getFastReenter() { return fastReenter; }

double SkinBrushContext::getMirrorTolerance() { return input.mirrorMinDist; }
int SkinBrushContext::getPaintMirror() { return input.paintMirror; }
bool SkinBrushContext::getSkipSkinValues() { return skipSkinValues; }

double SkinBrushContext::getSewVerticesOffset() { return sewVerticesMinDist; }
bool SkinBrushContext::getSewVertices() { return sewVertices; }

bool SkinBrushContext::getUseColorSetsWhilePainting() { return input.useColorSetsWhilePainting; }
bool SkinBrushContext::getDrawTriangles() { return input.drawTriangles; }
bool SkinBrushContext::getDrawEdges() { return input.drawEdges; }
bool SkinBrushContext::getDrawPoints() { return input.drawPoints; }
bool SkinBrushContext::getDrawTransparency() { return input.drawTransparency; }
int SkinBrushContext::getSoloColorType() { return input.soloColorTypeVal; }
bool SkinBrushContext::getCoverage() { return coverageVal; }
int SkinBrushContext::getInfluenceIndex() { return input.influenceIndex; }

double SkinBrushContext::getMinColor() { return input.minSoloColor; }
double SkinBrushContext::getMaxColor() { return input.maxSoloColor; }

MString SkinBrushContext::getInfluenceName()
{
    MString influenceName("FAILED");
    if (this->input.influenceIndex < this->influence.inflNames.length()) {
        influenceName = this->influence.inflNames[this->input.influenceIndex];
    }

    return influenceName;
}

MString SkinBrushContext::getSkinClusterName()
{
    if (!weights.skinObj.isNull()) {
        MFnDependencyNode skinDep(this->weights.skinObj);
        return skinDep.name();
    }
    else {
        return MString("");
    }
}

MString SkinBrushContext::getMeshName() { return this->mesh.meshDag.fullPathName(); }
bool SkinBrushContext::getPostSetting() { return input.postSetting; }
MIntArray SkinBrushContext::getWeightOrderedIndices()
{
    return interPersist.orderedIndicesByWeightsVals;
}
double SkinBrushContext::getAdjustValue() { return interPersist.adjustValue; }
MString SkinBrushContext::getPickedInfluence() { return interPersist.pickedInfluence; }

using namespace std::chrono;

void SkinBrushContext::catchTimeStamp() { startTimeStamp = high_resolution_clock::now(); }
void SkinBrushContext::endTimeStamp(MString infos)
{
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - startTimeStamp);
    float dura = float(duration.count() / 10000) * 0.01f;
    MGlobal::displayInfo(infos + MString(" executed in ") + dura + MString(" seconds"));
}
