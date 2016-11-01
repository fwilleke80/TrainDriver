// Unique Plugin IDs obtained from www.plugincafe.com
#define ID_TRAINDRIVERMAIN	1023458
#define ID_TRAINDRIVERCAR		1023459


///////////////////////////////////////////////////////////////////////////
// Includes
///////////////////////////////////////////////////////////////////////////

// Include some stuff from the API
#include "c4d.h"
#include "c4d_symbols.h"
#include "tTrainDriverMain.h"
#include "tTrainDriverCar.h"
#include "customgui_priority.h"
#include "lib_splinehelp.h"


///////////////////////////////////////////////////////////////////////////
// Plugin Class declaration
///////////////////////////////////////////////////////////////////////////
class TrainDriverMain : public TagData
{
	INSTANCEOF(TrainDriverMain, TagData)

	private:
		Bool AddCar(BaseContainer *bc, BaseObject *op);

	public:
		virtual Bool Init(GeListNode *node);
		virtual Bool Message(GeListNode *node, LONG type, void *data);
		virtual EXECUTIONRESULT Execute(BaseTag *tag, BaseDocument *doc, BaseObject *op, BaseThread *bt, LONG priority, EXECUTIONFLAGS flags);

		static NodeData *Alloc(void) { return gNew TrainDriverMain; }
};


///////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////

// Clamps a value between 0.0 and 1.0 with circular behavior
static Real CircularValue(Real x, Bool Circular)
{
	if (Circular)
	{
		if(x > RCO 1.0)
			return x - Floor(x);
		else if(x < RCO 0.0)
			return x - Floor(x);
	}
	else
		return Clamp(RCO 0.0, RCO 1.0, x);

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
	m.v1 = !(m.v3 % (vPos - vUpVector));
	m.v2 = !(m.v3 % m.v1);

	// Return result
	return m;
}

// Subdivide the spline
static SplineObject	*SubDivideSpline(SplineObject *op, LONG lSubdivs)
{
	return NULL;
}


///////////////////////////////////////////////////////////////////////////
// Public class member functions
///////////////////////////////////////////////////////////////////////////

// Set default values
Bool TrainDriverMain::Init(GeListNode *node)
{
	// Get pointer to tag's Container
	BaseTag				*tag	= (BaseTag*)node;
	BaseContainer	*data	= tag->GetDataInstance();

	// Set default values in user interface
	data->SetReal(TRAIN_MAIN_LENGTH, RCO 30.0);
	data->SetReal(TRAIN_MAIN_WHEELDISTANCE, RCO 25.0);

	// Set expression's priority bc
	GeData d(CUSTOMGUI_PRIORITY_DATA, DEFAULTVALUE);
	if (node->GetParameter(DescLevel(EXPRESSION_PRIORITY), d, DESCFLAGS_GET_0))
	{
		PriorityData *pd = (PriorityData*)d.GetCustomDataType(CUSTOMGUI_PRIORITY_DATA);
		if (pd) pd->SetPriorityValue(PRIORITYVALUE_CAMERADEPENDENT, GeData(TRUE));
		node->SetParameter(DescLevel(EXPRESSION_PRIORITY), d, DESCFLAGS_SET_0);
	}

	return TRUE;
}


// Recieve and handle messages
Bool TrainDriverMain::Message(GeListNode *node, LONG type, void *data)
{
	BaseTag *tag = (BaseTag*)node; if(!tag) return TRUE;
	BaseObject *op = tag->GetObject(); if (!op) return TRUE;
	BaseContainer *bc = tag->GetDataInstance(); if (!bc) return TRUE;

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

	return TRUE;
}


