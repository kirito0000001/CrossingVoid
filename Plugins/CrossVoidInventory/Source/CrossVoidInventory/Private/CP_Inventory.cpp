#include "CP_Inventory.h"
#include "InventoryFUClibrary.h"
#include "ShaderPrintParameters.h"


//构造
UCP_Inventory::UCP_Inventory()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCP_Inventory::RefreshInventoryDataByFuc()
{
	OnInventoryDataRefresh.Broadcast();
}

UInventoryBaseItem* UCP_Inventory::GiveItemByClass(int& Residue, TSubclassOf<UInventoryBaseItem> InClass, int Count,
                                                   bool UseStack)
{
	int CountMinus = Count; //剩余没分配的
	if (InClass == nullptr)//是无效物品类
	{
		Residue = CountMinus;
		return nullptr;
	}
	//是有效的物品数量吗
	if (Count <= 0)
	{
		Residue = 0;
		return nullptr;
	}
	//用了堆叠吗
	else if (UseStack)
	{
		for (const auto& Element : ItemDatas)
		{
			if (Element->GetClass()->IsChildOf(InClass) || Element->GetClass() == InClass) //但凡找到相同的类了
			{
				if (Element->ItemData.Count + CountMinus > Element->ItemData.MaxCount) //堆叠到这个上面会超过上限
				{
					if (Element->ItemData.Count == Element->ItemData.MaxCount)
					{
						//本来就满，不管了
					}
					else
					{
						CountMinus = CountMinus - (Element->ItemData.MaxCount - Element->ItemData.Count); //堆叠数量减去剩余的空位
						Element->ItemData.Count = Element->ItemData.MaxCount; //这一组设置为上限
						Element->ChangeItemData(); //触发委托，改变数值
					}
				}
				else if (Element->ItemData.Count + CountMinus == Element->ItemData.MaxCount) //刚好堆叠到这上面
				{
					CountMinus = 0; //没剩余了
					Element->ItemData.Count = Element->ItemData.MaxCount; //设置为上限
					Element->ChangeItemData(); //触发委托，改变数值
					Residue = 0; //没剩余了
					return Element; //返回这个物品
				}
				else //堆叠到这个上面不会超过上限，到结尾了
				{
					Element->ItemData.Count += CountMinus; //堆叠数量加上
					Element->ChangeItemData(); //触发委托，改变数值
					Residue = 0;
					return Element; //返回这个物品
				}
			}
		} //遍历物品合集
	}
	//没用堆叠，正常创建（堆叠剩余后也调用这个）
	FIntPoint LocationFirst; //准备位置
	int PageFirst = 0; //准备页数
	if (CanPutItem(LocationFirst, PageFirst)) //能放下第一个吗
	{
		UInventoryBaseItem* FirstNew = NewObject<UInventoryBaseItem>(this, InClass); //先创建第一个拿信息
		FirstNew->OwnerComponent = this; //设置拥有者
		FirstNew->ItemData.Location = LocationFirst; //先给位置
		FirstNew->ItemData.Pages = PageFirst; //给页数

		int ClassCountMax = FirstNew->ItemData.MaxCount; //获取最大堆叠

		if (ClassCountMax <= 0) //是无限堆叠
		{
			FirstNew->ItemData.Count = CountMinus; //无限堆叠就直接把数值设置
			ItemDatas.Add(FirstNew); //最后添加到合集，之后再去触发委托
			OnInventoryDataRefresh.Broadcast(); //触发委托
			Residue = 0; //没有剩余
			return FirstNew; //返回第一个创建的物品
		}
		else //堆叠有上限的
		{
			if (CountMinus <= ClassCountMax) //正好可以放下
			{
				FirstNew->ItemData.Count = CountMinus; //设置数值
				ItemDatas.Add(FirstNew); //最后添加到合集，之后再去触发委托
				OnInventoryDataRefresh.Broadcast(); //触发委托
				Residue = 0; //没有剩余
				return FirstNew; //返回第一个创建的物品
			}
			else //不能放下，需要创建新组
			{
				//先创建除数和余数，余数只用来判断有没有剩余
				float Remainder = CountMinus % ClassCountMax;
				float GroupCount = CountMinus / ClassCountMax;
				//根据组数创建各个组
				for (int i = 0; i < GroupCount; i++)
				{
					//这一轮的物品有位置创建吗
					FIntPoint LocationLoop;
					int PageLoop = 0;
					if (CanPutItem(LocationLoop, PageLoop))
					{
						UInventoryBaseItem* LoopItem = NewObject<UInventoryBaseItem>(this, InClass); //创建一组
						LoopItem->OwnerComponent = this;
						LoopItem->ItemData.Location = LocationLoop; //先给位置
						LoopItem->ItemData.Pages = PageLoop; //给页数
						LoopItem->ItemData.Count = ClassCountMax; //设置为上限
						ItemDatas.Add(LoopItem); //添加到合集，触发委托和返回留给剩余
					}
					else
					{
						OnInventoryDataRefresh.Broadcast(); //触发委托
						Residue = CountMinus - ClassCountMax * i; //返回剩余的值
						return nullptr; //没创建成功
					} //没位置了
				}
				//需要的组都创建完了
				if (Remainder == 0) //没有剩余的了
				{
					OnInventoryDataRefresh.Broadcast(); //触发委托
					Residue = 0; //没有剩余
					return ItemDatas.Last(); //返回最后一个物品
				}
				else
				{
					//最后的剩余物品有位置创建吗
					FIntPoint LocationEnd;
					int PageEnd = 0;
					if (CanPutItem(LocationEnd, PageEnd))
					{
						UInventoryBaseItem* EndItem = NewObject<UInventoryBaseItem>(this, InClass); //创建一组
						EndItem->OwnerComponent = this;
						EndItem->ItemData.Location = LocationEnd; //先给位置
						EndItem->ItemData.Pages = PageEnd; //给页数
						EndItem->ItemData.Count = CountMinus - ClassCountMax * GroupCount; //先减去一组的堆叠数量;//把剩余数设置上
						ItemDatas.Add(EndItem); //添加到合集
						OnInventoryDataRefresh.Broadcast(); //触发委托
						Residue = 0; //没有剩余了
						return EndItem; //不返回了
					}
					else
					{
						OnInventoryDataRefresh.Broadcast(); //触发委托
						Residue = CountMinus - ClassCountMax * GroupCount; //剩余数给了
						return nullptr; //不返回了
					} //没位置创建了
				} //还有剩余的，再创建
			}
		}
	}
	else //放不下就返回空物品，然后把剩余数全部返回
	{
		Residue = Count;
		return nullptr;
	}
}

