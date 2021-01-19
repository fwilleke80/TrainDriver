/////////////////////////////////////////////////////////////
// TrainDriver :: Main Tag Description Resource
/////////////////////////////////////////////////////////////

CONTAINER ttraindriver
{
	NAME ttraindriver;
	INCLUDE Texpression;

	GROUP TRAIN_MAIN_SETTINGS
	{
		DEFAULT 1;
		
		LINK	TRAIN_MAIN_PATHSPLINE		{ ACCEPT { OSpline; } }
		LINK	TRAIN_MAIN_RAILSPLINE		{ ACCEPT { OSpline; } }
		
		SEPARATOR														{ LINE; }
		
		REAL	TRAIN_MAIN_POSITION						{ UNIT PERCENT; MINSLIDER 0.0; MAXSLIDER 100.0; STEP 0.1; CUSTOMGUI REALSLIDER; }
		BOOL	TRAIN_MAIN_CIRCULAR						{  }
		
		SEPARATOR	TRAIN_MAIN_DEFAULTVALS		{  }
		
		REAL	TRAIN_MAIN_LENGTH							{ UNIT METER; MIN 0.1; STEP 0.1; }
		REAL	TRAIN_MAIN_WHEELDISTANCE			{ UNIT METER; MIN 0.1; STEP 0.1; }

		SEPARATOR TRAIN_MAIN_SPLINESUB			{  }
		BOOL	TRAIN_MAIN_SPLINESUB_ENABLE		{  }
		LONG	TRAIN_MAIN_SPLINESUB_VAL			{ MIN 1; }
		
		SEPARATOR	TRAIN_MAIN_INFO						{  }
		
		STATICTEXT	TRAIN_MAIN_CARCOUNT			{  }
		STATICTEXT	TRAIN_MAIN_TRACKLENGTH	{  }
		STATICTEXT	TRAIN_MAIN_ERROR				{  }

		SEPARATOR														{ LINE; }

		BUTTON	TRAIN_MAIN_CMD_ADDCAR				{  }
	}
}
