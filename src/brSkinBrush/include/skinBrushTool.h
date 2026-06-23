// ---------------------------------------------------------------------
//
//  skinBrushTool.h
//  skinBrushTool
//
//  Created by ingo on 11/18/18.
//  Copyright (c) 2018 Ingo Clemens. All rights reserved.
//
// ---------------------------------------------------------------------
#ifndef __skinBrushTool__skinBrushTool__
#define __skinBrushTool__skinBrushTool__

#include "enums.h"
#include "functions.h"
#include "setOverloads.h"
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
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MSyntax.h>
#include <maya/MThreadUtils.h>
#include <maya/MToolsInfo.h>
#include <maya/MUIDrawManager.h>
#include <maya/MUintArray.h>
#include <maya/MUserEventMessage.h>

#include <QtCore/QRect>
#include <QtCore/QString>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric> //std::iota
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

class SkinBrushContext;

class skinBrushTool : public MPxToolCommand {
  public:
    skinBrushTool();
    ~skinBrushTool();

    static void *creator();
    static MSyntax newSyntax();

    MStatus parseArgs(const MArgList &args);

    MStatus doIt(const MArgList &args);
    MStatus redoIt();
    MStatus undoIt();
    MStatus setWeightsForDoit(bool isUndo);
    MStatus callBrushRefresh();
    MStatus finalize();

    bool isUndoable() const;

    // setting the attributes
    void setColor(MColor &color);
    void setCurve(int value);
    void setDrawBrush(bool value);
    void setDrawRange(bool value);
    void setPythonImportPath(MString &value);
    void setEnterToolCommand(MString &value);
    void setExitToolCommand(MString &value);
    void setFractionOversampling(bool value);
    void setIgnoreLock(bool value);
    void setLineWidth(int value);
    void setMessage(int value);
    void setOversampling(int value);
    void setRange(double value);
    void setSize(double value);
    void setStrength(double value);
    void setSmoothStrength(double value);
    void setUndersampling(int value);
    void setVolume(bool value);
    void setCommandIndex(ModifierCommands value);
    void setSmoothRepeat(int value);
    void setSoloColor(int value);
    void setSoloColorType(int value);
    void setCoverage(bool value);
    void setPostSetting(bool value);

    void setInfluenceIndices(MIntArray &indices);
    void setInfluenceName(MString &name);
    void setMesh(MDagPath &dagPath);
    void setNurbs(MDagPath &dagPath);
    void setNormalize(bool value);

    void setSkinCluster(MObject &skinCluster);
    void setIsNurbs(bool value);
    void setnumCVInV(int value);

    void setSkinClusterName(MString &skinClusterName);
    MStatus getSkinClusterObj();

    void setWeights(MDoubleArray &weights);
    void setUndoVertices(MIntArray &editVertsIndices);
    void setUndoLocks(MIntArray &locks);
    void setRedoLocks(MIntArray &locks);

    void setMirrorTolerance(double value);
    void setPaintMirror(int value);
    void setUseColorSetsWhilePainting(bool value);
    void setDrawTriangles(bool value);
    void setDrawEdges(bool value);
    void setDrawPoints(bool value);
    void setDrawTransparency(bool value);

    void setMinColor(double value);
    void setMaxColor(double value);
    void setContextPointer(SkinBrushContext *c);

  private:
    SkinBrushContext *ctxt;

    UserInputData input;
    MeshState mesh;
    NurbsData nurbs;
    WeightData weights;
    InfluenceData influence;
    InteractionPerFrameData interFrame;

    double rangeVal;
    bool coverageVal;
    bool normalize;
    MString influenceName;
    MString skinName;

    MDoubleArray redoWeights;
    MDoubleArray undoWeights;
    MIntArray undoVertices;
    MObject vertexComponents;

    MIntArray undoLocks, redoLocks;
};

