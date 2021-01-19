#include "lib_splinehelp.h"
#include "customgui_priority.h"
#include "c4d_tagplugin.h"
#include "c4d_tagdata.h"
#include "c4d_general.h"
#include "ge_prepass.h"

#include "ttraindriver.h"
#include "ttraindrivercar.h"

#include "main.h"
#include "pluginids.h"
#include "c4d_symbols.h"


class TrainDriverTag : public TagData
{
	INSTANCEOF(TrainDriverTag, TagData)
	
private:
	Bool AddCar(BaseContainer *bc, BaseObject *op);
	
	AutoAlloc<SplineLengthData>	_sldPath;
	AutoAlloc<SplineLengthData>	_sldRail;
	
public:
	virtual Bool Init(GeListNode *node);
	virtual Bool Message(GeListNode *node, Int32 type, void *data);
	virtual EXECUTIONRESULT Execute(BaseTag *tag, BaseDocument *doc, BaseObject *op, BaseThread *bt, Int32 priority, EXECUTIONFLAGS flags);
	
	static NodeData *Alloc()
	{
		return NewObjClear(TrainDriverTag);

	}
};



// Clamps a value between 0.0 and 1.0 with optional circular behavior
static Float CircularValue(Float x, Bool circular)
{
	if (circular)
	{
		if (x > 1.0)
			return x - Floor(x);
		else if (x < 0.0)
			return x - Floor(x);
	}
	else
	{
		return ClampValue(x, 0.0, 1.0);
	}
	
	return x;
}

// Align a matrix to a target, using an up vector
static Matrix Target(const Vector &vPos, const Vector &vTargetPos, const Vector &vUpVector)
{
	// Get Positions
	Matrix m(DC);
	
	// Target
	m.off = vPos;
	m.v3 = !(vTargetPos - vPos);
	m.v1 = !Cross(m.v3, (vPos - vUpVector));
	m.v2 = !Cross(m.v3, m.v1);
	
	// Return result
	return m;
}

// Set default values
Bool TrainDriverMain::Init(GeListNode *node)
{
	// Get pointer to tag's Container
	BaseTag				*tag	= (BaseTag*)node;
	BaseContainer	*bc	= tag->GetDataInstance();
	
	// Set default values in user interface
	bc->SetFloat(TRAIN_MAIN_LENGTH, 30.0);
	bc->SetFloat(TRAIN_MAIN_WHEELDISTANCE, 25.0);
	
	// Set expression's priority
	GeData d(CUSTOMGUI_PRIORITY_DATA, DEFAULTVALUE);
	if (node->GetParameter(DescLevel(EXPRESSION_PRIORITY), d, DESCFLAGS_GET_0))
	{
		PriorityData *pd = (PriorityData*)d.GetCustomDataType(CUSTOMGUI_PRIORITY_DATA);
		if (pd) pd->SetPriorityValue(PRIORITYVALUE_CAMERADEPENDENT, GeData(true));
		node->SetParameter(DescLevel(EXPRESSION_PRIORITY), d, DESCFLAGS_SET_0);
	}
	
	return true;
}

// Recieve and handle messages
Bool TrainDriverMain::Message(GeListNode *node, Int32 type, void *data)
{
	BaseTag *tag = (BaseTag*)node; if(!tag) return true;
	BaseObject *op = tag->GetObject(); if (!op) return true;
	BaseContainer *bc = tag->GetDataInstance(); if (!bc) return true;
	
	switch (type)
	{
			// A command button has been clicked
		case MSG_DESCRIPTION_COMMAND:
		{
			DescriptionCommand *dc = (DescriptionCommand*)data;
			switch (dc->id[0].id)
			{
				case TRAIN_MAIN_CMD_ADDCAR:
					AddCar(bc, op);
					break;
			}
		}
	}
	
	return true;
}