bool UCP_Inventory::GiveItemByClassBatch(TArray<TSubclassOf<UInventoryBaseItem>> InClass, TArray<int> AllCount,
                                         int Count)
{
	//批量给予物品
	const bool bUseSameCount = Count > 0;

	//确保数量一致
	if (!bUseSameCount && InClass.Num() != AllCount.Num())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("给予物品数量错误"));
		return false;
	}

	//创建物品合集
	TMap<TSubclassOf<UInventoryBaseItem>, int> ItemCount;

	//遍历物品合集
	for (int32 Index = 0; Index < InClass.Num(); ++Index)
	{
		TSubclassOf<UInventoryBaseItem> ItemClass = InClass[Index];

		if (ItemClass == nullptr)
		{
			continue;
		}

		const int CountNow = bUseSameCount ? Count : AllCount[Index];

		if (CountNow <= 0)
		{
			continue;
		}

		ItemCount.FindOrAdd(ItemClass) += CountNow;
	}

	if (ItemCount.IsEmpty())
	{
		return false;
	}

	for (const auto& Element : ItemCount)
	{
		int Residue = 0;
		GiveItemByClass(Residue, Element.Key, Element.Value, true);
	}

	return true;
}


TArray<UInventoryBaseItem*> UCP_Inventory::GetItemDatas()
{
	return ItemDatas;
}