// ---------------------------------------------------------------------
// the context
// ---------------------------------------------------------------------
class SkinBrushContext : public MPxContext {
  public:
    SkinBrushContext();
    void toolOnSetup(MEvent &event);
    void toolOffCleanup();

    void getClassName(MString &name) const;

    MStatus doPress(MEvent &event);
    MStatus doDrag(MEvent &event);
    MStatus doRelease(MEvent &event);

    void drawCircle(MPoint point, MMatrix &mat, double radius) const;

    // VP2.0
    MStatus doPress(
        MEvent &event, MHWRender::MUIDrawManager &drawManager,
        const MHWRender::MFrameContext &context
    );
    MStatus doDrag(
        MEvent &event, MHWRender::MUIDrawManager &drawManager,
        const MHWRender::MFrameContext &context
    );
    MStatus doRelease(
        MEvent &event, MHWRender::MUIDrawManager &drawManager,
        const MHWRender::MFrameContext &context
    );
    MStatus drawTheMesh(MHWRender::MUIDrawManager &drawManager, MVector &worldVector);
    MStatus drawMeshWhileDrag(MHWRender::MUIDrawManager &drawManager);

    MStatus doPtrMoved(
        MEvent &event, MHWRender::MUIDrawManager &drawManager,
        const MHWRender::MFrameContext &context
    );
    int getHighestInfluence(int faceHit, MFloatPoint &hitPoint);
    int getClosestInfluenceToCursor(int screenX, int screenY);
    // common methods
    MStatus doPressCommon(MEvent &event);
    // doDragCommon where the magic happens
    MStatus doDragCommon(MEvent &event);
    MStatus doReleaseCommon(MEvent &event);
    void doTheAction();
    ModifierCommands getCommandIndexModifiers() const;
    MStatus getMesh();
    MStatus getTheOrigMeshForMirror();

    void getConnectedVertices();
    void getConnectedVerticesSecond();
    void getConnectedVerticesThird();
    void getConnectedVerticesTyler();
    void getConnectedVerticesFlatten(
        std::vector<int> &perVertexVerticesSetFLAT, std::vector<int> &perVertexVerticesSetINDEX,
        std::vector<int> &perFaceVerticesSetFLAT, std::vector<int> &perFaceVerticesSetINDEX
    ) const;

    std::vector<int> getSurroundingVerticesPerVert(int vertexIndex) const;
    std::vector<int> getSurroundingVerticesPerFace(int vertexIndex) const;

    void getFromMeshNormals();
    MStatus getSelection(MDagPath &dagPath);
    MStatus getSkinCluster(MDagPath &meshDag, MObject &skinClusterObj);
    void refreshJointsLocks();
    void refresh();
    void refreshDeformerColor(int influenceInd);
    void refreshTheseVertices(MIntArray &verticesIndices);
    void refreshMirrorInfluences(MIntArray &inputMirrorInfluences);

    void mergeMirrorArray(
        std::unordered_map<int, float> &valuesBase, std::unordered_map<int, float> &valuesMirrored
    );
    MStatus applyCommand(int influence, std::unordered_map<int, float> &valuesToSet);
    MStatus applyCommandMirror();
    MStatus refreshColors(
        MIntArray &editVertsIndices, MColorArray &multiEditColors, MColorArray &soloEditColors
    );
    MStatus editSoloColorSet(bool doBlack);
    MColor getASoloColor(double val);
    MStatus refreshPointsNormals();

    void getColorWithMirror(
        int vertexIndex, float valueBase, float valueMirror, MColor &multColor, MColor &soloColor
    );

