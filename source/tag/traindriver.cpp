#include "lib_description.h"
#include "lib_splinehelp.h"
#include "customgui_priority.h"
#include "c4d_tagplugin.h"
#include "c4d_tagdata.h"
#include "c4d_basetag.h"
#include "c4d_basebitmap.h"
#include "c4d_general.h"
#include "c4d_resource.h"
#include "ge_prepass.h"

#include "main.h"
#include "pluginids.h"

#include "ttraindriver.h"
#include "ttraindrivercar.h"
#include "texpression.h"
#include "c4d_symbols.h"


class TrainDriverTag : public TagData
{
	INSTANCEOF(TrainDriverTag, TagData)

public:
	virtual Bool Init(GeListNode* node);
	virtual Bool Message(GeListNode* node, Int32 type, void* data);
	virtual EXECUTIONRESULT Execute(BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags);

	static NodeData *Alloc()
	{
		return NewObjClear(TrainDriverTag);
	}

private:
	///
	/// \brief Apends a new carriage to the end of the train
	///
	/// \param[in] trainDriverTagPtr Pointer to the TrainDriver tag's container
	///
	/// return maxon::OK if successful, otherwise a maxon error object
	///
	maxon::Result<void> AddCar(BaseTag* trainDriverTagPtr);

private:
	AutoAlloc<SplineLengthData>	_pathSplineHelp;
	AutoAlloc<SplineLengthData>	_railSplineHelp;
};


///
/// \brief Clamps a value between 0.0 and 1.0 with optional circular behavior (like mod)
///
/// \param[in] x The input value
/// \param[in] circular Set to true for circular behavior
///
/// \return The resulting value
///
MAXON_ATTRIBUTE_FORCE_INLINE Float CircularValue(Float x, Bool circular)
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

///
/// \brief Aligns a matrix to a target, using an up vector
///
/// \param[in] pos The position from which to target
/// \param[in] targetPos The target position
/// \param[in] upVector Up vector
///
/// \return The aligned, normalized, orthogonal matrix
///
MAXON_ATTRIBUTE_FORCE_INLINE Matrix Target(const Vector& pos, const Vector& targetPos, const Vector& upVector)
{
	// New matrix
	Matrix m;

	// Calculate matrix alignment
	m.off = pos; // Position
	m.sqmat.v3 = !(targetPos - pos); // Z axis points from pos to targetPos
	m.sqmat.v1 = !Cross(m.sqmat.v3, (pos - upVector)); // X axis is perpendicular to Z axis and up vector
	m.sqmat.v2 = !Cross(m.sqmat.v3, m.sqmat.v1); // Y axis is perpendicular to Z axis and X axis

	return m;
}


Bool TrainDriverTag::Init(GeListNode* node)
{
	// Get tag's container
	BaseTag* tag = static_cast<BaseTag*>(node);
	BaseContainer& dataRef = tag->GetDataInstanceRef();

	// Set default values
	dataRef.SetFloat(TRAIN_MAIN_LENGTH, 30.0);
	dataRef.SetFloat(TRAIN_MAIN_WHEELDISTANCE, 25.0);

	// Set expression priority
	GeData d(CUSTOMGUI_PRIORITY_DATA, DEFAULTVALUE);
	if (node->GetParameter(DescLevel(EXPRESSION_PRIORITY), d, DESCFLAGS_GET::NONE))
	{
		PriorityData* pd = (PriorityData*)d.GetCustomDataType(CUSTOMGUI_PRIORITY_DATA);
		if (pd)
			pd->SetPriorityValue(PRIORITYVALUE_CAMERADEPENDENT, GeData(true));
		node->SetParameter(DescLevel(EXPRESSION_PRIORITY), d, DESCFLAGS_SET::NONE);
	}

	return true;
}