FInventorySaveData UCP_Inventory::BuildInventorySaveData()
{
	FInventorySaveData EndConstruct = {};
	FItemSaveSingleData Buffer;
	for (const auto& Element : ItemDatas)
	{
		Buffer.Class = Element->GetClass();
		Buffer.Name = Element->ItemData.Name;
		Buffer.Description = Element->ItemData.Description;
		Buffer.Count = Element->ItemData.Count;
		Buffer.MaxCount = Element->ItemData.MaxCount;
		Buffer.KeyWords = Element->ItemData.KeyWords;
		Buffer.Location = Element->ItemData.Location;
		Buffer.Quality = Element->ItemData.Quality;
		Buffer.Weight = Element->ItemData.Weight;
		Buffer.Price = Element->ItemData.Price;
		Buffer.Durability = Element->ItemData.Durability;
		Buffer.Level = Element->ItemData.Level;
		Buffer.Priority = Element->ItemData.Priority;
		Buffer.Pages = Element->ItemData.Pages;
		Buffer.ExtraAttributes = Element->ItemData.ExtraAttributes;
		Buffer.ExtraDescription = Element->ItemData.ExtraDescription;
		Buffer.CharShapeNow = Element->ItemData.CharData.CharShapeNow;
		Buffer.CharShapeHas = Element->ItemData.CharData.CharShapeHas;
		Buffer.SkillNow = Element->ItemData.CharData.SkillNow;
		Buffer.SkillHave = Element->ItemData.CharData.SkillHave;
		Buffer.SkillLevel = Element->ItemData.CharData.SkillLevel;
		Buffer.Speed = Element->ItemData.CharData.Speed;
		Buffer.Health = Element->ItemData.CharData.Health;
		Buffer.Attack = Element->ItemData.CharData.Attack;
		Buffer.PhyDefense = Element->ItemData.CharData.PhyDefense;
		Buffer.MagDefense = Element->ItemData.CharData.MagDefense;
		Buffer.Critical = Element->ItemData.CharData.Critical;
		Buffer.CriticalC = Element->ItemData.CharData.CriticalC;
		Buffer.Synchronize = Element->ItemData.CharData.Synchronize;

		EndConstruct.ItemSSaveData.Add(Buffer);
	}

	return EndConstruct;
}

void UCP_Inventory::LoadInventorySaveData(FInventorySaveData InSaveData)
{
	ItemDatas.Empty();
	for (const auto& Element : InSaveData.ItemSSaveData)
	{
		UInventoryBaseItem* Buffer = NewObject<UInventoryBaseItem>(this, Element.Class);
		Buffer->ItemData.Name = Element.Name;
		Buffer->ItemData.Description = Element.Description;
		Buffer->ItemData.Count = Element.Count;
		Buffer->ItemData.MaxCount = Element.MaxCount;
		Buffer->ItemData.KeyWords = Element.KeyWords;
		Buffer->ItemData.Location = Element.Location;
		Buffer->ItemData.Quality = Element.Quality;
		Buffer->ItemData.Weight = Element.Weight;
		Buffer->ItemData.Price = Element.Price;
		Buffer->ItemData.Durability = Element.Durability;
		Buffer->ItemData.Level = Element.Level;
		Buffer->ItemData.Priority = Element.Priority;
		Buffer->ItemData.Pages = Element.Pages;
		Buffer->ItemData.ExtraAttributes = Element.ExtraAttributes;
		Buffer->ItemData.ExtraDescription = Element.ExtraDescription;
		Buffer->ItemData.CharData.CharShapeNow = Element.CharShapeNow;
		Buffer->ItemData.CharData.CharShapeHas = Element.CharShapeHas;
		Buffer->ItemData.CharData.SkillNow = Element.SkillNow;
		Buffer->ItemData.CharData.SkillHave = Element.SkillHave;
		Buffer->ItemData.CharData.SkillLevel = Element.SkillLevel;
		Buffer->ItemData.CharData.Speed = Element.Speed;
		Buffer->ItemData.CharData.Health = Element.Health;
		Buffer->ItemData.CharData.Attack = Element.Attack;
		Buffer->ItemData.CharData.PhyDefense = Element.PhyDefense;
		Buffer->ItemData.CharData.MagDefense = Element.MagDefense;
		Buffer->ItemData.CharData.Critical = Element.Critical;
		Buffer->ItemData.CharData.CriticalC = Element.CriticalC;
		Buffer->ItemData.CharData.Synchronize = Element.Synchronize;

		ItemDatas.Add(Buffer);
		OnInventoryDataRefresh.Broadcast();
	}
}

void UCP_Inventory::ClearInventoryData()
{
	ItemDatas.Empty();
}