    MStatus querySkinClusterValues(
        MObject &skinCluster, MIntArray &verticesIndices, MDoubleArray &theSkinWeightList,
        bool doColors
    ) const;
    MStatus fillArrayValues(MObject &skinCluster, bool doColors);
    MStatus displayWeightValue(int vertexIndex, bool displayZero = false) const;
    MStatus fillArrayValuesDEP(MObject &skinCluster, bool doColors);
    void getSkinClusterAttributes(
        MObject &skinCluster, unsigned int &maxInfluences, bool &maintainMaxInfluences,
        unsigned int &normalize
    ) const;
    MIntArray getInfluenceIndices();
    bool getMirrorHit(int &faceHit, MFloatPoint &hitPoint) const;
    bool computeHit(
        short screenPixelX, short screenPixelY, bool getNormal, int &faceHit, MFloatPoint &hitPoint
    );
    bool expandHit(
        int faceHit, MFloatPoint &hitPoint, std::unordered_map<int, float> &dicVertsDist
    ) const;

    void growArrayOfHitsFromCenters(
        std::unordered_map<int, float> &dicVertsDist, MFloatPointArray &AllHitPoints
    );

    // smooth computation
    void preparePaint(
        std::unordered_map<int, float> &dicVertsDist,
        std::unordered_map<int, float> &dicVertsDistPrevPaint, std::vector<float> &intensityValues,
        std::unordered_map<int, float> &skinValToSet, std::set<int> &theVerticesPainted, bool mirror
    );

    MStatus doPerformPaint();

    void addBrushShapeFallof(std::unordered_map<int, float> &dicVertsDist) const;

    MObject allVertexComponents();
    void getVerticesInVolumeRange(
        int index, MIntArray &volumeIndices, MIntArray &rangeIndices, MFloatArray &values
    ) const;

    double getFalloffValue(double value, double strength) const;
    bool eventIsValid(MEvent &event);

    void setInViewMessage(bool display) const;

    // setting the attributes
    void setColorR(float value);
    void setColorG(float value);
    void setColorB(float value);
    void setCurve(int value);
    void setDrawBrush(bool value);
    void setDrawRange(bool value);
    void setPythonImportPath(MString &value);
    void setEnterToolCommand(MString &value);
    void setExitToolCommand(MString &value);
    void setFlood();
    void setVerbose(bool value);
    void setPickMaxInfluence(bool value);
    void setPickInfluence(bool value);
    void setFractionOversampling(bool value);
    void setIgnoreLock(bool value);
    void setLineWidth(int value);
    void setMessage(int value);
    void setOversampling(int value);
    void setRange(double value);
    void setSize(double value);
    void setStrength(double value);
    void setSmoothStrength(double value);
    void setUndersampling(int value);
    void setVolume(bool value);
    void setMirrorTolerance(double value);
    void setPaintMirror(int value);
    void setUseColorSetsWhilePainting(bool value);
    void setDrawTriangles(bool value);
    void setDrawEdges(bool value);
    void setDrawPoints(bool value);
    void setShiftSmooths(bool value);
    void setDrawTransparency(bool value);
    void setCoverage(bool value);
    void setInfluenceIndex(int value, bool selectInUI);
    void setCommandIndex(ModifierCommands value);
    void setSmoothRepeat(int value);
    void setSoloColor(int value);
    void maya2019RefreshColors(bool toggle = true);
    void setSoloColorType(int value);
    void setInfluenceByName(MString &value);
    void setPostSetting(bool value);

    void setMinColor(double value);
    void setMaxColor(double value);

    void setInteractiveValue(double value, int ind);

    // getting the attributes
    float getColorR();
    float getColorG();
    float getColorB();
    int getCurve();
    bool getDrawBrush();
    bool getDrawRange();
    MString getPythonImportPath();
    MString getEnterToolCommand();
    MString getExitToolCommand();
    bool getFractionOversampling();
    bool getIgnoreLock();

    int getLineWidth();
    int getMessage();
    int getOversampling();
    double getRange();
    double getSize();
    double getStrength();
    double getSmoothStrength();
    double getInteractiveValue(int ind);
    int getUndersampling();
    bool getVolume();
    bool getCoverage();
    int getInfluenceIndex();
    MString getInfluenceName();
    MString getSkinClusterName();
    MString getMeshName();
    ModifierCommands getCommandIndex();
    int getSmoothRepeat();
    int getSoloColor();