// Work on the train
EXECUTIONRESULT TrainDriverMain::Execute(BaseTag *tag, BaseDocument *doc, BaseObject *op, BaseThread *bt, Int32 priority, EXECUTIONFLAGS flags)
{
	if (!tag || !doc || !op || !_sldPath || !_sldRail)
		return EXECUTIONRESULT_OUTOFMEMORY;
	
	// Get Container from Tag
	BaseContainer *bc = tag->GetDataInstance();
	if (!bc)
		return EXECUTIONRESULT_OUTOFMEMORY;
	
	// Get settings from user interface
	SplineObject	*opPathSpline	= (SplineObject*)bc->GetObjectLink(TRAIN_MAIN_PATHSPLINE, doc);
	SplineObject	*opRailSpline	= (SplineObject*)bc->GetObjectLink(TRAIN_MAIN_RAILSPLINE, doc);
	Float					rOffset				= bc->GetFloat(TRAIN_MAIN_POSITION);
	Bool					bCircular			= bc->GetBool(TRAIN_MAIN_CIRCULAR);
	
	// Cancel if no Path Spline is given
	if (!opPathSpline)
		return EXECUTIONRESULT_USERBREAK;
	
	// Variables
	Float				rPathLength(DC);
	
	Float				rRelCarWheelDist(DC);
	Float				rRelCarLength(DC);
	
	Float				rMainOff(DC);
	Float				rWheel1Off(DC);
	Float				rWheel2Off(DC);
	
	Vector			vMainTangent(DC);
	Vector			vWheel1Tangent(DC);
	Vector			vWheel2Tangent(DC);
	
	BaseObject	*opMain		= nullptr;
	BaseObject	*opWheel1	= nullptr;
	BaseObject	*opWheel2	= nullptr;
	
	Vector			vMainPos(DC);
	Vector			vWheel1Pos(DC);
	Vector			vWheel2Pos(DC);
	
	Vector			vMainUp(DC);
	Vector			vWheel1Up(DC);
	Vector			vWheel2Up(DC);
	
	Vector			vMainScale(DC);
	Vector			vCarScale(DC);
	Vector			vWheel1Scale(DC);
	Vector			vWheel2Scale(DC);
	
	Matrix			mMainMtx(DC);
	Matrix			mWheel1Mtx(DC);
	Matrix			mWheel2Mtx(DC);
	
	BaseContainer	*bcCarData		= nullptr;
	BaseObject		*opCar				= nullptr;
	BaseTag				*btCarTag			= nullptr;
	
	Int32				lCarCount				= 0;
	Float				rCurrentOffset	= rOffset;	// Used to add up the offset as the cars get chained
	
	Float				rCarWheelDist(DC);
	Float				rCarLength(DC);
	
	// Get real splines
	if (opPathSpline->GetRealSpline())
		opPathSpline = opPathSpline->GetRealSpline();
	
	if (opRailSpline->GetRealSpline())
		opRailSpline = opRailSpline->GetRealSpline();
	
	// Init SplineLengthDatas
	
	if (!_sldPath->Init(opPathSpline, 0, opPathSpline->GetPointR()))
		return EXECUTIONRESULT_OUTOFMEMORY;
	rPathLength = _sldPath->GetLength();
	
	if (opRailSpline)
	{
		if (!_sldRail->Init(opRailSpline, 0, opRailSpline->GetPointR()))
			return EXECUTIONRESULT_OUTOFMEMORY;
	}
	
	// Cancel if path length == 0
	if (rPathLength == 0.0)
		goto Error_PathLength;
	
	// Get first car
	opCar = op->GetDown();
	
	// Cancel if there's no car
	if (!opCar)
		goto Error_NoCars;
	
	// Iterate cars
	while (opCar)
	{
		// Increase car counter
		lCarCount++;
		
		// Get opCar's Carriage tag
		btCarTag = opCar->GetTag(ID_TRAINDRIVERCAR);
		
		// Get Car bc from tag. If no tag found, use default values (set in the TrainDriver Main Tag)
		if (btCarTag)
		{
			bcCarData			= btCarTag->GetDataInstance();
			rCarWheelDist	= bcCarData->GetFloat(TRAIN_CAR_WHEELDISTANCE);
			rCarLength			= bcCarData->GetFloat(TRAIN_CAR_LENGTH);
			btCarTag->SetName(opCar->GetName());
		}
		else
		{
			rCarWheelDist = bc->GetFloat(TRAIN_MAIN_WHEELDISTANCE);
			rCarLength    = bc->GetFloat(TRAIN_MAIN_LENGTH);
		}
		
		// Convert Wheel Distance and opCar Length to percent values
		rRelCarWheelDist = rCarWheelDist / rPathLength;
		rRelCarLength = rCarLength / rPathLength;
		
		// Calculate relative offsets for opCar's main part and wheels
		rMainOff		= CircularValue(rCurrentOffset, bCircular);
		rWheel1Off	= rMainOff;
		rWheel2Off	= CircularValue(rWheel1Off - rRelCarWheelDist, true);
		
		// Get car's parts
		opMain		= opCar->GetDown(); if (!opMain) goto Error_Car;
		opWheel1	= opMain->GetNext(); if (!opWheel1) goto Error_Car;
		opWheel2	= opWheel1->GetNext(); if (!opWheel2) goto Error_Car;
		
		// Get path spline matrix
		Matrix pathSplineMg(opPathSpline->GetMg());
		
		// Get tangents
		vMainTangent = pathSplineMg.TransformVector(opPathSpline->GetSplineTangent(_sldPath->UniformToNatural(rMainOff)));
		vWheel1Tangent = pathSplineMg.TransformVector(opPathSpline->GetSplineTangent(_sldPath->UniformToNatural(rWheel1Off)));
		vWheel2Tangent = pathSplineMg.TransformVector(opPathSpline->GetSplineTangent(_sldPath->UniformToNatural(rWheel2Off)));
		
		// Position the car's parts
		opMain = opCar->GetDown(); if (!opMain) return EXECUTIONRESULT_OK;
		opWheel1 = opMain->GetNext(); if (!opWheel1) return EXECUTIONRESULT_OK;
		opWheel2 = opWheel1->GetNext(); if (!opWheel2) return EXECUTIONRESULT_OK;
		
		// Get positions
		vMainPos = pathSplineMg * opPathSpline->GetSplinePoint(_sldPath->UniformToNatural(rMainOff));
		vWheel1Pos = vMainPos;
		vWheel2Pos = pathSplineMg * opPathSpline->GetSplinePoint(_sldPath->UniformToNatural(rWheel2Off)); // wheel2Pos will get a value later
		
		if (opRailSpline)
		{
			// Get rail spline matrix
			Matrix railSplineMg(opRailSpline->GetMg());
			
			// Get UpVectors from Rail Spline
			vMainUp = railSplineMg * opRailSpline->GetSplinePoint(_sldRail->UniformToNatural(rMainOff));
			vWheel1Up = railSplineMg * opRailSpline->GetSplinePoint(_sldRail->UniformToNatural(rWheel1Off));
			vWheel2Up = railSplineMg * opRailSpline->GetSplinePoint(_sldRail->UniformToNatural(rWheel2Off));
		}
		else
		{
			// If no Rail Spline is used, assume default UpVectors (+Y)
			vMainUp = vMainPos + Vector(0.0, 1.0, 0.0);
			vWheel1Up = vWheel1Pos + Vector(0.0, 1.0, 0.0);
			vWheel2Up = vWheel2Pos + Vector(0.0, 1.0, 0.0);
		}
		
		// Get Car's parts' scales
		vCarScale = opCar->GetRelScale();
		vMainScale = opMain->GetRelScale();
		vWheel1Scale = opWheel1->GetRelScale();
		vWheel2Scale = opWheel2->GetRelScale();
		
		// Align Matrices
		mWheel1Mtx = Target(vWheel1Pos, vWheel1Pos + vWheel1Tangent, vWheel1Up);	// Align Wheel 1 to spline tangentially
		mWheel2Mtx = Target(vWheel2Pos, vWheel2Pos + vWheel2Tangent, vWheel2Up);	// Align Wheel 2 to spline tangentially
		mMainMtx = Target(vMainPos, vWheel2Pos, vMainUp);													// Target opMain part to Wheel 2
		
		// Positions
		mMainMtx.off = vMainPos;
		mWheel1Mtx.off = vWheel1Pos;
		mWheel2Mtx.off = vWheel2Pos;
		
		// Pass matrices back to Car's parts
		opCar->SetMg(mMainMtx);
		opMain->SetMg(mMainMtx);
		opWheel1->SetMg(mWheel1Mtx);
		opWheel2->SetMg(mWheel2Mtx);
		
		// Set scales back to original values
		opCar->SetRelScale(vCarScale);
		opMain->SetRelScale(vMainScale);
		opWheel1->SetRelScale(vWheel1Scale);
		opWheel2->SetRelScale(vWheel2Scale);
		
		// Remember rOffset for next car
		rCurrentOffset -= rRelCarLength;
		
		// Continue with next car
		opCar = opCar->GetNext();
	}
	
	// Write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, String::IntToString(lCarCount)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, String::FloatToString(rPathLength, 0, 3, false, 0)));
	bc->SetString(TRAIN_MAIN_ERROR, String());
	
	// Execution done
	return EXECUTIONRESULT_OK;
	
	
	///////////////////////////////////////////////////////////////////////////
	// Error handlers
	///////////////////////////////////////////////////////////////////////////
	
Error_Car:
	// Execution failed because of invalid car group, write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, String::IntToString(lCarCount)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, String::FloatToString(rPathLength, 0, 3, false, 0)));
	bc->SetString(TRAIN_MAIN_ERROR,  GeLoadString(TRAIN_INFO_ERROR_CAR, String::IntToString(lCarCount), opCar->GetName()));
	
	return EXECUTIONRESULT_USERBREAK;
	
Error_PathLength:
	// Execution failed because of invalid car group, write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, String::IntToString(lCarCount)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, String::FloatToString(rPathLength, 0, 3, false, 0)));
	bc->SetString(TRAIN_MAIN_ERROR,  GeLoadString(TRAIN_INFO_ERROR_PATHLENGTH));
	
	return EXECUTIONRESULT_USERBREAK;
	
Error_NoCars:
	// Execution failed because of invalid car group, write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, String::IntToString(0)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, String::FloatToString(rPathLength, 0, 3, false, 0)));
	bc->SetString(TRAIN_MAIN_ERROR,  GeLoadString(TRAIN_INFO_ERROR_NOCARS));
	
	return EXECUTIONRESULT_USERBREAK;
}