// Work on the train
EXECUTIONRESULT TrainDriverMain::Execute(BaseTag *tag, BaseDocument *doc, BaseObject *op, BaseThread *bt, LONG priority, EXECUTIONFLAGS flags)
{
	// Get Container from Tag
	BaseContainer *bc = tag->GetDataInstance();

	// Get settings from user interface
	SplineObject	*opPathSpline	= (SplineObject*)bc->GetObjectLink(TRAIN_MAIN_PATHSPLINE, doc);
	SplineObject	*opRailSpline	= (SplineObject*)bc->GetObjectLink(TRAIN_MAIN_RAILSPLINE, doc);
	Real					rOffset				= bc->GetReal(TRAIN_MAIN_POSITION);
	Bool					bCircular			= bc->GetBool(TRAIN_MAIN_CIRCULAR);

	// Cancel if no Path Spline is given
	if (!opPathSpline) return EXECUTIONRESULT_OUTOFMEMORY;

	// Variables
	Real				rPathLength(DC);

	Real				rRelCarWheelDist(DC);
	Real				rRelCarLength(DC);

	Real				rMainOff(DC);
	Real				rWheel1Off(DC);
	Real				rWheel2Off(DC);

	Vector			vMainTangent(DC);
	Vector			vWheel1Tangent(DC);
	Vector			vWheel2Tangent(DC);

	BaseObject	*opMain		= NULL;
	BaseObject	*opWheel1	= NULL;
	BaseObject	*opWheel2	= NULL;

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

	BaseContainer	*bcCarData		= NULL;
	BaseObject		*opCar				= NULL;
	BaseTag				*btCarTag			= NULL;

	LONG				lCarCount				= 0;
	Real				rCurrentOffset	= rOffset;	// Used to add up the offset as the cars get chained

	Real				rCarWheelDist(DC);
	Real				rCarLength(DC);

	SplineLengthData *sld_path = NULL;
	SplineLengthData *sld_rail = NULL;

	// Get real splines
	if (opPathSpline->GetRealSpline())
		opPathSpline = opPathSpline->GetRealSpline();

	if (opRailSpline->GetRealSpline())
		opRailSpline = opRailSpline->GetRealSpline();

	// Create SplineHelp
	sld_path = SplineLengthData::Alloc();
	if (!sld_path) return EXECUTIONRESULT_OUTOFMEMORY;
	if (!sld_path->Init(opPathSpline, 0, opPathSpline->GetPointR())) return EXECUTIONRESULT_OUTOFMEMORY;
	rPathLength = sld_path->GetLength();

	if (opRailSpline)
	{
		sld_rail = SplineLengthData::Alloc();
		if (!sld_rail) return EXECUTIONRESULT_OUTOFMEMORY;
		if (!sld_rail->Init(opRailSpline, 0, opRailSpline->GetPointR())) return EXECUTIONRESULT_OUTOFMEMORY;
	}

	// Cancel if path length == 0
	if (rPathLength == RCO 0.0) goto Error_PathLength;

	// Get first car
	opCar = op->GetDown();

	// Cancel if there's no car
	if (!opCar) goto Error_NoCars;

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
			rCarWheelDist	= bcCarData->GetReal(TRAIN_CAR_WHEELDISTANCE);
			rCarLength			= bcCarData->GetReal(TRAIN_CAR_LENGTH);
			btCarTag->SetName(opCar->GetName());
		}
		else
		{
			rCarWheelDist = bc->GetReal(TRAIN_MAIN_WHEELDISTANCE);
			rCarLength    = bc->GetReal(TRAIN_MAIN_LENGTH);
		}

		// Convert Wheel Distance and opCar Length to percent values
		rRelCarWheelDist = rCarWheelDist / rPathLength;
		rRelCarLength = rCarLength / rPathLength;

		// Calculate relative offsets for opCar's main part and wheels
		rMainOff		= CircularValue(rCurrentOffset, bCircular);
		rWheel1Off	= rMainOff;
		rWheel2Off	= CircularValue(rWheel1Off - rRelCarWheelDist, TRUE);

		// Get car's parts
		opMain		= opCar->GetDown(); if (!opMain) goto Error_Car;
		opWheel1	= opMain->GetNext(); if (!opWheel1) goto Error_Car;
		opWheel2	= opWheel1->GetNext(); if (!opWheel2) goto Error_Car;

		// Get tangents
		vMainTangent = opPathSpline->GetSplineTangent(sld_path->UniformToNatural(rMainOff)) ^ opPathSpline->GetMg();
		vWheel1Tangent = opPathSpline->GetSplineTangent(sld_path->UniformToNatural(rWheel1Off)) ^ opPathSpline->GetMg();
		vWheel2Tangent = opPathSpline->GetSplineTangent(sld_path->UniformToNatural(rWheel2Off)) ^ opPathSpline->GetMg();

		// Position the car's parts
		opMain = opCar->GetDown(); if (!opMain) return EXECUTIONRESULT_OK;
		opWheel1 = opMain->GetNext(); if (!opWheel1) return EXECUTIONRESULT_OK;
		opWheel2 = opWheel1->GetNext(); if (!opWheel2) return EXECUTIONRESULT_OK;

		// Get positions
		vMainPos = opPathSpline->GetSplinePoint(sld_path->UniformToNatural(rMainOff)) * opPathSpline->GetMg();
		vWheel1Pos = vMainPos;
		vWheel2Pos = opPathSpline->GetSplinePoint(sld_path->UniformToNatural(rWheel2Off)) * opPathSpline->GetMg(); // wheel2Pos will get a value later

		if (opRailSpline)
		{
			// Get UpVectors from Rail Spline
			vMainUp = opRailSpline->GetSplinePoint(sld_rail->UniformToNatural(rMainOff)) * opRailSpline->GetMg();
			vWheel1Up = opRailSpline->GetSplinePoint(sld_rail->UniformToNatural(rWheel1Off)) * opRailSpline->GetMg();
			vWheel2Up = opRailSpline->GetSplinePoint(sld_rail->UniformToNatural(rWheel2Off)) * opRailSpline->GetMg();
		}
		else
		{
			// If no Rail Spline is used, assume default UpVectors (+Y)
			vMainUp = vMainPos + Vector(RCO 0.0, RCO 1.0, RCO 0.0);
			vWheel1Up = vWheel1Pos + Vector(RCO 0.0, RCO 1.0, RCO 0.0);
			vWheel2Up = vWheel2Pos + Vector(RCO 0.0, RCO 1.0, RCO 0.0);
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

	// Free spline length(s)
	SplineLengthData::Free(sld_path);
	SplineLengthData::Free(sld_rail);

	// Write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, LongToString(lCarCount)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, RealToString(rPathLength, 0, 3, FALSE, 0)));
	bc->SetString(TRAIN_MAIN_ERROR, String());

	// Execution done
	return EXECUTIONRESULT_OK;


///////////////////////////////////////////////////////////////////////////
// Error handlers
///////////////////////////////////////////////////////////////////////////

Error_Car:
	// Execution failed because of invalid car group, write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, LongToString(lCarCount)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, RealToString(rPathLength, 0, 3, FALSE, 0)));
	bc->SetString(TRAIN_MAIN_ERROR,  GeLoadString(TRAIN_INFO_ERROR_CAR, LongToString(lCarCount), opCar->GetName()));

	return EXECUTIONRESULT_OUTOFMEMORY;

Error_PathLength:
	// Execution failed because of invalid car group, write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, LongToString(lCarCount)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, RealToString(rPathLength, 0, 3, FALSE, 0)));
	bc->SetString(TRAIN_MAIN_ERROR,  GeLoadString(TRAIN_INFO_ERROR_PATHLENGTH));

	return EXECUTIONRESULT_OUTOFMEMORY;

Error_NoCars:
	// Execution failed because of invalid car group, write info into User Interface
	bc->SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(TRAIN_INFO_CARCOUNT, LongToString(0)));
	bc->SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(TRAIN_INFO_TRACKLENGTH, RealToString(rPathLength, 0, 3, FALSE, 0)));
	bc->SetString(TRAIN_MAIN_ERROR,  GeLoadString(TRAIN_INFO_ERROR_NOCARS));

	return EXECUTIONRESULT_OUTOFMEMORY;
}

