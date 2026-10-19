// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/FLMarketData.h"

const FFLMarketGood* UFLMarketData::FindGood(FName GoodNickname) const
{
	return Goods.FindByPredicate([GoodNickname](const FFLMarketGood& G) { return G.Good == GoodNickname; });
}
