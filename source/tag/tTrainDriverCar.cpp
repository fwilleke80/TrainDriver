// Unique Plugin ID obtained from www.plugincafe.com
#define ID_TRAINDRIVERCAR	1023459

///////////////////////////////////////////////////////////////////////////
// Includes
///////////////////////////////////////////////////////////////////////////

// Include some stuff from the API
#include "c4d.h"
#include "c4d_symbols.h"
#include "tTrainDriverCar.h"


///////////////////////////////////////////////////////////////////////////
// Plugin Class declaration
///////////////////////////////////////////////////////////////////////////
class TrainDriverCar : public TagData
{
	public:
		virtual Bool Init(GeListNode *node);

		static NodeData *Alloc(void) { return gNew TrainDriverCar; }
};


///////////////////////////////////////////////////////////////////////////
// Class member functions
///////////////////////////////////////////////////////////////////////////

// Set default values
Bool TrainDriverCar::Init(GeListNode *node)
{
	// Get pointer to tag's Container
	BaseTag			*tag  = (BaseTag*)node; if (!tag) return FALSE;
	BaseContainer	*data = tag->GetDataInstance(); if (!data) return FALSE;

	// Set default values in user interface
	data->SetReal(TRAIN_CAR_LENGTH, RCO 30.0);
	data->SetReal(TRAIN_CAR_WHEELDISTANCE, RCO 25.0);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////
// Register function
///////////////////////////////////////////////////////////////////////////
Bool RegisterTrainDriverCar(void)
{
	// decide by name if the plugin shall be registered - just for user convenience
	String name = GeLoadString(IDS_TRAINDRIVERCAR); if (!name.Content()) return TRUE;
	return RegisterTagPlugin(ID_TRAINDRIVERCAR, name, TAG_VISIBLE, TrainDriverCar::Alloc, "tTrainDriverCar", AutoBitmap("TrainDriverCar.tif"), 0);
}
