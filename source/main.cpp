#include "c4d_general.h"
#include "c4d_resource.h"
#include "c4d_plugin.h"

#include "main.h"


Bool PluginStart()
{
	GePrint("TrainDriver 1.2.1"_s);
	if (!RegisterTrainDriverTag())
		return false;
	if (!RegisterTrainDriverCarTag())
		return false;
	
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
			if (!g_resource.Init())
				return false;
			return true;
	}
	
	return true;
}
