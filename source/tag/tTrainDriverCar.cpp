#include "c4d.h"
#include "c4d_symbols.h"
#include "tTrainDriverCar.h"
#include "main.h"
#include "pluginids.h"


class TrainDriverCar : public TagData
{
	public:
		virtual Bool Init(GeListNode *node);

		static NodeData *Alloc(void) { return NewObjClear(TrainDriverCar); }
};


// Set default values
Bool TrainDriverCar::Init(GeListNode *node)
{
	// Get pointer to tag's Container
	BaseTag				*tag  = (BaseTag*)node;
	if (!tag)
		return false;
	BaseContainer	*data = tag->GetDataInstance();
	if (!data)
		return false;

	// Set default values in user interface
	data->SetFloat(TRAIN_CAR_LENGTH, 30.0);
	data->SetFloat(TRAIN_CAR_WHEELDISTANCE, 25.0);

	return true;
}

///////////////////////////////////////////////////////////////////////////
// Register function
///////////////////////////////////////////////////////////////////////////
Bool RegisterTrainDriverCar()
{
	// decide by name if the plugin shall be registered - just for user convenience
	String name = GeLoadString(IDS_TRAINDRIVERCAR); if (!name.Content()) return true;
	return RegisterTagPlugin(ID_TRAINDRIVERCAR, name, TAG_VISIBLE, TrainDriverCar::Alloc, "tTrainDriverCar", AutoBitmap("TrainDriverCar.tif"), 0);
}
