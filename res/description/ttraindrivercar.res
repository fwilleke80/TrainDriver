/////////////////////////////////////////////////////////////
// TrainDriver :: Carriage Tag Description Resource
/////////////////////////////////////////////////////////////

CONTAINER ttraindrivercar
{
	NAME ttraindrivercar;

	GROUP TRAIN_CAR_PROPERTIES
	{
		DEFAULT 1;

		REAL	TRAIN_CAR_LENGTH			{ UNIT METER; MIN 0.0; STEP 0.1; }
		REAL	TRAIN_CAR_WHEELDISTANCE		{ UNIT METER; MIN 0.0; STEP 0.1; }
	}
}
