#include "c4d_tagplugin.h"
#include "c4d_tagdata.h"
#include "c4d_general.h"
#include "ge_prepass.h"

#include "ttraindrivercar.h"

#include "main.h"
#include "pluginids.h"
#include "c4d_symbols.h"


class TrainDriverCarTag : public TagData
{
public:
	virtual Bool Init(GeListNode *node);

	static NodeData *Alloc()
	{
		return NewObjClear(TrainDriverCarTag);
	}
};


// Set default values
Bool TrainDriverCarTag::Init(GeListNode *node)
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
	String name = GeLoadString(IDS_TRAINDRIVERCAR);
	if (!name.Content())
		return true;

	return RegisterTagPlugin(ID_TRAINDRIVERCAR, name, TAG_VISIBLE, TrainDriverCar::Alloc, "ttraindrivercar", AutoBitmap("ttraindrivercar.tif"_s), 0);
}
