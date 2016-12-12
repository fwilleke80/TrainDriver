#include "c4d.h"
#include "main.h"


#define PLUGIN_VERSION	String("1.2")

Bool PluginStart()
{
	GePrint("TrainDriver " + PLUGIN_VERSION);
	if (!RegisterTrainDriverMain()) return false;
	if (!RegisterTrainDriverCar()) return false;

	return true;
}

void PluginEnd()
{
}

Bool PluginMessage(Int32 id, void *data)
{
	// React to messages
	switch (id)
	{
		case C4DPL_INIT_SYS:
			// Don't start plugin without resources
			if (!resource.Init()) return false;
			return true;
	}

	return true;
}