///////////////////////////////////////////////////////////////////////////
// Register function
///////////////////////////////////////////////////////////////////////////

// Add a car as last element to the train group
Bool TrainDriverMain::AddCar(BaseContainer *bc, BaseObject *op)
{
	if (!bc || !op) return false;
	
	// Create new nullptr objects
	BaseObject *newCar = BaseObject::Alloc(Onull);
	if (!newCar)
		return false;
	
	BaseObject *carPartMain = BaseObject::Alloc(Onull);
	if (!carPartMain)
		return false;
	
	BaseObject *carPartWheel1 = BaseObject::Alloc(Onull);
	if (!carPartWheel1)
		return false;
	
	BaseObject *carPartWheel2 = BaseObject::Alloc(Onull);
	if (!carPartWheel2)
		return false;
	
	// Count existing elements
	Int32 carCount = 0;
	BaseObject *nextCar = op->GetDown();
	while (nextCar)
	{
		carCount++;
		nextCar = nextCar->GetNext();
	}
	
	// Name objects
	newCar->SetName(GeLoadString(TRAIN_OP_CAR) + " " + String::IntToString(carCount + 1));
	carPartMain->SetName(GeLoadString(TRAIN_OP_MAIN));
	carPartWheel1->SetName(GeLoadString(TRAIN_OP_WHEEL) + " 1");
	carPartWheel2->SetName(GeLoadString(TRAIN_OP_WHEEL) + " 2");
	
	// Create new Car tag
	BaseTag	*newCarTag = BaseTag::Alloc(ID_TRAINDRIVERCAR);
	if (!newCarTag)
		return false;
	
	// Set car attributes (copy default values from main tag)
	BaseContainer *tagBc = newCarTag->GetDataInstance(); if (!tagBc) return false;
	tagBc->SetFloat(TRAIN_CAR_LENGTH, bc->GetFloat(TRAIN_MAIN_LENGTH, 30.0));
	tagBc->SetFloat(TRAIN_CAR_WHEELDISTANCE, bc->GetFloat(TRAIN_MAIN_WHEELDISTANCE, 25.0));
	
	// Build car hierarchy
	carPartMain->InsertUnderLast(newCar);
	carPartWheel1->InsertUnderLast(newCar);
	carPartWheel2->InsertUnderLast(newCar);
	
	// Attach Car tag to group
	newCar->InsertTag(newCarTag);
	
	// Insert group under op
	newCar->InsertUnderLast(op);
	
	// Update train
	op->Message(MSG_UPDATE);
	
	return true;
}


Bool RegisterTrainDriverMain()
{
	// decide by name if the plugin shall be registered - just for user convenience
	String name = GeLoadString(IDS_TRAINDRIVERMAIN); if (!name.Content()) return true;
	return RegisterTagPlugin(ID_TRAINDRIVERMAIN, name, TAG_EXPRESSION|TAG_VISIBLE, TrainDriverMain::Alloc, "tTrainDriverMain", AutoBitmap("TrainDriverMain.tif"), 0);
}
