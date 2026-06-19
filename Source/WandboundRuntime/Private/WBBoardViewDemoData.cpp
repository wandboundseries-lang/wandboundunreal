#include "WBBoardViewDemoData.h"

FWBPublicBoardSummary WBBoardViewDemoData::MakeSmallDemoBoardSummary()
{
	FWBPublicBoardSummary Summary;
	Summary.BoardWidth = 9;
	Summary.BoardHeight = 9;
	Summary.DefaultTerrainId = FName(TEXT("Normal"));

	FWBPublicUnitBoardSummary PlayerUnit;
	PlayerUnit.UnitId = 1;
	PlayerUnit.OwnerId = 0;
	PlayerUnit.CardId = TEXT("demo_player_unit");
	PlayerUnit.X = 3;
	PlayerUnit.Y = 4;
	PlayerUnit.HP = 5;
	PlayerUnit.MaxHP = 5;
	PlayerUnit.ATK = 2;
	PlayerUnit.AR = 1;
	PlayerUnit.AttacksLeft = 1;
	Summary.Units.Add(PlayerUnit);

	FWBPublicUnitBoardSummary OpponentUnit;
	OpponentUnit.UnitId = 2;
	OpponentUnit.OwnerId = 1;
	OpponentUnit.CardId = TEXT("demo_opponent_unit");
	OpponentUnit.X = 5;
	OpponentUnit.Y = 4;
	OpponentUnit.HP = 4;
	OpponentUnit.MaxHP = 5;
	OpponentUnit.CurrentArmor = 1;
	OpponentUnit.MaxArmor = 2;
	OpponentUnit.ATK = 1;
	OpponentUnit.AR = 1;
	OpponentUnit.AttacksLeft = 1;
	Summary.Units.Add(OpponentUnit);

	FWBPublicWallEdgeSummary Wall;
	Wall.AX = 4;
	Wall.AY = 4;
	Wall.BX = 4;
	Wall.BY = 5;
	Wall.Orientation = FName(TEXT("vertical"));
	Summary.Walls.Add(Wall);

	FWBPublicTerrainTileSummary Mud;
	Mud.X = 2;
	Mud.Y = 2;
	Mud.TerrainId = FName(TEXT("mud"));
	Summary.TerrainTiles.Add(Mud);

	FWBPublicTerrainTileSummary Ice;
	Ice.X = 6;
	Ice.Y = 6;
	Ice.TerrainId = FName(TEXT("ice"));
	Summary.TerrainTiles.Add(Ice);

	return Summary;
}
