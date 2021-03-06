// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/AutomationTest.h"
#include "Templates/SubclassOf.h"
#include "Backends/JsonStructDeserializerBackend.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"
#include "Tests/StructSerializerTestTypes.h"

#include "UObject/MetaData.h"

#if WITH_DEV_AUTOMATION_TESTS

/* Internal helpers
 *****************************************************************************/

namespace StructSerializerTest
{
	void TestSerialization( FAutomationTestBase& Test, IStructSerializerBackend& SerializerBackend, IStructDeserializerBackend& DeserializerBackend )
	{
		// serialization
		FStructSerializerTestStruct TestStruct;

		UClass* MetaDataClass = LoadClass<UMetaData>(nullptr, TEXT("/Script/CoreUObject.MetaData"));
		UMetaData* MetaDataObject = NewObject<UMetaData>();
		// setup object tests
		TestStruct.Objects.Class = MetaDataClass;
		TestStruct.Objects.SubClass = MetaDataClass;
		TestStruct.Objects.SoftClass = MetaDataClass;
		TestStruct.Objects.Object = MetaDataObject;
		TestStruct.Objects.WeakObject = MetaDataObject;
		TestStruct.Objects.SoftObject = MetaDataObject;
		TestStruct.Objects.ClassPath = MetaDataClass;
		TestStruct.Objects.ObjectPath = MetaDataObject;

		{
			FStructSerializer::Serialize(TestStruct, SerializerBackend);
		}

		// deserialization
		FStructSerializerTestStruct TestStruct2(NoInit);
		{
			FStructDeserializerPolicies Policies;
			Policies.MissingFields = EStructDeserializerErrorPolicies::Warning;
			
			Test.TestTrue(TEXT("Deserialization must succeed"), FStructDeserializer::Deserialize(TestStruct2, DeserializerBackend, Policies));
		}

		// test numerics
		Test.TestEqual<int8>(TEXT("Numerics.Int8 value must be the same before and after de-/serialization"), TestStruct.Numerics.Int8, TestStruct2.Numerics.Int8);
		Test.TestEqual<int16>(TEXT("Numerics.Int16 value must be the same before and after de-/serialization"), TestStruct.Numerics.Int16, TestStruct2.Numerics.Int16);
		Test.TestEqual<int32>(TEXT("Numerics.Int32 value must be the same before and after de-/serialization"), TestStruct.Numerics.Int32, TestStruct2.Numerics.Int32);
		Test.TestEqual<int64>(TEXT("Numerics.Int64 value must be the same before and after de-/serialization"), TestStruct.Numerics.Int64, TestStruct2.Numerics.Int64);
		Test.TestEqual<uint8>(TEXT("Numerics.UInt8 value must be the same before and after de-/serialization"), TestStruct.Numerics.UInt8, TestStruct2.Numerics.UInt8);
		Test.TestEqual<uint16>(TEXT("Numerics.UInt16 value must be the same before and after de-/serialization"), TestStruct.Numerics.UInt16, TestStruct2.Numerics.UInt16);
		Test.TestEqual<uint32>(TEXT("Numerics.UInt32 value must be the same before and after de-/serialization"), TestStruct.Numerics.UInt32, TestStruct2.Numerics.UInt32);
		Test.TestEqual<uint64>(TEXT("Numerics.UInt64 value must be the same before and after de-/serialization"), TestStruct.Numerics.UInt64, TestStruct2.Numerics.UInt64);
		Test.TestEqual<float>(TEXT("Numerics.Float value must be the same before and after de-/serialization"), TestStruct.Numerics.Float, TestStruct2.Numerics.Float);
		Test.TestEqual<double>(TEXT("Numerics.Double value must be the same before and after de-/serialization"), TestStruct.Numerics.Double, TestStruct2.Numerics.Double);

		// test booleans
		Test.TestEqual<bool>(TEXT("Booleans.BoolFalse must be the same before and after de-/serialization"), TestStruct.Booleans.BoolFalse, TestStruct2.Booleans.BoolFalse);
		Test.TestEqual<bool>(TEXT("Booleans.BoolTrue must be the same before and after de-/serialization"), TestStruct.Booleans.BoolTrue, TestStruct2.Booleans.BoolTrue);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield0 must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield0, TestStruct2.Booleans.Bitfield0);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield1 must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield1, TestStruct2.Booleans.Bitfield1);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield2Set must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield2Set, TestStruct2.Booleans.Bitfield2Set);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield3 must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield3, TestStruct2.Booleans.Bitfield3);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield4Set must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield4Set, TestStruct2.Booleans.Bitfield4Set);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield5Set must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield5Set, TestStruct2.Booleans.Bitfield5Set);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield6 must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield6, TestStruct2.Booleans.Bitfield6);
		Test.TestEqual<bool>(TEXT("Booleans.Bitfield7 must be the same before and after de-/serialization"), TestStruct.Booleans.Bitfield7Set, TestStruct2.Booleans.Bitfield7Set);

		// test objects
		Test.TestEqual<class UClass*>(TEXT("Objects.Class must be the same before and after de-/serialization"), TestStruct.Objects.Class, TestStruct2.Objects.Class);
		Test.TestEqual<TSubclassOf<class UMetaData>>(TEXT("Objects.SubClass must be the same before and after de-/serialization"), TestStruct.Objects.SubClass, TestStruct2.Objects.SubClass);
		Test.TestEqual<TSoftClassPtr<class UMetaData>>(TEXT("Objects.SoftClass must be the same before and after de-/serialization"), TestStruct.Objects.SoftClass, TestStruct2.Objects.SoftClass);
		Test.TestEqual<UObject*>(TEXT("Objects.Object must be the same before and after de-/serialization"), TestStruct.Objects.Object, TestStruct2.Objects.Object);
		Test.TestEqual<TWeakObjectPtr<class UMetaData>>(TEXT("Objects.WeakObject must be the same before and after de-/serialization"), TestStruct.Objects.WeakObject, TestStruct2.Objects.WeakObject);
		Test.TestEqual<TSoftObjectPtr<class UMetaData>>(TEXT("Objects.SoftObject must be the same before and after de-/serialization"), TestStruct.Objects.SoftObject, TestStruct2.Objects.SoftObject);
		Test.TestEqual<FSoftClassPath>(TEXT("Objects.ClassPath must be the same before and after de-/serialization"), TestStruct.Objects.ClassPath, TestStruct2.Objects.ClassPath);
		Test.TestEqual<FSoftObjectPath>(TEXT("Objects.ObjectPath must be the same before and after de-/serialization"), TestStruct.Objects.ObjectPath, TestStruct2.Objects.ObjectPath);

		// test built-ins
		Test.TestEqual<FGuid>(TEXT("Builtins.Guid must be the same before and after de-/serialization"), TestStruct.Builtins.Guid, TestStruct2.Builtins.Guid);
		Test.TestEqual<FName>(TEXT("Builtins.Name must be the same before and after de-/serialization"), TestStruct.Builtins.Name, TestStruct2.Builtins.Name);
		Test.TestEqual<FString>(TEXT("Builtins.String must be the same before and after de-/serialization"), TestStruct.Builtins.String, TestStruct2.Builtins.String);
		Test.TestEqual<FString>(TEXT("Builtins.Text must be the same before and after de-/serialization"), TestStruct.Builtins.Text.ToString(), TestStruct2.Builtins.Text.ToString());
		Test.TestEqual<FVector>(TEXT("Builtins.Vector must be the same before and after de-/serialization"), TestStruct.Builtins.Vector, TestStruct2.Builtins.Vector);
		Test.TestEqual<FVector4>(TEXT("Builtins.Vector4 must be the same before and after de-/serialization"), TestStruct.Builtins.Vector4, TestStruct2.Builtins.Vector4);
		Test.TestEqual<FRotator>(TEXT("Builtins.Rotator must be the same before and after de-/serialization"), TestStruct.Builtins.Rotator, TestStruct2.Builtins.Rotator);
		Test.TestEqual<FQuat>(TEXT("Builtins.Quat must be the same before and after de-/serialization"), TestStruct.Builtins.Quat, TestStruct2.Builtins.Quat);
		Test.TestEqual<FColor>(TEXT("Builtins.Color must be the same before and after de-/serialization"), TestStruct.Builtins.Color, TestStruct2.Builtins.Color);


		// test arrays
		Test.TestEqual<TArray<int32>>(TEXT("Arrays.Int32Array must be the same before and after de-/serialization"), TestStruct.Arrays.Int32Array, TestStruct2.Arrays.Int32Array);
		Test.TestEqual<int32>(TEXT("Arrays.StaticSingleElement[0] must be the same before and after de-/serialization"), TestStruct.Arrays.StaticSingleElement[0], TestStruct2.Arrays.StaticSingleElement[0]);
		Test.TestEqual<int32>(TEXT("Arrays.StaticInt32Array[0] must be the same before and after de-/serialization"), TestStruct.Arrays.StaticInt32Array[0], TestStruct2.Arrays.StaticInt32Array[0]);
		Test.TestEqual<int32>(TEXT("Arrays.StaticInt32Array[1] must be the same before and after de-/serialization"), TestStruct.Arrays.StaticInt32Array[1], TestStruct2.Arrays.StaticInt32Array[1]);
		Test.TestEqual<int32>(TEXT("Arrays.StaticInt32Array[2] must be the same before and after de-/serialization"), TestStruct.Arrays.StaticInt32Array[2], TestStruct2.Arrays.StaticInt32Array[2]);
		Test.TestEqual<float>(TEXT("Arrays.StaticFloatArray[0] must be the same before and after de-/serialization"), TestStruct.Arrays.StaticFloatArray[0], TestStruct2.Arrays.StaticFloatArray[0]);
		Test.TestEqual<float>(TEXT("Arrays.StaticFloatArray[1] must be the same before and after de-/serialization"), TestStruct.Arrays.StaticFloatArray[1], TestStruct2.Arrays.StaticFloatArray[1]);
		Test.TestEqual<float>(TEXT("Arrays.StaticFloatArray[2] must be the same before and after de-/serialization"), TestStruct.Arrays.StaticFloatArray[2], TestStruct2.Arrays.StaticFloatArray[2]);
		Test.TestEqual<TArray<FVector>>(TEXT("Arrays.VectorArray must be the same before and after de-/serialization"), TestStruct.Arrays.VectorArray, TestStruct2.Arrays.VectorArray);

		// test maps
		Test.TestTrue(TEXT("Maps.IntToStr must be the same before and after de-/serialization"), TestStruct.Maps.IntToStr.OrderIndependentCompareEqual(TestStruct2.Maps.IntToStr));
		Test.TestTrue(TEXT("Maps.StrToStr must be the same before and after de-/serialization"), TestStruct.Maps.StrToStr.OrderIndependentCompareEqual(TestStruct2.Maps.StrToStr));
		Test.TestTrue(TEXT("Maps.StrToVec must be the same before and after de-/serialization"), TestStruct.Maps.StrToVec.OrderIndependentCompareEqual(TestStruct2.Maps.StrToVec));

		// test sets
		Test.TestTrue(TEXT("Sets.IntSet must be the same before and after de-/serialization"), TestStruct.Sets.IntSet.Num() == TestStruct2.Sets.IntSet.Num() && TestStruct.Sets.IntSet.Difference(TestStruct2.Sets.IntSet).Num() == 0);
		Test.TestTrue(TEXT("Sets.StrSet must be the same before and after de-/serialization"), TestStruct.Sets.StrSet.Num() == TestStruct2.Sets.StrSet.Num() && TestStruct.Sets.StrSet.Difference(TestStruct2.Sets.StrSet).Num() == 0);
		Test.TestTrue(TEXT("Sets.NameSet must be the same before and after de-/serialization"), TestStruct.Sets.NameSet.Num() == TestStruct2.Sets.NameSet.Num() && TestStruct.Sets.NameSet.Difference(TestStruct2.Sets.NameSet).Num() == 0);
		Test.TestTrue(TEXT("Sets.StructSet must be the same before and after de-/serialization"), TestStruct.Sets.StructSet.Num() == TestStruct2.Sets.StructSet.Num() && TestStruct.Sets.StructSet.Difference(TestStruct2.Sets.StructSet).Num() == 0);
	}
}


/* Tests
 *****************************************************************************/

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStructSerializerTest, "System.Core.Serialization.StructSerializer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)


bool FStructSerializerTest::RunTest( const FString& Parameters )
{
	const EStructSerializerBackendFlags TestFlags = EStructSerializerBackendFlags::Default;

	// json
	{
		TArray<uint8> Buffer;
		FMemoryReader Reader(Buffer);
		FMemoryWriter Writer(Buffer);

		FJsonStructSerializerBackend SerializerBackend(Writer, TestFlags);
		FJsonStructDeserializerBackend DeserializerBackend(Reader);

		StructSerializerTest::TestSerialization(*this, SerializerBackend, DeserializerBackend);

		// uncomment this to look at the serialized data
		//GLog->Logf(TEXT("%s"), (TCHAR*)Buffer.GetData());
	}
	// cbor
	{
		TArray<uint8> Buffer;
		FMemoryReader Reader(Buffer);
		FMemoryWriter Writer(Buffer);

		FCborStructSerializerBackend SerializerBackend(Writer, TestFlags);
		FCborStructDeserializerBackend DeserializerBackend(Reader);

		StructSerializerTest::TestSerialization(*this, SerializerBackend, DeserializerBackend);
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