void UCP_Inventory::RemoveItemByClass(TSubclassOf<UInventoryBaseItem> InClass)
{
	for (int i = 0; i < ItemDatas.Num(); i++)
	{
		if (ItemDatas[i]->GetClass() == InClass)
		{
			ItemDatas[i]->DestroyItem();
			ItemDatas.RemoveAt(i);
			i--;
		}
	}

	if (InClass != nullptr)
	{
		OnInventoryDataRefresh.Broadcast();
	}
}

void UCP_Inventory::RemoveItemByItem(UInventoryBaseItem* InItem)
{
	InItem->DestroyItem();
	ItemDatas.Remove(InItem);
	OnInventoryDataRefresh.Broadcast();
}

bool UCP_Inventory::UseItem(int& ResidueNeed, UInventoryBaseItem* InItem, int Count, bool UseOther)
{
	//物品有效吗
	if (InItem == nullptr)
	{
		ResidueNeed = 0;
		return false;
	}
	//如果要消耗的数量大于这组
	else if (InItem->ItemData.Count < Count)
	{
		if (UseOther == false)
		{
			ResidueNeed = Count - InItem->ItemData.Count;
			return false;
		}
		int ItemStartLoc = ItemDatas.Find(InItem);
		ItemDatas.Remove(InItem); //先把自己从合集里移除
		int CountBuffer = Count - InItem->ItemData.Count; //先减去要使用的物品，剩余数量

		//获取相同类型的物品
		TArray<UInventoryBaseItem*> SameClass = UInventoryFUClibrary::FilterItemSBYClass(
			ItemDatas, {InItem->GetClass()});
		if (SameClass.IsEmpty())
		{
			ItemDatas.Insert(InItem, ItemStartLoc); //加回原位
			ResidueNeed = CountBuffer;
			return false;
		} //如果没有相同类型的物品
		for (const auto& Element : SameClass)
		{
			if (CountBuffer > Element->ItemData.Count) //如果这组消耗了
			{
				CountBuffer = CountBuffer - Element->ItemData.Count; //记录消耗的数量
				Element->ItemData.KeyWords.Add("DestroyAndDelete"); //标记为删除
			}
			else if (Element->ItemData.Count == CountBuffer)
			{
				Element->ItemData.KeyWords.Add("DestroyAndDelete"); //标记为删除
				CountBuffer = 0;
			} //如果正好这组消耗完
			else if (CountBuffer < Element->ItemData.Count) //如果这组消耗后还有剩余
			{
				Element->ItemData.Count -= CountBuffer; //设置剩余数量（已消耗）
				CountBuffer = 0; //没有剩余了
				UInventoryBaseItem* EndingItem = Element; //记录最后一组
				Element->ChangeItemData(); //数值改变了
			}
		} //遍历相同类型的物品，逐步减去剩余数量
		if (CountBuffer > 0) //如果没有足够的使用
		{
			ItemDatas.Insert(InItem, ItemStartLoc); //加回原位
			ResidueNeed = CountBuffer; //返回剩余需要的量
			return false;
		}
		else
		{
			//获取要摧毁的物品
			TArray<UInventoryBaseItem*> DestroyMaps = UInventoryFUClibrary::FilterItemSBYKeyWord(
				ItemDatas, {"DestroyAndDelete"});
			DestroyMaps.Add(InItem);
			for (const auto& Element2 : DestroyMaps)
			{
				Element2->ROC_OnItemUse();
				Element2->DestroyItem();
			}
			ItemDatas.RemoveAll([&DestroyMaps](UInventoryBaseItem* Element)
			{
				return DestroyMaps.Contains(Element);
			});
			OnInventoryDataRefresh.Broadcast(); //通知组件物品数量已改变
			return true;
		} //如果能使用
	}
	//如果消耗的数量小于这组
	else if (InItem->ItemData.Count > Count)
	{
		InItem->ItemData.Count -= Count;
		InItem->ChangeItemData();
		return true;
	}
	//如果消耗的数量等于这组
	else
	{
		ItemDatas.Remove(InItem);
		InItem->ROC_OnItemUse();
		InItem->DestroyItem();
		OnInventoryDataRefresh.Broadcast();
		return true;
	}
}

