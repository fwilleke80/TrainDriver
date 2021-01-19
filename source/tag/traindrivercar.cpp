#include "lib_description.h"
#include "c4d_tagplugin.h"
#include "c4d_tagdata.h"
#include "c4d_basetag.h"
#include "c4d_basebitmap.h"
#include "c4d_general.h"
#include "c4d_resource.h"
#include "ge_prepass.h"

#include "main.h"
#include "pluginids.h"

#include "ttraindrivercar.h"
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


Bool TrainDriverCarTag::Init(GeListNode *node)
{
	BaseTag* tag = static_cast<BaseTag*>(node);
	if (!tag)
		return false;
	BaseContainer& dataRef = tag->GetDataInstanceRef();

	dataRef.SetFloat(TRAIN_CAR_LENGTH, 30.0);
	dataRef.SetFloat(TRAIN_CAR_WHEELDISTANCE, 25.0);

	return true;
}

Bool RegisterTrainDriverCarTag()
{
	return RegisterTagPlugin(ID_TRAINDRIVERCAR, GeLoadString(IDS_TRAINDRIVERCAR), TAG_VISIBLE, TrainDriverCarTag::Alloc, "ttraindrivercar"_s, AutoBitmap("ttraindrivercar.tif"_s), 0);
}
