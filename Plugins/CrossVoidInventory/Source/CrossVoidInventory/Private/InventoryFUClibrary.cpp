#include "InventoryFUClibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetTextLibrary.h"

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYName(TArray<UInventoryBaseItem*> MapsIN, FText Name,
                                                                    bool Precise)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Precise)
		{
			if (UKismetTextLibrary::EqualEqual_IgnoreCase_TextText(Element->ItemData.Name, Name))
			{
				Result.Add(Element);
			}
		}
		else
		{
			if (UKismetStringLibrary::Contains(Element->ItemData.Name.ToString(), Name.ToString(), false, false))
			{
				Result.Add(Element);
			}
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYClass(TArray<UInventoryBaseItem*> MapsIN,
                                                                     TSubclassOf<UInventoryBaseItem> Class)
{
	TArray<UInventoryBaseItem*> Result;

	for (const auto& Element : MapsIN)
	{
		if (Element->GetClass() == Class)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYKeyWord(TArray<UInventoryBaseItem*> MapsIN,
                                                                       TArray<FString> KeyWord)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		for (const auto& Key : KeyWord)
		{
			if (Key.IsEmpty())
			{
				continue;
			}
			else
			{
				for (const auto& KeyElement : Element->ItemData.KeyWords)
				{
					if (UKismetStringLibrary::Contains(KeyElement, Key))
					{
						Result.Add(Element);
					}
				}
			}
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYCount(TArray<UInventoryBaseItem*> MapsIN,
                                                                     FIntPoint CountRange)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Count >= CountRange.X && Element->ItemData.Count <= CountRange.Y)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYLocation(TArray<UInventoryBaseItem*> MapsIN,
                                                                        FVector2D Location)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Location.X >= 0)
		{
			if (Element->ItemData.Location.X == Location.X)
			{
				if (Location.Y >= 0)
				{
					if (Element->ItemData.Location.Y == Location.Y)
					{
						Result.Add(Element);
					}
				}
				else
				{
					Result.Add(Element);
				}
			}
		}
		else
		{
			if (Location.Y >= 0)
			{
				if (Element->ItemData.Location.Y == Location.Y)
				{
					Result.Add(Element);
				}
			}
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYQuality(TArray<UInventoryBaseItem*> MapsIN,
                                                                       FIntPoint QualityRange)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Quality >= QualityRange.X && Element->ItemData.Quality <= QualityRange.Y)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYWeight(TArray<UInventoryBaseItem*> MapsIN,
                                                                      FVector2D WeightRange)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Weight >= WeightRange.X && Element->ItemData.Weight <= WeightRange.Y)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYPrice(TArray<UInventoryBaseItem*> MapsIN,
                                                                     FIntPoint PriceRange)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Price >= PriceRange.X && Element->ItemData.Price <= PriceRange.Y)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYType(TArray<UInventoryBaseItem*> MapsIN,
                                                                    UPDA_InventoryType* Type)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Type == Type)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYDurability(TArray<UInventoryBaseItem*> MapsIN,
                                                                          FIntPoint DurabilityRange)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Durability >= DurabilityRange.X && Element->ItemData.Durability <= DurabilityRange.Y)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYLevel(TArray<UInventoryBaseItem*> MapsIN,
                                                                     FIntPoint LevelRange)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Level >= LevelRange.X && Element->ItemData.Level <= LevelRange.Y)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYDate(TArray<UInventoryBaseItem*> MapsIN,
                                                                    FVector2D YearRange,
                                                                    FVector2D MounthRange, FVector2D DayRange)
{
	TArray<UInventoryBaseItem*> Result;

	int MinYear = (YearRange.X < 1) ? 0 : YearRange.X;
	int MaxYear = (YearRange.Y < 1) ? 9999 : YearRange.Y;

	int MinMonth = (MounthRange.X < 1) ? 0 : MounthRange.X;
	int MaxMonth = (MounthRange.Y < 1) ? 30 : MounthRange.Y;

	int MinDay = (DayRange.X < 1) ? 0 : DayRange.X;
	int MaxDay = (DayRange.Y < 1) ? 90 : DayRange.Y;

	if (YearRange.X >= 1 || YearRange.Y >= 1) //如果年有效
	{
		if (MounthRange.X >= 1 || MounthRange.Y >= 1) //如果月有效
		{
			if (DayRange.X >= 1 || DayRange.Y >= 1) //如果日有效
			{
				for (const auto& Element : MapsIN)
				{
					if (MinYear <= Element->ItemData.Time.GetYear() && Element->ItemData.Time.GetYear() <= MaxYear)
					{
						if (MinMonth <= Element->ItemData.Time.GetMonth() && Element->ItemData.Time.GetMonth() <=
							MaxMonth)
						{
							if (MinDay <= Element->ItemData.Time.GetDay() && Element->ItemData.Time.GetDay() <= MaxDay)
							{
								Result.Add(Element);
							}
						}
					}
				}
			}
			else
			{
				for (const auto& Element : MapsIN)
				{
					if (MinYear <= Element->ItemData.Time.GetYear() && Element->ItemData.Time.GetYear() <= MaxYear)
					{
						if (MinMonth <= Element->ItemData.Time.GetMonth() && Element->ItemData.Time.GetMonth() <=
							MaxMonth)
						{
							Result.Add(Element);
						}
					}
				}
			}
		}
		else
		{
			for (const auto& Element : MapsIN)
			{
				if (MinYear <= Element->ItemData.Time.GetYear() && Element->ItemData.Time.GetYear() <= MaxYear)
				{
					Result.Add(Element);
				}
			}
		}
	}
	else if (MounthRange.X >= 1 || MounthRange.Y >= 1) //如果月有效
	{
		if (DayRange.X >= 1 || DayRange.Y >= 1) //如果日有效
		{
			for (const auto& Element : MapsIN)
			{
				if (MinMonth <= Element->ItemData.Time.GetMonth() && Element->ItemData.Time.GetMonth() <= MaxMonth)
				{
					if (MinDay <= Element->ItemData.Time.GetDay() && Element->ItemData.Time.GetDay() <= MaxDay)
					{
						Result.Add(Element);
					}
				}
			}
		}
		else
		{
			for (const auto& Element : MapsIN)
			{
				if (MinMonth <= Element->ItemData.Time.GetMonth() && Element->ItemData.Time.GetMonth() <= MaxMonth)
				{
					Result.Add(Element);
				}
			}
		}
	}
	else if (DayRange.X >= 1 || DayRange.Y >= 1) //如果日有效
	{
		for (const auto& Element : MapsIN)
		{
			if (MinDay <= Element->ItemData.Time.GetDay() && Element->ItemData.Time.GetDay() <= MaxDay)
			{
				Result.Add(Element);
			}
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYPriority(TArray<UInventoryBaseItem*> MapsIN,
                                                                        FIntPoint PriorityRange)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		if (Element->ItemData.Priority >= PriorityRange.X && Element->ItemData.Priority <= PriorityRange.Y)
		{
			Result.Add(Element);
		}
	}
	return Result;
}

TArray<UInventoryBaseItem*> UInventoryFUClibrary::FilterItemSBYRelatedItem(TArray<UInventoryBaseItem*> MapsIN,
                                                                           TArray<TSubclassOf<UInventoryBaseItem>>
                                                                           Class)
{
	TArray<UInventoryBaseItem*> Result;
	for (const auto& Element : MapsIN)
	{
		for (const auto& ElementIN : Class)
		{
			if (Element->ItemData.RelatedItem.Contains(ElementIN))
			{
				Result.Add(Element);
			}
		}
	}
	return Result;
}

void UInventoryFUClibrary::SwapItemXY(UInventoryBaseItem* Item1, UInventoryBaseItem* Item2)
{
	FIntPoint Buffer = Item1->ItemData.Location;
	Item1->ItemData.Location = Item2->ItemData.Location;
	Item2->ItemData.Location = Buffer;
	Item1->ChangeItemData();
	Item2->ChangeItemData();
}