bool UCP_Inventory::UseItemByClass(int& ResidueNeed, TSubclassOf<UInventoryBaseItem> InClass, int Count)
{
	//获取相同类型的物品
	TArray<UInventoryBaseItem*> SameClass = UInventoryFUClibrary::FilterItemSBYClass(ItemDatas, InClass);
	if (SameClass.IsEmpty())
	{
		ResidueNeed = Count;
		return false;
	} //如果没有相同类型的物品
	int CountBuffer = Count; //记录剩余数量
	for (const auto& Element : SameClass)
	{
		if (CountBuffer > Element->ItemData.Count) //如果这组消耗了
		{
			CountBuffer = CountBuffer - Element->ItemData.Count; //记录消耗的数量
			Element->ItemData.KeyWords.Add("DestroyAndDelete"); //标记为删除
		}
		else if (Element->ItemData.Count == CountBuffer)
		{
			Element->ItemData.KeyWords.Add("DestroyAndDelete"); //标记为删除
			CountBuffer = 0;
		} //如果正好这组消耗完
		else if (CountBuffer < Element->ItemData.Count) //如果这组消耗后还有剩余
		{
			Element->ItemData.Count -= CountBuffer; //设置剩余数量（已消耗）
			CountBuffer = 0; //没有剩余了
			UInventoryBaseItem* EndingItem = Element; //记录最后一组
			Element->ROC_OnItemUse(); //使用物品
			Element->ChangeItemData(); //数值改变了
		}
	} //遍历相同类型的物品，逐步减去剩余数量
	if (CountBuffer > 0) //如果没有足够的使用
	{
		ResidueNeed = CountBuffer; //返回剩余需要的量
		return false;
	}
	else
	{
		//获取要摧毁的物品
		TArray<UInventoryBaseItem*> DestroyMaps = UInventoryFUClibrary::FilterItemSBYKeyWord(
			ItemDatas, {"DestroyAndDelete"});
		for (const auto& Element2 : DestroyMaps)
		{
			Element2->ROC_OnItemUse();
			Element2->DestroyItem();
		}
		ItemDatas.RemoveAll([&DestroyMaps](UInventoryBaseItem* Element)
		{
			return DestroyMaps.Contains(Element);
		});
		OnInventoryDataRefresh.Broadcast(); //通知组件物品数量已改变
		return true;
	} //如果能使用
}