    double getMirrorTolerance();
    int getPaintMirror();
    bool getUseColorSetsWhilePainting();
    bool getDrawTriangles();
    bool getDrawEdges();
    bool getDrawPoints();
    bool getDrawTransparency();
    int getSoloColorType();
    bool getPostSetting();
    double getMinColor();
    double getMaxColor();

    MIntArray getWeightOrderedIndices();
    double getAdjustValue();
    MString getPickedInfluence();

  private:
    bool verbose = false;
    double interactiveValue = 1.0;  // for whateverUse in the code
    double interactiveValue1 = 1.0; // for whateverUse in the code
    double interactiveValue2 = 1.0; // for whateverUse in the code

    skinBrushTool *cmd;

    int performRefreshViewPort;
    int maxRefreshValue = 2;

    // Hard-coded values not stored in structs
    double rangeVal = 0.5;
    bool coverageVal = true;
    bool refreshDone = false;

    // Persistent state not covered by other structs
    bool doNormalize = true;
    bool foundBlurSkinAttribute = false;
    double pruneWeight;
    int nbJointsBig = 0;
    MIntArray deformersIndices;
    MIntArray cpIds; // vertex ids passed to update skin
    std::vector<std::vector<std::pair<int, float>>> skin_weights_;
    MIntArray VertexCountPerPolygon, fullVertexList;
    int fullVertexListLength = 0;
    MPointArray surfacePoints; // cursor positions on the mesh in world space
    MPoint worldMirrorPoint;
    MVector normalMirroredVector; // mirrored normal vector to camera
    unsigned int influenceCount;
    unsigned int maxInfluences;
    bool maintainMaxInfluences;
    bool normalize;
    MDagPath origMeshDag;
    MFnMesh meshOrigFn;
    unsigned int numElements = 0;
    bool UIsPeriodic_ = false, VIsPeriodic_ = false;
    unsigned int UDeg_ = 0, VDeg_ = 0;
    MIntArray vtxSelection; // currently selected vertices (flooding)
    MObject attrValue;
    MDoubleArray valuesForAttribute, paintArrayValues;
    std::vector<bool> selectedIndices;
    MObject allVtxCompObj;
    std::vector<bool> influenceLocks;
    MDGModifier colorSetMod;
    bool toggleColorState = false;
    std::vector<std::vector<int>> perVertexVerticesSet; // per vertex vertices
    std::vector<std::vector<int>> perFaceVerticesSet;   // per face vertices
    std::vector<std::vector<int>> normalsIds;           // vector of face normal ids
    std::vector<MIntArray> perFaceVertices;             // per face vertices (MIntArray form)
    MSelectionList prevSelection;
    MSelectionList prevHilite;
    M3dView view;
    unsigned int width;
    unsigned int height;

    // ── Structured state ────────────────────────────────────────────────────
    MeshState mesh;                         // Mesh geometry, topology, and raw-pointer caches
    NurbsData nurbs;                        // NURBS surface data
    InfluenceData influence;                // Skin cluster influences, colors, locks
    WeightData weights;                     // Per-vertex weights, colors, skin object
    UserInputData input;                    // All tool settings from the UI / flags
    InteractionStartData interStart;        // Press-time intersectors and screen anchors
    InteractionPersistentData interPersist; // State that persists across drag frames
    InteractionPerFrameData interFrame;     // Per-frame hit/mouse/modifier data
    MirrorableData paint;                   // Paint-side hit/weight/intensity data
    MirrorableData mirror;                  // Mirror-side hit/weight/intensity data
};

// ---------------------------------------------------------------------
// command to create the context
// ---------------------------------------------------------------------

class SkinBrushContextCmd : public MPxContextCommand {
  public:
    SkinBrushContextCmd();
    MPxContext *makeObj();
    static void *creator();
    MStatus appendSyntax();
    MStatus doEditFlags();
    MStatus doQueryFlags();

  protected:
    SkinBrushContext *smoothContext;
};

#endif