Bool TrainDriverTag::Message(GeListNode* node, Int32 type, void* data)
{
	iferr_scope_handler
	{
		GePrint(err.GetMessage());
		return false;
	};

	BaseTag* trainDriverTag = static_cast<BaseTag*>(node);
	if (!trainDriverTag)
		return true;

	switch (type)
	{
		case MSG_DESCRIPTION_COMMAND:
		{
			// A command button has been clicked. Check if we received the data.
			if (!data)
				iferr_throw(maxon::NullptrError(MAXON_SOURCE_LOCATION, "Received MSG_DESCRIPTION_COMMAND, but data is NULL!"_s));

			DescriptionCommand* dc = (DescriptionCommand*)data;
			if (dc->_descId[0].id == TRAIN_MAIN_CMD_ADDCAR)
			{
				AddCar(trainDriverTag) iferr_return;
			}
			break;
		}
	}

	return true;
}

// Work on the train
EXECUTIONRESULT TrainDriverTag::Execute(BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags)
{
	if (!tag || !doc || !op || !_pathSplineHelp || !_railSplineHelp)
		return EXECUTIONRESULT::OUTOFMEMORY;
	
	// Get container & parameters
	BaseContainer& trainDataRef = tag->GetDataInstanceRef();
	SplineObject* pathSpline	= static_cast<SplineObject*>(trainDataRef.GetObjectLink(TRAIN_MAIN_PATHSPLINE, doc));
	SplineObject* railSpline	= static_cast<SplineObject*>(trainDataRef.GetObjectLink(TRAIN_MAIN_RAILSPLINE, doc));
	const Float offset = trainDataRef.GetFloat(TRAIN_MAIN_POSITION);
	const Bool circular = trainDataRef.GetBool(TRAIN_MAIN_CIRCULAR);

	// Cancel if no Path Spline is given
	if (!pathSpline)
		return EXECUTIONRESULT::USERBREAK;

	// Default up vector, if user didn't define one
	static const Vector default_UpVector(0.0, 1.0, 0.0);

	// Get real splines
	if (pathSpline->GetRealSpline())
		pathSpline = pathSpline->GetRealSpline();

	if (railSpline->GetRealSpline())
		railSpline = railSpline->GetRealSpline();

	// Init SplineLengthDatas
	if (!_pathSplineHelp->Init(pathSpline, 0, pathSpline->GetPointR()))
		return EXECUTIONRESULT::OUTOFMEMORY;
	const Float pathLength = _pathSplineHelp->GetLength();
	trainDataRef.SetString(TRAIN_MAIN_TRACKLENGTH, GeLoadString(IDS_TRAIN_INFO_TRACKLENGTH, maxon::String::FloatToString(pathLength, 0, 3)));

	// Cancel if path length == 0.0
	if (pathLength == 0.0)
	{
		// Something went wrong; write info into TrainDriver main tag UI
		trainDataRef.SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(IDS_TRAIN_INFO_CARCOUNT, "0"_s));
		trainDataRef.SetString(TRAIN_MAIN_ERROR, GeLoadString(IDS_TRAIN_INFO_ERROR_NOCARS));
	}

	if (railSpline)
	{
		if (!_railSplineHelp->Init(railSpline, 0, railSpline->GetPointR()))
			return EXECUTIONRESULT::OUTOFMEMORY;
	}

	// Get first car
	BaseObject* carObjectPtr = op->GetDown();

	// Cancel if no cars found
	if (!carObjectPtr)
	{
		// Something went wrong; write info into TrainDriver main tag UI
		trainDataRef.SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(IDS_TRAIN_INFO_CARCOUNT, "0"_s));
		trainDataRef.SetString(TRAIN_MAIN_ERROR, GeLoadString(IDS_TRAIN_INFO_ERROR_NOCARS));
	}

	// Iterate cars
	Int32 carCount(0);
	Float currentOffset(offset);
	while (carObjectPtr)
	{
		// Increase car counter
		++carCount;

		//////////////////////////////////////////////////////
		//
		// Get data of current car
		//
		//////////////////////////////////////////////////////

		// Get car object's tag
		BaseTag* carTag = carObjectPtr->GetTag(ID_TRAINDRIVERCAR);

		Float carWheelDistance(0.0);
		Float carLength(0.0);
		if (carTag)
		{
			// Get container from tag
			BaseContainer& carDataRef = carTag->GetDataInstanceRef();
			carWheelDistance = carDataRef.GetFloat(TRAIN_CAR_WHEELDISTANCE);
			carLength = carDataRef.GetFloat(TRAIN_CAR_LENGTH);
			carTag->SetName(carObjectPtr->GetName());
		}
		else
		{
			// If no tag found, use default values (set in the TrainDriver Main Tag)
			carWheelDistance = trainDataRef.GetFloat(TRAIN_MAIN_WHEELDISTANCE);
			carLength = trainDataRef.GetFloat(TRAIN_MAIN_LENGTH);
		}

		//////////////////////////////////////////////////////
		//
		// Get values relative to spline length
		//
		//////////////////////////////////////////////////////

		// Convert wheel distance and car length to relative values
		const Float relativeCarWheelDistance = carWheelDistance / pathLength;
		const Float relativeCarLength = carLength / pathLength;

		// Calculate relative offsets for car's main part and wheels
		const Float carOffset = CircularValue(currentOffset, circular);
		const Float wheelOffset1 = carOffset;
		const Float wheelOffset2 = CircularValue(wheelOffset1 - relativeCarWheelDistance, true);

		//////////////////////////////////////////////////////
		//
		// Get car components
		//
		//////////////////////////////////////////////////////

		BaseObject* carMainObjectPtr = carObjectPtr->GetDown();
		if (!carMainObjectPtr)
		{
			// Something went wrong; write info into TrainDriver main tag UI
			trainDataRef.SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(IDS_TRAIN_INFO_CARCOUNT, maxon::String::IntToString(carCount)));
			trainDataRef.SetString(TRAIN_MAIN_ERROR, GeLoadString(IDS_TRAIN_INFO_ERROR_CAR));

		}
		BaseObject* carWheel1ObjectPtr = carMainObjectPtr->GetNext();
		if (!carWheel1ObjectPtr)
		{
			// Something went wrong; write info into TrainDriver main tag UI
			trainDataRef.SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(IDS_TRAIN_INFO_CARCOUNT, maxon::String::IntToString(carCount)));
			trainDataRef.SetString(TRAIN_MAIN_ERROR, GeLoadString(IDS_TRAIN_INFO_ERROR_CAR));

		}
		BaseObject* carWheel2ObjectPtr = carWheel1ObjectPtr->GetNext();
		if (!carWheel2ObjectPtr)
		{
			// Something went wrong; write info into TrainDriver main tag UI
			trainDataRef.SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(IDS_TRAIN_INFO_CARCOUNT, maxon::String::IntToString(carCount)));
			trainDataRef.SetString(TRAIN_MAIN_ERROR, GeLoadString(IDS_TRAIN_INFO_ERROR_CAR));
		}

		//////////////////////////////////////////////////////
		//
		// Calculate spline tangents
		//
		//////////////////////////////////////////////////////

		// Get path spline matrix
		const Matrix pathSplineMatrix(pathSpline->GetMg());

		// Calculate tangents
		const Vector carWheel1Tangent = pathSplineMatrix.sqmat * pathSpline->GetSplineTangent(_pathSplineHelp->UniformToNatural(wheelOffset1));
		const Vector carWheel2Tangent = pathSplineMatrix.sqmat * pathSpline->GetSplineTangent(_pathSplineHelp->UniformToNatural(wheelOffset2));

		//////////////////////////////////////////////////////
		//
		// Calculate car component positions
		//
		//////////////////////////////////////////////////////

		// Calculate car component positions
		const Vector carMainPosition(pathSplineMatrix * pathSpline->GetSplinePoint(_pathSplineHelp->UniformToNatural(carOffset)));
		const Vector carWheel1Position(carMainPosition);
		const Vector carWheel2Position(pathSplineMatrix * pathSpline->GetSplinePoint(_pathSplineHelp->UniformToNatural(wheelOffset2)));

		//////////////////////////////////////////////////////
		//
		// Calculate up vectors
		//
		//////////////////////////////////////////////////////

		// Calculate car component up vectors
		Vector carMainUpVector;
		Vector carWheel1UpVector;
		Vector carWheel2UpVector;
		if (railSpline)
		{
			// Get rail spline matrix
			const Matrix railSplineMatrix(railSpline->GetMg());

			// Get up vectors from rail spline
			carMainUpVector = railSplineMatrix * railSpline->GetSplinePoint(_railSplineHelp->UniformToNatural(carOffset));
			carWheel1UpVector = railSplineMatrix * railSpline->GetSplinePoint(_railSplineHelp->UniformToNatural(wheelOffset1));
			carWheel2UpVector = railSplineMatrix * railSpline->GetSplinePoint(_railSplineHelp->UniformToNatural(wheelOffset2));
		}
		else
		{
			// If no rail spline is used, use default up vector
			carMainUpVector = carMainPosition + default_UpVector;
			carWheel1UpVector = carWheel1Position + default_UpVector;
			carWheel2UpVector = carWheel2Position + default_UpVector;
		}

		//////////////////////////////////////////////////////
		//
		// Calculate aligned component matrices
		//
		//////////////////////////////////////////////////////

		// Memorize car components' scales (they will be reset to unit scale by the Target() function)
		const Vector carScale(carObjectPtr->GetRelScale());
		const Vector carMainScale(carMainObjectPtr->GetRelScale());
		const Vector carWheel1Scale(carWheel1ObjectPtr->GetRelScale());
		const Vector carWheel2Scale(carWheel2ObjectPtr->GetRelScale());

		// Align car component matrices
		Matrix carWheel1Matrix = Target(carWheel1Position, carWheel1Position + carWheel1Tangent, carWheel1UpVector); // Align wheel 1 to spline tangentially
		Matrix carWheel2Matrix = Target(carWheel2Position, carWheel2Position + carWheel2Tangent, carWheel2UpVector); // Align wheel 2 to spline tangentially
		Matrix carMainMatrix = Target(carMainPosition, carWheel2Position, carMainUpVector); // Target main part to wheel 2

		// Car component positions
		carMainMatrix.off = carMainPosition;
		carWheel1Matrix.off = carWheel1Position;
		carWheel2Matrix.off = carWheel2Position;

		//////////////////////////////////////////////////////
		//
		// Set data to all car components
		//
		//////////////////////////////////////////////////////

		// Pass matrices back to car components
		carObjectPtr->SetMg(carMainMatrix);
		carMainObjectPtr->SetMg(carMainMatrix);
		carWheel1ObjectPtr->SetMg(carWheel1Matrix);
		carWheel2ObjectPtr->SetMg(carWheel2Matrix);

		// Set scales back to original values
		carObjectPtr->SetRelScale(carScale);
		carMainObjectPtr->SetRelScale(carMainScale);
		carWheel1ObjectPtr->SetRelScale(carWheel1Scale);
		carWheel2ObjectPtr->SetRelScale(carWheel2Scale);

		//////////////////////////////////////////////////////
		//
		// Finalize, get ready for next car
		//
		//////////////////////////////////////////////////////

		// Remember relative offset for next car
		currentOffset -= relativeCarLength;

		// Continue with next car
		carObjectPtr = carObjectPtr->GetNext();
	}

	// Write info into TrainDriver main tag UI
	trainDataRef.SetString(TRAIN_MAIN_CARCOUNT, GeLoadString(IDS_TRAIN_INFO_CARCOUNT, maxon::String::IntToString(carCount)));
	trainDataRef.SetString(TRAIN_MAIN_ERROR, maxon::String());

	// Execution done
	return EXECUTIONRESULT::OK;
}