bool UCP_Inventory::CanPutItem(FIntPoint& LocationOut, int& PageOut)
{
	if (IsEnablePage && InventorySize.X > 0 && InventorySize.Y > 0)
	{
		int PageSize = InventorySize.Y * InventorySize.X;
		int Page = ItemDatas.Num() / PageSize;

		PageOut = Page;
	}
	if (InventorySize.X > 0) //X尺寸有效
	{
		//先获取除和余数
		float Remainder = ItemDatas.Num() % InventorySize.X;
		float Divide = ItemDatas.Num() / InventorySize.X;

		if (InventorySize.Y > 0 && IsEnablePage == false) //Y尺寸有效
		{
			if (Divide < InventorySize.Y)
			{
				LocationOut = FIntPoint(Remainder, Divide);
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			LocationOut = FIntPoint(Remainder, Divide);
			return true;
		}
	}
	else
	{
		LocationOut = FIntPoint(ItemDatas.Num(), 0);
		return true;
	}
}

void UCP_Inventory::ResetItemXY()
{
	RemoveItemByClass(nullptr); //先清除空物品
	TArray<UInventoryBaseItem*> SortItemSBuffer = ItemDatas; //把合集复制到缓冲区
	ItemDatas.Empty(); //清空合集
	for (const auto& Element : SortItemSBuffer) //全部重新排序
	{
		FIntPoint NewLoc; //新的位置
		int Nulv = 0;
		if (CanPutItem(NewLoc, Nulv))
		{
			Element->ItemData.Location = NewLoc; //设置新位置
			ItemDatas.Add(Element); //再加回去
		}
	}
	OnInventoryDataSort.Broadcast(ItemDatas);
}

void UCP_Inventory::SortItemXYByInput(TArray<UInventoryBaseItem*> MapSin, bool Reverse)
{
	RemoveItemByClass(nullptr); //先清除空物品
	TArray<UInventoryBaseItem*> SortItemSBuffer = ItemDatas; //把合集复制到缓冲区
	ItemDatas.Empty(); //清空合集
	if (Reverse)
	{
		std::reverse(MapSin.begin(), MapSin.end());
	} //翻转吗
	for (const auto& Element : MapSin) //开始排序输入的合集
	{
		FIntPoint Sortoc; //排序成功的位置
		int Nulv = 0;
		if (CanPutItem(Sortoc, Nulv))
		{
			Element->ItemData.Location = Sortoc; //设置新位置
			ItemDatas.Add(Element); //先加一个占位
		}
	}
	ItemDatas = SortItemSBuffer; //排序完后把合集还回去
	OnInventoryDataSort.Broadcast(MapSin);
}

void UCP_Inventory::SortItemXYByCount(TArray<UInventoryBaseItem*> MapSin, bool Reverse)
{
	TArray<UInventoryBaseItem*> ItemsBuffer = ItemDatas; //暂存
	ItemDatas.Empty(); //清空
	TArray<UInventoryBaseItem*> MapBuffer; //为了输入准备
	int Judge = -1; //判断条件制作

	for (const auto& MapSinV : MapSin)
	{
		if (MapBuffer.IsEmpty()) //如果是空合集，先设置一个
		{
			MapBuffer.Add(MapSinV);
		}
		else //开始排序
		{
			Judge = -1; //重置判断条件
			for (const auto& BufferV : MapBuffer) //开始和数组里的排序
			{
				if (MapSinV->ItemData.Count <= BufferV->ItemData.Count) //如果小于了
				{
					Judge = MapBuffer.Find(BufferV);
					break;
				}
			}
			if (Judge >= 0) //如果找到的对应的位置
			{
				MapBuffer.Insert(MapSinV, Judge);
			}
			else //比数值里的都大，是最后一个
			{
				MapBuffer.Add(MapSinV);
			}
		}
	}
	if (Reverse)
	{
		std::reverse(MapBuffer.begin(), MapBuffer.end());
	} //翻转吗
	for (const auto& Element3 : MapBuffer)
	{
		int Nulv = 0;
		if (CanPutItem(Element3->ItemData.Location, Nulv)) //开始排序
		{
			ItemDatas.Add(Element3); //+1
		}
	}

	ItemDatas = ItemsBuffer; //还回去
	OnInventoryDataSort.Broadcast(MapSin);
}

void UCP_Inventory::SortItemXYByQuality(TArray<UInventoryBaseItem*> MapSin, bool Reverse)
{
	TArray<UInventoryBaseItem*> ItemsBuffer = ItemDatas; //暂存
	ItemDatas.Empty(); //清空
	TArray<UInventoryBaseItem*> MapBuffer; //为了输入准备
	int Judge = -1; //判断条件制作

	for (const auto& MapSinV : MapSin)
	{
		if (MapBuffer.IsEmpty()) //如果是空合集，先设置一个
		{
			MapBuffer.Add(MapSinV);
		}
		else //开始排序
		{
			Judge = -1; //重置判断条件
			for (const auto& BufferV : MapBuffer) //开始和数组里的排序
			{
				if (MapSinV->ItemData.Quality <= BufferV->ItemData.Quality) //如果小于了
				{
					Judge = MapBuffer.Find(BufferV);
					break;
				}
			}
			if (Judge >= 0) //如果找到的对应的位置
			{
				MapBuffer.Insert(MapSinV, Judge);
			}
			else //比数值里的都大，是最后一个
			{
				MapBuffer.Add(MapSinV);
			}
		}
	}
	if (Reverse)
	{
		std::reverse(MapBuffer.begin(), MapBuffer.end());
	} //翻转吗
	for (const auto& Element3 : MapBuffer)
	{
		int Nulv = 0;
		if (CanPutItem(Element3->ItemData.Location, Nulv)) //开始排序
		{
			ItemDatas.Add(Element3); //+1
		}
	}

	ItemDatas = ItemsBuffer; //还回去
	OnInventoryDataSort.Broadcast(MapSin);
}

void UCP_Inventory::SortItemXYByWeight(TArray<UInventoryBaseItem*> MapSin, bool Reverse)

{
	TArray<UInventoryBaseItem*> ItemsBuffer = ItemDatas; //暂存
	ItemDatas.Empty(); //清空
	TArray<UInventoryBaseItem*> MapBuffer; //为了输入准备
	int Judge = -1; //判断条件制作

	for (const auto& MapSinV : MapSin)
	{
		if (MapBuffer.IsEmpty()) //如果是空合集，先设置一个
		{
			MapBuffer.Add(MapSinV);
		}
		else //开始排序
		{
			Judge = -1; //重置判断条件
			for (const auto& BufferV : MapBuffer) //开始和数组里的排序
			{
				if (MapSinV->ItemData.Weight <= BufferV->ItemData.Weight) //如果小于了
				{
					Judge = MapBuffer.Find(BufferV);
					break;
				}
			}
			if (Judge >= 0) //如果找到的对应的位置
			{
				MapBuffer.Insert(MapSinV, Judge);
			}
			else //比数值里的都大，是最后一个
			{
				MapBuffer.Add(MapSinV);
			}
		}
	}
	if (Reverse)
	{
		std::reverse(MapBuffer.begin(), MapBuffer.end());
	} //翻转吗
	for (const auto& Element3 : MapBuffer)
	{
		int Nulv = 0;
		if (CanPutItem(Element3->ItemData.Location, Nulv)) //开始排序
		{
			ItemDatas.Add(Element3); //+1
		}
	}

	ItemDatas = ItemsBuffer; //还回去
	OnInventoryDataSort.Broadcast(MapSin);
}

void UCP_Inventory::SortItemXYByPrice(TArray<UInventoryBaseItem*> MapSin, bool Reverse)

{
	TArray<UInventoryBaseItem*> ItemsBuffer = ItemDatas; //暂存
	ItemDatas.Empty(); //清空
	TArray<UInventoryBaseItem*> MapBuffer; //为了输入准备
	int Judge = -1; //判断条件制作

	for (const auto& MapSinV : MapSin)
	{
		if (MapBuffer.IsEmpty()) //如果是空合集，先设置一个
		{
			MapBuffer.Add(MapSinV);
		}
		else //开始排序
		{
			Judge = -1; //重置判断条件
			for (const auto& BufferV : MapBuffer) //开始和数组里的排序
			{
				if (MapSinV->ItemData.Price <= BufferV->ItemData.Price) //如果小于了
				{
					Judge = MapBuffer.Find(BufferV);
					break;
				}
			}
			if (Judge >= 0) //如果找到的对应的位置
			{
				MapBuffer.Insert(MapSinV, Judge);
			}
			else //比数值里的都大，是最后一个
			{
				MapBuffer.Add(MapSinV);
			}
		}
	}
	if (Reverse)
	{
		std::reverse(MapBuffer.begin(), MapBuffer.end());
	} //翻转吗
	for (const auto& Element3 : MapBuffer)
	{
		int Nulv = 0;
		if (CanPutItem(Element3->ItemData.Location, Nulv)) //开始排序
		{
			ItemDatas.Add(Element3); //+1
		}
	}

	ItemDatas = ItemsBuffer; //还回去
	OnInventoryDataSort.Broadcast(MapSin);
}

void UCP_Inventory::SortItemXYByDurability(TArray<UInventoryBaseItem*> MapSin, bool Reverse)
{
	TArray<UInventoryBaseItem*> ItemsBuffer = ItemDatas; //暂存
	ItemDatas.Empty(); //清空
	TArray<UInventoryBaseItem*> MapBuffer; //为了输入准备
	int Judge = -1; //判断条件制作

	for (const auto& MapSinV : MapSin)
	{
		if (MapBuffer.IsEmpty()) //如果是空合集，先设置一个
		{
			MapBuffer.Add(MapSinV);
		}
		else //开始排序
		{
			Judge = -1; //重置判断条件
			for (const auto& BufferV : MapBuffer) //开始和数组里的排序
			{
				if (MapSinV->ItemData.Durability <= BufferV->ItemData.Durability) //如果小于了
				{
					Judge = MapBuffer.Find(BufferV);
					break;
				}
			}
			if (Judge >= 0) //如果找到的对应的位置
			{
				MapBuffer.Insert(MapSinV, Judge);
			}
			else //比数值里的都大，是最后一个
			{
				MapBuffer.Add(MapSinV);
			}
		}
	}
	if (Reverse)
	{
		std::reverse(MapBuffer.begin(), MapBuffer.end());
	} //翻转吗
	for (const auto& Element3 : MapBuffer)
	{
		int Nulv = 0;
		if (CanPutItem(Element3->ItemData.Location, Nulv)) //开始排序
		{
			ItemDatas.Add(Element3); //+1
		}
	}

	ItemDatas = ItemsBuffer; //还回去
	OnInventoryDataSort.Broadcast(MapSin);
}

void UCP_Inventory::SortItemXYByLevel(TArray<UInventoryBaseItem*> MapSin, bool Reverse)

{
	TArray<UInventoryBaseItem*> ItemsBuffer = ItemDatas; //暂存
	ItemDatas.Empty(); //清空
	TArray<UInventoryBaseItem*> MapBuffer; //为了输入准备
	int Judge = -1; //判断条件制作

	for (const auto& MapSinV : MapSin)
	{
		if (MapBuffer.IsEmpty()) //如果是空合集，先设置一个
		{
			MapBuffer.Add(MapSinV);
		}
		else //开始排序
		{
			Judge = -1; //重置判断条件
			for (const auto& BufferV : MapBuffer) //开始和数组里的排序
			{
				if (MapSinV->ItemData.Level <= BufferV->ItemData.Level) //如果小于了
				{
					Judge = MapBuffer.Find(BufferV);
					break;
				}
			}
			if (Judge >= 0) //如果找到的对应的位置
			{
				MapBuffer.Insert(MapSinV, Judge);
			}
			else //比数值里的都大，是最后一个
			{
				MapBuffer.Add(MapSinV);
			}
		}
	}
	if (Reverse)
	{
		std::reverse(MapBuffer.begin(), MapBuffer.end());
	} //翻转吗
	for (const auto& Element3 : MapBuffer)
	{
		int Nulv = 0;
		if (CanPutItem(Element3->ItemData.Location, Nulv)) //开始排序
		{
			ItemDatas.Add(Element3); //+1
		}
	}

	ItemDatas = ItemsBuffer; //还回去
	OnInventoryDataSort.Broadcast(MapSin);
}

void UCP_Inventory::SortItemXYByPriority(TArray<UInventoryBaseItem*> MapSin, bool Reverse)

{
	TArray<UInventoryBaseItem*> ItemsBuffer = ItemDatas; //暂存
	ItemDatas.Empty(); //清空
	TArray<UInventoryBaseItem*> MapBuffer; //为了输入准备
	int Judge = -1; //判断条件制作

	for (const auto& MapSinV : MapSin)
	{
		if (MapBuffer.IsEmpty()) //如果是空合集，先设置一个
		{
			MapBuffer.Add(MapSinV);
		}
		else //开始排序
		{
			Judge = -1; //重置判断条件
			for (const auto& BufferV : MapBuffer) //开始和数组里的排序
			{
				if (MapSinV->ItemData.Priority <= BufferV->ItemData.Priority) //如果小于了
				{
					Judge = MapBuffer.Find(BufferV);
					break;
				}
			}
			if (Judge >= 0) //如果找到的对应的位置
			{
				MapBuffer.Insert(MapSinV, Judge);
			}
			else //比数值里的都大，是最后一个
			{
				MapBuffer.Add(MapSinV);
			}
		}
	}
	if (Reverse)
	{
		std::reverse(MapBuffer.begin(), MapBuffer.end());
	} //翻转吗
	for (const auto& Element3 : MapBuffer)
	{
		int Nulv = 0;
		if (CanPutItem(Element3->ItemData.Location, Nulv)) //开始排序
		{
			ItemDatas.Add(Element3); //+1
		}
	}

	ItemDatas = ItemsBuffer; //还回去
	OnInventoryDataSort.Broadcast(MapSin);
}


void UCP_Inventory::SetItemValue(UInventoryBaseItem* InItem, EItemSaveSingleEnum Param, const int32& Variable)
{
}

void UCP_Inventory::MergeItemSBYClass(TSubclassOf<UInventoryBaseItem> Class)
{
	int ThisClassCount = 0;
	int NulV = 0;
	for (auto Element : UInventoryFUClibrary::FilterItemSBYClass(ItemDatas, Class))
	{
		ThisClassCount += Element->ItemData.Count;
	}
	RemoveItemByClass(Class);
	GiveItemByClass(NulV, Class, ThisClassCount, true);
	ResetItemXY();
}