///////////////////////////////////////////////////////////////////////////
// Register function
///////////////////////////////////////////////////////////////////////////

// Add a car as last element to the train group
Bool TrainDriverMain::AddCar(BaseContainer *bc, BaseObject *op)
{
	if (!bc || !op) return FALSE;

	// Create new Null objects
	BaseObject *car = BaseObject::Alloc(Onull); if (!car) return FALSE;
	BaseObject *main = BaseObject::Alloc(Onull); if (!main) return FALSE;
	BaseObject *wheel1 = BaseObject::Alloc(Onull); if (!wheel1) return FALSE;
	BaseObject *wheel2 = BaseObject::Alloc(Onull); if (!wheel2) return FALSE;

	// Count existing elements
	LONG lCount = 0;
	BaseObject *ccar = op->GetDown();
	while (ccar)
	{
		lCount++;
		ccar = ccar->GetNext();
	}

	// Name objects
	car->SetName(GeLoadString(TRAIN_OP_CAR) + " " + LongToString(lCount + 1));
	main->SetName(GeLoadString(TRAIN_OP_MAIN));
	wheel1->SetName(GeLoadString(TRAIN_OP_WHEEL) + " 1");
	wheel2->SetName(GeLoadString(TRAIN_OP_WHEEL) + " 2");

	// Create new Car tag
	BaseTag	*tag = BaseTag::Alloc(ID_TRAINDRIVERCAR); if (!tag) return FALSE;

	// Set car attributes (copy default values from main tag)
	BaseContainer *tagbc = tag->GetDataInstance(); if (!tagbc) return FALSE;
	tagbc->SetReal(TRAIN_CAR_LENGTH, bc->GetReal(TRAIN_MAIN_LENGTH, RCO 30.0));
	tagbc->SetReal(TRAIN_CAR_WHEELDISTANCE, bc->GetReal(TRAIN_MAIN_WHEELDISTANCE, RCO 25.0));

	// Build car hierarchy
	main->InsertUnderLast(car);
	wheel1->InsertUnderLast(car);
	wheel2->InsertUnderLast(car);

	// Attach Car tag to group
	car->InsertTag(tag);

	// Insert group under op
	car->InsertUnderLast(op);

	// Update train
	op->Message(MSG_UPDATE);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////
// Register function
///////////////////////////////////////////////////////////////////////////
Bool RegisterTrainDriverMain(void)
{
	// decide by name if the plugin shall be registered - just for user convenience
	String name=GeLoadString(IDS_TRAINDRIVERMAIN); if (!name.Content()) return TRUE;
	return RegisterTagPlugin(ID_TRAINDRIVERMAIN, name, TAG_EXPRESSION|TAG_VISIBLE, TrainDriverMain::Alloc, "tTrainDriverMain", AutoBitmap("TrainDriverMain.tif"), 0);
}