maxon::Result<void> TrainDriverTag::AddCar(BaseTag* trainDriverTagPtr)
{
	if (!trainDriverTagPtr)
		return maxon::NullptrError(MAXON_SOURCE_LOCATION, "trainDriverTagPtr must not be NULL!"_s);

	// Get main train object (the object that holds the TrainDriver tag)
	BaseObject *trainMainObjectPtr = trainDriverTagPtr->GetObject();
	if (!trainMainObjectPtr)
		return maxon::IllegalStateError(MAXON_SOURCE_LOCATION, "Could not get trainDriverTagPtr's object!"_s);

	// Get TrainDriver tag's container
	const BaseContainer& dataRef = trainDriverTagPtr->GetDataInstanceRef();

	// Create new Null objects
	BaseObject *newCar = BaseObject::Alloc(Onull);
	if (!newCar)
		return maxon::OutOfMemoryError(MAXON_SOURCE_LOCATION, "Couldn't allocate new car object!"_s);

	BaseObject *carPartMain = BaseObject::Alloc(Onull);
	if (!carPartMain)
		return maxon::OutOfMemoryError(MAXON_SOURCE_LOCATION, "Couldn't allocate new car main component!"_s);

	BaseObject *carPartWheel1 = BaseObject::Alloc(Onull);
	if (!carPartWheel1)
		return maxon::OutOfMemoryError(MAXON_SOURCE_LOCATION, "Couldn't allocate new car wheel1 component!"_s);

	BaseObject *carPartWheel2 = BaseObject::Alloc(Onull);
	if (!carPartWheel2)
		return maxon::OutOfMemoryError(MAXON_SOURCE_LOCATION, "Couldn't allocate new car wheel2 component!"_s);

	// Count existing elements
	Int32 carCount = 0;
	BaseObject *nextCar = trainMainObjectPtr->GetDown();
	while (nextCar)
	{
		++carCount;
		nextCar = nextCar->GetNext();
	}

	// Name objects
	newCar->SetName(GeLoadString(IDS_TRAIN_OP_CAR) + " " + String::IntToString(carCount + 1));
	carPartMain->SetName(GeLoadString(IDS_TRAIN_OP_MAIN));
	carPartWheel1->SetName(GeLoadString(IDS_TRAIN_OP_WHEEL) + " 1");
	carPartWheel2->SetName(GeLoadString(IDS_TRAIN_OP_WHEEL) + " 2");

	// Create new Car tag
	BaseTag	*newCarTag = BaseTag::Alloc(ID_TRAINDRIVERCAR);
	if (!newCarTag)
		return maxon::OutOfMemoryError(MAXON_SOURCE_LOCATION, "Couldn't allocate new car tag!"_s);

	// Set car attributes (copy default values from main tag)
	BaseContainer& carDataRef = newCarTag->GetDataInstanceRef();
	carDataRef.SetFloat(TRAIN_CAR_LENGTH, dataRef.GetFloat(TRAIN_MAIN_LENGTH, 30.0));
	carDataRef.SetFloat(TRAIN_CAR_WHEELDISTANCE, dataRef.GetFloat(TRAIN_MAIN_WHEELDISTANCE, 25.0));

	// Build car hierarchy
	carPartMain->InsertUnderLast(newCar);
	carPartWheel1->InsertUnderLast(newCar);
	carPartWheel2->InsertUnderLast(newCar);

	// Attach car tag to group
	newCar->InsertTag(newCarTag);

	// Insert group under trainMainObjectPtr
	newCar->InsertUnderLast(trainMainObjectPtr);

	// Update train main object
	trainMainObjectPtr->Message(MSG_UPDATE);

	return maxon::OK;
}


Bool RegisterTrainDriverTag()
{
	return RegisterTagPlugin(ID_TRAINDRIVERMAIN, GeLoadString(IDS_TRAINDRIVERMAIN), TAG_EXPRESSION|TAG_VISIBLE, TrainDriverTag::Alloc, "ttraindriver"_s, AutoBitmap("ttraindriver.tif"_s), 0);
}
